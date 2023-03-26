////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>

namespace ork::lev2::pbr::deferrednode {

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
  void gpuUpdate(CompositorDrawData& drawdata, const ViewData& VD, enumeratedlights_constptr_t enumlights);
  void renderDecals(CompositorDrawData& drawdata, const ViewData& VD, enumeratedlights_constptr_t enumlights);
  void renderLights(CompositorDrawData& drawdata, const ViewData& VD, enumeratedlights_constptr_t enumlights);
  /////////////////////////////////////////////////////


  /////////////////////////////////////////////////////
  void _updatePointLightUBOparams(Context* ctx, const pointlightlist_t& lights, fvec3 campos);
  void _updateSpotLightUBOparams(Context* ctx, const spotlightlist_t& lights, fvec3 campos);

  /////////////////////////////////////////////////////

  void _gpuInit(lev2::Context* target);
  void _renderUnshadowedUntexturedPointLights(CompositorDrawData& drawdata, const ViewData& VD, enumeratedlights_constptr_t enumlights);
  void _renderUnshadowedTexturedPointLights(CompositorDrawData& drawdata, const ViewData& VD, enumeratedlights_constptr_t enumlights);
  void _renderUnshadowedTexturedSpotLights(CompositorDrawData& drawdata, const ViewData& VD, enumeratedlights_constptr_t enumlights);
  void _renderShadowedTexturedSpotLights(CompositorDrawData& drawdata, const ViewData& VD, enumeratedlights_constptr_t enumlights);
  void _renderTexturedSpotDecals(CompositorDrawData& drawdata, const ViewData& VD, enumeratedlights_constptr_t enumlights);

  /////////////////////////////////////////////////////

  FxShaderParamBuffer* _lightbuffer = nullptr;
  DeferredContext& _deferredContext;
  DeferredCompositingNodePbr* _defcompnode;
};

} // namespace ork::lev2::deferrednode
