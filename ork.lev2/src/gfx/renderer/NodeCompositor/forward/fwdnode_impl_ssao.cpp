////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "fwdnode_impl.h"
#include <ork/util/logger.h>

namespace ork::lev2::pbr {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SSAO (Linearize Depth) pass
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_ssao_linearize_depth(forward_pass_ptr_t fpass) {

  auto rtg_out   = fpass->_rtg_out;
  auto FBI       = _currentContext->FBI();
  auto GBI       = _currentContext->GBI();
  int W          = _currentWidth;
  int H          = _currentHeight;

  _currentRCFD->_subpassID = "SSAO_LINDEPTH"_crcu;
  //printf("W<%d> H<%d>\n", W, H);
  auto LDOUT = _rtg_primary_depth_copy_linear;
  if (LDOUT->width() != W or LDOUT->height() != H) {
    LDOUT->Resize(W, H);
  }

  _currentContext->debugPushGroup("ForwardPBR::depth-linearize pass");

  LDOUT->_autoclear      = true;
  LDOUT->_depthOnly      = false;
  LDOUT->_clearMaskDepth = false;
  LDOUT->_clearMaskColor = true;

  FBI->PushRtGroup(LDOUT.get());

  RenderContextInstData RCID(_currentRCFD);

  _ssao_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _ssao_material->_rasterstate->setDepthTest(EDepthTest::OFF);
  _ssao_material->_rasterstate->setCullTest(ECullTest::OFF);
  _ssao_material->_rasterstate->setWriteMaskZ(false);
  _ssao_material->_rasterstate->setWriteMaskRGB(true);
  _ssao_material->_rasterstate->setWriteMaskA(true);

  _ssao_material->begin(_tek_ssao_lindepth, _currentRCFD);

  // printf( "VD._near<%g> VD._far<%g>\n", VD._near, VD._far );
  auto depth_texture = fpass->_rtg_depth_copy->_depthBuffer->_texture;
  _ssao_material->bindParamMatrix(_fxpSSAOMVP, fmtx4::Identity());
  _ssao_material->bindParamTexture(_fxpSSAOMapDepth, depth_texture.get());
  _ssao_material->bindParamVec2(_fxpZndc2eye, fvec2(_currentViewData._near, _currentViewData._far));
  _ssao_material->bindParamMatrix(_fxpInvP, _currentViewData.PL.inverse());
  _ssao_material->bindParamMatrix(_fxpP, _currentViewData.PL);

  fvec2 ivpsize = fvec2(1.0f / W, 1.0f / H);

  _ssao_material->bindParamVec2(_fxpSSAOInvViewportSize, ivpsize);

  ViewportRect extents(0, 0, W, H);
  FBI->pushViewport(extents);
  FBI->pushScissor(extents);

  GBI->render2dQuadEML(); // full screen quad
  FBI->popViewport();
  FBI->popScissor();

  _ssao_material->end(_currentRCFD);

  FBI->PopRtGroup();
  _currentContext->debugPopGroup();

  _currentRCFD->setUserProperty("LINEAR_DEPTH_MAP"_crcu, _rtg_primary_depth_copy_linear->texture(0));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SSAO (AO cpmpute) pass
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_ssao_prepass(forward_pass_ptr_t fpass) {
  auto drawdata  = fpass->_drawdata;
  auto rtg_out   = fpass->_rtg_out;
  auto FBI       = _currentContext->FBI();
  auto GBI       = _currentContext->GBI();
  auto pbrcommon = _node->_pbrcommon;
  int node_frame = _node->_frameIndex;

  //printf("render ssaoprepass rtg<%p>\n", (void*)rtg_out.get());

  auto ssao_kernel   = pbrcommon->ssaoKernel(_currentContext, node_frame);
  //printf("node_frame<%d>\n", node_frame);
  auto ssao_scrnoise = pbrcommon->ssaoScrNoise(_currentContext, node_frame, _currentWidth, _currentHeight);
  _currentRCFD->setUserProperty("SSAO_KERNEL"_crcu, ssao_kernel);
  _currentRCFD->setUserProperty("SSAO_SCRNOISE"_crcu, ssao_scrnoise);

  OrkAssert(pbrcommon->_useDepthPrepass);

  bool buf_select = (_node->_frameIndex & 1);

  _currentRCFD->_subpassID = "SSAO_PREPASS"_crcu;

  auto ambocc_accum_w = buf_select ? _rtg_ambocc_accum : _rtg_ambocc_accum2;
  auto ambocc_accum_r = buf_select ? _rtg_ambocc_accum2 : _rtg_ambocc_accum;

  if (ambocc_accum_w->width() != _currentWidth or ambocc_accum_w->height() != _currentHeight) {
    ambocc_accum_w->Resize(_currentWidth, _currentHeight);
  }
  if (ambocc_accum_r->width() != _currentWidth or ambocc_accum_r->height() != _currentHeight) {
    ambocc_accum_r->Resize(_currentWidth, _currentHeight);
  }

  // FBI->validateRtGroup(ambocc_accum_w);
  _currentContext->debugPushGroup("ForwardPBR::ssao-pre pass");

  ambocc_accum_w->_autoclear      = false;
  ambocc_accum_w->_depthOnly      = false;
  ambocc_accum_w->_clearMaskDepth = false;
  ambocc_accum_w->_clearMaskColor = false;

  FBI->PushRtGroup(ambocc_accum_w.get());

  RenderContextInstData RCID(_currentRCFD);

  _ssao_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _ssao_material->_rasterstate->setDepthTest(EDepthTest::OFF);
  _ssao_material->_rasterstate->setCullTest(ECullTest::OFF);
  _ssao_material->_rasterstate->setWriteMaskZ(false);
  _ssao_material->_rasterstate->setWriteMaskRGB(true);
  _ssao_material->_rasterstate->setWriteMaskA(true);

  _ssao_material->begin(_tek_ssao_prepass, _currentRCFD);

  _ssao_material->bindParamMatrix(_fxpSSAOMVP, fmtx4::Identity());
  _ssao_material->bindParamInt(_fxpSSAONumSamples, pbrcommon->_ssaoNumSamples);
  _ssao_material->bindParamInt(_fxpSSAONumSteps, pbrcommon->_ssaoNumSteps);
  _ssao_material->bindParamFloat(_fxpSSAOBias, pbrcommon->_ssaoBias);
  _ssao_material->bindParamFloat(_fxpSSAORadius, pbrcommon->_ssaoRadius);
  _ssao_material->bindParamFloat(_fxpSSAOWeight, pbrcommon->_ssaoWeight);
  _ssao_material->bindParamFloat(_fxpSSAOPower, pbrcommon->_ssaoPower);
  _ssao_material->bindParamFloat(_fxpSSAOFeedback, pbrcommon->_ssaoFeedback);

  _ssao_material->bindParamTexture(_fxpSSAOMapDepth, _rtg_primary->_depthBuffer->_texture.get());
  _ssao_material->bindParamTexture(_fxpSSAOKernel, ssao_kernel.get());
  _ssao_material->bindParamTexture(_fxpSSAOScrNoise, ssao_scrnoise.get());
  _ssao_material->bindParamTexture(_fxpSSAOPREV, ambocc_accum_r->texture(0).get());
  _ssao_material->bindParamVec2(_fxpZndc2eye, _currentViewData._zndc2eye);
  _ssao_material->bindParamMatrix(_fxpInvP, _currentViewData.IPM);
  _ssao_material->bindParamMatrix(_fxpP, _currentViewData.PM);

  //_currentViewData.IPM.dump("IPM");

  fvec2 ivpsize = fvec2(1.0f / _currentWidth, 1.0f / _currentHeight);

  _ssao_material->bindParamVec2(_fxpSSAOInvViewportSize, ivpsize);

  ViewportRect extents(0, 0, _currentWidth, _currentHeight);
  FBI->pushViewport(extents);
  FBI->pushScissor(extents);

  GBI->render2dQuadEML(); // full screen quad
  FBI->popViewport();
  FBI->popScissor();

  _ssao_material->end(_currentRCFD);

  FBI->PopRtGroup();
  _currentContext->debugPopGroup();

  _currentRCFD->setUserProperty("SSAO_MAP"_crcu, ambocc_accum_w->texture(0));
  fvec2 ssao_dim = fvec2(ambocc_accum_w->width(), ambocc_accum_w->height());
  _currentRCFD->setUserProperty("SSAO_DIM"_crcu, ssao_dim);
  _currentRCFD->setUserProperty("SSAO_POWER"_crcu, pbrcommon->_ssaoPower);
  _currentRCFD->setUserProperty("SSAO_WEIGHT"_crcu, pbrcommon->_ssaoWeight);
}

} //namespace ork::lev2::pbr {
