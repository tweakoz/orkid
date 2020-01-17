////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/any.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/tempstring.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/orkstl.h>
#include <ork/rtti/RTTI.h>
#include <ork/util/multi_buffer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2{

typedef fixedlut<PoolString, const CameraData*, 16> CameraDataLut;
typedef fixedlut<PoolString, CameraMatrices, 16> CameraMatricesLut;
class Simulation;
class DrawableBuffer;
class Drawable;
class XgmModel;
class XgmModelAsset;
class XgmModelInst;
class IRenderer;
class LightManager;
class GfxMaterialFx;
class GfxMaterialFxEffectInstance;

///////////////////////////////////////////////////////////////////////////
// todo find a better name
///////////////////////////////////////////////////////////////////////////

struct DrawableOwner : public ork::Object {
  typedef orkvector<Drawable *> DrawableVector;
	typedef orklut<PoolString,DrawableVector*> LayerMap;

  DrawableOwner();
  ~DrawableOwner();

  void _addDrawable( const PoolString& layername, Drawable* pdrw );

	DrawableVector* GetDrawables( const PoolString& layer );
	const DrawableVector* GetDrawables( const PoolString& layer ) const;

	const LayerMap& GetLayers() const { return mLayerMap; }

  LayerMap								mLayerMap;

private:

  RttiDeclareAbstract( DrawableOwner, ork::Object );
};

///////////////////////////////////////////////////////////////////////////

struct DrawQueueXfData {
  ork::fmtx4 mWorldMatrix;
};

///////////////////////////////////////////////////////////////////////////

class DrawableBufItem {
public:

  typedef ork::lev2::IRenderable::var_t var_t;

  DrawableBufItem() : mpDrawable(0), miBufferIndex(0) {}

  ~DrawableBufItem() {}

  const Drawable* GetDrawable() const { return mpDrawable; }
  void SetDrawable(const Drawable* pdrw) { mpDrawable = pdrw; }

  DrawQueueXfData mXfData;
  var_t mUserData0;
  var_t mUserData1;
  int miBufferIndex;

private:
  const Drawable* mpDrawable;

}; // ~100 bytes

///////////////////////////////////////////////////////////////////////////

struct Layer {
  PoolString mLayerName;
  Layer();
};

///////////////////////////////////////////////////////////////////////////

struct DrawableBufLayer {
  static const int kmaxitems = 4096;
  DrawableBufItem mDrawBufItems[kmaxitems];
  int miItemIndex;
  int miBufferIndex;
  bool HasData() const { return (miItemIndex != -1); }
  void Reset(const DrawableBuffer& dB);
  DrawableBufItem& Queue(const DrawQueueXfData& xfdata, const Drawable* d);

  DrawableBufLayer();
}; // ~ 100K

///////////////////////////////////////////////////////////////////////////

struct RenderSyncToken {
  RenderSyncToken() : mFrameIndex(-1) {}
  bool valid() const { return mFrameIndex!=-1; }
  int mFrameIndex;
};

///////////////////////////////////////////////////////////////////////////

typedef std::function<void(lev2::RenderContextFrameData&RCFD)> prerendercallback_t;

class DrawableBuffer {
public:
  static const int kmaxlayers = 8;
  typedef ork::fixedlut<PoolString, DrawableBufLayer*, kmaxlayers> LayerLut;
  typedef ork::fixedlut<int, prerendercallback_t, 32> CallbackLut_t;

  CameraDataLut _cameraDataLUT;
  DrawableBufLayer mRawLayers[kmaxlayers];
  int miNumLayersUsed;
  LayerLut mLayerLut;
  int miBufferIndex;
  int miReadCount;
  orkset<PoolString> mLayers;
  CallbackLut_t _preRenderCallbacks;

  static ork::MpMcBoundedQueue<RenderSyncToken> mOfflineRenderSynchro;
  static ork::MpMcBoundedQueue<RenderSyncToken> mOfflineUpdateSynchro;
  static ork::atomic<bool> gbInsideClearAndSync;


  void Reset();
  DrawableBuffer(int ibidx);
  ~DrawableBuffer();

  void setPreRenderCallback(int key,prerendercallback_t cb);
  void invokePreRenderCallbacks(lev2::RenderContextFrameData&RCFD) const;

  static const int kmaxbuffers = 6;

  static const DrawableBuffer* acquireReadDB(int lid);
  static void releaseReadDB(const DrawableBuffer* db);

  static DrawableBuffer* LockWriteBuffer(int lid);
  static void UnLockWriteBuffer(DrawableBuffer* db);

  static RenderSyncToken acquireRenderToken();

  static void BeginClearAndSyncWriters();
  static void EndClearAndSyncWriters();

  static void BeginClearAndSyncReaders();
  static void EndClearAndSyncReaders();
  static void ClearAndSyncReaders();
  static void ClearAndSyncWriters();

  const CameraData* cameraData(int icam) const;
  const CameraData* cameraData(const PoolString& named) const;

  DrawableBufLayer* MergeLayer(const PoolString& layername);

  void enqueueLayerToRenderQueue(const PoolString& LayerName,lev2::IRenderer* renderer) const;

}; // ~1MiB

///////////////////////////////////////////////////////////////////////////

class Drawable : public ork::Object {
  RttiDeclareAbstract(Drawable, ork::Object);

public:

  typedef ork::lev2::IRenderable::var_t var_t;

  Drawable();
  virtual ~Drawable();

  virtual void enqueueToRenderQueue(const DrawableBufItem& item,
                               lev2::IRenderer* prenderer) const = 0; // 	AssertOnOpQ2( mainSerialQueue() );
  virtual void enqueueOnLayer(const DrawQueueXfData& xfdata,
                            DrawableBufLayer& buffer) const = 0; // AssertOnOpQ2( updateSerialQueue() );

  const ork::Object* GetOwner() const { return mOwner; }
  void SetOwner(const ork::Object* owner) { mOwner = owner; }

  void SetUserDataA(var_t data) { mDataA = data; }
  const var_t& GetUserDataA() const { return mDataA; }
  void SetUserDataB(var_t data) { mDataB = data; }
  const var_t& GetUserDataB() const { return mDataB; }
  bool IsEnabled() const { return mEnabled; }
  void Enable() { mEnabled = true; }
  void Disable() { mEnabled = false; }

protected:
  const ork::Object* mOwner;
  Layer* mpLayer;
  var_t mDataA;
  var_t mDataB;
  bool mEnabled;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ModelDrawable : public Drawable {
  RttiDeclareConcrete(ModelDrawable, Drawable);

public:
  ModelDrawable(DrawableOwner* owner = NULL);
  ~ModelDrawable();

  void SetModelInst(lev2::XgmModelInst* pModelInst); // { mModelInst = pModelInst; }
  lev2::XgmModelInst* GetModelInst() const { return mModelInst; }
  void SetScale(float fscale) { mfScale = fscale; }
  float GetScale() const { return mfScale; }

  const fvec3& GetRotate() const { return mRotate; }
  const fvec3& GetOffset() const { return mOffset; }

  void SetRotate(const fvec3& v) { mRotate = v; }
  void SetOffset(const fvec3& v) { mOffset = v; }

  void SetEngineParamFloat(int idx, float fv);
  float GetEngineParamFloat(int idx) const;

  void ShowBoundingSphere(bool bflg) { mbShowBoundingSphere = bflg; }

private:
  void enqueueToRenderQueue(const DrawableBufItem& item, lev2::IRenderer* renderer) const override;
  void enqueueOnLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const override;

  lev2::XgmModelInst* mModelInst;
  lev2::XgmWorldPose* mpWorldPose;
  float mfScale;
  fvec3 mOffset;
  fvec3 mRotate;
  bool mbShowBoundingSphere;

  static const int kMaxEngineParamFloats = ork::lev2::RenderContextInstData::kMaxEngineParamFloats;
  float mEngineParamFloats[kMaxEngineParamFloats];
};

///////////////////////////////////////////////////////////////////////////////

class ICallbackDrawableDataDestroyer {
protected:
  virtual ~ICallbackDrawableDataDestroyer() {}

public:
  virtual void Destroy() = 0;
};

///////////////////////////////////////////////////////////////////////////////

class CallbackDrawable : public Drawable {
  RttiDeclareAbstract(CallbackDrawable, Drawable);

  typedef void (*Q2LCBType)(DrawableBufItem& cdb);

public:
  CallbackDrawable(DrawableOwner* owner);
  ~CallbackDrawable();

  void SetDataDestroyer(ICallbackDrawableDataDestroyer* pdestroyer) { mDataDestroyer = pdestroyer; }
  void SetRenderCallback(lev2::CallbackRenderable::cbtype_t cb) { mRenderCallback = cb; }
  void SetenqueueOnLayerCallback(Q2LCBType cb) { menqueueOnLayerCallback = cb; }
  U32 GetSortKey() const { return mSortKey; }
  void SetSortKey(U32 uv) { mSortKey = uv; }
  void enqueueToRenderQueue(const DrawableBufItem& item, lev2::IRenderer* renderer) const final;
  void enqueueOnLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const final;

private:
  ICallbackDrawableDataDestroyer* mDataDestroyer;
  lev2::CallbackRenderable::cbtype_t mRenderCallback;
  Q2LCBType menqueueOnLayerCallback;
  U32 mSortKey;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
