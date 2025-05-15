////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pbr_node_forward_impl.h"
#include <ork/util/logger.h>

namespace ork::lev2 {
extern appinitdata_ptr_t _ginitdata;
} // namespace ork::lev2

namespace ork::lev2::pbr {

static logchannel_ptr_t logchan_pbr_fwd = logger()->createChannel("mtlpbrFWD", fvec3(0.8, 0.8, 0.1), true);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ForwardPbrNodeImpl::ForwardPbrNodeImpl(ForwardNode* node)
    : _node(node)
    , _camname("Camera") { //

  _SHADOWCAM = std::make_shared<CameraMatrices>();
  _CUBECAM   = std::make_shared<CameraMatrices>();
  _main_pass = std::make_shared<ForwardPass>();

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ForwardPbrNodeImpl::~ForwardPbrNodeImpl() {
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ForwardPbrNodeImpl::init(lev2::Context* context, int iw, int ih) {

  if (nullptr == _rtgs_main) {

    _rtg_main_depth_copy  = std::make_shared<RtGroup>(context, 8, 8);
    _rtg_cube1_depth_copy = std::make_shared<RtGroup>(context, 8, 8);
    _rtg_ambocc_accum     = std::make_shared<RtGroup>(context, 8, 8);
    _rtg_ambocc_accum2    = std::make_shared<RtGroup>(context, 8, 8);

    auto pbrcommon = _node->_pbrcommon;

    EBufferFormat efmt = EBufferFormat::RGBA8;
    if (pbrcommon->_useFloatColorBuffer) {
      efmt = EBufferFormat::RGBA32F;
    }

    auto e_msaa = intToMsaaEnum(_ginitdata->_msaa_samples);
    _rtgs_main  = std::make_shared<RtgSet>(context, e_msaa, "rtgs-main");
    _rtgs_main->addBuffer("ForwardRt0", efmt);
    // MsaaSamples msaa = rtg_out->_msaa_samples;

    _rtg_ambocc_accum->createRenderTarget(EBufferFormat::R32F);
    _rtg_ambocc_accum2->createRenderTarget(EBufferFormat::R32F);
    _rtg_main_depth_copy_linear = std::make_shared<RtGroup>(context, 8, 8, e_msaa);
    _rtg_main_depth_copy_linear->createRenderTarget(EBufferFormat::R32F);

    printf("PBRFWD_MSAA<%d>\n", int(_ginitdata->_msaa_samples));
    //_rtg             = std::make_shared<RtGroup>(context, 8, 8, intToMsaaEnum(_ginitdata->_msaa_samples));
    // auto buf1        = _rtg->createRenderTarget(EBufferFormat::RGBA8);
    // buf1->_debugName = "ForwardRt0";
    _skybox_material           = std::make_shared<PBRMaterial>(context);
    _skybox_material->_variant = "skybox.forward"_crcu;
    _skybox_fxcache            = _skybox_material->pipelineCache();
    _enumeratedLights          = std::make_shared<EnumeratedLights>();

    if (_ginitdata->_msaa_samples > 1) {
      switch (_ginitdata->_msaa_samples) {
        case 0:
        case 1:
          _rtgs_resolve_msaa = std::make_shared<RtgSet>(context, MsaaSamples::MSAA_1X, "rtgs-,main-resolve");
          break;
        case 4:
          _rtgs_resolve_msaa = std::make_shared<RtgSet>(context, MsaaSamples::MSAA_4X, "rtgs-,main-resolve");
          break;
        case 9:
          _rtgs_resolve_msaa = std::make_shared<RtgSet>(context, MsaaSamples::MSAA_9X, "rtgs-,main-resolve");
          break;
        case 16:
          _rtgs_resolve_msaa = std::make_shared<RtgSet>(context, MsaaSamples::MSAA_16X, "rtgs-,main-resolve");
          break;
        default:
          OrkAssert(false);
          break;
      }
      _rtgs_resolve_msaa->addBuffer("MsaaDownsampleBuffer", efmt);
      //_rtg_resolve_msaa = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
      // auto dsbuf        = _rtg_resolve_msaa->createRenderTarget(EBufferFormat::RGBA8);
      // dsbuf->_debugName = "MsaaDownsampleBuffer";
      _blit2screenmtl.gpuInit(context, "orkshader://solid");
      _fxtechnique1x1 = _blit2screenmtl.technique("texcolor");
      _fxpMVP         = _blit2screenmtl.param("MatMVP");
      _fxpColorMap    = _blit2screenmtl.param("ColorMap");
    }

    /////////////////
    // SSAO
    /////////////////

    _ssao_material = std::make_shared<FreestyleMaterial>();
    _ssao_material->gpuInit(context, "orkshader://framefx");
    _tek_ssao     = _ssao_material->technique("framefx_ssao");
    _tek_lindepth = _ssao_material->technique("framefx_linearize_depth");

    _fxpSSAOMVP             = _ssao_material->param("mvp");
    _fxpSSAONumSamples      = _ssao_material->param("SSAONumSamples");
    _fxpSSAONumSamples      = _ssao_material->param("SSAONumSamples");
    _fxpSSAONumSteps        = _ssao_material->param("SSAONumSteps");
    _fxpSSAOBias            = _ssao_material->param("SSAOBias");
    _fxpSSAORadius          = _ssao_material->param("SSAORadius");
    _fxpSSAOWeight          = _ssao_material->param("SSAOWeight");
    _fxpSSAOPower           = _ssao_material->param("SSAOPower");
    _fxpSSAOKernel          = _ssao_material->param("SSAOKernel");
    _fxpSSAOScrNoise        = _ssao_material->param("SSAOScrNoise");
    _fxpSSAOMapDepth        = _ssao_material->param("MapDepth");
    _fxpSSAOTexelSize       = _ssao_material->param("TexelSize");
    _fxpSSAOInvViewportSize = _ssao_material->param("InvViewportSize");
    _fxpSSAOPREV            = _ssao_material->param("SSAOPREV");
    _fxpZndc2eye            = _ssao_material->param("Zndc2eye");
    _fxpInvP                = _ssao_material->param("MatInvP");
    _fxpP                   = _ssao_material->param("MatP");

    auto mtl_load_req1 = std::make_shared<asset::LoadRequest>("src://effect_textures/white");
    _whiteTexture      = asset::AssetManager<TextureAsset>::load(mtl_load_req1);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ForwardPbrNodeImpl::_render_dpp(forward_pass_ptr_t fpass) {
  auto drawdata  = fpass->_drawdata;
  auto rtg_out   = fpass->_rtg_out;
  //auto RCFD      = drawdata->RCFD();
  //auto context   = drawdata->context();
  //auto irenderer = drawdata->property("irenderer"_crcu).get<lev2::IRenderer*>();
  auto FBI       = _currentContext->FBI();

  _currentContext->debugPushGroup("ForwardPBR::depth-pre pass");
  _currentDrawQueue->enqueueLayerToRenderQueue(fpass->_dpp_pass_layer, _currentIRenderer);
  _currentRCFD->_renderingmodel = "DEPTH_PREPASS"_crcu;
  _currentRCFD->_subpassID      = "DEPTH_PREPASS"_crcu;

  rtg_out->_autoclear      = true;
  rtg_out->_depthOnly      = true;
  rtg_out->_clearMaskDepth = true;
  rtg_out->_clearMaskColor = false;
  FBI->PushRtGroup(rtg_out.get());

  _currentIRenderer->drawEnqueuedRenderables(true);
  FBI->PopRtGroup();
  _currentContext->debugPopGroup();

  FBI->cloneDepthBuffer(rtg_out, fpass->_rtg_depth_copy);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ForwardPbrNodeImpl::_render_skybox(forward_pass_ptr_t fpass) {
  auto drawdata = fpass->_drawdata;
  auto context  = drawdata->context();
  auto rtg_out  = fpass->_rtg_out;
  auto RCFD     = drawdata->RCFD();
  auto FBI      = context->FBI();
  auto GBI      = context->GBI();

  context->debugPushGroup("ForwardPBR::skybox pass");

  rtg_out->_depthOnly      = false;
  rtg_out->_autoclear      = true;
  rtg_out->_clearMaskDepth = true;
  rtg_out->_clearMaskColor = true;

  RCFD->_renderingmodel = "CUSTOM"_crcu;
  RCFD->_subpassID      = "SKYBOX"_crcu;
  RenderContextInstData RCID(RCFD);
  RCID._pipeline_cache = _skybox_fxcache;
  auto pipeline        = _skybox_fxcache->findPipeline(RCID);
  FBI->PushRtGroup(rtg_out.get());
  pipeline->wrappedDrawCall(RCID, [GBI]() {
    GBI->render2dQuadEML(
        fvec4(-1, -1, 2, 2), //
        fvec4(0, 0, 1, 1),   //
        fvec4(0, 0, 1, 1),   //
        0.9999f);            // full screen quad
  });
  context->debugPopGroup();
  FBI->PopRtGroup();
}

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

  auto LDOUT = _rtg_main_depth_copy_linear;
  if (LDOUT->width() != W or LDOUT->height() != H) {
    LDOUT->Resize(W, H);
  }

  _currentContext->debugPushGroup("ForwardPBR::depth-linearize pass");

  LDOUT->_autoclear      = false;
  LDOUT->_depthOnly      = false;
  LDOUT->_clearMaskDepth = false;
  LDOUT->_clearMaskColor = false;

  FBI->PushRtGroup(LDOUT.get());

  RenderContextInstData RCID(_currentRCFD);

  _ssao_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _ssao_material->_rasterstate->setDepthTest(EDepthTest::OFF);
  _ssao_material->_rasterstate->setCullTest(ECullTest::OFF);
  _ssao_material->_rasterstate->setWriteMaskZ(false);
  _ssao_material->_rasterstate->setWriteMaskRGB(true);
  _ssao_material->_rasterstate->setWriteMaskA(true);

  _ssao_material->begin(_tek_lindepth, _currentRCFD);

  // printf( "VD._near<%g> VD._far<%g>\n", VD._near, VD._far );
  _ssao_material->bindParamMatrix(_fxpSSAOMVP, fmtx4::Identity());
  _ssao_material->bindParamTexture(_fxpSSAOMapDepth, rtg_out->_depthBuffer->_texture.get());
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

  _currentRCFD->setUserProperty("LINEAR_DEPTH_MAP"_crcu, _rtg_main_depth_copy_linear->GetMrt(0)->_texture);
  _currentRCFD->setUserProperty("NEAR_FAR"_crcu, fvec2(_currentViewData._near, _currentViewData._far));
  _currentRCFD->setUserProperty("PMATRIX"_crcu, _currentViewData.PL);
  _currentRCFD->setUserProperty("IPMATRIX"_crcu, _currentViewData.PL.inverse());
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

  auto ssao_kernel   = pbrcommon->ssaoKernel(_currentContext, node_frame);
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

  _ssao_material->begin(_tek_ssao, _currentRCFD);

  _ssao_material->bindParamMatrix(_fxpSSAOMVP, fmtx4::Identity());
  _ssao_material->bindParamInt(_fxpSSAONumSamples, pbrcommon->_ssaoNumSamples);
  _ssao_material->bindParamInt(_fxpSSAONumSteps, pbrcommon->_ssaoNumSteps);
  _ssao_material->bindParamFloat(_fxpSSAOBias, pbrcommon->_ssaoBias);
  _ssao_material->bindParamFloat(_fxpSSAORadius, pbrcommon->_ssaoRadius);
  _ssao_material->bindParamFloat(_fxpSSAOWeight, pbrcommon->_ssaoWeight);
  _ssao_material->bindParamFloat(_fxpSSAOPower, pbrcommon->_ssaoPower);

  _ssao_material->bindParamTexture(_fxpSSAOMapDepth, _rtg_main_depth_copy_linear->GetMrt(0)->_texture.get());
  _ssao_material->bindParamTexture(_fxpSSAOKernel, ssao_kernel.get());
  _ssao_material->bindParamTexture(_fxpSSAOScrNoise, ssao_scrnoise.get());
  _ssao_material->bindParamTexture(_fxpSSAOPREV, ambocc_accum_r->GetMrt(0)->_texture.get());
  _ssao_material->bindParamVec2(_fxpZndc2eye, _currentViewData._zndc2eye);
  _ssao_material->bindParamMatrix(_fxpInvP, _currentViewData.PL.inverse());
  _ssao_material->bindParamMatrix(_fxpP, _currentViewData.PL);

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

  _currentRCFD->setUserProperty("SSAO_MAP"_crcu, ambocc_accum_w->GetMrt(0)->_texture);
  fvec2 ssao_dim = fvec2(ambocc_accum_w->width(), ambocc_accum_w->height());
  _currentRCFD->setUserProperty("SSAO_DIM"_crcu, ssao_dim);
  _currentRCFD->setUserProperty("SSAO_POWER"_crcu, pbrcommon->_ssaoPower);
  _currentRCFD->setUserProperty("SSAO_WEIGHT"_crcu, pbrcommon->_ssaoWeight);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Color pass
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_colorpass(forward_pass_ptr_t fpass) {

  auto drawdata  = fpass->_drawdata;
  auto rtg_out   = fpass->_rtg_out;
  auto FBI       = _currentContext->FBI();
  auto GBI       = _currentContext->GBI();

  _currentContext->debugMarker("ForwardPBR::renderEnqueuedScene::layer<std_forward>");
  _currentDrawQueue->enqueueLayerToRenderQueue(fpass->_fwd_pass_layer, _currentIRenderer);

  _currentRCFD->_renderingmodel = "FORWARD_PBR"_crcu;
  _currentRCFD->_subpassID      = "COLOR"_crcu;
  _currentContext->debugPushGroup("ForwardPBR::color pass");
  _currentIRenderer->_debugLog     = false;
  rtg_out->_autoclear      = false;
  rtg_out->_depthOnly      = false;
  rtg_out->_clearMaskDepth = false; // not clearing anyway ...
  rtg_out->_clearMaskColor = false; // not clearing anyway ...
  FBI->PushRtGroup(rtg_out.get());
  _currentIRenderer->drawEnqueuedRenderables(true);
  _currentContext->debugPopGroup();

  FBI->PopRtGroup();


}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// render, from a given view (supplied externally)
//    the dpp, sky, ssao, color passes into a render target
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_dppskyssaocolor(forward_pass_ptr_t fpass) {

  auto drawdata = fpass->_drawdata;
  auto rtg_out  = fpass->_rtg_out;

  /////////////////////////////////////////////////////////////////////////////////////////

  RtGroupRenderTarget rt(rtg_out.get());

  CompositingPassData MY_CPD = _currentCIMPL->topCPD(); // copy top CPD
  auto pbrcommon             = _node->_pbrcommon;
  bool renderingPROBE        = fpass->_renderingPROBE;

  ///////////////////////////////////////////////////////////////////////////
  // CPD modifications for this set of passes
  ///////////////////////////////////////////////////////////////////////////

  MY_CPD._single_pass_stereo = fpass->_single_pass_stereo;
  // MY_CPD._mono_cam_matrices = drawdata->property("defcammtx"_crcu).get<cameramatrices_ptr_t>();
  _currentCIMPL->pushCPD(MY_CPD);

  ///////////////////////////////////////////////////////////////////////////
  // setup global RCFD state for PBR materials
  ///////////////////////////////////////////////////////////////////////////

  bool have_probes = (_enumeratedLights->_lightprobes.size() > 0);

  _currentRCFD->setUserProperty("enumeratedlights"_crcu, _enumeratedLights);
  _currentRCFD->setUserProperty("renderingPROBE"_crcu, renderingPROBE);
  _currentRCFD->setUserProperty("havePROBES"_crcu, have_probes);
  _currentRCFD->setUserProperty("OutputWidth"_crcu, _currentWidth);
  _currentRCFD->setUserProperty("OutputHeight"_crcu, _currentHeight);

  ///////////////////////////////////////////////////////////////////////////
  // Render Skybox first so MSAA can blend with it
  ///////////////////////////////////////////////////////////////////////////

  _render_skybox(fpass);

  ///////////////////////////////////////////////////////////////////////////
  // depth prepass
  ///////////////////////////////////////////////////////////////////////////

  if (pbrcommon->_useDepthPrepass) {
    // depth prepass
    _render_dpp(fpass);
  }

  ///////////////////////////////////////////////////////////////////////////
  // SSAO Linearize depth
  ///////////////////////////////////////////////////////////////////////////

  bool is_ssao_active = (pbrcommon->_ssaoNumSamples >= 8);

  if (is_ssao_active) {
    // linearize depth -> fpass->_rtg_depth_copy_linear
    _render_ssao_linearize_depth(fpass);
  }

  ///////////////////////////////////////////////////////////////////////////
  // store depth buffer in RCFD
  ///////////////////////////////////////////////////////////////////////////

  if (pbrcommon->_useDepthPrepass) {
    _currentRCFD->setUserProperty("DEPTH_MAP"_crcu, fpass->_rtg_depth_copy->_depthBuffer->_texture);
  }

  ///////////////////////////////////////////////////////////////////////////
  // SSAO prepass
  ///////////////////////////////////////////////////////////////////////////

  if (is_ssao_active) {
    _render_ssao_prepass(fpass);
  } else {
    // set SSAO to white..
    _currentRCFD->setUserProperty("SSAO_MAP"_crcu, _whiteTexture->GetTexture());
    _currentRCFD->setUserProperty("SSAO_DIM"_crcu, fvec2(8, 8));
    _currentRCFD->setUserProperty("SSAO_POWER"_crcu, 1.0f);
    _currentRCFD->setUserProperty("SSAO_WEIGHT"_crcu, 0.0f);
  }
  _currentRCFD->setUserProperty("PBR_COMMON"_crcu, pbrcommon);

  ///////////////////////////////////////////////////////////////////////////
  // main color pass
  ///////////////////////////////////////////////////////////////////////////

  _render_colorpass(fpass);

  ///////////////////////////////////////////////////////////////////////////
  _currentCIMPL->popCPD();
  ///////////////////////////////////////////////////////////////////////////
  auto& ddprops = drawdata->_properties;
  ddprops["depthbuffer"_crcu].set<rtbuffer_ptr_t>(rtg_out->_depthBuffer);

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ForwardPbrNodeImpl::_update_env_probes(CompositorDrawData& drawdata) {

  auto TXI     = _currentContext->TXI();
  auto topcomp = _currentRCFD->topCompositor();
  auto CPD     = _currentCIMPL->topCPD();

  for (auto probe : _enumeratedLights->_lightprobes) {
    switch (probe->_type) {
      case LightProbeType::REFLECTION: {
        if (nullptr == probe->_cubeRenderRTG) {
          probe->_cubeRenderRTG           = std::make_shared<RtGroup>(_currentContext, 8, 8);
          probe->_cubeRenderRTG->_name    = "ReflectionProbeRTG";
          auto colorbuf                   = probe->_cubeRenderRTG->createRenderTarget(EBufferFormat::RGBA8);
          colorbuf->_debugName            = "ReflectionProbeColorCubeMap";
          probe->_cubeRenderRTG->_cubeMap = true;
        }
        if (probe->_dirty) {
          int prevW = probe->_cubeRenderRTG->width();
          int prevH = probe->_cubeRenderRTG->height();
          if (prevW != probe->_dim or prevH != probe->_dim) {
            probe->_cubeRenderRTG->Resize(probe->_dim, probe->_dim);
          }

          auto CMATRIX = probe->_worldMatrix;

          fvec3 POSX = CMATRIX.xNormal() * -1;
          fvec3 POSY = CMATRIX.yNormal();
          fvec3 POSZ = CMATRIX.zNormal() * -1;

          fvec3 position = CMATRIX.translation();

          CompositingPassData cubemapCPD = CPD.clone();

          // compute projection matrix
          _CUBECAM->_pmatrix.perspective(90.0f * DTOR, 1.0f, 0.01f, 1000.0f);

          // flip y on projection matrix
          fmtx4 flipy;
          flipy.setScale(1, -1, 1);
          _CUBECAM->_pmatrix = flipy * _CUBECAM->_pmatrix;

          for (int iface = 0; iface < 6; iface++) {

            _currentContext->debugPushGroup(FormatString("ForwardPBR::cubemap pass<%d>", iface));

            // compute view matrices from cubeface and CMATRIX
            //  face 0 = POSX
            //  face 1 = NEGX
            //  face 2 = POSY
            //  face 3 = NEGY
            //  face 4 = POSZ
            //  face 5 = NEGZ

            switch (iface) {
              case 1:
                _CUBECAM->_vmatrix.lookAt(position, position + POSX, POSY);
                break;
              case 0:
                _CUBECAM->_vmatrix.lookAt(position, position - POSX, POSY);
                break;
              case 2:
                _CUBECAM->_vmatrix.lookAt(position, position + POSY, POSZ * -1);
                break;
              case 3:
                _CUBECAM->_vmatrix.lookAt(position, position - POSY, POSZ);
                break;
              case 4:
                _CUBECAM->_vmatrix.lookAt(position, position + POSZ, POSY);
                break;
              case 5:
                _CUBECAM->_vmatrix.lookAt(position, position - POSZ, POSY);
                break;
            }

            _CUBECAM->_vpmatrix  = _CUBECAM->_vmatrix * _CUBECAM->_pmatrix;
            _CUBECAM->_ivpmatrix = _CUBECAM->_vpmatrix.inverse();
            _CUBECAM->_ivmatrix  = _CUBECAM->_vmatrix.inverse();
            _CUBECAM->_ipmatrix  = _CUBECAM->_pmatrix.inverse();
            _CUBECAM->_frustum.set(_CUBECAM->_vmatrix, _CUBECAM->_pmatrix);
            _CUBECAM->_explicitProjectionMatrix = true;
            _CUBECAM->_explicitViewMatrix       = true;
            _CUBECAM->_aspectRatio              = 1.0f;

            auto probe_pass                        = std::make_shared<ForwardPass>();
            probe_pass->_drawdata                  = &drawdata;
            probe_pass->_rtg_out                   = probe->_cubeRenderRTG;
            probe_pass->_rtg_depth_copy            = _rtg_cube1_depth_copy;
            probe_pass->_renderingPROBE            = true;
            probe_pass->_fwd_pass_layer            = "probe";
            probe_pass->_single_pass_stereo        = false;
            probe->_cubeRenderRTG->_cubeRenderFace = iface;

            cubemapCPD._mono_cam_matrices = _CUBECAM;
            _currentRCFD->_passID         = "PROBE"_crcu;

            topcomp->pushCPD(cubemapCPD);
            _render_dppskyssaocolor(probe_pass);
            topcomp->popCPD();

            _currentContext->debugPopGroup();
          }

          probe->_cubeTexture = probe->_cubeRenderRTG->GetMrt(0)->_texture;
          TXI->generateMipMaps(probe->_cubeTexture.get());
          probe->_dirty = false;
        }
        break;
      }
      default:
        break;
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ForwardPbrNodeImpl::_update_shadow_maps(){
  if (_enumeratedLights) {
    auto topcomp = _currentRCFD->topCompositor();
    _currentRCFD->_renderingmodel  = "DEPTH_PREPASS"_crcu;
    _currentRCFD->_passID          = "SHADOW"_crcu;
    int num_shadow_casters = 0;
    for (auto light : _enumeratedLights->_alllights) {
      if (not light->_castsShadows)
        continue;

      if (auto as_spotlight = dynamic_cast<SpotLight*>(light)) {

        if (light->_depthRTG == nullptr) {
          auto depcookie = light->_cookieDepth;
          OrkAssert(depcookie);
          auto rtg         = depcookie->createRenderTarget(_currentContext);
          rtg->_depthOnly  = true;
          light->_depthRTG = rtg;
        }

        CompositingPassData shadowCPD = _currentCIMPL->topCPD().clone();

        _SHADOWCAM->_pmatrix                  = as_spotlight->mProjectionMatrix;
        _SHADOWCAM->_vmatrix                  = as_spotlight->mViewMatrix;
        _SHADOWCAM->_vpmatrix                 = _SHADOWCAM->_vmatrix * _SHADOWCAM->_pmatrix;
        _SHADOWCAM->_ivpmatrix                = _SHADOWCAM->_vpmatrix.inverse();
        _SHADOWCAM->_ivmatrix                 = _SHADOWCAM->_vmatrix.inverse();
        _SHADOWCAM->_ipmatrix                 = _SHADOWCAM->_pmatrix.inverse();
        _SHADOWCAM->_frustum                  = as_spotlight->mWorldSpaceLightFrustum;
        _SHADOWCAM->_explicitProjectionMatrix = true;
        _SHADOWCAM->_explicitViewMatrix       = true;
        _SHADOWCAM->_aspectRatio              = 1.0f;

        shadowCPD._mono_cam_matrices = _SHADOWCAM;

        // FBI->validateRtGroup(light->_depthRTG.get());
        _currentDrawQueue->enqueueLayerToRenderQueue("depth_prepass", _currentIRenderer);

        topcomp->pushCPD(shadowCPD);
        auto FBI = _currentContext->FBI();
        FBI->PushRtGroup(light->_depthRTG.get());

        _currentIRenderer->drawEnqueuedRenderables(true);

        FBI->PopRtGroup();
        topcomp->popCPD();
      }
      num_shadow_casters++;
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ForwardPbrNodeImpl::_render_top(CompositorDrawData& drawdata) {
  EASY_BLOCK("pbr-_render");

  auto context = drawdata.context();
  auto FBI     = context->FBI();
  auto TXI     = context->TXI();
  auto CIMPL   = drawdata._cimpl;
  auto RCFD    = drawdata.RCFD();
  auto topcomp = RCFD->topCompositor();

  int node_frame = _node->_frameIndex;
  RCFD->setUserProperty("noise_seed"_crcu, node_frame);
  // printf( "node_frame<%d>\n", node_frame );
  /////////////////////////////////////////////////
  // enumerate lights / PBR
  /////////////////////////////////////////////////

  if (auto lmgr = CIMPL->lightManager()) {
    EASY_BLOCK("lights-1");
    const auto TOPCPD = CIMPL->topCPD();
    lmgr->enumerateInPass(TOPCPD, _enumeratedLights);
  }

  //////////////////////////////////////////////////////
  // Resize RenderTargets
  //////////////////////////////////////////////////////

  _currentWidth  = drawdata.property("OutputWidth"_crcu).get<int>();
  _currentHeight = drawdata.property("OutputHeight"_crcu).get<int>();

  uint64_t rtg_key = _node->_bufferKey;
  auto rtg_main    = _rtgs_main->fetch(rtg_key);

  if (rtg_main->width() != _currentWidth or rtg_main->height() != _currentHeight) {
    rtg_main->Resize(_currentWidth, _currentHeight);
  }
  rtg_main->_autoclear = false;

  //////////////////////////////////////////////////////
  // get draw queue (otherwise we cant draw anything)
  //////////////////////////////////////////////////////

  auto autorelease_fpbr_rgroup = context->debugPushGroup("ForwardPBR::render",true);
  _currentDrawQueue = RCFD->GetDB();
  if(nullptr == _currentDrawQueue) {
    return;
  }

  //////////////////////////////////////////////////////
  // cache some rendering state
  //////////////////////////////////////////////////////

  _currentViewData = drawdata.computeViewData();
  _currentRCFD = RCFD;
  _currentIRenderer = drawdata.property("irenderer"_crcu).get<lev2::IRenderer*>();
  _currentContext = context;
  _currentCIMPL = CIMPL;

  ////////////////////////////
  // generate and push composition pass data
  ////////////////////////////

  RCFD->_pbrcommon = _node->_pbrcommon;


  auto CPD               = CIMPL->topCPD();
  CPD._mono_cam_matrices = drawdata.property("defcammtx"_crcu).get<cameramatrices_ptr_t>();
  CPD.assignLayers("depth_prepass,std_forward,probe,depth_probe");
  CPD._clearColor = _node->_pbrcommon->_clearColor;
  RtGroupRenderTarget rt(rtg_main.get());
  CPD._irendertarget = &rt;
  CPD.SetDstRect(context->mainSurfaceRectAtOrigin());
  CPD._width  = _currentWidth;
  CPD._height = _currentHeight;

  context->debugMarker(FormatString("ForwardPBR::preclear"));

  rtg_main->_autoclear      = true;
  rtg_main->_clearMaskDepth = true;
  rtg_main->_clearMaskColor = true;
  rtg_main->_clearDepth     = 1.0f;
  rtg_main->_clearColor     = _node->_pbrcommon->_clearColor;
  FBI->PushRtGroup(rtg_main.get()); // creates and clears...
  FBI->PopRtGroup();

  CIMPL->pushCPD(CPD);

  ////////////////////////////
  // shadow passes
  //  these only need to be done once per final-frame
  ////////////////////////////

  if (1) {
    _update_shadow_maps();
  }

  /////////////////////////////////////////////////
  // update enviroment probes
  /////////////////////////////////////////////////

  _update_env_probes(drawdata);

  ////////////////////////////
  // main pass
  ////////////////////////////

  context->debugPushGroup("ForwardPBR::MAIN RTG PASS");

  //_main_pass->_node                  = _node;
  _main_pass->_drawdata              = &drawdata;
  _main_pass->_rtg_out               = rtg_main;
  _main_pass->_rtg_depth_copy        = _rtg_main_depth_copy;
  _main_pass->_rtg_depth_copy_linear = _rtg_main_depth_copy_linear;
  _main_pass->_renderingPROBE        = false;
  _main_pass->_single_pass_stereo    = CPD._single_pass_stereo;

  RCFD->_passID = "MAIN"_crcu;

  _render_dppskyssaocolor(_main_pass);

  CIMPL->popCPD();

  context->debugPopGroup();

  ////////////////////////////
  // resolve msaa
  ////////////////////////////

  if (_rtgs_resolve_msaa) {
    context->debugPushGroup("ForwardPBR::MSAA RESOLVE");
    auto FBI      = context->FBI();
    RCFD->_passID = "MSAARESOLVE"_crcu;
    FBI->msaaBlit(rtg_main, _rtgs_resolve_msaa->fetch(rtg_key));
    context->debugPopGroup();
  }
}
} // namespace ork::lev2::pbr
