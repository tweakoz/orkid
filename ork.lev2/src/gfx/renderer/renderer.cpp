////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
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

IRenderer::IRenderer(GfxTarget* pTARG)
    : mPerformanceItem(0)
    , mpTarget(pTARG)
    , mRenderQueue()
{}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::QueueRenderable(IRenderable* pRenderable) { mRenderQueue.QueueRenderable(pRenderable); }

///////////////////////////////////////////////////////////////////////////////

void IRenderer::DrawQueuedRenderables() {

  if (mPerformanceItem)
    mPerformanceItem->Enter();

	///////////////////////////////////////////////////////
  size_t renderQueueSize = mRenderQueue.Size();

  if (renderQueueSize == 0) {
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

  int irun      = 0;
  int imdlcount = 0;

  float fruntot = 0.0f;

	///////////////////////////////////////////////////////
  static const ork::lev2::ModelRenderable* spGroupedModels[RenderQueue::krqmaxsize];
  for (size_t i = 0; i < renderQueueSize; i++) {
    OrkAssert(sortedRenderQueueIndices[i] < U32(renderQueueSize));
    const RenderQueue::Node* pnode = mQueueSortNodes[sortedRenderQueueIndices[i]];
    OrkAssert(pnode);
    int igroupsize = 0;
    bool bren      = true;
    bool islast    = (i + 1 == renderQueueSize);

    // if( pnode )
    //{
    // u32 ukey = pnode->_renderable->ComposeSortKey( this );
    // printf( "ukek<0x%x>\n", ukey );
    //}
    if (i < renderQueueSize) {
      int sortedindex                        = sortedRenderQueueIndices[i + 1];
      const RenderQueue::Node* pnext         = islast ? 0 : mQueueSortNodes[sortedindex];
      const ork::lev2::ModelRenderable* pmdl = rtti::autocast(pnode->_renderable);

      if (pmdl) {
        imdlcount++;
      }
      if (pnext && pnode->_renderable->CanGroup(pnext->_renderable)) {
        spGroupedModels[irun] = pmdl;
        bren                  = false;
        irun++;
        fruntot += 1.0f;
      } else {
        if (irun > 0) {
          const ork::lev2::ModelRenderable* pmdl = rtti::autocast(pnode->_renderable);
          spGroupedModels[irun]                  = pmdl;
          igroupsize                             = (irun + 1);
        }
        irun = 0;
      }
    }
    // orkprintf( "rq <%d:%p> irun<%d> igroupsize<%d>\n", i, pnode->_renderable, irun, igroupsize );
    // orkprintf( "//////////////////////////\n" );
    if (bren) {
      if (igroupsize) {
        // render renderables as a group to amortize state setup costs
        // orkprintf( "rq<%d> Rendering Group size<%d>\n", i, igroupsize );
        // OrkAssert( mpTarget->FBI()->IsPickState() == false );

        this->RenderModelGroup(spGroupedModels, igroupsize);
      } else {
        pnode->_renderable->Render(this);
      }
    }
    // orkprintf( "//////////////////////////\n" );
  }
  float favgrun = fruntot / float(imdlcount);
	///////////////////////////////////////////////////////

  ResetQueue();
	///////////////////////////////////////////////////////

  if (mPerformanceItem)
    mPerformanceItem->Exit();
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::ResetQueue(void) {
  mRenderQueue.Reset();
  mModels.clear();
  mCallbacks.clear();
}

///////////////////////////////////////////////////////////////////////////////

void IRenderer::RenderCallback(const lev2::CallbackRenderable& cbren) const {
  lev2::RenderContextInstData MatCtx;
  lev2::GfxTarget* pTARG = GetTarget();
  MatCtx.SetRenderer(this);

  if (cbren.GetRenderCallback()) {
    cbren.GetRenderCallback()(MatCtx, pTARG, &cbren);
  }
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
