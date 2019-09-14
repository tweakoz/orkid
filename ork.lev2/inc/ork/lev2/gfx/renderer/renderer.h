////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/gfx/radixsort.h>
#include "renderable.h"
#include "renderqueue.h"
#include "rendercontext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork {

class PerformanceItem;
class CameraData;

namespace lev2 {

class GfxTarget;

class IRenderer {
public:
  static const int kmaxrables    = 4096;
  static const int kmaxrablesmed = 1024;
  static const int kmaxrablessm  = 64;

private:
  GfxTarget* mpTarget;

  ork::fixedvector<U32, RenderQueue::krqmaxsize> mQueueSortKeys;
  ork::fixedvector<const RenderQueue::Node*, RenderQueue::krqmaxsize> mQueueSortNodes;

  ork::fixedvector<ModelRenderable, kmaxrables> mModels;
  ork::fixedvector<CallbackRenderable, kmaxrablesmed> mCallbacks;

public:

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Immediate Rendering (sort of, actually just submit the renderable to the target, which might itself place into a display list)
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual void RenderModel(const ModelRenderable& ModelRen, RenderGroupState rgs = ERGST_NONE) const = 0;
  virtual void RenderModelGroup(const ModelRenderable** Renderables, int inumr) const                = 0;
  void RenderCallback(const CallbackRenderable& cbren) const;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Queued rendering
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ModelRenderable& QueueModel() {
    ModelRenderable& rend = mModels.create();
    QueueRenderable(&rend);
    return rend;
  }
  CallbackRenderable& QueueCallback() {
    CallbackRenderable& rend = mCallbacks.create();
    QueueRenderable(&rend);
    return rend;
  }

  void QueueRenderable(IRenderable* pRenderable);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// Each Renderer implements this function as a helper for Renderables when composing their sort keys
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual U32 ComposeSortKey(U32 texIndex, U32 depthIndex, U32 passIndex, U32 transIndex) const { return 0; }

  void DrawQueuedRenderables();

  inline void SetPerformanceItem(PerformanceItem* perfitem) { mPerformanceItem = perfitem; }

  GfxTarget* GetTarget() const { return mpTarget; }
  void SetTarget(GfxTarget* ptarg) { mpTarget = ptarg; }

  void FakeDraw() { ResetQueue(); }

protected:
  void ResetQueue(void);
  RadixSort mRadixSorter;
  RenderQueue mRenderQueue;
  PerformanceItem* mPerformanceItem;

  IRenderer(GfxTarget* pTARG);
};

} // namespace lev2
} // namespace ork
