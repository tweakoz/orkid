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

namespace ork::lev2::deferrednode {

///////////////////////////////////////////////////////////////////////////////
// SimpleLightProcessor :
//  no light culling and only 1 chunk
//  so lights have to fit in 1 UBO
//  submit light primitives on CPU
//  primarily used for simple test cases
///////////////////////////////////////////////////////////////////////////////

struct SimpleLightProcessor {

  static constexpr size_t KMAXLIGHTSPERCHUNK = 32768 / sizeof(fvec4);
  static constexpr size_t KMAXLIGHTS         = KMAXLIGHTSPERCHUNK;

  /////////////////////////////////////////////////////

  SimpleLightProcessor(DeferredContext& defctx,DeferredCompositingNodePbr* compnode);

  /////////////////////////////////////////////////////

  void render(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights);

  /////////////////////////////////////////////////////
  void _gpuInit(lev2::Context* target);
  void _clearFrameLighting();
  void _renderUnshadowedUnTexturedPointLights(CompositorDrawData& drawdata, const ViewData& VD, const EnumeratedLights& enumlights);

  typedef std::vector<PointLight> pllist_t;
  typedef ork::LockedResource<pllist_t> locked_pllist_t;

  FxShaderParamBuffer* _lightbuffer = nullptr;
  DeferredContext& _deferredContext;
  DeferredCompositingNodePbr* _defcompnode;
  pllist_t _pointlights;

};

} // namespace ork::lev2::deferrednode {
