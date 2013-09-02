////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkstl.h>
#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/lev2/gfx/renderable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/kernel/tempstring.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/any.h>
#include <ork/kernel/fixedlut.h>
#include "componentfamily.h"
#include "component.h"
#include <ork/gfx/camera.h>
#include <ork/util/triple_buffer.h>
#include <ork/kernel/concurrent_queue.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

class CCameraData;
class Drawable;

namespace lev2 { class XgmModel; class XgmModelAsset; }
namespace lev2 { class XgmModelInst; }
namespace lev2 { class Renderer; }
namespace lev2 { class LightManager; }
namespace lev2 { class GfxMaterialFx; }
namespace lev2 { class GfxMaterialFxEffectInstance; }

namespace ent {

struct Layer
{
	PoolString	mLayerName;
	
	Layer();
	
};

class DrawableBufItem
{
public:

	DrawableBufItem() : mpDrawable(0), miBufferIndex(0) {}
	~DrawableBufItem()
	{
	}

	const Drawable* GetDrawable() const { return mpDrawable; }
	void SetDrawable( const Drawable* pdrw )
	{
		//printf( "bi<%p> SetDrw<%p>\n", this, pdrw );
		mpDrawable = pdrw; 
	}

	CMatrix4					mMatrix;
	anyp						mUserData0;
	anyp						mUserData1;
	int							miBufferIndex;

private:

	const Drawable*				mpDrawable;

};
class SceneInst;
class DrawableBuffer;
typedef ork::fixedlut<PoolString,ork::CCameraData,16>	CameraLut;
struct DrawableBufLayer
{
	static const int kmaxitems = 1024;
	DrawableBufItem				mDrawBufItems[kmaxitems];
	int							miItemIndex;
	int							miBufferIndex;
	bool HasData() const { return (miItemIndex!=-1); }
	void Reset(const DrawableBuffer& dB);
	DrawableBufItem& Queue();
	
	DrawableBufLayer();
};
///////////////////////////////////////////////////////////////////////////
class SceneDrawLayerData
{

public:

	SceneDrawLayerData();

	void Queue( SceneInst* psi, DrawableBuffer* dbuf );
	const CCameraData* GetCameraData( int icam ) const;
	const CCameraData* GetCameraData( const PoolString& named ) const;

private:
	
	CameraLut mCameraLut;

};

///////////////////////////////////////////////////////////////////////////

struct RenderSyncToken
{
	RenderSyncToken() : mFrameIndex(0) {}
	int mFrameIndex;
};

///////////////////////////////////////////////////////////////////////////

class DrawableBuffer
{
public:
	static const int kmaxlayers = 8;
	typedef ork::fixedlut<PoolString,DrawableBufLayer*,32>			LayerLut;

	CameraLut										mCameraDataLUT;
	DrawableBufLayer								mRawLayers[kmaxlayers];
	int												miNumLayersUsed;
	LayerLut										mLayerLut;
	int												miBufferIndex;
	int												miReadCount;
	//mutable ork::recursive_mutex					mBufferMutex;
	orkset<PoolString>								mLayers;
	static ork::MpMcBoundedQueue<RenderSyncToken>	mOfflineRenderSynchro;	
	static ork::MpMcBoundedQueue<RenderSyncToken>	mOfflineUpdateSynchro;	
	SceneDrawLayerData								mSDLD;

	void Reset();
	DrawableBuffer(int ibidx);
	~DrawableBuffer();

	static const int kmaxbuffers = 6;

	static const DrawableBuffer* LockReadBuffer(int lid);
	static DrawableBuffer* LockWriteBuffer(int lid);
	static void UnLockWriteBuffer(DrawableBuffer*db);
	static void UnLockReadBuffer(const DrawableBuffer*db);
	static void ClearAndSync();
	static void BeginClearAndSync();
	static void EndClearAndSync();
	static bool InsideClearAndSync() { return gbInsideClearAndSync; }
	
	DrawableBufLayer* MergeLayer( const PoolString& layername );

private:

	static concurrent_triple_buffer<DrawableBuffer> gBuffers;

	static bool										gbInsideClearAndSync;

};


class Drawable : public ork::Object
{
	RttiDeclareAbstract(Drawable, ork::Object);

public:

	Drawable(Entity* pent);
	virtual ~Drawable();

	virtual void QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* prenderer) const = 0;
	virtual void QueueToBuffer(DrawableBufLayer&buffer) const = 0;

	const ork::Object* GetOwner() const { return mOwner; }

	Entity* GetEntity() const { return mEntity; }
	void SetEntity(Entity* pent)
	{
		printf( "Drawable<%p> SetEnt<%p>\n", this, pent );
		mEntity = pent;
	}
	const DagNode *GetDagNode() const;

	void SetOwner( const ork::Object* owner ) { mOwner=owner; }

	void SetData( anyp data ) { mData = data; }
	const anyp& GetData() const { return mData; }

protected:

	const ork::Object*			mOwner;
	Entity*						mEntity;
	Layer*						mpLayer;
	anyp						mData;


};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class CameraDrawable : public Drawable
{
	RttiDeclareAbstract(CameraDrawable, Drawable);
public:
	CameraDrawable( Entity* pent, const CCameraData* camData);
	/*virtual*/ void QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* renderer) const;
	/*virtual*/ void QueueToBuffer(DrawableBufLayer&buffer) const;
private:
	const CCameraData* mCameraData;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ModelDrawable : public Drawable
{
	RttiDeclareConcrete(ModelDrawable, Drawable);
public:
	const ork::TransformNode3D*	mOverrideXF;
	void SetOverrideXF( const ork::TransformNode3D* ovr ) { mOverrideXF=ovr; }

	ModelDrawable(Entity *pent = NULL);
	~ModelDrawable();

	void SetModelInst(lev2::XgmModelInst* pModelInst);// { mModelInst = pModelInst; }
	lev2::XgmModelInst* GetModelInst() const { return mModelInst; }
	void SetScale( float fscale ) { mfScale=fscale; }
	float GetScale() const { return mfScale; }

	const CVector3& GetRotate() const { return mRotate; }
	const CVector3& GetOffset() const { return mOffset; }

	void SetRotate( const CVector3& v ) { mRotate=v; }
	void SetOffset( const CVector3& v ) { mOffset=v; }

	void SetEngineParamFloat(int idx, float fv);
	float GetEngineParamFloat(int idx) const;
	
	void ShowBoundingSphere( bool bflg ) { mbShowBoundingSphere=bflg; }
	
private:
	/*virtual*/ void QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* renderer) const;
	/*virtual*/ void QueueToBuffer(DrawableBufLayer&buffer) const;
	lev2::XgmModelInst*	mModelInst;
	lev2::XgmWorldPose*	mpWorldPose;
	float				mfScale;
	CVector3			mOffset;
	CVector3			mRotate;
	bool				mbShowBoundingSphere;
	
	static const int kMaxEngineParamFloats = ork::lev2::RenderContextInstData::kMaxEngineParamFloats;
	float mEngineParamFloats[kMaxEngineParamFloats];

};

///////////////////////////////////////////////////////////////////////////////

class ICallbackDrawableDataDestroyer
{
protected:
	virtual ~ICallbackDrawableDataDestroyer() {}
public:
	virtual void Destroy() = 0;
};

///////////////////////////////////////////////////////////////////////////////

class CallbackDrawable : public Drawable
{
	RttiDeclareAbstract(CallbackDrawable, Drawable);

	typedef void (*BufferCBType)(DrawableBufItem&cdb);

public:

	CallbackDrawable(Entity *pent);
	~CallbackDrawable();

	void SetDataDestroyer( ICallbackDrawableDataDestroyer* pdestroyer ) { mDataDestroyer = pdestroyer; }
	void SetCallback(lev2::CallbackRenderable::cbtype cb) { mCallback = cb; }
	void SetBufferCallback(BufferCBType cb) { mBufferCallback = cb; }
	U32 GetSortKey() const { return mSortKey; }
	void SetSortKey( U32 uv ) { mSortKey=uv; }

private:
	/*virtual*/ void QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* renderer) const;
	/*virtual*/ void QueueToBuffer(DrawableBufLayer&buffer) const;
	ICallbackDrawableDataDestroyer*		mDataDestroyer;
	lev2::CallbackRenderable::cbtype	mCallback;
	BufferCBType						mBufferCallback;
	U32 mSortKey;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}}
