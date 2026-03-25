////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "fwdnode_impl.h"
#include <ork/util/logger.h>

namespace ork::lev2 {
extern appinitdata_ptr_t _ginitdata;
} // namespace ork::lev2

namespace ork::lev2::pbr {

static logchannel_ptr_t logchan_pbr_fwd = logger()->configureChannel("mtlpbrFWD", fvec3(0.8, 0.8, 0.1), true);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ForwardPbrNodeImpl::ForwardPbrNodeImpl(ForwardNode* node)
    : _node(node)
    , _camname("Camera") { //

  _SHADOWCAM = std::make_shared<CameraMatrices>();
  _CUBECAM   = std::make_shared<CameraMatrices>();
  _primary_pass = std::make_shared<ForwardPass>();

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ForwardPbrNodeImpl::~ForwardPbrNodeImpl() {
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::init(lev2::Context* context, int iw, int ih) {

  if (nullptr == _rtgs_primary) {

    _rtg_primary_depth_copy  = std::make_shared<RtGroup>(context, iw, ih);
    _rtg_cube1_depth_copy = std::make_shared<RtGroup>(context, 8, 8);
    _rtg_ambocc_accum     = std::make_shared<RtGroup>(context, iw, ih);
    _rtg_ambocc_accum2    = std::make_shared<RtGroup>(context, iw, ih);

    auto pbrcommon = _node->_pbrcommon;

    EBufferFormat efmt = EBufferFormat::RGBA8;
    if (pbrcommon->_useFloatColorBuffer) {
      efmt = EBufferFormat::RGBA32F;
    }

    auto e_msaa = intToMsaaEnum(_ginitdata->_msaa_samples);
    _rtgs_primary  = std::make_shared<RtgSet>(context, iw, ih, e_msaa, "rtgs-main", "color"_crcu);
    _rtgs_primary->addBuffer("ForwardRt0", efmt);
    static int buffer_index = 0;
    //_rtgs_primary->_debugName = FormatString("FwdNodePri%d", buffer_index++);

    auto rtb1 = _rtg_ambocc_accum->createRenderTarget(EBufferFormat::R32F);
    auto rtb2 = _rtg_ambocc_accum2->createRenderTarget(EBufferFormat::R32F);
    rtb1->_debugName = "SSAO-Accum1";
    rtb2->_debugName = "SSAO-Accum2";
    _skybox_material           = std::make_shared<PBRMaterial>(context);
    _skybox_material->_variant = "skybox.forward"_crcu;
    _skybox_fxcache            = _skybox_material->pipelineCache();
    _enumeratedLights          = std::make_shared<EnumeratedLights>();

    _par_ublk_std_matrices = _skybox_material->_as_freestyle->uniformBlock("ublk_std_matrices");
    _ubuf_std_matrices = context->FXI()->createUniformBuffer(1024);
    auto mapped  = context->FXI()->mapUniformBuffer(_ubuf_std_matrices);
    //memclr(mapped->data(), mapped->size());
    mapped->unmap();

    /////////////////
    // SSAO
    /////////////////

    _ssao_material = std::make_shared<FreestyleMaterial>();
    _ssao_material->gpuInit(context, "orkshader://framefx");
    _tek_ssao_prepass = _ssao_material->technique("framefx_ssao_prepass");
    _tek_ssao_lindepth = _ssao_material->technique("framefx_linearize_depth");

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
    //
    _fxpSSAOMVP             = _ssao_material->param("mvp");
    _fxpInvP                = _ssao_material->param("inv_p");
    _fxpP                   = _ssao_material->param("p");

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
  auto FBI      = _currentContext->FBI();

  //printf("render dppskyssaocolor rtg<%p>\n", (void*)rtg_out.get());

  /////////////////////////////////////////////////////////////////////////////////////////

  RtGroupRenderTarget rt(rtg_out.get());

  CompositingPassData MY_CPD = _currentCIMPL->topCPD(); // copy top CPD
  auto pbrcommon             = _node->_pbrcommon;
  bool renderingPROBE        = fpass->_renderingPROBE;
  pbrcommon->_useDepthPrepass = false;
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

  ///////////////////////////////////////////////////////////////////////////
  // clear
  ///////////////////////////////////////////////////////////////////////////

  rtg_out->_clearMaskDepth = true;
  rtg_out->_clearMaskColor = true;
  rtg_out->buffer(0)->_clearColor  = _node->_pbrcommon->_clearcolor;
  rtg_out->_autoclear      = true;

  FBI->setViewport(0,0,_currentWidth, _currentHeight);
  FBI->setScissor(0,0,_currentWidth, _currentHeight);

  ///////////////////////////////////////////////////////////////////////////
  // depth prepass
  ///////////////////////////////////////////////////////////////////////////

  if (pbrcommon->_useDepthPrepass) {
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:depth_prepass");
    _render_dpp(fpass);
    _currentRCFD->setUserProperty("DEPTH_MAP"_crcu, rtg_out->_depthBuffer->_texture);
  }
  else{
    _currentRCFD->setUserProperty("DEPTH_MAP"_crcu, rtg_out->_depthBuffer->_texture);

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
  // SSAO prepass
  ///////////////////////////////////////////////////////////////////////////

  if ( is_ssao_active) {
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:ssao");
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

  //FBI->rtGroupClear(rtg_out.get()); // TODO: vulkan 
  FBI->PushRtGroup(rtg_out.get());
  if(_node->_pbrcommon->_enable_skybox){
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:skybox");
    _render_skybox(fpass);
  }
  { 
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:color_pass");
    _render_colorpass(fpass);
  }
  FBI->PopRtGroup();

  ///////////////////////////////////////////////////////////////////////////
  _currentCIMPL->popCPD();
  ///////////////////////////////////////////////////////////////////////////
  auto& ddprops = drawdata->_properties;
  ddprops["depthbuffer"_crcu].set<rtbuffer_ptr_t>(rtg_out->_depthBuffer);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_top(CompositorDrawData& drawdata) {

  //printf("ForwardPBR::render_top\n");

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
    auto pl_buffer = PBRMaterial::lightingDataBuffer(context);
    lmgr->bindEnumeratedToStorageBuffer( context, _enumeratedLights, pl_buffer );
  }

  //////////////////////////////////////////////////////
  // Resize RenderTargets
  //////////////////////////////////////////////////////

  _currentWidth  = drawdata.property("OutputWidth"_crcu).get<int>();
  _currentHeight = drawdata.property("OutputHeight"_crcu).get<int>();

  uint64_t rtg_key = _node->_bufferKey;
  _rtg_primary    = _rtgs_primary->fetch(rtg_key);

  if (_rtg_primary->width() != _currentWidth or _rtg_primary->height() != _currentHeight) {
    _rtg_primary->Resize(_currentWidth, _currentHeight);
  }

  //////////////////////////////////////////////////////
  // get draw queue (otherwise we cant draw anything)
  //////////////////////////////////////////////////////

  OrkProfilerSampleBegin(CHANNEL_GPU, "fwd:total");

  auto autorelease_fpbr_rgroup = context->debugPushGroupAutoRelease("ForwardPBR::render");
  _currentDrawQueue = RCFD->GetDB();
  if(nullptr == _currentDrawQueue) {
    OrkProfilerSampleEnd(CHANNEL_GPU, "fwd:total");
    return;
  }

  //////////////////////////////////////////////////////
  // cache some rendering state
  //////////////////////////////////////////////////////

  _currentViewData = drawdata.computeViewData();
  _currentRCFD = RCFD;
  _currentIRenderer = drawdata.property("irenderer"_crcu).get<lev2::IRenderer*>();
  _currentContext   = context;
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
  CPD.assignLayers("depth_prepass,std_forward,std_editor,probe,depth_probe");
  CPD._clearColor = _node->_pbrcommon->_clearcolor;
  RtGroupRenderTarget rt(_rtg_primary.get());
  CPD._irendertarget = &rt;
  CPD.SetDstRect(ViewportRect(0, 0, _currentWidth, _currentHeight));
  CPD._width  = _currentWidth;
  CPD._height = _currentHeight;

  context->debugMarker(FormatString("ForwardPBR::preclear"));

  CIMPL->pushCPD(CPD);

  //printf("_currentViewData._near<%f> _currentViewData._far<%f>\n", _currentViewData._near, _currentViewData._far);
  RCFD->setUserProperty("NEAR_FAR"_crcu, fvec2(_currentViewData._near, _currentViewData._far));
  RCFD->setUserProperty("PMATRIX"_crcu, _currentViewData.PL);
  RCFD->setUserProperty("VMATRIX"_crcu, _currentViewData.VL);
  RCFD->setUserProperty("VPMATRIX"_crcu, _currentViewData.VPL);
  RCFD->setUserProperty("IVMATRIX"_crcu, _currentViewData.VL.inverse());
  RCFD->setUserProperty("IVPMATRIX"_crcu, _currentViewData.IVPL);
  RCFD->setUserProperty("IPMATRIX"_crcu, _currentViewData.PL.inverse());

  ////////////////////////////
  // shadow passes
  //  these only need to be done once per final-frame
  // update enviroment probes
  ////////////////////////////

  { 
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:shadow_maps");
    _update_shadow_maps();
  }
  {
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:env_probes");
    _update_env_probes(drawdata);
  }

  ////////////////////////////
  // primary pass
  ////////////////////////////

  context->debugPushGroup("ForwardPBR::PRIMARY RTG PASS");

  //_primary_pass->_node                  = _node;
  _primary_pass->_drawdata              = &drawdata;
  _primary_pass->_rtg_out               = _rtg_primary;
  _primary_pass->_rtg_depth_copy        = _rtg_primary_depth_copy;
  _primary_pass->_rtg_depth_copy_linear = _rtg_primary_depth_copy_linear;
  _primary_pass->_renderingPROBE        = false;
  _primary_pass->_single_pass_stereo    = CPD._single_pass_stereo;

  RCFD->_passID = "PRIMARY"_crcu;

  _render_dppskyssaocolor(_primary_pass);

  CIMPL->popCPD();

  context->debugPopGroup();

  RCFD->exchangeDebugRenderingModel(prev_dbg_rmodel);
  RCFD->exchangeDebugPassID(prev_dbg_passid);
  RCFD->exchangeDebugSubPassID(prev_dbg_subpid);

  OrkProfilerSampleEnd(CHANNEL_GPU, "fwd:total");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
