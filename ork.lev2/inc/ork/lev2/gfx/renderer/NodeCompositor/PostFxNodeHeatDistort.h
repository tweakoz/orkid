////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
// E2B item D — heat-distortion post-fx.
//
// Consumes a GENERIC AUX CHANNEL (default "heat") rendered by the forward
// node (see RenderCompositingNode::_auxChannels): content on layer
// "aux_<channel>" accumulates additively into an RGBA16F RT whose .r is the
// heat intensity. This node refracts the composited frame by the heat
// field's screen-space gradient (hot air bends light), with optional
// chromatic spread. Fully reflected — declare in the scene DSL via
// addPostFxNode and it round-trips into the zero-Python player.
///////////////////////////////////////////////////////////////////////////////

class PostFxNodeHeatDistort : public PostCompositingNode {
  DeclareConcreteX(PostFxNodeHeatDistort, PostCompositingNode);

public:
  PostFxNodeHeatDistort();
  ~PostFxNodeHeatDistort();

  // UV-offset gain on the heat gradient. The gradient of a smooth sprite
  // blob is small (~1e-3/texel), so useful values are O(1..32).
  float _strength = 8.0f;
  // chromatic spread: r/b sample at offset*(1±chroma). 0 = none.
  float _chroma = 0.25f;
  // which aux channel to read (layer "aux_<channel>", drawdata property
  // crc("aux_<channel>")). Channel absent at render = clean passthrough.
  std::string _channel = "heat";

  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;

  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
