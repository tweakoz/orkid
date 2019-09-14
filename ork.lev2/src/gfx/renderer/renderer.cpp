////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxenv.h>

#include <ork/kernel/timer.h>
#include <ork/kernel/Array.hpp>

///////////////////////////////////////////////////////////////////////////////

template class ork::fixedvector<U32,ork::lev2::RenderQueue::krqmaxsize>;
template class ork::fixedvector<const ork::lev2::RenderQueue::Node*,ork::lev2::RenderQueue::krqmaxsize>;

template class ork::fixedvector<ork::lev2::ModelRenderable,ork::lev2::Renderer::kmaxrables>;
template class ork::fixedvector<ork::lev2::CallbackRenderable,ork::lev2::Renderer::kmaxrables>;

namespace ork { namespace lev2 {
static const int kRenderbufferSize = 1024 << 10;

///////////////////////////////////////////////////////////////////////////////

Renderer::Renderer(GfxTarget* pTARG)
	: mpCurrentObject(0)
	, mpCurrentQueueObject(0)
	, mPerformanceItem(0)
	, mpTarget( pTARG )
	, mRenderQueue()
	//, mpCameraData(0)
{
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::QueueRenderable( IRenderable* pRenderable )
{
	mRenderQueue.QueueRenderable(pRenderable, this);
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::DrawQueuedRenderables()
{

	if(mPerformanceItem)
		mPerformanceItem->Enter();

	size_t renderQueueSize  = mRenderQueue.Size();

	if(renderQueueSize == 0)
	{
		return;
	}

	mQueueSortKeys.clear();

	mRenderQueue.ExportRenderableNodes(mQueueSortNodes);

	mQueueSortKeys.resize(renderQueueSize);
	for( size_t i = 0; i < renderQueueSize; i++ )
	{
		mQueueSortKeys[i] = mQueueSortNodes[i]->mpRenderable->ComposeSortKey( this );
	}

	//orkprintf( "rqsize<%d>\n", renderQueueSize );

	U32& first = (*mQueueSortKeys.begin());

	mRadixSorter.Sort(&first, U32(renderQueueSize));

	U32* sortedRenderQueueIndices = mRadixSorter.GetIndices();

	int irun = 0;
	int imdlcount = 0;

	float fruntot = 0.0f;

	static const ork::lev2::ModelRenderable*	spGroupedModels[RenderQueue::krqmaxsize];
	for( size_t i = 0; i < renderQueueSize; i++ )
	{
		OrkAssert(sortedRenderQueueIndices[i] < U32(renderQueueSize));
		const RenderQueue::Node *pnode = mQueueSortNodes[ sortedRenderQueueIndices[i] ];
		OrkAssert(pnode);
		int igroupsize = 0;
		bool bren = true;
		bool islast = (i+1 == renderQueueSize);

		//if( pnode )
		//{
			//u32 ukey = pnode->mpRenderable->ComposeSortKey( this );
			//printf( "ukek<0x%x>\n", ukey );
		//}
		if( i < renderQueueSize )
		{	int sortedindex = sortedRenderQueueIndices[i+1];
			const RenderQueue::Node *pnext = islast ? 0 : mQueueSortNodes[ sortedindex ];
			const ork::lev2::ModelRenderable* pmdl = rtti::autocast(pnode->mpRenderable);


			if( pmdl )
			{
				imdlcount++;
			}
			if( pnext && pnode->mpRenderable->CanGroup( pnext->mpRenderable ) )
			{	spGroupedModels[irun] = pmdl;
				bren = false;
				irun++;
				fruntot += 1.0f;
			}
			else
			{	if( irun>0 )
				{	const ork::lev2::ModelRenderable* pmdl = rtti::autocast(pnode->mpRenderable);
					spGroupedModels[irun] = pmdl;
					igroupsize = (irun+1);
				}
				irun = 0;
			}
		}
		//orkprintf( "rq <%d:%p> irun<%d> igroupsize<%d>\n", i, pnode->mpRenderable, irun, igroupsize );
		//orkprintf( "//////////////////////////\n" );
		if( bren )
		{	if( igroupsize ) // render renderables as a group to amortize state setup costs
			{
				//orkprintf( "rq<%d> Rendering Group size<%d>\n", i, igroupsize );
				//OrkAssert( mpTarget->FBI()->IsPickState() == false );

				this->RenderModelGroup( spGroupedModels, igroupsize );
			}
			else
			{
				//orkprintf( "rq<%d> Rendering node <%p>\n", i, pnode->mpRenderable );
				mpCurrentQueueObject = pnode->mpContextObject;
				pnode->mpRenderable->Render( this );
			}
		}
		//orkprintf( "//////////////////////////\n" );
	}
	float favgrun = fruntot/float(imdlcount);

	ResetQueue();

	if(mPerformanceItem)
		mPerformanceItem->Exit();
}

///////////////////////////////////////////////////////////////////////////////

void Renderer::ResetQueue( void )
{
	mpCurrentObject = 0;
	mpCurrentQueueObject = 0;
	mRenderQueue.Reset();

	mModels.clear();
	mCallbacks.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Renderer::PushPickID(const Object *pObject)
{
	mpCurrentObject = pObject;

}

void Renderer::PopPickID()
{
	mpCurrentObject = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


} } // namespace ork
