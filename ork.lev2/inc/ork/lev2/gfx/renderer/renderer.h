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
#include <limits>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

class PerformanceItem;

namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

struct IRenderer {
public:
  static const int kmaxrables    = 65536;
  static const int kmaxrablesmed = 8192;
  // static const int kmaxrablessm  = 64;

  IRenderer(Context* pTARG = nullptr);
  virtual ~IRenderer() {
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Immediate Rendering (sort of, actually just submit the renderable to the target, which might itself place into a display list)
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  void _renderCallbackRenderable(const CallbackRenderable& cbren) const;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Queued rendering
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ModelRenderable& enqueueModel();
  SkeletonRenderable& enqueueSkeleton();
  CallbackRenderable& enqueueCallback();

  void enqueueRenderable(IRenderable* pRenderable);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// Each Renderer implements this function as a helper for Renderables when composing their sort keys
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  /// Draw the enqueued renderables whose sort key falls in [skey_lo..skey_hi]. The range
  /// exists so a pass can SPLIT its queue into segments that need different attachment
  /// state (the forward node's depth-writable draw-last tail) without re-enqueueing:
  /// reset_after must be false on every segment but the last, or the held-back renderables
  /// are dropped with the queue.
  void drawEnqueuedRenderables(
      bool reset_after = false,
      int skey_lo      = 0,
      int skey_hi      = std::numeric_limits<int>::max());

  /// How many enqueued renderables carry a sort key >= skey. Asked BEFORE the first
  /// segment draws, because whether a tail segment exists decides whether its render pass
  /// is worth beginning at all.
  size_t countEnqueuedAtOrAboveSortKey(int skey) const;

  void SetPerformanceItem(PerformanceItem* perfitem);

  Context* GetTarget() const;
  void setContext(Context* ptarg);

  void fakeDraw();

  void resetQueue(void);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  Context* _target;

  ork::fixedvector<U32, RenderQueue::krqmaxsize> _sortkeys;
  ork::fixedvector<const RenderQueue::Node*, RenderQueue::krqmaxsize> _sortedNodes;

  ork::fixedvector<ModelRenderable, kmaxrables> _models;
  ork::fixedvector<SkeletonRenderable, kmaxrables> _skeletons;
  ork::fixedvector<CallbackRenderable, kmaxrablesmed> _callbacks;
  RadixSort _radixsorter;
  RenderQueue _unsortedNodes;
  std::string _renderername;
  bool _debugLog = false;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using renderer_ptr_t = std::shared_ptr<IRenderer>;

} // namespace lev2
} // namespace ork
