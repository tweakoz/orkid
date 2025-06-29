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

void ForwardPbrNodeImpl::_render_dpp(forward_pass_ptr_t fpass) {
  auto drawdata = fpass->_drawdata;
  auto rtg_out  = fpass->_rtg_out;
  auto FBI = _currentContext->FBI();

  //printf("render dppass rtg<%p>\n", (void*)rtg_out.get());

  _currentDrawQueue->enqueueLayerToRenderQueue(fpass->_dpp_pass_layer, _currentIRenderer);

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

  context->debugPushGroup("ForwardPBR::skybox pass");
  //printf("render skybox rtg<%p>\n", (void*)rtg_out.get());

  RCFD->_renderingmodel = "CUSTOM"_crcu;
  RCFD->_subpassID      = "SKYBOX"_crcu;
  RenderContextInstData RCID(RCFD);
  RCID._pipeline_cache = _skybox_fxcache;
  auto pipeline        = _skybox_fxcache->findPipeline(RCID);
  FBI->PushRtGroup(rtg_out.get());
  pipeline->_rasterstate->setWriteMaskZ(true);
  pipeline->_rasterstate->setWriteMaskRGB(true);
  pipeline->_rasterstate->setWriteMaskA(true);
  pipeline->_rasterstate->setDepthTest(EDepthTest::OFF);
  pipeline->bindUniformBuffer(_par_ublk_std_matrices, "ub_skybox"_crcu);
  pipeline->wrappedDrawCall(RCID, [=]() {
    FXI->applyRasterState(*pipeline->_rasterstate);
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

          probe->_cubeTexture = probe->_cubeRenderRTG->texture(0);
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

void ForwardPbrNodeImpl::_update_shadow_maps() {
  if (_enumeratedLights) {
    auto topcomp                  = _currentRCFD->topCompositor();
    _currentRCFD->_renderingmodel = "DEPTH_PREPASS"_crcu;
    _currentRCFD->_passID         = "SHADOW"_crcu;
    int num_shadow_casters        = 0;
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

  FBI->PushRtGroup(rtg_out.get());
  _currentIRenderer->drawEnqueuedRenderables(true);
  FBI->PopRtGroup();

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
