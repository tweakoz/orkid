////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "component.h"
#include "componentfamily.h"
#include <ork/gfx/camera.h>
#include <ork/kernel/any.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/tempstring.h>
#include <ork/lev2/gfx/renderable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/orkstl.h>
#include <ork/rtti/RTTI.h>
#include <ork/util/multi_buffer.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

typedef fixedlut<PoolString, CameraData, 16> CameraLut;

class Drawable;

namespace lev2 {
class XgmModel;
class XgmModelAsset;
} // namespace lev2
namespace lev2 {
class XgmModelInst;
}
namespace lev2 {
class Renderer;
}
namespace lev2 {
class LightManager;
}
namespace lev2 {
class GfxMaterialFx;
}
namespace lev2 {
class GfxMaterialFxEffectInstance;
}

namespace ent {

class Simulation;
class DrawableBuffer;

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
  RenderSyncToken() : mFrameIndex(0) {}
  int mFrameIndex;
};

///////////////////////////////////////////////////////////////////////////

class DrawableBuffer {
public:
  static const int kmaxlayers = 8;
  typedef ork::fixedlut<PoolString, DrawableBufLayer*, kmaxlayers> LayerLut;

  CameraLut mCameraDataLUT;
  DrawableBufLayer mRawLayers[kmaxlayers];
  int miNumLayersUsed;
  LayerLut mLayerLut;
  int miBufferIndex;
  int miReadCount;
  orkset<PoolString> mLayers;
  static ork::MpMcBoundedQueue<RenderSyncToken> mOfflineRenderSynchro;
  static ork::MpMcBoundedQueue<RenderSyncToken> mOfflineUpdateSynchro;
  static ork::atomic<bool> gbInsideClearAndSync;

  void Reset();
  DrawableBuffer(int ibidx);
  ~DrawableBuffer();

  static const int kmaxbuffers = 6;

  static const DrawableBuffer* BeginDbRead(int lid);
  static void EndDbRead(const DrawableBuffer* db);

  static DrawableBuffer* LockWriteBuffer(int lid);
  static void UnLockWriteBuffer(DrawableBuffer* db);

  static void BeginClearAndSyncWriters();
  static void EndClearAndSyncWriters();

  static void BeginClearAndSyncReaders();
  static void EndClearAndSyncReaders();
  static void ClearAndSyncReaders();
  static void ClearAndSyncWriters();

  const CameraData* GetCameraData(int icam) const;
  const CameraData* GetCameraData(const PoolString& named) const;

  DrawableBufLayer* MergeLayer(const PoolString& layername);

}; // ~1MiB

///////////////////////////////////////////////////////////////////////////

class Drawable : public ork::Object {
  RttiDeclareAbstract(Drawable, ork::Object);

public:

  typedef ork::lev2::IRenderable::var_t var_t;

  Drawable();
  virtual ~Drawable();

  virtual void QueueToRenderer(const DrawableBufItem& item,
                               lev2::Renderer* prenderer) const = 0; // 	AssertOnOpQ2( MainThreadOpQ() );
  virtual void QueueToLayer(const DrawQueueXfData& xfdata,
                            DrawableBufLayer& buffer) const = 0; // AssertOnOpQ2( UpdateSerialOpQ() );

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

class CameraDrawable : public Drawable {
  RttiDeclareAbstract(CameraDrawable, Drawable);

public:
  CameraDrawable(Entity* pent, const CameraData* camData);

private:
  void QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* renderer) const override;
  void QueueToLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const override;
  const CameraData* mCameraData;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ModelDrawable : public Drawable {
  RttiDeclareConcrete(ModelDrawable, Drawable);

public:
  ModelDrawable(Entity* pent = NULL);
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
  void QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* renderer) const override;
  void QueueToLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const override;

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
  CallbackDrawable(Entity* pent);
  ~CallbackDrawable();

  void SetDataDestroyer(ICallbackDrawableDataDestroyer* pdestroyer) { mDataDestroyer = pdestroyer; }
  void SetRenderCallback(lev2::CallbackRenderable::cbtype_t cb) { mRenderCallback = cb; }
  void SetQueueToLayerCallback(Q2LCBType cb) { mQueueToLayerCallback = cb; }
  U32 GetSortKey() const { return mSortKey; }
  void SetSortKey(U32 uv) { mSortKey = uv; }
  void QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* renderer) const final;
  void QueueToLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const final;

private:
  ICallbackDrawableDataDestroyer* mDataDestroyer;
  lev2::CallbackRenderable::cbtype_t mRenderCallback;
  Q2LCBType mQueueToLayerCallback;
  U32 mSortKey;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ent
} // namespace ork
