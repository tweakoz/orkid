#pragma once

#include <ork/application/application.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/asset/Asset.inl>
#include <ork/profiling.inl>

namespace ork::lev2::pbr {

struct ForwardPbrNodeImpl;

struct ForwardPass {
  //ForwardNode* _node                 = nullptr;
  ForwardPbrNodeImpl* _impl          = nullptr;
  CompositorDrawData* _drawdata      = nullptr;
  std::string _fwd_pass_layer        = "std_forward";
  std::string _dpp_pass_layer        = "depth_prepass";
  bool _single_pass_stereo           = false;
  rtgroup_ptr_t _rtg_out;
  rtgroup_ptr_t _rtg_depth_copy;
  rtgroup_ptr_t _rtg_depth_copy_linear;
  bool _renderingPROBE = false;
};
using forward_pass_ptr_t = std::shared_ptr<ForwardPass>;

struct ForwardPbrNodeImpl {
  static const int KMAXLIGHTS = 32;
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ForwardPbrNodeImpl(ForwardNode* node);
  ~ForwardPbrNodeImpl();
  void init(lev2::Context* context, int iw, int ih);
  void _render_top(CompositorDrawData& drawdata);
  void _render_dppskyssaocolor(forward_pass_ptr_t fpass);
  void _render_dpp(forward_pass_ptr_t fpass);
  void _render_skybox(forward_pass_ptr_t fpass);
  void _render_ssao_linearize_depth(forward_pass_ptr_t fpass);
  void _render_ssao_prepass(forward_pass_ptr_t fpass);
  void _render_colorpass(forward_pass_ptr_t fpass);
  void _update_env_probes(CompositorDrawData& drawdata);
  void _update_shadow_maps();
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ForwardNode* _node;
  std::string _camname;
  enumeratedlights_ptr_t _enumeratedLights;

  rtgset_ptr_t  _rtgs_primary;
  rtgroup_ptr_t _rtg_primary;
  rtgroup_ptr_t _rtg_primary_depth_copy;
  rtgroup_ptr_t _rtg_primary_depth_copy_linear;
  rtgroup_ptr_t _rtg_ambocc_accum;
  rtgroup_ptr_t _rtg_ambocc_accum2;
  rtgroup_ptr_t _rtg_cube1_depth_copy;
  rtgset_ptr_t _rtgs_resolve_msaa;
  fmtx4 _viewOffsetMatrix;
  pbrmaterial_ptr_t _skybox_material;
  freestyle_mtl_ptr_t _ssao_material;
  fxpipelinecache_constptr_t _skybox_fxcache;
  fxpipelinecache_constptr_t _ssao_fxcache;
  textureassetptr_t _whiteTexture;
  cameramatrices_ptr_t _SHADOWCAM;
  cameramatrices_ptr_t _CUBECAM;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique1x1;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpP;
  const FxShaderParam* _fxpInvP;
  const FxShaderParam* _fxpColorMap;

  const FxUniformBlock* _par_ublk_std_matrices = nullptr;  
  FxUniformBuffer* _ubuf_std_matrices = nullptr;

  const FxShaderTechnique* _tek_ssao_prepass;
  const FxShaderTechnique* _tek_ssao_lindepth;

  const FxShaderParam* _fxpSSAONumSamples;
  const FxShaderParam* _fxpSSAONumSteps;
  const FxShaderParam* _fxpSSAOBias;
  const FxShaderParam* _fxpSSAORadius;
  const FxShaderParam* _fxpSSAOWeight;
  const FxShaderParam* _fxpSSAOPower;
  const FxShaderParam* _fxpSSAOFeedback;
  const FxShaderParam* _fxpSSAOKernel;
  const FxShaderParam* _fxpSSAOScrNoise;
  const FxShaderParam* _fxpSSAOMapDepth;
  const FxShaderParam* _fxpSSAOTexelSize;
  const FxShaderParam* _fxpSSAOInvViewportSize;
  const FxShaderParam* _fxpSSAOMVP;
  const FxShaderParam* _fxpSSAOPREV;
  const FxShaderParam* _fxpZndc2eye;

  const DrawQueue* _currentDrawQueue = nullptr;
  ViewData _currentViewData;
  rcfd_ptr_t _currentRCFD;
  lev2::IRenderer* _currentIRenderer = nullptr;
  Context* _currentContext       = nullptr;
  compositorimpl_ptr_t _currentCIMPL = nullptr;
  int _currentWidth              = 0;
  int _currentHeight             = 0;

  forward_pass_ptr_t _primary_pass;

}; // IMPL

} // namespace ork::lev2::pbr
