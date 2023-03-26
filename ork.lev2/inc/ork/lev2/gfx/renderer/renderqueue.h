////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>
#include <ork/kernel/orkpool.h>

namespace ork::lev2 {

struct RenderQueue {
public:
  static const int krqmaxsize = 8192;

  void enqueueRenderable(const IRenderable* pRenderable);
  void Reset();
  size_t Size() const { return _nodes.size(); }
  RenderQueue() { Reset(); }
  /////////////////////////////////////////
  struct Node {
    const IRenderable* _renderable;

    Node(const IRenderable* renderable = nullptr)
        : _renderable(renderable) {}
  };
  /////////////////////////////////////////
  void exportRenderableNodes(ork::fixedvector<const RenderQueue::Node*, krqmaxsize>& nodes) const;
  /////////////////////////////////////////
protected:
  ork::fixedvector<Node, krqmaxsize> _nodes;
};

} // namespace ork::lev2
