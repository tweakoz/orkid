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
void SimpleLightProcessor::_clearFrameLighting() {
  _pointlights.clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::render(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights) {
  FrameRenderer& framerenderer = drawdata.mFrameRenderer;
  RenderContextFrameData& RCFD = framerenderer.framedata();
  auto context                 = RCFD.GetTarget();
  _gpuInit(context);
  _clearFrameLighting();
  _renderUnshadowedUnTexturedPointLights(drawdata, VD, enumlights);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SimpleLightProcessor::_renderUnshadowedUnTexturedPointLights(
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

  const int KTILEMAXX = _deferredContext._clusterW - 1;
  const int KTILEMAXY = _deferredContext._clusterH - 1;

  deferrednode::PointLight deferred_pointlight;

  for (auto l : scene_lights) {

    if (l->isShadowCaster())
      continue;

    fvec3 color = l->GetColor();

    if (auto as_point = dynamic_cast<lev2::PointLight*>(l)) {
      float radius = as_point->GetRadius();
      fvec3 pos    = as_point->GetWorldPosition();
      float faloff = as_point->GetFalloff();

      deferred_pointlight._radius = radius;
      deferred_pointlight._pos    = as_point->GetWorldPosition();
      deferred_pointlight._color  = color;
      deferred_pointlight.dist2cam = (deferred_pointlight._pos - VD._camposmono).Mag();

      _pointlights.push_back(deferred_pointlight);
    }
  }

  /////////////////////////////////////
  // render all pointlights
  /////////////////////////////////////

  auto& lightmtl = _deferredContext._lightingmtl;
  _deferredContext.beginPointLighting(drawdata, VD);
  FXI->bindParamBlockBuffer(_deferredContext._lightblock, _lightbuffer);
  auto mapping        = FXI->mapParamBuffer(_lightbuffer, 0, 65536);
  size_t chunk_offset = 0;
  size_t numlights    = _pointlights.size();
  OrkAssert(numlights<KMAXLIGHTSPERCHUNK);

  /////////////////////////////////////////////////////////
  constexpr size_t KPOSPASE = KMAXLIGHTSPERCHUNK * sizeof(fvec4);
  for (size_t lidx = 0; lidx < numlights; lidx++) {
    const auto& light = _pointlights[lidx];
    /////////////////////////////////////////////////////////
    // embed chunk's lights into lighting UBO
    /////////////////////////////////////////////////////////
    mapping->ref<fvec4>(chunk_offset)            = fvec4(light._color, light.dist2cam);
    mapping->ref<fvec4>(KPOSPASE + chunk_offset) = fvec4(light._pos, light._radius);
    chunk_offset += sizeof(fvec4);
    //printf("lidx<%d> light_color<%g %g %g>\n", lidx, light._color.x, light._color.y, light._color.z);
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
/////////////////////////////////////
} // namespace ork::lev2::deferrednode
