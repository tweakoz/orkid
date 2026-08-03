////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"
#include "pbr_common.h"

namespace ork::lev2::pbr {

///////////////////////////////////////////////////////////////////////////////

struct ForwardNode : public RenderCompositingNode {
  DeclareAbstractX(ForwardNode, RenderCompositingNode);

public:
  ForwardNode(pbr::commonstuff_ptr_t pbrc);
  ~ForwardNode();

  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;
  // once-per-composited-frame: light enumeration + SSBO packing, shadow-map
  // updates, env-probe captures (view-independent — shared by all eyes)
  void renderPrologue(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;

  // the CANONICAL forward role set (+ one "aux_<name>" role per configured
  // aux channel) — _render_top assembles the frame from exactly this list,
  // and Scene::initWithParams pre-creates each as a layer
  const std::vector<std::string>& renderedLayerRoles() const final;

  mutable std::vector<std::string> _rolesCache; // rebuilt when _auxChannels changes

  svar256_t _impl;
  pbr::commonstuff_ptr_t _pbrcommon;

  // SINGLE-PASS STEREO (SPVR): the primary render targets become 2-layer multiview
  //  groups and the whole pass chain runs ONCE for both eyes. Declared by the preset
  //  that pairs this node with SinglePassStereoVrOutputNode, BEFORE gpuInit — the
  //  layer count is baked into every RtBuffer at construction, so it is not a
  //  per-frame switch.
  bool _singlePassStereo = false;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
