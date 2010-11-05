// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


#ifndef __VW_PLATE_PLATE_CARREE_PLATEMANAGER_H__
#define __VW_PLATE_PLATE_CARREE_PLATEMANAGER_H__

#include <vw/Plate/PlateManager.h>
#include <vw/Cartography/GeoReference.h>
#include <vw/Cartography/GeoTransform.h>
#include <vw/Image/ImageViewBase.h>
#include <vw/Image/ImageViewRef.h>
#include <vw/Image/Filter.h>
#include <vw/Math/Vector.h>

namespace vw {
namespace cartography { class GeoReference; }
namespace platefile {

  class PlateFile;

  template <class PixelT>
  class PlateCarreePlateManager : public PlateManager<PixelT> {
  protected:
    // Transforms an input image with input georef to plate carree at
    // most ideal matching pyramid level.
    void transform_image( cartography::GeoReference const& georef,
                          ImageViewRef<PixelT>& image,
                          TransformRef& txref, int& level ) const;

  public:
    PlateCarreePlateManager(boost::shared_ptr<PlateFile> platefile) :
      PlateManager<PixelT>(platefile) {}

    // Provide a georeference that represents a pyramid level
    cartography::GeoReference georeference( int level ) const;

    /// This function generates a specific mipmap tile at the given
    /// col, row, and level, and transaction_id.
    void generate_mipmap_tile(int col, int row, int level,
                              int transaction_id, bool preblur) const;
  };

}} // namespace vw::plate

#endif // __VW_PLATE_PLATE_CARREE_PLATEMANAGER_H__
