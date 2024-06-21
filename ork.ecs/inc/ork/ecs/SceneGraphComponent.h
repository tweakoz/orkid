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
  std::string _layername;
  decompxf_ptr_t _transform;
  fvec4 _modcolor;
};
using nodedef_ptr_t = std::shared_ptr<NodeDef>;

struct SceneGraphNodeItemData : public ork::Object {
  DeclareConcreteX(SceneGraphNodeItemData, ork::Object);
public:
  lev2::drawabledata_ptr_t _drawabledata;
  std::string _layername;
  std::string _nodename;
  decompxf_ptr_t _xfoverride;
  fvec4 _modcolor;
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

  std::map<std::string,sgnodeitemdata_ptr_t> _nodedatas;
  lev2::scenegraph::node_instance_data_ptr_t _INSTANCEDATA;

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
    void addStaticDrawableData(std::string layername, lev2::drawabledata_ptr_t drw);
    void addStaticDrawable(std::string layername, lev2::drawable_ptr_t drw);

    void bindToRtGroup(lev2::rtgroup_ptr_t rtgroup);
    void bindToCamera(lev2::cameradata_ptr_t camera);
    void declareLayer(const std::string& layername);

    void declareNodeOnLayer( nodedef_ptr_t ndef );

private:

  friend struct SceneGraphSystem;

    using oncreatesys_lambda_t = std::function<void(SceneGraphSystem*)>;
    void enqueueOnSystemCreation(oncreatesys_lambda_t l);

  System* createSystem(Simulation* pinst) const final;
  std::set<lev2::drawabledata_ptr_t> _drawdatas_prefetchlist;
  lev2::cameradata_ptr_t _camera;

  varmap::varmap_ptr_t _internalParams;
  lev2::rendervar_strmap_t _userParams;

  std::vector<lev2::scenegraph::drawabledatakvpair_ptr_t> _staticDrawableDatas;
  std::vector<lev2::scenegraph::DrawableKvPair> _staticDrawables;
  std::vector<oncreatesys_lambda_t> _onCreateSystemOperations;
  std::vector<std::string> _declaredLayers;

  std::map<std::string,sgnodeitemdata_ptr_t> _nodedatas;
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
  ///////////////////////////////
  static constexpr systemkey_t SystemType = "SceneGraphSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }
  ///////////////////////////////
  SceneGraphSystem(const SceneGraphSystemData& data, Simulation* pinst);
  ~SceneGraphSystem();
  ///////////////////////////////
  void _addStaticDrawable(std::string layername, lev2::drawable_ptr_t drw);
  ///////////////////////////////
  void _onStageComponent(SceneGraphComponent* component);
  void _onUnstageComponent(SceneGraphComponent* component);
  void _onActivateComponent(SceneGraphComponent* component);
  void _onDeactivateComponent(SceneGraphComponent* component);
  ///////////////////////////////
  void _onGpuInit(Simulation* psi, lev2::Context* ctx) final;
  void _onGpuExit(Simulation* psi, lev2::Context* ctx) final;
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
  void _onRequest(impl::sys_response_ptr_t response, token_t reqID, evdata_t data ) final;
  void _onRenderWithStandardCompositorFrame(Simulation* psi, lev2::standardcompositorframe_ptr_t sframe) final;

  void _instantiateDeclaredNodes();
  void enqueueOnGpuInit(void_lambda_t l);
  
  ///////////////////////////////
  void _rt_process();
  ///////////////////////////////
  lev2::scenegraph::scene_ptr_t _scene;
  lev2::scenegraph::layer_ptr_t _default_layer;
  lev2::cameradata_ptr_t _camera;
  lev2::cameradatalut_ptr_t _camlut;
  lev2::drawablecache_ptr_t _drwcache;
  std::vector<lev2::scenegraph::DrawableKvPair> _staticDrawables;
  LockedResource<std::vector<void_lambda_t>> _onGpuInitOpQueue;

  std::map<std::string,sgnodeitem_ptr_t> _nodeitems;
  ///////////////////////////////
  varmap::varmap_ptr_t _mergedParams;
  ///////////////////////////////
  using component_set_t = std::unordered_set<SceneGraphComponent*>;
  LockedResource<component_set_t> _components;
  int _numComponents = 0;
  const SceneGraphSystemData& _SGSD;
  MpMcBoundedQueue<void_lambda_t,4096> _renderops;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs {