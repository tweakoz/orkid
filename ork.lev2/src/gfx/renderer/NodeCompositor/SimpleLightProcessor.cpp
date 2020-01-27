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
//#include <ork/gfx/image.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>

#include "NodeCompositorDeferred.h"
#include "SimpleLightProcessor.h"

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
void SimpleLightProcessor::render(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights) {
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto context                 = RCFD.GetTarget();
  _gpuInit(context);
  _renderUnshadowedUntexturedPointLights(drawdata, VD, enumlights);
  _renderUnshadowedTexturedPointLights(drawdata, VD, enumlights);
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
  // filter enumerated scenelights
  /////////////////////////////////////

  const auto& scene_lights = enumlights._enumeratedLights;

  _pointlights.clear();
  for (auto l : scene_lights) {
    if (l->isShadowCaster())
      continue;
    if (auto as_point = dynamic_cast<lev2::PointLight*>(l)) {
      if (as_point->texture() == nullptr)
        _pointlights.push_back(as_point);
    }
  }

  /////////////////////////////////////
  // render all pointlights
  /////////////////////////////////////

  auto& lightmtl = _deferredContext._lightingmtl;
  _deferredContext.beginPointLighting(drawdata, VD,nullptr);
  FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
  auto mapping        = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
  size_t chunk_offset = 0;
  size_t numlights    = _pointlights.size();
  OrkAssert(numlights < KMAXLIGHTSPERCHUNK);

  /////////////////////////////////////////////////////////
  constexpr size_t KPOSPASE = KMAXLIGHTSPERCHUNK * sizeof(fvec4);
  for (size_t lidx = 0; lidx < numlights; lidx++) {
    auto light     = _pointlights[lidx];
    fvec3 color    = light->color();
    float radius   = light->radius();
    fvec3 pos      = light->worldPosition();
    float falloff  = light->falloff();
    float dist2cam = (pos - VD._camposmono).Mag();
    /////////////////////////////////////////////////////////
    // embed chunk's lights into lighting UBO
    /////////////////////////////////////////////////////////
    mapping->ref<fvec4>(chunk_offset)            = fvec4(color, dist2cam);
    mapping->ref<fvec4>(KPOSPASE + chunk_offset) = fvec4(pos, radius);
    chunk_offset += sizeof(fvec4);
    // printf("lidx<%d> light_color<%g %g %g>\n", lidx, light._color.x, light._color.y, light._color.z);
  }
  /////////////////////////////////////
  // chunk ready, fire it off..
  /////////////////////////////////////
  FXI->unmapParamBuffer(mapping.get());
  //////////////////////////////////////////////////
  // set number of lights for tile
  //////////////////////////////////////////////////
  lightmtl.bindParamInt(_deferredContext._parNumLights, numlights);
  lightmtl.commit();
  //////////////////////////////////////////////////
  // accumulate light for tile
  //////////////////////////////////////////////////

  // printf("numlighttiles<%zu>\n", _chunktiles_pos.size());
  if (VD._isStereo) {
    // float L = (float(ix) / float(_clusterW));
    // this_buf->Render2dQuadEML(fvec4(L - 1.0f, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
    // this_buf->Render2dQuadEML(fvec4(L, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
  } else {
    fvec4 quad_pos(-1, -1, 2, 2);
    fvec4 quad_uva(0, 0, 1, 1);
    fvec4 quad_uvb(0, numlights, 0, 0);
    this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
  }
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
  // convert enumerated scenelights to deferred format
  /////////////////////////////////////

  const auto& scene_lights = enumlights._enumeratedLights;

  _tex2pointmap.clear();
  for (auto l : scene_lights) {
    if (l->isShadowCaster())
      continue;
    if (auto as_point = dynamic_cast<lev2::PointLight*>(l)) {
      auto texture = as_point->texture();
      if (texture) {
        _tex2pointmap[texture].push_back(as_point);
      }
    }
  }

  /////////////////////////////////////
  // render all pointlights for all pointlight textures
  /////////////////////////////////////

  auto& lightmtl = _deferredContext._lightingmtl;
  /////////////////////////////////////////////////////////
  constexpr size_t KPOSPASE = KMAXLIGHTSPERCHUNK * sizeof(fvec4);
  for (auto texture_item : _tex2pointmap) {
    auto texture = texture_item.first;
    int lidx     = 0;
    _deferredContext.beginPointLighting(drawdata, VD,texture);
    FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
    auto mapping        = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
    size_t chunk_offset = 0;
    size_t numlights    = texture_item.second.size();
    OrkAssert(numlights < KMAXLIGHTSPERCHUNK);
    for (auto light : texture_item.second) {
      fvec3 color    = light->color();
      float radius   = light->radius();
      fvec3 pos      = light->worldPosition();
      float falloff  = light->falloff();
      float dist2cam = (pos - VD._camposmono).Mag();
      /////////////////////////////////////////////////////////
      // embed chunk's lights into lighting UBO
      /////////////////////////////////////////////////////////
      mapping->ref<fvec4>(chunk_offset)            = fvec4(color, dist2cam);
      mapping->ref<fvec4>(KPOSPASE + chunk_offset) = fvec4(pos, radius);
      chunk_offset += sizeof(fvec4);
      printf("lidx<%d> light_color<%g %g %g>\n", lidx, color.x, color.y, color.z);
    }
    /////////////////////////////////////
    // chunk ready, fire it off..
    /////////////////////////////////////
    FXI->unmapParamBuffer(mapping.get());
    //////////////////////////////////////////////////
    // set number of lights for tile
    //////////////////////////////////////////////////
    lightmtl.bindParamInt(_deferredContext._parNumLights, numlights);
    lightmtl.commit();
    //////////////////////////////////////////////////
    if (VD._isStereo) {
      // float L = (float(ix) / float(_clusterW));
      // this_buf->Render2dQuadEML(fvec4(L - 1.0f, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
      // this_buf->Render2dQuadEML(fvec4(L, T, KTILESIZX * 0.5, KTILESIZY), fvec4(0, 0, 1, 1));
    } else {
      fvec4 quad_pos(-1, -1, 2, 2);
      fvec4 quad_uva(0, 0, 1, 1);
      fvec4 quad_uvb(0, numlights, 0, 0);
      this_buf->Render2dQuadsEML(1, &quad_pos, &quad_uva, &quad_uvb);
    }
    /////////////////////////////////////
    _deferredContext.endPointLighting(drawdata, VD);
  } // for (auto texture_item : _tex2pointmap ){
}
/////////////////////////////////////
} // namespace ork::lev2::deferrednode
