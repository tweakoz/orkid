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

namespace ork::lev2::pbr {

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
  auto FXI      = context->FXI();
  auto GBI      = context->GBI();
  auto DWI      = context->DWI();

  context->debugPushGroup("ForwardPBR::skybox pass");
  //printf("render skybox rtg<%p>\n", (void*)rtg_out.get());

  RCFD->_renderingmodel = "CUSTOM"_crcu;
  RCFD->_subpassID      = "SKYBOX"_crcu;
  RenderContextInstData RCID(RCFD);
  RCID._pipeline_cache = _skybox_fxcache;
  auto pipeline        = _skybox_fxcache->findPipeline(RCID);
  rtg_out->_autoclear = true;

  pipeline->_rasterstate->setWriteMaskZ(true);
  pipeline->_rasterstate->setWriteMaskRGB(true);
  pipeline->_rasterstate->setWriteMaskA(true);
  pipeline->_rasterstate->setDepthTest(EDepthTest::OFF);
  pipeline->_rasterstate->setCullTest(ECullTest::OFF);
  pipeline->bindUniformBuffer(_par_ublk_std_matrices, "ub_skybox"_crcu);
  pipeline->wrappedDrawCall(RCID, [=]() {
    FXI->applyRasterState(*pipeline->_rasterstate);
    DWI->fullscreenQuad(
        fvec4(0, 1, 1, -1),   // uv0 (x,y,w,h)
        fvec4(0, 1, 1, -1),   // uv1 (x,y,w,h)
        0.9999f);            // full screen quad
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
        // Enqueue every layer that plays the "depth_prepass" role for
        // this scene. With no role override, iterates a single-element
        // {"depth_prepass"} list — identical to the previous code path.
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

        auto FBI = _currentContext->FBI();
        FBI->PushRtGroup(light->_depthRTG.get());
        topcomp->pushCPD(shadowCPD);

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

  _currentIRenderer->drawEnqueuedRenderables(true);

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
} // namespace ork::lev2::pbr
