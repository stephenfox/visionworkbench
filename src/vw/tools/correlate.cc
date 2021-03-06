// __BEGIN_LICENSE__
//  Copyright (c) 2006-2012, United States Government as represented by the
//  Administrator of the National Aeronautics and Space Administration. All
//  rights reserved.
//
//  The NASA Vision Workbench is licensed under the Apache License,
//  Version 2.0 (the "License"); you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
// __END_LICENSE__


#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4996)
#endif

#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
namespace po = boost::program_options;
namespace fs = boost::filesystem;

#include <vw/Core/Debugging.h>
#include <vw/Math.h>
#include <vw/Image.h>
#include <vw/FileIO.h>
#include <vw/InterestPoint/InterestData.h>
#include <vw/Stereo/CorrelationView.h>
#include <vw/Stereo/CostFunctions.h>

using namespace vw;
using namespace vw::stereo;

int main( int argc, char *argv[] ) {
  try {

    std::string left_file_name, right_file_name;
    float log;
    int32 h_corr_min, h_corr_max;
    int32 v_corr_min, v_corr_max;
    int32 xkernel, ykernel;
    int lrthresh;
    int correlator_type;
    bool found_alignment = false;
    Matrix3x3 alignment;

    po::options_description desc("Options");
    desc.add_options()
      ("help,h", "Display this help message")
      ("left", po::value(&left_file_name), "Explicitly specify the \"left\" input file")
      ("right", po::value(&right_file_name), "Explicitly specify the \"right\" input file")
      ("log", po::value(&log)->default_value(1.4), "Apply LOG filter with the given sigma, or 0 to disable")
      ("h-corr-min", po::value(&h_corr_min)->default_value(-30), "Minimum horizontal disparity")
      ("h-corr-max", po::value(&h_corr_max)->default_value(-30), "Maximum horizontal disparity")
      ("v-corr-min", po::value(&v_corr_min)->default_value(-5), "Minimum vertical disparity")
      ("v-corr-max", po::value(&v_corr_max)->default_value(5), "Maximum vertical disparity")
      ("xkernel", po::value(&xkernel)->default_value(15), "Horizontal correlation kernel size")
      ("ykernel", po::value(&ykernel)->default_value(15), "Vertical correlation kernel size")
      ("lrthresh", po::value(&lrthresh)->default_value(2), "Left/right correspondence threshold")
      ("correlator-type", po::value(&correlator_type)->default_value(0), "0 - Abs difference; 1 - Sq Difference; 2 - NormXCorr")
      ("affine-subpix", "Enable affine adaptive sub-pixel correlation (slower, but more accurate)")
      ("pyramid", "Use the pyramid based correlator")
      ;
    po::positional_options_description p;
    p.add("left", 1);
    p.add("right", 1);

    po::variables_map vm;
    try {
      po::store( po::command_line_parser( argc, argv ).options(desc).positional(p).run(), vm );
      po::notify( vm );
    } catch (const po::error& e) {
      std::cout << "An error occured while parsing command line arguments.\n";
      std::cout << "\t" << e.what() << "\n\n";
      std::cout << desc << std::endl;
      return 1;
    }

    if( vm.count("help") ) {
      vw_out() << desc << std::endl;
      return 1;
    }

    if( vm.count("left") != 1 || vm.count("right") != 1 ) {
      vw_out() << "Error: Must specify one (and only one) left and right input file!" << std::endl;
      vw_out() << desc << std::endl;
      return 1;
    }

    std::string match_filename =
      fs::path( left_file_name ).replace_extension().string() + "__" +
      fs::path( right_file_name ).stem().string() + ".match";
    if ( fs::exists( match_filename ) ) {
      vw_out() << "Found a match file. Using it to pre-align images.\n";
      std::vector<ip::InterestPoint> matched_ip1, matched_ip2;
      ip::read_binary_match_file( match_filename,
                                  matched_ip1, matched_ip2 );
      std::vector<Vector3> ransac_ip1 = ip::iplist_to_vectorlist(matched_ip1);
      std::vector<Vector3> ransac_ip2 = ip::iplist_to_vectorlist(matched_ip2);
      vw::math::RandomSampleConsensus<vw::math::HomographyFittingFunctor, vw::math::InterestPointErrorMetric> ransac( vw::math::HomographyFittingFunctor(), vw::math::InterestPointErrorMetric(), 100, 30, ransac_ip1.size()/2, true );
      alignment = ransac( ransac_ip2, ransac_ip1 );

      DiskImageView<PixelGray<float> > right_disk_image( right_file_name );
      right_file_name = "aligned_right.tif";
      write_image( right_file_name, transform(right_disk_image, HomographyTransform(alignment)),
                   TerminalProgressCallback( "tools.correlate", "Aligning: ") );
      found_alignment = true;
    }

    DiskImageView<PixelGray<float> > left_disk_image(left_file_name );
    DiskImageView<PixelGray<float> > right_disk_image(right_file_name );
    int cols = std::min(left_disk_image.cols(),right_disk_image.cols());
    int rows = std::min(left_disk_image.rows(),right_disk_image.rows());
    ImageViewRef<PixelGray<float> > left = edge_extend(left_disk_image,0,0,cols,rows);
    ImageViewRef<PixelGray<float> > right = edge_extend(right_disk_image,0,0,cols,rows);


    stereo::CostFunctionType corr_type = ABSOLUTE_DIFFERENCE;
    if (correlator_type == 1)
      corr_type = SQUARED_DIFFERENCE;
    else if (correlator_type == 2)
      corr_type = CROSS_CORRELATION;

    ImageViewRef<PixelMask<Vector2i> > disparity_map;
    if (vm.count("pyramid")) {
      disparity_map =
        stereo::pyramid_correlate( left, right,
                                   constant_view( uint8(255), left ),
                                   constant_view( uint8(255), right ),
                                   stereo::LaplacianOfGaussian(log),
                                   BBox2i(Vector2i(h_corr_min, v_corr_min),
                                          Vector2i(h_corr_max, v_corr_max)),
                                   Vector2i(xkernel, ykernel),
                                   corr_type, lrthresh );
    } else {
      disparity_map =
        stereo::correlate( left, right,
                           stereo::LaplacianOfGaussian(log),
                           BBox2i(Vector2i(h_corr_min, v_corr_min),
                                  Vector2i(h_corr_max, v_corr_max)),
                           Vector2i(xkernel, ykernel),
                           corr_type, lrthresh );
    }

    ImageViewRef<PixelMask<Vector2f> > result = pixel_cast<PixelMask<Vector2f> >(disparity_map);
    if ( found_alignment )
      result = pixel_cast<PixelMask<Vector2f> >(transform_disparities(disparity_map, HomographyTransform(alignment) ) );

    // Actually invoke the raster
    {
      vw::Timer corr_timer("Correlation Time");
      boost::scoped_ptr<DiskImageResource> r(DiskImageResource::create("disparity.tif",result.format()));
      r->set_block_write_size( Vector2i(1024,1024) );
      block_write_image( *r, result,
                         TerminalProgressCallback( "", "Rendering: ") );
    }

    // Write disparity debug images
    DiskImageView<PixelMask<Vector2f> > solution("disparity.tif");
    BBox2 disp_range = get_disparity_range(solution);
    std::cout << "Found disparity range: " << disp_range << "\n";

    write_image( "x_disparity.tif",
                 channel_cast<uint8>(apply_mask(copy_mask(clamp(normalize(select_channel(solution,0), disp_range.min().x(), disp_range.max().x(),0,255)),solution))) );
    write_image( "y_disparity.tif",
                 channel_cast<uint8>(apply_mask(copy_mask(clamp(normalize(select_channel(solution,1), disp_range.min().y(), disp_range.max().y(),0,255)),solution))) );
  }
  catch (const vw::Exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  return 0;
}

