////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/renderer.h>

namespace ork { namespace ent {
class SceneEditorBase;
}}; // namespace ork::ent

namespace ork { namespace tool {

class Renderer : public lev2::IRenderer {

public:
  Renderer(ent::SceneEditorBase& editor, lev2::Context* ptarg = nullptr);

private:
  void RenderModel(const lev2::ModelRenderable& ModelRen, ork::lev2::RenderGroupState rgs = ork::lev2::RenderGroupState::NONE) const final;
  void RenderModelGroup(const lev2::IRenderer::modelgroup_t&) const final;

  ent::SceneEditorBase& mEditor;
};

}} // namespace ork::tool
