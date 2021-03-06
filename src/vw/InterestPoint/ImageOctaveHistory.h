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


/// \file ImageOctaveHistory.h
///
/// Class for storing all of the intermediate images processed while
/// iterating through an ImageOctave. This can be useful for generating
/// descriptors after interest point detection has been completed.
///
#ifndef __IMAGE_OCTAVE_HISTORY_H__
#define __IMAGE_OCTAVE_HISTORY_H__

#include <vw/InterestPoint/InterestData.h>
#include <vw/InterestPoint/ImageOctave.h>

namespace vw {
namespace ip {

/// This is a convenient container class for the image data
/// generated by iterating through different scale space octaves
/// with ImageOctave. It templated to allow storage of all
/// relevant data, usually of type ImageView or ImageInterestData.
/// The image in the scale space pyramid most closely corresponding
/// to a particular scale can be retrieved with the
/// image_at_scale method.
template <class ImageT>
class ImageOctaveHistory : std::vector<std::vector<ImageT> > {
 private:
  int num_scales;

 public:
  /// Construct an empty ImageOctaveHistory.
  ImageOctaveHistory() : std::vector<std::vector<ImageT> >(0), num_scales(0) {}

  /// Number of octaves recorded.
  inline int octaves() const { return this->size(); }

  /// Number of scales per octave.
  /// This is two less than the number of planes.
  inline int scales() const {
    return num_scales;
  }

  /// Add an octave to the recorded history.
  inline void add_octave(const std::vector<ImageT>& octave) {
    this->push_back(octave);
    num_scales = octave.size() - 2;
  }

  /// Retrieve image data most closely matching a given scale.
  // TODO: the interface could be changed to be more forgiving.
  ImageT const& image_at_scale(float scale) const {
    int octave = (int)(log(scale) / M_LN2);
    if (octave == octaves()) octave = octaves() - 1;
    VW_ASSERT( (octave >= 0) && (octave < octaves()) , ArgumentErr()
               << "ImageOctaveHistory::image_at_scale: No image matching scale.");
    // TODO: move this outside ImageOctave
    int plane = ImageOctave<ImageT>::scale_to_plane_index(1 << octave, num_scales, scale);
    VW_ASSERT( (plane >= 0) && (plane < scales() + 2) , ArgumentErr()
               << "ImageOctaveHistory::image_at_scale: No image matching scale.");
    return (*this)[octave][plane];
  }
};

}} // namespace vw::ip

#endif // __IMAGE_OCTAVE_HISTORY_H__
