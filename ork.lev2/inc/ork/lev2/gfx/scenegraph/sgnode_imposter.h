#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ImposterPassData {
  ImposterPassData();
  fxpipeline_ptr_t _pipeline;
  rtgroup_ptr_t _rtg;
  varmap::varmap_ptr_t _userdata;
  bool _debug_viz = false;
  bool _enabled = true;
  void_lambda_t _onPreRender;
  void_lambda_t _onPostRender;
};

struct ImposterDrawableData final : public DrawableData {

  DeclareConcreteX(ImposterDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  ImposterDrawableData();
  ~ImposterDrawableData();
  std::vector<imposterpassdataptr_t> _user_passes;
  imposterpassdataptr_t _imp_pass;
  imposterpassdataptr_t _blit_pass;
  svar64_t _shape;
  size_t _detail = 0;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
