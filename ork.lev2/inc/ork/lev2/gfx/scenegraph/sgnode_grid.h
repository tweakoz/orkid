#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct GridDrawableData final : public DrawableData {

  DeclareConcreteX(GridDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  GridDrawableData();
  ~GridDrawableData();

  std::string _colortexpath;
  float _extent = 100.0f;
  float _majorTileDim = 1.0f;
  float _minorTileDim = 0.1f;
  std::string _shader_suffix = "";
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
