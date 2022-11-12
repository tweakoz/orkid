////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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

struct IRenderer {
public:
  static const int kmaxrables    = 65536;
  static const int kmaxrablesmed = 8192;
  //static const int kmaxrablessm  = 64;



protected:
  
  virtual ~IRenderer() {}
  
  Context* _target;

  ork::fixedvector<U32, RenderQueue::krqmaxsize> _sortkeys;
  ork::fixedvector<const RenderQueue::Node*, RenderQueue::krqmaxsize> _sortedNodes;

  ork::fixedvector<ModelRenderable, kmaxrables> _models;
  ork::fixedvector<CallbackRenderable, kmaxrablesmed> _callbacks;

public:
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Immediate Rendering (sort of, actually just submit the renderable to the target, which might itself place into a display list)
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  virtual void RenderModel(const ModelRenderable& ModelRen, RenderGroupState rgs = RenderGroupState::NONE) const = 0;
  void RenderCallback(const CallbackRenderable& cbren) const;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Queued rendering
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  ModelRenderable& enqueueModel() {
    ModelRenderable& rend = _models.create();
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

protected:
  RadixSort _radixsorter;
  RenderQueue _unsortedNodes;
  PerformanceItem* mPerformanceItem;

  IRenderer(Context* pTARG);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct DefaultRenderer : public lev2::IRenderer {

public:
  DefaultRenderer(lev2::Context* ptarg = nullptr);

private:
  void RenderModel(const lev2::ModelRenderable& ModelRen, ork::lev2::RenderGroupState rgs = ork::lev2::RenderGroupState::NONE) const final;
};


using renderer_ptr_t = std::shared_ptr<IRenderer>;

} // namespace lev2
} // namespace ork
