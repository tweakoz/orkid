////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/pch.h>

#include <ork/kernel/Array.hpp>
#include <ork/kernel/timer.h>
#include <ork/profiling.inl>

///////////////////////////////////////////////////////////////////////////////
// Explicit template instantiations for fixed vectors used in renderer
///////////////////////////////////////////////////////////////////////////////

template class ork::fixedvector<U32, ork::lev2::RenderQueue::krqmaxsize>;
template class ork::fixedvector<const ork::lev2::RenderQueue::Node*, ork::lev2::RenderQueue::krqmaxsize>;
template class ork::fixedvector<ork::lev2::ModelRenderable, ork::lev2::IRenderer::kmaxrables>;
template class ork::fixedvector<ork::lev2::CallbackRenderable, ork::lev2::IRenderer::kmaxrables>;

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////
// Base renderer implementation
// Handles render queue management and sorting
///////////////////////////////////////////////////////////////////////////////

IRenderer::IRenderer(Context* pTARG)
    : _target(pTARG)
    , _unsortedNodes() {
      _debugLog = false;
}

///////////////////////////////////////////////////////////////////////////////
// Add a renderable to the unsorted queue
// This is the entry point for all renderables
///////////////////////////////////////////////////////////////////////////////

void IRenderer::enqueueRenderable(IRenderable* renderable) {
  _unsortedNodes.enqueueRenderable(renderable);
}

///////////////////////////////////////////////////////////////////////////////
// Main render queue processing
// Sorts and renders all queued renderables
///////////////////////////////////////////////////////////////////////////////

void IRenderer::drawEnqueuedRenderables(bool reset_after) {

  EASY_BLOCK("IRenderer::DER1", profiler::colors::Red);

  ///////////////////////////////////////////////////////
  // Early out if queue is empty
  ///////////////////////////////////////////////////////
  
  size_t renderQueueSize = _unsortedNodes.Size();
  _target->debugPushGroup(FormatString("IRenderer::drawEnqueuedRenderables renderQueueSize<%zu>", renderQueueSize));

  if(_debugLog){
    printf( "renderQueueSize<%zu>\n", renderQueueSize);
  }

  if (renderQueueSize == 0) {
    //printf("IRenderer::drawEnqueuedRenderables empty queue\n");
    _target->debugPopGroup();
    if(reset_after){
      resetQueue();
    }
    return;
  }

  ///////////////////////////////////////////////////////
  // Stage 1: Export nodes and compute sort keys
  ///////////////////////////////////////////////////////
  
  _sortkeys.clear();
  EASY_END_BLOCK;
  EASY_BLOCK("IRenderer::DER2", profiler::colors::Red);
  _unsortedNodes.exportRenderableNodes(_sortedNodes);

  _sortkeys.resize(renderQueueSize);
  for (size_t i = 0; i < renderQueueSize; i++) {
    auto ren = _sortedNodes[i]->_renderable;
    int skey     = ren->_sortkey;
    _sortkeys[i] = skey;
    if(_debugLog){
      printf( "skey<%zu:%0x>\n", i, skey  );
    }
  }
  EASY_END_BLOCK;

  ///////////////////////////////////////////////////////
  // Stage 2: Radix sort the render queue
  ///////////////////////////////////////////////////////
  
  EASY_BLOCK("IRenderer::DER3", profiler::colors::Red);

  U32& first = (*_sortkeys.begin());
  _radixsorter.Sort(&first, U32(renderQueueSize));
  U32* sortedRenderQueueIndices = _radixsorter.GetIndices();

  int imdlcount = 0;
  float fruntot = 0.0f;
  EASY_END_BLOCK;

  ///////////////////////////////////////////////////////
  // Stage 3: Debug output of sort keys
  ///////////////////////////////////////////////////////

  EASY_BLOCK("IRenderer::DER4", profiler::colors::Red);

  for (size_t i = 0; i < renderQueueSize; i++) {
    int sorted = _sortkeys[i];
    _target->debugMarker(FormatString("IRenderer::drawEnqueuedRenderables sorting index<%zu> sorted<%d>", i, sorted));
  }

  EASY_END_BLOCK;

  ///////////////////////////////////////////////////////
  // Stage 4: Render sorted queue
  ///////////////////////////////////////////////////////

  EASY_BLOCK("IRenderer::DER5", profiler::colors::Red);

  for (size_t i = 0; i < renderQueueSize; i++) {
    int sorted = sortedRenderQueueIndices[i];
    OrkAssert(sorted < U32(renderQueueSize));
    const RenderQueue::Node* pnode = _sortedNodes[sorted];
    auto ren = pnode->_renderable;
    if(_debugLog){
      int sortkey = _sortkeys[sorted];
      const char* name = ren->_drawable ? ren->_drawable->_name.c_str() : "???";
      printf( "render i<%zu> sorted<%d> sortkey<0x%x> drw: %p:%s\n", i, sorted, sortkey, ren->_drawable, name );
    }


    _target->debugPushGroup(FormatString("IRenderer::drawEnqueuedRenderables render item<%zu> node<%p>", i, pnode));
    ren->Render(this);
    _target->debugPopGroup();
  }

  float favgrun = fruntot / float(imdlcount);
  EASY_END_BLOCK;

  ///////////////////////////////////////////////////////
  // Stage 5: Cleanup and performance tracking
  ///////////////////////////////////////////////////////

  EASY_BLOCK("IRenderer::DER6", profiler::colors::Red);

  _target->debugPopGroup();
  if(reset_after){
    resetQueue();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Reset the render queue, clearing all renderables
///////////////////////////////////////////////////////////////////////////////

void IRenderer::resetQueue(void) {
  _unsortedNodes.Reset();
  _models.clear();
  _skeletons.clear();
  _callbacks.clear();
}

///////////////////////////////////////////////////////////////////////////////
// Render a callback renderable
// Used for custom rendering operations
///////////////////////////////////////////////////////////////////////////////

void IRenderer::_renderCallbackRenderable(const CallbackRenderable& cbren) const {
  // Check _renderEnabled on the drawable at render execution time
  // (equivalent to ModelDrawable's "if (!_modelinst) return;" pattern)
  if (cbren._drawable) {
    auto* cbd = static_cast<const CallbackDrawable*>(cbren._drawable);
    if (!cbd->_renderEnabled.load(std::memory_order_acquire)) return;
  }
  if (cbren.GetRenderCallback()) {
    auto context = GetTarget();
    RenderContextInstData RCID(context->topRenderContextFrameData());
    RCID.SetRenderer(this);
    RCID.setRenderable(&cbren);
    RCID._pickID = cbren._pickID;
    context->RefModColor() = cbren._modColor;
    cbren.GetRenderCallback()(RCID);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Factory methods for different renderable types
// These create and enqueue the renderables in one step
///////////////////////////////////////////////////////////////////////////////

ModelRenderable& IRenderer::enqueueModel() {
  ModelRenderable& rend = _models.create();
  enqueueRenderable(&rend);
  return rend;
}

///////////////////////////////////////////////////////////////////////////////

SkeletonRenderable& IRenderer::enqueueSkeleton() {
  SkeletonRenderable& rend = _skeletons.create();
  enqueueRenderable(&rend);
  return rend;
}

///////////////////////////////////////////////////////////////////////////////

CallbackRenderable& IRenderer::enqueueCallback() {
  CallbackRenderable& rend = _callbacks.create();
  enqueueRenderable(&rend);
  return rend;
}

///////////////////////////////////////////////////////////////////////////////
// Performance monitoring and context management
///////////////////////////////////////////////////////////////////////////////

Context* IRenderer::GetTarget() const {
  return _target;
}

void IRenderer::setContext(Context* ptarg) {
  _target = ptarg;
}

void IRenderer::fakeDraw() {
  resetQueue();
}

}} // namespace ork::lev2
