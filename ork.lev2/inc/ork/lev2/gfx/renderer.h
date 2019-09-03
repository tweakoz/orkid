////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

#ifndef _ORK_ENTITY_RENDERER_H
#define _ORK_ENTITY_RENDERER_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderable.h>
#include <ork/gfx/radixsort.h>

#include <ork/lev2/gfx/renderqueue.h>

#include "lev2renderer.h"

#include <ork/lev2/gfx/renderer_base.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

class IZoneManager;
class PerformanceItem;
class CameraData;

namespace lev2 {

class GfxTarget;

class Renderer // Abstract Renderer that doesnt use virtuals (function pointers embedded in object, not in vtable)
{
public:
	//static const int kmaxnodes = 4096;
	static const int kmaxrables = 4096;
	static const int kmaxrablesmed = 1024;
	static const int kmaxrablessm = 64;

private:
	GfxTarget*		mpTarget;

	ork::fixedvector<U32,RenderQueue::krqmaxsize>						mQueueSortKeys;
	ork::fixedvector<const RenderQueue::Node*,RenderQueue::krqmaxsize>	mQueueSortNodes;


	ork::fixedvector<BoxRenderable,kmaxrablessm>			mBoxes;
	ork::fixedvector<ModelRenderable,kmaxrables>			mModels;
	ork::fixedvector<FrustumRenderable,kmaxrablessm>		mFrustums;
	ork::fixedvector<SphereRenderable,kmaxrables>			mSpheres;
	ork::fixedvector<CallbackRenderable,kmaxrablesmed>			mCallbacks;

public:

	/******************************************************************************************************************
	 * Immediate Rendering (sort of, actually just submit the renderable to the target, which might itself place into a display list)
	 ******************************************************************************************************************/

	virtual void RenderBox( const BoxRenderable & CubeRen ) const = 0;
	virtual void RenderModel( const ModelRenderable & ModelRen, RenderGroupState rgs=ERGST_NONE ) const = 0;
	virtual void RenderModelGroup( const ModelRenderable** Renderables, int inumr ) const = 0;
	virtual void RenderFrustum( const FrustumRenderable & Frusren ) const = 0;
	virtual void RenderSphere( const SphereRenderable & SphereRen ) const = 0;
	virtual void RenderCallback( const CallbackRenderable & cbren ) const = 0;

	/******************************************************************************************************************
	 * Deferred rendering
	 ******************************************************************************************************************/

	BoxRenderable & QueueBox()				{ BoxRenderable& rend=mBoxes.create(); QueueRenderable(&rend); return rend; }
	ModelRenderable & QueueModel()			{ ModelRenderable& rend=mModels.create(); QueueRenderable(&rend); return rend; }
	FrustumRenderable & QueueFrustum()		{ FrustumRenderable& rend=mFrustums.create(); QueueRenderable(&rend); return rend; }
	SphereRenderable & QueueSphere()		{ SphereRenderable& rend=mSpheres.create(); QueueRenderable(&rend); return rend; }
	CallbackRenderable & QueueCallback()	{ CallbackRenderable& rend=mCallbacks.create(); QueueRenderable(&rend); return rend; }

	void QueueRenderable( IRenderable *pRenderable );

	/// Each Renderer implements this function as a helper for Renderables when composing their sort keys
	virtual U32				ComposeSortKey( U32 texIndex, U32 depthIndex, U32 passIndex, U32 transIndex ) const { return 0; }

	void					DrawQueuedRenderables();

	const Object *GetCurrentQueuedObject() const { return mpCurrentQueueObject; }
	const Object *GetCurrentObject() const { return mpCurrentObject; }

	inline void SetPerformanceItem(PerformanceItem* perfitem) { mPerformanceItem = perfitem; }

	void PushPickID(const Object *pObject);
	void PopPickID();

	GfxTarget* GetTarget() const { return mpTarget;	}
	void SetTarget( GfxTarget *ptarg ) { mpTarget = ptarg; }

	void FakeDraw() { ResetQueue(); }

	//void BindCameraData( const ork::CameraData* pc ) { mpCameraData=pc; }
	//const ork::CameraData* GetCameraData() const { return mpCameraData; }

protected:
	void						ResetQueue( void );
	RadixSort					mRadixSorter;
	const Object*				mpCurrentObject;
	const Object*				mpCurrentQueueObject;
	RenderQueue					mRenderQueue;
	//const ork::CameraData*		mpCameraData;

	PerformanceItem* mPerformanceItem;

	Renderer( GfxTarget* pTARG );
};

} } // namespace ork / lev2

#endif
