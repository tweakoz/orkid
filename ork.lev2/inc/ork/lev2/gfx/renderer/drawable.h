////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkstl.h>
#include <ork/kernel/any.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/semaphore.h>
#include <ork/kernel/tempstring.h>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/rtti/RTTI.h>
//#include <ork/util/triple_buffer.h>

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/dbgfontman.h>

template <typename T> struct concurrent_triple_buffer;

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////
// todo find a better name
///////////////////////////////////////////////////////////////////////////

using onrenderable_fn_t = std::function<void(IRenderable*)>;
using on_render_rcid_t = std::function<void(lev2::RenderContextInstData& RCID)>;

struct DrawableOwner : public ork::Object {
  typedef orkvector<drawable_ptr_t> DrawableVector;
  typedef orklut<std::string, DrawableVector*> LayerMap;

  DrawableOwner();
  ~DrawableOwner();

  void _addDrawable(const std::string& layername, drawable_ptr_t pdrw);

  DrawableVector* GetDrawables(const std::string& layer);
  const DrawableVector* GetDrawables(const std::string& layer) const;

  const LayerMap& GetLayers() const {
    return mLayerMap;
  }

  LayerMap mLayerMap;

private:
  RttiDeclareAbstract(DrawableOwner, ork::Object);
};

///////////////////////////////////////////////////////////////////////////

struct DrawQueueXfData {
  DrawQueueXfData();
  decompxf_ptr_t _worldTransform;
  fvec4 _modcolor;
  bool _use_modcolor = false;
};

///////////////////////////////////////////////////////////////////////////

struct DrawableBufItem {
public:
  typedef ork::lev2::IRenderable::var_t var_t;

  using usermap_t = std::unordered_map<uint32_t, rendervar_t>;

  DrawableBufItem();
  ~DrawableBufItem();

  void terminate();

  const Drawable* _drawable;
  DrawQueueXfData mXfData;
  int _bufferIndex;
  int _serialno = 0;
  int _sortkey = 0;
  onrenderable_fn_t _onrenderable;
  std::atomic<int> _state;
  usermap_t _usermap;

}; // ~100 bytes

///////////////////////////////////////////////////////////////////////////

struct LayerData { /// deprecated (this struct does not do much...)
  LayerData();
  std::string _layerName;
};

///////////////////////////////////////////////////////////////////////////

struct DrawableBufLayer {

  using itemvect_t = std::vector<drawablebufitem_constptr_t>;

  std::string _name;
  LockedResource<itemvect_t> _items;
  int _itemIndex;
  int miBufferIndex;
  std::atomic<int> _state;
  int _sortkey = 0;
  bool HasData() const {
    return (_itemIndex != -1);
  }
  void Reset(const DrawableBuffer& dB);
  drawablebufitem_ptr_t enqueueDrawable(const DrawQueueXfData& xfdata, const Drawable* d);

  DrawableBufLayer();
  ~DrawableBufLayer();

}; // ~ 100K

///////////////////////////////////////////////////////////////////////////

typedef std::function<void(lev2::RenderContextFrameData& RCFD)> prerendercallback_t;

struct DrawableBuffer {
public:
  using usermap_t   = orklut<CrcString, rendervar_t>;

  static std::atomic<int> _gate;

  static const int kmaxlayers = 8;
  typedef ork::fixedlut<std::string, DrawableBufLayer*, kmaxlayers> LayerLut;
  typedef ork::fixedlut<int, prerendercallback_t, 32> CallbackLut_t;

  LockedResource<cameradatalut_ptr_t> _cameraDataLUT;
  DrawableBufLayer mRawLayers[kmaxlayers];
  LayerLut mLayerLut;
  orkset<std::string> mLayers;
  CallbackLut_t _preRenderCallbacks;
  usermap_t _userProperties;
  std::atomic<int> _state;

  int miNumLayersUsed = 0;
  int miBufferIndex   = -1;
  int miReadCount     = 0;

  const usermap_t& userProperties() const {
    return _userProperties;
  }
  usermap_t& userProperties() {
    return _userProperties;
  }

  void setUserProperty(CrcString, rendervar_t data);
  void unSetUserProperty(CrcString);
  rendervar_t getUserProperty(CrcString prop) const;

  template <typename T> void setUserPropertyAs(CrcString key, const T& data) {
    rendervar_t rv;
    rv.set<T>(data);
    auto it = _userProperties.find(key);
    if (it == _userProperties.end())
      _userProperties.AddSorted(key, rv);
    else
      it->second = rv;
  }

  template <typename T> std::shared_ptr<T> //
  mergeSharedUserPropertyAs(CrcString key) {
    std::shared_ptr<T> shared;
    auto it = _userProperties.find(key);
    if (it == _userProperties.end()) {
      rendervar_t rv;
      shared = rv.makeShared<T>();
      _userProperties.AddSorted(key, rv);
    } else {
      shared = it->second.getShared<T>();
    }
    return shared;
  }

  template <typename T> std::shared_ptr<T> //
  getSharedUserPropertyAs(CrcString key) const {
    std::shared_ptr<T> shared;
    auto it = _userProperties.find(key);
    OrkAssert(it != _userProperties.end());
    return it->second.getShared<T>();
  }

  template <typename T> const T& getUserPropertyAs(CrcString key) const {
    rendervar_t rv;
    auto it = _userProperties.find(key);
    OrkAssert(it != _userProperties.end());
    return it->second.get<T>();
  }

  static ork::atomic<bool> gbInsideClearAndSync;

  void copyCameras(const CameraDataLut& cameras);
  void Reset();
  void terminate();
  DrawableBuffer(int ibidx);
  ~DrawableBuffer();

  void setPreRenderCallback(int key, prerendercallback_t cb);
  void invokePreRenderCallbacks(lev2::RenderContextFrameData& RCFD) const;

  ///////////////////////////////////////////////////////

  static void BeginClearAndSyncWriters();
  static void EndClearAndSyncWriters();

  static void BeginClearAndSyncReaders();
  static void EndClearAndSyncReaders();
  static void ClearAndSyncReaders();
  static void ClearAndSyncWriters();
  static void terminateAll();

  cameradata_constptr_t cameraData(int icam) const;
  cameradata_constptr_t cameraData(const std::string& named) const;

  DrawableBufLayer* MergeLayer(const std::string& layername);

  void enqueueLayerToRenderQueue(const std::string& LayerName, lev2::IRenderer* renderer) const;

}; // ~1MiB

///////////////////////////////////////////////////////////////////////////

struct DrawBufContext {

  DrawBufContext();
  ~DrawBufContext();

  DrawableBuffer* acquireForWriteLocked();
  void releaseFromWriteLocked(DrawableBuffer* db);
  const DrawableBuffer* acquireForReadLocked();
  void releaseFromReadLocked(const DrawableBuffer* db);

  using tbuf_t     = concurrent_triple_buffer<DrawableBuffer>;
  using tbuf_ptr_t = std::shared_ptr<tbuf_t>;

  tbuf_ptr_t _triple;

  std::string _name;
  ork::mutex _lockedBufferMutex;
  ork::semaphore _rendersync_sema;
  ork::semaphore _rendersync_sema2;
  int _rendersync_counter = 0;
  std::shared_ptr<DrawableBuffer> _lockeddrawablebuffer;
};

////////////////////////////////////////////////////////////////////////////////

struct AcquiredUpdateDrawBuffer{
  DrawableBuffer* _DB = nullptr;
};

////////////////////////////////////////////////////////////////////////////////

struct AcquiredRenderDrawBuffer{
  AcquiredRenderDrawBuffer( rcfd_ptr_t rcfd=nullptr );
  const DrawableBuffer* _DB;
  rcfd_ptr_t _RCFD;
};

///////////////////////////////////////////////////////////////////////////

struct Drawable {

  typedef ork::lev2::IRenderable::var_t var_t;

  Drawable();
  virtual ~Drawable();

  virtual void enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* prenderer) const;

  virtual drawablebufitem_ptr_t enqueueOnLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const;

  void SetUserDataA(var_t data) {
    mDataA = data;
  }
  const var_t& GetUserDataA() const {
    return mDataA;
  }
  void SetUserDataB(var_t data) {
    mDataB = data;
  }
  const var_t& GetUserDataB() const {
    return mDataB;
  }
  bool IsEnabled() const {
    return mEnabled;
  }
  void Enable() {
    mEnabled = true;
  }
  void Disable() {
    mEnabled = false;
  }
  void terminate();

  virtual bool isInstanced() const {
    return false;
  }

  pickvariant_t _pickID;
  var_t mDataA;
  var_t mDataB;
  varmap::varmap_ptr_t _properties;
  fvec4 _modcolor;
  onrenderable_fn_t _onrenderable;
  on_render_rcid_t _rendercb;
  on_render_rcid_t _rendercb_user;
  bool mEnabled;
  bool _pickable = true;
  std::string _name;

  uint32_t _sortkey = 0;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct DrawableCache {
  drawable_ptr_t fetch(drawabledata_ptr_t data);
  std::unordered_map<drawabledata_ptr_t, drawable_ptr_t> _cache;
};

///////////////////////////////////////////////////////////////////////////////

struct DrawableData : public ork::Object { // todo subclass reflection Object

  DeclareAbstractX(DrawableData, ork::Object);

  DrawableData();
  virtual drawable_ptr_t createDrawable() const = 0;
  fvec4 _modcolor;
  rendervar_strmap_t _assetvars;
};

///////////////////////////////////////////////////////////////////////////////

struct ModelDrawableData : public DrawableData {

  DeclareConcreteX(ModelDrawableData, DrawableData);

  ModelDrawableData() {
  }
  ModelDrawableData(AssetPath path);
  drawable_ptr_t createDrawable() const final;
  AssetPath _assetpath;
  asset::vars_t _asset_vars;
};

///////////////////////////////////////////////////////////////////////////////

struct InstancedModelDrawableData : public DrawableData {

  DeclareConcreteX(InstancedModelDrawableData, DrawableData);

  InstancedModelDrawableData() {
  }
  InstancedModelDrawableData(AssetPath path);
  drawable_ptr_t createDrawable() const final;
  AssetPath _assetpath;
};

///////////////////////////////////////////////////////////////////////////////

struct ModelDrawable : public Drawable {

  ModelDrawable(DrawableOwner* owner = NULL);
  ~ModelDrawable();

  void bindModelInst(xgmmodelinst_ptr_t pModelInst); 
  void enqueueToRenderQueue(drawablebufitem_constptr_t, lev2::IRenderer* renderer) const final;

  asset::loadrequest_ptr_t bindModelAsset(AssetPath assetpath);
  asset::loadrequest_ptr_t bindModelAsset(AssetPath assetpath,asset::vars_t asset_vars);
  void bindModelAsset(asset::loadrequest_ptr_t loadreq);
  void bindModelAsset(xgmmodelassetptr_t asset);

  void bindModel(xgmmodel_ptr_t model);

  const ModelDrawableData* _data = nullptr;
  xgmmodelinst_ptr_t _modelinst;
  xgmworldpose_ptr_t _worldpose;
  xgmmodelassetptr_t _asset;
  xgmmodel_ptr_t _model;

  float _scale             = 1.0f;
  bool _showBoundingSphere = false;

  fvec3 _offset;
  fquat _orientation;

};

///////////////////////////////////////////////////////////////////////////////

struct InstancedDrawable : public Drawable {

  InstancedDrawable();

  void resize(size_t count);
  bool isInstanced() const final {
    return true;
  }
  drawablebufitem_ptr_t enqueueOnLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const final;

  static constexpr size_t k_texture_dimension_x = 4096;
  static constexpr size_t k_texture_dimension_y = 256;
  static constexpr size_t k_max_instances       = k_texture_dimension_x * k_texture_dimension_y / 4;

  mutable texture_ptr_t _instanceMatrixTex;
  mutable texture_ptr_t _instanceIdTex;
  mutable texture_ptr_t _instanceColorTex;

  instanceddrawinstancedata_ptr_t _instancedata;
  mutable int _drawcount = 0;
  size_t _count;
};
///////////////////////////////////////////////////////////////////////////////

struct InstancedModelDrawable final : public InstancedDrawable {

  InstancedModelDrawable();
  ~InstancedModelDrawable();
  void enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const override;
  void bindModelAsset(AssetPath assetpath);
  void bindModel(xgmmodel_ptr_t model);
  void gpuInit(Context* ctx) const;

  const InstancedModelDrawableData* _data = nullptr;

  xgmmodelassetptr_t _asset;
  xgmmodel_ptr_t _model;
  svar16_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

struct InstancedDrawableInstanceData {
  void resize(size_t count);
  std::vector<fmtx4> _worldmatrices;
  std::vector<fvec4> _modcolors;
  std::vector<uint64_t> _pickids;
  std::vector<svar64_t> _miscdata;
  size_t _count = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct StringDrawableData : public DrawableData {

  DeclareConcreteX(StringDrawableData, DrawableData);

  StringDrawableData() {
  }
  StringDrawableData(AssetPath path);
  drawable_ptr_t createDrawable() const final;
  std::string _initialString;
  fvec2 _pos2D;
  fvec4 _color;
  float _scale = 1.0f;
  std::string _font;
  on_render_rcid_t _onRender;
};

///////////////////////////////////////////////////////////////////////////////

struct LabeledPointDrawableData : public DrawableData {

  DeclareConcreteX(LabeledPointDrawableData, DrawableData);

  LabeledPointDrawableData();
  drawable_ptr_t createDrawable() const final;
  meshutil::submesh_ptr_t _points_only_mesh;
  fxpipeline_ptr_t _points_pipeline;
  fxpipeline_ptr_t _text_pipeline;
  fvec4 _color;
  float _scale = 1.0f;
  std::string _font;
  on_render_rcid_t _onRender;
};
struct LabeledPointDrawable : public Drawable {
  LabeledPointDrawable(const LabeledPointDrawableData* data);
  ~LabeledPointDrawable();
  void enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const override;
  const LabeledPointDrawableData* _data = nullptr;
};
///////////////////////////////////////////////////////////////////////////////

struct BillboardStringDrawableData : public DrawableData {

  DeclareConcreteX(BillboardStringDrawableData, DrawableData);

  BillboardStringDrawableData() {
  }
  BillboardStringDrawableData(AssetPath path);
  drawable_ptr_t createDrawable() const final;
  std::string _initialString;
  fvec3 _offset;
  fvec3 _upvec;
  float _scale = 1.0f;
  fvec4 _color;
  bool _cameraRelativeOffset = false;
  BlendingMacro _blendmode = BlendingMacro::ALPHA_ADDITIVE;
  lev2::font_rawconstptr_t _font = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct InstancedBillboardStringDrawableData : public DrawableData {

  DeclareConcreteX(InstancedBillboardStringDrawableData, DrawableData);

  InstancedBillboardStringDrawableData();
  drawable_ptr_t createDrawable() const final;
  std::string _initialString;
  fvec3 _offset;
  fvec3 _upvec;
  float _scale = 1.0f;
};

///////////////////////////////////////////////////////////////////////////////

struct StringDrawable final : public Drawable {

  StringDrawable(const StringDrawableData* data);
  ~StringDrawable();
  void enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const override;
  const StringDrawableData* _data = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct BillboardStringDrawable final : public Drawable {

  BillboardStringDrawable(const BillboardStringDrawableData* data);
  ~BillboardStringDrawable();
  void enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const override;

  const BillboardStringDrawableData* _data = nullptr;

  std::string _currentString;
  fvec3 _offset;
  float _scale = 1.0f;
  fvec3 _upvec;
  fvec4 _color;
  BlendingMacro _blendmode = BlendingMacro::ALPHA_ADDITIVE;
};

///////////////////////////////////////////////////////////////////////////////

struct OverlayStringDrawableData : public DrawableData {

  DeclareConcreteX(OverlayStringDrawableData, DrawableData);

  OverlayStringDrawableData();

  drawable_ptr_t createDrawable() const final;
  std::string _initialString;
  fvec2 _position;
  float _scale = 1.0f;
  fvec4 _color;
  std::string _font;
  //lev2::font_rawconstptr_t _font = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct OverlayStringDrawable final : public Drawable {

  OverlayStringDrawable(const OverlayStringDrawableData* data);
  ~OverlayStringDrawable();
  void enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const override;
  const OverlayStringDrawableData* _data;
  std::string _font;
  std::string _currentString;
  fvec2 _position;
  float _scale = 1.0f;
  fvec4 _color;
};

///////////////////////////////////////////////////////////////////////////////

struct InstancedBillboardStringDrawable final : public InstancedDrawable {

  InstancedBillboardStringDrawable();
  ~InstancedBillboardStringDrawable();
  void enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const override;
  // std::string _currentString;
  const InstancedBillboardStringDrawableData* _data = nullptr;
  fvec3 _offset;
  float _scale = 1.0f;
  fvec3 _upvec;
  textitem_vect _text_items;
};

///////////////////////////////////////////////////////////////////////////////

class ICallbackDrawableDataDestroyer {
protected:
  virtual ~ICallbackDrawableDataDestroyer() {
  }

public:
  virtual void Destroy() = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct CallbackDrawable : public Drawable {

  using RLCBType      = std::function<void(RenderContextInstData& RCID)>;
  using Q2LCBType     = void(drawablebufitem_constptr_t cdb);
  using Q2LLambdaType = std::function<void(drawablebufitem_constptr_t)>;

  CallbackDrawable(DrawableOwner* owner);
  ~CallbackDrawable();

  void SetDataDestroyer(ICallbackDrawableDataDestroyer* pdestroyer) {
    mDataDestroyer = pdestroyer;
  }
  void SetRenderCallback(lev2::CallbackRenderable::cbtype_t cb) {
    mRenderCallback = cb;
  }
  static void _renderWithLambda(RenderContextInstData& RCID);
  void setRenderLambda(RLCBType cb);

  void setEnqueueOnLayerCallback(Q2LCBType cb) {
    _enqueueOnLayerCallback = cb;
  }
  void setEnqueueOnLayerLambda(Q2LLambdaType cb) {
    _enqueueOnLayerLambda = cb;
  }
  void enqueueToRenderQueue(drawablebufitem_constptr_t item, lev2::IRenderer* renderer) const final;
  drawablebufitem_ptr_t enqueueOnLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const final;

  ICallbackDrawableDataDestroyer* mDataDestroyer;
  lev2::CallbackRenderable::cbtype_t mRenderCallback;
  Q2LCBType* _enqueueOnLayerCallback;
  Q2LLambdaType _enqueueOnLayerLambda;
  RLCBType _renderLambda;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
