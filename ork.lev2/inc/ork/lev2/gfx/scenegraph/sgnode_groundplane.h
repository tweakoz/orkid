#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct GroundPlaneDrawableData final : public DrawableData {

  DeclareConcreteX(GroundPlaneDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  GroundPlaneDrawableData();
  ~GroundPlaneDrawableData();

  fxpipeline_ptr_t _pipeline;
  pbrmaterial_ptr_t _material;
  float _extent = 100.0f;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
