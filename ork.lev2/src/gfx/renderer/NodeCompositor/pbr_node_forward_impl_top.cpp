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

    auto rtb1 = _rtg_ambocc_accum->createRenderTarget(EBufferFormat::R32F);
    auto rtb2 = _rtg_ambocc_accum2->createRenderTarget(EBufferFormat::R32F);
    _rtg_main_depth_copy_linear = std::make_shared<RtGroup>(context, 8, 8);
    auto rtb3 = _rtg_main_depth_copy_linear->createRenderTarget(EBufferFormat::R32F);
    rtb1->_debugName = "SSAO-Accum1";
    rtb2->_debugName = "SSAO-Accum2";
    rtb3->_debugName = "SSAO-LinDepth";
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
    _tek_ssao_prepass = _ssao_material->technique("framefx_ssao_prepass");
    _tek_ssao_lindepth = _ssao_material->technique("framefx_linearize_depth");

    _fxpSSAOMVP             = _ssao_material->param("mvp");
    _fxpSSAONumSamples      = _ssao_material->param("SSAONumSamples");
    _fxpSSAONumSteps        = _ssao_material->param("SSAONumSteps");
    _fxpSSAOBias            = _ssao_material->param("SSAOBias");
    _fxpSSAORadius          = _ssao_material->param("SSAORadius");
    _fxpSSAOWeight          = _ssao_material->param("SSAOWeight");
    _fxpSSAOFeedback        = _ssao_material->param("SSAOFeedback");
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

  if (true) { //pbrcommon->_useDepthPrepass) {
    // depth prepass
    _render_dpp(fpass);
  }

  ///////////////////////////////////////////////////////////////////////////
  // SSAO Linearize depth
  ///////////////////////////////////////////////////////////////////////////

  bool is_ssao_active = (pbrcommon->_ssaoNumSamples >= 8);
  if (is_ssao_active) {
    // linearize depth -> fpass->_rtg_depth_copy_linear
    //_render_ssao_linearize_depth(fpass);
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

  if ( is_ssao_active) {
    _render_ssao_prepass(fpass);
  } else {
    // set SSAO to white..
    _currentRCFD->setUserProperty("SSAO_MAP"_crcu, _whiteTexture->GetTexture());
    _currentRCFD->setUserProperty("SSAO_DIM"_crcu, fvec2(8, 8));
    _currentRCFD->setUserProperty("SSAO_POWER"_crcu, 1.0f);
    _currentRCFD->setUserProperty("SSAO_WEIGHT"_crcu, 10.0f);
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
  _rtg_main    = _rtgs_main->fetch(rtg_key);

  if (_rtg_main->width() != _currentWidth or _rtg_main->height() != _currentHeight) {
    _rtg_main->Resize(_currentWidth, _currentHeight);
  }
  _rtg_main->_autoclear = false;

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

  uint32_t prev_dbg_rmodel = RCFD->exchangeDebugRenderingModel(_node->_debugRenderingModel);
  uint32_t prev_dbg_passid = RCFD->exchangeDebugPassID(_node->_debugPassID);
  uint32_t prev_dbg_subpid = RCFD->exchangeDebugSubPassID(_node->_debugSubPassID);

  auto CPD               = CIMPL->topCPD();
  CPD._mono_cam_matrices = drawdata.property("defcammtx"_crcu).get<cameramatrices_ptr_t>();
  CPD.assignLayers("depth_prepass,std_forward,probe,depth_probe");
  CPD._clearColor = _node->_pbrcommon->_clearColor;
  RtGroupRenderTarget rt(_rtg_main.get());
  CPD._irendertarget = &rt;
  CPD.SetDstRect(context->mainSurfaceRectAtOrigin());
  CPD._width  = _currentWidth;
  CPD._height = _currentHeight;

  context->debugMarker(FormatString("ForwardPBR::preclear"));

  _rtg_main->_autoclear      = true;
  _rtg_main->_clearMaskDepth = true;
  _rtg_main->_clearMaskColor = true;
  _rtg_main->_clearDepth     = 1.0f;
  _rtg_main->_clearColor     = _node->_pbrcommon->_clearColor;
  FBI->PushRtGroup(_rtg_main.get()); // creates and clears...
  FBI->PopRtGroup();

  CIMPL->pushCPD(CPD);

  RCFD->setUserProperty("NEAR_FAR"_crcu, fvec2(_currentViewData._near, _currentViewData._far));
  RCFD->setUserProperty("PMATRIX"_crcu, _currentViewData.PL);
  RCFD->setUserProperty("IPMATRIX"_crcu, _currentViewData.PL.inverse());

  ////////////////////////////
  // shadow passes
  //  these only need to be done once per final-frame
  // update enviroment probes
  ////////////////////////////

  _update_shadow_maps();
  _update_env_probes(drawdata);

  ////////////////////////////
  // main pass
  ////////////////////////////

  context->debugPushGroup("ForwardPBR::MAIN RTG PASS");

  //_main_pass->_node                  = _node;
  _main_pass->_drawdata              = &drawdata;
  _main_pass->_rtg_out               = _rtg_main;
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
    FBI->msaaBlit(_rtg_main, _rtgs_resolve_msaa->fetch(rtg_key));
    context->debugPopGroup();
  }

  RCFD->exchangeDebugRenderingModel(prev_dbg_rmodel);
  RCFD->exchangeDebugPassID(prev_dbg_passid);
  RCFD->exchangeDebugSubPassID(prev_dbg_subpid);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
