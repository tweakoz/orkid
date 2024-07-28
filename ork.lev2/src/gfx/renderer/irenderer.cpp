////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/pch.h>

#include <ork/kernel/Array.hpp>
#include <ork/kernel/timer.h>
#include <ork/profiling.inl>

///////////////////////////////////////////////////////////////////////////////

template class ork::fixedvector<U32, ork::lev2::RenderQueue::krqmaxsize>;
template class ork::fixedvector<const ork::lev2::RenderQueue::Node*, ork::lev2::RenderQueue::krqmaxsize>;
template class ork::fixedvector<ork::lev2::ModelRenderable, ork::lev2::IRenderer::kmaxrables>;
template class ork::fixedvector<ork::lev2::CallbackRenderable, ork::lev2::IRenderer::kmaxrables>;

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

IRenderer::IRenderer(Context* pTARG)
    : _target(pTARG)
    , _unsortedNodes()
    , mPerformanceItem(0) {
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::enqueueRenderable(IRenderable* renderable) {
  _unsortedNodes.enqueueRenderable(renderable);
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::drawEnqueuedRenderables(bool reset_after) {

  EASY_BLOCK("IRenderer::DER1", profiler::colors::Red);

  if (mPerformanceItem)
    mPerformanceItem->Enter();

  ///////////////////////////////////////////////////////
  size_t renderQueueSize = _unsortedNodes.Size();
  _target->debugPushGroup(FormatString("IRenderer::drawEnqueuedRenderables renderQueueSize<%zu>", renderQueueSize));

  //if(_debugLog){
    //printf( "renderQueueSize<%zu>\n", renderQueueSize);
  //}

  if (renderQueueSize == 0) {
    _target->debugPopGroup();
    if(reset_after){
      resetQueue();
    }
    return;
  }

  ///////////////////////////////////////////////////////
  _sortkeys.clear();
  EASY_END_BLOCK;
  EASY_BLOCK("IRenderer::DER2", profiler::colors::Red);
  _unsortedNodes.exportRenderableNodes(_sortedNodes);

  _sortkeys.resize(renderQueueSize);
  for (size_t i = 0; i < renderQueueSize; i++) {
    int skey     = _sortedNodes[i]->_renderable->ComposeSortKey(this);
    _sortkeys[i] = skey;
    if(_debugLog){
      printf( "skey<%zu:%d>\n", i, skey );
    }
  }
  EASY_END_BLOCK;

  ///////////////////////////////////////////////////////
  // orkprintf( "rqsize<%d>\n", renderQueueSize );
  EASY_BLOCK("IRenderer::DER3", profiler::colors::Red);

  U32& first = (*_sortkeys.begin());

  _radixsorter.Sort(&first, U32(renderQueueSize));

  U32* sortedRenderQueueIndices = _radixsorter.GetIndices();

  int imdlcount = 0;

  float fruntot = 0.0f;
  EASY_END_BLOCK;

  ///////////////////////////////////////////////////////

  EASY_BLOCK("IRenderer::DER4", profiler::colors::Red);

  for (size_t i = 0; i < renderQueueSize; i++) {
    int sorted = _sortkeys[i];
    _target->debugMarker(FormatString("IRenderer::drawEnqueuedRenderables sorting index<%zu> sorted<%d>", i, sorted));
  }

  EASY_END_BLOCK;

  EASY_BLOCK("IRenderer::DER5", profiler::colors::Red);

  //printf("renderQueueSize<%zu>\n", renderQueueSize);

  for (size_t i = 0; i < renderQueueSize; i++) {
    int sorted = sortedRenderQueueIndices[i];
    // printf( "sorted<%d:%d>\n", i, sorted );
    OrkAssert(sorted < U32(renderQueueSize));
    const RenderQueue::Node* pnode = _sortedNodes[sorted];
    _target->debugPushGroup(FormatString("IRenderer::drawEnqueuedRenderables render item<%zu> node<%p>", i, pnode));
    pnode->_renderable->Render(this);
    _target->debugPopGroup();
  }

  float favgrun = fruntot / float(imdlcount);
  ///////////////////////////////////////////////////////
  EASY_END_BLOCK;

  // resetQueue();
  ///////////////////////////////////////////////////////

  EASY_BLOCK("IRenderer::DER6", profiler::colors::Red);

  if (mPerformanceItem)
    mPerformanceItem->Exit();

  EASY_END_BLOCK;

  EASY_BLOCK("IRenderer::DER7", profiler::colors::Red);

  _target->debugPopGroup();
    if(reset_after){
      resetQueue();
    }
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::resetQueue(void) {
  _unsortedNodes.Reset();
  _models.clear();
  _skeletons.clear();
  _callbacks.clear();
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::_renderCallbackRenderable(const CallbackRenderable& cbren) const {
  if (cbren.GetRenderCallback()) {
    auto context = GetTarget();
    RenderContextInstData RCID(context->topRenderContextFrameData());
    RCID.SetRenderer(this);
    RCID.setRenderable(&cbren);
    context->RefModColor() = cbren._modColor;
    cbren.GetRenderCallback()(RCID);
  }
}

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Each Renderer implements this function as a helper for Renderables when composing their sort keys
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRenderer::SetPerformanceItem(PerformanceItem* perfitem) {
  mPerformanceItem = perfitem;
}

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
