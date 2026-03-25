#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/file/path.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct BillboardDrawableData final : public DrawableData {

  DeclareConcreteX(BillboardDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  BillboardDrawableData();
  ~BillboardDrawableData();

  file::Path _imagePath;     // image file path (reflection/UI — png file browser)
  image_ptr_t _image;        // programmatic: set image directly (overrides _imagePath)
  float _alpha = 1.0f;
  float _screenSize = 30.0f; // constant screen-size in pixels
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
