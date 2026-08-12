////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "fwdnode_impl.h"
#include <ork/util/logger.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/sky_atmosphere.h> // the mode lives on SkyAtmosphereData; SkyFrameState carries the LUTs
#include <ork/lev2/gfx/image.h>                                 // ORKID_HAZESHAFT_READBACK host round-trip

///////////////////////////////////////////////////////////////////////////////
// QUARTER-RES SUN SHAFTS — the engine half of SkyAtmosphereData::_hazeSunShadow
// mode 2. The shader half, and the argument for why the arithmetic is exact
// rather than approximate, is in hazeshaft.fxv2; read that first.
//
// The two passes and where they sit in the frame:
//
//   _render_hazeshaft    runs AFTER the depth prepass has published DEPTH_MAP
//                        and the depth image has been transitioned for
//                        sampling, and BEFORE the color pass pushes the primary
//                        RTG. It owns its own (half-dims) target, so it must
//                        not be inside anybody else's pass.
//
//   _composite_hazeshaft runs INSIDE the color pass, after the opaque image is
//                        complete (including the draw-last segment — the grass
//                        carpet is opaque and wants its shafts) and before
//                        std_transparent. Additive blend, negative radiance:
//                        dst = dst - loss is the only read-modify-write a
//                        fragment gets on the attachment it is drawing to.
//
// ORDER IS THE CONTRACT. _hazeshaft_marched is cleared at the top of every
// march and only set once one has actually landed, so a composite can never
// subtract a loss target that this view did not write this frame.
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2::pbr {

static logchannel_ptr_t logchan_hzs = logger()->configureChannel("HAZESHAFT", fvec3(0.9, 0.7, 0.2), true);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEBUG READBACK — ORKID_HAZESHAFT_READBACK is a bitmask: 1 = the quarter-res
// loss target, 2 = the depth image the march reconstructs world positions from.
// Unset = nothing happens at all. This is the instrument that splits a
// frame-to-frame difference in the COMPOSITED image into "the march produced a
// different loss" versus "the composite consumed the same loss differently",
// which no amount of looking at the final image can decide. It is what proved
// both of those inputs byte-identical while the composited frame was not, and
// so what pointed at the depth attachment the mirror now stands in for.
//
// It reads at the TOP of the march, so what comes back is the PREVIOUS frame's
// content of a target this frame is about to overwrite: no live sampler depends
// on the layout round-trip the readback performs. A device-local readback
// dropped into the middle of the frame graph aborts the command buffer;
// captureAsFormat suspends the render pass, copies into host staging and
// restores the layout, which is the only shape of readback this graph tolerates.
//
// ORKID_HAZESHAFT_READBACK_DUMP=<prefix> additionally writes the raw texels of
// every read frame to <prefix>_loss_<frame>.bin / _depth_<frame>.bin, so a
// difference can be located spatially instead of only detected.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

int _hzs_readback_mask() {
  static int mask = []() -> int {
    const char* e = getenv("ORKID_HAZESHAFT_READBACK");
    return e ? atoi(e) : 0;
  }();
  return mask;
}

const char* _hzs_readback_dump_prefix() {
  static const char* pfx = getenv("ORKID_HAZESHAFT_READBACK_DUMP");
  return pfx;
}

uint64_t _hzs_fnv1a(const void* data, size_t len) {
  const uint8_t* p = (const uint8_t*)data;
  uint64_t h       = 0xcbf29ce484222325ull;
  for (size_t i = 0; i < len; i++) {
    h ^= uint64_t(p[i]);
    h *= 0x100000001b3ull;
  }
  return h;
}

void _hzs_dump_raw(const char* tag, int frame, const void* data, size_t len) {
  const char* pfx = _hzs_readback_dump_prefix();
  if (not pfx)
    return;
  // one dump every ORKID_HAZESHAFT_READBACK_EVERY frames (default every frame) —
  // a full-res RGBA16F frame is megabytes and a settle is hundreds of frames.
  static int every = []() -> int {
    const char* e = getenv("ORKID_HAZESHAFT_READBACK_EVERY");
    return (e and atoi(e) > 0) ? atoi(e) : 1;
  }();
  if (frame % every)
    return;
  auto path = FormatString("%s_%s_%05d.bin", pfx, tag, frame);
  if (FILE* f = fopen(path.c_str(), "wb")) {
    fwrite(data, 1, len, f);
    fclose(f);
  }
}

int _hzs_debug_frame = 0;

} // namespace

void ForwardPbrNodeImpl::_hazeshaft_debug_readback(rtbuffer_ptr_t depthbuf) {
  int mask = _hzs_readback_mask();
  if (0 == mask)
    return;
  auto FBI  = _currentContext->FBI();
  int frame = _hzs_debug_frame;

  if ((mask & 1) and _rtg_hazeshaft) {
    auto capbuf = std::make_shared<CaptureBuffer>();
    FBI->captureAsFormat(_rtg_hazeshaft->buffer(0).get(), capbuf, EBufferFormat::RGBA16F, [capbuf, frame]() {
      auto img    = capbuf->_image;
      size_t len  = size_t(img->_width) * size_t(img->_height) * 8;
      const void* d = img->_data->data();
      // the half-float texels, summed as a scalar so a hash mismatch can be read
      // against a magnitude rather than only reported as "different".
      double sum   = 0.0;
      int nonzero  = 0;
      const uint16_t* h16 = (const uint16_t*)d;
      size_t ntex  = size_t(img->_width) * size_t(img->_height);
      for (size_t i = 0; i < ntex; i++) {
        const uint16_t* px = h16 + i * 4;
        bool nz = false;
        for (int c = 0; c < 3; c++) {
          uint16_t v = px[c];
          if (v & 0x7fff)
            nz = true;
          // half -> float, enough for a magnitude
          int e = (v >> 10) & 0x1f;
          int m = v & 0x3ff;
          float f = (e == 0) ? (float(m) * 5.96046448e-8f) : ldexpf(1.0f + float(m) / 1024.0f, e - 15);
          sum += (v & 0x8000) ? -double(f) : double(f);
        }
        if (nz)
          nonzero++;
      }
      printf(
          "[hzsdbg] frame<%d> loss %dx%d hash<%016llx> nonzero<%d> sum<%.9g>\n",
          frame,
          int(img->_width),
          int(img->_height),
          (unsigned long long)_hzs_fnv1a(d, len),
          nonzero,
          sum);
      fflush(stdout);
      _hzs_dump_raw("loss", frame, d, len);
    });
  }

  if ((mask & 2) and depthbuf) {
    auto capbuf = std::make_shared<CaptureBuffer>();
    FBI->captureAsFormat(depthbuf.get(), capbuf, EBufferFormat::R32F, [capbuf, frame]() {
      auto img      = capbuf->_image;
      size_t ntex   = size_t(img->_width) * size_t(img->_height);
      const float* d = (const float*)img->_data->data();
      double sum    = 0.0;
      int scene     = 0;
      for (size_t i = 0; i < ntex; i++) {
        sum += double(d[i]);
        if (d[i] < 0.999999f)
          scene++;
      }
      printf(
          "[hzsdbg] frame<%d> depth %dx%d hash<%016llx> scenepx<%d> sum<%.9g>\n",
          frame,
          int(img->_width),
          int(img->_height),
          (unsigned long long)_hzs_fnv1a(d, ntex * 4),
          scene,
          sum);
      fflush(stdout);
      _hzs_dump_raw("depth", frame, d, ntex * 4);
    });
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_init_hazeshaft(lev2::Context* context) {
  _hazeshaft_material = std::make_shared<FreestyleMaterial>();
  _hazeshaft_material->gpuInit(context, "orkshader://hazeshaft");

  _tek_hzs_depthmirror        = _hazeshaft_material->technique("hazeshaft_depthmirror");
  _tek_hzs_depthmirror_stereo = _hazeshaft_material->technique("hazeshaft_depthmirror_stereo");
  _tek_hzs_march            = _hazeshaft_material->technique("hazeshaft_march");
  _tek_hzs_march_stereo     = _hazeshaft_material->technique("hazeshaft_march_stereo");
  _tek_hzs_composite        = _hazeshaft_material->technique("hazeshaft_composite");
  _tek_hzs_composite_stereo = _hazeshaft_material->technique("hazeshaft_composite_stereo");

  _par_hzs_ublk_sun    = _hazeshaft_material->uniformBlock("ublk_sun");
  _par_hzs_ublk_stereo = _hazeshaft_material->uniformBlock("ublk_stereo");

  _par_hzs_mvp          = _hazeshaft_material->param("mvp");
  _par_hzs_invvp        = _hazeshaft_material->param("inv_vp");
  _par_hzs_invvpsize    = _hazeshaft_material->param("InvViewportSize");
  _par_hzs_eyepos       = _hazeshaft_material->param("HzsEyePos");
  _par_hzs_lowdim       = _hazeshaft_material->param("HzsLowDim");
  _par_hzs_geom         = _hazeshaft_material->param("HzsGeom");
  _par_hzs_depth        = _hazeshaft_material->param("HzsDepthMap");
  _par_hzs_depth_array  = _hazeshaft_material->param("HzsDepthMapArray");
  _par_hzs_loss         = _hazeshaft_material->param("HzsLossMap");
  _par_hzs_loss_array   = _hazeshaft_material->param("HzsLossMapArray");
  _par_hzs_sunshadowmap = _hazeshaft_material->param("sun_shadow_map");

  // the ONE haze binder, by handle rather than by material — the march reads
  // the same atmosphere the forward fragments do, and a second copy of that
  // packing here is exactly how the two looks would drift.
  _hzs_sky_params._density          = _hazeshaft_material->param("SkyHazeDensity");
  _hzs_sky_params._rayleighScatter  = _hazeshaft_material->param("SkyRayleighScatter");
  _hzs_sky_params._mieScatter       = _hazeshaft_material->param("SkyMieScatter");
  _hzs_sky_params._ozoneAbsorb      = _hazeshaft_material->param("SkyOzoneAbsorb");
  _hzs_sky_params._ozoneTent        = _hazeshaft_material->param("SkyOzoneTent");
  _hzs_sky_params._radii            = _hazeshaft_material->param("SkyRadii");
  _hzs_sky_params._sunDirection     = _hazeshaft_material->param("SkySunDirection");
  _hzs_sky_params._sunIlluminance   = _hazeshaft_material->param("SkySunIlluminance");
  _hzs_sky_params._scatterTint      = _hazeshaft_material->param("SkyHazeScatterTint");
  _hzs_sky_params._inscatterTint    = _hazeshaft_material->param("SkyHazeInscatterTint");
  _hzs_sky_params._geom             = _hazeshaft_material->param("SkyHazeGeom");
  _hzs_sky_params._transmittanceLut = _hazeshaft_material->param("SkyTransmittanceLUT");
  _hzs_sky_params._multiScatterLut  = _hazeshaft_material->param("SkyMultiScatterLUT");
  _hzs_sky_params._viewLut          = _hazeshaft_material->param("SkyViewLUT");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The mode, reduced against everything that disarms the haze itself. Reducing
// here rather than at each call site is what keeps the march and the composite
// from ever disagreeing about whether this pass has shafts.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int ForwardPbrNodeImpl::_hazeshaft_mode(forward_pass_ptr_t fpass) const {
  if (fpass->_renderingPROBE)
    return 0; // no haze in probe captures (v1) — the binder disarms it too
  auto pbrcommon = _node->_pbrcommon;
  auto atmo      = pbrcommon ? pbrcommon->_atmosphere : nullptr;
  if (not atmo)
    return 0;
  if (not atmo->_aerialPerspectiveEnable)
    return 0;
  // the march samples the same two LUTs the forward fragments do; without a
  // published sky frame the haze branch is not taken at all.
  if (not _currentRCFD->hasUserProperty("SKY_FRAME"_crcu))
    return 0;
  auto skyframe = _currentRCFD->userPropertyAs<pbr::skyframestate_ptr_t>("SKY_FRAME"_crcu);
  if (not(skyframe and skyframe->_transmittanceLUT and skyframe->_multiScatterLUT))
    return 0;
  float m = atmo->_hazeSunShadow;
  if (m > 1.5f)
    return 2;
  if (m > 0.5f)
    return 1;
  return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MARCH — half dims, one eye ray per low-res pixel.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_hazeshaft(forward_pass_ptr_t fpass) {

  _hazeshaft_marched = false;

  if (_hazeshaft_mode(fpass) != 2)
    return;

  auto rtg_out   = fpass->_rtg_out;
  auto FBI       = _currentContext->FBI();
  auto GBI       = _currentContext->GBI();
  auto FXI       = _currentContext->FXI();
  auto pbrcommon = _node->_pbrcommon;

  ///////////////////////////////////////////////////////////////////////////
  // THE PRECONDITION, AND IT IS NOT NEGOTIABLE. The march reconstructs a world
  // position per low-res pixel out of the SINGLE-SAMPLE scene depth the depth
  // prepass fills. Without that image there is no geometry to march toward, and
  // a march against the color pass's own write target would read undefined
  // depth — shafts placed at random distances, which is worse than none. The
  // prepass is an engine invariant, so reaching here without one means a rig
  // built its own pass and forgot: say so by name and render no shafts rather
  // than render wrong ones.
  ///////////////////////////////////////////////////////////////////////////
  texture_ptr_t depthtex;
  if (pbrcommon and pbrcommon->_useDepthPrepass and rtg_out->_depthBuffer)
    depthtex = rtg_out->_depthBuffer->_texture;
  if (not depthtex) {
    if (not _hazeshaft_nodepth_warned) {
      _hazeshaft_nodepth_warned = true;
      printf("[HAZESHAFT] ERROR haze-in-shadow mode 2 (quarter-res) needs the DEPTH PREPASS — this "
             "pass publishes no resolved scene depth to reconstruct world positions from, so the "
             "shaft march is REFUSED and this frame carries NO sun shafts at all (the haze itself "
             "is unaffected). Enable pbr_common.useDepthPrepass, or author haze shadow mode 1 "
             "(inline), which needs no depth image.\n");
      fflush(stdout);
    }
    return;
  }

  _hzs_debug_frame++;
  _hazeshaft_debug_readback(rtg_out->_depthBuffer);

  int lowW = std::max(1, _currentWidth / 2);
  int lowH = std::max(1, _currentHeight / 2);
  bool spvr = fpass->_single_pass_stereo;

  ///////////////////////////////////////////////////////////////////////////
  // THE DEPTH MIRROR, and it is a correctness pass rather than a copy for
  // convenience. The composite half of this effect draws from INSIDE the colour
  // pass, and that pass holds this depth image as its depth attachment; a
  // fragment that samples an image its own render pass has attached reads
  // values with no ordering against the pass's other accesses to it, and they
  // are not the same values twice. That is the whole of the frame instability
  // mode 2 shipped with — see the ledger in hazeshaft.fxv2. Every shaft-pass
  // depth fetch, the march's included, therefore goes through this copy, which
  // no pass has attached and which both halves read identically.
  ///////////////////////////////////////////////////////////////////////////
  if (not _rtg_hazeshaft_depth) {
    _rtg_hazeshaft_depth = std::make_shared<RtGroup>(_currentContext, _currentWidth, _currentHeight);
    if (spvr) {
      _rtg_hazeshaft_depth->_numLayers = 2;
      _rtg_hazeshaft_depth->_multiview = true;
    }
    auto buf        = _rtg_hazeshaft_depth->createRenderTarget(EBufferFormat::R32F);
    buf->_debugName = "HazeShaftDepthMirror";
  }
  if (_rtg_hazeshaft_depth->width() != _currentWidth or _rtg_hazeshaft_depth->height() != _currentHeight)
    _rtg_hazeshaft_depth->Resize(_currentWidth, _currentHeight);

  _currentRCFD->_subpassID = "HAZESHAFT_DEPTHMIRROR"_crcu;
  {
    auto autorelease_mirror = _currentContext->debugPushGroupAutoRelease("ForwardPBR::hazeshaft-depthmirror");

    // NO clear: the copy covers every texel of its own target every frame.
    _rtg_hazeshaft_depth->_autoclear      = false;
    _rtg_hazeshaft_depth->_depthOnly      = false;
    _rtg_hazeshaft_depth->_clearMaskDepth = false;
    _rtg_hazeshaft_depth->_clearMaskColor = false;

    FBI->PushRtGroup(_rtg_hazeshaft_depth.get());

    _hazeshaft_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
    _hazeshaft_material->_rasterstate->setDepthTest(EDepthTest::OFF);
    _hazeshaft_material->_rasterstate->setCullTest(ECullTest::OFF);
    _hazeshaft_material->_rasterstate->setWriteMaskZ(false);
    _hazeshaft_material->_rasterstate->setWriteMaskRGB(true);
    _hazeshaft_material->_rasterstate->setWriteMaskA(true);

    _hazeshaft_material->begin(spvr ? _tek_hzs_depthmirror_stereo : _tek_hzs_depthmirror, _currentRCFD);
    _hazeshaft_material->bindParamMatrix(_par_hzs_mvp, fmtx4::Identity());
    _hazeshaft_material->bindParamVec2(
        _par_hzs_invvpsize, fvec2(1.0f / float(_currentWidth), 1.0f / float(_currentHeight)));
    if (spvr) {
      if (_par_hzs_depth_array)
        _hazeshaft_material->bindParamTexture(_par_hzs_depth_array, depthtex.get());
    } else {
      if (_par_hzs_depth)
        _hazeshaft_material->bindParamTexture(_par_hzs_depth, depthtex.get());
    }

    ViewportRect full(0, 0, _currentWidth, _currentHeight);
    FBI->pushViewport(full);
    FBI->pushScissor(full);
    GBI->render2dQuadEML();
    FBI->popViewport();
    FBI->popScissor();

    _hazeshaft_material->end(_currentRCFD);
    FBI->PopRtGroup();
  }

  // from here on, "the scene depth" means the mirror.
  auto mirrortex = _rtg_hazeshaft_depth->texture(0);

  ///////////////////////////////////////////////////////////////////////////
  // the target: half the primary's dims, one layer per view. Lazily created so
  // scenes that never ask for mode 2 allocate nothing.
  ///////////////////////////////////////////////////////////////////////////
  if (not _rtg_hazeshaft) {
    _rtg_hazeshaft = std::make_shared<RtGroup>(_currentContext, lowW, lowH);
    if (spvr) {
      // layered + multiview, for the same reason the primary group is: the two
      // eyes' rays are different rays, so their losses are different images.
      _rtg_hazeshaft->_numLayers = 2;
      _rtg_hazeshaft->_multiview = true;
    }
    auto buf         = _rtg_hazeshaft->createRenderTarget(EBufferFormat::RGBA16F);
    buf->_debugName  = "HazeShaftLoss";
  }
  if (_rtg_hazeshaft->width() != lowW or _rtg_hazeshaft->height() != lowH)
    _rtg_hazeshaft->Resize(lowW, lowH);

  _currentRCFD->_subpassID = "HAZESHAFT_MARCH"_crcu;
  auto autorelease_group   = _currentContext->debugPushGroupAutoRelease("ForwardPBR::hazeshaft-march");

  // CLEARED, always: a pixel the shader returns early on (sky) must read as
  // zero loss for the composite's neighbourhood taps, not as last frame's shaft.
  _rtg_hazeshaft->_autoclear             = true;
  _rtg_hazeshaft->_depthOnly             = false;
  _rtg_hazeshaft->_clearMaskDepth        = false;
  _rtg_hazeshaft->_clearMaskColor        = true;
  _rtg_hazeshaft->buffer(0)->_clearColor = fvec4(0, 0, 0, 0);

  FBI->PushRtGroup(_rtg_hazeshaft.get());

  RenderContextInstData RCID(_currentRCFD);

  _hazeshaft_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  _hazeshaft_material->_rasterstate->setDepthTest(EDepthTest::OFF);
  _hazeshaft_material->_rasterstate->setCullTest(ECullTest::OFF);
  _hazeshaft_material->_rasterstate->setWriteMaskZ(false);
  _hazeshaft_material->_rasterstate->setWriteMaskRGB(true);
  _hazeshaft_material->_rasterstate->setWriteMaskA(true);

  _hazeshaft_material->begin(spvr ? _tek_hzs_march_stereo : _tek_hzs_march, _currentRCFD);

  _hazeshaft_material->bindParamMatrix(_par_hzs_mvp, fmtx4::Identity());
  _hazeshaft_material->bindParamVec2(_par_hzs_invvpsize, fvec2(1.0f / float(lowW), 1.0f / float(lowH)));
  _hazeshaft_material->bindParamVec4(_par_hzs_lowdim, fvec4(float(lowW), float(lowH), 1.0f / float(lowW), 1.0f / float(lowH)));
  // x: the far-depth test. A cleared depth attachment reads exactly 1.0, so any
  //    pixel at or above this has no scene behind it and the sky owns it.
  // y: bilateral tolerance as a FRACTION of the pixel's own eye distance (the
  //    composite's; harmless here).
  // z: layered.
  _hazeshaft_material->bindParamVec4(_par_hzs_geom, fvec4(0.999999f, 0.05f, spvr ? 1.0f : 0.0f, 0.0f));

  if (spvr) {
    // the per-view state the _ST stages index. WRITTEN here rather than
    // inherited: this pass runs BEFORE the color pass's first draw, so the
    // block the forward materials write per-frame has not been written yet this
    // frame and would still hold the previous frame's head pose.
    const auto& CPD = _currentCIMPL->topCPD();
    OrkAssertI(
        CPD._stereo_cam_matrices,
        "single-pass-stereo shaft march with no stereo camera matrices — the eyes' rays are unknowable");
    PBRMaterial::writeStereoBlock(FXI, _currentContext, CPD._stereo_cam_matrices);
    if (_par_hzs_ublk_stereo)
      FXI->bindUniformBuffer(_par_hzs_ublk_stereo, PBRMaterial::stereoDataBuffer(_currentContext));
    if (_par_hzs_depth_array)
      _hazeshaft_material->bindParamTexture(_par_hzs_depth_array, mirrortex.get());
  } else {
    _hazeshaft_material->bindParamMatrix(_par_hzs_invvp, _currentViewData.IVPL);
    _hazeshaft_material->bindParamVec4(_par_hzs_eyepos, fvec4(_currentViewData.VL.inverse().translation(), 1.0f));
    if (_par_hzs_depth)
      _hazeshaft_material->bindParamTexture(_par_hzs_depth, mirrortex.get());
  }

  // sun cascade state — the same per-frame UBO and the same depth array the
  // forward lighting lambda binds, so the air is tested against exactly the
  // cascades the surfaces were.
  auto lmgr = _currentCIMPL->_lightmgr;
  if (_par_hzs_ublk_sun)
    FXI->bindUniformBuffer(_par_hzs_ublk_sun, PBRMaterial::sunDataBuffer(_currentContext));
  if (_par_hzs_sunshadowmap and lmgr)
    _hazeshaft_material->bindParamTextureArray(_par_hzs_sunshadowmap, lmgr->_sun_shadow_cascades.get());

  bindSkyHazeState(_hzs_sky_params, RCID);

  ViewportRect extents(0, 0, lowW, lowH);
  FBI->pushViewport(extents);
  FBI->pushScissor(extents);
  GBI->render2dQuadEML();
  FBI->popViewport();
  FBI->popScissor();

  _hazeshaft_material->end(_currentRCFD);

  FBI->PopRtGroup();

  ///////////////////////////////////////////////////////////////////////////
  // ORDERING: the composite samples this image LATER IN THE SAME FRAME, from
  // inside the color pass, and popping a render target does not by itself order
  // those writes against that read. Every other in-frame producer of a sampled
  // color target in this tree does the same (the sky LUT chain, the pick pass).
  //
  // HONESTY NOTE: this was added while chasing a frame-to-frame instability in
  // mode 2 and it did NOT resolve it (the instability is unchanged with and
  // without). It stays because the dependency is real regardless of what it
  // does or does not explain — not because it fixed anything.
  ///////////////////////////////////////////////////////////////////////////
  FBI->rtGroupTransitionToTexture(_rtg_hazeshaft.get());

  _hazeshaft_marched = true;

  ///////////////////////////////////////////////////////////////////////////
  // OBSERVABLE ENGAGEMENT, at the site that actually dispatched — "the branch
  // compiled" is true of a pass that never ran. Once per run. Grep: HAZESHAFT
  ///////////////////////////////////////////////////////////////////////////
  if (not _hazeshaft_engaged) {
    _hazeshaft_engaged = true;
    logchan_hzs->log(
        "quarter-res shaft march ENGAGED low<%dx%d> full<%dx%d> layers<%d> multiview<%d>",
        lowW,
        lowH,
        _currentWidth,
        _currentHeight,
        _rtg_hazeshaft->_numLayers,
        int(_rtg_hazeshaft->_multiview));
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// COMPOSITE — full res, additive, negative radiance. Called from inside the
// color pass, which is already pushed; this adds no push/pop of its own.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_composite_hazeshaft(forward_pass_ptr_t fpass) {

  if (not _hazeshaft_marched)
    return;

  auto rtg_out = fpass->_rtg_out;
  auto FBI     = _currentContext->FBI();
  auto GBI     = _currentContext->GBI();
  auto FXI     = _currentContext->FXI();
  bool spvr    = fpass->_single_pass_stereo;

  // THE MIRROR, NOT THE LIVE DEPTH. rtg_out's depth image is this pass's own
  // depth attachment and sampling it from here is what made mode 2 differ from
  // itself frame to frame; _render_hazeshaft published a copy for exactly this
  // fetch. Refuse rather than fall back — a fallback to the attachment would
  // reintroduce the instability silently.
  auto mirrortex = _rtg_hazeshaft_depth ? _rtg_hazeshaft_depth->texture(0) : nullptr;
  if (not mirrortex)
    return; // the march would not have run without one; nothing to say twice

  int lowW = _rtg_hazeshaft->width();
  int lowH = _rtg_hazeshaft->height();

  _currentRCFD->_subpassID = "HAZESHAFT_COMPOSITE"_crcu;
  auto autorelease_group   = _currentContext->debugPushGroupAutoRelease("ForwardPBR::hazeshaft-composite");

  // ADDITIVE, and the fragment emits -loss. Depth test and depth write both
  // off: the pass's depth attachment is read-only here and a full-screen quad
  // has no business in the depth buffer anyway.
  _hazeshaft_material->_rasterstate->setBlendingMacro(BlendingMacro::ADDITIVE);
  _hazeshaft_material->_rasterstate->setDepthTest(EDepthTest::OFF);
  _hazeshaft_material->_rasterstate->setCullTest(ECullTest::OFF);
  _hazeshaft_material->_rasterstate->setWriteMaskZ(false);
  _hazeshaft_material->_rasterstate->setWriteMaskRGB(true);
  // target1 (the SSSS diffuse-only buffer) must come out of this untouched, and
  // the fragment writes only location 0; alpha stays masked off so the additive
  // blend cannot disturb the alpha the forward pass wrote.
  _hazeshaft_material->_rasterstate->setWriteMaskA(false);

  _hazeshaft_material->begin(spvr ? _tek_hzs_composite_stereo : _tek_hzs_composite, _currentRCFD);

  _hazeshaft_material->bindParamMatrix(_par_hzs_mvp, fmtx4::Identity());
  _hazeshaft_material->bindParamVec2(
      _par_hzs_invvpsize, fvec2(1.0f / float(_currentWidth), 1.0f / float(_currentHeight)));
  _hazeshaft_material->bindParamVec4(
      _par_hzs_lowdim, fvec4(float(lowW), float(lowH), 1.0f / float(lowW), 1.0f / float(lowH)));
  _hazeshaft_material->bindParamVec4(_par_hzs_geom, fvec4(0.999999f, 0.05f, spvr ? 1.0f : 0.0f, 0.0f));

  if (spvr) {
    if (_par_hzs_ublk_stereo)
      FXI->bindUniformBuffer(_par_hzs_ublk_stereo, PBRMaterial::stereoDataBuffer(_currentContext));
    if (_par_hzs_depth_array)
      _hazeshaft_material->bindParamTexture(_par_hzs_depth_array, mirrortex.get());
    if (_par_hzs_loss_array)
      _hazeshaft_material->bindParamTexture(_par_hzs_loss_array, _rtg_hazeshaft->texture(0).get());
  } else {
    _hazeshaft_material->bindParamMatrix(_par_hzs_invvp, _currentViewData.IVPL);
    _hazeshaft_material->bindParamVec4(_par_hzs_eyepos, fvec4(_currentViewData.VL.inverse().translation(), 1.0f));
    if (_par_hzs_depth)
      _hazeshaft_material->bindParamTexture(_par_hzs_depth, mirrortex.get());
    if (_par_hzs_loss)
      _hazeshaft_material->bindParamTexture(_par_hzs_loss, _rtg_hazeshaft->texture(0).get());
  }

  ViewportRect extents(0, 0, _currentWidth, _currentHeight);
  FBI->pushViewport(extents);
  FBI->pushScissor(extents);
  GBI->render2dQuadEML();
  FBI->popViewport();
  FBI->popScissor();

  _hazeshaft_material->end(_currentRCFD);
}

} // namespace ork::lev2::pbr
