////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/material_freestyle.inl>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>

namespace ork::lev2::deferrednode {

///////////////////////////////////////////////////////////////////////////////
// SimpleLightProcessor :
//  no light culling and only 1 chunk
//  so lights have to fit in 1 UBO
//  submit light primitives on CPU
//  primarily used for simple test cases
///////////////////////////////////////////////////////////////////////////////

struct SimpleLightProcessor {

  static constexpr size_t KMAXLIGHTSPERCHUNK = 256;
  static constexpr size_t KMAXLIGHTS         = KMAXLIGHTSPERCHUNK;

  /////////////////////////////////////////////////////

  SimpleLightProcessor(DeferredContext& defctx, DeferredCompositingNodePbr* compnode);

  /////////////////////////////////////////////////////

  void render(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights);

  /////////////////////////////////////////////////////
  void _gpuInit(lev2::Context* target);
  void _renderUnshadowedUntexturedPointLights(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights);
  void _renderUnshadowedTexturedPointLights(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights);
  void _renderUnshadowedTexturedSpotLights(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights);

  using pointlightlist_t    = std::vector<lev2::PointLight*>;
  using spotlightlist_t     = std::vector<lev2::SpotLight*>;
  using tex2pointlightmap_t = std::map<lev2::Texture*, pointlightlist_t>;
  using tex2spotlightmap_t  = std::map<lev2::Texture*, spotlightlist_t>;

  FxShaderParamBuffer* _lightbuffer = nullptr;
  DeferredContext& _deferredContext;
  DeferredCompositingNodePbr* _defcompnode;
  pointlightlist_t _untexturedpointlights;
  tex2pointlightmap_t _tex2pointlightmap;
  spotlightlist_t _untexturedspotlights;
  tex2spotlightmap_t _tex2spotlightmap;
};

} // namespace ork::lev2::deferrednode
