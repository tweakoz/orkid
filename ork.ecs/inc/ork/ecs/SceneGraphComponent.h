////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>

#include "component.h"
#include "componenttable.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

struct SceneGraphSystemData;
struct SceneGraphSystem;

///////////////////////////////////////////////////////////////////////////////

struct NodeDef{
  NodeDef();
  std::string _nodename;
  lev2::drawabledata_ptr_t _drawabledata;
  // Name of an AssetSystemData gen that produced _drawabledata. Set by
  // higher-level construction (Scene DSL); empty for inline drawables.
  // Survives JSON round-trip so the post-deserialize wiring step can
  // re-attach the materialized drawable to nodes whose _drawabledata
  // was a placeholder (e.g. RigidPrimitiveDrawableData with no runtime
  // fields populated).
  std::string _drawable_asset_name;
  // PBR2 Phase 0 — per-node HDRI override. Two value forms:
  //   "asset://<name>" — cross-ref to an HdriToXirGenData; resolved to
  //                       its baked .xir path at load time by
  //                       wire_scene_data before SG creates the drawable.
  //   anything else    — literal path, passed straight to
  //                       ork::lev2::loadEnvMapOverride.
  // Applied after _drawabledata->createDrawable() in
  // SceneGraphSystem.cpp; overrides the scene-global skybox for this
  // node's drawable only.
  std::string _envmap_path;
  std::string _layername;
  std::vector<std::string> _multilayers;
  decompxf_ptr_t _transform;
  fvec4 _modcolor;
  // When true, SceneGraphSystem::_onStageComponent will NOT auto-add
  // this drawable node to the "depth_prepass" layer. Default false
  // preserves the legacy behavior where every drawable participates
  // in depth prepass / shadows. Opt in to skip for drawables whose
  // material samples the depth RTG (e.g. translucent water) — those
  // must not render during depth_prepass, since the depth attachment
  // is still in DEPTH_ATTACHMENT_OPTIMAL at that point and binding
  // it as a texture asserts in Vulkan.
  bool _skipAutoDepthPrepass = false;
};
using nodedef_ptr_t = std::shared_ptr<NodeDef>;

struct SceneGraphNodeItemData : public ork::Object {
  DeclareConcreteX(SceneGraphNodeItemData, ork::Object);
public:
  SceneGraphNodeItemData() : _modcolor(1,1,1,1) {}
  lev2::drawabledata_ptr_t _drawabledata;
  // See NodeDef::_drawable_asset_name.
  std::string _drawable_asset_name;
  // See NodeDef::_envmap_path.
  std::string _envmap_path;
  std::string _layername;
  std::vector<std::string> _multilayers;
  std::string _nodename;
  decompxf_ptr_t _xfoverride;
  fvec4 _modcolor;
  // See NodeDef::_skipAutoDepthPrepass.
  bool _skipAutoDepthPrepass = false;
};

using sgnodeitemdata_ptr_t = std::shared_ptr<SceneGraphNodeItemData>;



struct SceneGraphComponentData : public ComponentData {
  DeclareConcreteX(SceneGraphComponentData, ComponentData);

public:

  SceneGraphComponentData();

  ecs::Component* createComponent(Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(SceneComposer& sc) const final;

  void declareNodeOnLayer( nodedef_ptr_t ndef );

  // Rewrite every reference to `old_name` as `new_name` across every
  // registered node item's _layername / _multilayers. Used by
  // SceneData::remapLayerName.
  void remapLayerName(const std::string& old_name, const std::string& new_name);

  std::map<std::string,sgnodeitemdata_ptr_t> _nodedatas;
  lev2::scenegraph::node_instance_data_ptr_t _INSTANCEDATA;
  // the SERIALIZABLE form of _INSTANCEDATA (the ptr is pyext-runtime only):
  // non-empty = this entity renders as ONE INSTANCE of the named system-level
  // instanced node (matched by NAME against the sibling physics component).
  std::string _instanceNodeName;

};

///////////////////////////////////////////////////////////////////////////////
struct SceneGraphNodeItem {
  lev2::drawable_ptr_t _drawable;
  lev2::scenegraph::node_ptr_t _sgnode;
  std::string _nodename;
  sgnodeitemdata_ptr_t _data;
};
using sgnodeitem_ptr_t = std::shared_ptr<SceneGraphNodeItem>;

struct SceneGraphComponent : public Component {
  DeclareAbstractX(SceneGraphComponent, Component);
public:
  SceneGraphComponent(const SceneGraphComponentData& cd, Entity* pent);
  ~SceneGraphComponent();
  ///////////////////////////////
  void _onUninitialize(Simulation* psi) final;
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  void _onNotify(Simulation* psi, token_t evID, evdata_t data ) final;
  void _onRequest(Simulation* psi, impl::comp_response_ptr_t response, token_t evID, evdata_t data) final;
  ///////////////////////////////
  void_lambda_t _genTransformOperation();
  ///////////////////////////////
  const SceneGraphComponentData& _SGCD;
  ::ork::lev2::scenegraph::node_instance_ptr_t _INSTANCE;
  std::map<std::string,sgnodeitem_ptr_t> _nodeitems;

  xfnode_ptr_t _currentXF;
  SceneGraphSystem* _system = nullptr;
  void_lambda_t _onInstanceCreated;
};
///////////////////////////////////////////////////////////////////////////////

struct SceneGraphSystem;

struct SceneGraphSystemData : public SystemData {

  DeclareConcreteX(SceneGraphSystemData, SystemData);

public:

  SceneGraphSystemData();

    void declarePrefetchDrawableData(lev2::drawabledata_ptr_t data);
    void setInternalSceneParam(const varmap::key_t& key, const varmap::VarMap::value_type& val);
    // Reflected (serializable) counterpart to setInternalSceneParam.
    // _userParams is the directMapProperty that survives JSON
    // round-trip; the runtime merge in _onLink layers it over
    // _internalParams. Author-facing scene configuration (SkyboxTexPathStr,
    // SkyboxIntensity, etc.) should go through here so JSON preserves it;
    // setInternalSceneParam is reserved for non-serializable runtime
    // injections (e.g. outputRTG).
    void setUserSceneParam(const std::string& key, const varmap::VarMap::value_type& val);
  bool hasUserSceneParam(const std::string& key) const;
    // PBR2 P3.D — reflected post-fx node registry. Add a node under a
    // stable string key (overwrite on collision). Execution order is the
    // separate _postfx_order string (comma-delimited names). Both
    // _postfx_nodes and _postfx_order are reflected → JSON round-trip.
    void addPostFxNode(const std::string& name, lev2::compositorpostnode_ptr_t node);
    // Append a name to _postfx_order. No-op if name is already present
    // (substring match on comma boundaries). Adds "," separator as needed.
    void appendPostFxOrder(const std::string& name);
    void addStaticDrawableData(std::string layername, lev2::drawabledata_ptr_t drw);
    void addStaticDrawable(std::string layername, lev2::drawable_ptr_t drw);

    void bindToRtGroup(lev2::rtgroup_ptr_t rtgroup);
    void bindToCamera(lev2::cameradata_ptr_t camera);
    void declareLayer(const std::string& layername);
    void clearDeclaredLayers() { _declaredLayers.clear(); }
    const std::vector<std::string>& declaredLayers() const { return _declaredLayers; }

    // Rewrite every reference to `old_name` as `new_name` in the
    // declared layers and in every registered node item's _layername /
    // _multilayers. Used by SceneData::remapLayerName.
    void remapLayerName(const std::string& old_name, const std::string& new_name);

    void declareNodeOnLayer( nodedef_ptr_t ndef );

private:

  friend struct SceneGraphSystem;
  friend struct SceneData;

    using oncreatesys_lambda_t = std::function<void(SceneGraphSystem*)>;
    void enqueueOnSystemCreation(oncreatesys_lambda_t l);

  System* createSystem(Simulation* pinst) const final;
  std::set<lev2::drawabledata_ptr_t> _drawdatas_prefetchlist;
  lev2::cameradata_ptr_t _camera;

  varmap::varmap_ptr_t _internalParams;
  lev2::rendervar_strmap_t _userParams;

  // PBR2 P3.D — reflected post-fx node registry + execution order.
  // _postfx_nodes is keyed by stable name; _postfx_order is a
  // comma-delimited list of names from the map specifying run order.
  // At _onLink, parsed into the runtime postfx chain pushed to
  // _userParams["PostFxChain"] (the existing compositor consumer).
public:
  std::map<std::string, lev2::compositorpostnode_ptr_t> _postfx_nodes;
  std::string _postfx_order;
private:

  std::vector<lev2::scenegraph::drawabledatakvpair_ptr_t> _staticDrawableDatas;
  std::vector<lev2::scenegraph::DrawableKvPair> _staticDrawables;
  std::vector<oncreatesys_lambda_t> _onCreateSystemOperations;
  std::vector<std::string> _declaredLayers;

  std::map<std::string,sgnodeitemdata_ptr_t> _nodedatas;

  int _cookieAtlasWidth  = 1024;
  int _cookieAtlasHeight = 1024;
  int _shadowAtlasWidth  = 1024;
  int _shadowAtlasHeight = 1024;

public:
  // PBR2 Phase 0 — single skybox source field. Two value forms:
  //   1. "asset://<asset_name>" — cross-ref to an HdriToXirGenData in the
  //      Scene's AssetSystemData. wire_scene_data materializes the gen
  //      (running the bake) and the resulting .xir path is stuffed into
  //      _userParams["SkyboxTexPathStr"] for the existing SG consumer.
  //   2. Anything else (e.g. "ork_envmaps|cold4k", "<ork_envmaps2>/x.xir")
  //      — literal path passed straight through to SkyboxTexPathStr.
  // Empty = honor whatever _userParams["SkyboxTexPathStr"] was set
  // directly (back-compat path; bare-string author surface unchanged).
  std::string _skybox_path;
};

using sgsystemdata_ptr_t = std::shared_ptr<SceneGraphSystemData>;

///////////////////////////////////////////////////////////////////////////////
struct SceneGraphSystem final : public System {
  DeclareAbstractX(SceneGraphSystem, System);
  ///////////////////////////////
  static constexpr auto ResizeFromMainSurface = "ResizeFromMainSurface"_ecstok;
  static constexpr auto UpdateCamera = "UpdateCamera"_ecstok;
  DeclareToken(UpdateFramebufferSize);
  DeclareToken(CreateNode);
  DeclareToken(DestroyNode);
  DeclareToken(ChangeModColor);
  DeclareToken(HighlightBySpawnData);
  DeclareToken(SyncTransformBySpawnData);
  ///////////////////////////////
  static constexpr systemkey_t SystemType = "SceneGraphSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }
  // render-sync: keep enqueueing draw buffers while PAUSED (live camera / view effects)
  bool updatesWhilePaused() const final { return true; }
  ///////////////////////////////
  SceneGraphSystem(const SceneGraphSystemData& data, Simulation* pinst);
  ~SceneGraphSystem();
  ///////////////////////////////
  void _addStaticDrawable(std::string layername, lev2::drawable_ptr_t drw);
  void _removeStaticDrawable(lev2::drawable_ptr_t drw);
  void reloadDrawableData(lev2::drawabledata_ptr_t data);
  void processRenderOps();
  void initializeForEditMode(lev2::Context* ctx);
  ///////////////////////////////
  void _onStageComponent(SceneGraphComponent* component);
  void _onUnstageComponent(SceneGraphComponent* component);
  void _onActivateComponent(SceneGraphComponent* component);
  void _onDeactivateComponent(SceneGraphComponent* component);
  ///////////////////////////////
  void _onGpuInit(Simulation* psi, lev2::Context* ctx) final;
  void _onGpuStage(Simulation* psi, lev2::Context* ctx) final;
  void _onGpuExit(Simulation* psi, lev2::Context* ctx) final;
  void _onGpuUpdate(Simulation* psi, lev2::Context* ctx) final;
  ///////////////////////////////
  bool _onLink(Simulation* psi) final;
  void _onUnLink(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* inst) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* inst) final;
  void _onUpdate(Simulation* inst) final;
  void _onRender(Simulation* psi,ui::drawevent_constptr_t drwev) final;
  void _onNotify(token_t evID, evdata_t data ) final;
  void _onGpuNotify(token_t evID, evdata_t data ) final;  // render-thread notify (SetHmdPose @ render rate)
  void _applyHmdPose(evdata_t data);                      // shared by _onNotify + _onGpuNotify
  void _onPropertyChanged(token_t name, evdata_t value) final;
  void _onRequest(impl::sys_response_ptr_t response, token_t reqID, evdata_t data ) final;
  void _onRenderWithStandardCompositorFrame(Simulation* psi, lev2::standardcompositorframe_ptr_t sframe) final;

  void _instantiateDeclaredNodes();
  void enqueueOnGpuInit(void_lambda_t l);
  void _updateLightBridge();

  ///////////////////////////////
  void _rt_process();
  ///////////////////////////////
  lev2::scenegraph::scene_ptr_t _scene;
  lev2::scenegraph::layer_ptr_t _default_layer;
  lev2::cameradata_ptr_t _camera;
  lev2::cameradatalut_ptr_t _camlut;
  lev2::drawablecache_ptr_t _drwcache;
  lev2::xgmmodel_assetcache_ptr_t _modelAssetCache;
  std::vector<lev2::scenegraph::DrawableKvPair> _staticDrawables;
  LockedResource<std::vector<void_lambda_t>> _onGpuInitOpQueue;

  std::map<std::string,sgnodeitem_ptr_t> _nodeitems;
  ///////////////////////////////
  varmap::varmap_ptr_t _mergedParams;
  ///////////////////////////////
  using component_set_t = std::unordered_set<SceneGraphComponent*>;
  LockedResource<component_set_t> _components;
  int _numComponents = 0;
  ///////////////////////////////
  // per-frame entity-varmap -> light bridge. One entry per staged light node,
  // pushed on the render thread when the light is injected and consumed on the
  // update thread by _updateLightBridge (hence the lock).
  struct LightBridgeItem {
    SceneGraphComponent* _component = nullptr;
    Entity* _entity                 = nullptr;
    lev2::light_ptr_t _light;
    lev2::lightdata_ptr_t _lightdata;
    lev2::scenegraph::node_ptr_t _sgnode;
  };
  using lightbridge_vect_t = std::vector<LightBridgeItem>;
  LockedResource<lightbridge_vect_t> _lightbridges;
  ///////////////////////////////
  const SceneGraphSystemData& _SGSD;
  MpMcBoundedQueue<void_lambda_t,65536> _renderops;
  bool _autodraw = true;   // when false, skip renderOnContext / renderWithStandardCompositorFrame
  bool _autoupdate = true; // when false, skip enqueueToRenderer
  ///////////////////////////////
  // cookie atlas (built during gpuInit, slices assigned during staging)
  std::map<std::string, lev2::texturearraysliceref_ptr_t> _cookiePathToSliceRef;
  lev2::texturearray_ptr_t _cookieColorArray;
  lev2::texturearray_ptr_t _cookieDepthArray;
  int _nextDepthSlice = 0;
  bool _isSharedScene = false;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs {