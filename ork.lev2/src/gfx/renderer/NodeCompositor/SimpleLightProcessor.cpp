////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <ork/pch.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/material_freestyle.inl>
#include <ork/kernel/datacache.inl>
#include <ork/gfx/brdf.inl>
#include <ork/gfx/dds.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/SimpleLightProcessor.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::deferrednode {
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
  auto context                 = RCFD.GetTarget();
  _gpuInit(context);

  /////////////////////////////////////
  // convert enumerated scenelights to deferred format
  /////////////////////////////////////

  const auto& scene_lights = enumlights._enumeratedLights;

  _untexturedpointlights.clear();
  _untexturedspotlights.clear();
  _tex2pointlightmap.clear();
  _tex2spotlightmap.clear();
  _tex2spotdecalmap.clear();

  for (auto l : scene_lights) {
    if (l->isShadowCaster())
      continue;
    if (auto as_point = dynamic_cast<lev2::PointLight*>(l)) {
      auto cookie = as_point->cookie();
      if (cookie)
        _tex2pointlightmap[cookie].push_back(as_point);
      else
        _untexturedpointlights.push_back(as_point);
    } else if (auto as_spot = dynamic_cast<lev2::SpotLight*>(l)) {
      auto cookie = as_spot->cookie();
      bool decal  = as_spot->decal();
      if (decal) {
        if (cookie)
          _tex2spotdecalmap[cookie].push_back(as_spot);
      } else {
        if (cookie)
          _tex2spotlightmap[cookie].push_back(as_spot);
        else
          _untexturedspotlights.push_back(as_spot);
      }
    }
  }
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
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_renderUnshadowedUntexturedPointLights(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    const EnumeratedLights& enumlights) {
  bool is_stereo = VD._isStereo;
  /////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  const auto& RCFD             = framerenderer.framedata();
  auto gfxctx                  = RCFD.GetTarget();
  auto FXI                     = gfxctx->FXI();
  auto RSI                     = gfxctx->RSI();
  auto this_buf                = gfxctx->FBI()->GetThisBuffer();

  /////////////////////////////////////
  // render all (untextured) pointlights
  /////////////////////////////////////

  auto& lightmtl = _deferredContext._lightingmtl;
  _deferredContext.beginPointLighting(drawdata, VD, nullptr);
  FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
  auto mapping     = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
  size_t numlights = _untexturedpointlights.size();
  OrkAssert(numlights < KMAXLIGHTSPERCHUNK);
  /////////////////////////////////////////////////////////
  size_t offset_cd  = 0;
  size_t offset_mtx = offset_cd + KMAXLIGHTSPERCHUNK * sizeof(fvec4);
  size_t offset_rad = offset_mtx + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
  /////////////////////////////////////////////////////////
  constexpr size_t KPOSPASE = KMAXLIGHTSPERCHUNK * sizeof(fvec4);
  for (size_t lidx = 0; lidx < numlights; lidx++) {
    auto light     = _untexturedpointlights[lidx];
    fvec3 color    = light->color();
    float radius   = light->radius();
    fvec3 pos      = light->worldPosition();
    float falloff  = light->falloff();
    float dist2cam = (pos - VD._camposmono).Mag();
    /////////////////////////////////////////////////////////
    // embed chunk's lights into lighting UBO
    /////////////////////////////////////////////////////////
    mapping->ref<fvec4>(offset_cd)  = fvec4(color, dist2cam);
    mapping->ref<fmtx4>(offset_mtx) = light->worldMatrix();
    mapping->ref<float>(offset_rad) = radius;
    offset_cd += sizeof(fvec4);
    offset_mtx += sizeof(fmtx4);
    offset_rad += sizeof(float);
    // printf("lidx<%zu> pos<%g %g %g> color<%g %g %g>\n", lidx, pos.x, pos.y, pos.z, color.x, color.y, color.z);
  }
  /////////////////////////////////////
  // chunk ready, fire it off..
  /////////////////////////////////////
  FXI->unmapParamBuffer(mapping.get());
  //////////////////////////////////////////////////
  // set number of lights for tile
  //////////////////////////////////////////////////
  lightmtl.bindParamInt(_deferredContext._parNumLights, numlights);
  lightmtl.bindParamFloat(_deferredContext._parDepthFogDistance, 1.0f / _deferredContext._depthFogDistance);
  lightmtl.bindParamFloat(_deferredContext._parDepthFogPower, _deferredContext._depthFogPower);
  lightmtl.commit();
  //////////////////////////////////////////////////
  // accumulate light for tile
  //////////////////////////////////////////////////

  // printf("numlighttiles<%zu>\n", _chunktiles_pos.size());
  fvec4 quad_pos(-1, -1, 2, 2);
  fvec4 quad_uva(0, 0, 1, 1);
  fvec4 quad_uvb(0, numlights, 0, 0);
  this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
  /////////////////////////////////////
  _deferredContext.endPointLighting(drawdata, VD);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_renderUnshadowedTexturedPointLights(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    const EnumeratedLights& enumlights) {
  bool is_stereo = VD._isStereo;
  /////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  const auto& RCFD             = framerenderer.framedata();
  auto gfxctx                  = RCFD.GetTarget();
  auto FXI                     = gfxctx->FXI();
  auto RSI                     = gfxctx->RSI();
  auto this_buf                = gfxctx->FBI()->GetThisBuffer();

  /////////////////////////////////////
  // render all pointlights for all pointlight textures
  /////////////////////////////////////

  auto& lightmtl = _deferredContext._lightingmtl;
  /////////////////////////////////////////////////////////
  for (auto texture_item : _tex2pointlightmap) {
    auto texture = texture_item.first;
    int lidx     = 0;
    _deferredContext.beginPointLighting(drawdata, VD, texture);
    FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
    auto mapping     = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
    size_t numlights = texture_item.second.size();
    OrkAssert(numlights < KMAXLIGHTSPERCHUNK);
    size_t offset_cd  = 0;
    size_t offset_mtx = offset_cd + KMAXLIGHTSPERCHUNK * sizeof(fvec4);
    size_t offset_rad = offset_mtx + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
    for (auto light : texture_item.second) {
      fvec3 color    = light->color();
      float radius   = light->radius();
      float falloff  = light->falloff();
      float dist2cam = (light->worldPosition() - VD._camposmono).Mag();
      /////////////////////////////////////////////////////////
      // embed chunk's lights into lighting UBO
      /////////////////////////////////////////////////////////
      mapping->ref<fvec4>(offset_cd)  = fvec4(color, dist2cam);
      mapping->ref<fmtx4>(offset_mtx) = light->worldMatrix();
      mapping->ref<float>(offset_rad) = radius;
      offset_cd += sizeof(fvec4);
      offset_mtx += sizeof(fmtx4);
      offset_rad += sizeof(float);
      // printf("tex-light<%p> pos<%g %g %g> color<%g %g %g>\n", light, pos.x, pos.y, pos.z, color.x, color.y, color.z);
    }
    /////////////////////////////////////
    // chunk ready, fire it off..
    /////////////////////////////////////
    FXI->unmapParamBuffer(mapping.get());
    //////////////////////////////////////////////////
    // set number of lights for tile
    //////////////////////////////////////////////////
    lightmtl.bindParamFloat(_deferredContext._parDepthFogDistance, 1.0f / _deferredContext._depthFogDistance);
    lightmtl.bindParamFloat(_deferredContext._parDepthFogPower, _deferredContext._depthFogPower);
    lightmtl.bindParamInt(_deferredContext._parNumLights, numlights);
    lightmtl.commit();
    //////////////////////////////////////////////////
    fvec4 quad_pos(-1, -1, 2, 2);
    fvec4 quad_uva(0, 0, 1, 1);
    fvec4 quad_uvb(0, numlights, 0, 0);
    this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
    /////////////////////////////////////
    _deferredContext.endPointLighting(drawdata, VD);
  } // for (auto texture_item : _tex2pointmap ){
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_renderUnshadowedTexturedSpotLights(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    const EnumeratedLights& enumlights) {
  bool is_stereo = VD._isStereo;
  /////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  const auto& RCFD             = framerenderer.framedata();
  auto gfxctx                  = RCFD.GetTarget();
  auto FXI                     = gfxctx->FXI();
  auto RSI                     = gfxctx->RSI();
  auto this_buf                = gfxctx->FBI()->GetThisBuffer();

  /////////////////////////////////////
  // render all pointlights for all pointlight textures
  /////////////////////////////////////

  auto& lightmtl = _deferredContext._lightingmtl;
  /////////////////////////////////////////////////////////
  for (auto texture_item : _tex2spotlightmap) {
    auto texture = texture_item.first;
    int lidx     = 0;
    _deferredContext.beginSpotLighting(drawdata, VD, texture);
    FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
    auto mapping     = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
    size_t numlights = texture_item.second.size();
    OrkAssert(numlights < KMAXLIGHTSPERCHUNK);
    size_t offset_cd  = 0;
    size_t offset_mtx = offset_cd + KMAXLIGHTSPERCHUNK * sizeof(fvec4);
    size_t offset_rad = offset_mtx + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
    for (auto light : texture_item.second) {
      fvec3 color    = light->color();
      float fovy     = light->GetFovy();
      float range    = light->GetRange();
      float dist2cam = (light->worldPosition() - VD._camposmono).Mag();
      /////////////////////////////////////////////////////////
      // embed chunk's lights into lighting UBO
      /////////////////////////////////////////////////////////
      mapping->ref<fvec4>(offset_cd)  = fvec4(color, dist2cam);
      mapping->ref<float>(offset_rad) = range;

      fmtx4 matV, matP;
      float near   = range / 1000.0f;
      float far    = range;
      float aspect = 1.0;

      fvec3 wnx, wny, wnz, wpos;
      light->worldMatrix().toNormalVectors(wnx, wny, wnz);
      wpos      = light->worldMatrix().GetTranslation();
      fvec3 ctr = wpos + wnz;
      // matV = light->worldMatrix();
      matV.LookAt(wpos, ctr, wny);

      matP.Perspective(fovy, aspect, near, far);
      mapping->ref<fmtx4>(offset_mtx) = matV * matP;
      // mapping->ref<fmtx4>(offset_mtx) = light->worldMatrix();

      offset_cd += sizeof(fvec4);
      offset_mtx += sizeof(fmtx4);
      offset_rad += sizeof(float);
      // printf("tex-light<%p> pos<%g %g %g> color<%g %g %g>\n", light, pos.x, pos.y, pos.z, color.x, color.y, color.z);
    }
    /////////////////////////////////////
    // chunk ready, fire it off..
    /////////////////////////////////////
    FXI->unmapParamBuffer(mapping.get());
    //////////////////////////////////////////////////
    // set number of lights for tile
    //////////////////////////////////////////////////
    lightmtl.bindParamFloat(_deferredContext._parDepthFogDistance, 1.0f / _deferredContext._depthFogDistance);
    lightmtl.bindParamFloat(_deferredContext._parDepthFogPower, _deferredContext._depthFogPower);
    lightmtl.bindParamInt(_deferredContext._parNumLights, numlights);
    lightmtl.commit();
    //////////////////////////////////////////////////
    fvec4 quad_pos(-1, -1, 2, 2);
    fvec4 quad_uva(0, 0, 1, 1);
    fvec4 quad_uvb(0, numlights, 0, 0);
    this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
    /////////////////////////////////////
    _deferredContext.endSpotLighting(drawdata, VD);
  } // for (auto texture_item : _tex2pointmap ){
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_renderTexturedSpotDecals(
    CompositorDrawData& drawdata,
    const ViewData& VD,
    const EnumeratedLights& enumlights) {
  bool is_stereo = VD._isStereo;
  /////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  const auto& RCFD             = framerenderer.framedata();
  auto gfxctx                  = RCFD.GetTarget();
  auto FXI                     = gfxctx->FXI();
  auto RSI                     = gfxctx->RSI();
  auto this_buf                = gfxctx->FBI()->GetThisBuffer();

  /////////////////////////////////////
  // render all pointlights for all pointlight textures
  /////////////////////////////////////

  auto& lightmtl = _deferredContext._lightingmtl;
  /////////////////////////////////////////////////////////
  for (auto texture_item : _tex2spotdecalmap) {
    auto texture = texture_item.first;
    int lidx     = 0;
    _deferredContext.beginSpotDecaling(drawdata, VD, texture);
    FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
    auto mapping     = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
    size_t numlights = texture_item.second.size();
    OrkAssert(numlights < KMAXLIGHTSPERCHUNK);
    size_t offset_cd  = 0;
    size_t offset_mtx = offset_cd + KMAXLIGHTSPERCHUNK * sizeof(fvec4);
    size_t offset_rad = offset_mtx + KMAXLIGHTSPERCHUNK * sizeof(fmtx4);
    for (auto light : texture_item.second) {
      fvec3 color    = light->color();
      float fovy     = light->GetFovy();
      float range    = light->GetRange();
      float dist2cam = (light->worldPosition() - VD._camposmono).Mag();
      /////////////////////////////////////////////////////////
      // embed chunk's lights into lighting UBO
      /////////////////////////////////////////////////////////
      mapping->ref<fvec4>(offset_cd)  = fvec4(color, dist2cam);
      mapping->ref<float>(offset_rad) = range;

      fmtx4 matV, matP;
      float near   = range / 1000.0f;
      float far    = range;
      float aspect = 1.0;

      fvec3 wnx, wny, wnz, wpos;
      light->worldMatrix().toNormalVectors(wnx, wny, wnz);
      wpos      = light->worldMatrix().GetTranslation();
      fvec3 ctr = wpos + wnz;
      // matV = light->worldMatrix();
      matV.LookAt(wpos, ctr, wny);

      matP.Perspective(fovy, aspect, near, far);
      mapping->ref<fmtx4>(offset_mtx) = matV * matP;

      offset_cd += sizeof(fvec4);
      offset_mtx += sizeof(fmtx4);
      offset_rad += sizeof(float);
      // printf("tex-light<%p> pos<%g %g %g> color<%g %g %g>\n", light, pos.x, pos.y, pos.z, color.x, color.y, color.z);
    }
    /////////////////////////////////////
    // chunk ready, fire it off..
    /////////////////////////////////////
    FXI->unmapParamBuffer(mapping.get());
    //////////////////////////////////////////////////
    // set number of lights for tile
    //////////////////////////////////////////////////
    lightmtl.bindParamFloat(_deferredContext._parDepthFogDistance, 1.0f / _deferredContext._depthFogDistance);
    lightmtl.bindParamFloat(_deferredContext._parDepthFogPower, _deferredContext._depthFogPower);
    lightmtl.bindParamInt(_deferredContext._parNumLights, numlights);
    lightmtl.commit();
    //////////////////////////////////////////////////
    fvec4 quad_pos(-1, -1, 2, 2);
    fvec4 quad_uva(0, 0, 1, 1);
    fvec4 quad_uvb(0, numlights, 0, 0);
    this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
    /////////////////////////////////////
    _deferredContext.endSpotDecaling(drawdata, VD);
  } // for (auto texture_item : _tex2pointmap ){
}
/////////////////////////////////////
} // namespace ork::lev2::deferrednode
