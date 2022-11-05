////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/pch.h>

#include <ork/kernel/Array.hpp>
#include <ork/kernel/timer.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

DefaultRenderer::DefaultRenderer(Context* ptarg)
    : IRenderer(ptarg) {
}

///////////////////////////////////////////////////////////////////////////////

void DefaultRenderer::RenderModelGroup(const modelgroup_t& mdlgroup) const {
  for (auto r : mdlgroup)
    RenderModel(*r);
}

///////////////////////////////////////////////////////////////////////////////

void DefaultRenderer::RenderModel(const ModelRenderable& mdl_renderable, RenderGroupState rgs) const {

}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
