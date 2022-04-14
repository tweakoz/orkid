////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
