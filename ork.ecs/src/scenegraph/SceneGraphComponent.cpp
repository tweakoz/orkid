////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/lev2/ui/event.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectObjectVector.inl>

#include <ork/ecs/ecs.h>
#include <ork/ecs/system.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>
#include <ork/ecs/datatable.h>

#include "../core/message_private.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::lev2;
using modeldrawable_ptr_t = std::shared_ptr<lev2::ModelDrawableData>;
///////////////////////////////////////////////////////////////////////////////
void SceneGraphNodeItemData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("NodeName", &SceneGraphNodeItemData::_nodename);
  clazz->directProperty("LayerName", &SceneGraphNodeItemData::_layername);
  clazz->directObjectProperty("DrawableData", &SceneGraphNodeItemData::_drawabledata);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphComponentData::describeX(ComponentDataClass* clazz) {
  clazz->directObjectMapProperty("NodeDatas", &SceneGraphComponentData::_nodedatas);
}
///////////////////////////////////////////////////////////////////////////////

SceneGraphComponentData::SceneGraphComponentData() {
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphComponentData::createNodeOnLayer(std::string nodename, lev2::drawabledata_ptr_t d, std::string l) {

  auto nid             = std::make_shared<SceneGraphNodeItemData>();
  nid->_nodename       = nodename;
  nid->_drawabledata   = d;
  nid->_layername      = l;
  _nodedatas[nodename] = nid;
}

///////////////////////////////////////////////////////////////////////////////

Component* SceneGraphComponentData::createComponent(ecs::Entity* pent) const {
  return new SceneGraphComponent(*this, pent);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphComponentData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::SceneGraphSystemData>();
}

object::ObjectClass* SceneGraphComponentData::componentClass() {
  return SceneGraphComponent::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphComponent::describeX(object::ObjectClass* clazz) {
}

SceneGraphComponent::SceneGraphComponent(const SceneGraphComponentData& data, ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , _SGCD(data) {
  _currentXF = std::make_shared<TransformNode>();
}
SceneGraphComponent::~SceneGraphComponent() {
}

/////////////////////////////////////////////////////////////////////////////////
// bool SceneGraphComponent::_onInitialize(Simulation* psi, const DecompTransform& world) {
// return true;
//}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphComponent::_onUninitialize(Simulation* psi) {
}
///////////////////////////////////////////////////////////////////////////////

bool SceneGraphComponent::_onLink(ork::ecs::Simulation* psi) {
  _system = psi->findSystem<SceneGraphSystem>();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphComponent::_onUnlink(ork::ecs::Simulation* psi) {
}
///////////////////////////////////////////////////////////////////////////////
bool SceneGraphComponent::_onStage(Simulation* psi) {
  _system->_onStageComponent(this);
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphComponent::_onUnstage(Simulation* psi) {
  _system->_onUnstageComponent(this);
}
///////////////////////////////////////////////////////////////////////////////
bool SceneGraphComponent::_onActivate(Simulation* psi) {
  _system->_onActivateComponent(this);
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphComponent::_onDeactivate(Simulation* psi) {
  _system->_onDeactivateComponent(this);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphComponent::_onNotify(Simulation* psi, token_t evID, svar64_t data) {
  switch (evID.hashed()) {
    case SceneGraphSystem::DestroyNode._hashed: {

      auto remove_operation = [=]() {
        auto handle   = data.get<response_ref_t>();
        auto response = psi->_findComponentResponseFromRef(handle);
        auto node     = response->_responseData.get<lev2::scenegraph::drawable_node_ptr_t>();
        _system->_default_layer->removeDrawableNode(node);
      };
      _system->_renderops.push(remove_operation);
      break;
    }
    case SceneGraphSystem::ChangeModColor._hashed: {
      auto change_operation = [=]() {
        auto modcolor = data.get<fvec4>();
        // if (auto as_drwnode = dynamic_pointer_cast<scenegraph::DrawableNode>(_sgnode)) {
        // as_drwnode->_modcolor = modcolor;
        // }
      };
      _system->_renderops.push(change_operation);
      break;
    }
    case "SETNAME"_crcu: {
      auto change_operation = [=]() {
        auto name = data.get<std::string>();

        for (auto item : _nodeitems) {
          auto node_item = item.second;
          auto drw       = node_item->_drawable;
          auto node      = node_item->_sgnode;

          if (auto as_bb = dynamic_pointer_cast<BillboardStringDrawable>(drw)) {
            as_bb->_currentString = name;
          }
          else if(auto as_bbi = dynamic_pointer_cast<InstancedBillboardStringDrawable>(drw) ){

            auto instanced_node =  dynamic_pointer_cast<scenegraph::InstancedDrawableNode>(node);
            size_t iid = instanced_node->_instance_id;
            auto instance_data = as_bbi->_instancedata;
            instance_data->_miscdata[iid].set<std::string>(name);
            instance_data->_modcolors[iid] = fvec4(1,1,1,1);
            instance_data->_pickids[iid] = 0;

            //printf( "setname<%s>\n", name.c_str() );
          }

        }
      };
      _system->_renderops.push(change_operation);
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphComponent::_onRequest(Simulation* psi, impl::comp_response_ptr_t response, token_t reqID, svar64_t data) {
  switch (reqID.hashed()) {
    case SceneGraphSystem::CreateNode._hashed: {

      auto makenode_op = [=]() {
        const auto& table = data.get<DataTable>();
        const auto& mdata = table["modeldata"_tok].get<modeldrawable_ptr_t>();
        float scale       = table["uniformScale"_tok].get<float>();
        auto nodename     = table["nodeName"_tok].get<std::string>();

        auto drawable = _system->_drwcache->fetch(mdata);

        ///////////////////////////////
        // create scenegraph node
        ///////////////////////////////

        auto layer          = _system->_default_layer;
        auto sgnode         = layer->createDrawableNode(nodename, drawable);
        auto xform          = _entity->transform();
        xform->_uniformScale = scale;

        auto nitem       = std::make_shared<SceneGraphNodeItem>();
        nitem->_drawable = drawable;
        nitem->_sgnode   = sgnode;
        nitem->_nodename = nodename;

        _nodeitems[nodename] = nitem;

        sgnode->_dqxfdata._worldTransform->set(xform);

        ///////////////////////////////
        // track sgnode in response
        ///////////////////////////////

        response->_responseData.set<lev2::scenegraph::drawable_node_ptr_t>(sgnode);
      };
      _system->_renderops.push(makenode_op);
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystemData::describeX(SystemDataClass* clazz) {
  tokenize(SceneGraphSystem::UpdateCamera);
  ImplementToken(UpdateFramebufferSize);
  ImplementToken(CreateNode);
  ImplementToken(DestroyNode);
  ImplementToken(ChangeModColor);
  ImplementToken(eye);
  ImplementToken(tgt);
  ImplementToken(up);
  ImplementToken(near);
  ImplementToken(far);
  ImplementToken(fovy);
  ImplementToken(width);
  ImplementToken(height);

  clazz->directMapProperty("userparams", &SceneGraphSystemData::_userParams);
  clazz->directObjectVectorProperty("drawabledatas", &SceneGraphSystemData::_staticDrawableDatas);
}

///////////////////////////////////////////////////////////////////////////////

SceneGraphSystemData::SceneGraphSystemData() {
  _internalParams = std::make_shared<varmap::VarMap>();
}

void SceneGraphSystemData::setInternalSceneParam(const varmap::key_t& key, const varmap::VarMap::value_type& val) {
  _internalParams->setValueForKey(key, val);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::addStaticDrawableData(std::string layername, lev2::drawabledata_ptr_t drwdata) {
  auto ddkvpair           = std::make_shared<lev2::scenegraph::DrawableDataKvPair>();
  ddkvpair->_layername    = layername;
  ddkvpair->_drawabledata = drwdata;
  _staticDrawableDatas.push_back(ddkvpair);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::addStaticDrawable(std::string layername, lev2::drawable_ptr_t drw) {
  lev2::scenegraph::DrawableKvPair kvpair;
  kvpair._layername = layername;
  kvpair._drawable  = drw;
  _staticDrawables.push_back(kvpair);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::bindToRtGroup(lev2::rtgroup_ptr_t rtgroup) {
  setInternalSceneParam("outputRTG", rtgroup);
}
void SceneGraphSystemData::bindToCamera(lev2::cameradata_ptr_t camera) {
  _camera = camera;
}

///////////////////////////////////////////////////////////////////////////////

System* SceneGraphSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new SceneGraphSystem(*this, pinst);
}

void SceneGraphSystemData::enqueueOnSystemCreation(oncreatesys_lambda_t l) {
  _onCreateSystemOperations.push_back(l);
}

void SceneGraphSystemData::declarePrefetchDrawableData(lev2::drawabledata_ptr_t data) {
  _drawdatas_prefetchlist.insert(data);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::describeX(object::ObjectClass* clazz) {
}

SceneGraphSystem::SceneGraphSystem(const SceneGraphSystemData& data, ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst)
    , _SGSD(data) {

  _mergedParams = std::make_shared<varmap::VarMap>();

  if (_SGSD._camera) {
    _camera = _SGSD._camera;
  } else {
    _camera = std::make_shared<CameraData>();
  }
  _camlut = std::make_shared<CameraDataLut>();
  (*_camlut)["spawncam"] = _camera;
  _drwcache = std::make_shared<DrawableCache>();

  for (auto item : data._onCreateSystemOperations) {
    item(this);
  }
}
///////////////////////////////////////////////////////////////////////////////

SceneGraphSystem::~SceneGraphSystem() {

  // defer destruction of the scene to the rendering thread
  //  by stashing it in the simulation for later destruction
  
  _simulation->_stashRenderThreadDestructable(_scene);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_addStaticDrawable(std::string layername, lev2::drawable_ptr_t drw) {
  lev2::scenegraph::DrawableKvPair kvpair;
  kvpair._layername = layername;
  kvpair._drawable  = drw;
  _staticDrawables.push_back(kvpair);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_onGpuInit(Simulation* sim, lev2::Context* ctx) { // final

  /////////////////////////////////////////
  // copy in user params
  /////////////////////////////////////////

  _mergedParams->mergeVars(*_SGSD._internalParams);

  for (auto item : _SGSD._userParams) {
    auto k = item.first;
    auto v = item.second;
    _mergedParams->setValueForKey(k, v);
  }

  /////////////////////////////////////////

  _scene         = std::make_shared<scenegraph::Scene>(_mergedParams);
  _default_layer = _scene->createLayer("sg_default");

  _scene->_staticDrawables = _SGSD._staticDrawables;

  for (auto item : _staticDrawables) {
    _scene->_staticDrawables.push_back(item);
  }

  _scene->_dbufcontext = sim->dbufcontext();

  _scene->gpuInit(ctx);

  /////////////////////////////////////////
  // preload statically declared drawables (and -> assets)
  /////////////////////////////////////////

  auto scenedata = sim->GetData();
  auto compdatas = scenedata->findAllTypedComponents<SceneGraphComponentData>();
  for (auto COMPDATA : compdatas) {
    for (auto NID_item : COMPDATA->_nodedatas) {
      auto NID = NID_item.second;
      if (NID->_drawabledata) {
        _drwcache->fetch(NID->_drawabledata);
      }
    }
  }

  for (auto DRWDATA : _SGSD._drawdatas_prefetchlist) {
    _drwcache->fetch(DRWDATA);
  }

  /////////////////////////////////////////

  auto resize_op = [=]() {
    auto& compositor_ctx = _scene->_compositorImpl->compositingContext();
    compositor_ctx.Resize(1280, 720);
  };
  _renderops.push(resize_op);

  /////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onStageComponent(SceneGraphComponent* component) {
  // printf("sgsys stage component<%p>\n", (void*) component);
  this->_components.atomicOp([component](SceneGraphSystem::component_set_t& unlocked) { //
    unlocked.insert(component);                                                         //
  });
  //////////////////////////////
  auto setdrw_op = [=]() {
    auto& COMPDATA = component->_SGCD;
    for (auto NID_item : COMPDATA._nodedatas) {
      auto NID     = NID_item.second;
      auto drwdata = NID->_drawabledata;

      auto it_drw    = component->_nodeitems.find(NID->_nodename);
      bool was_found = it_drw != component->_nodeitems.end();

      if (not was_found) {
        auto layer    = _scene->findLayer(NID->_layername);
        /////////////////////////////////////////////////
        // light ?
        /////////////////////////////////////////////////
        auto as_light = dynamic_pointer_cast<LightData>(drwdata);
        if (as_light) {
          auto l = dynamic_pointer_cast<Light>(as_light->createDrawable());

          auto nitem                            = std::make_shared<SceneGraphNodeItem>();
          nitem->_drawable                      = l;
          nitem->_sgnode                        = layer->createLightNode(NID->_nodename, l);
          nitem->_nodename                      = NID->_nodename;
          component->_nodeitems[NID->_nodename] = nitem;

          auto ent           = component->GetEntity();
          l->_xformgenerator = [=]() -> fmtx4 {
            fmtx4 rval;
            auto node = nitem->_sgnode;
            if (node) {
              auto xform = ent->transform();
              rval       = xform->composed();
            }
            return rval;
          };

        /////////////////////////////////////////////////
        // drawable ?
        /////////////////////////////////////////////////
        } else {

          auto nitem                            = std::make_shared<SceneGraphNodeItem>();
          nitem->_drawable                      = _drwcache->fetch(drwdata);
          nitem->_nodename                      = NID->_nodename;
          component->_nodeitems[NID->_nodename] = nitem;

          if(auto as_instanced = dynamic_pointer_cast<InstancedDrawable>(nitem->_drawable)){
            nitem->_sgnode = layer->createInstancedDrawableNode(NID->_nodename, as_instanced);
          }
          else{
            nitem->_sgnode = layer->createDrawableNode(NID->_nodename, nitem->_drawable);
          }


        }
      }
    }
  };
  //////////////////////////////
  auto setxform_op = [=]() {
    auto ent              = component->GetEntity();
    auto init_xf          = ent->data()->_dagnode->_xfnode;
    auto ent_xf           = ent->GetDagNode()->_xfnode;
    ent_xf->_transform->set(init_xf->_transform);
    component->_currentXF = ent_xf;
    for (auto NITEM : component->_nodeitems) {
      auto node = NITEM.second->_sgnode;
      if (node) {
        node->_dqxfdata._worldTransform = ent_xf->_transform;
      }
    }
  };
  //////////////////////////////
  _renderops.push(setdrw_op);
  _renderops.push(setxform_op);
  //////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onUnstageComponent(SceneGraphComponent* component) {
  ///////////////////////////////
  // untrack component
  ///////////////////////////////
  _components.atomicOp([component](SceneGraphSystem::component_set_t& unlocked) {
    auto it = unlocked.find(component);
    OrkAssert(it != unlocked.end());
    unlocked.erase(it);
  });
  ///////////////////////////////
  // remove from scenegraph
  ///////////////////////////////
  auto remove_operation = [=]() {
    for (auto NITEM : component->_nodeitems) {
      auto sgnode = NITEM.second->_sgnode;

      if (sgnode) {
        auto as_lightnode    = std::dynamic_pointer_cast<scenegraph::LightNode>(sgnode);
        auto as_drawablenode = std::dynamic_pointer_cast<scenegraph::DrawableNode>(sgnode);

        if (as_lightnode) {
          _default_layer->removeLightNode(as_lightnode);
        } else if (as_drawablenode) {
          _default_layer->removeDrawableNode(as_drawablenode);
        } else {
          OrkAssert(false);
        }
      }
    }
  };
  _renderops.push(remove_operation);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onActivateComponent(SceneGraphComponent* component) {
  // printf("sgsys activate component<%p>\n", (void*) component);
  auto setxform_op = [=]() {
    auto entity           = component->GetEntity();
    auto init_xf          = entity->data()->_dagnode->_xfnode;
    auto ent_xf           = entity->GetDagNode()->_xfnode;
    auto ov_xf            = entity->_override_initial_xf;
    component->_currentXF = ent_xf;
    for (auto NITEM : component->_nodeitems) {
      auto node = NITEM.second->_sgnode;
      // printf("sgsys activate node<%p>\n", (void*) node.get());
      if (node) {
        //////////////////////////////////////////
        // init mutable xform data from const data
        //////////////////////////////////////////
        //(*ent_xf->_transform) = (*init_xf->_transform);
        // if(ov_xf){
        //(*ent_xf->_transform) = (*ov_xf);
        //}
        node->_dqxfdata._worldTransform = ent_xf->_transform;
        //////////////////////////////////////////
        // auto mtx    = ent_xf->_transform->composed();
        // auto mtxstr = mtx.dump4x3cn();
        // printf("_onActivateComponent set node to ent_xf<%p>\n", (void*) ent_xf.get());
        // printf(" value<%s>\n", mtxstr.c_str());
        //////////////////////////////////////////
      }
    }
  };
  _renderops.push(setxform_op);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onDeactivateComponent(SceneGraphComponent* component) {
  auto setpos_op = [=]() {
    auto init_xf          = component->GetEntity()->data()->_dagnode->_xfnode;
    component->_currentXF = init_xf;
    //////////////////////////////////////////
    auto mtx    = init_xf->_transform->composed();
    auto mtxstr = mtx.dump4x3cn();
    for (auto NITEM : component->_nodeitems) {
      auto node = NITEM.second->_sgnode;
      if (node) {
        // printf("_onDeactivateComponent set node to init_xf<%p>\n", (void*) init_xf.get());
        // printf(" value<%s>\n", mtxstr.c_str());
      }
    }
  };
  _renderops.push(setpos_op);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onGpuExit(Simulation* psi, lev2::Context* ctx) { // final
  if (_scene) {
    _scene->gpuExit(ctx);
  }
}

bool SceneGraphSystem::_onLink(Simulation* psi) // final
{
  return true;
}
void SceneGraphSystem::_onUnLink(Simulation* psi) // final
{
}
///////////////////////////////////////////////////////////////////////////////
bool SceneGraphSystem::_onStage(Simulation* psi) {
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onUnstage(Simulation* psi) {
  _components.atomicOp([this](SceneGraphSystem::component_set_t& unlocked) { unlocked.clear(); });
}
///////////////////////////////////////////////////////////////////////////////
bool SceneGraphSystem::_onActivate(Simulation* psi) // final
{
  return true;
}
void SceneGraphSystem::_onDeactivate(Simulation* inst) // final
{
}
void SceneGraphSystem::_onUpdate(Simulation* psi) // final
{
  if (_scene) {
    _scene->enqueueToRenderer(_camlut);
  }
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_rt_process() {
  ///////////////////////////////////////
  // execute render ops
  ///////////////////////////////////////
  void_lambda_t render_op;
  while (_renderops.try_pop(render_op)) {
    render_op();
  }
  ///////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onRenderWithStandardCompositorFrame(Simulation* psi, lev2::standardcompositorframe_ptr_t sframe) {
  _rt_process();
  if (_scene) {
    _scene->renderWithStandardCompositorFrame(sframe);
  }
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onRender(Simulation* psi, ui::drawevent_constptr_t drwev) // final
{
  _rt_process();

  if (_scene) {
    _scene->renderOnContext(drwev->GetTarget());
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_onNotify(token_t evID, svar64_t data) {

  switch (evID.hashed()) {
    case UpdateCamera._hashed: {
      const auto& table = data.get<DataTable>();
      const auto& eye   = table["eye"_tok].get<fvec3>();
      const auto& tgt   = table["tgt"_tok].get<fvec3>();
      const auto& up    = table["up"_tok].get<fvec3>();
      float near        = table["near"_tok].get<float>();
      float far         = table["far"_tok].get<float>();
      float fovy        = table["fovy"_tok].get<float>();
      _camera->Lookat(eye, tgt, up);
      _camera->Persp(near, far, fovy);
      break;
    }
    case UpdateFramebufferSize._hashed: {
      const auto& table = data.get<DataTable>();
      int w             = table["width"_tok].get<int>();
      int h             = table["height"_tok].get<int>();
      auto resize_op    = [=]() {
        if (_scene) {
          auto cimpl = _scene->_compositorImpl;
          // printf("W<%d> H<%d> _scene<%p> cimpl<%p>\n", w, h, _scene.get(), cimpl.get() );
          auto& compositor_ctx = cimpl->compositingContext();
          compositor_ctx.Resize(w, h);
        }
      };
      _renderops.push(resize_op);

      break;
    }
    case DestroyNode._hashed: {

      auto handle           = data.get<response_ref_t>();
      auto response         = _simulation->_findSystemResponseFromRef(handle);
      auto remove_operation = [this, response]() {
        auto node = response->_responseData.get<lev2::scenegraph::drawable_node_ptr_t>();
        //_simulation->debugBanner(128,255,0,"DestroyNode <%p>\n", node.get());
        this->_default_layer->removeDrawableNode(node);
      };
      _renderops.push(remove_operation);
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_onRequest(impl::sys_response_ptr_t response, token_t reqID, svar64_t data) {

  switch (reqID.hashed()) {
    case CreateNode.hashed(): {

      ///////////////////////////////
      // fetch drawabledata / drawable
      ///////////////////////////////

      const auto& table = data.get<DataTable>();
      auto mdata        = table["modeldata"_tok].get<modeldrawable_ptr_t>();

      ///////////////////////////////
      // create scenegraph node
      ///////////////////////////////

      auto add_operation = [response, mdata, this]() {
        auto drawable = _drwcache->fetch(mdata);

        std::string nodename = "???";
        auto sgnode          = _default_layer->createDrawableNode(nodename, drawable);

        ///////////////////////////////
        // track sgnode in response
        ///////////////////////////////

        //_simulation->debugBanner(128,255,0,"CreateNode <%p>\n", sgnode.get() );

        response->_responseData.set<lev2::scenegraph::drawable_node_ptr_t>(sgnode);
      };
      _renderops.push(add_operation);
      ///////////////////////////////

      break;
    }
    default:
      OrkAssert(false);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::SceneGraphNodeItemData, "SceneGraphNodeItemData");
ImplementReflectionX(ork::ecs::SceneGraphComponentData, "SceneGraphComponentData");
ImplementReflectionX(ork::ecs::SceneGraphSystemData, "SceneGraphSystemData");
ImplementReflectionX(ork::ecs::SceneGraphComponent, "SceneGraphComponent");
ImplementReflectionX(ork::ecs::SceneGraphSystem, "SceneGraphSystem");
