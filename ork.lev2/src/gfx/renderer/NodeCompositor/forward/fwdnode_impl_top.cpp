////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "fwdnode_impl.h"
#include <ork/util/logger.h>
// Needed for scene->layersForRole() in the compositor's layer list
// assembly (forward decl in lev2_types.h isn't enough — we call a
// member function, so require the full Scene definition here).
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/renderer/hzb.h>
#include <ork/lev2/vr/vr.h> // SPVR: the eye/center cameras the layered pyramid registers against
#include <ork/lev2/gfx/renderphasestats.h> // perf HUD render-phase timing sink

namespace ork::lev2 {
extern appinitdata_ptr_t _ginitdata;
} // namespace ork::lev2

namespace ork::lev2::pbr {

static logchannel_ptr_t logchan_pbr_fwd = logger()->configureChannel("mtlpbrFWD", fvec3(0.8, 0.8, 0.1), true);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ORKID_HZB_ALLOW_SAMEFRAME — REVERT KNOB for the strictly-earlier HZB seed guard.
//  Armed, the readiness test drops back to "this key has been seeded at all", which is
//  the pre-guard behaviour: a pyramid may then be built from depth RECORDED THIS FRAME
//  (second eye / second compositor pass). That is the state the guard exists to forbid,
//  and the only way a cull oracle can prove it can SEE the state when it is present —
//  a negative control that cannot be armed is not a control.
//  Default OFF; the guarded path is byte-identical to the unarmed tree.
//  Read once, announced once: a control nobody can prove was armed is not a control.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool hzbAllowSameFrameDepth() {
  static const bool _armed = []() -> bool {
    auto env = std::getenv("ORKID_HZB_ALLOW_SAMEFRAME");
    bool on  = env and (std::string(env) == "1");
    if (on) {
      printf("[FWD:HZB] ORKID_HZB_ALLOW_SAMEFRAME=1 — strictly-earlier seed guard REVERTED "
             "(same-frame depth may feed the pyramid)\n");
      fflush(stdout);
    }
    return on;
  }();
  return _armed;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ORKID_SPVR_HZB=0 — the escape hatch on the single-pass-stereo occlusion pyramid. ON by
//  default, exactly as the pyramid is on the mono chain (ORKID_HZB_OCCLUSION, default 2,
//  remains the one switch that turns occlusion off everywhere): stereo culling that only
//  works when a knob is set is stereo culling that does not work. This knob exists so a
//  stereo run can be priced with and without the pyramid without disturbing the mono
//  contract. Read once, announced once when it disarms the path.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static bool spvrLayeredHzbEnabled() {
  static const bool _armed = []() -> bool {
    auto env = std::getenv("ORKID_SPVR_HZB");
    bool off = env and (std::string(env) == "0");
    if (off) {
      printf("[SPVR:HZB] ORKID_SPVR_HZB=0 — layered occlusion pyramid DISARMED; per-view culls "
             "are frustum-only under single-pass stereo\n");
      fflush(stdout);
    }
    return not off;
  }();
  return _armed;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ForwardPbrNodeImpl::ForwardPbrNodeImpl(ForwardNode* node)
    : _node(node)
    , _camname("Camera") { //

  _SHADOWCAM = std::make_shared<CameraMatrices>();
  _CUBECAM   = std::make_shared<CameraMatrices>();
  _SUNCAM    = std::make_shared<CameraMatrices>();
  _COOKIECAM = std::make_shared<CameraMatrices>();
  for (int i = 0; i < SunCullSetPlan::kMaxSets; i++) // one union sun ortho per cullset
    _CULLCAM[i] = std::make_shared<CameraMatrices>();
  _primary_pass = std::make_shared<ForwardPass>();

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ForwardPbrNodeImpl::~ForwardPbrNodeImpl() {
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Build (or rebuild) the MSAA-aware primary RtgSet. --msaa LEVEL (0=off,1=2x,...) -> hw sample count,
// clamped to the device max. RGBA32F is too heavy to multisample, so color drops to RGBA16F when MSAA
// is on. Records the level it built at (_msaa_level_built) so DoRender can rebuild if the scene `msaa=`
// param arrives AFTER this node's early doGpuInit() build (the param set order vs node init isn't fixed).
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ForwardPbrNodeImpl::_buildPrimaryRtgs(lev2::Context* context, int iw, int ih) {
  int   level    = _ginitdata ? _ginitdata->_msaa_samples : 0;
  int   devmax   = context->msaaMaxSamples();
  int   clamped  = msaaForwardSampleCount(context);
  auto  e_msaa   = msaaSamplesFromInt(clamped);
  bool  msaa_on  = (clamped > 1);
  EBufferFormat efmt = msaa_on ? EBufferFormat::RGBA16F : EBufferFormat::RGBA32F;
  _msaa_level_built  = level;
  if (msaa_on)
    logchan_pbr_fwd->log("ForwardPBR MSAA: level<%d> -> %dx (device max %d), color=RGBA16F", level, clamped, devmax);
  _rtgs_primary = std::make_shared<RtgSet>(context, iw, ih, e_msaa, "rtgs-main", "color"_crcu);
  // SPVR: one 2-layer multiview group instead of one 2D group rendered twice. Depth is
  //  layered with the color (a 1-layer depth against a 2-layer color is an instant
  //  validation error), which is what gives each eye its own depth image.
  if (_node->_singlePassStereo) {
    _rtgs_primary->_numLayers = 2;
    _rtgs_primary->_multiview = true;
  }
  _rtgs_primary->addBuffer("ForwardRt0", efmt);
  _rtgs_primary->addBuffer("ForwardRt1", efmt);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::init(lev2::Context* context, int iw, int ih) {

  if (nullptr == _rtgs_primary) {

    _rtg_primary_depth_copy  = std::make_shared<RtGroup>(context, iw, ih);
    _rtg_cube1_depth_copy = std::make_shared<RtGroup>(context, 8, 8);
    _rtg_ambocc_accum     = std::make_shared<RtGroup>(context, iw, ih);
    _rtg_ambocc_accum2    = std::make_shared<RtGroup>(context, iw, ih);

    auto pbrcommon = _node->_pbrcommon;

    (void)pbrcommon;

    // The MSAA-aware primary RtgSet (color MRTs + msaa). Built here AND re-buildable per-frame in
    // DoRender if the scene `msaa=` level changes after this early init (see _buildPrimaryRtgs).
    _buildPrimaryRtgs(context, iw, ih);
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

    /////////////////
    // QUARTER-RES SUN SHAFTS
    /////////////////

    _init_hazeshaft(context);

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
  MY_CPD._debugName = "_render_dppskyssaocolor";
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

  ///////////////////////////////////////////////////////////////////////////
  // clear
  ///////////////////////////////////////////////////////////////////////////

  rtg_out->_clearMaskDepth = true;
  rtg_out->_clearMaskColor = true;
  rtg_out->buffer(0)->_clearColor  = _node->_pbrcommon->_clearcolor;
  // P3.B — target1 (diffuse irradiance) cleared to opaque black so SSSS
  // post-pass sees a clean buffer if a pixel was untouched by the forward
  // pass (e.g. uncovered viewport background).
  if(rtg_out->numImageBuffers() > 1) {
    rtg_out->buffer(1)->_clearColor = fvec4(0, 0, 0, 1);
  }
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
  // SSAO's prepass binds the scene depth as a plain sampler2D and writes ONE screen-space
  //  accumulation buffer. Under SPVR the depth is a 2-layer array and there is no per-view
  //  accumulation, so running it would shade both eyes from one view's occlusion. Refuse
  //  BY NAME rather than render a quietly wrong frame; a per-layer SSAO is its own task.
  if (is_ssao_active and fpass->_single_pass_stereo) {
    static bool s_spvr_ssao_warned = false;
    if (not s_spvr_ssao_warned) {
      s_spvr_ssao_warned = true;
      printf("[FWD:SPVR] ERROR single-pass-stereo pass with ssaoNumSamples<%d> — SSAO is NOT "
             "layered and is DISABLED for this pass (it would shade both eyes from one view). "
             "Set ssaoNumSamples=0 for VR.\n",
             pbrcommon->_ssaoNumSamples);
      fflush(stdout);
    }
    is_ssao_active = false;
  }
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
  // When depth prepass is active, flip the rtg's depth buffer into
  // read-only mode for the color pass. This lets shaders that bind
  // RCFD_DEPTH_MAP (e.g. water translucency) sample the already-filled
  // depth texture while it's still bound as a read-only depth attachment.
  // One-shot: VkFrameBufferInterface resets the mode after the matching
  // PopRtGroup below.
  // With NO prepass the color pass IS the depth-producing pass, so the
  // read-only flag must not reach it from ANY earlier consumer (the HZB
  // build below samples this same depth and used to leave the flag set:
  // every opaque draw then lost its depth write and the frame collapsed to
  // painter order). Demand the mode this pass needs instead of assuming it.
  if (pbrcommon->_useDepthPrepass) {
    FBI->transitionDepthForSampling(rtg_out);
  } else {
    FBI->transitionDepthForWriting(rtg_out);
  }
  ///////////////////////////////////////////////////////////////////////////
  // QUARTER-RES SUN SHAFTS, half one. Between the depth transition and the
  // color pass's push, because it samples the prepass depth (so it needs the
  // sampling layout) and renders into its OWN half-dims target (so it cannot
  // be inside the color pass). The composite half runs later, from inside
  // _render_colorpass. No-op in every mode but 2.
  ///////////////////////////////////////////////////////////////////////////
  {
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:hazeshaft");
    _render_hazeshaft(fpass);
  }
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
// frame PROLOGUE — view-independent work, exactly ONCE per composited frame
//   (called from NodeCompositingTechnique::assemble BEFORE the assembler's eye
//    fan-out; on a per-eye output path _render_top then runs once per eye). Absorbs:
//    light enumeration + lighting-SSBO packing (world-space data, enumerateInPass
//    culling disabled), spotlight shadow-map renders, env-probe cube captures.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_prologue(CompositorDrawData& drawdata) {

  EASY_BLOCK("pbr-_prologue");
  OrkProfilerSampleScope(CHANNEL_GPU, "fwd:prologue");

  auto context = drawdata.context();
  auto CIMPL   = drawdata._cimpl;
  auto RCFD    = drawdata.RCFD();

  // frame identity is the context's target frame (one increment per EndFrame —
  // stable across both eyes, which render inside ONE context frame).
  // SILENT same-frame dedup, first assemble wins (the Scene::gpuUpdate /
  // Scene::_invokeFramePrologueHooks contract): shared-scene multi-viewport
  // configs legitimately assemble this node several times per context frame
  // (each SceneGraphViewport repaint runs its own assemble), and a miswired
  // per-eye prologue call becomes a no-op instead of doubled work.
  int this_frame = context->GetTargetFrame();
  if (_prologueTargetFrame == this_frame)
    return;
  _prologueTargetFrame = this_frame;

  auto autorelease_group = context->debugPushGroupAutoRelease("ForwardPBR::prologue");

  // establish the impl-wide "current" state the shadow/probe sub-passes consume
  // (re-established per-eye by _render_top with the eye's own view data).
  _currentContext  = context;
  _currentCIMPL    = CIMPL;
  _currentRCFD     = RCFD;
  RCFD->_pbrcommon = _node->_pbrcommon;

  /////////////////////////////////////////////////
  // BAKED IBL COLD START (CommonStuff::drainPendingRadianceMapLoad). Ahead of
  // every consumer below, because a skybox load that has not published yet
  // lights this whole frame off a black environment and no later frame repairs
  // the one a capture took. Costs one pointer test once the set is live.
  /////////////////////////////////////////////////

  if (_node->_pbrcommon)
    _node->_pbrcommon->drainPendingRadianceMapLoad(context);

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

  /////////////////////////////////////////////////
  // shadow / probe sub-passes render scene geometry — without a draw queue
  //  there is nothing to render (the per-eye color pass bails identically).
  /////////////////////////////////////////////////

  _currentDrawQueue = RCFD->GetDB();
  if (nullptr == _currentDrawQueue)
    return;
  _currentIRenderer = drawdata.property("irenderer"_crcu).get<lev2::IRenderer*>();

  // The output node's per-view CPD is NOT pushed yet (the prologue precedes the
  // assembler), so build the frame's mono view state here with the same camera
  // pick CompositingPassData::defaultSetup uses (sim camera wins over default).
  // The probe color passes consume the resulting NEAR_FAR/P/IP RCFD props (SSAO
  // reconstruction binds); shadow/probe passes push their own light/cube-face
  // cameras on top of this CPD.
  CompositingPassData CPD = CIMPL->topCPD().clone();
  CPD._debugName          = "fwd:prologue";
  if (auto try_sim = drawdata.property("simcammtx"_crcu).tryAs<cameramatrices_ptr_t>())
    CPD._mono_cam_matrices = try_sim.value();
  else if (auto try_def = drawdata.property("defcammtx"_crcu).tryAs<cameramatrices_ptr_t>())
    CPD._mono_cam_matrices = try_def.value();

  // "OutputWidth"/"OutputHeight" are output-node beginAssemble products — not
  // available yet. The compositor context dims are the same source the default
  // camera's aspect derives from; they only feed the prologue's transient
  // viewport set + SSAO accum sizing (probe render targets size themselves).
  const auto& cctx = CIMPL->compositingContext();
  _currentWidth    = cctx.miWidth;
  _currentHeight   = cctx.miHeight;

  uint32_t prev_dbg_rmodel = RCFD->exchangeDebugRenderingModel(_node->_debugRenderingModel);
  uint32_t prev_dbg_passid = RCFD->exchangeDebugPassID(_node->_debugPassID);
  uint32_t prev_dbg_subpid = RCFD->exchangeDebugSubPassID(_node->_debugSubPassID);

  CIMPL->pushCPD(CPD);

  _currentViewData = drawdata.computeViewData();
  RCFD->setUserProperty("NEAR_FAR"_crcu, fvec2(_currentViewData._near, _currentViewData._far));
  RCFD->setUserProperty("PMATRIX"_crcu, _currentViewData.PL);
  RCFD->setUserProperty("VMATRIX"_crcu, _currentViewData.VL);
  RCFD->setUserProperty("VPMATRIX"_crcu, _currentViewData.VPL);
  RCFD->setUserProperty("IVMATRIX"_crcu, _currentViewData.VL.inverse());
  RCFD->setUserProperty("IVPMATRIX"_crcu, _currentViewData.IVPL);
  RCFD->setUserProperty("IPMATRIX"_crcu, _currentViewData.PL.inverse());

  {
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:shadow_maps");
    RenderPhaseScope _shmaps("shadow-maps"); // always-on: the profiler scope above vanishes in default builds
    _update_shadow_maps();
  }
  {
    // BEFORE the cascades: _update_sun_cascades writes the cookie fields of
    // ublk_sun at its very top, so a cookie filled after it would arrive a
    // frame late (and a scene captured in one frame would show none at all).
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:sun_cookie");
    RenderPhaseScope _suncookie("sun-cookie");
    _update_sun_cookie();
  }
  {
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:sun_cascades");
    RenderPhaseScope _suncasc("sun-cascades");
    _update_sun_cascades();
  }
  {
    // BEFORE the probe captures — those run the skybox pass themselves, so in
    // procedural mode they need this frame's sky-view LUT (and the sky frame
    // state published with it) to already be in place.
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:sky_luts");
    RenderPhaseScope _skylut("sky-lut"); // always-on: the profiler scope above vanishes in default builds
    _update_sky_luts();
  }
  {
    // consumes the SKY_FRAME the step above just published; renders nothing
    // except on the frame a refilter cycle begins.
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:sky_ibl");
    RenderPhaseScope _skyibl("sky-ibl");
    _update_sky_ibl();
  }
  {
    OrkProfilerSampleScope(CHANNEL_GPU, "fwd:env_probes");
    RenderPhaseScope _envprobes("env-probes");
    _update_env_probes(drawdata);
  }

  CIMPL->popCPD();

  RCFD->exchangeDebugRenderingModel(prev_dbg_rmodel);
  RCFD->exchangeDebugPassID(prev_dbg_passid);
  RCFD->exchangeDebugSubPassID(prev_dbg_subpid);
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

  // PROLOGUE CONTRACT: _render_prologue must have run for THIS target frame —
  //  a compositing path that bypasses NodeCompositingTechnique::assemble's
  //  renderPrologue call would render with stale lights/shadows/probes.
  if (_prologueTargetFrame != context->GetTargetFrame()) {
    printf(
        "[FWD:PROLOGUE] ERROR _render_top for target frame<%d> without a same-frame "
        "prologue (last prologue frame<%d>) — lights/shadows/probes would be stale\n",
        context->GetTargetFrame(), _prologueTargetFrame);
    OrkAssert(false);
  }

  int node_frame = _node->_frameIndex;
  RCFD->setUserProperty("noise_seed"_crcu, node_frame);
  // printf( "node_frame<%d>\n", node_frame );

  //////////////////////////////////////////////////////
  // Resize RenderTargets
  //////////////////////////////////////////////////////

  _currentWidth  = drawdata.property("OutputWidth"_crcu).get<int>();
  _currentHeight = drawdata.property("OutputHeight"_crcu).get<int>();

  // The scene `msaa=` level can land AFTER this node's early doGpuInit() build (param-set order
  // vs node init isn't fixed). Rebuild the primary RtgSet here, once, when the level changed.
  bool rtg_fresh = false;
  if (_ginitdata and _ginitdata->_msaa_samples != _msaa_level_built) {
    _buildPrimaryRtgs(context, _currentWidth, _currentHeight);
    rtg_fresh = true;
  }

  uint64_t rtg_key = _node->_bufferKey;
  _rtg_primary    = _rtgs_primary->fetch(rtg_key);

  if (_rtg_primary->width() != _currentWidth or _rtg_primary->height() != _currentHeight) {
    _rtg_primary->Resize(_currentWidth, _currentHeight);
    rtg_fresh = true;
  }
  if (rtg_fresh) {
    _hzb_recorded_rtgs.erase(rtg_key); // fresh depth image — unrecorded until re-rendered
  }

  // 1-phase occlusion HZB — built at FRAME START from LAST frame's depth. _rtg_primary is keyed-fetched
  // (line above), so before this frame's passes overwrite it, its depth holds the PREVIOUS frame's
  // resolved + GPU-COMPLETE depth (that frame's command buffer already presented). Building here (not at
  // frame end) fixes the hazard where the HZB's OWN compute submission ran before this frame's depth
  // passes + the sampling-layout transition. The per-view cull reads the resulting HZB SSBO off the Scene
  // next preRender. (MSAA: also depends on the depth resolve into the single-sample _imgobj working.)
  // mode 0 => the pyramid has no consumer this run (both per-view culls skip it on the same
  // mode), so BUILDING it is pure cost. Scene::_hzb simply stays null; the culls read the
  // stamped handle back as null and stay frustum-only, exactly as they do before first build.
  // permanent extent backstop against the HZB-vs-resize bug class: a pyramid built at a prior
  // extent whose base dims no longer match the extent it would be built at NOW must NOT be
  // consumed — its stale mip0 clamp maps this frame's NDC onto the wrong footprint (false culls
  // / banding). On mismatch occlusion is disabled for the frame; the build below re-validates at
  // the new extent once the depth is seeded. The expectation is derived from the DEPTH IMAGE the
  // builder actually samples (_baseW == depthW/2 by construction in HZBBuilder::_ensure), not
  // from the rtg's own width: those two agree on the mono chain, but assuming the rtg relation
  // is what would latch the pyramid permanently invalid the moment a stereo target's depth
  // extent stops matching its color extent. No depth texture yet => nothing to compare against,
  // and the readiness test below rejects the build anyway.
  auto hzb_extent_backstop = [&](hzbbuilder_ptr_t hzb) {
    if (not hzb)
      return;
    auto depthtex = (_rtg_primary and _rtg_primary->_depthBuffer) ? _rtg_primary->_depthBuffer->_texture : nullptr;
    if (not depthtex)
      return;
    int expect_baseW = std::max(1, depthtex->_width / 2);
    int expect_baseH = std::max(1, depthtex->_height / 2);
    if (hzb->_baseW != expect_baseW or hzb->_baseH != expect_baseH)
      hzb->_valid = false;
  };
  // SPVR: the pyramid IS built under single-pass stereo, from the 2-layer ARRAY depth the one
  //  multiview pass wrote, via the layered mip0 kernel that reduces over BOTH eye layers
  //  (hzb.cpp, cs_hzb_mip0_layered). A texel then occludes only what BOTH eyes agree is
  //  occluded — the under-occlude-only direction. The eye cameras go WITH the depth because
  //  agreement alone is not registration: the cull indexes the pyramid with the center
  //  camera, and the kernel needs the measured eye->center displacement to widen its
  //  reduction over. Misregistered occlusion removes geometry that should survive.
  //  ORDERING: same admission test as the dual-pass chain below — the LATEST frame that
  //  recorded this rtg's depth (_hzb_recorded_rtgs) must be STRICTLY earlier than this one —
  //  with a rejection that says so out loud rather than degrading silently.
  if (_node->_singlePassStereo) {
    auto* hzbscene = (spvrLayeredHzbEnabled() and _node->_pbrcommon) ? _node->_pbrcommon->_scene : nullptr;
    //////////////////////////////////////////////////////////////////////
    // THE PYRAMID IS ONLY MEANINGFUL TO A CULL THAT USES THE HEAD CAMERA.
    //  Scene::_renderIMPL picks the per-view cull camera by exactly this predicate: with a
    //  tracked pose (or an XR runtime that owns presentation) it culls against the HEAD
    //  camera, which is the camera this pyramid is registered to; otherwise it falls back to
    //  the DESKTOP draw camera ("spawncam"), whose projection has nothing to do with the eye
    //  depth this pyramid was reduced from. Measured with that fallback active (offscreen
    //  stereo, static pose): the armed pyramid removed geometry the disarmed arm draws — 60
    //  to 745 eaten pixels depending on extent, and a different set every head step. So the
    //  build is refused rather than published misregistered, and any pyramid left over from
    //  an earlier frame is invalidated so no consumer keeps reading it.
    //////////////////////////////////////////////////////////////////////
    auto vrdev            = orkidvr::device();
    bool cull_uses_head   = vrdev                                                        //
                          and vrdev->_active                                             //
                          and (vrdev->_trackedPoseValid or vrdev->ownsHmdPresentation()) //
                          and vrdev->_centercamera and vrdev->_leftcamera and vrdev->_rightcamera;
    if (hzbscene and not cull_uses_head) {
      if (hzbscene->_hzb)
        hzbscene->_hzb->_valid = false;
      static bool s_spvr_hzb_nohead = false;
      if (not s_spvr_hzb_nohead) {
        s_spvr_hzb_nohead = true;
        printf("[SPVR:HZB] layered build REFUSED — this frame's per-view cull resolves the DESKTOP "
               "camera, not the head camera (tracked pose absent and the device does not own HMD "
               "presentation). The pyramid is registered to the eye views; indexing it with an "
               "unrelated projection removes visible geometry. Culls stay frustum-only.\n");
        fflush(stdout);
      }
      hzbscene = nullptr;
    }
    if (hzbscene) {
      OrkProfilerSampleScope(CHANNEL_MAIN, "cpu:fwd:hzb:spvr");
      ////////////////////////////////////////////////////////////////////////
      // MSAA note (no longer a caveat, kept because the shape is easy to re-break): under
      //  multisampling there are TWO depth images. The depth TEST runs against the
      //  multisample ATTACHMENT, which no shader can sample, so this pyramid — like every
      //  other sampler-side depth consumer — reads the SINGLE-SAMPLE copy. One multiview
      //  pass cannot fill that copy for more than one view, so the backend resolves the
      //  eyes one at a time after the depth pass (VkRtGroupImpl::_resolveMultiviewDepth).
      //  Before that existed the copy carried no eye depth at all under stereo and the
      //  both-eyes combine read far everywhere, rejecting nothing.
      ////////////////////////////////////////////////////////////////////////
      hzb_extent_backstop(hzbscene->_hzb);
      auto rec_it        = _hzb_recorded_rtgs.find(rtg_key);
      bool have_depth    = (rec_it != _hzb_recorded_rtgs.end());
      int  depth_frame   = have_depth ? rec_it->second : -1;
      bool depth_earlier = have_depth                                //
                           and (hzbAllowSameFrameDepth()             //
                                or (context->GetTargetFrame() > depth_frame));
      if (have_depth and not depth_earlier) {
        // a same-frame stamp means this frame's depth passes are RECORDED but not yet
        // submitted, while this build's dispatch submits on its own — sampling it would read
        // an undefined image. Reject, and say so rather than degrade silently.
        static bool s_spvr_hzb_sameframe = false;
        if (not s_spvr_hzb_sameframe) {
          s_spvr_hzb_sameframe = true;
          printf("[SPVR:HZB] layered build REJECTED — depth for this rtg was recorded THIS frame "
                 "(depth_frame<%d> this_frame<%d>); the pyramid needs strictly-earlier depth\n",
                 depth_frame, int(context->GetTargetFrame()));
          fflush(stdout);
        }
      }
      bool depth_impl_ready = depth_earlier                //
                              and _rtg_primary             //
                              and _rtg_primary->_depthBuffer //
                              and _rtg_primary->_depthBuffer->_texture;
      if (depth_impl_ready) {
        const auto& tex_impl = _rtg_primary->_depthBuffer->_texture->_impl;
        depth_impl_ready     = tex_impl.isSet() and not tex_impl.isA<std::nullptr_t>();
      }
      if (depth_impl_ready) {
        if (not hzbscene->_hzb)
          hzbscene->_hzb = std::make_shared<ork::lev2::HZBBuilder>();
        int depth_layers = _rtg_primary->_depthBuffer->_numLayers;
        // PAIRED transition, same as the mono chain: a compute-side depth consumer with no
        // push/pop of its own, released as soon as the build returns.
        FBI->transitionDepthForSampling(_rtg_primary);
        hzbscene->_hzb->buildLayered(
            context,
            _rtg_primary->_depthBuffer->_texture,
            depth_layers,
            vrdev->_leftcamera.get(),
            vrdev->_rightcamera.get(),
            vrdev->_centercamera.get());
        FBI->transitionDepthForWriting(_rtg_primary);
        hzbscene->_hzb->_sourceDepthFrame = depth_frame;
        ////////////////////////////////////////////////////////////////////////
        // OBSERVABLE ENGAGEMENT, AT THE POINT OF USE. "the branch compiled" and
        //  "no validation error" are both true of a build that never ran; this
        //  line fires from the site that actually dispatched, and names the layer
        //  count and combine direction it dispatched WITH. Once per run.
        //  Grep token: SPVR:HZB
        ////////////////////////////////////////////////////////////////////////
        static bool s_spvr_hzb_built = false;
        if (not s_spvr_hzb_built) {
          s_spvr_hzb_built = true;
          const auto& sreg = hzbscene->_hzb->_stereoReg;
          // the map's affine terms are reported as s/t (what an asymmetric rig makes large)
          // and its PROJECTIVE terms as p (what a CANTED rig makes nonzero — the pair an
          // affine map cannot express, and whose absence used to land in c0).
          printf("[SPVR:HZB] layered pyramid BUILT layers<%d> base<%dx%d> mips<%d> "
                 "combine<max = both-eyes-agree> reg<c0 %.3f c1 %.3f texels> "
                 "map<L s %.4f,%.4f t %.4f,%.4f p %.5f,%.5f | "
                 "R s %.4f,%.4f t %.4f,%.4f p %.5f,%.5f> "
                 "depthframe<%d> thisframe<%d>\n",
                 depth_layers,
                 hzbscene->_hzb->_baseW,
                 hzbscene->_hzb->_baseH,
                 hzbscene->_hzb->_mips,
                 sreg._c0,
                 sreg._c1,
                 sreg._regH[0][0],
                 sreg._regH[0][4],
                 sreg._regH[0][2],
                 sreg._regH[0][5],
                 sreg._regH[0][6],
                 sreg._regH[0][7],
                 sreg._regH[1][0],
                 sreg._regH[1][4],
                 sreg._regH[1][2],
                 sreg._regH[1][5],
                 sreg._regH[1][6],
                 sreg._regH[1][7],
                 depth_frame,
                 int(context->GetTargetFrame()));
          fflush(stdout);
        }
      }
    }
  }
  else if (auto* hzbscene = (_node->_pbrcommon) ? _node->_pbrcommon->_scene : nullptr) {
    OrkProfilerSampleScope(CHANNEL_MAIN, "cpu:fwd:hzb");
    hzb_extent_backstop(hzbscene->_hzb);
    // only sample LAST frame's depth if this rtg's depth passes have actually
    // completed at least once since (re)build/resize — otherwise the depth image
    // is UNDEFINED (or the texture has no backend impl yet) and dispatching
    // leaves u_depth unbound / samples an invalid layout (validation errors).
    // note: Texture::_impl default-initializes to nullptr_t, which counts as
    // "set" for the variant — so exclude that explicitly.
    // the stamp is the frame that RECORDED this rtg's depth passes; that frame's graphics work
    // (including the depth image's creation-time layout barriers) is not submitted until the
    // frame ends, while this dispatch submits on its own. So require a STRICTLY earlier frame
    // — a same-frame stamp (any second compositor pass on this rtg, e.g. a second view)
    // means the depth is recorded but not yet on the queue, and sampling it reads an
    // UNDEFINED image: the pyramid then varies render to render, and with it which marginal
    // instances get culled. The stamp is ASSIGNED every recording frame, so it is the LATEST
    // one: a sticky first-recording stamp would leave "GetTargetFrame() > stamp" true forever
    // and the guard would never fire (which is what it used to do).
    auto rec_it           = _hzb_recorded_rtgs.find(rtg_key);
    bool seed_earlier     = (rec_it != _hzb_recorded_rtgs.end())              //
                            and (hzbAllowSameFrameDepth()                     //
                                 or (context->GetTargetFrame() > rec_it->second));
    bool depth_impl_ready = seed_earlier //
                            and _rtg_primary //
                            and _rtg_primary->_depthBuffer //
                            and _rtg_primary->_depthBuffer->_texture;
    if (depth_impl_ready) {
      const auto& tex_impl = _rtg_primary->_depthBuffer->_texture->_impl;
      depth_impl_ready     = tex_impl.isSet() and not tex_impl.isA<std::nullptr_t>();
    }
    if (depth_impl_ready) {
      if (not hzbscene->_hzb)
        hzbscene->_hzb = std::make_shared<ork::lev2::HZBBuilder>();
      // PAIRED transition: this is a compute-side depth consumer with no
      // push/pop of its own, so nothing else would ever clear the read-only
      // flag. Consumption ends when build() returns — release it here rather
      // than leaving it for whichever pass happens to push this rtg next.
      FBI->transitionDepthForSampling(_rtg_primary);
      hzbscene->_hzb->build(context, _rtg_primary->_depthBuffer->_texture);
      FBI->transitionDepthForWriting(_rtg_primary);
      // PROVENANCE (gate leg (m)): the frame whose depth passes LAST wrote the image this
      //  pyramid was just built from — the same stamp the admission test above accepted, so
      //  equality with Context::GetTargetFrame() at consume time is the direct observable for
      //  the invariant that test encodes.
      hzbscene->_hzb->_sourceDepthFrame = rec_it->second;
    }
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
  // Assemble the compositor's active layer list via the scene's
  // layer-role lookup. With no overrides, layersForRole(role)
  // returns {role}, so the resulting string is byte-identical to
  // the previous hardcoded "depth_prepass,std_forward,...".
  // Overrides redirect or extend any role to custom layer names,
  // enabling layer-swap-based scene isolation.
  {
    // role list lives on the node (ForwardNode::renderedLayerRoles) — the
    // SINGLE source shared with Scene::initWithParams's layer pre-creation
    std::string layer_csv;
    auto* scene = _node->_pbrcommon ? _node->_pbrcommon->_scene : nullptr;
    for (const auto& role : _node->renderedLayerRoles()) {
      if (scene) {
        for (const auto& layer : scene->layersForRole(role)) {
          if (!layer_csv.empty()) layer_csv += ",";
          layer_csv += layer;
        }
      } else {
        if (!layer_csv.empty()) layer_csv += ",";
        layer_csv += role;
      }
    }
    CPD.assignLayers(layer_csv);
  }
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
  // primary pass
  //  (shadow maps + env probes already rendered once-per-frame in _render_prologue)
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
  // depth passes recorded — the LATEST recording frame, which is both what a later frame's
  // HZB build samples and what its admission test compares against (assign, not emplace: a
  // stamp that does not move forward every frame stops being the truth after frame one).
  _hzb_recorded_rtgs[rtg_key] = context->GetTargetFrame();

  CIMPL->popCPD();

  context->debugPopGroup();

  RCFD->exchangeDebugRenderingModel(prev_dbg_rmodel);
  RCFD->exchangeDebugPassID(prev_dbg_passid);
  RCFD->exchangeDebugSubPassID(prev_dbg_subpid);

  OrkProfilerSampleEnd(CHANNEL_GPU, "fwd:total");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
