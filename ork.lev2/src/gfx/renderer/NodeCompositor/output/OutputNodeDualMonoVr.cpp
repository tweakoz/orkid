////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/ezapp.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/vr_composite.h>
#include <ork/lev2/vr/vr.h>
#include <ork/lev2/vr/vr_hud_overlay.h>
#include <ork/lev2/gfx/gbi.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/profiling.inl>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::DualMonoVrOutputNode, "DualMonoVrOutputNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
struct DMVRIMPL {
  ///////////////////////////////////////
  DMVRIMPL(DualMonoVrOutputNode* node)
      : _vrnode(node) {
    _tmpcameramatrices = std::make_shared<CameraMatrices>();
    _stereomatrices    = std::make_shared<StereoCameraMatrices>();
  }
  ///////////////////////////////////////
  ~DMVRIMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* context) {
    if (_doinit) {
      int width  = orkidvr::device()->_width * 2 * (_vrnode->supersample() + 1);
      int height = orkidvr::device()->_height * (_vrnode->supersample() + 1);

      _blit2screenmtl.gpuInit(context, "orkshader://blit");
      _blit2screenmtl._rasterstate->setCullTest(ECullTest::OFF);
      _fxtechnique_downsample[0] = _blit2screenmtl.technique("blituv");
      _fxtechnique_downsample[1] = _blit2screenmtl.technique("downsample_2x2");
      _fxtechnique_downsample[2] = _blit2screenmtl.technique("downsample_3x3");
      _fxtechnique_downsample[3] = _blit2screenmtl.technique("downsample_4x4");
      _fxpMVP                    = _blit2screenmtl.param("MatMVP");
      _fxpColorMap               = _blit2screenmtl.param("ColorMap");
      _fxpDitherAmt              = _blit2screenmtl.param("DitherAmt");
      _ssaadownsamplebufferL     = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
      _ssaadownsamplebufferR     = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
      _ssaadownsamplebufferL->_name = "dmvr.downsampleL";
      _ssaadownsamplebufferR->_name = "dmvr.downsampleR";
      auto dsbufL                = _ssaadownsamplebufferL->createRenderTarget(_vrnode->_format);
      dsbufL->_debugName         = "MsaaDownsampleBufferL";
      auto dsbufR                = _ssaadownsamplebufferR->createRenderTarget(_vrnode->_format);
      dsbufR->_debugName         = "MsaaDownsampleBufferR";

      // head-locked debug HUD panel, drawn per-eye in TWO layers (see _drawHudPanel):
      //  1) a translucent dark SLATE (uidev_modcolor_alpha, straight ALPHA) that dims the
      //     scene 50% over the panel, then
      //  2) the PREMULTIPLIED text/graph RT (uitextured_prema, PREMA over the slate).
      //  Blend is pipeline-baked from the technique state_block (a runtime setBlendingMacro
      //  on a FreestyleMaterial never reaches it — the state_block wins the resolve). The
      //  slate+premult split is required because FontMan's glyph blend (alpha factors
      //  (ONE,ZERO)) punches alpha-0 holes wherever a glyph quad is, so a slate baked into
      //  the RT would show as opaque boxes; keeping the slate separate + premult text is
      //  the standard fringeless, no-double-darken pipeline.
      _hudpanelmtl.gpuInit(context, "orkshader://ui");
      _hudpanel_tek_slate = _hudpanelmtl.technique("uidev_modcolor_alpha");
      _hudpanel_tek_text  = _hudpanelmtl.technique("uitextured_prema");
      _hudpanel_par_mvp  = _hudpanelmtl.param("mvp");
      _hudpanel_par_map  = _hudpanelmtl.param("ColorMap");
      _hudpanel_par_modc = _hudpanelmtl.param("ModColor");
      _hudpanel_vbuf     = std::make_shared<DynamicVertexBuffer<SVtxV16T16C16>>(64, 0);
      _hudpanel_vbuf->SetRingLock(true);

      _doinit = false;
    }
  }
  ///////////////////////////////////////
  // Head-locked debug HUD panel. If the host published a HUD texture into
  //  VrHudOverlay, draw ONE textured quad into THIS eye's just-downsampled buffer,
  //  positioned in head-center space at (0,0,-distance) and projected with THIS
  //  eye's view+projection so natural stereo disparity places it at that depth.
  //  Head-locked: the world placement rides the per-frame center-eye pose
  //  (_stereomatrices->_mono inverse-view = head->world). Alpha-blended over the
  //  eye's final content; a no-op (early-out) whenever the overlay is disabled.
  //  Called from _downsample while the eye's down RTG is still bound.
  void _drawHudPanel(CompositorDrawData& drawdata, bool is_left_eye) {
    auto& overlay = VrHudOverlay::instance();
    if (not overlay._enabled.load())
      return;
    auto tex = overlay._texture;
    if (not tex or not _stereomatrices->_mono)
      return;

    auto context   = drawdata.context();
    auto fbi       = context->FBI();
    auto gbi       = context->GBI();
    auto framedata = drawdata.RCFD();

    // panel geometry in head-center space (meters). Centered on the forward axis,
    //  width fixed, height derived from the published content aspect so text isn't
    //  stretched. Kept well inside the central FOV so lens distortion doesn't eat it.
    const float dist   = overlay._distance_m;
    const float halfW  = overlay._width_m * 0.5f;
    const float aspect = (overlay._aspect > 0.01f) ? overlay._aspect : 1.0f; // w/h
    const float halfH  = halfW / aspect;

    // head-center -> world (inverse view of the mono/center camera), then this
    //  eye's V*P. multiply_ltor(M,V,P): clip = pos_row * M * V * P.
    fmtx4 worldModel = _stereomatrices->_mono->GetIVMatrix();
    fmtx4 mvp        = is_left_eye ? _stereomatrices->MVPL(worldModel) //
                                   : _stereomatrices->MVPR(worldModel);

    // vertical placement (owner call: HUD sits LOW): drop the panel center
    //  _yoffset_frac of the FRAME height. Frame height is MEASURED, not assumed:
    //  project one head-space meter of Y at the panel depth through this eye's
    //  own mvp and read the NDC span — exact for any fov/asymmetric XR frustum,
    //  no projection-convention gamble. (w depends only on depth, so NDC-y is
    //  linear in y at fixed z and the two-point measurement is exact.)
    float yoff = 0.0f;
    if (overlay._yoffset_frac != 0.0f) {
      const fvec4 a        = fvec4(0, 0, -dist, 1).transform(mvp);
      const fvec4 b        = fvec4(0, 1, -dist, 1).transform(mvp);
      const float ndc_per_m = fabsf(b.y / b.w - a.y / a.w);
      if (ndc_per_m > 1e-6f)
        yoff = -overlay._yoffset_frac * (2.0f / ndc_per_m); // full NDC height = 2
    }

    fvec3 TL(-halfW, halfH + yoff, -dist), TR(halfW, halfH + yoff, -dist);
    fvec3 BL(-halfW, -halfH + yoff, -dist), BR(halfW, -halfH + yoff, -dist);

    // Panel-top (+halfH) samples the RT content top. Orkid-Vulkan RTG textures sample
    //  v=0 at row 0 (the same orientation the desktop mirror/ext-viewer read the down
    //  buffers upright), so content-top = v=0. If the in-headset text reads upside-down,
    //  swap the two v literals below (single-line flip — no other change).
    auto uv = [](float u, float v) { return fvec4(u, v, 0, 0); };
    VtxWriter<SVtxV16T16C16> vw;
    vw.Lock(context, _hudpanel_vbuf.get(), 6);
    fvec4 c(1, 1, 1, 1);
    vw.AddVertex(SVtxV16T16C16(TL, uv(0, 0), c));
    vw.AddVertex(SVtxV16T16C16(TR, uv(1, 0), c));
    vw.AddVertex(SVtxV16T16C16(BR, uv(1, 1), c));
    vw.AddVertex(SVtxV16T16C16(TL, uv(0, 0), c));
    vw.AddVertex(SVtxV16T16C16(BR, uv(1, 1), c));
    vw.AddVertex(SVtxV16T16C16(BL, uv(0, 1), c));
    vw.UnLock(context);

    ViewportRect vp(0, 0, _per_eye_width, _per_eye_height);
    fbi->pushViewport(vp);
    fbi->pushScissor(vp);
    auto& mtl = _hudpanelmtl;
    // blend/depth/cull come from each technique's state_block (a runtime setBlendingMacro on
    //  a FreestyleMaterial does NOT reach the baked pipeline blend — the state_block wins).
    // LAYER 1: the SLATE — flat (0,0,0,0.5), straight ALPHA over the scene => 50% dim.
    mtl.begin(_hudpanel_tek_slate, framedata);
    mtl.bindParamMatrix(_hudpanel_par_mvp, mvp);
    mtl.bindParamVec4(_hudpanel_par_modc, fvec4(0, 0, 0, 0.5f));
    gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
    mtl.end(framedata);
    // LAYER 2: the PREMULTIPLIED text/graph RT, PREMA over the slate (no halo/box).
    mtl.begin(_hudpanel_tek_text, framedata);
    mtl.bindParamMatrix(_hudpanel_par_mvp, mvp);
    mtl.bindParamTexture(_hudpanel_par_map, tex.get());
    mtl.bindParamVec4(_hudpanel_par_modc, fvec4(1, 1, 1, 1));
    gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
    mtl.end(framedata);
    fbi->popScissor();
    fbi->popViewport();
  }
  ///////////////////////////////////////
  void _beginAssembleEye(CompositorDrawData& drawdata, bool is_left_eye) {
    EASY_BLOCK("onodevr-begass");
    auto& ddprops = drawdata._properties;
    auto RCFD     = drawdata.RCFD();
    auto CIMPL    = drawdata._cimpl;

    auto VRDEV = orkidvr::device();
    int ssaa   = _vrnode->supersample();
    OrkAssert(ssaa >= 0 and ssaa <= 3);
    _multiplier     = ssaa + 1;
    _per_eye_width  = VRDEV->_width; // * 2;
    _per_eye_height = VRDEV->_height;
    _ssaa_width     = _per_eye_width * _multiplier;
    _ssaa_height    = _per_eye_height * _multiplier;

    /////////////////////////////////////////////////////////////////////////////
    // The eye cameras already carry the walker's world placement: onGpuUpdate composed
    //  the world/root transform into the device user matrix BEFORE this frame's pose
    //  update ran (Device::_updatePosesCommon: cmv = usermtx*base*hmd). So there is
    //  nothing to route per-eye here — every draw path (this node's mono eye passes and
    //  the SSBO-custom mono paths alike) transforms a true-WORLD position by the eye
    //  camera. _outputViewOffsetMatrix == that user matrix (world->root VIEW).
    /////////////////////////////////////////////////////////////////////////////

    _viewOffsetMatrix = orkidvr::device()->_outputViewOffsetMatrix;

    /////////////////////////////////////////////////////////////////////////////
    // NB: the device frame update (orkidvr::device()->gpuUpdate) is NOT called here.
    //  It runs exactly once per frame in onGpuUpdate() before EITHER eye — both eye
    //  passes read the SAME per-frame device state (_leftcamera/_rightcamera/
    //  _centercamera, _outputViewOffsetMatrix) populated by that single update.
    ///////////////////////////////////

    // printf( "B: vr width<%d> height<%d>\n", width, height );

    drawdata._properties["OutputWidth"_crcu].set<int>(_ssaa_width);
    drawdata._properties["OutputHeight"_crcu].set<int>(_ssaa_height);

    // we are not using single pass stereo,
    //  so we will render using a mono camera
    //  and we will switch the mono camera from left to right
    //  depending on the eye we are rendering

    drawdata._properties["SinglePassStereo"_crcu].set<bool>(false);
    auto mono_cam = is_left_eye ? VRDEV->_leftcamera : VRDEV->_rightcamera;
    drawdata._properties["defcammtx"_crcu].set<cameramatrices_ptr_t>(mono_cam);
    drawdata._properties["centercam"_crcu].set<cameramatrices_ptr_t>(VRDEV->_centercamera);
    mono_cam->_camdat.Persp(VRDEV->_near, VRDEV->_far, VRDEV->_fov*RTOD);
    _stereomatrices->_left  = VRDEV->_leftcamera;
    _stereomatrices->_right = VRDEV->_rightcamera;
    _stereomatrices->_mono  = VRDEV->_centercamera;
    // publish VR camera diagnostics for the perf HUD (terrain-invisibility hunt). root =
    //  the world-root position folded into the device user matrix this frame — that matrix
    //  is a VIEW matrix (world->root), so invert it and take the translation ((0,0,0) still
    //  flags a camera-lookup miss -> identity user matrix). eye = center-eye WORLD position
    //  (the center camera now carries the world offset, so its inverse-view translation is
    //  the true world eye). Written every eye (idempotent; same per-frame state).
    {
      auto& ov     = VrHudOverlay::instance();
      ov._cam_root = VRDEV->_outputViewOffsetMatrix.inverse().translation();
      if (VRDEV->_centercamera)
        ov._cam_eye = VRDEV->_centercamera->GetIVMatrix().translation();
    }
    drawdata._properties["StereoMatrices"_crcu].set<const StereoCameraMatrices*>(_stereomatrices.get());
    drawdata._properties["eyeindex"_crcu].set<int>(is_left_eye ? 0 : 1);
    using smat_ptr_t = const StereoCameraMatrices*;
    RCFD->setUserProperty("StereoMatrices"_crcu, (smat_ptr_t) _stereomatrices.get());
    RCFD->setUserProperty("eyeindex"_crcu, int(is_left_eye ? 0 : 1));
    _CPD.defaultSetup(drawdata);

    _CPD._stereo_cam_matrices = _stereomatrices.get();

    //////////////////////////////////////////////////////

    CIMPL->pushCPD(_CPD);

    if (_vrnode->_onCameraChange) {
      _vrnode->_onCameraChange(drawdata);
    }
  }
  ///////////////////////////////////////
  void _endAssembleEye(CompositorDrawData& drawdata, bool is_left_eye) {
    EASY_BLOCK("onodevr-endass");
    auto CIMPL = drawdata._cimpl;
    CIMPL->popCPD();
  }
  ///////////////////////////////////////
  // Resolve the world/root transform for this frame from the same DrawQueue camera the
  //  node used before (device _camera_name -> DB LUT; or the "vrcam" user property). It is
  //  the spawncam VIEW matrix (world->root) — exactly the convention Device usermtx wants
  //  (cmv = usermtx*base*hmd). Fed to the device in onGpuUpdate BEFORE the pose update, so
  //  this frame's eye cameras carry the walker's placement. VIEW matrix is aspect-
  //  independent, so no viewport aspect is needed here. Identity (camera-lookup miss) puts
  //  the viewer at world origin — flagged loudly below and by the HUD root:(0,0,0).
  fmtx4 _computeRootMatrix(CompositorDrawData& drawdata) {
    auto RCFD  = drawdata.RCFD();
    auto DB    = RCFD->GetDB();
    auto VRDEV = orkidvr::device();
    fmtx4 rootmatrix;
    auto vrdev_camname = VRDEV->_camera_name;
    if (vrdev_camname != "") {
      cameradata_constptr_t camera;
      DB->_cameraDataLUT.atomicOp([&](const cameradatalut_ptr_t& unlocked) { camera = unlocked->find(vrdev_camname); });
      if (camera) {
        (*_tmpcameramatrices) = camera->computeMatrices(1.0f);
        rootmatrix            = _tmpcameramatrices->GetVMatrix();
      } else {
        // Camera-lookup miss = the world-root offset silently drops to IDENTITY — the viewer
        //  renders from world origin (underground on big terrains: invisible scene). This
        //  exact silent default burned a live VR debugging session — never let it be quiet.
        static int s_root_miss_warns = 0;
        if (s_root_miss_warns < 8) {
          s_root_miss_warns++;
          printf("[VROUT] WARN root camera<%s> NOT FOUND in DB camera LUT — device user matrix=IDENTITY (viewer at world origin!)\n",
                 vrdev_camname.c_str());
          fflush(stdout);
        }
      }
    } else {
      if (auto as_cam = RCFD->tryUserProperty<const CameraData*>("vrcam"_crc)) {
        auto vrcam = as_cam.value();
        rootmatrix.lookAt(vrcam->GetEye(), vrcam->GetTarget(), vrcam->GetUp());
      }
    }
    return rootmatrix;
  }
  ///////////////////////////////////////
  void _downsample(
      CompositorDrawData& drawdata, //
      RtBuffer* render_out,         //
      bool is_left_eye) {           //

    auto context   = drawdata.context();
    auto fbi       = context->FBI();
    auto gbi       = context->GBI();
    auto dwi       = context->DWI();
    auto framedata = drawdata.RCFD();
    auto tex       = render_out->texture();
    auto this_buf  = context->FBI()->GetThisBuffer();
    // resize ssaadownsamplebuffer
    auto downRTG = is_left_eye ? _ssaadownsamplebufferL : _ssaadownsamplebufferR;
    // printf("_per_eye_width<%d> _per_eye_height<%d>\n", _per_eye_width, _per_eye_height);
    if (downRTG->width() != _per_eye_width || downRTG->height() != _per_eye_height) {
      downRTG->Resize(_per_eye_width, _per_eye_height);
    }

    fbi->PushRtGroup(downRTG.get());

    auto& mtl = _blit2screenmtl;
    mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
    mtl._rasterstate->setDepthTest(EDepthTest::OFF);
    mtl._rasterstate->setCullTest(ECullTest::OFF);
    mtl._rasterstate->_force = true;

    int ssaa = _vrnode->supersample();
    OrkAssert(ssaa >= 0 and ssaa <= 3);

    auto tek = _fxtechnique_downsample[ssaa];
    if(tek==nullptr){
      printf("WTF: tek is null for ssaa<%d>\n", ssaa);
      OrkAssert(false);
    }
    context->debugPushGroup("ScreenCompositingNode::to_screen<%d>", ssaa);

    mtl.begin(tek, framedata);
    mtl.bindParamTexture(_fxpColorMap, tex);
    mtl.bindParamMatrix(_fxpMVP, fmtx4::Identity());
    // THE eye-side 8-bit encode: downRTG is RGBA8 (DualMonoVrOutputNode::_format)
    // and is the texture handed to the XR runtime, so this blit owns the dither.
    // The desktop-mirror blits below read these buffers AFTER quantization and
    // deliberately leave DitherAmt at 0.
    mtl.bindParamFloat(_fxpDitherAmt, 1.0f);
    ViewportRect extents(0, 0, _per_eye_width, _per_eye_height);
    fbi->pushViewport(extents);
    fbi->pushScissor(extents);
    dwi->fullscreenQuad();
    fbi->popViewport();
    fbi->popScissor();
    mtl.end(framedata);

    // head-locked debug HUD panel into THIS eye's down buffer, while it is still the
    //  bound RTG — so both the XR handoff and the desktop mirror (which read these
    //  buffers in composite()) inherit it. No-op when the overlay is disabled.
    _drawHudPanel(drawdata, is_left_eye);

    fbi->PopRtGroup();

    context->debugPopGroup();
  }
  ///////////////////////////////////////
  // Preserve this eye's scene depth before the sibling eye overwrites the shared forward
  //  RTG depth. ONLY when the active device owns HMD presentation (live XR) — a genuine
  //  no-op on desktop/preview. Captured into per-eye samplable depth textures the composite
  //  hands to the runtime's depth layer (vrCompositeDepthToXrImage). Must run BEFORE the
  //  postfx loop reassigns render_outg (we want the forward render's depth, not a postfx RTG).
  void _captureEyeDepth(CompositorDrawData& drawdata, rtgroup_ptr_t render_outg, bool is_left_eye) {
    auto vrdev = orkidvr::device();
    if (not (vrdev and vrdev->_active and vrdev->ownsHmdPresentation()))
      return;
    if (not render_outg)
      return;
    auto depthtex = render_outg->depthTexture();
    if (not depthtex)
      return;
    int ssaa = _vrnode->supersample();
    if (ssaa != 0) {
      static bool s_ssaa_depth_warned = false;
      if (not s_ssaa_depth_warned) {
        s_ssaa_depth_warned = true;
        printf("[VROUT] DualMonoVr depth capture at ssaa=%d — captured depth is %dx render-resolution; "
               "the XR depth layer samples it with normalized UVs (reduced depth precision).\n",
               ssaa, ssaa + 1);
        fflush(stdout);
      }
    }
    auto& dst = is_left_eye ? _depthCaptureTexL : _depthCaptureTexR;
    ::ork::lev2::vulkan::vrCaptureEyeDepth(drawdata.context(), depthtex.get(), dst);
  }
  ///////////////////////////////////////
  std::shared_ptr<StereoCameraMatrices> _stereomatrices;
  std::shared_ptr<CameraMatrices> _tmpcameramatrices;
  // per-eye preserved scene depth (device-local, samplable) for the XR depth layer.
  texture_ptr_t _depthCaptureTexL;
  texture_ptr_t _depthCaptureTexR;

  DualMonoVrOutputNode* _vrnode                               = nullptr;
  const orkidvr::StandardVrPresentation* _installedPresentation = nullptr;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
  bool _doinit  = true;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique_downsample[4];
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
  const FxShaderParam* _fxpDitherAmt;
  // head-locked debug HUD panel (VrHudOverlay -> slate + premult text, per eye)
  FreestyleMaterial _hudpanelmtl;
  const FxShaderTechnique* _hudpanel_tek_slate = nullptr;
  const FxShaderTechnique* _hudpanel_tek_text  = nullptr;
  const FxShaderParam*     _hudpanel_par_mvp   = nullptr;
  const FxShaderParam*     _hudpanel_par_map   = nullptr;
  const FxShaderParam*     _hudpanel_par_modc  = nullptr;
  std::shared_ptr<DynamicVertexBuffer<SVtxV16T16C16>> _hudpanel_vbuf;
  int _multiplier     = 1;
  int _ssaa_width     = 0;
  int _ssaa_height    = 0;
  int _per_eye_width  = 0;
  int _per_eye_height = 0;
  rtgroup_ptr_t _ssaadownsamplebufferL;
  rtgroup_ptr_t _ssaadownsamplebufferR;
};
using DMVRIMPL_ptr_t = std::shared_ptr<DMVRIMPL>;
///////////////////////////////////////////////////////////////////////////////
DualMonoVrOutputNode::DualMonoVrOutputNode() {
  _impl = std::make_shared<DMVRIMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
DualMonoVrOutputNode::~DualMonoVrOutputNode() {
  closeExternalViewer();
}
///////////////////////////////////////////////////////////////////////////////
ezsecondarywin_ptr_t DualMonoVrOutputNode::createExternalViewer(
    orkezapp_ptr_t app, const EzSecondaryWinConfig& cfg, bool mono) {

  closeExternalViewer();

  auto impl = _impl.get<DMVRIMPL_ptr_t>();
  auto win = app->createSecondaryWindow(cfg);

  struct ExtViewerState {
    FreestyleMaterial _mtl;
    const FxShaderTechnique* _tek = nullptr;
    const FxShaderParam* _fxpMVP = nullptr;
    const FxShaderParam* _fxpColorMap = nullptr;
    bool _initialized = false;
    bool _mono = true;
  };
  auto ext = std::make_shared<ExtViewerState>();
  ext->_mono = mono;

  win->_onGpuInit = [ext](Context* ctx) {
    ext->_mtl.gpuInit(ctx, "orkshader://blit");
    ext->_mtl._rasterstate->setCullTest(ECullTest::OFF);
    ext->_mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
    ext->_mtl._rasterstate->setDepthTest(EDepthTest::OFF);
    ext->_tek = ext->_mtl.technique("blituv");
    ext->_fxpMVP = ext->_mtl.param("MatMVP");
    ext->_fxpColorMap = ext->_mtl.param("ColorMap");
    ext->_initialized = true;
  };

  win->_onDraw = [impl, ext](ui::drawevent_constptr_t drwev) {
    if (!ext->_initialized) return;

    auto ctx = drwev->GetTarget();
    auto fbi = ctx->FBI();
    auto dwi = ctx->DWI();

    auto texL = impl->_ssaadownsamplebufferL
                  ? impl->_ssaadownsamplebufferL->texture(0).get()
                  : nullptr;
    if (!texL) return;

    int w = ctx->mainSurfaceWidth();
    int h = ctx->mainSurfaceHeight();
    auto framedata = drwev->_acqdbuf->_RCFD;
    auto this_buf = fbi->GetThisBuffer();

    ctx->beginFrame();

    ViewportRect vprect(0, 0, w, h);
    fbi->pushViewport(vprect);
    fbi->pushScissor(vprect);

    auto& mtl = ext->_mtl;
    mtl.begin(ext->_tek, framedata);
    mtl.bindParamMatrix(ext->_fxpMVP, fmtx4::Identity());

    if (ext->_mono) {
      // Single eye -> full window, zoom to fit (crop excess)
      auto uvrect = fvec4(0, 1, 1, -1);
      if (impl->_per_eye_width > 0 && impl->_per_eye_height > 0 && w > 0 && h > 0) {
        float tex_aspect = float(impl->_per_eye_width) / float(impl->_per_eye_height);
        float win_aspect = float(w) / float(h);
        float uv_x = 0.0f, uv_y = 0.0f, uv_w = 1.0f, uv_h = 1.0f;
        if (tex_aspect > win_aspect) {
          uv_w = win_aspect / tex_aspect;
          uv_x = (1.0f - uv_w) * 0.5f;
        } else {
          uv_h = tex_aspect / win_aspect;
          uv_y = (1.0f - uv_h) * 0.5f;
        }
        // Vulkan Y-flip: start at uv_y+uv_h, walk down by -uv_h.
        uvrect = fvec4(uv_x, uv_y + uv_h, uv_w, -uv_h);
      }
      mtl.bindParamTexture(ext->_fxpColorMap, texL);
      this_buf->Render2dQuadEML(
          fvec4(-1, -1, 2, 2),
          uvrect,
          uvrect);
    } else {
      auto texR = impl->_ssaadownsamplebufferR
                    ? impl->_ssaadownsamplebufferR->texture(0).get()
                    : nullptr;
      if (!texR) { mtl.end(framedata); fbi->popScissor(); fbi->popViewport(); ctx->endFrame(); return; }

      // Vulkan Y-flip applied to UV rects.
      const fvec4 flipped_uv(0, 1, 1, -1);

      // Left eye -> left half
      mtl.bindParamTexture(ext->_fxpColorMap, texL);
      this_buf->Render2dQuadEML(
          fvec4(-1, -1, 1, 2),
          flipped_uv,
          flipped_uv);

      // Right eye -> right half
      mtl.bindParamTexture(ext->_fxpColorMap, texR);
      this_buf->Render2dQuadEML(
          fvec4(0, -1, 1, 2),
          flipped_uv,
          flipped_uv);
    }

    mtl.end(framedata);

    fbi->popScissor();
    fbi->popViewport();

    ctx->endFrame();
  };

  _externalViewer = win;
  return win;
}
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::closeExternalViewer() {
  if (_externalViewer) {
    _externalViewer->requestClose();
    _externalViewer.reset();
  }
}
///////////////////////////////////////////////////////////////////////////////
// Read accessor for the per-eye final downsampled RtGroup (the same buffers the
//  desktop mirror blit and the XR runtime handoff read in composite()). Enables
//  headless per-eye capture from the pybound onEndAssemble hook without any render-
//  path change. Null before gpuInit / first assemble.
lev2::rtgroup_ptr_t DualMonoVrOutputNode::downsampledEyeRtGroup(bool left_eye) {
  auto impl = _impl.get<DMVRIMPL_ptr_t>();
  return left_eye ? impl->_ssaadownsamplebufferL : impl->_ssaadownsamplebufferR;
}
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<DMVRIMPL_ptr_t>()->gpuInit(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::onGpuUpdate(CompositorDrawData& drawdata) {
  // exactly-once-per-frame device update. The XR runtime pacing contract is one
  //  xrWaitFrame/xrBeginFrame per frame (frameWaitCount=1); driving it once per eye
  //  double-paces the runtime AND desyncs the L/R predicted poses (each eye would
  //  locate views at a different predictedDisplayTime). Both eye passes below read the
  //  single per-frame state (_leftcamera/_rightcamera/_centercamera) this update fills.
  //
  // Feed the world/root transform into the device user matrix FIRST: gpuUpdate ->
  //  _updatePosesCommon composes cmv = usermtx*base*hmd this same frame, so the eye
  //  cameras carry the walker's world placement and every draw path (standard + SSBO-
  //  custom mono) works in true-WORLD space with no per-draw root compose.
  auto impl        = _impl.get<DMVRIMPL_ptr_t>();
  fmtx4 rootmatrix = impl->_computeRootMatrix(drawdata);
  orkidvr::device()->_usermtxgen = [rootmatrix]() -> fmtx4 { return rootmatrix; };
  orkidvr::device()->gpuUpdate(*drawdata.RCFD());
}
///////////////////////////////////////////////////////////////////////////////
compdrawdata_fn_t DualMonoVrOutputNode::createAssembler(nodecompositortechnique_ptr_t tek) {
  return [this, tek](CompositorDrawData& drawdata) {
    auto rnode = tek->_renderNode;
    auto context = drawdata.context();
    // printf("rendering dual-mono-vr!\n");
    ////////////////////////////////////////////////////////////////////////////
    // this assembler will run the render and postfx nodes twice,
    //  once for each eye
    ////////////////////////////////////////////////////////////////////////////
    // for now, so we dont have to change the render and postfx nodes
    //  we will stash each eye in a holding buffer for final compositing
    ////////////////////////////////////////////////////////////////////////////
    auto do_for_eye = [&](uint64_t eye) {
      auto impl    = _impl.get<DMVRIMPL_ptr_t>();
      auto context = drawdata.context();
      auto framedata = drawdata.RCFD();
      ////////////////////////////////////////////////////////////////////////////
      bool is_left_eye  = (eye == "left"_crcu);
      bool is_right_eye = (not is_left_eye);
      framedata->setUserProperty("eye_index"_crcu, int(is_left_eye ? 0 : 1));
      ////////////////////////////////////////////////////////////////////////////
      rtgroup_ptr_t render_outg = rnode ? rnode->GetOutputGroup() : nullptr;
      RtBuffer* render_out      = rnode ? rnode->GetOutput().get() : nullptr;
      drawdata._properties["render_out"_crcu].set<RtBuffer*>(render_out);
      drawdata._properties["render_outgroup"_crcu].set<rtgroup_ptr_t>(render_outg);
      ////////////////////////////////////////////////////////////////////////////
      // assemble (render) the eye
      ////////////////////////////////////////////////////////////////////////////
      context->debugPushGroup("DualMonoVrOutputNode::beginAssembleEye");
      impl->_beginAssembleEye(drawdata, is_left_eye);
      context->debugPopGroup();
      ////////////////////////////////////////////////////////////////////////////
      rnode->Render(drawdata);
      ////////////////////////////////////////////////////////////////////////////
      context->debugPushGroup("DualMonoVrOutputNode::endAssembleEye");
      impl->_endAssembleEye(drawdata, is_left_eye);
      context->debugPopGroup();
      ////////////////////////////////////////////////////////////////////////////
      // preserve this eye's forward-render depth (live-XR only) before the sibling eye
      //  overwrites the shared RTG depth and before postfx reassigns render_outg.
      ////////////////////////////////////////////////////////////////////////////
      impl->_captureEyeDepth(drawdata, render_outg, is_left_eye);
      ////////////////////////////////////////////////////////////////////////////
      // postfx nodes for the eye
      ////////////////////////////////////////////////////////////////////////////
      size_t num_fx_nodes = tek->_postEffectNodes.size();
      for (auto pfxnode : tek->_postEffectNodes) {
        if (pfxnode->_disabled) {
          continue;
        }
        drawdata._properties["postfx_in"_crcu].set<rtgroup_ptr_t>(render_outg);
        pfxnode->Render(drawdata);
        render_outg = pfxnode->GetOutputGroup();
        render_out  = pfxnode->GetOutput().get();
      }
      drawdata._properties["final_out"_crcu].set<RtBuffer*>(render_out);
      drawdata._properties["final_outgroup"_crcu].set<rtgroup_ptr_t>(render_outg);
      ////////////////////////////////////////////////////////////////////////////
      // downsample ?
      ////////////////////////////////////////////////////////////////////////////
      if (render_out) {
        impl->_downsample(drawdata, render_out, is_left_eye);
      }
      ////////////////////////////////////////////////////////////////////////////
    };

    context->debugPushGroup("DualMonoVrOutputNode::assemble");

    if (_onBeginAssemble) {
      _onBeginAssemble(drawdata);
    }

    context->debugPushGroup("DualMonoVrOutputNode::assembleL");
    do_for_eye("left"_crcu);
    context->debugPopGroup();

    context->debugPushGroup("DualMonoVrOutputNode::assembleR");
    do_for_eye("right"_crcu);
    context->debugPopGroup();

    if (_onEndAssemble) {
      _onEndAssemble(drawdata);
    }

    context->debugPopGroup();
  };
}
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("DualMonoVrOutputNode::composite");
  auto impl        = _impl.get<DMVRIMPL_ptr_t>();
  Context* context = drawdata.context();
  auto vrdev       = orkidvr::device();
  // The XR runtime owns HMD presentation only for the OpenXR device while active. In
  //  that mode this node hands the two final per-eye textures to the runtime (which
  //  does its own lens distortion), so the client-side distortion lambda is NOT
  //  adopted below. Other VR devices / desktop-preview present through the mirror blit.
  const bool runtime_owns = vrdev and vrdev->_active and vrdev->ownsHmdPresentation();
  // one-shot: states this run's mode + whether the per-eye handoff (__compositeStereo)
  //  will be called (runtime owns presentation) or this is a desktop/preview present.
  static bool s_dmvrout_first = false;
  if (not s_dmvrout_first) {
    s_dmvrout_first = true;
    printf("[VROUT] DualMonoVrOutputNode::composite first entry (vrdev=%d active=%d ownsHmdPresentation=%d) — per-eye __compositeStereo %s.\n",
           int(vrdev != nullptr),
           int(vrdev ? vrdev->_active : false),
           int(runtime_owns),
           runtime_owns ? "WILL be called (runtime owns presentation)" : "will NOT be called (desktop/preview)");
    fflush(stdout);
  }
  /////////////////////////////////////////////////////////////////////////////
  // adopt the device's standard VR presentation (host-configured, C++-executed).
  //  install / refresh the per-eye distortion pass when it changes; a null
  //  presentation falls back to the fixed blit below. SKIPPED when the XR runtime
  //  owns presentation — it applies its own lens distortion, so adopting the client-
  //  side distortion lambda here would double-distort.
  /////////////////////////////////////////////////////////////////////////////
  if (not runtime_owns) {
    auto pres     = orkidvr::device()->_presentation;
    auto pres_raw = pres.get();
    if (pres_raw != impl->_installedPresentation) {
      _distortion_lambda           = pres_raw ? pres_raw->genLambda() : distortion_lambda_t();
      impl->_installedPresentation = pres_raw;
    }
  }
  /////////////////////////////////////////////////////////////////////////////
  // DMVR compositor
  /////////////////////////////////////////////////////////////////////////////
  auto fbi         = context->FBI();
  auto gbi         = context->GBI();
  auto dwi         = context->DWI();

  if (auto try_final = drawdata._properties["final_out"_crcu].tryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      assert(buffer != nullptr);
      auto tex = buffer->texture();
      if (tex) {
        auto framedata = drawdata.RCFD();

        /////////////////////////////////////////////////////////////////////////////
        // XR runtime handoff (runtime-owned presentation): hand the two FINAL per-eye
        //  textures (post-postfx, post-downsample — the exact textures the desktop
        //  mirror blit below reads) to the device, which blits each into its half of
        //  the runtime's wide swapchain image and submits the frame. No-op on devices
        //  that do not own HMD presentation (this whole block is gated off then).
        /////////////////////////////////////////////////////////////////////////////
        if (runtime_owns) {
          auto texL = impl->_ssaadownsamplebufferL ? impl->_ssaadownsamplebufferL->texture(0).get() : nullptr;
          auto texR = impl->_ssaadownsamplebufferR ? impl->_ssaadownsamplebufferR->texture(0).get() : nullptr;
          // per-eye preserved depth (captured during assemble) -> runtime depth layer; null
          //  when the depth capture did not run/succeed, in which case the device chains
          //  color-only. Same flipV decision as the color blit is applied inside the device.
          auto depthL = impl->_depthCaptureTexL.get();
          auto depthR = impl->_depthCaptureTexR.get();
          context->debugPushGroup("DualMonoVrOutputNode::compositeStereo");
          vrdev->__compositeStereo(context, texL, texR, depthL, depthR);
          context->debugPopGroup();
        }

        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.context()->debugPushGroup("DualMonoVrOutputNode::to_screen");

        auto& mtl             = impl->_blit2screenmtl;
        auto inp_rtg          = drawdata._properties["final_outgroup"_crcu].get<rtgroup_ptr_t>();
        auto this_buf         = context->FBI()->GetThisBuffer();
        auto tek_nodownsample = impl->_fxtechnique_downsample[0];

        /////////////////////////////////////////////////////
        if (_distortion_lambda) { // Lens distortion ?
        /////////////////////////////////////////////////////
          drawdata.context()->debugPushGroup("DualMonoVrOutputNode::distortion_lambda");
          int out_surface_width  = context->mainSurfaceWidth();
          int out_surface_height = context->mainSurfaceHeight();
          int wd2 = out_surface_width>>1;
          int h = out_surface_height;
          DistortionRect drectL = {
              impl->_ssaadownsamplebufferL->texture(0).get(),
              SRect(wd2, 0, wd2*2, h),
              'L'
          };
          DistortionRect drectR = {
              impl->_ssaadownsamplebufferR->texture(0).get(),
              SRect(0, 0, wd2, h),
              'R'
          };
          //printf("out_surface_width<%d> out_surface_height<%d>\n", out_surface_width, out_surface_height);
          _distortion_lambda(framedata, drectL);
          _distortion_lambda(framedata, drectR);
          drawdata.context()->debugPopGroup();
        /////////////////////////////////////////////////////
        } else { // no lens distortion
        /////////////////////////////////////////////////////
          drawdata.context()->debugPushGroup("DualMonoVrOutputNode::to_hmd");

          mtl.begin(tek_nodownsample, framedata);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());

          int out_surface_width  = context->mainSurfaceWidth();
          int out_surface_height = context->mainSurfaceHeight();
          // printf("out_surface_width<%d> out_surface_height<%d>\n", out_surface_width, out_surface_height);
          ViewportRect extents(0, 0, out_surface_width, out_surface_height);
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);

          ////////////
          // Downsampled Left Eye -> Output
          ////////////

          // PRESENTATION ORIGIN: the eye buffers are stored TOP-DOWN (the XR runtime's
          //  convention — _downsample writes them through DWI, and the runtime handoff
          //  consumes them unflipped), while this blit targets the presented surface, whose
          //  origin is the context's. DWI::quad2D is the one path that applies the
          //  logical->native V correction (Render2dQuadEML's correction fires only for
          //  logical-Y-down/native-Y-up, i.e. never under Vulkan), so the mirror must go
          //  through it or it presents inverted on every Y-down backend. _flipY stays
          //  LOGICAL: true = no extra flip, the platform decides the rest.
          const fvec4 uvrect = _flipY ? fvec4(0, 0, 1, 1) : fvec4(0, 1, 1, -1);

          auto tex = impl->_ssaadownsamplebufferL->texture(0).get();
          mtl.bindParamTexture(impl->_fxpColorMap, tex);
          dwi->quad2D(
              fvec4(-1, -1, 1, 2), // xywh
              uvrect,              // uvrectA(u,v,w,h)
              uvrect);             // uvrectB(u,v,w,h)

          ////////////
          // Downsampled Right Eye -> Output
          ////////////

          tex = impl->_ssaadownsamplebufferR->texture(0).get();
          mtl.bindParamTexture(impl->_fxpColorMap, tex);
          dwi->quad2D(
              fvec4(0, -1, 1, 2), // xywh
              uvrect,             // uvrectA(u,v,w,h)
              uvrect);            // uvrectB(u,v,w,h)

          ////////////
          // done
          ////////////

          fbi->popViewport();
          fbi->popScissor();

          mtl.end(framedata);

          drawdata.context()->debugPopGroup();
        } // no distortion

        drawdata.context()->debugPopGroup();
      }
    }
  }
  // mark external viewer dirty so it redraws with fresh eye textures
  if (_externalViewer) {
    _externalViewer->markDirty();
  }
  drawdata.context()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
