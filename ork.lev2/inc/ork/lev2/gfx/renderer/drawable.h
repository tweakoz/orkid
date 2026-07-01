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
#include <ork/kernel/shared_pool.inl>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/rtti/RTTI.h>
#include <ork/util/tsl/robin_map.h>
#include <ork/util/triple_buffer.h>

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/dbgfontman.h>

template <typename T> struct concurrent_triple_buffer;

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
namespace scenegraph{
  struct Scene;
  struct Node;
  struct Layer;
  using scene_ptr_t        = std::shared_ptr<Scene>;
  using node_ptr_t         = std::shared_ptr<Node>;
}

///////////////////////////////////////////////////////////////////////////
// todo find a better name
///////////////////////////////////////////////////////////////////////////

using onrenderable_fn_t = std::function<void(IRenderable*)>;
using on_render_rcid_t = std::function<void(lev2::RenderContextInstData& RCID)>;

struct DrawableContainer : public ork::Object {
  using drawable_vect_t = orkvector<drawable_ptr_t> ;
  using drawable_vect_ptr_t = std::shared_ptr<drawable_vect_t> ;
  using layermap_t = tsl::robin_map<std::string, drawable_vect_ptr_t> ;

  DrawableContainer();
  ~DrawableContainer();

  void _addDrawable(const std::string& layername, drawable_ptr_t pdrw);

  drawable_vect_ptr_t _getDrawables(const std::string& layer);
  const drawable_vect_ptr_t _getDrawables(const std::string& layer) const;

  const layermap_t& getLayers() const {
    return _layerMap;
  }

  layermap_t _layerMap;

private:
  RttiDeclareAbstract(DrawableContainer, ork::Object);
};

///////////////////////////////////////////////////////////////////////////
// DrawQueueTransferData
//  - data which is copied to the 
//     drawqueue from update-thread -> render-thread.
//     after copied, the update side data is safe for mutation. 
//  - contains:
//     transform
//     modcolor
///////////////////////////////////////////////////////////////////////////

struct DrawQueueTransferData {
  DrawQueueTransferData();
  decompxf_ptr_t _worldTransform;
  fvec4 _modcolor;
  bool _use_modcolor = false;
};

///////////////////////////////////////////////////////////////////////////

struct DrawQueueItem {
public:
  typedef ork::lev2::IRenderable::var_t var_t;

  using usermap_t = std::unordered_map<uint32_t, rendervar_t>;

  DrawQueueItem();
  ~DrawQueueItem();

  void terminate();

  const Drawable* _drawable;
  DrawQueueTransferData _dqxferdata;
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
// DrawQueueLayer
///////////////////////////////////////////////////////////////////////////

struct DrawQueueLayer {

  using itemvect_t = std::vector<drawqueueitem_constptr_t>;

  std::string _name;
  LockedResource<itemvect_t> _items;
  int _itemIndex;
  int miBufferIndex;
  std::atomic<int> _state;
  int _sortkey = 0;
  bool HasData() const {
    return (_itemIndex != -1);
  }
  void Reset(const DrawQueue& dB);
  drawqueueitem_ptr_t enqueueDrawable(const DrawQueueTransferData& xfdata, const Drawable* d);

  DrawQueueLayer();
  ~DrawQueueLayer();

}; // ~ 100K

///////////////////////////////////////////////////////////////////////////
// DrawQueue - multi-buffered queue of drawables
//  used for transferring drawables from update-thread to render-thread
//  contains a frames worth of drawables organized into layers of drawqueueitems
///////////////////////////////////////////////////////////////////////////

typedef std::function<void(lev2::RenderContextFrameData& RCFD)> prerendercallback_t;

struct DrawQueue {
public:
  using usermap_t   = orklut<CrcString, rendervar_t>;

  static std::atomic<int> _gate;

  static const int kmaxlayers = 16; // doubled (owner call): role pre-creation + aux channels pushed past 8
  typedef ork::fixedlut<std::string, DrawQueueLayer*, kmaxlayers> LayerLut;
  typedef ork::fixedlut<int, prerendercallback_t, 32> CallbackLut_t;

  LockedResource<cameradatalut_ptr_t> _cameraDataLUT;
  DrawQueueLayer mRawLayers[kmaxlayers];
  LayerLut mLayerLut;
  orkset<std::string> mLayers;
  CallbackLut_t _preRenderCallbacks;
  usermap_t _userProperties;
  std::atomic<int> _state;

  int miNumLayersUsed = 0;
  int miBufferIndex   = -1;
  int miReadCount     = 0;
  bool _enableEditorLayers = false;

  const usermap_t& userProperties() const {
    return _userProperties;
  }
  usermap_t& userProperties() {
    return _userProperties;
  }

  void setUserProperty(CrcString, rendervar_t data);
  void unSetUserProperty(CrcString);

  template <typename T> attempt_cast_const<T> tryUserProperty(CrcString prop) const {
    auto it = _userProperties.find(prop);
    if (it != _userProperties.end())
      return it->second.tryAs<T>();
    return attempt_cast_const<T>(nullptr);
  }

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
  DrawQueue(int ibidx);
  ~DrawQueue();

  void setPreRenderCallback(int key, prerendercallback_t cb);
  void invokePreRenderCallbacks(lev2::rcfd_ptr_t RCFD) const;

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

  DrawQueueLayer* MergeLayer(const std::string& layername);

  void enqueueLayerToRenderQueue(const std::string& LayerName, lev2::IRenderer* renderer) const;

}; // ~1MiB

///////////////////////////////////////////////////////////////////////////
// DrawQueueContext
//  - a context for managing a multi-buffered drawqueue
///////////////////////////////////////////////////////////////////////////

struct DrawQueueContext {

  DrawQueueContext();
  ~DrawQueueContext();

  DrawQueue* acquireForWriteLocked();
  void releaseFromWriteLocked(DrawQueue* db);
  const DrawQueue* acquireForReadLocked();
  void releaseFromReadLocked(const DrawQueue* db);

  using tbuf_t     = concurrent_triple_buffer<DrawQueue>;
  using tbuf_ptr_t = std::shared_ptr<tbuf_t>;

  tbuf_ptr_t _triple;

  std::string _name;
  ork::mutex _lockedBufferMutex;
  ork::semaphore _rendersync_sema;
  ork::semaphore _rendersync_sema2;
  int _rendersync_counter = 0;
  std::shared_ptr<DrawQueue> _lockeddrawablebuffer;
};

////////////////////////////////////////////////////////////////////////////////

struct AcquiredDrawQueueForUpdate{
  DrawQueue* _DB = nullptr;
};

////////////////////////////////////////////////////////////////////////////////

struct AcquiredDrawQueueForRendering{
  AcquiredDrawQueueForRendering( rcfd_ptr_t rcfd=nullptr );
  const DrawQueue* _DB;
  rcfd_ptr_t _RCFD;
};

///////////////////////////////////////////////////////////////////////////

struct Drawable {

  typedef ork::lev2::IRenderable::var_t var_t;

  Drawable();
  virtual ~Drawable();

  virtual void enqueueToRenderQueue(drawqueueitem_constptr_t item, lev2::IRenderer* prenderer) const;

  virtual drawqueueitem_ptr_t enqueueOnLayer(const DrawQueueTransferData& xfdata, DrawQueueLayer& buffer) const;

  // Per-frame pre-render GPU hook. Invoked by Scene::gpuUpdate on the render thread BEFORE the
  // render pass begins (a command buffer is bound, but no render pass is active — so a compute
  // dispatch is legal here, unlike the render-callback / enqueue path). Default no-op; override
  // for VIEW-INDEPENDENT GPU work, e.g. uploading per-instance data once per frame. Runs ONCE,
  // globally (no camera selected yet — see ezapp_topwidget gpuUpdateAll). For view-dependent
  // work (frustum cull) use onPreRender instead.
  virtual void onGpuUpdate(lev2::Context* ctx) const {}

  // Per-VIEWPORT pre-render GPU hook. Invoked by Scene::preRender from each SceneGraphViewport's
  // DoRePaintSurface, BEFORE that viewport's render pass, with the viewport's own CameraMatrices
  // (correct per-VP camera + aspect). Same compute-legal context as onGpuUpdate. Default no-op;
  // override for VIEW-DEPENDENT GPU work such as a frustum-cull compute feeding an indirect draw.
  virtual void onPreRender(lev2::Context* ctx, const CameraMatrices& cammtx) const {}

  void SetUserDataA(var_t data) {
    _implA = data;
  }
  const var_t& GetUserDataA() const {
    return _implA;
  }
  void SetUserDataB(var_t data) {
    _implB = data;
  }
  const var_t& GetUserDataB() const {
    return _implB;
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
  var_t _implA;
  var_t _implB;
  varmap::varmap_ptr_t _properties;
  fvec4 _modcolor;
  onrenderable_fn_t _onrenderable;
  on_render_rcid_t _rendercb;
  on_render_rcid_t _rendercb_user;
  bool mEnabled;
  bool _pickable = true;
  // Render-EXECUTION gate, read at render time on the render thread (atomic:
  // toggled from the update thread). Distinct from mEnabled, which gates
  // enqueue. Lives on the BASE because CallbackRenderable carries arbitrary
  // drawable types (CallbackDrawable, InstancedModelDrawable, ...) and the
  // renderer's pre-callback check must be valid for all of them.
  std::atomic<bool> _renderEnabled{true};
  std::string _name;
  scenegraph::scene_ptr_t _sg;
  scenegraph::node_ptr_t _sgnode;
  const DrawableData* _drawabledata = nullptr;
  int _sortkey = 0;
  uint64_t _drawable_type = 0;  // type identifier for enumeration (e.g. "model"_crcu)
  uint64_t _tag = 0;            // user-defined tag for custom filtering
  pbr::radiancemaps_ptr_t _envmapOverride;  // per-drawable environment map override
  // Per-drawable cube probe override (live LightProbe). Resolved ONCE
  // at the perfect time (consumer's _onActivateComponent, after every
  // probe has gone through _onStageComponent and registered with
  // LightManager). No per-frame name lookups; the resolved pointer
  // propagates Drawable → IRenderable → RCID. Distinct from
  // _envmapOverride: that's a baked equirect (MapSpecularEnv channel);
  // _probeOverride is the live cube (reflectionPROBE channel).
  lightprobe_ptr_t _probeOverride;
  // PBR2 Phase 0 — when true, this drawable is skipped during probe
  // cubemap captures. Used for noisy/transient drawables that
  // shouldn't appear in reflections (particles, sprites, FX) or for
  // visuals that would feedback-loop through their own probe (a
  // glass surface that's also seen by a probe inside the glass).
  // Default false; ParticlesDrawableData::createDrawable sets true.
  bool _excludeFromProbe = false;
};

// Resolve `envpath` (file path, supports <token>/$ENV expansion) into a
// RadianceMaps from the process-wide cache and stuff it into
// drw->_envmapOverride. No-op on empty path. Called by:
//   1. ModelDrawableData::createDrawable (legacy DrawableData-level
//      override via _environmentMapPath).
//   2. SceneGraphSystem post-createDrawable, per-NodeDef override
//      authored via SG.component(nodes={..."envmap": probe_wrapper})
//      (PBR2 Phase 0).
void loadEnvMapOverride(Drawable* drw, const std::string& envpath);

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

  inline drawable_ptr_t createSGDrawable(scenegraph::scene_ptr_t SG) const {
    auto drw = createDrawable();
    attachSGDrawable(drw, SG);
    return drw;
  }

  inline void attachSGDrawable(drawable_ptr_t drw, scenegraph::scene_ptr_t SG) const {
    drw->_sg = SG;
    _doAttachSGDrawable(drw, SG);
  }
  virtual void _doAttachSGDrawable(drawable_ptr_t drw, scenegraph::scene_ptr_t SG) const {};

  // Whether a single Drawable should be shared across all entities.
  // True for instanced drawables (one SSBO, N instances).
  // False for regular drawables (each entity gets its own Drawable).
  virtual bool isSharedDrawable() const { return false; }

  // Called to reload an existing drawable when this data object's properties change.
  // Subclasses override to rebind assets, textures, etc. on the existing drawable.
  virtual void reloadDrawable(drawable_ptr_t drw) const {}

  fvec4 _modcolor;
  rendervar_strmap_t _assetvars;
  varmap::varmap_ptr_t _vars;
  int _sortkey = 0;
  std::string _name;
  std::string _environmentMapPath;  // per-drawable env map override (supports <assetcache>, ${ENV})
};

///////////////////////////////////////////////////////////////////////////////

struct ModelDrawableData : public DrawableData {

  DeclareConcreteX(ModelDrawableData, DrawableData);

  ModelDrawableData() {
  }
  ModelDrawableData(AssetPath path);
  drawable_ptr_t createDrawable() const final;
  drawable_ptr_t createDrawableWithAsset(xgmmodelassetptr_t asset) const;
  void reloadDrawable(drawable_ptr_t drw) const override;
  AssetPath _assetpath;
  asset::vars_t _asset_vars;
};

///////////////////////////////////////////////////////////////////////////////

struct ModelDrawable : public Drawable {

  ModelDrawable(DrawableContainer* owner = NULL);
  ~ModelDrawable();

  void bindModelInst(xgmmodelinst_ptr_t pModelInst); 
  void enqueueToRenderQueue(drawqueueitem_constptr_t, lev2::IRenderer* renderer) const final;

  asset::loadrequest_ptr_t bindModelAsset(AssetPath assetpath);
  asset::loadrequest_ptr_t bindModelAsset(AssetPath assetpath,asset::vars_ptr_t asset_vars);
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

struct InstancedModelDrawableData : public DrawableData {

  DeclareConcreteX(InstancedModelDrawableData, DrawableData);

  InstancedModelDrawableData() {
  }
  InstancedModelDrawableData(AssetPath path);
  drawable_ptr_t createDrawable() const final;
  bool isSharedDrawable() const override { return true; }
  void reloadDrawable(drawable_ptr_t drw) const override;
  void resize(size_t count) { _maxinstances=count; }
  AssetPath _assetpath;
  size_t _maxinstances = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct InstancedDrawable : public Drawable {

  InstancedDrawable();

  void resize(size_t count);
  bool isInstanced() const final {
    return true;
  }
  drawqueueitem_ptr_t enqueueOnLayer(const DrawQueueTransferData& xfdata, DrawQueueLayer& buffer) const final;

  // SSBO-based instancing — the array sizes in storage_interface storage_instancing
  // (stdtools.i2) ARE this layout: the shader's fixed array lengths must equal
  // k_max_instances or the colors/pickids offsets diverge and the shader reads
  // zeros (black instances). Change BOTH together.
  static constexpr size_t k_max_instances = 131072;
  // SSBO layout offsets (std430), at k_max_instances=131072:
  // - matrices: 0 (64 bytes each)
  // - colors: 64 * k_max_instances = 8388608 (16 bytes each)
  // - pickids: 8388608 + 16 * k_max_instances = 10485760 (8 bytes each)
  // - total: 10485760 + 8 * k_max_instances = 11534336 bytes (~11MB)
  static constexpr size_t k_ssbo_offset_matrices = 0;
  static constexpr size_t k_ssbo_offset_colors   = 64 * k_max_instances;  // 8388608
  static constexpr size_t k_ssbo_offset_pickids  = k_ssbo_offset_colors + 16 * k_max_instances;  // 10485760
  static constexpr size_t k_ssbo_total_size      = k_ssbo_offset_pickids + 8 * k_max_instances;  // 11534336

  mutable FxShaderStorageBuffer* _instanceSSBO = nullptr;

  instanceddrawinstancedata_ptr_t _instancedata;
  //using idb_pool_t = ork::shared_pool::fixed_pool<svarshp_t,8>;
  //LockedResource<idb_pool_t> _idbuf_pool;
  mutable concurrent_triple_buffer<InstancedDrawableInstanceData> _idbuf_pool;
  mutable int _drawcount = 0;
  size_t _count;
  // matrices-only instancing: the GPU instance buffer is a COUNT-SIZED matrices-only SSBO (no
  // colors/pickids), so it is NOT bounded by k_max_instances. Set by the derived drawable before
  // resize() (which then skips the k_max_instances cap). See project_instanced_matrices_only.
  bool _matrices_only = false;
};
///////////////////////////////////////////////////////////////////////////////

struct InstancedModelDrawable final : public InstancedDrawable {

  InstancedModelDrawable();
  ~InstancedModelDrawable();
  void enqueueToRenderQueue(drawqueueitem_constptr_t item, lev2::IRenderer* renderer) const override;
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
  InstancedDrawableInstanceData(int index=-1);

  void copyFrom(const InstancedDrawableInstanceData& oth);
  void resize(size_t count);
  int allocInstance();
  void freeInstance(int index);


  std::vector<fmtx4> _worldmatrices;
  std::vector<fvec4> _modcolors;
  std::vector<uint64_t> _pickids;
  std::vector<svar64_t> _miscdata;
  std::unordered_set<int> _instancePool;
  size_t _count = 0;
  int _index = -1;
  bool _uses_alloc_free = false;
  bool _uses_picking = false;
  bool _uses_miscdata = false;
  bool _matrices_only = false;  // when set, only _worldmatrices is allocated/copied (no colors/pickids/misc)
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
  // normalized corner anchor: (0,0)=top-left (default, legacy behavior), (0,1)=bottom-left,
  // (1,0)=top-right, (1,1)=bottom-right. pos2D becomes the offset from the anchored corner —
  // y-anchored text is BLOCK-HEIGHT compensated (the block's bottom rests on the margin), so a
  // bottom-left HUD is anchor=(0,1), pos2D=(margin, -margin) at ANY line count / window size.
  fvec2 _anchor;
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
  void enqueueToRenderQueue(drawqueueitem_constptr_t item, lev2::IRenderer* renderer) const override;
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
  lev2::font_ptr_t _font = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct InstancedBillboardStringDrawableData : public DrawableData {

  DeclareConcreteX(InstancedBillboardStringDrawableData, DrawableData);

  InstancedBillboardStringDrawableData();
  drawable_ptr_t createDrawable() const final;
  bool isSharedDrawable() const override { return true; }
  std::string _initialString;
  fvec3 _offset;
  fvec3 _upvec;
  float _scale = 1.0f;
};

///////////////////////////////////////////////////////////////////////////////

struct StringDrawable final : public Drawable {

  StringDrawable(const StringDrawableData* data);
  ~StringDrawable();
  void enqueueToRenderQueue(drawqueueitem_constptr_t item, lev2::IRenderer* renderer) const override;
  const StringDrawableData* _data = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct BillboardStringDrawable final : public Drawable {

  BillboardStringDrawable(const BillboardStringDrawableData* data);
  ~BillboardStringDrawable();
  void enqueueToRenderQueue(drawqueueitem_constptr_t item, lev2::IRenderer* renderer) const override;

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
  void enqueueToRenderQueue(drawqueueitem_constptr_t item, lev2::IRenderer* renderer) const override;
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
  void enqueueToRenderQueue(drawqueueitem_constptr_t item, lev2::IRenderer* renderer) const override;
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
  using Q2LCBType     = void(drawqueueitem_constptr_t cdb);
  using Q2LLambdaType = std::function<void(drawqueueitem_constptr_t)>;
  using GpuUpdLambdaType = std::function<void(lev2::Context*)>;

  CallbackDrawable(DrawableContainer* owner);
  ~CallbackDrawable();

  void SetDataDestroyer(ICallbackDrawableDataDestroyer* pdestroyer);
  void SetRenderCallback(lev2::CallbackRenderable::cbtype_t cb);
  static void _renderWithLambda(RenderContextInstData& RCID);
  void setRenderLambda(RLCBType cb);
  void setEnqueueOnLayerCallback(Q2LCBType cb);
  void setEnqueueOnLayerLambda(Q2LLambdaType cb);
  void setOnGpuUpdateLambda(GpuUpdLambdaType cb);  // pre-render, render-pass-safe GPU hook
  void onGpuUpdate(lev2::Context* ctx) const override;
  void enqueueToRenderQueue(drawqueueitem_constptr_t item, lev2::IRenderer* renderer) const override;
  drawqueueitem_ptr_t enqueueOnLayer(const DrawQueueTransferData& xfdata, DrawQueueLayer& buffer) const override;

  ICallbackDrawableDataDestroyer* mDataDestroyer;
  lev2::CallbackRenderable::cbtype_t mRenderCallback;
  Q2LCBType* _enqueueOnLayerCallback;
  Q2LLambdaType _enqueueOnLayerLambda;
  RLCBType _renderLambda;
  GpuUpdLambdaType _onGpuUpdateLambda;
};

struct CallbackDrawableData : public DrawableData {

  DeclareConcreteX(CallbackDrawableData, DrawableData);

  CallbackDrawableData();
  drawable_ptr_t createDrawable() const final;
  void SetRenderCallback(lev2::CallbackRenderable::cbtype_t cb);
  lev2::CallbackRenderable::cbtype_t mRenderCallback;

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
