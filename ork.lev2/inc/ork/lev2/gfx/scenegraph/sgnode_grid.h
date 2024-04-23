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
  std::string _normaltexpath;
  fvec3 _modcolor = fvec3(1, 1, 1);
  float _intensityA = 1.0;
  float _intensityB = 1.0;
  float _intensityC = 1.0;
  float _intensityD = 1.0;
  float _lineWidth = 0.05;
  float _extent = 100.0f;
  float _majorTileDim = 1.0f;
  float _minorTileDim = 0.1f;
  std::string _shader_suffix = "";
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
