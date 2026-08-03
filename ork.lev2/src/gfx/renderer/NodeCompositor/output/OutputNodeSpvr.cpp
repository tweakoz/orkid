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
#include <ork/kernel/profiler.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::SinglePassStereoVrOutputNode, "SinglePassStereoVrOutputNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
// Per-EYE CPU stage bracket for the post-scene half (extract/postfx/downsample).
//  Same resolve-once-per-eye shape DualMonoVr uses: OrkProfilerSampleScope stamps
//  ONE static series per source site, and this node's eye body is one lambda run
//  twice, so the macro alone cannot separate L from R.
///////////////////////////////////////////////////////////////////////////////
#if defined(ORK_PROFILER_ENABLE)
#define SpvrEyeStageScope(_stage)                                                                   \
  static thread_local SampleProfilerSeries* OrkUnique(_eyeser)[2] = {nullptr, nullptr};              \
  auto*& OrkUnique(_eyeslot)                                      = OrkUnique(_eyeser)[is_left_eye ? 0 : 1]; \
  if (nullptr == OrkUnique(_eyeslot)) [[unlikely]]                                                  \
    OrkUnique(_eyeslot) = Profiler::acquireSeries<SampleProfilerSeries>(                            \
        std::string(CHANNEL_MAIN), std::string(is_left_eye ? "cpu:spvr:L:" _stage : "cpu:spvr:R:" _stage)); \
  auto OrkUnique(_eyescope) = OrkUnique(_eyeslot)->sampleScope()
#else
#define SpvrEyeStageScope(_stage)
#endif
///////////////////////////////////////////////////////////////////////////////
void SinglePassStereoVrOutputNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
struct SPVRIMPL {
  ///////////////////////////////////////
  SPVRIMPL(SinglePassStereoVrOutputNode* node)
      : _vrnode(node) {
    _tmpcameramatrices = std::make_shared<CameraMatrices>();
    _stereomatrices    = std::make_shared<StereoCameraMatrices>();
  }
  ///////////////////////////////////////
  ~SPVRIMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* context) {
    if (not _doinit)
      return;

    _blit2screenmtl.gpuInit(context, "orkshader://blit");
    _blit2screenmtl._rasterstate->setCullTest(ECullTest::OFF);
    _fxtechnique_downsample[0] = _blit2screenmtl.technique("blituv");
    _fxtechnique_downsample[1] = _blit2screenmtl.technique("downsample_2x2");
    _fxtechnique_downsample[2] = _blit2screenmtl.technique("downsample_3x3");
    _fxtechnique_downsample[3] = _blit2screenmtl.technique("downsample_4x4");
    _fxpMVP                    = _blit2screenmtl.param("MatMVP");
    _fxpColorMap               = _blit2screenmtl.param("ColorMap");
    _fxpDitherAmt              = _blit2screenmtl.param("DitherAmt");

    // the layered->per-eye seam (see spvr_extract.fxv2)
    _extractmtl.gpuInit(context, "orkshader://spvr_extract");
    _extractmtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
    _extractmtl._rasterstate->setDepthTest(EDepthTest::OFF);
    _extractmtl._rasterstate->setCullTest(ECullTest::OFF);
    _extract_tek   = _extractmtl.technique("spvr_extract");
    OrkAssert(_extract_tek);
    _extract_par_mvp   = _extractmtl.param("MatMVP");
    _extract_par_slice = _extractmtl.param("SliceIndex");
    _extract_par_map   = _extractmtl.param("LayeredMap");

    _ssaadownsamplebufferL        = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
    _ssaadownsamplebufferR        = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
    _ssaadownsamplebufferL->_name = "spvr.downsampleL";
    _ssaadownsamplebufferR->_name = "spvr.downsampleR";
    auto dsbufL                   = _ssaadownsamplebufferL->createRenderTarget(_vrnode->_format);
    dsbufL->_debugName            = "MsaaDownsampleBufferL";
    auto dsbufR                   = _ssaadownsamplebufferR->createRenderTarget(_vrnode->_format);
    dsbufR->_debugName            = "MsaaDownsampleBufferR";

    // head-locked debug HUD panel — identical two-layer (slate + premultiplied text)
    //  construction as DualMonoVr, for the same FontMan glyph-blend reason documented
    //  there. Kept a peer implementation rather than shared: the two nodes are meant to
    //  be diffable against each other, not coupled.
    _hudpanelmtl.gpuInit(context, "orkshader://ui");
    _hudpanel_tek_slate = _hudpanelmtl.technique("uidev_modcolor_alpha");
    _hudpanel_tek_text  = _hudpanelmtl.technique("uitextured_prema");
    _hudpanel_par_mvp   = _hudpanelmtl.param("mvp");
    _hudpanel_par_map   = _hudpanelmtl.param("ColorMap");
    _hudpanel_par_modc  = _hudpanelmtl.param("ModColor");
    _hudpanel_vbuf      = std::make_shared<DynamicVertexBuffer<SVtxV16T16C16>>(64, 0);
    _hudpanel_vbuf->SetRingLock(true);

    _doinit = false;
  }
  ///////////////////////////////////////
  // Head-locked debug HUD panel into THIS eye's just-downsampled buffer. Same
  //  placement math and the same slate+premultiplied-text layering as DualMonoVr —
  //  the panel is drawn per eye with that eye's V*P, so it carries natural stereo
  //  disparity at the published depth. No-op when the overlay is disabled.
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

    const float dist   = overlay._distance_m;
    const float halfW  = overlay._width_m * 0.5f;
    const float aspect = (overlay._aspect > 0.01f) ? overlay._aspect : 1.0f; // w/h
    const float halfH  = halfW / aspect;

    fmtx4 worldModel = _stereomatrices->_mono->GetIVMatrix();
    fmtx4 mvp        = is_left_eye ? _stereomatrices->MVPL(worldModel) //
                                   : _stereomatrices->MVPR(worldModel);

    // vertical placement is MEASURED through this eye's own mvp (two-point NDC span at
    //  the panel depth) so no projection convention is assumed — see the DualMonoVr peer.
    float yoff = 0.0f;
    if (overlay._yoffset_frac != 0.0f) {
      const fvec4 a         = fvec4(0, 0, -dist, 1).transform(mvp);
      const fvec4 b         = fvec4(0, 1, -dist, 1).transform(mvp);
      const float ndc_per_m = fabsf(b.y / b.w - a.y / a.w);
      if (ndc_per_m > 1e-6f)
        yoff = -overlay._yoffset_frac * (2.0f / ndc_per_m); // full NDC height = 2
    }

    fvec3 TL(-halfW, halfH + yoff, -dist), TR(halfW, halfH + yoff, -dist);
    fvec3 BL(-halfW, -halfH + yoff, -dist), BR(halfW, -halfH + yoff, -dist);

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
    // blend/depth/cull come from each technique's state_block (a runtime setBlendingMacro
    //  on a FreestyleMaterial does NOT reach the baked pipeline blend).
    mtl.begin(_hudpanel_tek_slate, framedata);
    mtl.bindParamMatrix(_hudpanel_par_mvp, mvp);
    mtl.bindParamVec4(_hudpanel_par_modc, fvec4(0, 0, 0, 0.5f));
    gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
    mtl.end(framedata);
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
  // ONE CPD carrying BOTH eyes. The mono slot is the CENTER camera, deliberately:
  //  every mono consumer under this pass (the impostor sub-render, HZB/frustum culling,
  //  the RCFD_Camera_*_Mono providers, the particle billboard basis) is a single-view
  //  approximation that must not be an eye — a per-view producer reads ublk_stereo
  //  instead, indexed by the multiview view selector.
  void _beginAssemble(CompositorDrawData& drawdata) {
    EASY_BLOCK("onodespvr-begass");
    auto RCFD  = drawdata.RCFD();
    auto CIMPL = drawdata._cimpl;

    auto VRDEV = orkidvr::device();
    int ssaa   = _vrnode->supersample();
    OrkAssert(ssaa >= 0 and ssaa <= 3);
    _multiplier     = ssaa + 1;
    _per_eye_width  = VRDEV->_width;
    _per_eye_height = VRDEV->_height;
    _ssaa_width     = _per_eye_width * _multiplier;
    _ssaa_height    = _per_eye_height * _multiplier;

    _viewOffsetMatrix = VRDEV->_outputViewOffsetMatrix;

    drawdata._properties["OutputWidth"_crcu].set<int>(_ssaa_width);
    drawdata._properties["OutputHeight"_crcu].set<int>(_ssaa_height);

    drawdata._properties["SinglePassStereo"_crcu].set<bool>(true);
    // the center camera carries the same near/far/fov the per-eye cameras do — the
    //  stereo CPD reads near/far off THIS camera's camdat (CompositingPassData::
    //  nearAndFar), so leaving it unset would hand the whole pass a 0..1 depth range.
    VRDEV->_centercamera->_camdat.Persp(VRDEV->_near, VRDEV->_far, VRDEV->_fov * RTOD);
    drawdata._properties["defcammtx"_crcu].set<cameramatrices_ptr_t>(VRDEV->_centercamera);
    drawdata._properties["centercam"_crcu].set<cameramatrices_ptr_t>(VRDEV->_centercamera);
    _stereomatrices->_left  = VRDEV->_leftcamera;
    _stereomatrices->_right = VRDEV->_rightcamera;
    _stereomatrices->_mono  = VRDEV->_centercamera;
    {
      auto& ov     = VrHudOverlay::instance();
      ov._cam_root = VRDEV->_outputViewOffsetMatrix.inverse().translation();
      if (VRDEV->_centercamera)
        ov._cam_eye = VRDEV->_centercamera->GetIVMatrix().translation();
    }
    drawdata._properties["StereoMatrices"_crcu].set<const StereoCameraMatrices*>(_stereomatrices.get());
    // there is no "current eye" in a single-pass frame. 0 = view 0, which is what any
    //  surviving RCFD_EYE_INDEX consumer resolves to under the layered pass; the real
    //  per-view selector is ofx_viewIndex inside the shader.
    drawdata._properties["eyeindex"_crcu].set<int>(0);
    using smat_ptr_t = const StereoCameraMatrices*;
    RCFD->setUserProperty("StereoMatrices"_crcu, (smat_ptr_t)_stereomatrices.get());
    RCFD->setUserProperty("eyeindex"_crcu, int(0));
    _CPD.defaultSetup(drawdata);

    _CPD._stereo_cam_matrices = _stereomatrices.get();

    CIMPL->pushCPD(_CPD);

    if (_vrnode->_onCameraChange) {
      _vrnode->_onCameraChange(drawdata);
    }
  }
  ///////////////////////////////////////
  void _endAssemble(CompositorDrawData& drawdata) {
    EASY_BLOCK("onodespvr-endass");
    auto CIMPL = drawdata._cimpl;
    CIMPL->popCPD();
  }
  ///////////////////////////////////////
  // R6 SEAM: copy ONE layer of the layered scene color into this eye's own 2D RtGroup,
  //  at 1:1 and unfiltered. Everything after this point is the DualMonoVr per-eye chain
  //  verbatim (postfx nodes, the downsample/dither blit, the HUD panel), so those
  //  consumers keep their exact shape and the OpenXR handoff never sees a layered target.
  rtgroup_ptr_t _extractEye(CompositorDrawData& drawdata, rtgroup_ptr_t layered, bool is_left_eye) {
    auto context = drawdata.context();
    auto fbi     = context->FBI();
    auto dwi     = context->DWI();
    auto rcfd    = drawdata.RCFD();

    auto src_buf = layered->buffer(0);
    OrkAssert(src_buf);

    auto& dst = is_left_eye ? _eyeExtractL : _eyeExtractR;
    if (nullptr == dst) {
      dst        = std::make_shared<RtGroup>(context, layered->width(), layered->height(), MsaaSamples::MSAA_1X);
      dst->_name = is_left_eye ? "spvr.eyeExtractL" : "spvr.eyeExtractR";
      // no depth: nothing downstream of the scene pass reads it (DualMonoVr's postfx
      //  chain and downsample are color-only), and a depth attachment here would only
      //  be an unwritten one.
      dst->_needsDepth = false;
      auto buf         = dst->createRenderTarget(src_buf->format());
      buf->_debugName  = is_left_eye ? "SpvrEyeExtractL" : "SpvrEyeExtractR";
    }
    if (dst->width() != layered->width() or dst->height() != layered->height())
      dst->Resize(layered->width(), layered->height());

    dst->_autoclear      = false;
    dst->_clearMaskColor = false;
    dst->_clearMaskDepth = false;

    context->debugPushGroup("SinglePassStereoVrOutputNode::extractEye");
    fbi->PushRtGroup(dst.get());
    ViewportRect extents(0, 0, dst->width(), dst->height());
    fbi->pushViewport(extents);
    fbi->pushScissor(extents);
    auto& mtl = _extractmtl;
    mtl.begin(_extract_tek, rcfd);
    mtl.bindParamMatrix(_extract_par_mvp, fmtx4::Identity());
    mtl.bindParamInt(_extract_par_slice, is_left_eye ? 0 : 1);
    mtl.bindParamTexture(_extract_par_map, src_buf->texture());
    dwi->fullscreenQuad();
    mtl.end(rcfd);
    fbi->popScissor();
    fbi->popViewport();
    fbi->PopRtGroup();
    context->debugPopGroup();

    return dst;
  }
  ///////////////////////////////////////
  // The per-eye downsample/dither blit — byte-for-byte the DualMonoVr operation, run
  //  against this eye's extracted 2D source.
  void _downsample(
      CompositorDrawData& drawdata, //
      RtBuffer* render_out,         //
      bool is_left_eye) {           //

    auto context   = drawdata.context();
    auto fbi       = context->FBI();
    auto dwi       = context->DWI();
    auto framedata = drawdata.RCFD();
    auto tex       = render_out->texture();

    auto downRTG = is_left_eye ? _ssaadownsamplebufferL : _ssaadownsamplebufferR;
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
    OrkAssertI(tek != nullptr, "SPVR: no downsample technique for this ssaa level");
    context->debugPushGroup("SinglePassStereoVrOutputNode::to_screen<%d>", ssaa);

    mtl.begin(tek, framedata);
    mtl.bindParamTexture(_fxpColorMap, tex);
    mtl.bindParamMatrix(_fxpMVP, fmtx4::Identity());
    // THE eye-side 8-bit encode: downRTG is RGBA8 and is the texture handed to the XR
    // runtime, so this blit owns the dither (the mirror blits read it after quantization).
    mtl.bindParamFloat(_fxpDitherAmt, 1.0f);
    ViewportRect extents(0, 0, _per_eye_width, _per_eye_height);
    fbi->pushViewport(extents);
    fbi->pushScissor(extents);
    dwi->fullscreenQuad();
    fbi->popViewport();
    fbi->popScissor();
    mtl.end(framedata);

    _drawHudPanel(drawdata, is_left_eye);

    fbi->PopRtGroup();

    context->debugPopGroup();
  }
  ///////////////////////////////////////
  // Same world/root resolve DualMonoVr does: the spawncam VIEW matrix (world->root) fed
  //  to the device BEFORE the pose update, so this frame's eye cameras already carry the
  //  walker's placement and no draw path composes a root transform per draw.
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
        // a camera-lookup miss silently drops the world-root offset to IDENTITY (viewer at
        //  world origin, underground on big terrains) — never let it be quiet.
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
  std::shared_ptr<StereoCameraMatrices> _stereomatrices;
  std::shared_ptr<CameraMatrices> _tmpcameramatrices;

  SinglePassStereoVrOutputNode* _vrnode                         = nullptr;
  const orkidvr::StandardVrPresentation* _installedPresentation = nullptr;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
  bool _doinit = true;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique_downsample[4];
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
  const FxShaderParam* _fxpDitherAmt;
  // the layered->per-eye extract
  FreestyleMaterial _extractmtl;
  const FxShaderTechnique* _extract_tek     = nullptr;
  const FxShaderParam* _extract_par_mvp     = nullptr;
  const FxShaderParam* _extract_par_slice   = nullptr;
  const FxShaderParam* _extract_par_map     = nullptr;
  rtgroup_ptr_t _eyeExtractL;
  rtgroup_ptr_t _eyeExtractR;
  // head-locked debug HUD panel
  FreestyleMaterial _hudpanelmtl;
  const FxShaderTechnique* _hudpanel_tek_slate = nullptr;
  const FxShaderTechnique* _hudpanel_tek_text  = nullptr;
  const FxShaderParam* _hudpanel_par_mvp       = nullptr;
  const FxShaderParam* _hudpanel_par_map       = nullptr;
  const FxShaderParam* _hudpanel_par_modc      = nullptr;
  std::shared_ptr<DynamicVertexBuffer<SVtxV16T16C16>> _hudpanel_vbuf;
  int _multiplier     = 1;
  int _ssaa_width     = 0;
  int _ssaa_height    = 0;
  int _per_eye_width  = 0;
  int _per_eye_height = 0;
  rtgroup_ptr_t _ssaadownsamplebufferL;
  rtgroup_ptr_t _ssaadownsamplebufferR;
};
using SPVRIMPL_ptr_t = std::shared_ptr<SPVRIMPL>;
///////////////////////////////////////////////////////////////////////////////
SinglePassStereoVrOutputNode::SinglePassStereoVrOutputNode() {
  _impl = std::make_shared<SPVRIMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
SinglePassStereoVrOutputNode::~SinglePassStereoVrOutputNode() {
}
///////////////////////////////////////////////////////////////////////////////
lev2::rtgroup_ptr_t SinglePassStereoVrOutputNode::downsampledEyeRtGroup(bool left_eye) {
  auto impl = _impl.get<SPVRIMPL_ptr_t>();
  return left_eye ? impl->_ssaadownsamplebufferL : impl->_ssaadownsamplebufferR;
}
///////////////////////////////////////////////////////////////////////////////
void SinglePassStereoVrOutputNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<SPVRIMPL_ptr_t>()->gpuInit(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void SinglePassStereoVrOutputNode::onGpuUpdate(CompositorDrawData& drawdata) {
  // exactly-once-per-frame device update, for the same XR pacing contract DualMonoVr
  //  documents (one xrWaitFrame/xrBeginFrame per frame). The world/root transform is fed
  //  in FIRST so this frame's eye cameras carry the walker's placement.
  auto impl        = _impl.get<SPVRIMPL_ptr_t>();
  fmtx4 rootmatrix = impl->_computeRootMatrix(drawdata);
  orkidvr::device()->_usermtxgen = [rootmatrix]() -> fmtx4 { return rootmatrix; };
  orkidvr::device()->gpuUpdate(*drawdata.RCFD());
}
///////////////////////////////////////////////////////////////////////////////
compdrawdata_fn_t SinglePassStereoVrOutputNode::createAssembler(nodecompositortechnique_ptr_t tek) {
  return [this, tek](CompositorDrawData& drawdata) {
    auto rnode   = tek->_renderNode;
    auto context = drawdata.context();
    auto impl    = _impl.get<SPVRIMPL_ptr_t>();

    OrkProfilerSampleScope(CHANNEL_MAIN, "cpu:spvr:assemble");
    context->debugPushGroup("SinglePassStereoVrOutputNode::assemble");

    if (_onBeginAssemble) {
      _onBeginAssemble(drawdata);
    }

    ////////////////////////////////////////////////////////////////////////////
    // ONE scene pass, both eyes. The render node's whole chain (depth prepass,
    //  skybox, color) runs once against the 2-layer multiview group — that chain is
    //  order-dependent and not separable, which is why the collapse happens here and
    //  not inside the render node.
    ////////////////////////////////////////////////////////////////////////////
    rtgroup_ptr_t render_outg = rnode ? rnode->GetOutputGroup() : nullptr;
    RtBuffer* render_out      = rnode ? rnode->GetOutput().get() : nullptr;
    drawdata._properties["render_out"_crcu].set<RtBuffer*>(render_out);
    drawdata._properties["render_outgroup"_crcu].set<rtgroup_ptr_t>(render_outg);

    {
      OrkProfilerSampleScope(CHANNEL_MAIN, "cpu:spvr:begin");
      context->debugPushGroup("SinglePassStereoVrOutputNode::beginAssemble");
      impl->_beginAssemble(drawdata);
      context->debugPopGroup();
    }
    {
      OrkProfilerSampleScope(CHANNEL_MAIN, "cpu:spvr:render");
      context->debugPushGroup("SinglePassStereoVrOutputNode::layeredScenePass");
      rnode->Render(drawdata);
      context->debugPopGroup();
    }
    {
      OrkProfilerSampleScope(CHANNEL_MAIN, "cpu:spvr:end");
      context->debugPushGroup("SinglePassStereoVrOutputNode::endAssemble");
      impl->_endAssemble(drawdata);
      context->debugPopGroup();
    }

    // re-read: the render node may have (re)built its primary group this frame
    //  (msaa level / resize), and the eye work below must not run against a dead one.
    render_outg = rnode ? rnode->GetOutputGroup() : nullptr;

    ////////////////////////////////////////////////////////////////////////////
    // ...and everything downstream stays PER EYE (R6).
    ////////////////////////////////////////////////////////////////////////////
    auto do_for_eye = [&](bool is_left_eye) {
      auto framedata = drawdata.RCFD();
      framedata->setUserProperty("eye_index"_crcu, int(is_left_eye ? 0 : 1));

      rtgroup_ptr_t eye_outg = nullptr;
      {
        SpvrEyeStageScope("extract");
        eye_outg = impl->_extractEye(drawdata, render_outg, is_left_eye);
      }
      RtBuffer* eye_out = eye_outg->buffer(0).get();
      drawdata._properties["render_out"_crcu].set<RtBuffer*>(eye_out);
      drawdata._properties["render_outgroup"_crcu].set<rtgroup_ptr_t>(eye_outg);
      ////////////////////////////////////////////////////////////////////////////
      {
        SpvrEyeStageScope("postfx");
        for (auto pfxnode : tek->_postEffectNodes) {
          if (pfxnode->_disabled) {
            continue;
          }
          drawdata._properties["postfx_in"_crcu].set<rtgroup_ptr_t>(eye_outg);
          pfxnode->Render(drawdata);
          eye_outg = pfxnode->GetOutputGroup();
          eye_out  = pfxnode->GetOutput().get();
        }
      }
      drawdata._properties["final_out"_crcu].set<RtBuffer*>(eye_out);
      drawdata._properties["final_outgroup"_crcu].set<rtgroup_ptr_t>(eye_outg);
      ////////////////////////////////////////////////////////////////////////////
      if (eye_out) {
        SpvrEyeStageScope("downsample");
        impl->_downsample(drawdata, eye_out, is_left_eye);
      }
    };

    if (render_outg) {
      context->debugPushGroup("SinglePassStereoVrOutputNode::eyeL");
      do_for_eye(true);
      context->debugPopGroup();
      context->debugPushGroup("SinglePassStereoVrOutputNode::eyeR");
      do_for_eye(false);
      context->debugPopGroup();
    }

    if (_onEndAssemble) {
      _onEndAssemble(drawdata);
    }

    context->debugPopGroup();
  };
}
///////////////////////////////////////////////////////////////////////////////
void SinglePassStereoVrOutputNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("SinglePassStereoVrOutputNode::composite");
  auto impl        = _impl.get<SPVRIMPL_ptr_t>();
  Context* context = drawdata.context();
  auto vrdev       = orkidvr::device();
  // The XR runtime owns HMD presentation only for the OpenXR device while active; in that
  //  mode it applies its own lens distortion, so the client-side lambda is not adopted.
  const bool runtime_owns = vrdev and vrdev->_active and vrdev->ownsHmdPresentation();
  static bool s_spvrout_first = false;
  if (not s_spvrout_first) {
    s_spvrout_first = true;
    // INSTALLATION PROOF, at the point of USE. "autoselect armed" only says an env
    //  var was read and "the node was constructed" only says a preset resolved;
    //  this fires the first time the node's own composite actually runs a frame,
    //  which is the only thing that proves the single-pass path is what rendered.
    //  Grep token: SPVR:NODE
    printf("[SPVR:NODE] COMPOSITE first frame — SinglePassStereoVrOutputNode RUNNING\n");
    fflush(stdout);
    printf("[VROUT] SinglePassStereoVrOutputNode::composite first entry (vrdev=%d active=%d ownsHmdPresentation=%d) — per-eye __compositeStereo %s.\n",
           int(vrdev != nullptr),
           int(vrdev ? vrdev->_active : false),
           int(runtime_owns),
           runtime_owns ? "WILL be called (runtime owns presentation)" : "will NOT be called (desktop/preview)");
    fflush(stdout);
  }
  if (not runtime_owns) {
    auto pres     = orkidvr::device()->_presentation;
    auto pres_raw = pres.get();
    if (pres_raw != impl->_installedPresentation) {
      _distortion_lambda           = pres_raw ? pres_raw->genLambda() : distortion_lambda_t();
      impl->_installedPresentation = pres_raw;
    }
  }
  /////////////////////////////////////////////////////////////////////////////
  auto fbi = context->FBI();
  auto dwi = context->DWI();

  if (auto try_final = drawdata._properties["final_out"_crcu].tryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      auto tex = buffer->texture();
      if (tex) {
        auto framedata = drawdata.RCFD();

        /////////////////////////////////////////////////////////////////////////////
        // XR runtime handoff — the SAME two per-eye textures DualMonoVr hands over.
        //  Depth layers are NOT published by this node in phase 1: the scene depth is a
        //  2-layer array and the depth-layer capture path takes a single 2D texture, so
        //  publishing one eye's slice under both eyes' names would be a silent lie. The
        //  device chains color-only on nulls, which is its documented behaviour.
        /////////////////////////////////////////////////////////////////////////////
        if (runtime_owns) {
          auto texL = impl->_ssaadownsamplebufferL ? impl->_ssaadownsamplebufferL->texture(0).get() : nullptr;
          auto texR = impl->_ssaadownsamplebufferR ? impl->_ssaadownsamplebufferR->texture(0).get() : nullptr;
          static bool s_depthlayer_warned = false;
          if (not s_depthlayer_warned) {
            s_depthlayer_warned = true;
            printf("[VROUT] SPVR: XR depth layer NOT published (layered scene depth has no single-2D "
                   "per-eye capture path yet) — the runtime chains color-only for this node.\n");
            fflush(stdout);
          }
          context->debugPushGroup("SinglePassStereoVrOutputNode::compositeStereo");
          vrdev->__compositeStereo(context, texL, texR, nullptr, nullptr);
          context->debugPopGroup();
        }

        /////////////////////////////////////////////////////////////////////////////
        // desktop mirror — identical to the DualMonoVr composite it must be diffable
        //  against (same buffers, same halves, same flip decision, same techniques).
        /////////////////////////////////////////////////////////////////////////////
        drawdata.context()->debugPushGroup("SinglePassStereoVrOutputNode::to_screen");

        auto& mtl             = impl->_blit2screenmtl;
        auto this_buf         = context->FBI()->GetThisBuffer();
        auto tek_nodownsample = impl->_fxtechnique_downsample[0];

        if (_distortion_lambda) { // Lens distortion ?
          drawdata.context()->debugPushGroup("SinglePassStereoVrOutputNode::distortion_lambda");
          int out_surface_width  = context->mainSurfaceWidth();
          int out_surface_height = context->mainSurfaceHeight();
          int wd2                = out_surface_width >> 1;
          int h                  = out_surface_height;
          DistortionRect drectL  = {impl->_ssaadownsamplebufferL->texture(0).get(), SRect(wd2, 0, wd2 * 2, h), 'L'};
          DistortionRect drectR  = {impl->_ssaadownsamplebufferR->texture(0).get(), SRect(0, 0, wd2, h), 'R'};
          _distortion_lambda(framedata, drectL);
          _distortion_lambda(framedata, drectR);
          drawdata.context()->debugPopGroup();
        } else { // no lens distortion
          drawdata.context()->debugPushGroup("SinglePassStereoVrOutputNode::to_hmd");

          mtl.begin(tek_nodownsample, framedata);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());

          int out_surface_width  = context->mainSurfaceWidth();
          int out_surface_height = context->mainSurfaceHeight();
          ViewportRect extents(0, 0, out_surface_width, out_surface_height);
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);

          // presentation origin — see the DualMonoVr peer for the derivation: the eye
          //  buffers are top-down (runtime convention), the presented surface origin is the
          //  context's, and DWI::quad2D is the only quad path that applies that correction.
          const fvec4 uvrect = _flipY ? fvec4(0, 0, 1, 1) : fvec4(0, 1, 1, -1);

          auto tex_l = impl->_ssaadownsamplebufferL->texture(0).get();
          mtl.bindParamTexture(impl->_fxpColorMap, tex_l);
          dwi->quad2D(fvec4(-1, -1, 1, 2), uvrect, uvrect);

          auto tex_r = impl->_ssaadownsamplebufferR->texture(0).get();
          mtl.bindParamTexture(impl->_fxpColorMap, tex_r);
          dwi->quad2D(fvec4(0, -1, 1, 2), uvrect, uvrect);

          fbi->popViewport();
          fbi->popScissor();

          mtl.end(framedata);

          drawdata.context()->debugPopGroup();
        } // no distortion

        drawdata.context()->debugPopGroup();
      }
    }
  }
  drawdata.context()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
