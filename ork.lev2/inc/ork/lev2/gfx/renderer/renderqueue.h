////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/orkpool.h>

namespace ork::lev2 {

class Renderer;
class IRenderable;

class RenderQueue {
public:
  static const int krqmaxsize = 8192;

  void QueueRenderable(const IRenderable* pRenderable, Renderer* pRenderer);
  void Reset();
  size_t Size() { return mNodes.size(); }
  RenderQueue() { Reset(); }
  struct Node {
    const IRenderable* mpRenderable;
    const Object* mpContextObject;

    Node(const Object* object = 0, const IRenderable* renderable = 0)
        : mpContextObject(object)
        , mpRenderable(renderable) {}
  };
  void ExportRenderableNodes(ork::fixedvector<const RenderQueue::Node*, krqmaxsize>& nodes);

protected:
  ork::fixedvector<Node, krqmaxsize> mNodes;
};

} // namespace ork::lev2
