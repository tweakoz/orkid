////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/orkpool.h>

namespace ork::lev2 {

class IRenderable;

class RenderQueue {
public:
  static const int krqmaxsize = 8192;

  void QueueRenderable(const IRenderable* pRenderable);
  void Reset();
  size_t Size() { return mNodes.size(); }
  RenderQueue() { Reset(); }
  struct Node {
    const IRenderable* _renderable;

    Node(const IRenderable* renderable = 0)
        : _renderable(renderable) {}
  };
  void ExportRenderableNodes(ork::fixedvector<const RenderQueue::Node*, krqmaxsize>& nodes);

protected:
  ork::fixedvector<Node, krqmaxsize> mNodes;
};

} // namespace ork::lev2
