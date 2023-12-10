////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/renderqueue.h>
#include <ork/lev2/gfx/renderer/renderer_enum.h>
#include <ork/gfx/radixsort.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

class PerformanceItem;

namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

struct IRenderer {
public:
  static const int kmaxrables    = 65536;
  static const int kmaxrablesmed = 8192;
  //static const int kmaxrablessm  = 64;

  IRenderer(Context* pTARG=nullptr);
  virtual ~IRenderer() {}  
  
  Context* _target;

  ork::fixedvector<U32, RenderQueue::krqmaxsize> _sortkeys;
  ork::fixedvector<const RenderQueue::Node*, RenderQueue::krqmaxsize> _sortedNodes;

  ork::fixedvector<ModelRenderable, kmaxrables> _models;
  ork::fixedvector<SkeletonRenderable, kmaxrables> _skeletons;
  ork::fixedvector<CallbackRenderable, kmaxrablesmed> _callbacks;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Immediate Rendering (sort of, actually just submit the renderable to the target, which might itself place into a display list)
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  void _renderCallbackRenderable(const CallbackRenderable& cbren) const;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Queued rendering
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ModelRenderable& enqueueModel() {
    ModelRenderable& rend = _models.create();
    enqueueRenderable(&rend);
    return rend;
  }
  SkeletonRenderable& enqueueSkeleton() {
    SkeletonRenderable& rend = _skeletons.create();
    enqueueRenderable(&rend);
    return rend;
  }
  CallbackRenderable& enqueueCallback() {
    CallbackRenderable& rend = _callbacks.create();
    enqueueRenderable(&rend);
    return rend;
  }

  void enqueueRenderable(IRenderable* pRenderable);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// Each Renderer implements this function as a helper for Renderables when composing their sort keys
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  void drawEnqueuedRenderables();

  inline void SetPerformanceItem(PerformanceItem* perfitem) {
    mPerformanceItem = perfitem;
  }

  Context* GetTarget() const {
    return _target;
  }
  void setContext(Context* ptarg) {
    _target = ptarg;
  }

  void fakeDraw() {
    resetQueue();
  }

  void resetQueue(void);

  RadixSort _radixsorter;
  RenderQueue _unsortedNodes;
  PerformanceItem* mPerformanceItem;
  std::string _renderername;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using renderer_ptr_t = std::shared_ptr<IRenderer>;

} // namespace lev2
} // namespace ork
