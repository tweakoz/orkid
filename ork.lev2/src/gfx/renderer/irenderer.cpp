////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/pch.h>

#include <ork/kernel/Array.hpp>
#include <ork/kernel/timer.h>

///////////////////////////////////////////////////////////////////////////////

template class ork::fixedvector<U32, ork::lev2::RenderQueue::krqmaxsize>;
template class ork::fixedvector<const ork::lev2::RenderQueue::Node*, ork::lev2::RenderQueue::krqmaxsize>;
template class ork::fixedvector<ork::lev2::ModelRenderable, ork::lev2::IRenderer::kmaxrables>;
template class ork::fixedvector<ork::lev2::CallbackRenderable, ork::lev2::IRenderer::kmaxrables>;

namespace ork { namespace lev2 {
static const int kRenderbufferSize = 1024 << 10;

///////////////////////////////////////////////////////////////////////////////

IRenderer::IRenderer(Context* pTARG)
    : mPerformanceItem(0)
    , mpTarget(pTARG)
    , mRenderQueue() {
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::QueueRenderable(IRenderable* pRenderable) {
  mRenderQueue.QueueRenderable(pRenderable);
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::drawEnqueuedRenderables() {

  if (mPerformanceItem)
    mPerformanceItem->Enter();

  ///////////////////////////////////////////////////////
  size_t renderQueueSize = mRenderQueue.Size();
  mpTarget->debugPushGroup(FormatString("IRenderer::drawEnqueuedRenderables renderQueueSize<%zu>", renderQueueSize));

  if (renderQueueSize == 0) {
    mpTarget->debugPopGroup();
    return;
  }

  ///////////////////////////////////////////////////////
  mQueueSortKeys.clear();

  mRenderQueue.ExportRenderableNodes(mQueueSortNodes);

  mQueueSortKeys.resize(renderQueueSize);
  for (size_t i = 0; i < renderQueueSize; i++) {
    mQueueSortKeys[i] = mQueueSortNodes[i]->_renderable->ComposeSortKey(this);
  }

  ///////////////////////////////////////////////////////
  // orkprintf( "rqsize<%d>\n", renderQueueSize );

  U32& first = (*mQueueSortKeys.begin());

  mRadixSorter.Sort(&first, U32(renderQueueSize));

  U32* sortedRenderQueueIndices = mRadixSorter.GetIndices();

  int imdlcount = 0;

  float fruntot = 0.0f;

  ///////////////////////////////////////////////////////
  // attempt renderable grouping...
  ///////////////////////////////////////////////////////

  _groupedModels.clear();

  for (size_t i = 0; i < renderQueueSize; i++) {
    int sorted = mQueueSortKeys[i];
    mpTarget->debugMarker(FormatString("IRenderer::drawEnqueuedRenderables sorting index<%zu> sorted<%d>", i, sorted));
  }

  for (size_t i = 0; i < renderQueueSize; i++) {
    int sorted = sortedRenderQueueIndices[i];
    OrkAssert(sorted < U32(renderQueueSize));
    const RenderQueue::Node* pnode = mQueueSortNodes[sorted];
    mpTarget->debugPushGroup(FormatString("IRenderer::drawEnqueuedRenderables render item<%zu> node<%p>", i, pnode));
    pnode->_renderable->Render(this);
    mpTarget->debugPopGroup();
  }

  float favgrun = fruntot / float(imdlcount);
  ///////////////////////////////////////////////////////

  ResetQueue();
  ///////////////////////////////////////////////////////

  if (mPerformanceItem)
    mPerformanceItem->Exit();

  mpTarget->debugPopGroup();
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::ResetQueue(void) {
  mRenderQueue.Reset();
  mModels.clear();
  mCallbacks.clear();
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::RenderCallback(const CallbackRenderable& cbren) const {
  if (cbren.GetRenderCallback()) {
    auto context = GetTarget();
    RenderContextInstData RCID(context->topRenderContextFrameData());
    RCID.SetRenderer(this);
    RCID._dagrenderable = &cbren;
    cbren.GetRenderCallback()(RCID);
  }
}

}} // namespace ork::lev2
