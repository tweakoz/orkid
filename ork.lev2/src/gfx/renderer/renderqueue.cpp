////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/Array.hpp>
#include <ork/lev2/gfx/renderer/renderer.h>

///////////////////////////////////////////////////////////////////////////////

template class ork::fixedvector<ork::lev2::RenderQueue::Node, ork::lev2::RenderQueue::krqmaxsize>;

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void RenderQueue::enqueueRenderable(const IRenderable* renderable) {
  new (&_nodes.create()) Node(renderable);
}

///////////////////////////////////////////////////////////////////////////////

void RenderQueue::exportRenderableNodes(ork::fixedvector<const RenderQueue::Node*, krqmaxsize>& nodes) const {
  nodes.resize(Size());
  int idx = 0;
  for (const Node& n : _nodes) {
    nodes[idx++] = &n;
  }
}

///////////////////////////////////////////////////////////////////////////////

void RenderQueue::Reset() { _nodes.clear(); }

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
