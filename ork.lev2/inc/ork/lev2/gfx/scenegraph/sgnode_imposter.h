#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ImposterDrawableData final : public DrawableData {

  DeclareConcreteX(ImposterDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  ImposterDrawableData();
  ~ImposterDrawableData();
  svar64_t _shape;
  fxpipeline_ptr_t _pipeline;
  rtgroup_ptr_t _rtg;
  size_t _detail = 0;
  bool _debug_viz = false;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
