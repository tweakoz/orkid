////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "fwdnode_impl.h"
#include <ork/util/logger.h>
// Full Scene definition needed for scene->layersForRole() in the
// shadow-map pass below.
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/sky_atmosphere.h>
#include <ork/lev2/gfx/renderphasestats.h> // perf HUD render-phase timing sink
#include <chrono>
#include <ork/lev2/gfx/image.h>            // ORKID_SUN_COOKIE_DUMP readback

namespace ork::lev2::pbr {

static logchannel_ptr_t logchan_fwd_sub = logger()->configureChannel("pbrFWDSUB", fvec3(0.8, 0.8, 0.1), true);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_dpp(forward_pass_ptr_t fpass) {
  auto drawdata = fpass->_drawdata;
  auto rtg_out  = fpass->_rtg_out;
  auto FBI = _currentContext->FBI();

  //printf("render dppass rtg<%p>\n", (void*)rtg_out.get());

  _currentDrawQueue->enqueueLayerToRenderQueue( fpass->_dpp_pass_layer, //
                                                _currentIRenderer);     //

  //fpass->_fwd_pass_layer
  _currentRCFD->_renderingmodel = "DEPTH_PREPASS"_crcu;
  _currentRCFD->_subpassID      = "DEPTH_PREPASS"_crcu;

  _currentContext->debugPushGroup("ForwardPBR::depth-pre pass");
  // The prepass WRITES depth, so it must resolve MSAA depth into the
  // single-sample sampled image. Clear any leaked read-only flag (e.g. from
  // the HZB's compute-side depth sample, which transitions for sampling with
  // no matching PopRtGroup) so the resolve isn't suppressed. See
  // VkFrameBufferInterface::transitionDepthForWriting.
  FBI->transitionDepthForWriting(rtg_out);
  FBI->PushRtGroup(rtg_out.get());
  { // TEMPORARY (aug11) prepass draw census — ORKID_DEBUG_RTGID=1
    static const bool s_dppcensus = (getenv("ORKID_DEBUG_RTGID") != nullptr);
    if (s_dppcensus) {
      int f = _currentContext->GetTargetFrame();
      if ((f % 500) == 0) {
        printf("[RTGID] DPP      f<%d> rtg<%p> enqueued<%zu>\n",
               f, (void*)rtg_out.get(), _currentIRenderer->countEnqueuedAtOrAboveSortKey(0));
        fflush(stdout);
      }
    }
  }
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
  auto FXI      = context->FXI();
  auto GBI      = context->GBI();
  auto DWI      = context->DWI();

  context->debugPushGroup("ForwardPBR::skybox pass");
  //printf("render skybox rtg<%p>\n", (void*)rtg_out.get());

  RCFD->_renderingmodel = "CUSTOM"_crcu;
  RCFD->_subpassID      = "SKYBOX"_crcu;
  RenderContextInstData RCID(RCFD);
  RCID._pipeline_cache = _skybox_fxcache;
  // SKYLIGHT lane B — sky source switch (L6, per scene, no parity window).
  // Forcing the technique is what keys the pipeline cache, so both sources stay
  // cached and the choice is live per frame. BAKED leaves it null and resolves
  // to exactly the pipeline this pass always used.
  if (_node->_pbrcommon->_sky_source == SkySource::PROCEDURAL) {
    OrkAssertI(
        _skybox_material->_tek_FWD_SKYBOX_PROC != nullptr,
        "procedural sky source selected but orkshader://pbr declares no FWD_SKYBOX_PROC technique");
    OrkAssertI(
        RCFD->hasUserProperty("SKY_FRAME"_crcu),
        "procedural sky source selected but the forward prologue published no sky frame state");
    RCID._forced_technique = _skybox_material->_tek_FWD_SKYBOX_PROC;
  }
  auto pipeline        = _skybox_fxcache->findPipeline(RCID);
  rtg_out->_autoclear = true;

  // The sky NEVER writes depth (S3): a written far-ish z rejects every fragment
  // beyond it. The quad's 0.9999 (below) only has to beat the 1.0 depth clear for
  // the technique's LESS test, so the background keeps the cleared far value.
  // NOTE the depth/blend state a skybox pipeline is BUILT from comes from the
  // technique state block (sb_skybox in orkshader://pbr) — on a priority tie the
  // state block wins over this rasterstate, which reaches the draw as the dynamic
  // cull mode only. Keep the two in agreement.
  pipeline->_rasterstate->setWriteMaskZ(false);
  pipeline->_rasterstate->setWriteMaskRGB(true);
  pipeline->_rasterstate->setWriteMaskA(true);
  pipeline->_rasterstate->setDepthTest(EDepthTest::LESS);
  pipeline->_rasterstate->setCullTest(ECullTest::OFF);
  pipeline->bindUniformBuffer(_par_ublk_std_matrices, "ub_skybox"_crcu);
  pipeline->wrappedDrawCall(RCID, [=]() {
    FXI->applyRasterState(*pipeline->_rasterstate);
    DWI->fullscreenQuad(
        fvec4(0, 1, 1, -1),   // uv0 (x,y,w,h)
        fvec4(0, 1, 1, -1),   // uv1 (x,y,w,h)
        0.9999f);            // full screen quad, z just inside the 1.0 depth clear
  });
  context->debugPopGroup();
  //FBI->PopRtGroup();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_setupCubeFaceCamera(lightprobe_ptr_t probe, int iface) {
  auto CMATRIX = probe->_worldMatrix;
  fvec3 POSX = CMATRIX.xNormal() * -1;
  fvec3 POSY = CMATRIX.yNormal();
  fvec3 POSZ = CMATRIX.zNormal() * -1;
  fvec3 position = CMATRIX.translation();

  // compute projection matrix
  // Legacy GL→Vulkan Y-flip compensation removed: the rest of the
  // renderer no longer double-flips, so this per-face flip would now
  // leave every captured cubemap face upside-down relative to the new
  // top-left convention (and break the cube→equirect projection in
  // cube2equirectangular.fxv2).
  _CUBECAM->_pmatrix.perspective(90.0f * DTOR, 1.0f, 0.01f, 1000.0f);

  // compute view matrix from cubeface
  switch (iface) {
    case 1: _CUBECAM->_vmatrix.lookAt(position, position + POSX, POSY); break;
    case 0: _CUBECAM->_vmatrix.lookAt(position, position - POSX, POSY); break;
    case 2: _CUBECAM->_vmatrix.lookAt(position, position + POSY, POSZ * -1); break;
    case 3: _CUBECAM->_vmatrix.lookAt(position, position - POSY, POSZ); break;
    case 4: _CUBECAM->_vmatrix.lookAt(position, position + POSZ, POSY); break;
    case 5: _CUBECAM->_vmatrix.lookAt(position, position - POSZ, POSY); break;
  }

  _CUBECAM->_vpmatrix  = _CUBECAM->_vmatrix * _CUBECAM->_pmatrix;
  _CUBECAM->_ivpmatrix = _CUBECAM->_vpmatrix.inverse();
  _CUBECAM->_ivmatrix  = _CUBECAM->_vmatrix.inverse();
  _CUBECAM->_ipmatrix  = _CUBECAM->_pmatrix.inverse();
  _CUBECAM->_frustum.set(_CUBECAM->_vmatrix, _CUBECAM->_pmatrix);
  _CUBECAM->_explicitProjectionMatrix = true;
  _CUBECAM->_explicitViewMatrix       = true;
  _CUBECAM->_aspectRatio              = 1.0f;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_update_env_probes(CompositorDrawData& drawdata) {

  auto TXI     = _currentContext->TXI();
  auto topcomp = _currentRCFD->topCompositor();
  auto CPD     = _currentCIMPL->topCPD();

  // Phase 1: Allocate RTG/VRAM for ALL probes (including inactive)
  if (auto lmgr = _currentCIMPL->lightManager()) {
    for (auto& probe : lmgr->_lightprobes) {
      if (probe->_type == LightProbeType::REFLECTION && nullptr == probe->_cubeRenderRTG) {
        probe->_cubeRenderRTG           = std::make_shared<RtGroup>(_currentContext, 8, 8);
        probe->_cubeRenderRTG->_name    = "ReflectionProbeRTG";
        probe->_cubeRenderRTG->_cubeMap = true;
        auto colorbuf                   = probe->_cubeRenderRTG->createRenderTarget(EBufferFormat::RGBA8);
        colorbuf->_debugName            = "ReflectionProbeColorCubeMap";
        colorbuf->_mipgen               = RtBuffer::EMG_AUTOCOMPUTE;
        probe->_cubeRenderRTG->createDepthBuffer(EBufferFormat::Z32F, true);
      }
      // SKYLIGHT lane C — SH_Radiance probes capture the same way, into an
      // RGBA16F cube (the SH integral wants linear HDR radiance, and nothing
      // resamples it, so no mipchain). The SSBO slot is claimed once and never
      // recycled: slot identity must survive probes going inactive.
      if (probe->_type == LightProbeType::SH_Radiance && nullptr == probe->_cubeRenderRTG) {
        probe->_cubeRenderRTG           = std::make_shared<RtGroup>(_currentContext, 8, 8);
        probe->_cubeRenderRTG->_name    = "SHProbeRTG";
        probe->_cubeRenderRTG->_cubeMap = true;
        auto colorbuf                   = probe->_cubeRenderRTG->createRenderTarget(EBufferFormat::RGBA16F);
        colorbuf->_debugName            = "SHProbeColorCubeMap";
        probe->_cubeRenderRTG->createDepthBuffer(EBufferFormat::Z32F, true);
        if (nullptr == _probeSH)
          _probeSH = std::make_shared<ProbeSHProjector>();
        probe->_shSlot      = _probeSHSlotCounter++;
        probe->_shProjector = _probeSH;
      }
    }
  }

  // Lazy-init blit material for SSAA/TAA
  if (!_probeBlitInitDone) {
    _initProbeBlitMaterial(_currentContext);
  }

  // Phase 2: Render cubemaps only for active + dirty probes.
  // _dynamic probes are always considered dirty (live updates each
  // frame); _dirty alone covers explicit bake triggers + first-time.
  for (auto probe : _enumeratedLights->_lightprobes) {
    switch (probe->_type) {
      case LightProbeType::REFLECTION: {
        if (probe->_dirty || probe->_dynamic) {
          int prevW = probe->_cubeRenderRTG->width();
          int prevH = probe->_cubeRenderRTG->height();
          if (prevW != probe->dim() or prevH != probe->dim()) {
            probe->_cubeRenderRTG->Resize(probe->dim(), probe->dim());
          }

          CompositingPassData cubemapCPD = CPD.clone();
          cubemapCPD._debugName = FormatString("ProbeCubemapPass<%s>", probe->_name.c_str());
          // Restrict the cubemap pass to ONLY the probe's render layer.
          // CPD.clone() inherits every layer active on the outer pass,
          // including HUD/overlay layers (text, debug, etc.) — those
          // should NOT be baked into the reflection cube. assignLayers
          // is a hard set (clears prior layers + set), so the cube
          // captures only world geometry from the probe's chosen layer.
          cubemapCPD.assignLayers(probe->renderLayer());

          if (probe->temporalFrames() > 0) {
            // TAA path (handles SSAA internally if also enabled)
            _renderProbeWithTAA(probe, drawdata, cubemapCPD);
          } else if (probe->supersample() > 0) {
            // SSAA-only path
            _renderProbeWithSSAA(probe, drawdata, cubemapCPD);
          } else {
            // Default path: direct render into cubemap face (unchanged)
            for (int iface = 0; iface < 6; iface++) {
              _currentContext->debugPushGroup(FormatString("ForwardPBR::cubemap pass<%d>", iface));

              _setupCubeFaceCamera(probe, iface);

              auto probe_pass                        = std::make_shared<ForwardPass>();
              probe_pass->_drawdata                  = &drawdata;
              probe_pass->_rtg_out                   = probe->_cubeRenderRTG;
              probe_pass->_rtg_depth_copy            = _rtg_cube1_depth_copy;
              probe_pass->_renderingPROBE            = true;
              probe_pass->_fwd_pass_layer            = probe->renderLayer();
              probe_pass->_single_pass_stereo        = false;
              probe->_cubeRenderRTG->_cubeRenderFace = iface;

              cubemapCPD._mono_cam_matrices = _CUBECAM;
              _currentRCFD->_passID         = "PROBE"_crcu;

              topcomp->pushCPD(cubemapCPD);
              _render_dppskyssaocolor(probe_pass);
              topcomp->popCPD();

              _currentContext->debugPopGroup();
            }

            probe->_cubeTexture = probe->_cubeRenderRTG->texture(0);
            TXI->generateMipMaps(probe->_cubeTexture.get());
            probe->_dirty = false;
          }
        }
        break;
      }
      // SKYLIGHT lane C step 2 — capture, then project onto the L2 SH basis.
      // No SSAA/TAA branch: an L2 projection integrates the whole sphere, so
      // per-face antialiasing cannot move the 9 coefficients meaningfully.
      case LightProbeType::SH_Radiance: {
        // FIRST: project the PREVIOUS capture. A dispatch phase submits+waits its own
        // command buffer immediately, ahead of this frame's graphics submission, so a
        // cube rendered below is not yet on the GPU when a same-call projection would
        // sample it (measured: all-black). One frame of lag, HZB-style.
        if (probe->_shPendingProject and probe->_cubeTexture) {
          OrkAssertI(_probeSH, "SH probe has a pending projection with no projector");
          _probeSH->project(
              _currentContext, probe->_cubeTexture, probe->_cubeRenderRTG->width(), probe->_shSlot);
          probe->_shPendingProject = false;
        }
        if (probe->_dirty || probe->_dynamic) {
          int shdim = probe->dim() > 0 ? probe->dim() : kProbeSHDefaultDim;
          if (probe->_cubeRenderRTG->width() != shdim or probe->_cubeRenderRTG->height() != shdim) {
            probe->_cubeRenderRTG->Resize(shdim, shdim);
          }

          CompositingPassData cubemapCPD = CPD.clone();
          cubemapCPD._debugName          = FormatString("ProbeSHPass<%s>", probe->_name.c_str());
          cubemapCPD.assignLayers(probe->renderLayer());

          for (int iface = 0; iface < 6; iface++) {
            _currentContext->debugPushGroup(FormatString("ForwardPBR::SH cubemap pass<%d>", iface));

            _setupCubeFaceCamera(probe, iface);

            auto probe_pass                        = std::make_shared<ForwardPass>();
            probe_pass->_drawdata                  = &drawdata;
            probe_pass->_rtg_out                   = probe->_cubeRenderRTG;
            probe_pass->_rtg_depth_copy            = _rtg_cube1_depth_copy;
            probe_pass->_renderingPROBE            = true;
            probe_pass->_fwd_pass_layer            = probe->renderLayer();
            probe_pass->_single_pass_stereo        = false;
            probe->_cubeRenderRTG->_cubeRenderFace = iface;

            cubemapCPD._mono_cam_matrices = _CUBECAM;
            _currentRCFD->_passID         = "PROBE"_crcu;

            topcomp->pushCPD(cubemapCPD);
            _render_dppskyssaocolor(probe_pass);
            topcomp->popCPD();

            _currentContext->debugPopGroup();
          }

          probe->_cubeTexture = probe->_cubeRenderRTG->texture(0);
          OrkAssertI(_probeSH, "SH probe reached capture with no projector — phase 1 allocation was skipped");
          probe->_shPendingProject = true;
          probe->_dirty            = false;
        }
        break;
      }
      default:
        break;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_update_shadow_maps() {
  if (_enumeratedLights) {
    auto topcomp                  = _currentRCFD->topCompositor();
    _currentRCFD->_renderingmodel = "DEPTH_PREPASS"_crcu;
    _currentRCFD->_passID         = "SHADOW"_crcu;
    int num_shadow_casters        = 0;
    static int _shadow_dbg_frame = 0;
    for (auto light : _enumeratedLights->_alllights) {
      if (not light->_castsShadows)
        continue;

      if (auto as_spotlight = dynamic_cast<SpotLight*>(light)) {
        _currentContext->debugPushGroup(FormatString("ForwardPBR::_update_shadow_maps spot<%p>", (void*) light));

        if (light->_depthRTG == nullptr) {
          auto depcookie = light->_cookieDepth;
          OrkAssert(depcookie);
          auto rtg         = depcookie->createRenderTarget(_currentContext);
          rtg->_depthOnly  = true;
          light->_depthRTG = rtg;
        }
        
        CompositingPassData shadowCPD = _currentCIMPL->topCPD().clone();
        shadowCPD._debugName = "ShadowMapPass";

        // The prologue's CPD carries NO layer set — and enqueueLayerToRenderQueue
        // only admits layers present on the ACTIVE CPD (HasLayer gate). Assign the
        // depth_prepass role layers here and push the CPD BEFORE enqueueing, or
        // the shadow pass renders nothing (D4). A drawable spot-shadows only if it
        // plays the depth_prepass role (same contract as sun cascades).
        std::string dpp_layer_csv;
        {
          auto* scene = _node->_pbrcommon ? _node->_pbrcommon->_scene : nullptr;
          if (scene) {
            for (const auto& layer_name : scene->layersForRole("depth_prepass")) {
              if (!dpp_layer_csv.empty())
                dpp_layer_csv += ",";
              dpp_layer_csv += layer_name;
            }
          } else {
            dpp_layer_csv = "depth_prepass";
          }
        }
        shadowCPD.assignLayers(dpp_layer_csv);

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

        auto FBI = _currentContext->FBI();
        FBI->PushRtGroup(light->_depthRTG.get());
        topcomp->pushCPD(shadowCPD);

        // enqueue AFTER the push — the HasLayer gate reads the active CPD
        {
          auto* scene = _node->_pbrcommon ? _node->_pbrcommon->_scene : nullptr;
          if (scene) {
            for (const auto& layer_name : scene->layersForRole("depth_prepass")) {
              _currentDrawQueue->enqueueLayerToRenderQueue(layer_name, _currentIRenderer);
            }
          } else {
            _currentDrawQueue->enqueueLayerToRenderQueue("depth_prepass", _currentIRenderer);
          }
        }

        _currentIRenderer->drawEnqueuedRenderables(true);
        topcomp->popCPD();

        FBI->PopRtGroup();

        _currentContext->debugPopGroup();
      }
      num_shadow_casters++;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SKYLIGHT lane A — sun state + cascaded shadow maps, once per composited frame
//  (prologue). Writes the ublk_sun UBO (has_sun=0 when no directional light —
//  the shader's no-op branch keeps sunless scenes byte-identical), then fits
//  WORLD-ANCHORED ortho cascades — nested world-space bands centered on the
//  viewer POSITION, geometric radii, each snapped to its own (constant) world
//  texel grid, MANDATORY texel snapping — VR shimmer law — and renders each
//  band with the existing DEPTH_PREPASS machinery.
//
//  VIEW INDEPENDENCE IS THE LAW HERE (W7): nothing in the fit, and nothing in
//  the refresh gate, may read the camera's ORIENTATION. A band is a sphere
//  around where the viewer stands, so turning in place cannot invalidate it and
//  cannot change one texel footprint. The cost of covering 360 degrees instead
//  of a view wedge is ~2x the texels for a given sharpness — accepted, and paid
//  back by the resolution work that follows.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// ublk_sun std140 offsets — hand-mirrored with the block in stdtools.i2, and
// mirrored HERE ONLY: the fit writes them at snapshot publish, the prologue
// writes the live sun fields every frame, and both must agree with one table.
////////////////////////////////////////

namespace {
// PER-BAND SCALARS PAST BAND 3. A vec4 carries four of them and the ladder now
// reserves five, so each per-band scalar table is a PAIR of vec4s: the base one
// (bands 0..3, every offset it ever had relative to its neighbours) plus a _hi
// one whose .x is band 4. The alternative — a std140 float[5] — pads every
// element to 16 bytes (80 bytes for 5 floats) and makes the shader's dynamic
// index a stride-4 walk, so the pair is both smaller and simpler.
constexpr size_t k_off_shmtx  = 0;   // mat4 sun_shadow_matrix[kSunCascadeStorage]
constexpr size_t k_off_splits = 320; // vec4 sun_split_distances (bands 0..3 SELECT radii, world meters)
constexpr size_t k_off_splits_hi = 336; // vec4 sun_split_distances_hi (x = band 4)
constexpr size_t k_off_dir    = 352; // vec4 sun_dir (xyz dir, w has_sun)
constexpr size_t k_off_color  = 368; // vec4 sun_color (rgb, w intensity)
constexpr size_t k_off_params = 384; // vec4 sun_shadow_params (bias METRES, pcf, count, texel)
constexpr size_t k_off_basis  = 400; // vec4 sun_cascade_basis (xyz = band anchor)
constexpr size_t k_off_texel  = 416; // vec4 sun_cascade_texel (bands 0..3 1/dim — S2b took the slot
                                     //  the old sun_cascade_basis[1] reserved, and a uniform-dim rig
                                     //  writes 1/map_dim four times)
constexpr size_t k_off_texel_hi = 432; // vec4 sun_cascade_texel_hi (x = band 4's 1/dim)
constexpr size_t k_off_ckmtx  = 448; // mat4 sun_cookie_matrix (world -> cookie uv)
constexpr size_t k_off_ckpar  = 512; // vec4 sun_cookie_params (enable, strength, lod, EXTINCTION tau)
// S2a snapshot-flip crossfade — APPENDED (every offset above unchanged, so a
// consumer that never reads the fade is untouched by this growth). The mat4
// ARRAY is last on purpose — see the member-order note in stdtools.i2: the
// offset following a mat4[] is rounded to 64 by the block reflection but not by
// plain std140, and the two must not disagree about any member.
constexpr size_t k_off_psplits = 528; // vec4 sun_split_distances_prev
constexpr size_t k_off_psplits_hi = 544; // vec4 sun_split_distances_prev_hi (x = band 4)
constexpr size_t k_off_pbasis  = 560; // vec4 sun_cascade_basis_prev (xyz = its band anchor)
constexpr size_t k_off_fade    = 576; // vec4 sun_shadow_fade (weight, cur slice base, prev slice base, -)
constexpr size_t k_off_skyamb  = 592; // vec4 sky_ambient (x = measured background luminance) — inserted BEFORE the trailing mat4[] per the member-order law (merge of the exposure split across S2b)
constexpr size_t k_off_iblw    = 608; // vec4 sun_shadow_ibl_weights (x = cloud->IBL, y = cascade->IBL) — also
                                      //  BEFORE the trailing mat4[] per the member-order law
constexpr size_t k_off_pshmtx  = 624; // mat4 sun_shadow_matrix_prev[kSunCascadeStorage] (the OUTGOING snapshot's fit)
constexpr size_t k_ubo_size    = 960; // reflected block size (the array's trailing pad included)
// The block must FIT the allocation. A block that outgrew it mapped short and
// dropped its tail fields with no error at all (S2a: the crossfade's prev
// matrices simply never arrived), so growing this table past the buffer is a
// BUILD failure, not a rendering mystery — raise kSunDataBufferBytes with it.
static_assert(
    k_ubo_size <= PBRMaterial::kSunDataBufferBytes,
    "ublk_sun host layout exceeds PBRMaterial::sunDataBuffer's allocation — "
    "the tail fields would be silently truncated");
// ...and BOTH mat4 arrays are sized by the storage ceiling, so raising that
// constant without re-walking this table is also a build failure rather than a
// tail nobody notices is short.
static_assert(
    k_off_splits == LightManager::kSunCascadeStorage * sizeof(fmtx4),
    "ublk_sun: sun_shadow_matrix[] no longer ends where sun_split_distances begins");
static_assert(
    (k_off_pshmtx + LightManager::kSunCascadeStorage * sizeof(fmtx4)) <= k_ubo_size,
    "ublk_sun: sun_shadow_matrix_prev[] runs past the declared block size");

// ORKID_SUNSNAP_TRACE wall clock — seconds since the first traced event, so a
// cadence can be READ off the trace instead of inferred from frame numbers
// (frames are not a clock: an offscreen loop runs at whatever rate it runs at).
double _sunsnap_trace_now() {
  using clk         = std::chrono::steady_clock;
  static const auto t0 = clk::now();
  return std::chrono::duration<double>(clk::now() - t0).count();
}
} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The OUTGOING snapshot's blend weight: 1 on the flip frame, 0 when the window
// is over. Two ramps, one law — a fade is a DURATION. With a wall-clock length
// declared the ramp is the clock's, so the blend covers the same span of time
// at 30 fps and at 1300; with none it falls back to the frame count exactly as
// the frames-only path shipped.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float ForwardPbrNodeImpl::_sun_fade_weight() const {
  if (_sun_fade_total <= 0)
    return 0.0f;
  float progress = (_sun_fade_secs > 0.0f)
                 ? float(_sun_fade_timer.SecsSinceStart() / double(_sun_fade_secs))
                 : (1.0f - (float(_sun_fade_remaining) / float(_sun_fade_total)));
  return std::clamp(1.0f - progress, 0.0f, 1.0f);
}

void ForwardPbrNodeImpl::_update_sun_cascades() {
  if (nullptr == _enumeratedLights)
    return;
  auto lmgr = _currentCIMPL->lightManager();
  if (nullptr == lmgr)
    return;
  auto context = _currentContext;
  auto FXI     = context->FXI();

  ////////////////////////////////////////
  // sun selection — highest-priority active shadow-casting directional wins.
  //  _directionallights is sorted DESCENDING by LightData Priority in
  //  LightManager::enumerateInPass, so "first" here means "highest rank" and
  //  index [0] is the reconciled fallback (same law the sky source uses).
  ////////////////////////////////////////

  DirectionalLight* sun = nullptr;
  int num_shadow_suns   = 0;
  for (auto dl : _enumeratedLights->_directionallights) {
    if (dl->_castsShadows) {
      num_shadow_suns++;
      if (nullptr == sun)
        sun = dl;
    }
  }
  static int s_last_sun_count = -1;
  if (num_shadow_suns != s_last_sun_count) {
    s_last_sun_count = num_shadow_suns;
    if (num_shadow_suns > 1)
      printf(
          "[FWD:SUN] WARNING: %d shadow-casting DirectionalLights active — only ONE sun "
          "is supported, first wins\n",
          num_shadow_suns);
  }
  bool sun_shadowed = (sun != nullptr);
  if (nullptr == sun and _enumeratedLights->_directionallights.size())
    sun = _enumeratedLights->_directionallights[0]; // lit but unshadowed (cascade_count=0)

  ////////////////////////////////////////
  // ublk_sun write (std140 offsets — the k_off_* table above)
  ////////////////////////////////////////

  auto sun_buffer = PBRMaterial::sunDataBuffer(context);
  auto mapped     = FXI->mapUniformBuffer(sun_buffer, 0, k_ubo_size);

  // Tunables come off the sun's DirectionalLightData (reflected — A8); a scene
  // between suns reads the defaults rather than the last sun's numbers.
  static const DirectionalLightData s_default_dldata;

  // HOW MUCH OF EACH SHADOW FACTOR REACHES THE AMBIENT/IBL TERM — a cloud
  // occludes sky, a tree occludes the disc (see DirectionalLightData). Written
  // on every path for the same reason the cookie params are: a stale weight
  // over a disarmed cookie is a shadow the frame does not have.
  {
    const DirectionalLightData* wd  = (sun and sun->_dldata) ? sun->_dldata : &s_default_dldata;
    mapped->ref<fvec4>(k_off_iblw) = fvec4(
        std::clamp(wd->_cloudShadowIblWeight, 0.0f, 1.0f),   //
        std::clamp(wd->_cascadeShadowIblWeight, 0.0f, 1.0f), //
        std::clamp(wd->_cascadeShadowFloor, 0.0f, 1.0f),     // .z — direct-term shadow floor
        0.0f);
  }

  // SUN COOKIE (cloud shadows) — published on the LightManager by whoever owns
  // the deck. Written on EVERY path including the sunless early-out: the cookie
  // params must never be stale, or a scene that drops its sun keeps a shadow.
  bool cookie_armed = lmgr->_sun_cookie and (lmgr->_sun_cookie_strength > 0.0f)
                      and (lmgr->_sun_cookie != lmgr->_sun_cookie_default);
  mapped->ref<fmtx4>(k_off_ckmtx) = cookie_armed ? lmgr->_sun_cookie_matrix : fmtx4();
  mapped->ref<fvec4>(k_off_ckpar) = cookie_armed
                                  ? fvec4(1.0f,
                                          lmgr->_sun_cookie_strength,
                                          lmgr->_sun_cookie_lod,
                                          lmgr->_sun_cookie_extinction)
                                  : fvec4(0, 0, 0, 0);

  // SKY BACKGROUND LUMINANCE — written here, above every early-out, for the
  // same reason the cookie is: a consumer that reads a stale value sees a sky
  // brightness the frame does not have. Negative (nothing published yet) is
  // clamped to zero — an unmeasured sky is the darkest thing a contrast test
  // can be asked about, which is the reading that keeps stars visible rather
  // than blanking them on a warm-up frame.
  {
    auto pbrc     = _node->_pbrcommon;
    float sky_lum = pbrc ? pbrc->availableLightLuminance() : 0.0f;
    mapped->ref<fvec4>(k_off_skyamb) = fvec4(std::max(sky_lum, 0.0f), 0, 0, 0);
  }

  if (nullptr == sun) {
    mapped->ref<fvec4>(k_off_dir) = fvec4(0, -1, 0, 0); // has_sun = 0
    mapped->unmap();
    _sun_job._active     = false; // an in-flight snapshot has no caster left to be fit for
    _sun_fade_remaining  = 0;
    _sun_snap_valid      = false; // nothing is published any more — see the note below
    return;
  }

  const DirectionalLightData* dldata = sun->_dldata ? sun->_dldata : &s_default_dldata;

  // BAND COUNT IS AUTHORED, NOT NEGOTIATED. A count past the storage ceiling
  // used to CLAMP, which silently handed a scene asking for more coverage the
  // ladder it already had — the fifth band would simply not exist and the
  // missing far shadow would read as a shader bug. Say so instead.
  OrkAssertI(
      dldata->_shadowCascadeCount <= LightManager::kSunCascadeStorage,
      "sun cascade count exceeds LightManager::kSunCascadeStorage — author fewer bands "
      "or raise the storage ceiling (and the ublk_sun layout table with it)");
  int cascade_count  = std::max(dldata->_shadowCascadeCount, 2);
  int map_dim        = std::max(dldata->shadowMapSize(), 128);
  float max_dist     = std::max(dldata->_shadowMaxDistance, 1.0f);
  float band_radius0 = std::max(dldata->_shadowBandRadius, 0.5f);
  float band_ratio   = std::max(dldata->_shadowBandRatio, 1.25f);

  fvec3 sun_dir = sun->direction().normalized();

  // log the first few direction changes (capped: an orbiting sun would spam) —
  // a sun stuck at a default direction, or pointing UP (y > 0, below-horizon),
  // is THE symptom of the entity-orientation aim path breaking upstream
  // (see elevation_azimuth_quat's conjugate-trap note).
  static fvec3 s_last_logged_dir(0, 0, 0);
  static int s_dir_logs_left = 8;
  if (s_dir_logs_left > 0 and (sun_dir - s_last_logged_dir).magnitude() > 0.05f) {
    s_last_logged_dir = sun_dir;
    s_dir_logs_left--;
    printf("[FWD:SUN] direction<%g %g %g>\n", sun_dir.x, sun_dir.y, sun_dir.z);
  }

  mapped->ref<fvec4>(k_off_dir)   = fvec4(sun_dir, 1.0f);
  mapped->ref<fvec4>(k_off_color) = fvec4(sun->color(), sun->intensity());

  if (not sun_shadowed) {
    // unshadowed sun: light contribution only, cascade_count=0 → shadow factor 1
    mapped->ref<fvec4>(k_off_params) = fvec4(0, 0, 0, 0);
    mapped->unmap();
    _sun_job._active    = false;
    _sun_fade_remaining = 0;
    // ...AND THE HOLD IS BROKEN. This branch WIPES the shader-facing shadow
    // params (cascade count 0), which only a snapshot PUBLISH ever writes back.
    // Every-frame refits hid that: the frame after a caster re-arms republished
    // them. Under any cadence — declared interval or refresh gate — the maps
    // would be sampled with a count of zero, i.e. NO SHADOW, until the next
    // tick. So a disarm invalidates the held snapshot outright and the re-arm
    // reads as structural. (Measured: the shadow-attribution gate, which toggles
    // shadowCaster between captures, came back with zero shadow pixels.)
    _sun_snap_valid     = false;
    return;
  }

  ////////////////////////////////////////
  // band anchor = the viewer POSITION (mono prologue camera). POSITION ONLY —
  // the camera's orientation is not read here and must not be: the bands are
  // world spheres about this point, and the shader selects a band by distance
  // from it. Read here but written NOWHERE above the snapshot gate (the
  // k_off_basis write that must travel with the held matrices stays below it —
  // a live anchor over held matrices would select bands the maps were never
  // fit for).
  ////////////////////////////////////////

  fvec3 cam_eye = _currentViewData._camposmono;

  ////////////////////////////////////////
  // SNAPSHOT INTERVAL — _shadowSnapshotInterval seconds between cascade
  // snapshots (0 = every frame = shipped behavior). Everything below this gate
  // is ONE held unit: band fit, cascade anchor, union cull box and the
  // per-band depth passes. Fresh matrices over stale depth (or the reverse) is
  // swimming shadows, and a cull box refreshed on its own culls the held maps
  // against an anchor that has moved. The sun's LIVE state — cookie matrix,
  // sun_dir, sun_color — is written above and never held: a held SHADOW is the
  // trade, a stuck SUN is a bug.
  //
  // A hold needs no extra storage. ublk_sun is ONE persistent host-mapped
  // allocation (PBRMaterial::sunDataBuffer), not a per-frame ring, so fields
  // nobody writes keep last tick's bytes; _sun_cascade_rtgs are reallocated
  // only on a dimension change and GPU RT contents survive a frame that draws
  // nothing into them.
  //
  // THE INTERVAL IS THE CADENCE — neither a ceiling nor a floor, just the
  // update period (owner ruling, 2026-07-30). A scene that declares 60 seconds
  // gets a refit every 60 seconds and not one more, however fast the sun sweeps
  // or the viewer walks: drift triggers made the declared number advisory, and
  // a scene asking for a slow shadow got a ~1 Hz one instead. Nothing here may
  // grow a "but the premises aged" test again — a premise the author decided is
  // allowed to age for a minute is not a defect, it is the declaration.
  //
  // FORCED REFRESH — the STRUCTURAL cases only, where there is no valid
  // snapshot to hold at all:
  //  * nothing held yet (first-ever render);
  //  * CASTER FLIP (sun->moon): a fit made for the old caster's direction is
  //    geometrically wrong for the new one. The caster scan runs every frame
  //    above, precisely so this is seen on the flip frame;
  //  * RIG CHANGE (map dims, band count/radius/ratio, extrusion depth, bias,
  //    pcf): ensureSunCascades and the shadow-params write both live below, so
  //    an interval-deferred resize would sample maps at dimensions nothing
  //    rendered into.
  ////////////////////////////////////////

  ////////////////////////////////////////
  // SNAPSHOT GRANULARITY (S2a) — how many bands ONE frame may draw, and how
  // long a flip is blended for. Both resolved HERE, at the single site that
  // starts a job, mirroring the IBL granularity properties: 0 = unset = the
  // shipped behavior (whole snapshot in one frame, hard swap), which is also
  // what keeps a scene that declares neither byte-identical to before.
  //
  // The double buffer is allocated only when one of them actually needs the
  // live maps to survive a snapshot render — amortizing INTO the live set would
  // sample half-drawn bands, and a crossfade needs both sets resident.
  ////////////////////////////////////////

  // ...and the fade's LENGTH is a duration, not a frame count. Frames are not a
  // clock: the same 12 declared here are 24 ms in an unthrottled offscreen loop
  // and 200 ms at 60 Hz, so the fraction of wall time the scene spends blending
  // — the cost AND the visible smoothing — was a function of the frame rate.
  // The sky IBL feed hit this first (SkyAtmosphereData::_iblCrossfadeMaxSecs);
  // this is the same clock on the sun's flip. 0 = ride the frame count, which
  // is the pre-clock behavior exactly.
  int fade_frames = std::max(dldata->_shadowCrossfadeFrames, 0);
  float fade_secs = std::max(dldata->_shadowCrossfadeSecs, 0.0f);
  int bands_knob  = std::clamp(dldata->_shadowSnapshotBandsPerFrame, 0, LightManager::kSunCascadeStorage);
  int bands_per_frame = (bands_knob > 0) ? bands_knob : cascade_count;
  int want_sets       = ((bands_per_frame < cascade_count) or (fade_frames > 0)) ? 2 : 1;

  float snap_interval = std::max(dldata->_shadowSnapshotInterval, 0.0f);

  ////////////////////////////////////////
  // PER-BAND RESOLUTION + PER-SNAPSHOT JITTER (S2b) — resolved at the same
  // single site, same law: 1 / 0 is the shipped rig exactly.
  //
  // The jitter is GATED ON THE CROSSFADE. Its only mechanism is that two
  // successive snapshots sample the penumbra at different sub-texel offsets and
  // the fade blends them; with a hard swap the offset is not supersampling, it
  // is a fit that moves under the shadow on every flip — per-snapshot crawl,
  // the same artifact class that got per-fragment kernel rotation deleted from
  // the shader. So no fade window, no jitter, and the knob cannot be armed
  // into the regression by accident.
  ////////////////////////////////////////
  float band_res_ratio = std::clamp(dldata->_shadowBandResRatio, 1.0f, 8.0f);
  float jitter_amp     = (fade_frames > 0) ? std::clamp(dldata->_shadowJitterTexels, 0.0f, 1.0f) : 0.0f;

  // CULLSETS — the authored rig, as authored. Compared as raw text every frame
  // (cheap); PARSED only when a snapshot actually starts, which is where a
  // mis-spelled family or a mis-counted band list gets to fail loud.
  std::string cullset_rig = dldata->_shadowCullSets + "|" + dldata->_shadowBandCullSets;

  bool caster_changed = (sun != _sun_snap_caster) or (dldata->_skyBody != _sun_snap_body);
  bool rig_changed    = (map_dim != _sun_snap_map_dim)                     //
                     or (cascade_count != _sun_snap_cascades)              //
                     or (max_dist != _sun_snap_max_dist)                   //
                     or (band_radius0 != _sun_snap_band_radius)            //
                     or (band_ratio != _sun_snap_band_ratio)               //
                     or (dldata->GetShadowBias() != _sun_snap_bias)        //
                     or (dldata->_pcfDither != _sun_snap_pcf)              //
                     or (band_res_ratio != _sun_snap_res_ratio)            //
                     or (cullset_rig != _sun_snap_cullsets)                //
                     or (want_sets != _sun_snap_sets);
  // ...and a per-band DIM change is one of the few rig edits the outgoing
  // snapshot cannot be faded from even though the array survives it: the fade's
  // prev half would be sampled with the NEW band's uv scale over depth drawn at
  // the OLD viewport size, i.e. half a band's worth of shift.
  bool band_dims_changed = (band_res_ratio != _sun_snap_res_ratio);

  // STRUCTURAL invalidation — the storage or the geometry the in-flight
  // snapshot is being drawn into is wrong, so it restarts on the spot and
  // renders WHOLE this frame: there is nothing valid left to keep sampling
  // while it amortizes (first-ever included — the maps hold no snapshot at all
  // and an amortized first fill would show unshadowed frames).
  bool structural = (not _sun_snap_valid) or caster_changed or rig_changed;
  // CADENCE — the declared period, and nothing else. It STARTS the next
  // snapshot; it does not restart one in flight (at interval 0 an
  // unconditional restart would re-premise every frame and never publish a
  // band set), so the extra lag is bounded by the job's own length and the
  // "never two in flight" rule holds.
  //
  // ...and it WAITS FOR THE FADE TO SETTLE, the same condition the sky IBL feed
  // conjoins onto its own declared interval. A cadence start landing inside a
  // fade window can only be resolved by retiring the fade, i.e. by snapping the
  // blend weight to 0 — the pop the fade exists to hide, delivered on a
  // schedule. The cost is that the achieved cadence is FLOORED at snapshot +
  // fade; when the declaration is under that floor the engine says so once
  // rather than silently running faster than it can blend.
  double held_secs  = _sun_snap_timer.SecsSinceStart();
  bool fade_settled = (_sun_fade_remaining <= 0);
  bool elapsed      = true;
  if (snap_interval > 0.0f) {
    elapsed = (held_secs >= double(snap_interval));
  } else {
    ////////////////////////////////////////
    // REFRESH GATE — the UNDECLARED cadence. "No interval" used to mean "refit
    // every frame", which redraws four identical depth passes for a still
    // viewer under a still sun. The gate asks instead whether either PREMISE of
    // the held fit has actually moved: the anchor the world-spheres are
    // centered on, and the direction the light travels. Nothing else may enter
    // it — camera ORIENTATION in particular is what a band fit is deliberately
    // independent of (W7), and reading it here would hand that property back.
    //
    // The CEILING is not a staleness heuristic on the fit (the fit cannot go
    // stale under premises that did not move); it is the CASTERS' clock, and it
    // is OFF by default. Wind moves leaves and a walker moves a body without
    // touching any premise above, but paying a full fit + cull + four depth
    // passes four times a second to chase that (measured 1.9 ms on a PAUSED
    // frame) is a worse trade than the frozen shimmer it buys (owner ruling,
    // 2026-08-05). A scene that wants the shimmer declares a rate.
    //
    // ITS OFF VALUE IS 0 ONLY WHILE ANOTHER THRESHOLD IS ARMED: all three at 0
    // is the disarmed gate above (refit every frame), so the ceiling cannot be
    // the last one standing.
    //
    // A DECLARED interval takes the whole branch away: the declaration is the
    // cadence, and a premise-drift trigger under it is exactly the "advisory
    // number" the interval ruling forbids.
    ////////////////////////////////////////
    float ang_thresh  = std::max(dldata->_shadowRefreshAngleDeg, 0.0f);
    float dist_thresh = std::max(dldata->_shadowRefreshDistance, 0.0f);
    float max_secs    = std::max(dldata->_shadowRefreshMaxSecs, 0.0f);
    bool gate_armed   = (ang_thresh > 0.0f) or (dist_thresh > 0.0f) or (max_secs > 0.0f);
    if (gate_armed) {
      // THE CASTER SET IS A PREMISE TOO. Neither the anchor nor the light
      // direction moves when content ARRIVES, and the very first snapshot of a
      // run is taken on the frame the scene is still assembling — so a gate that
      // read only the two geometric premises published an EMPTY cascade and held
      // it until the ceiling. (Measured: the shadow-attribution gate captured
      // zero shadow pixels; every-frame refits had hidden it by redrawing on the
      // next frame.) Counting the drawables in the depth-prepass role — the
      // caster set itself, the same walk the cookie's self-defense makes — is
      // what makes streamed-in geometry cast on the frame it lands. It is a
      // refresh TRIGGER, not a structural one: an in-flight amortized snapshot
      // must not restart every time a chunk streams in.
      size_t caster_nodes = 0;
      if (auto* dpp_scene = _node->_pbrcommon ? _node->_pbrcommon->_scene : nullptr) {
        for (const auto& layer_name : dpp_scene->layersForRole("depth_prepass")) {
          if (auto layer = dpp_scene->tryFindLayer(layer_name))
            layer->_drawable_nodes.atomicOp([&](const scenegraph::Layer::drawablenodevect_t& nodes) { //
              caster_nodes += nodes.size();
            });
        }
      }
      bool casters_changed = (caster_nodes != _sun_snap_casters);
      _sun_snap_casters    = caster_nodes;
      // cos of the travel angle, straight off two unit vectors — no acos on the
      // per-frame path; the threshold is converted the other way once.
      float cos_moved = std::clamp(sun_dir.dotWith(_sun_snap_sundir), -1.0f, 1.0f);
      float cos_gate  = cosf(ang_thresh * float(DTOR));
      bool sun_moved  = (ang_thresh > 0.0f) and (cos_moved < cos_gate);
      bool eye_moved  = (dist_thresh > 0.0f) and ((cam_eye - _sun_snap_anchor).magnitude() > dist_thresh);
      bool ceiling    = (max_secs > 0.0f) and (held_secs >= double(max_secs));
      // WARM-UP. A drawable node exists several frames before it DRAWS — its
      // pipeline compiles, its model loads, its buffers realize — so the first
      // snapshots of a run are fit for a scene that is not on the GPU yet, and
      // the caster COUNT cannot see that (the nodes were all there at frame 0).
      // Measured: the shadow-attribution gate's first snapshot drew empty maps
      // and the hold kept them for a quarter second, i.e. for its whole capture
      // sequence. So the gate does not engage until the compositor has run a
      // scene for k_gate_warm_frames — the every-frame path it replaces, for
      // exactly as long as a scene takes to become drawable.
      constexpr int k_gate_warm_frames = 60;
      bool warming    = (_sun_gate_frames < k_gate_warm_frames);
      _sun_gate_frames++;
      elapsed         = warming or sun_moved or eye_moved or ceiling or casters_changed;
    }
  }
  bool start_job = structural or (elapsed and fade_settled and not _sun_job._active);

  if ((snap_interval > 0.0f) and elapsed and (not fade_settled) and (not _sun_cadence_floor_warned)) {
    _sun_cadence_floor_warned = true;
    logchan_fwd_sub->log(
        "sun cascade declared snapshot interval %.3f s is BELOW the snapshot+fade floor "
        "(%d bands at %d per frame, %d fade frames / %.3f s) - the cadence runs at the FLOOR, "
        "not the declaration. Raise the interval or shorten the crossfade.",
        double(snap_interval), cascade_count, bands_per_frame, fade_frames, double(fade_secs));
  }

  if (start_job and getenv("ORKID_SUNSNAP_TRACE")) {
    printf("[sunsnap] frame<%d> t<%.3f> START held<%.3f> interval<%g> structural<%d> caster<%d> rig<%d>\n",
           context->GetTargetFrame(), _sunsnap_trace_now(), held_secs, snap_interval,
           int(structural), int(caster_changed), int(rig_changed));
    fflush(stdout);
  }

  ////////////////////////////////////////
  // CROSSFADE TICK — one frame of the fade window. Only a STRUCTURAL start can
  // land here (the cadence start waits for the settle above), and that one
  // still retires the fade rather than advancing it: the new snapshot renders
  // into the very set the fade is still sampling, and blending against a set
  // being repainted is worse than the pop.
  ////////////////////////////////////////
  if (_sun_fade_remaining > 0) {
    if (start_job) {
      _sun_fade_remaining = 0;
      _sun_fade_total     = 0;
    } else {
      _sun_fade_remaining--;
      // CLOCK-RAMPED window: the frame counter only keeps the fade ARMED, the
      // clock decides when it is over. Held at 1 until the budget lands so a
      // fast loop cannot expire the window in a few milliseconds, and dropped
      // to 0 the moment it does so a slow one cannot outrun it.
      if (fade_secs > 0.0f)
        _sun_fade_remaining = (_sun_fade_weight() > 0.0f) ? std::max(_sun_fade_remaining, 1) : 0;
    }
    mapped->ref<fvec4>(k_off_fade) = fvec4(
        _sun_fade_weight(),
        float(_sun_live_set * lmgr->_sun_cascade_bands),
        float(_sun_fade_prev_set * lmgr->_sun_cascade_bands),
        0.0f);
    if (getenv("ORKID_SUNSNAP_TRACE")) {
      printf("[sunsnap] frame<%d> t<%.3f> FADE w<%.4f> rem<%d> total<%d> secs<%g>\n",
             context->GetTargetFrame(), _sunsnap_trace_now(), _sun_fade_weight(),
             _sun_fade_remaining, _sun_fade_total, double(_sun_fade_secs));
      fflush(stdout);
    }
  }

  mapped->unmap();

  if (not start_job and not _sun_job._active)
    return; // held: the cascade matrices, splits, anchor and params keep last tick's bytes

  // cadence witness (perf HUD / ORKID_PLAYER_HUD_STDOUT): rows are cleared every
  // frame, so the mere PRESENCE of this one is an exact per-frame record of
  // whether snapshot WORK ran. An amortized snapshot spends several consecutive
  // frames here — that is the honest reading, one row per frame that drew.
  RenderPhaseScope _sun_cascade_fit_scope("sun-cascade-fit");

  if (not start_job) {
    // an amortized snapshot is in flight: this frame draws its next batch of
    // bands and nothing else. No fit, no cull — both are FROZEN premises of the
    // job that started, and re-deriving either here is what would publish a
    // snapshot whose bands disagree about where the viewer stands.
    _render_sun_snapshot_bands(bands_per_frame, fade_frames, fade_secs);
    return;
  }

  ////////////////////////////////////////
  // BAND SCHEME — nested world spheres about the anchor, geometric radii:
  //  fit_radius[i] = band_radius0 * ratio^i, the declared numbers exactly. No
  //  frustum, no near/far plane, no split lambda: the camera's projection
  //  cannot reach the fit at all, which is exactly what makes a turn free.
  ////////////////////////////////////////

  float fit_radius[LightManager::kSunCascadeStorage];
  {
    float r = band_radius0;
    for (int i = 0; i < LightManager::kSunCascadeStorage; i++) {
      fit_radius[i] = r;
      r *= band_ratio;
    }
  }

  ////////////////////////////////////////
  // PER-BAND DIMS. One array allocation at map_dim (the NEAR band's, the
  // sharpest); band i draws into the top-left dim[i]² sub-rect of its own slice
  // and the shader scales that band's uv by dim[i]/map_dim. Floored at
  // k_band_dim_floor — below that a band's own PCF kernel is a large fraction
  // of the map and the softening reads as a blur, not a shadow.
  ////////////////////////////////////////
  constexpr int k_band_dim_floor = 256;
  int band_dim[LightManager::kSunCascadeStorage];
  {
    float d = float(map_dim);
    for (int i = 0; i < LightManager::kSunCascadeStorage; i++) {
      band_dim[i] = std::clamp(int(d + 0.5f), std::min(k_band_dim_floor, map_dim), map_dim);
      d /= band_res_ratio;
    }
  }

  ////////////////////////////////////////
  // PER-SNAPSHOT JITTER — one sub-texel offset pair for the whole snapshot,
  // from the R2 low-discrepancy sequence (Roberts): consecutive samples are
  // maximally spread, so a short fade chain still covers the texel evenly
  // instead of clustering the way a random pair does. In TEXELS, applied to the
  // SNAPPED light-space origin below — the texel SIZE is untouched, which is
  // what keeps S1's constant-texel invariant (and therefore the whole
  // no-shimmer argument) intact.
  ////////////////////////////////////////
  float jitter_x = 0.0f;
  float jitter_y = 0.0f;
  if (jitter_amp > 0.0f) {
    constexpr float k_r2_a1 = 0.7548776662466927f; // 1/g, g = plastic number
    constexpr float k_r2_a2 = 0.5698402909980532f; // 1/g^2
    float n  = float(_sun_snap_jitter_index);
    jitter_x = (fmodf(0.5f + k_r2_a1 * n, 1.0f) - 0.5f) * jitter_amp;
    jitter_y = (fmodf(0.5f + k_r2_a2 * n, 1.0f) - 0.5f) * jitter_amp;
  }
  _sun_snap_jitter_index++;

  // The fit lands in the JOB, not in the UBO: the shader-facing matrices,
  // splits, anchor and params are published together at COMPLETION, when the
  // depth they describe has actually been drawn.
  _sun_job._anchor = cam_eye;

  ////////////////////////////////////////
  // per-band ortho fit: the band sphere, centered on the anchor, its center
  //  texel-snapped in light space (translation stability — no shimmer under
  //  viewer motion). TEXEL INVARIANT: texel_ws = 2*fit_radius/map_dim is a
  //  function of tunables alone, so no refit can change a band's world texel
  //  footprint; any change to that pair is a RIG CHANGE and refits every band.
  ////////////////////////////////////////

  // sin(sun elevation): sun_dir TRAVELS downward for a sun above the horizon, so
  // its negated y IS the sine. FLOORED — the toward-light extrusion below divides
  // by it, and a sun on the horizon casts shadows of unbounded length; the floor
  // is the point past which a caster ceiling stops being purchasable at all (the
  // extrusion, and the union cull box built from it, would otherwise diverge).
  constexpr float k_min_sun_sine = 0.125f; // ~7.2 degrees elevation => at most an 8x reach
  const float sin_elev           = std::clamp(-sun_dir.y, k_min_sun_sine, 1.0f);

  fvec3 up = (fabsf(sun_dir.dotWith(fvec3(0, 1, 0))) > 0.99f) ? fvec3(0, 0, 1) : fvec3(0, 1, 0);
  fmtx4 snap_view; // rotation-only light view used purely for the snap
  snap_view.lookAt(fvec3(0, 0, 0), sun_dir, up);
  fmtx4 snap_view_inv = snap_view.inverse();

  fmtx4 cascade_view[LightManager::kSunCascadeStorage];
  fmtx4 cascade_proj[LightManager::kSunCascadeStorage];

  // SELECT radii published to the shader (it picks the innermost band whose
  // radius contains the fragment's distance from the anchor). Inset by two
  // texels off the fit radius: the band center is snapped, so the fitted
  // window sits up to one texel off the anchor on each light-space axis, and a
  // fragment selected at exactly the fit radius could land outside its map.
  // ...at the BAND's own dim, and widened by the jitter: the snap already puts
  // the window up to one texel off the anchor, and the jitter can add half of
  // one more on each axis.
  float split_dists[LightManager::kSunCascadeStorage];
  for (int i = 0; i < LightManager::kSunCascadeStorage; i++)
    split_dists[i] = fit_radius[i] * (1.0f - (4.0f + 2.0f * jitter_amp) / float(band_dim[i]));

  ////////////////////////////////////////
  // CULLSETS — resolve the authored rig HERE, at the one site that starts a
  // snapshot, alongside every other premise the job freezes. Unauthored yields
  // ONE set over every family and every band: one volume, one cull, no filter.
  ////////////////////////////////////////
  const SunCullSetPlan plan =
      resolveSunCullSets(dldata->_shadowCullSets, dldata->_shadowBandCullSets, cascade_count);
  _sun_job._plan = plan;

  // cascade-cull fix — accumulate the UNION of a CULLSET's cascade ortho boxes in a fixed sun-light
  // basis (F = sun_dir, R/U perpendicular). Each cascade box is [center ± radius] laterally and
  // [center - backoff, center + radius] along F (the toward-sun near backoff INCLUDED, so casters
  // behind the slice are kept). Min/max of dot(center,axis)±extent gives an AABB in the light basis
  // that CONTAINS every cascade box of that set -> _CULLCAM[s]'s survivors are a superset of every
  // subscribed slice's casters.
  //
  // PER SET, not per band: the volume is the union of ONLY its own bands' fit
  // radii, so a set whose outermost band is 1778 m is culled against 1778 m
  // however far the ladder reaches past it. One union over every band is what
  // dragged a scattered canopy into a 10 km box and then into every band's draw.
  const fvec3 cull_F = sun_dir;
  const fvec3 cull_R = up.crossWith(cull_F).normalized();
  const fvec3 cull_U = cull_F.crossWith(cull_R).normalized();
  float uRmin[SunCullSetPlan::kMaxSets], uRmax[SunCullSetPlan::kMaxSets];
  float uUmin[SunCullSetPlan::kMaxSets], uUmax[SunCullSetPlan::kMaxSets];
  float uFmin[SunCullSetPlan::kMaxSets], uFmax[SunCullSetPlan::kMaxSets];
  for (int s = 0; s < SunCullSetPlan::kMaxSets; s++) {
    uRmin[s] = +1e30f; uRmax[s] = -1e30f;
    uUmin[s] = +1e30f; uUmax[s] = -1e30f;
    uFmin[s] = +1e30f; uFmax[s] = -1e30f;
  }

  for (int ic = 0; ic < cascade_count; ic++) {
    float radius  = fit_radius[ic];
    fvec3 center  = cam_eye; // the band is centered on the viewer, nowhere else

    // texel snap (MANDATORY): quantize the band center to the shadow
    // texel grid in light space so the ortho window slides in whole texels.
    float texel_ws = (2.0f * radius) / float(band_dim[ic]); // CONSTANT for this band (see the invariant above)
    fvec4 c_ls     = fvec4(center, 1).transform(snap_view);
    c_ls.x         = floorf(c_ls.x / texel_ws) * texel_ws;
    c_ls.y         = floorf(c_ls.y / texel_ws) * texel_ws;
    // ...then the snapshot's sub-texel offset, AFTER the snap and in the same
    // light-space axes: the grid the window slides on is unchanged, the window
    // simply starts a fraction of a texel elsewhere on it, so this snapshot
    // samples the penumbra where the last one did not.
    c_ls.x += jitter_x * texel_ws;
    c_ls.y += jitter_y * texel_ws;
    fvec3 center_snapped = fvec4(c_ls.x, c_ls.y, c_ls.z, 1).transform(snap_view_inv).xyz();

    // TOWARD-LIGHT EXTENTS — explicitly PER BAND (near = how far back the eye
    // is pulled toward the light, far = how far past the band casters still
    // count). S1 gives every band the same pair; the pancaking slice varies
    // them per band, and it varies THESE two locals, nothing buried elsewhere.
    // Pull the eye back past the band so casters behind/above it still land in
    // the map (near extent derived from the shadowMaxDistance tunable).
    //
    // ...and the near extent is that tunable as a CASTER CEILING, not as a raw
    // along-light distance. A caster standing `max_dist` above the band sits
    // max_dist/sin(elevation) away ALONG the light, so extruding by max_dist
    // flat is elevation-blind: with the sun overhead it buys the whole ceiling,
    // at 15 degrees it buys a quarter of it, and the ridge whose shadow is
    // actually falling on the band is outside the window that was fitted to
    // capture it. Measured: a 150m mesa 600m up-sun at 15 degrees needs 570m of
    // reach and had 260. sin_elev converts the ceiling into the reach that
    // delivers it, so the same knob now means the same thing at every sun
    // angle, and this is the ONLY toward-light term the union cull box is built
    // from (backoff below) — cull volume and depth window stay in agreement.
    float band_near_extrude = radius + max_dist / sin_elev;
    float backoff           = band_near_extrude;
    fvec3 eye               = center_snapped - sun_dir * backoff;
    // ...and the SAME headroom on the far side. The ortho box is fitted to the
    // VIEW SLICE's bounding sphere, which bounds where shadows are RECEIVED, not
    // how far the casting geometry extends: terrain relief, a hillside, anything
    // standing further along the sun direction than the slice's far edge fell
    // outside the depth range and stopped casting entirely (owner, long-standing:
    // "objects dropping out of shadow due to far plane clipping"). The near side
    // was given max_dist of slack when this was written; the far side was given
    // none, and that asymmetry WAS the bug. Same reasoning as the terrain
    // camera's deliberately generous flat far plane (_terrain.py cam_far: sizing
    // it to the extent "was too tight: the far edge sat at the far plane and
    // clipped") — applied one pass further, to the shadow projection.
    // GATE HOOK (standing regression instrument): ORKID_SUN_SHADOW_FARPAD scales the
    // far headroom, so 0 reproduces the old clipping EXACTLY from the same binary and
    // the A/B needs no second build. Default 1 = fixed.
    static const float s_farpad_scale = []() -> float {
      if (const char* e = getenv("ORKID_SUN_SHADOW_FARPAD"))
        return std::max(0.0f, float(atof(e)));
      return 1.0f;
    }();
    float far_pad          = max_dist * s_farpad_scale;
    float band_far_extrude = radius + far_pad; // the band's OTHER toward-light extent (S3 varies this)
    // fold THIS cascade's ortho box into ITS CULLSET's union (light-basis AABB) for the sun-shadow cull.
    {
      const int cs = plan._bandSet[ic];
      float cr = center_snapped.dotWith(cull_R);
      float cu = center_snapped.dotWith(cull_U);
      float cf = center_snapped.dotWith(cull_F);
      uRmin[cs] = std::min(uRmin[cs], cr - radius); uRmax[cs] = std::max(uRmax[cs], cr + radius);
      uUmin[cs] = std::min(uUmin[cs], cu - radius); uUmax[cs] = std::max(uUmax[cs], cu + radius);
      uFmin[cs] = std::min(uFmin[cs], cf - backoff); uFmax[cs] = std::max(uFmax[cs], cf + band_far_extrude);
    }
    cascade_view[ic].lookAt(eye, center_snapped, up);
    cascade_proj[ic].ortho(-radius, radius, radius, -radius, 0.0f, backoff + band_far_extrude);

    _sun_job._view[ic] = cascade_view[ic];
    _sun_job._proj[ic] = cascade_proj[ic];
    // shader-facing shadow matrix — same composition as SpotLight::shadowMatrix
    _sun_job._shmtx[ic] = cascade_proj[ic] * cascade_view[ic];
  }
  for (int i = 0; i < LightManager::kSunCascadeStorage; i++)
    _sun_job._dim[i] = band_dim[i];
  // the per-band scalar tables, split across the vec4 pair the shader reads (see
  // the layout note): the base vec4 is bands 0..3 exactly as it always was, the
  // _hi one carries whatever the ladder reserves past that.
  auto band_scalars = [](const float* src, int lane) {
    fvec4 rval(0, 0, 0, 0);
    for (int i = 0; i < 4; i++) {
      int band = lane * 4 + i;
      if (band < LightManager::kSunCascadeStorage)
        rval[i] = src[band];
    }
    return rval;
  };
  float band_texel[LightManager::kSunCascadeStorage];
  for (int i = 0; i < LightManager::kSunCascadeStorage; i++)
    band_texel[i] = 1.0f / float(band_dim[i]);
  _sun_job._texel     = band_scalars(band_texel, 0);
  _sun_job._texel_hi  = band_scalars(band_texel, 1);
  _sun_job._splits    = band_scalars(split_dists, 0);
  _sun_job._splits_hi = band_scalars(split_dists, 1);
  // the bias travels in WORLD METRES; the evaluator divides by the band's own
  // fitted depth range (fwdtools.i2), so one authored number means the same
  // slack in every band of a ladder whose ranges differ by 3x or more.
  _sun_job._params = fvec4(
      dldata->GetShadowBias(),  //
      dldata->_pcfDither,       //
      float(cascade_count),     //
      1.0f / float(map_dim));

  ////////////////////////////////////////
  // render the cascades — one DEPTH_PREPASS pass per slice, mirroring
  // _update_shadow_maps (depth-only comes from the array's Z32F format).
  ////////////////////////////////////////

  // a dim/set change REALLOCATES the depth array: the outgoing snapshot's
  // depth ceases to exist, so it can no longer be faded from (its matrices
  // would sample whatever the new allocation happens to contain).
  bool storage_rebuilt = (map_dim != lmgr->_sun_cascade_dim)     //
                      or (want_sets != lmgr->_sun_cascade_sets)  //
                      or (cascade_count != lmgr->_sun_cascade_bands);
  lmgr->ensureSunCascades(context, map_dim, cascade_count, want_sets);
  if (storage_rebuilt or band_dims_changed)
    _sun_pub_valid = false;
  if (1 == want_sets)
    _sun_live_set = 0; // single-buffered: there is only one set to sample

  ////////////////////////////////////////
  // cascade-cull fix — the union-frustum sun cull, now ONE PER CULLSET. Build _CULLCAM[s] from that
  // set's accumulated light-basis AABB (symmetric ortho about the box center, eye pulled to the near
  // plane). Off-view casters (dropped by the eye cull) survive here, so the cascade passes draw the
  // correct shadow set.
  //
  // THE CULL ITSELF IS NOT DISPATCHED HERE. Bands are drawn in the plan's set-grouped order and each
  // set's cull runs immediately before its first band (_render_sun_snapshot_bands), so a snapshot
  // costs exactly one shadow cull PER SET — not per band, and not one giant cull the near bands then
  // over-draw. With one set that is the single pre-cullset dispatch, in the same place in the frame
  // (before any cascade depth draw) — Scene::shadowCull still no-ops when nothing culls.
  ////////////////////////////////////////
  static const bool s_shadowtrace = (getenv("ORKID_SHADOWCULL_TRACE") != nullptr);
  for (int s = 0; s < plan._numSets; s++) {
    _sun_job._cullvalid[s] = false;
    if (not(uRmax[s] > uRmin[s] and uUmax[s] > uUmin[s] and uFmax[s] > uFmin[s]))
      continue; // no band subscribed to this set (or a degenerate fold) -> nothing to cull for
    float hr    = (uRmax[s] - uRmin[s]) * 0.5f;
    float hu    = (uUmax[s] - uUmin[s]) * 0.5f;
    float depth = (uFmax[s] - uFmin[s]);
    fvec3 boxcenter = cull_R * ((uRmin[s] + uRmax[s]) * 0.5f) //
                    + cull_U * ((uUmin[s] + uUmax[s]) * 0.5f) //
                    + cull_F * ((uFmin[s] + uFmax[s]) * 0.5f);
    fvec3 cull_eye  = boxcenter - cull_F * (depth * 0.5f);
    auto CULLCAM    = _CULLCAM[s];
    CULLCAM->_vmatrix.lookAt(cull_eye, boxcenter, up);
    CULLCAM->_pmatrix.ortho(-hr, hr, hu, -hu, 0.0f, depth);
    // VP for the CULL (clip = world * VP): the camera convention is multiply_ltor(V,P), NOT operator*
    // (which is right-to-left and yields a VP the cull's plane extraction reads as garbage — an
    // off-view caster provably inside the ortho box was rejected until this).
    CULLCAM->_vpmatrix                 = fmtx4::multiply_ltor(CULLCAM->_vmatrix, CULLCAM->_pmatrix);
    CULLCAM->_ivpmatrix                = CULLCAM->_vpmatrix.inverse();
    CULLCAM->_ivmatrix                 = CULLCAM->_vmatrix.inverse();
    CULLCAM->_ipmatrix                 = CULLCAM->_pmatrix.inverse();
    CULLCAM->_frustum.set(CULLCAM->_vmatrix, CULLCAM->_pmatrix);
    CULLCAM->_explicitProjectionMatrix = true;
    CULLCAM->_explicitViewMatrix       = true;
    CULLCAM->_aspectRatio              = 1.0f;
    _sun_job._cullvalid[s]             = true;
    // shadow-flicker instrumentation (ORKID_SHADOWCULL_TRACE=1; OFF = not one instruction of cost).
    // A set's union box is the ONLY caster-rejection volume for the bands that subscribe to it, so
    // its per-frame motion is what a survivor-count series has to be correlated against. Corners are
    // emitted in world space (box center +- the light-basis half-extents) alongside the anchor and
    // light direction that drive them (the camera's ORIENTATION drives nothing here and is
    // deliberately absent).
    if (s_shadowtrace) {
      printf(
          "[cullbox] frame<%d> set<%s> mask<0x%x> anchor<%.3f %.3f %.3f> sundir<%.4f %.4f %.4f> "
          "center<%.3f %.3f %.3f> hr<%.3f> hu<%.3f> depth<%.3f>",
          context->GetTargetFrame(),
          plan._names[s].c_str(),
          plan._familyMask[s],
          cam_eye.x, cam_eye.y, cam_eye.z,
          sun_dir.x, sun_dir.y, sun_dir.z,
          boxcenter.x, boxcenter.y, boxcenter.z,
          hr, hu, depth);
      for (int ci = 0; ci < 8; ci++) {
        fvec3 corner = boxcenter                                       //
                     + cull_R * (((ci & 1) ? +hr : -hr))               //
                     + cull_U * (((ci & 2) ? +hu : -hu))               //
                     + cull_F * (((ci & 4) ? +depth : -depth) * 0.5f); //
        printf(" c%d<%.3f %.3f %.3f>", ci, corner.x, corner.y, corner.z);
      }
      printf("\n");
      fflush(stdout);
    }
  }
  _sun_job._culled_set = -1; // nothing culled yet for THIS snapshot

  ////////////////////////////////////////
  // SNAPSHOT STARTED — record the STRUCTURAL premises (which caster, which
  // rig) and restart the cadence clock. Recorded at job START, not at
  // completion: the structural tests compare against the fit the in-flight job
  // is being drawn for, so an amortized job cannot re-trigger itself frame
  // after frame and never land.
  // Recorded unconditionally (interval 0 included) so that ARMING the knob
  // mid-run holds against this frame rather than against a stale record.
  ////////////////////////////////////////
  _sun_snap_valid       = true;
  _sun_snap_caster      = sun;
  _sun_snap_body        = dldata->_skyBody;
  _sun_snap_map_dim     = map_dim;
  _sun_snap_cascades    = cascade_count;
  _sun_snap_max_dist    = max_dist;
  _sun_snap_band_radius = band_radius0;
  _sun_snap_band_ratio  = band_ratio;
  _sun_snap_bias        = dldata->GetShadowBias();
  _sun_snap_pcf         = dldata->_pcfDither;
  _sun_snap_res_ratio   = band_res_ratio;
  _sun_snap_cullsets    = cullset_rig;
  _sun_snap_sets        = want_sets;
  // ...and the refresh gate's geometric premises, recorded with the rest of
  // them: the gate measures drift from the fit THIS job is being drawn for,
  // never from the last published one, or an amortized snapshot would
  // re-trigger itself. (The caster count is edge-tested in the gate itself —
  // it is a change in the WORLD, not a premise of this fit.)
  _sun_snap_anchor      = cam_eye;
  _sun_snap_sundir      = sun_dir;
  _sun_snap_timer.Start();

  _sun_job._active     = true;
  _sun_job._next_band  = 0;
  _sun_job._band_count = cascade_count;
  // draw into the set the shader is NOT sampling; single-buffered means there
  // is no other set, which is only reachable when the whole snapshot lands in
  // this one frame.
  _sun_job._target_set = (2 == want_sets) ? (1 - _sun_live_set) : 0;

  // A STRUCTURAL start has nothing valid left to sample while it amortizes
  // (first-ever, a caster flip, or a rig change that just reallocated the
  // array), so it draws whole, this frame, regardless of the knob.
  _render_sun_snapshot_bands(structural ? cascade_count : bands_per_frame, fade_frames, fade_secs);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// S2a — draw up to `bands_this_frame` of the in-flight snapshot's cascade
// bands, and PUBLISH the snapshot when the last one lands.
//
// Every premise the bands are drawn against was frozen when the job started
// (matrices, anchor, and the one union cull box whose SHADOW survivor set the
// depth draws read — that set lives in the drawables' own buffers and is not
// re-derived per frame, so a deferred band draws against exactly the caster set
// its snapshot was culled for).
//
// PUBLISH is the atomic flip: the whole band set becomes live together. Bands
// are never published one at a time — band 0 fit around where the viewer stands
// now and band 3 fit around where they stood four frames ago is a world that
// does not exist, and the seam between two such bands is worse than the lag.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_sun_snapshot_bands(int bands_this_frame, int fade_frames, float fade_secs) {
  if (not _sun_job._active)
    return;
  auto lmgr = _currentCIMPL->lightManager();
  if (nullptr == lmgr)
    return;
  auto context = _currentContext;

  auto topcomp                  = _currentRCFD->topCompositor();
  _currentRCFD->_renderingmodel = "DEPTH_PREPASS"_crcu;
  _currentRCFD->_passID         = "SHADOW"_crcu;

  int budget = std::clamp(bands_this_frame, 1, _sun_job._band_count);
  for (int n = 0; n < budget and _sun_job._next_band < _sun_job._band_count; n++) {
    // CULLSETS — bands are drawn in the plan's SET-GROUPED order (identity when
    // one set, so an unauthored scene draws 0,1,2,... exactly as before). The
    // grouping is what makes the per-set cull run once per SET: crossing into a
    // set re-culls, and every band of that set then draws its survivors.
    int ic    = _sun_job._plan._drawOrder[_sun_job._next_band++];
    int cs    = _sun_job._plan._bandSet[ic];
    int slice = _sun_job._target_set * lmgr->_sun_cascade_bands + ic;
    if (cs != _sun_job._culled_set) {
      _sun_job._culled_set = cs;
      // the frozen union camera of THIS set (built at job start), and only the
      // families the set subscribes to — a drawable outside the set is not
      // culled for it and is not drawn in its bands.
      if (_sun_job._cullvalid[cs])
        if (auto* scene = _node->_pbrcommon ? _node->_pbrcommon->_scene : nullptr)
          scene->shadowCull(context, *_CULLCAM[cs], _sun_job._plan._familyMask[cs]);
    }
    if (getenv("ORKID_SUNSNAP_TRACE")) {
      printf("[sunsnap] frame<%d> DRAW band<%d> slice<%d>\n", context->GetTargetFrame(), ic, slice);
      fflush(stdout);
    }
    _currentContext->debugPushGroup(FormatString("ForwardPBR::_update_sun_cascades<%d>", ic));

    CompositingPassData shadowCPD = _currentCIMPL->topCPD().clone();
    shadowCPD._debugName          = "SunCascadePass";
    shadowCPD._sunCascadeShadowPass = true; // cascade-cull fix: GPU-culled draws read the SHADOW set
    // CULLSETS: this band draws only its set's caster families (the enqueue gate
    // in DrawQueue reads this). All-families = the pre-cullset pass verbatim.
    shadowCPD._sunCascadeCullFamilies = _sun_job._plan._familyMask[cs];
    shadowCPD._sunCascadeBand         = ic;

    // The prologue's CPD carries NO layer set — and enqueueLayerToRenderQueue
    // only admits layers present on the ACTIVE CPD (HasLayer gate). Assign the
    // depth_prepass role layers here and push the CPD BEFORE enqueueing, or
    // the cascade pass renders nothing. A drawable sun-shadows only if it
    // plays the depth_prepass role (same contract as spots).
    std::string dpp_layer_csv;
    {
      auto* scene = _node->_pbrcommon ? _node->_pbrcommon->_scene : nullptr;
      if (scene) {
        for (const auto& layer_name : scene->layersForRole("depth_prepass")) {
          if (!dpp_layer_csv.empty())
            dpp_layer_csv += ",";
          dpp_layer_csv += layer_name;
        }
      } else {
        dpp_layer_csv = "depth_prepass";
      }
    }
    shadowCPD.assignLayers(dpp_layer_csv);

    _SUNCAM->_pmatrix                  = _sun_job._proj[ic];
    _SUNCAM->_vmatrix                  = _sun_job._view[ic];
    _SUNCAM->_vpmatrix                 = _SUNCAM->_vmatrix * _SUNCAM->_pmatrix;
    _SUNCAM->_ivpmatrix                = _SUNCAM->_vpmatrix.inverse();
    _SUNCAM->_ivmatrix                 = _SUNCAM->_vmatrix.inverse();
    _SUNCAM->_ipmatrix                 = _SUNCAM->_pmatrix.inverse();
    _SUNCAM->_frustum.set(_SUNCAM->_vmatrix, _SUNCAM->_pmatrix);
    _SUNCAM->_explicitProjectionMatrix = true;
    _SUNCAM->_explicitViewMatrix       = true;
    _SUNCAM->_aspectRatio              = 1.0f;

    shadowCPD._mono_cam_matrices = _SUNCAM;

    auto FBI = _currentContext->FBI();
    FBI->PushRtGroup(lmgr->_sun_cascade_rtgs[slice].get());
    // PER-BAND RESOLUTION — the slice is allocated at the near band's dim and
    // this band draws into its top-left dim² corner. PushRtGroup has just
    // pushed the FULL-slice viewport/scissor, so the narrowing is pushed on top
    // of it and popped before the RTG (the pop order is the reverse of
    // PopRtGroup's own, which pops the pair it pushed). Corner, not center: the
    // viewport origin is what the shader's uv scale assumes, and an origin of 0
    // makes that scale a single multiply with no offset to get wrong.
    int band_dim = std::clamp(_sun_job._dim[ic], 8, lmgr->_sun_cascade_dim);
    bool sub_rect = (band_dim < lmgr->_sun_cascade_dim);
    if (sub_rect) {
      FBI->pushViewport(0, 0, band_dim, band_dim);
      FBI->pushScissor(0, 0, band_dim, band_dim);
    }
    topcomp->pushCPD(shadowCPD);

    // enqueue AFTER the push — the HasLayer gate reads the active CPD
    {
      auto* scene = _node->_pbrcommon ? _node->_pbrcommon->_scene : nullptr;
      if (scene) {
        for (const auto& layer_name : scene->layersForRole("depth_prepass")) {
          _currentDrawQueue->enqueueLayerToRenderQueue(layer_name, _currentIRenderer);
        }
      } else {
        _currentDrawQueue->enqueueLayerToRenderQueue("depth_prepass", _currentIRenderer);
      }
    }

    _currentIRenderer->drawEnqueuedRenderables(true);
    topcomp->popCPD();

    if (sub_rect) {
      FBI->popScissor();
      FBI->popViewport();
    }
    FBI->PopRtGroup();

    _currentContext->debugPopGroup();
  }

  if (_sun_job._next_band < _sun_job._band_count)
    return; // still drawing — the LIVE set stays published and sampled meanwhile

  ////////////////////////////////////////
  // FLIP. The fade half is armed only when there is an outgoing snapshot to
  // fade FROM, a second set to keep it in, and a fit the shader can still
  // select bands with — a rig change moves the band count and the map dims
  // under the outgoing maps, so it swaps hard by construction (and has already
  // reallocated the array anyway).
  ////////////////////////////////////////

  RenderPhaseScope _sun_cascade_flip_scope("sun-cascade-flip");

  int bands = _sun_job._band_count;
  bool fade_armed = (fade_frames > 0)          //
                and _sun_pub_valid             //
                and (2 == _sun_snap_sets)      //
                and (bands == _sun_pub_cascades);

  auto sun_buffer = PBRMaterial::sunDataBuffer(context);
  auto pub        = context->FXI()->mapUniformBuffer(sun_buffer, 0, k_ubo_size);

  if (fade_armed) {
    for (int ic = 0; ic < bands; ic++)
      pub->ref<fmtx4>(k_off_pshmtx + ic * sizeof(fmtx4)) = _sun_pub_shmtx[ic];
    pub->ref<fvec4>(k_off_psplits)    = _sun_pub_splits;
    pub->ref<fvec4>(k_off_psplits_hi) = _sun_pub_splits_hi;
    pub->ref<fvec4>(k_off_pbasis)     = fvec4(_sun_pub_anchor, 0);
    _sun_fade_prev_set   = _sun_live_set;
    _sun_fade_total      = fade_frames;
    _sun_fade_remaining  = fade_frames;
    _sun_fade_secs       = fade_secs;
    _sun_fade_timer.Start();
  } else {
    _sun_fade_remaining = 0;
    _sun_fade_total     = 0;
    _sun_fade_secs      = 0.0f;
  }

  _sun_live_set = _sun_job._target_set;

  for (int ic = 0; ic < bands; ic++)
    pub->ref<fmtx4>(k_off_shmtx + ic * sizeof(fmtx4)) = _sun_job._shmtx[ic];
  pub->ref<fvec4>(k_off_splits)     = _sun_job._splits;
  pub->ref<fvec4>(k_off_splits_hi)  = _sun_job._splits_hi;
  pub->ref<fvec4>(k_off_params)     = _sun_job._params;
  pub->ref<fvec4>(k_off_basis)      = fvec4(_sun_job._anchor, 0);
  // per-band texel sizes travel with the matrices they belong to. The fade's
  // PREV half reads this same vec4: a per-band DIM change forces a hard swap
  // (see band_dims_changed), so within any fade window both snapshots were
  // drawn at the same per-band viewports and one table describes both.
  pub->ref<fvec4>(k_off_texel)      = _sun_job._texel;
  pub->ref<fvec4>(k_off_texel_hi)   = _sun_job._texel_hi;
  // THE FLIP FRAME IS FULL PREV WEIGHT. The old form published
  // remaining/(total+1) — 12/13 for a 12-frame window — so every publish opened
  // with a 1/13 jump of the very quantity the fade exists to move smoothly.
  pub->ref<fvec4>(k_off_fade)       = fvec4(
      fade_armed ? _sun_fade_weight() : 0.0f,
      float(_sun_live_set * lmgr->_sun_cascade_bands),
      float(_sun_fade_prev_set * lmgr->_sun_cascade_bands),
      0.0f);
  pub->unmap();

  // the fit that just went live is the NEXT flip's outgoing half. Kept CPU-side
  // because the UBO is write-combined host memory — reading back what we wrote
  // to recover it would stall the render thread.
  for (int ic = 0; ic < bands; ic++)
    _sun_pub_shmtx[ic] = _sun_job._shmtx[ic];
  _sun_pub_splits    = _sun_job._splits;
  _sun_pub_splits_hi = _sun_job._splits_hi;
  _sun_pub_anchor   = _sun_job._anchor;
  _sun_pub_cascades = bands;
  _sun_pub_valid    = true;

  _sun_job._active = false;
  if (getenv("ORKID_SUNSNAP_TRACE")) {
    // the radii are printed PER LIVE BAND, not per vec4 lane: a trace that
    // stopped at four could not tell a 5-band ladder from a 4-band one, which
    // is the one thing this line is read for.
    auto band_list = [](const fvec4& lo, const fvec4& hi, int count) {
      std::string rval;
      for (int i = 0; i < count; i++)
        rval += FormatString("%s%g", i ? " " : "", (i < 4) ? lo[i] : hi[i - 4]);
      return rval;
    };
    printf("[sunsnap] frame<%d> t<%.3f> PUBLISH live<%d> prevset<%d> fade_armed<%d> w<%.4f> rem<%d> bands<%d> splits<%s> psplits<%s>\n",
           context->GetTargetFrame(), _sunsnap_trace_now(), _sun_live_set, _sun_fade_prev_set, int(fade_armed),
           fade_armed ? _sun_fade_weight() : 0.0f, _sun_fade_remaining, bands,
           band_list(_sun_job._splits, _sun_job._splits_hi, bands).c_str(),
           band_list(_sun_pub_splits, _sun_pub_splits_hi, _sun_pub_cascades).c_str());
    fflush(stdout);
  }

  ////////////////////////////////////////
  // ORKID_SUN_CASCADE_DUMP=<prefix> — ONE-SHOT eyeball of the depth the bands
  // actually hold. EVERY slice of the array (both snapshot sets when the double
  // buffer is armed) lands as <prefix>_slice<N>.png beside a stats line: how
  // much of the map is still CLEAR, the depth range the drawn texels span, the
  // bounding box they occupy, and where the band ANCHOR itself projects through
  // the published matrix. Three different faults read differently here: an
  // all-clear map is a caster that never drew, a populated map whose content
  // sits away from the anchor's texel is an anchor/matrix mismatch, and a
  // populated map at the wrong depth range is a projection fault.
  //
  // The readback transitions the array out of the layout the shader samples, so
  // the frame it fires on does not shade correctly; a diagnostic, not a live
  // path (ORKID_SUN_COOKIE_DUMP precedent).
  //
  // ORKID_SUN_CASCADE_PROBE="x,y,z[;x,y,z...]" adds a WORLD POINT readout to
  // every band's line: the point's uv/ndc.z through the published matrix, the
  // depth actually STORED at that texel, and the difference. That difference is
  // what separates the two ways a shadow can go missing — a stored depth equal
  // to the point's own is a receiver looking at itself (no caster there), a
  // stored depth well in front of it is a caster the sampler is failing to act
  // on. Whichever band the shader would SELECT for the point is flagged.
  ////////////////////////////////////////
  if (const char* dumppfx = getenv("ORKID_SUN_CASCADE_DUMP")) {
    static std::vector<fvec3> s_probes = []() -> std::vector<fvec3> {
      std::vector<fvec3> rval;
      const char* e = getenv("ORKID_SUN_CASCADE_PROBE");
      if (nullptr == e)
        return rval;
      std::string all(e);
      size_t pos = 0;
      while (pos <= all.size()) {
        size_t nx  = all.find(';', pos);
        std::string one = all.substr(pos, (nx == std::string::npos) ? std::string::npos : (nx - pos));
        float x = 0, y = 0, z = 0;
        if (3 == sscanf(one.c_str(), "%f,%f,%f", &x, &y, &z))
          rval.push_back(fvec3(x, y, z));
        if (nx == std::string::npos)
          break;
        pos = nx + 1;
      }
      return rval;
    }();
    // ...on the Nth PUBLISH (ORKID_SUN_CASCADE_DUMP_AT, default 30): a scene
    // needs a few snapshots before its streamed casters are on the GPU, and how
    // many publishes a settle takes is scene- and rate-dependent, so the count
    // is a knob rather than a constant nobody can reach.
    static int s_dump_countdown = []() -> int {
      if (const char* e = getenv("ORKID_SUN_CASCADE_DUMP_AT"))
        return std::max(1, atoi(e));
      return 30;
    }();
    if (s_dump_countdown > 0 and --s_dump_countdown == 0) {
      auto FBI = context->FBI();
      printf("[suncascade] live_set<%d> sets<%d> bands<%d> slices<%zu>\n",
             _sun_live_set, _sun_snap_sets, bands, lmgr->_sun_cascade_rtgs.size());
      for (int islice = 0; islice < int(lmgr->_sun_cascade_rtgs.size()); islice++) {
        int ic    = islice % std::max(lmgr->_sun_cascade_bands, 1);
        int slice = islice;
        auto rtg  = lmgr->_sun_cascade_rtgs[slice];
        if (nullptr == rtg or nullptr == rtg->_depthBuffer)
          continue;
        auto capbuf           = std::make_shared<CaptureBuffer>();
        capbuf->_captureLayer = slice;
        std::string path      = FormatString("%s_slice%d.png", dumppfx, slice);
        int band              = slice;
        int bdim              = _sun_job._dim[ic];
        fvec3 anchor          = _sun_job._anchor;
        fmtx4 shmtx           = _sun_job._shmtx[ic];
        fvec4 splits          = _sun_job._splits;
        fvec4 splits_hi       = _sun_job._splits_hi;
        int bandcount         = _sun_job._band_count;
        auto probes           = s_probes;
        FBI->captureAsFormat(
            rtg->_depthBuffer.get(),
            capbuf,
            EBufferFormat::R32F,
            [capbuf, path, band, bdim, anchor, shmtx, splits, splits_hi, bandcount, probes]() {
              auto src   = capbuf->_image;
              int w      = int(src->_width);
              int h      = int(src->_height);
              auto depth = (const float*)src->_data->data();
              float dmin = 1e30f, dmax = -1e30f;
              int drawn = 0, x0 = w, x1 = -1, y0 = h, y1 = -1;
              for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                  float d = depth[y * w + x];
                  if (d >= 0.999999f)
                    continue;
                  drawn++;
                  dmin = std::min(dmin, d);
                  dmax = std::max(dmax, d);
                  x0   = std::min(x0, x);
                  x1   = std::max(x1, x);
                  y0   = std::min(y0, y);
                  y1   = std::max(y1, y);
                }
              // the anchor's own texel — where the ground under the viewer must
              // be if the render matrices and the published ones agree.
              fvec4 aclip = fvec4(anchor, 1).transform(shmtx);
              fvec3 andc  = aclip.xyz() * (1.0f / std::max(fabsf(aclip.w), 1e-9f));
              float au    = andc.x * 0.5f + 0.5f;
              float av    = 1.0f - (andc.y * 0.5f + 0.5f);
              printf(
                  "[suncascade] band<%d> dim<%d> of <%dx%d> drawn<%d> (%.2f%%) depthrange<%g..%g> "
                  "bbox<%d %d %d %d> anchor<%.2f %.2f %.2f> anchor_uv<%.4f %.4f> anchor_z<%g>\n",
                  band, bdim, w, h, drawn, 100.0f * float(drawn) / float(w * h),
                  (drawn ? dmin : -1.0f), (drawn ? dmax : -1.0f), x0, y0, x1, y1,
                  anchor.x, anchor.y, anchor.z, au, av, andc.z);
              fflush(stdout);
              // ...and the same reading for every declared world probe, sampled
              // out of the depth this band actually holds. uv_scale is the band's
              // viewport fraction of the slice (1.0 for a full-dim band), exactly
              // the factor the evaluator applies.
              float uv_scale = float(bdim) / float(w);
              for (size_t ip = 0; ip < probes.size(); ip++) {
                fvec3 P     = probes[ip];
                fvec4 pclip = fvec4(P, 1).transform(shmtx);
                fvec3 pndc  = pclip.xyz() * (1.0f / std::max(fabsf(pclip.w), 1e-9f));
                float pu    = (pndc.x * 0.5f + 0.5f) * uv_scale;
                float pv    = (0.5f - pndc.y * 0.5f) * uv_scale;
                int tx      = int(pu * float(w));
                int ty      = int(pv * float(h));
                bool inmap  = (tx >= 0) and (tx < w) and (ty >= 0) and (ty < h) //
                          and (fabsf(pndc.x) < 1.0f) and (fabsf(pndc.y) < 1.0f) //
                          and (pndc.z > 0.0f) and (pndc.z < 1.0f);
                float stored = inmap ? depth[ty * w + tx] : -1.0f;
                // the band the EVALUATOR would pick for this point (innermost
                // split sphere that contains it) — a reading in any other band
                // is informative but is not the one the shader acts on.
                float bdist = (P - anchor).magnitude();
                int selband = -1;
                for (int i = bandcount - 1; i >= 0; i--)
                  if (bdist <= ((i < 4) ? splits[i] : splits_hi[i - 4]))
                    selband = i;
                printf(
                    "[suncascade] band<%d> probe%zu<%.2f %.2f %.2f> uv<%.5f %.5f> texel<%d %d> "
                    "ndcz<%g> stored<%g> delta<%g> inmap<%d> banddist<%.2f> selected_band<%d>\n",
                    band, ip, P.x, P.y, P.z, pu, pv, tx, ty, pndc.z, stored, pndc.z - stored, int(inmap), bdist, selband);
              }
              fflush(stdout);
              // MAGENTA = clear (nothing drew there), grayscale = the drawn
              // depth stretched across its own range so any content is legible
              // whatever slab it occupies.
              auto out = std::make_shared<Image>();
              out->initWithFormat(w, h, EBufferFormat::RGBA8);
              auto dst   = (uint8_t*)out->_data->data();
              float span = std::max(dmax - dmin, 1e-9f);
              for (int i = 0; i < w * h; i++) {
                float d = depth[i];
                uint8_t r, g, b;
                if (d >= 0.999999f) {
                  r = 255; g = 0; b = 255;
                } else {
                  uint8_t v = uint8_t(std::clamp(255.0f * (1.0f - (d - dmin) / span), 0.0f, 255.0f));
                  r = g = b = v;
                }
                dst[i * 4 + 0] = r;
                dst[i * 4 + 1] = g;
                dst[i * 4 + 2] = b;
                dst[i * 4 + 3] = 255;
              }
              out->writeToFile(file::Path(path.c_str()));
              printf("[suncascade] wrote %s\n", path.c_str());
              fflush(stdout);
            });
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLOUD SHADOWS — fill the SUN COOKIE.
//
// ONE occlusion source, no second model: the cookie is the cloud decks
// THEMSELVES, drawn with their own shader and their own params from a
// sun-aligned ortho camera. The number that lands in the texture is the very
// alpha the decks already compute (fractional_occlusion of the deck's presence
// and depth — ptex3d/functions.py, "the one term"), so the extinction model
// cannot drift between what the sky shows and what the ground is shaded by.
//
// THE BLEND IS THE MATH. The decks draw PREMULTIPLIED, and that blend's ALPHA
// equation — a = a_src + a_dst*(1 - a_src) — is exactly the order-independent
// union of N shells' occlusions. So the cookie's ALPHA channel, cleared to 0,
// IS the accumulated cloud occlusion: no depth buffer, no sorting, and not one
// line of duplicated arithmetic. Consumers read TRANSMITTANCE as 1 - a. (The RGB
// channel carries the decks' premultiplied radiance over a white clear and is
// ignored; it exists so ORKID_SUN_COOKIE_DUMP is legible.) The pushed raster
// state outranks the deck technique's own state block (priority) only to force
// depth and cull OFF — the blend is the material's own.
//
// WHICH GEOMETRY: the "sun_cookie" layer ROLE. A deck opts in by declaring a
// second scenegraph node there (the decks' visible node stays on
// std_transparent), so nothing else a scene draws can accidentally paint a
// cloud shadow, and a scene with no cookie layer simply publishes nothing.
//
// SOFTNESS is the mip chain: the fill is followed by generateMipMaps and the
// shader samples at the published LOD bias. Cloud shadows must read FUZZIER
// than the ground-object shadows in the same frame (owner law) — a penumbra
// scaled by a filter footprint, never a coverage stencil.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_update_sun_cookie() {
  auto lmgr = _currentCIMPL ? _currentCIMPL->lightManager() : nullptr;
  if (nullptr == lmgr)
    return;

  // DISARM is the default and must be reachable on EVERY early return: a stale
  // cookie is a shadow that outlives its clouds.
  auto disarm = [this, lmgr]() {
    lmgr->_sun_cookie          = lmgr->_sun_cookie_default;
    lmgr->_sun_cookie_strength = 0.0f;
    _sun_cookie_valid          = false; // nothing published to hold onto
  };

  if (nullptr == _enumeratedLights) {
    disarm();
    return;
  }

  // caster selection MIRRORS _update_sun_cascades (first shadow-caster in the
  // priority-sorted list, else the reconciled fallback): the cookie must be
  // projected from whatever body holds the cascade, or the ground would be
  // shaded from one direction and lit from another.
  DirectionalLight* sun = nullptr;
  for (auto dl : _enumeratedLights->_directionallights) {
    if (dl->_castsShadows and (nullptr == sun))
      sun = dl;
  }
  if (nullptr == sun and _enumeratedLights->_directionallights.size())
    sun = _enumeratedLights->_directionallights[0];
  if (nullptr == sun) {
    disarm();
    return;
  }

  static const DirectionalLightData s_default_dldata;
  const DirectionalLightData* dldata = sun->_dldata ? sun->_dldata : &s_default_dldata;
  float strength                     = std::clamp(dldata->_cloudShadowStrength, 0.0f, 1.0f);
  if (strength <= 0.0f) {
    disarm();
    return;
  }

  auto* scene = _node->_pbrcommon ? _node->_pbrcommon->_scene : nullptr;
  if (nullptr == scene) {
    disarm();
    return;
  }

  // Self-defense: cloud shadows asked for with nothing to cast them is a scene
  // authoring error, not a look. Loud ONCE per state change — this runs every
  // frame and a per-frame print would drown the log it is trying to be seen in.
  size_t cookie_drawables = 0;
  for (const auto& layer_name : scene->layersForRole("sun_cookie")) {
    if (auto layer = scene->tryFindLayer(layer_name))
      layer->_drawable_nodes.atomicOp([&](const scenegraph::Layer::drawablenodevect_t& nodes) { //
        cookie_drawables += nodes.size();
      });
  }
  static size_t s_last_cookie_drawables = size_t(-1);
  if (cookie_drawables != s_last_cookie_drawables) {
    s_last_cookie_drawables = cookie_drawables;
    if (0 == cookie_drawables)
      printf(
          "[FWD:SUN] WARNING: CloudShadowStrength<%g> but the 'sun_cookie' layer role holds no "
          "drawables — nothing can cast a cloud shadow. Declare the deck's cookie node "
          "(Scene.cloud_decks(cookie_layer=...)) or set the strength to 0.\n",
          dldata->_cloudShadowStrength);
  }
  if (0 == cookie_drawables) {
    disarm();
    return;
  }

  ////////////////////////////////////////
  // COOKIE CADENCE — one fill serves _cloudShadowRefreshFrames frames. HELD, not
  // recomputed: the published texture, matrix and dials are all last fill's, and
  // they belong together (the matrix is what makes the texture a world-space
  // field; refreshing it alone would slide the clouds' shadow across the ground
  // by the camera's motion). Every DISARM above this point is unreachable from
  // here and stays immediate — a strength of 0, a scene with no decks, or a lost
  // light drops the cookie on the frame it happens, never at the next tick.
  ////////////////////////////////////////
  int cookie_refresh = std::clamp(dldata->_cloudShadowRefreshFrames, 1, 240);
  if (_sun_cookie_valid and (_sun_cookie_held < (cookie_refresh - 1))) {
    _sun_cookie_held++;
    return;
  }
  _sun_cookie_held = 0;

  auto context = _currentContext;
  int dim      = std::clamp(dldata->_cloudShadowMapSize, 64, 4096);
  lmgr->ensureSunCookie(context, dim);
  auto rtg = lmgr->_sun_cookie_rtg;
  if (nullptr == rtg or nullptr == rtg->texture(0)) {
    disarm();
    return;
  }

  ////////////////////////////////////////
  // sun-aligned ortho fit about the viewer, texel-snapped in light space —
  // the same shimmer law the cascades obey: the window must slide in whole
  // texels or a walking viewer makes the cloud shadows crawl.
  ////////////////////////////////////////

  fvec3 sun_dir = sun->direction().normalized(); // the direction the light TRAVELS
  fvec3 up      = (fabsf(sun_dir.dotWith(fvec3(0, 1, 0))) > 0.99f) ? fvec3(0, 0, 1) : fvec3(0, 1, 0);
  float extent  = std::max(dldata->_cloudShadowExtent, 1.0f);
  float depth   = std::max(dldata->_cloudShadowDepth, 2.0f * extent);

  fmtx4 snap_view;
  snap_view.lookAt(fvec3(0, 0, 0), sun_dir, up);
  fmtx4 snap_view_inv = snap_view.inverse();

  float texel_ws = (2.0f * extent) / float(dim);
  fvec4 c_ls     = fvec4(_currentViewData._camposmono, 1).transform(snap_view);
  c_ls.x         = floorf(c_ls.x / texel_ws) * texel_ws;
  c_ls.y         = floorf(c_ls.y / texel_ws) * texel_ws;
  fvec3 center   = fvec4(c_ls.x, c_ls.y, c_ls.z, 1).transform(snap_view_inv).xyz();

  // THE EYE SITS AT THE VIEWER, looking toward the light — the cookie asks
  // "how much cloud is between HERE and the sun", which is the same question
  // the receiver asks, from the same place. It also keeps the pass camera's
  // DISTANCE to the deck honest: the deck material fades itself out with
  // aerial perspective measured from the rendering eye, and a camera parked
  // kilometers up-sun would haze the deck away and hand back a cookie far
  // thinner than the sky the viewer is standing under.
  //  v1 LIMIT: the near plane is therefore the viewer's own plane, so a camera
  // flying ABOVE the deck sees no cloud between itself and the sun and casts no
  // shadow on the ground below it.
  fvec3 eye = center;
  fmtx4 cookie_view, cookie_proj;
  // looking UP-SUN (-sun_dir): the decks are between the viewer and the sun, so
  // they are what lies in FRONT of this camera — and it is the deck's underside,
  // the same face the ground sees.
  cookie_view.lookAt(eye, eye - sun_dir * depth, up);
  cookie_proj.ortho(-extent, extent, extent, -extent, 0.0f, depth);

  // world -> cookie UV. The bias folds the NDC->[0,1] map AND the v flip: the
  // raster path uses a negative viewport height, so ndc.y=+1 lands at texture
  // row 0 (the same flip _sun_shadow_set_factor applies by hand).
  fmtx4 bias;
  bias.setElemXY(0, 0, 0.5f);
  bias.setElemXY(1, 1, -0.5f);
  bias.setElemXY(3, 0, 0.5f);
  bias.setElemXY(3, 1, 0.5f);
  fmtx4 cookie_matrix = bias * (cookie_proj * cookie_view);

  ////////////////////////////////////////
  // the fill pass
  ////////////////////////////////////////

  _currentContext->debugPushGroup("ForwardPBR::_update_sun_cookie");

  auto topcomp             = _currentRCFD->topCompositor();
  auto prev_renderingmodel = _currentRCFD->_renderingmodel;
  auto prev_passID         = _currentRCFD->_passID;
  // the decks must resolve their FORWARD pipeline here — the prologue's last
  // shadow pass left DEPTH_PREPASS on the RCFD, which would draw them as depth.
  _currentRCFD->_renderingmodel = "FORWARD_PBR"_crcu;
  _currentRCFD->_passID         = "PRIMARY"_crcu;

  // The forward material state lambdas read these RCFD properties UNCONDITIONALLY
  // (fwdnode_pipeline.cpp) and the color pass does not publish them until well
  // after the prologue — a missing one is a hard assert, not a black draw. The
  // lighting/probe values are the real ones (already enumerated above); the
  // screen-space ones are NEUTRAL stand-ins, mirroring the ssao-disabled branch
  // in _render_dppskyssaocolor: a cookie is a transmittance map, and screen-space
  // occlusion of the CAMERA's frame has no meaning in the sun's frame.
  bool have_probes = (_enumeratedLights->_lightprobes.size() > 0);
  _currentRCFD->setUserProperty("enumeratedlights"_crcu, _enumeratedLights);
  _currentRCFD->setUserProperty("renderingPROBE"_crcu, false);
  _currentRCFD->setUserProperty("havePROBES"_crcu, have_probes);
  _currentRCFD->setUserProperty("PBR_COMMON"_crcu, _node->_pbrcommon);
  auto neutral_tex = _whiteTexture ? _whiteTexture->GetTexture() : nullptr;
  _currentRCFD->setUserProperty("SSAO_MAP"_crcu, neutral_tex);
  _currentRCFD->setUserProperty("DEPTH_MAP"_crcu, neutral_tex);
  _currentRCFD->setUserProperty("SSAO_DIM"_crcu, fvec2(8, 8));
  _currentRCFD->setUserProperty("SSAO_POWER"_crcu, 1.0f);
  _currentRCFD->setUserProperty("SSAO_WEIGHT"_crcu, 10.0f);

  CompositingPassData cookieCPD = _currentCIMPL->topCPD().clone();
  cookieCPD._debugName          = "SunCookiePass";
  // ALPHA-ONLY PASS. Materials that can produce their alpha without scene lighting select their
  // FWD_SUNCOOKIE technique off this flag (FxPipelinePermutation::_is_sun_cookie) — the forward
  // technique declares 16 lighting samplers this pass never reads, and a pipeline layout counts
  // declarations, so it lands at 17 against Metal's per-stage cap of 16. Materials without that
  // technique keep the forward pipeline, so nothing else about this pass changes.
  cookieCPD._sunCookiePass      = true;
  cookieCPD.assignLayers([&]() -> std::string {
    std::string csv;
    for (const auto& layer_name : scene->layersForRole("sun_cookie")) {
      if (not csv.empty())
        csv += ",";
      csv += layer_name;
    }
    return csv;
  }());

  _COOKIECAM->_pmatrix                  = cookie_proj;
  _COOKIECAM->_vmatrix                  = cookie_view;
  _COOKIECAM->_vpmatrix                 = fmtx4::multiply_ltor(cookie_view, cookie_proj);
  _COOKIECAM->_ivpmatrix                = _COOKIECAM->_vpmatrix.inverse();
  _COOKIECAM->_ivmatrix                 = cookie_view.inverse();
  _COOKIECAM->_ipmatrix                 = cookie_proj.inverse();
  _COOKIECAM->_frustum.set(cookie_view, cookie_proj);
  _COOKIECAM->_explicitProjectionMatrix = true;
  _COOKIECAM->_explicitViewMatrix       = true;
  _COOKIECAM->_aspectRatio              = 1.0f;
  cookieCPD._mono_cam_matrices          = _COOKIECAM;
  cookieCPD.SetDstRect(ViewportRect(0, 0, dim, dim));
  cookieCPD._width  = dim;
  cookieCPD._height = dim;

  // OCCLUSION ACCUMULATION (see the header note): the deck's OWN premultiplied
  // blend, which composes alpha as a = a_src + a_dst*(1 - a_src) — the
  // order-independent union of the shells' occlusions. Priority outranks the
  // deck technique's state block only to force depth OFF and cull OFF (no depth
  // buffer here, and every shell must fold in whichever way it faces); the blend
  // is deliberately the same one the visible deck draws with.
  static const rasterstate_ptr_t s_cookie_rstate = []() -> rasterstate_ptr_t {
    auto rs   = std::make_shared<RasterState>();
    rs->_name = "SunCookieAccum";
    rs->setBlendingMacro(BlendingMacro::PREMA);
    rs->setDepthTest(EDepthTest::OFF);
    rs->setCullTest(ECullTest::OFF);
    rs->setWriteMaskZ(false);
    rs->setWriteMaskRGB(true);
    rs->setWriteMaskA(true);
    rs->_priority = 1 << 24;
    return rs;
  }();

  // DISARM FOR THE DURATION OF THE FILL. Every PBR draw binds lmgr->_sun_cookie
  // (fwdnode_pipeline), and the decks are PBR draws — so leaving last frame's
  // publish in place would bind the very image this pass is rendering INTO
  // (color-attachment layout; the bind asserts). Pointing at the 1x1 white
  // default is also the honest answer: a cloud does not shadow itself.
  disarm();

  auto FBI = context->FBI();
  auto FXI = context->FXI();
  FBI->PushRtGroup(rtg.get());
  // PushRtGroup does NOT size the viewport — it inherits whatever the previous
  // pass left, and the prologue's spot-shadow passes leave theirs. Without this
  // the cookie window is rasterized at some other pass's scale and the map comes
  // back white (the defect this comment exists to stop from recurring).
  ViewportRect cookie_vp(0, 0, dim, dim);
  FBI->pushViewport(cookie_vp);
  FBI->pushScissor(cookie_vp);
  FXI->pushRasterState(s_cookie_rstate);
  topcomp->pushCPD(cookieCPD);

  for (const auto& layer_name : scene->layersForRole("sun_cookie"))
    _currentDrawQueue->enqueueLayerToRenderQueue(layer_name, _currentIRenderer);

  if (getenv("ORKID_SUN_COOKIE_TRACE")) {
    fvec4 eye_uv = fvec4(_currentViewData._camposmono, 1).transform(cookie_matrix);
    printf(
        "[suncookie] frame<%d> dim<%d> extent<%g> depth<%g> strength<%g> lod<%g> "
        "sundir<%g %g %g> center<%g %g %g> queued<%zu> eye_uv<%g %g>\n",
        context->GetTargetFrame(), dim, extent, depth, strength, dldata->_cloudShadowSoftness,
        sun_dir.x, sun_dir.y, sun_dir.z, center.x, center.y, center.z,
        _currentIRenderer->_unsortedNodes.Size(),
        eye_uv.w != 0.0f ? eye_uv.x / eye_uv.w : -1.0f,
        eye_uv.w != 0.0f ? eye_uv.y / eye_uv.w : -1.0f);
    fflush(stdout);
  }

  _currentIRenderer->drawEnqueuedRenderables(true);

  topcomp->popCPD();
  FXI->popRasterState();
  FBI->popScissor();
  FBI->popViewport();
  FBI->PopRtGroup();

  _currentRCFD->_renderingmodel = prev_renderingmodel;
  _currentRCFD->_passID         = prev_passID;

  // mips ARE the softness knob (and the sampler must be (re)built against a
  // GPU-initialized, mipped texture — see the impostor atlas note).
  auto TXI = context->TXI();
  auto tex = rtg->texture(0);
  TXI->generateMipMaps(tex.get());
  TXI->ApplySamplingMode(tex.get());

  _currentContext->debugPopGroup();

  // ORKID_SUN_COOKIE_DUMP=<path.png> — ONE-SHOT eyeball of the transmittance map
  // itself (white = sun through, black = deck). The readback transitions the
  // image to host-read, so the frame it fires on will not sample the cookie
  // correctly; it is a diagnostic, not a live path (ORKID_IMPOSTOR_DUMP
  // precedent).
  if (const char* dumppath = getenv("ORKID_SUN_COOKIE_DUMP")) {
    static int s_dump_countdown = 30; // let the decks settle first
    if (s_dump_countdown > 0 and --s_dump_countdown == 0) {
      auto capbuf      = std::make_shared<CaptureBuffer>();
      std::string path = dumppath;
      FBI->captureAsFormat(rtg->buffer(0).get(), capbuf, EBufferFormat::RGBA8, [capbuf, path]() {
        capbuf->_image->writeToFile(file::Path(path.c_str()));
        printf("[suncookie] wrote %s\n", path.c_str());
        fflush(stdout);
      });
    }
  }

  lmgr->_sun_cookie          = tex;
  lmgr->_sun_cookie_matrix   = cookie_matrix;
  lmgr->_sun_cookie_strength = strength;
  _sun_cookie_valid          = true; // a fill landed — the cadence may hold it
  lmgr->_sun_cookie_lod      = std::max(dldata->_cloudShadowSoftness, 0.0f);
  lmgr->_sun_cookie_extinction = std::max(dldata->_cloudExtinction, 0.0f);
  lmgr->_sun_cookie_disc_lod   = std::max(dldata->_cloudDiscSoftness, 0.0f);
  lmgr->_sun_cookie_dir      = sun_dir;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SKYLIGHT lane B — Hillaire LUT chain. Runs inside the prologue CPD bracket,
// BEFORE the probe captures: those draw the skybox themselves, so in procedural
// mode they need this frame's sky-view LUT already baked and published. Fully
// inert (and allocates nothing) for a scene with no atmosphere and a baked sky
// source, which is what keeps baked-envmap renders byte-identical.
//
// The sky-view LUT is built from the CENTER-EYE position (L4) — the prologue's
// mono CPD camera, the same source the cascade fit and the probe passes use —
// so both eyes share one LUT.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_update_sky_luts() {
  auto pbrcommon = _node->_pbrcommon;
  if (nullptr == pbrcommon)
    return;
  auto atmo = pbrcommon->_atmosphere;
  if (nullptr == atmo) {
    // self-defense: a scene that selected the procedural sky without attaching
    // an atmosphere gets the earth-like default. The technique has no
    // baked-envmap fallback (T10), so the alternative would be a black sky.
    if (pbrcommon->_sky_source != SkySource::PROCEDURAL)
      return;
    atmo = std::make_shared<SkyAtmosphereData>();
    pbrcommon->_atmosphere = atmo;
    logchan_fwd_sub->log("procedural sky source with no SkyAtmosphereData attached - using defaults");
  }

  auto context = _currentContext;
  if (nullptr == _hillaire_sky)
    _hillaire_sky = HillaireSky::create(context);

  // direction TOWARD the sun. DirectionalLight::direction() is the TRAVEL
  // direction of the sunlight, hence the negation. Same sun pick as
  // _update_sun_cascades: _directionallights[0] is the HIGHEST-priority
  // directional (LightManager::enumerateInPass sorts descending by LightData
  // Priority), so the sky source and the cascade slot agree by construction.
  // With no sun in the scene the atmosphere still needs a well-defined
  // configuration, so fall back to straight overhead rather than degenerating
  // to a black LUT.
  fvec3 dir_to_sun(0, 1, 0);
  if (_enumeratedLights and _enumeratedLights->_directionallights.size())
    dir_to_sun = _enumeratedLights->_directionallights[0]->direction().normalized() * -1.0f;

  // the MOON is picked by DECLARATION (LightData SkyBody), never by rank: the
  // priority gap below the sun is what keeps the sky source unambiguous, and the
  // moon's shadow-caster flag flips with the night policy. No moon declared =
  // zero, which is the visible disc's "no moon" gate — there is no fallback
  // direction, since an invented moon would be a second sky.
  //
  // Its ILLUMINANCE comes off the very same light: color x intensity, which is
  // where a scene's lunar phase scaling already lives. That is what the sky's
  // moonlight scattering is driven by, so a waning moon dims the night ambient
  // without anything else being told about the phase.
  fvec3 dir_to_moon(0, 0, 0);
  fvec3 moon_illuminance(0, 0, 0);
  if (_enumeratedLights) {
    for (auto dl : _enumeratedLights->_directionallights) {
      if (dl->skyBody() == 2) {
        dir_to_moon      = dl->direction().normalized() * -1.0f;
        moon_illuminance = dl->color() * dl->intensity();
        break;
      }
    }
  }

  // the floor is applied HERE, not only inside updateSkyView, so the altitude
  // published below is the one the LUT was actually parameterized with — the
  // skybox shader rebuilds its horizon angle from it.
  float altitude_km = std::max(
      _currentViewData._camposmono.y * atmo->_kilometersPerWorldUnit, //
      atmo->_minViewAltitude);

  _currentRCFD->_renderingmodel = "CUSTOM"_crcu;
  _currentRCFD->_subpassID      = "SKY_LUT"_crcu;
  _hillaire_sky->updateSkyView(context, _currentRCFD, atmo, dir_to_sun, altitude_km);

  // publish what the LUT was ACTUALLY baked with, for every consumer downstream
  // in this frame (the skybox technique today; aerial perspective and the IBL
  // snapshot in later slices). Re-deriving the sun anywhere else is how the
  // visible disc and the LUT would drift apart.
  auto skyframe               = std::make_shared<SkyFrameState>();
  skyframe->_skyViewLUT       = _hillaire_sky->skyViewTexture();
  skyframe->_transmittanceLUT = _hillaire_sky->transmittanceTexture();
  skyframe->_multiScatterLUT  = _hillaire_sky->multiScatterTexture();
  skyframe->_dirToSun         = dir_to_sun;
  skyframe->_dirToMoon        = dir_to_moon;
  skyframe->_moonIlluminance  = moon_illuminance;
  skyframe->_viewAltitudeKm   = altitude_km;
  _currentRCFD->setUserProperty("SKY_FRAME"_crcu, skyframe);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SKYLIGHT lane B slice B3 — the IBL feed (spec §4.B step 5), the LAGGED tier's
// entry point. Immediately after the LUT step, so it consumes THIS frame's
// SKY_FRAME, and before the probe captures, which have nothing to do with it.
//
// Renders the equirect snapshot and starts a sliced refilter ONLY at cycle
// start; the rest of the time (which is nearly every frame) it costs one dot
// product. The single-snapshot + hard-gate policy is what satisfies the §2
// snapshot law: while a job is in flight NOTHING repaints its source, so the
// prefilter's input is frozen for its whole multi-frame life (T12). The cost is
// bounded lag on a fast sun scrub — the accepted L3 trade.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_update_sky_ibl() {
  auto pbrcommon = _node->_pbrcommon;
  if (nullptr == pbrcommon)
    return;
  if (pbrcommon->_sky_source != SkySource::PROCEDURAL)
    return;
  auto atmo = pbrcommon->_atmosphere;
  if ((nullptr == atmo) or (not atmo->_iblFeedEnable))
    return;
  auto state = pbrcommon->_sky_ibl;
  if (nullptr == state)
    return;

  // One frame of the publish crossfade, BEFORE any early-out below: the fade
  // runs on frames where the feed has nothing else to do (that is most of
  // them), and stalling it would leave the outgoing maps blended in forever.
  state->tickCrossfade(_currentContext);

  // Likewise BEFORE the in-flight gate: the projection this services runs while
  // the refilter it shares a snapshot with is still slicing, and the promotion
  // has to land on the publish frame whatever the trigger is doing.
  _update_sky_sh();

  // The gate. NOT an error — a cycle spanning many frames is the design; every
  // frame in that window simply declines to start another one.
  if (state->_inflight.load())
    return;

  // _update_sky_luts ran first and published this; the sun/altitude it carries
  // are the ones the sky-view LUT the snapshot resamples was baked with.
  auto skyframe = _currentRCFD->userPropertyAs<skyframestate_ptr_t>("SKY_FRAME"_crcu);
  OrkAssertI(
      skyframe != nullptr,
      "sky IBL feed reached with no SKY_FRAME published - _update_sky_ibl must follow _update_sky_luts "
      "inside the same prologue");
  OrkAssertI(_hillaire_sky != nullptr, "sky IBL feed reached before the LUT chain was created");

  ////////////////////////////////////////
  // trigger policy (A8 - every threshold is a reflected knob)
  ////////////////////////////////////////

  uint64_t medium_hash = atmo->mediumHash();
  uint64_t haze_hash   = atmo->hazePresentationHash();
  float sun_delta_deg  = 0.0f;
  if (state->_ever_snapped) {
    float ct      = std::clamp(skyframe->_dirToSun.dotWith(state->_last_snapshot_dir_to_sun), -1.0f, 1.0f);
    sun_delta_deg = acosf(ct) * float(RTOD);
  }

  // CHAINING LAW. With _iblContinuousChain the sun-motion threshold stops being
  // a keyframe size and becomes a floor: a cycle starts as soon as (a) the
  // previous fade has SETTLED (weight pinned at 1 — a cycle started mid-fade
  // would need three map sets blended, which completeCycle can only resolve by
  // dropping one, i.e. by popping) and (b) the sun has moved at least
  // _iblChainMinAngleDeg since the snapshot. The floor is never zero: a
  // dead-still sun must not chain cycles forever over an unchanged sky. A
  // medium edit stays an immediate trigger in both modes (the header's law).
  //
  // _iblChainMaxHz adds the third chained-cycle condition: (c) at least
  // 1/max_hz seconds have passed since the LAST cycle started. Uncapped at 0,
  // which is the default and the behavior every measurement so far was taken
  // under.
  //
  // DECLARED CADENCE replaces every SUN-MOTION policy above. With
  // _iblSnapshotInterval > 0 the sun stops driving the feed and the clock is the
  // only motion trigger: neither the chaining angle floor nor the chaining-OFF
  // keyframe threshold is consulted, so the rebake rate stops being a function of
  // the scene's day-cycle rate. Both branches have to go, not just the else:
  // chaining is the default, so leaving its floor live would keep a sun-delta
  // trigger the declaration meant to retire.
  //
  // fadeSettled() SURVIVES the swap, and is the one condition that does. A cycle
  // starting mid-fade needs three map sets blended, which completeCycle can only
  // resolve by dropping one — i.e. by popping — and no knob may make a pop the
  // default behavior of a shipped path. So the declared interval is FLOORED at
  // the measured cycle+fade span (SkyIblState::cadenceFloorSecs): above the floor
  // the declared rate holds exactly, below it the feed runs at the floor and says
  // so once, loudly. Silent clamping is the footgun this avoids; the alternative
  // (honoring the declaration) buys a rate nobody can see without a pop.
  bool sun_trigger = false;
  if (atmo->_iblSnapshotInterval > 0.0f) {
    bool interval_elapsed = state->snapshotIntervalElapsed(atmo->_iblSnapshotInterval);
    bool fade_settled     = state->fadeSettled();
    sun_trigger           = interval_elapsed and fade_settled;
    // the floor BINDING, caught at the only moment it is observable: the clock
    // says go and the fade says not yet. Once per state — the condition recurs on
    // every frame of every fade.
    if (interval_elapsed and (not fade_settled) and (not state->_interval_floor_warned)) {
      state->_interval_floor_warned = true;
      double floor_secs             = state->cadenceFloorSecs();
      double effective              = std::max(double(atmo->_iblSnapshotInterval), floor_secs);
      logchan_fwd_sub->log(
          "sky IBL declared snapshot interval %.3f s is BELOW the measured cycle+fade floor "
          "(>= %.3f s at %dx%d) - the feed runs at the FLOOR, not the declaration: effective "
          "interval %.3f s (%.3f Hz). Raise the interval above the floor or shrink the snapshot.",
          double(atmo->_iblSnapshotInterval),
          floor_secs,
          atmo->_iblSnapshotWidth,
          atmo->_iblSnapshotHeight,
          effective,
          (effective > 0.0) ? (1.0 / effective) : 0.0);
    }
  } else if (atmo->_iblContinuousChain) {
    float chain_floor = std::max(atmo->_iblChainMinAngleDeg, 1.0e-3f);
    sun_trigger       = state->fadeSettled()                        //
                  and (sun_delta_deg >= chain_floor)                //
                  and state->chainCadenceElapsed(atmo->_iblChainMaxHz);
  } else {
    sun_trigger = (sun_delta_deg > atmo->_iblRefilterAngleDeg);
  }
  // THE HAZE STAMP is an immediate trigger beside the medium's, and it has to be
  // its own: the artist layer is presentation tier (mediumHash never sees it, so
  // a look edit never re-bakes the transmittance / multi-scatter LUTs), but the
  // snapshot now bakes that layer into the sky it captures. Without this a haze
  // edit would only reach reflections and SH ambient once something ELSE started
  // a cycle — which under a still sun is never.
  bool trigger = (not state->_ever_snapped)                            //
                 or (medium_hash != state->_last_snapshot_medium_hash) //
                 or (haze_hash != state->_last_snapshot_haze_hash)     //
                 or sun_trigger;
  if (not trigger)
    return;

  ////////////////////////////////////////
  // cycle start: freeze the source, then hand it to the sliced prefilter
  ////////////////////////////////////////

  // CADENCE WITNESS. Rows are cleared every frame, so a row scoped to work that
  // only runs at cycle START makes its PRESENCE an exact per-frame witness of the
  // trigger — which the unconditional "sky-ibl" row around the call site cannot
  // be. Measures the render-thread cost of the snapshot render plus the refilter
  // kickoff, not the sliced filter work that follows on later frames.
  RenderPhaseScope _skysnap("sky-ibl-snap");

  auto context = _currentContext;
  _currentRCFD->_renderingmodel = "CUSTOM"_crcu;
  _currentRCFD->_subpassID      = "SKY_IBL_SNAPSHOT"_crcu;
  _hillaire_sky->renderEquirectSnapshot(
      context,                     //
      _currentRCFD,                //
      atmo,                        //
      skyframe->_dirToSun,         //
      skyframe->_viewAltitudeKm,   //
      skyframe->_dirToMoon,        //
      skyframe->_moonIlluminance); //

  if (nullptr == state->_maps)
    state->_maps = std::make_shared<RadianceMaps>();
  state->_snapshot_rtg = _hillaire_sky->_rtgEquirect;

  // W4-S8 — the sky SH probe reads THIS snapshot, but not on this frame: a
  // dispatch phase submits and waits its own command buffer ahead of the
  // frame's graphics submission, so a texture rendered above reads back black
  // (measured, on the cube path — same law here). Stamp the frame and let
  // _update_sky_sh pick it up on a later one.
  state->_sh_project_pending = true;
  state->_sh_projected_frame = _currentContext->GetTargetFrame();

  // Chained fades are sized to the span the last cycle actually took; the knob
  // is the pre-measurement seed (and, at 0, still the hard-swap opt-out). That
  // span is in FRAMES, which is not a duration — _iblCrossfadeMaxSecs is the
  // wall-clock bound the window also has to land inside, resolved here (once,
  // per cycle) so a mid-cycle edit cannot move the window a running cycle
  // publishes into.
  int fade_frames      = atmo->_iblContinuousChain //
                        ? state->autoFadeFrames(atmo->_iblCrossfadeFrames)
                        : atmo->_iblCrossfadeFrames;
  float fade_max_secs = std::max(atmo->_iblCrossfadeMaxSecs, 0.0f);

  // Past the first publish this job is RECURRING, not one-shot: the offscreen
  // settle/drain must read it as steady state, or a chaining feed pins the
  // pending-async census forever and a movie pre-roll never completes. The FIRST
  // cycle deliberately stays one-shot — that one IS appearance work a capture
  // should wait for.
  bool steady_state = state->_ready.load();

  // COLD START — the same _ready edge, a different contract: until the first
  // cycle publishes there are no maps to keep showing, so this bake is not
  // amortized, it is drained (the microtask drops its per-frame quota and its
  // budget gate). Every later cycle has a published set to sit on and pays the
  // normal paced cost. Nothing needs to be un-done when _ready flips: the flag
  // lives on the microtask instance this cycle builds, and cycle 2 is built
  // from a steady _ready with it false.
  bool cold_start = not steady_state;

  state->beginCycle();
  state->_last_snapshot_dir_to_sun = skyframe->_dirToSun;
  state->_last_snapshot_medium_hash = medium_hash;
  state->_last_snapshot_haze_hash   = haze_hash;
  state->_ever_snapped              = true;

  // the callback runs on THIS thread (render) after the swap's GPU UPLOADS have
  // completed, not merely after the swap call returned — publishStagedToTarget
  // defers it to the last upload's completion semaphore, so _generation only ever
  // announces maps the GPU can actually sample. Holding the state by shared_ptr
  // keeps it alive even if the scene drops its CommonStuff mid-cycle; the
  // context is captured raw because it is the one whose frame poll fires this
  // callback, and the fade window is captured by value so a mid-cycle knob edit
  // cannot change the window the running cycle publishes into.
  getRadianceMapCache()->refilterFromTexture(
      _hillaire_sky->equirectSnapshotTexture(), //
      state->_maps,                             //
      context,                                  //
      [state, context, fade_frames, fade_max_secs](datablock_ptr_t) {
        state->completeCycle(context, fade_frames, fade_max_secs);
      },
      atmo->_iblSpecularSamples, // COMFORT-1: the feed's own quality/cost trade,
                                 // NOT the baked path's bake-time count
      steady_state,
      // GRANULARITY, passed UNRESOLVED — 0 on any of them is the scene declaring
      // nothing, and the microtask ctor is the one place that turns that into the
      // env var or the shipped default. Read per cycle on purpose: an edit lands
      // on the next cycle, never mid-plan.
      atmo->_iblLevelBatches,
      atmo->_iblSlicesPerFrame,
      atmo->_iblMipChainBudgetPx,
      cold_start);

  logchan_fwd_sub->log(
      "sky IBL refilter cycle %llu started (sun moved %.2f deg, fade %d frames / %.3f s max, last cycle %d frames)",
      (unsigned long long)state->_cycles_started.load(),
      sun_delta_deg,
      fade_frames,
      double(fade_max_secs),
      state->_cycle_frames.load());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// W4-S8 — the sky's DIFFUSE ambient, as an L2 spherical-harmonic probe fed by
// the same equirect snapshot the specular refilter consumes.
//
// Two steps, both cheap and both at most once per IBL cycle:
//
//  PROJECT  once the pending snapshot is a frame old, run ProbeSHProjector over
//           it and read the nine coefficients back. The capture PRE-SCALE is
//           divided out inside the kernel (S7's one decode seam), so what lands
//           in _sh_staged is radiance in scene units.
//
//  PROMOTE  on the frame the cycle PUBLISHES, move staged -> current and the old
//           current -> prev. That is the same instant the prefiltered maps swap
//           and the crossfade window opens, so ONE EnvBlendWeight fades both and
//           the diffuse ambient can never disagree with the specular about which
//           sky it is looking at.
//
// The FIRST promotion seeds prev from cur: a fade out of all-black coefficients
// would darken the first lit frames of every scene.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_update_sky_sh() {
  auto pbrcommon = _node->_pbrcommon;
  if (nullptr == pbrcommon)
    return;
  auto state = pbrcommon->_sky_ibl;
  auto atmo  = pbrcommon->_atmosphere;
  if ((nullptr == state) or (nullptr == atmo))
    return;

  if (state->_sh_project_pending                                       //
      and state->_snapshot_rtg                                         //
      and (_currentContext->GetTargetFrame() > state->_sh_projected_frame)) {

    auto snapshot = state->_snapshot_rtg->texture(0);
    OrkAssertI(
        snapshot != nullptr,
        "sky SH probe: the frozen snapshot RTG has no texture - the equirect render did not realize it");

    if (nullptr == _probeSH)
      _probeSH = std::make_shared<ProbeSHProjector>();
    if (_skySHSlot < 0)
      _skySHSlot = _probeSHSlotCounter++;

    // the ENCODE side of this is one bind in renderEquirectSnapshot; this is the
    // matching decode and the only one the coefficients ever get.
    float capture_scale = atmo->_iblCaptureScale;
    float decode        = (capture_scale > 0.0f) ? (1.0f / capture_scale) : 1.0f;

    _probeSH->projectEquirect(
        _currentContext,               //
        snapshot,                      //
        state->_snapshot_rtg->width(), //
        state->_snapshot_rtg->height(),
        _skySHSlot,
        decode);

    fvec3 coeffs[kProbeSHCoeffs];
    bool ok = _probeSH->readback(_currentContext, _skySHSlot, coeffs);
    OrkAssertI(ok, "sky SH probe: the coefficient readback failed after a completed projection");
    for (int i = 0; i < kProbeSHCoeffs; i++)
      state->_sh_staged[i] = fvec4(coeffs[i], 0.0f);
    state->_sh_staged_valid    = true;
    state->_sh_project_pending = false;
  }

  uint64_t gen = state->_generation.load();
  if (state->_sh_staged_valid and (gen != state->_sh_published_gen)) {
    for (int i = 0; i < kProbeSHCoeffs; i++) {
      state->_sh_prev[i] = state->_sh_valid ? state->_sh_cur[i] : state->_sh_staged[i];
      state->_sh_cur[i]  = state->_sh_staged[i];
    }
    state->_sh_valid         = true;
    state->_sh_staged_valid  = false;
    state->_sh_published_gen = gen;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_initProbeBlitMaterial(Context* ctx) {
  if (_probeBlitInitDone) return;
  _probeBlitMtl.gpuInit(ctx, "orkshader://blit");
  _probeBlitMtl._rasterstate->setCullTest(ECullTest::OFF);
  _tek_probe_blit     = _probeBlitMtl.technique("blit");
  _tek_probe_ds[0]    = _probeBlitMtl.technique("blit");
  _tek_probe_ds[1]    = _probeBlitMtl.technique("downsample_2x2");
  _tek_probe_ds[2]    = _probeBlitMtl.technique("downsample_3x3");
  _tek_probe_ds[3]    = _probeBlitMtl.technique("downsample_4x4");
  _tek_probe_ds[4]    = _probeBlitMtl.technique("downsample_5x5");
  _tek_probe_ds[5]    = _probeBlitMtl.technique("downsample_6x6");
  _tek_probe_ds[6]    = _probeBlitMtl.technique("downsample_7x7");
  _tek_probe_temporal = _probeBlitMtl.technique("tek_temporal_blend");
  _par_probe_colormap    = _probeBlitMtl.param("ColorMap");
  _par_probe_accummap    = _probeBlitMtl.param("AccumMap");
  _par_probe_blendweight = _probeBlitMtl.param("BlendWeight");
  _par_probe_mvp         = _probeBlitMtl.param("MatMVP");
  _par_probe_vpdim       = _probeBlitMtl.param("ViewportDim");
  _par_probe_flipy       = _probeBlitMtl.param("FlipY");
  _par_probe_flipx       = _probeBlitMtl.param("FlipX");
  _probeBlitInitDone = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_renderProbeWithSSAA(
    lightprobe_ptr_t probe,
    CompositorDrawData& drawdata,
    CompositingPassData& cubemapCPD) {

  auto TXI = _currentContext->TXI();
  auto FBI = _currentContext->FBI();
  auto DWI = _currentContext->DWI();
  auto topcomp = _currentRCFD->topCompositor();

  int ss = probe->supersample();
  int ssaa_dim = probe->dim() * (ss + 1);

  // Ensure SSAA render RTG exists at upscaled resolution
  if (!probe->_ssaaRenderRTG || probe->_ssaaRenderRTG->width() != ssaa_dim) {
    probe->_ssaaRenderRTG = std::make_shared<RtGroup>(_currentContext, ssaa_dim, ssaa_dim);
    probe->_ssaaRenderRTG->_name = "ProbeSSAA_Render";
    auto colorbuf = probe->_ssaaRenderRTG->createRenderTarget(EBufferFormat::RGBA8);
    colorbuf->_debugName = "ProbeSSAA_Color";
    probe->_ssaaRenderRTG->createDepthBuffer(EBufferFormat::Z32F, true);
  }

  for (int iface = 0; iface < 6; iface++) {
    _currentContext->debugPushGroup(FormatString("ForwardPBR::cubemap SSAA pass<%d>", iface));

    _setupCubeFaceCamera(probe, iface);

    // Step 1: Render at upscaled resolution into _ssaaRenderRTG
    auto probe_pass = std::make_shared<ForwardPass>();
    probe_pass->_drawdata = &drawdata;
    probe_pass->_rtg_out = probe->_ssaaRenderRTG;
    probe_pass->_rtg_depth_copy = _rtg_cube1_depth_copy;
    probe_pass->_renderingPROBE = true;
    probe_pass->_fwd_pass_layer = probe->renderLayer();
    probe_pass->_single_pass_stereo = false;

    cubemapCPD._mono_cam_matrices = _CUBECAM;
    _currentRCFD->_passID = "PROBE"_crcu;

    topcomp->pushCPD(cubemapCPD);
    _render_dppskyssaocolor(probe_pass);
    topcomp->popCPD();

    // Step 2: Downsample _ssaaRenderRTG -> cubeRenderRTG[iface]
    probe->_cubeRenderRTG->_cubeRenderFace = iface;
    probe->_cubeRenderRTG->_autoclear = false;
    FBI->PushRtGroup(probe->_cubeRenderRTG.get());

    auto tex = probe->_ssaaRenderRTG->buffer(0)->texture();
    auto& mtl = _probeBlitMtl;
    mtl._rasterstate->_force = true;
    mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
    mtl._rasterstate->setDepthTest(EDepthTest::OFF);
    mtl._rasterstate->setCullTest(ECullTest::OFF);
    mtl.begin(_tek_probe_ds[ss], _currentRCFD);
    mtl.bindParamTexture(_par_probe_colormap, tex);
    mtl.bindParamMatrix(_par_probe_mvp, fmtx4::Identity());
    mtl.bindParamVec2(_par_probe_vpdim, fvec2(float(probe->dim()), float(probe->dim())));
    mtl.bindParamInt(_par_probe_flipy, 0);
    mtl.bindParamInt(_par_probe_flipx, 0);
    ViewportRect extents(0, 0, probe->dim(), probe->dim());
    FBI->pushViewport(extents);
    FBI->pushScissor(extents);
    DWI->fullscreenQuad();
    FBI->popViewport();
    FBI->popScissor();
    mtl.end(_currentRCFD);

    FBI->PopRtGroup();
    _currentContext->debugPopGroup();
  }

  probe->_cubeTexture = probe->_cubeRenderRTG->texture(0);
  TXI->generateMipMaps(probe->_cubeTexture.get());
  probe->_dirty = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_renderProbeWithTAA(
    lightprobe_ptr_t probe,
    CompositorDrawData& drawdata,
    CompositingPassData& cubemapCPD) {

  auto TXI = _currentContext->TXI();
  auto FBI = _currentContext->FBI();
  auto DWI = _currentContext->DWI();
  auto topcomp = _currentRCFD->topCompositor();

  int dim = probe->dim();
  bool doSSAA = (probe->supersample() > 0);
  int ss = probe->supersample();

  // Ensure SSAA RTG if needed
  if (doSSAA) {
    int ssaa_dim = dim * (ss + 1);
    if (!probe->_ssaaRenderRTG || probe->_ssaaRenderRTG->width() != ssaa_dim) {
      probe->_ssaaRenderRTG = std::make_shared<RtGroup>(_currentContext, ssaa_dim, ssaa_dim);
      probe->_ssaaRenderRTG->_name = "ProbeSSAA_Render";
      auto colorbuf = probe->_ssaaRenderRTG->createRenderTarget(EBufferFormat::RGBA8);
      colorbuf->_debugName = "ProbeSSAA_Color";
      probe->_ssaaRenderRTG->createDepthBuffer(EBufferFormat::Z32F, true);
    }
  }

  // Ensure temp face RTG for current-frame face result
  if (!probe->_tempFaceRTG || probe->_tempFaceRTG->width() != dim) {
    probe->_tempFaceRTG = std::make_shared<RtGroup>(_currentContext, dim, dim);
    probe->_tempFaceRTG->_name = "ProbeTAA_TempFace";
    auto buf = probe->_tempFaceRTG->createRenderTarget(EBufferFormat::RGBA32F);
    buf->_debugName = "ProbeTAA_TempFaceColor";
    probe->_tempFaceRTG->createDepthBuffer(EBufferFormat::Z32F, true);
    probe->_tempFaceRTG->_autoclear = false;
  }

  // Ensure per-face accumulation RTGs
  for (int f = 0; f < 6; f++) {
    for (int pp = 0; pp < 2; pp++) {
      if (!probe->_accumFaceRTG[f][pp] || probe->_accumFaceRTG[f][pp]->width() != dim) {
        probe->_accumFaceRTG[f][pp] = std::make_shared<RtGroup>(_currentContext, dim, dim);
        probe->_accumFaceRTG[f][pp]->_name = FormatString("ProbeTAA_Accum_f%d_pp%d", f, pp);
        auto buf = probe->_accumFaceRTG[f][pp]->createRenderTarget(EBufferFormat::RGBA32F);
        buf->_debugName = FormatString("ProbeTAA_AccumColor_f%d_pp%d", f, pp);
        probe->_accumFaceRTG[f][pp]->_autoclear = false;
      }
    }
  }

  int writeIdx = probe->_accumWriteIdx;
  int readIdx = writeIdx ^ 1;
  float weight = 1.0f / float(std::min(probe->_accumFrameCount + 1, probe->temporalFrames()));

  for (int iface = 0; iface < 6; iface++) {
    _currentContext->debugPushGroup(FormatString("ForwardPBR::cubemap TAA pass<%d> frame<%d>", iface, probe->_accumFrameCount));

    _setupCubeFaceCamera(probe, iface);

    // Step 1: Render current face
    if (doSSAA) {
      // Render at upscaled resolution
      auto probe_pass = std::make_shared<ForwardPass>();
      probe_pass->_drawdata = &drawdata;
      probe_pass->_rtg_out = probe->_ssaaRenderRTG;
      probe_pass->_rtg_depth_copy = _rtg_cube1_depth_copy;
      probe_pass->_renderingPROBE = true;
      probe_pass->_fwd_pass_layer = probe->renderLayer();
      probe_pass->_single_pass_stereo = false;

      cubemapCPD._mono_cam_matrices = _CUBECAM;
      _currentRCFD->_passID = "PROBE"_crcu;
      topcomp->pushCPD(cubemapCPD);
      _render_dppskyssaocolor(probe_pass);
      topcomp->popCPD();

      // Downsample into _tempFaceRTG
      FBI->PushRtGroup(probe->_tempFaceRTG.get());
      auto tex = probe->_ssaaRenderRTG->buffer(0)->texture();
      auto& mtl = _probeBlitMtl;
      mtl._rasterstate->_force = true;
      mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
      mtl._rasterstate->setDepthTest(EDepthTest::OFF);
      mtl._rasterstate->setCullTest(ECullTest::OFF);
      mtl.begin(_tek_probe_ds[ss], _currentRCFD);
      mtl.bindParamTexture(_par_probe_colormap, tex);
      mtl.bindParamMatrix(_par_probe_mvp, fmtx4::Identity());
      mtl.bindParamVec2(_par_probe_vpdim, fvec2(float(dim), float(dim)));
      bool isTopBottom = (iface == 2 || iface == 3);
      mtl.bindParamInt(_par_probe_flipy, isTopBottom ? 0 : 1);
      mtl.bindParamInt(_par_probe_flipx, isTopBottom ? 1 : 0);
      ViewportRect extents(0, 0, dim, dim);
      FBI->pushViewport(extents);
      FBI->pushScissor(extents);
      DWI->fullscreenQuad();
      FBI->popViewport();
      FBI->popScissor();
      mtl.end(_currentRCFD);
      FBI->PopRtGroup();
    } else {
      // Render directly into _tempFaceRTG at native resolution
      auto probe_pass = std::make_shared<ForwardPass>();
      probe_pass->_drawdata = &drawdata;
      probe_pass->_rtg_out = probe->_tempFaceRTG;
      probe_pass->_rtg_depth_copy = _rtg_cube1_depth_copy;
      probe_pass->_renderingPROBE = true;
      probe_pass->_fwd_pass_layer = probe->renderLayer();
      probe_pass->_single_pass_stereo = false;

      cubemapCPD._mono_cam_matrices = _CUBECAM;
      _currentRCFD->_passID = "PROBE"_crcu;
      topcomp->pushCPD(cubemapCPD);
      _render_dppskyssaocolor(probe_pass);
      topcomp->popCPD();
    }

    // Step 2: Temporal blend
    auto temp_tex = probe->_tempFaceRTG->buffer(0)->texture();
    ViewportRect extents(0, 0, dim, dim);

    if (probe->_accumFrameCount == 0) {
      // First frame: just copy temp to accum[write]
      FBI->PushRtGroup(probe->_accumFaceRTG[iface][writeIdx].get());
      auto& mtl = _probeBlitMtl;
      mtl._rasterstate->_force = true;
      mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
      mtl._rasterstate->setDepthTest(EDepthTest::OFF);
      mtl._rasterstate->setCullTest(ECullTest::OFF);
      mtl.begin(_tek_probe_blit, _currentRCFD);
      mtl.bindParamTexture(_par_probe_colormap, temp_tex);
      mtl.bindParamMatrix(_par_probe_mvp, fmtx4::Identity());
      mtl.bindParamVec2(_par_probe_vpdim, fvec2(float(dim), float(dim)));
      mtl.bindParamInt(_par_probe_flipy, 0);
      mtl.bindParamInt(_par_probe_flipx, 0);
      FBI->pushViewport(extents);
      FBI->pushScissor(extents);
      DWI->fullscreenQuad();
      FBI->popViewport();
      FBI->popScissor();
      mtl.end(_currentRCFD);
      FBI->PopRtGroup();
    } else {
      // Blend temp + accum[read] -> accum[write]
      auto accum_read_tex = probe->_accumFaceRTG[iface][readIdx]->buffer(0)->texture();
      FBI->PushRtGroup(probe->_accumFaceRTG[iface][writeIdx].get());
      auto& mtl = _probeBlitMtl;
      mtl._rasterstate->_force = true;
      mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
      mtl._rasterstate->setDepthTest(EDepthTest::OFF);
      mtl._rasterstate->setCullTest(ECullTest::OFF);
      mtl.begin(_tek_probe_temporal, _currentRCFD);
      mtl.bindParamTexture(_par_probe_colormap, temp_tex);
      mtl.bindParamTexture(_par_probe_accummap, accum_read_tex);
      mtl.bindParamMatrix(_par_probe_mvp, fmtx4::Identity());
      mtl.bindParamFloat(_par_probe_blendweight, weight);
      FBI->pushViewport(extents);
      FBI->pushScissor(extents);
      DWI->fullscreenQuad();
      FBI->popViewport();
      FBI->popScissor();
      mtl.end(_currentRCFD);
      FBI->PopRtGroup();
    }

    // Step 3: Copy accum[write] -> cubeRenderRTG[iface]
    auto accum_result_tex = probe->_accumFaceRTG[iface][writeIdx]->buffer(0)->texture();
    probe->_cubeRenderRTG->_cubeRenderFace = iface;
    probe->_cubeRenderRTG->_autoclear = false;
    FBI->PushRtGroup(probe->_cubeRenderRTG.get());
    {
      auto& mtl = _probeBlitMtl;
      mtl._rasterstate->_force = true;
      mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
      mtl._rasterstate->setDepthTest(EDepthTest::OFF);
      mtl._rasterstate->setCullTest(ECullTest::OFF);
      mtl.begin(_tek_probe_blit, _currentRCFD);
      mtl.bindParamTexture(_par_probe_colormap, accum_result_tex);
      mtl.bindParamMatrix(_par_probe_mvp, fmtx4::Identity());
      mtl.bindParamVec2(_par_probe_vpdim, fvec2(float(dim), float(dim)));
      mtl.bindParamInt(_par_probe_flipy, 0);
      mtl.bindParamInt(_par_probe_flipx, 0);
      FBI->pushViewport(extents);
      FBI->pushScissor(extents);
      DWI->fullscreenQuad();
      FBI->popViewport();
      FBI->popScissor();
      mtl.end(_currentRCFD);
    }
    FBI->PopRtGroup();

    _currentContext->debugPopGroup();
  }

  // After all 6 faces: swap ping-pong and advance frame count
  probe->_accumWriteIdx ^= 1;
  probe->_accumFrameCount++;

  if (probe->_accumFrameCount >= probe->temporalFrames()) {
    // Converged
    probe->_cubeTexture = probe->_cubeRenderRTG->texture(0);
    TXI->generateMipMaps(probe->_cubeTexture.get());
    probe->_dirty = false;
    probe->_accumFrameCount = 0;

    // Release temporary buffers
    for (int f = 0; f < 6; f++) {
      probe->_accumFaceRTG[f][0] = nullptr;
      probe->_accumFaceRTG[f][1] = nullptr;
    }
    probe->_tempFaceRTG = nullptr;
    probe->_ssaaRenderRTG = nullptr;
  } else {
    // Intermediate: update cube texture for live display but stay dirty
    probe->_cubeTexture = probe->_cubeRenderRTG->texture(0);
    TXI->generateMipMaps(probe->_cubeTexture.get());
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Color pass
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_colorpass(forward_pass_ptr_t fpass) {

  auto drawdata  = fpass->_drawdata;
  auto rtg_out   = fpass->_rtg_out;
  auto FBI       = _currentContext->FBI();
  auto GBI       = _currentContext->GBI();
  auto pbrcommon = _node->_pbrcommon;

  //printf("render colorpass rtg<%p>\n", (void*)rtg_out.get());

  _currentContext->debugMarker("ForwardPBR::renderEnqueuedScene::layer<std_forward>");
  _currentDrawQueue->enqueueLayerToRenderQueue(fpass->_fwd_pass_layer, _currentIRenderer);

  _currentRCFD->_renderingmodel = "FORWARD_PBR"_crcu;
  _currentRCFD->_subpassID      = "COLOR"_crcu;
  auto autorelease_dbg_group    = _currentContext->debugPushGroupAutoRelease("ForwardPBR::color pass");
  _currentIRenderer->_debugLog  = false;

  ////////////////////////////////
  // DRAW-LAST SPLIT. Renderables at IRenderable::kLastRenderableSortKey (the grass carpet)
  // are held back from this segment and re-issued by _render_drawlast into a depth-WRITABLE
  // continuation of the same pass: the color pass proper runs with the depth attachment in
  // DEPTH_READ_ONLY_OPTIMAL (that is what lets in-pass DEPTH_MAP samplers read the prepass
  // depth), and a read-only depth attachment forbids depth writes for EVERY draw in it.
  // reset_after stays false here — the tail segment owns the queue reset, and resetting
  // between the two would throw the held-back renderables away.
  // PROBE captures keep the single-segment form: their RTG is a cube face in the middle of
  // its face setup, so a pop/push there buys nothing a reflection bake can see.
  ////////////////////////////////

  bool have_drawlast = (not fpass->_renderingPROBE) //
                       and (_currentIRenderer->countEnqueuedAtOrAboveSortKey(
                                IRenderable::kLastRenderableSortKey) > 0);

  if (have_drawlast) {
    _currentIRenderer->drawEnqueuedRenderables(false, 0, IRenderable::kLastRenderableSortKey - 1);
    _render_drawlast(fpass);
  } else {
    _currentIRenderer->drawEnqueuedRenderables(true);
  }

  ////////////////////////////////
  // QUARTER-RES SUN SHAFTS, half two: subtract the bilateral-upsampled shadow
  // loss from the finished OPAQUE image. Here, and not earlier, because the
  // draw-last segment above is opaque geometry (the grass carpet) that wants
  // its shafts; and not later, because transparent surfaces must blend over an
  // already-corrected image rather than be darkened by a loss marched for the
  // opaque behind them. No-op in every mode but 2.
  ////////////////////////////////

  _composite_hazeshaft(fpass);

  // PBR2 Phase 2 — std_transparent layer. Drawn after opaques so the
  // (eventual P2.7) transmission lobe can sample the opaque framebuffer
  // backbuffer pre-overlay. Skipped during probe captures (refractive
  // lobes short-circuit on rendering_probe anyway). Empty by default —
  // materials opt in by being placed on this layer.
  if (not fpass->_renderingPROBE) {
    _currentContext->debugMarker("ForwardPBR::renderEnqueuedScene::layer<std_transparent>");
    _currentDrawQueue->enqueueLayerToRenderQueue("std_transparent", _currentIRenderer);
    _currentIRenderer->drawEnqueuedRenderables(true);
  }

  // GENERIC AUX CHANNELS (E2B item D) — one extra pass per declared channel:
  // render layer "aux_<name>" into the channel's own RTG (RGBA16F, primary
  // dims, NO depth attachment — v1 limit: aux content is not occluded by
  // scene geometry). The RCFD subpass marker + AUX_CHANNEL property select
  // each material's aux technique pair (MaterialBase::pipeline returns
  // nullptr for non-participants — renderers skip). The RTG is published
  // into the drawdata properties under crc("aux_<name>") for postfx
  // consumption (PostFxNodeHeatDistort et al). Skipped in probe captures.
  if (not fpass->_renderingPROBE) {
    for (const auto& chan : _node->_auxChannels) {
      std::string layer_name = "aux_" + chan;
      auto& aux_rtg          = _aux_rtgs[chan];
      if (nullptr == aux_rtg) {
        aux_rtg  = std::make_shared<RtGroup>(
            _currentContext, rtg_out->width(), rtg_out->height(), MsaaSamples::MSAA_1X);
        auto buf = aux_rtg->createRenderTarget(EBufferFormat::RGBA16F);
        buf->_debugName = FormatString("FwdAux<%s>", chan.c_str());
      }
      aux_rtg->Resize(rtg_out->width(), rtg_out->height());
      uint64_t chan_key = CrcString(("aux_" + chan).c_str()).hashed();
      drawdata->_properties[chan_key].set<rtgroup_ptr_t>(aux_rtg);
      _currentContext->debugMarker(FormatString("ForwardPBR::renderEnqueuedScene::layer<%s>", layer_name.c_str()));
      _currentRCFD->_subpassID = "AUX"_crcu;
      _currentRCFD->setUserProperty("AUX_CHANNEL"_crcu, uint64_t(CrcString(chan.c_str()).hashed()));
      FBI->PushRtGroup(aux_rtg.get());
      _currentDrawQueue->enqueueLayerToRenderQueue(layer_name, _currentIRenderer);
      _currentIRenderer->drawEnqueuedRenderables(true);
      FBI->PopRtGroup();
      _currentRCFD->unSetUserProperty("AUX_CHANNEL"_crcu);
      _currentRCFD->_subpassID = "COLOR"_crcu;
    }
  }

  // Overlay post-pass — std_editor (gizmos/manipulators) and
  // hud_overlay (UI text, debug HUD). Drawn AFTER the scene color
  // pass so they composite on top of world geometry, and skipped
  // entirely when rendering into a probe cube — both layers are
  // screen-space UI, not world content that should appear in
  // reflection bakes. std_editor is gated by _enableEditorLayers
  // (only the editor sets that); hud_overlay is unconditional
  // because the layer is empty in scenes that don't use it, so
  // enqueue is a no-op.
  if (not fpass->_renderingPROBE) {
    bool have_overlay = false;
    if (_currentDrawQueue->_enableEditorLayers) {
      _currentContext->debugMarker("ForwardPBR::renderEnqueuedScene::layer<std_editor>");
      _currentDrawQueue->enqueueLayerToRenderQueue("std_editor", _currentIRenderer);
      have_overlay = true;
    }
    _currentContext->debugMarker("ForwardPBR::renderEnqueuedScene::layer<hud_overlay>");
    _currentDrawQueue->enqueueLayerToRenderQueue("hud_overlay", _currentIRenderer);
    have_overlay = true;
    if (have_overlay) {
      _currentIRenderer->drawEnqueuedRenderables(true);
    }
  }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw-last segment — the tail of the color pass, run with a WRITABLE depth attachment.
//
// Everything the color pass draws before this point runs against depth in
// DEPTH_READ_ONLY_OPTIMAL, so no draw in it can write depth. The carpet has to: it is a
// dense, per-blade surface that must occlude (and be occluded by) the world it sits in.
// Rather than make the whole color pass writable — which would re-arm the depth CLEAR
// (loadOp follows _autoclear AND not read-only) and discard the prepass, and would strand
// the in-pass DEPTH_MAP samplers with an attachment they may not read — this ends the
// read-only segment and re-enters the same RTG once more with depth writable.
//
// SPVR needs no separate arrangement: the multiview viewMask lives on the RTG, so the
// re-entered pass broadcasts to both eye layers exactly like the segment it continues.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ForwardPbrNodeImpl::_render_drawlast(forward_pass_ptr_t fpass) {

  auto rtg_out = fpass->_rtg_out;
  auto FBI     = _currentContext->FBI();

  auto autorelease_dbg_group = _currentContext->debugPushGroupAutoRelease("ForwardPBR::draw-last");

  // The segment CONTINUES a pass whose color the frame already depends on. _autoclear is
  // re-read at every push (VkRtGroupImpl::_updateClearParams), so leaving the frame's value
  // set would clear the skybox and every opaque draw back out at the re-entry.
  bool saved_autoclear = rtg_out->_autoclear;
  rtg_out->_autoclear  = false;

  // PopRtGroup on a "user" RTG also clears _depthReadOnlyMode, so the push below transitions
  // depth back to an attachment layout on its own; the explicit call states the requirement
  // rather than leaning on that side effect.
  FBI->PopRtGroup();
  FBI->transitionDepthForWriting(rtg_out);
  FBI->PushRtGroup(rtg_out.get());
  _currentIRenderer->drawEnqueuedRenderables(true, IRenderable::kLastRenderableSortKey);
  FBI->PopRtGroup();

  // Hand the caller back the pass it pushed, in the state it pushed it: the segments after
  // this one (std_transparent, aux channels, overlays) see read-only depth exactly as before.
  FBI->transitionDepthForSampling(rtg_out);
  FBI->PushRtGroup(rtg_out.get());
  rtg_out->_autoclear = saved_autoclear;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
