////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/kernel/datacache.h>
#include <ork/gfx/brdf.inl>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>

#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_light_processor_simple.h>
#include <ork/profiling.inl>

////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr::deferrednode {
////////////////////////////////////////////////////////////////

SimpleLightProcessor::SimpleLightProcessor(DeferredContext& defctx, DeferredCompositingNodePbr* compnode)
    : _deferredContext(defctx)
    , _defcompnode(compnode) {
}
void SimpleLightProcessor::_gpuInit(lev2::Context* target) {
  if (nullptr == _lightbuffer) {
    _lightbuffer = target->FXI()->createParamBuffer(65536);
    auto mapped  = target->FXI()->mapParamBuffer(_lightbuffer);
    size_t base  = 0;
    for (int i = 0; i < KMAXLIGHTSPERCHUNK; i++)
      mapped->ref<fvec3>(base + i * sizeof(fvec4)) = fvec3(0, 0, 0);
    base += KMAXLIGHTSPERCHUNK * sizeof(fvec4);
    for (int i = 0; i < KMAXLIGHTSPERCHUNK; i++)
      mapped->ref<fvec4>(base + i * sizeof(fvec4)) = fvec4();
    mapped->unmap();
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::gpuUpdate(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights) {
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto context                 = drawdata.context();
  _gpuInit(context);

  /////////////////////////////////////
  // convert enumerated scenelights to deferred format
  // in a "real" light processor we will figure out how to
  //  update items incrementally
  /////////////////////////////////////

  //const auto& scene_lights = enumlights._enumeratedLights;


}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::renderDecals(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights) {
  _renderTexturedSpotDecals(drawdata, VD, enumlights);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::renderLights(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights) {
  _renderUnshadowedUntexturedPointLights(drawdata, VD, enumlights);
  _renderUnshadowedTexturedPointLights(drawdata, VD, enumlights);
  _renderUnshadowedTexturedSpotLights(drawdata, VD, enumlights);
  _renderShadowedTexturedSpotLights(drawdata, VD, enumlights);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_updatePointLightUBOparams(Context* ctx, const pointlightlist_t& lights, fvec3 campos) {
  auto FXI           = ctx->FXI();
  size_t offset_cd   = 0;
  size_t offset_mtx  = offset_cd + KMAXLIGHTSPERCHUNK * sizeof(fvec4);
  size_t offset_mtx2 = offset_mtx + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
  size_t offset_rad  = offset_mtx2 + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
  size_t numlights   = lights.size();
  OrkAssert(numlights < KMAXLIGHTSPERCHUNK);
  ctx->debugPushGroup("SimpleLightProcessor::_updatePointLightUBOparams");
  auto mapping = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
  for (auto light : lights) {
    fvec3 color                     = light->color()*light->intensity();
    float dist2cam                  = light->distance(campos);
    mapping->ref<fvec4>(offset_cd)  = fvec4(color, dist2cam);
    mapping->ref<float>(offset_rad) = light->radius();
    mapping->ref<fmtx4>(offset_mtx) = light->worldMatrix();
    offset_cd += sizeof(fvec4);
    offset_mtx += sizeof(fmtx4);
    offset_mtx2 += sizeof(fmtx4);
    offset_rad += sizeof(float);
  }
  FXI->unmapParamBuffer(mapping.get());
  FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
  _deferredContext._lightingmtl.bindParamFloat(_deferredContext._parDepthFogDistance, 1.0f / _deferredContext._depthFogDistance);
  _deferredContext._lightingmtl.bindParamFloat(_deferredContext._parDepthFogPower, _deferredContext._depthFogPower);
  _deferredContext._lightingmtl.bindParamInt(_deferredContext._parNumLights, numlights);
  _deferredContext._lightingmtl.commit();
  ctx->debugPopGroup();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_updateSpotLightUBOparams(Context* ctx, const spotlightlist_t& lights, fvec3 campos) {
  auto FXI           = ctx->FXI();
  size_t offset_cd   = 0;
  size_t offset_mtx  = offset_cd + KMAXLIGHTSPERCHUNK * sizeof(fvec4);
  size_t offset_mtx2 = offset_mtx + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
  size_t offset_rad  = offset_mtx2 + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
  size_t numlights   = lights.size();
  OrkAssert(numlights < KMAXLIGHTSPERCHUNK);
  ctx->debugPushGroup("SimpleLightProcessor::_updateSpotLightUBOparams");
  auto mapping = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
  for (auto light : lights) {
    fvec3 color                      = light->color()*light->intensity();
    float dist2cam                   = light->distance(campos);
    mapping->ref<fvec4>(offset_cd)   = fvec4(color, dist2cam);
    mapping->ref<float>(offset_rad)  = light->GetRange();
    mapping->ref<fmtx4>(offset_mtx)  = light->worldMatrix();
    mapping->ref<fmtx4>(offset_mtx2) = light->shadowMatrix();
    offset_cd += sizeof(fvec4);
    offset_mtx += sizeof(fmtx4);
    offset_mtx2 += sizeof(fmtx4);
    offset_rad += sizeof(float);
  }
  FXI->unmapParamBuffer(mapping.get());
  FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
  _deferredContext._lightingmtl.bindParamFloat(_deferredContext._parDepthFogDistance, 1.0f / _deferredContext._depthFogDistance);
  _deferredContext._lightingmtl.bindParamFloat(_deferredContext._parDepthFogPower, _deferredContext._depthFogPower);
  _deferredContext._lightingmtl.bindParamInt(_deferredContext._parNumLights, numlights);
  _deferredContext._lightingmtl.commit();
  ctx->debugPopGroup();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_renderUnshadowedUntexturedPointLights(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    const EnumeratedLights& enumlights) {
  /////////////////////////////////////////////////////////////////
  auto context  = drawdata.context();
  auto FXI      = context->FXI();
  auto this_buf = context->FBI()->GetThisBuffer();
  context->debugPushGroup("SimpleLightProcessor::_renderUnshadowedUntexturedPointLights");
  _deferredContext.beginPointLighting(drawdata, VD, nullptr);
  _updatePointLightUBOparams(context, enumlights._untexturedpointlights, VD._camposmono);
  int numlights = enumlights._untexturedpointlights.size();
  //printf("numlights<%d>\n", numlights );
  //////////////////////////////////////////////////
  fvec4 quad_pos(-1, -1, 2, 2);
  fvec4 quad_uva(0, 0, 1, 1);
  fvec4 quad_uvb(0, numlights, 0, 0);
  this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
  /////////////////////////////////////
  _deferredContext.endPointLighting(drawdata, VD);
  context->debugPopGroup();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_renderUnshadowedTexturedPointLights(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    const EnumeratedLights& enumlights) {
  /////////////////////////////////////////////////////////////////
  auto context  = drawdata.context();
  auto FXI      = context->FXI();
  auto this_buf = context->FBI()->GetThisBuffer();
  /////////////////////////////////////////////////////////
  context->debugPushGroup("SimpleLightProcessor::_renderUnshadowedTexturedPointLights");
  for (auto texture_item : enumlights._tex2pointlightmap) {
    auto texture = texture_item.first;
    int lidx     = 0;
    _deferredContext.beginPointLighting(drawdata, VD, texture);
    _updatePointLightUBOparams(context, texture_item.second, VD._camposmono);
    int numlights = texture_item.second.size();
    //////////////////////////////////////////////////
    fvec4 quad_pos(-1, -1, 2, 2);
    fvec4 quad_uva(0, 0, 1, 1);
    fvec4 quad_uvb(0, numlights, 0, 0);
    this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
    /////////////////////////////////////
    _deferredContext.endPointLighting(drawdata, VD);
  } // for (auto texture_item : _tex2pointmap ){
  context->debugPopGroup();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_renderUnshadowedTexturedSpotLights(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    const EnumeratedLights& enumlights) {
  /////////////////////////////////////////////////////////////////
  auto context  = drawdata.context();
  auto FXI      = context->FXI();
  auto this_buf = context->FBI()->GetThisBuffer();
  /////////////////////////////////////////////////////////
  context->debugPushGroup("SimpleLightProcessor::_renderUnshadowedTexturedSpotLights");
  for (auto texture_item : enumlights._tex2spotlightmap) {
    auto texture = texture_item.first;
    int lidx     = 0;
    _deferredContext.beginSpotLighting(drawdata, VD, texture);
    _updateSpotLightUBOparams(context, texture_item.second, VD._camposmono);
    int numlights = texture_item.second.size();
    //////////////////////////////////////////////////
    fvec4 quad_pos(-1, -1, 2, 2);
    fvec4 quad_uva(0, 0, 1, 1);
    fvec4 quad_uvb(0, numlights, 0, 0);
    this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
    /////////////////////////////////////
    _deferredContext.endSpotLighting(drawdata, VD);
  } // for (auto texture_item : _tex2pointmap ){
  context->debugPopGroup();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_renderTexturedSpotDecals(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    const EnumeratedLights& enumlights) {
  /////////////////////////////////////////////////////////////////
  auto context  = drawdata.context();
  auto FXI      = context->FXI();
  auto this_buf = context->FBI()->GetThisBuffer();
  /////////////////////////////////////////////////////////
  context->debugPushGroup("SimpleLightProcessor::_renderTexturedSpotDecals");
  for (auto texture_item : enumlights._tex2spotdecalmap) {
    auto texture = texture_item.first;
    int lidx     = 0;
    _deferredContext.beginSpotDecaling(drawdata, VD, texture);
    _updateSpotLightUBOparams(context, texture_item.second, VD._camposmono);
    int numlights = texture_item.second.size();
    //////////////////////////////////////////////////
    fvec4 quad_pos(-1, -1, 2, 2);
    fvec4 quad_uva(0, 0, 1, 1);
    fvec4 quad_uvb(0, numlights, 0, 0);
    this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
    /////////////////////////////////////
    _deferredContext.endSpotDecaling(drawdata, VD);
  } // for (auto texture_item : _tex2pointmap ){
  context->debugPopGroup();
}
/////////////////////////////////////
void SimpleLightProcessor::_renderShadowedTexturedSpotLights(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    const EnumeratedLights& enumlights) {
  auto& RCFD     = drawdata.RCFD();
  auto context   = drawdata.context();
  auto& ddprops  = drawdata._properties;
  auto irenderer = ddprops["irenderer"_crcu].get<lev2::IRenderer*>();
  auto FBI       = context->FBI();
  auto CIMPL     = drawdata._cimpl;
  auto FXI       = context->FXI();
  auto this_buf  = context->FBI()->GetThisBuffer();

  /////////////////////////////////////
  // render depth maps
  /////////////////////////////////////
  context->debugPushGroup("SimpleLightProcessor::_renderShadowedTexturedSpotLights::depthmaps");

  auto DEPTHRENDERCPD = CIMPL->topCPD();
  for (auto texture_item : enumlights._tex2shadowedspotlightmap) {
    for (auto light : texture_item.second) {
      auto irt              = light->rendertarget(context);
      auto shadowrect       = ViewportRect(0, 0, light->_shadowmapDim, light->_shadowmapDim);
      auto shadowmtx        = light->shadowMatrix();
      auto lightcamdat      = light->shadowCamDat();
      CameraMatrices cammtc = lightcamdat.computeMatrices(1.0f);

      DEPTHRENDERCPD._irendertarget        = irt;
      DEPTHRENDERCPD._cameraMatrices       = &cammtc;
      DEPTHRENDERCPD._stereoCameraMatrices = nullptr;
      DEPTHRENDERCPD._stereo1pass          = false;
      DEPTHRENDERCPD.SetDstRect(shadowrect);
      FBI->SetAutoClear(false);
      CIMPL->pushCPD(DEPTHRENDERCPD); // base lighting
      FBI->PushRtGroup(irt->_rtgroup);
      context->beginFrame();
      FBI->clearDepth(1.0f);
      auto DB = RCFD.GetDB();
      if (DB) {
        for (const auto& layer_name : DEPTHRENDERCPD.getLayerNames()) {
          context->debugMarker(FormatString("enqshadowlayer<%s>", layer_name.c_str()));
          DB->enqueueLayerToRenderQueue(layer_name, irenderer);
        }
        irenderer->drawEnqueuedRenderables();
      }
      CIMPL->popCPD();
      context->endFrame();
      FBI->PopRtGroup();
    }
  }
  FBI->SetAutoClear(false);
  context->debugPopGroup();

  /////////////////////////////////////
  // accumulate all spotlight dept maps
  /////////////////////////////////////

  context->debugPushGroup("SimpleLightProcessor::_renderShadowedTexturedSpotLights::accum");
  auto& lightmtl = _deferredContext._lightingmtl;

  for (auto texture_item : enumlights._tex2shadowedspotlightmap) {
    auto cookie  = texture_item.first;
    auto& lights = texture_item.second;

    _deferredContext.beginShadowedSpotLighting(drawdata, VD, cookie);

    lightmtl.bindParamFloat(_deferredContext._parDepthFogDistance, 1.0f / _deferredContext._depthFogDistance);
    lightmtl.bindParamFloat(_deferredContext._parDepthFogPower, _deferredContext._depthFogPower);
    lightmtl.commit();

    ///////////////////////////////////////////////////////////////////
    // TODO we would need texture arrays and all shadow buffers to be the same
    // size in order to batch these lights together.
    ///////////////////////////////////////////////////////////////////

    size_t numlights = lights.size();
    OrkAssert(numlights < KMAXLIGHTSPERCHUNK);
    for (auto light : lights) {
      numlights                        = 1;
      auto shadowtex                   = light->_shadowRTG->_depthTexture;
      fvec3 color                      = light->color()*light->intensity();
      float dist2cam                   = light->distance(VD._camposmono);
      auto mapping                     = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
      size_t offset_cd                 = 0;
      size_t offset_mtx                = offset_cd + KMAXLIGHTSPERCHUNK * sizeof(fvec4);
      size_t offset_mtx2               = offset_mtx + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
      size_t offset_rad                = offset_mtx2 + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
      mapping->ref<fvec4>(offset_cd)   = fvec4(color, dist2cam);
      mapping->ref<float>(offset_rad)  = light->GetRange();
      mapping->ref<fmtx4>(offset_mtx)  = light->worldMatrix();
      mapping->ref<fmtx4>(offset_mtx2) = light->shadowMatrix();
      FXI->unmapParamBuffer(mapping.get());
      FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
      lightmtl.bindParamCTex(_deferredContext._parMapShadowDepth, shadowtex);
      fvec4 shadowp;
      shadowp.x = (1.0f / float(light->_shadowmapDim));
      shadowp.y = (1.0f / 9.0f);
      shadowp.z = light->shadowDepthBias();
      lightmtl.bindParamVec4(_deferredContext._parShadowParams, shadowp);
      // offset_cd += sizeof(fvec4);
      // offset_mtx += sizeof(fmtx4);
      // offset_rad += sizeof(float);
      fvec4 quad_pos(-1, -1, 2, 2);
      fvec4 quad_uva(0, 0, 1, 1);
      fvec4 quad_uvb(0, numlights, 0, 0);
      this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
    }

    ///////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////
    /////////////////////////////////////
    _deferredContext.endShadowedSpotLighting(drawdata, VD);
  } // for (auto texture_item : _tex2pointmap ){
  context->debugPopGroup();
}
/////////////////////////////////////
} // namespace ork::lev2::deferrednode
