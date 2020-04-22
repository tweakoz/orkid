////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

class Context;
class CameraData;

class IRenderer {
public:
  static const int kmaxrables    = 4096;
  static const int kmaxrablesmed = 1024;
  static const int kmaxrablessm  = 64;

protected:
  Context* mpTarget;

  ork::fixedvector<U32, RenderQueue::krqmaxsize> mQueueSortKeys;
  ork::fixedvector<const RenderQueue::Node*, RenderQueue::krqmaxsize> mQueueSortNodes;

  ork::fixedvector<ModelRenderable, kmaxrables> mModels;
  ork::fixedvector<CallbackRenderable, kmaxrablesmed> mCallbacks;

  typedef ork::fixedvector<const ModelRenderable*, kmaxrablesmed> modelgroup_t;
  modelgroup_t _groupedModels;

public:
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Immediate Rendering (sort of, actually just submit the renderable to the target, which might itself place into a display list)
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual void RenderModel(const ModelRenderable& ModelRen, RenderGroupState rgs = ERGST_NONE) const = 0;
  virtual void RenderModelGroup(const modelgroup_t& group) const                                     = 0;
  void RenderCallback(const CallbackRenderable& cbren) const;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Queued rendering
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ModelRenderable& enqueueModel() {
    ModelRenderable& rend = mModels.create();
    QueueRenderable(&rend);
    return rend;
  }
  CallbackRenderable& enqueueCallback() {
    CallbackRenderable& rend = mCallbacks.create();
    QueueRenderable(&rend);
    return rend;
  }

  void QueueRenderable(IRenderable* pRenderable);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /// Each Renderer implements this function as a helper for Renderables when composing their sort keys
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  void drawEnqueuedRenderables();

  inline void SetPerformanceItem(PerformanceItem* perfitem) {
    mPerformanceItem = perfitem;
  }

  Context* GetTarget() const {
    return mpTarget;
  }
  void setContext(Context* ptarg) {
    mpTarget = ptarg;
  }

  void FakeDraw() {
    ResetQueue();
  }

protected:
  void ResetQueue(void);
  RadixSort mRadixSorter;
  RenderQueue mRenderQueue;
  PerformanceItem* mPerformanceItem;

  IRenderer(Context* pTARG);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class DefaultRenderer : public lev2::IRenderer {

public:
  DefaultRenderer(lev2::Context* ptarg = nullptr);

private:
  void RenderModel(const lev2::ModelRenderable& ModelRen, ork::lev2::RenderGroupState rgs = ork::lev2::ERGST_NONE) const final;
  void RenderModelGroup(const lev2::IRenderer::modelgroup_t&) const final;
};

} // namespace lev2
} // namespace ork
