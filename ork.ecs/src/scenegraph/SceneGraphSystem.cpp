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
#include <ork/math/cvector4.h>

#include <ork/ecs/ecs.h>
#include <ork/ecs/system.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>
#include <ork/ecs/datatable.h>

#include "../core/message_private.h"
#include <ork/util/logger.h>
#include <ork/profiling.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_sgsys = logger()->createChannel("ecs.sgcomp", fvec3(0.9, 0.7, 0));
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::lev2;
using modeldrawable_ptr_t = std::shared_ptr<lev2::ModelDrawableData>;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystemData::describeX(SystemDataClass* clazz) {
  tokenize(SceneGraphSystem::UpdateCamera);
  ImplementToken(ResizeFromMainSurface);
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

void SceneGraphSystemData::declareLayer(const std::string& layername) {
  _declaredLayers.push_back(layername);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::declareNodeOnLayer(nodedef_ptr_t ndef) {
  auto nid           = std::make_shared<SceneGraphNodeItemData>();
  nid->_nodename     = ndef->_nodename;
  nid->_drawabledata = ndef->_drawabledata;
  nid->_layername    = ndef->_layername;
  nid->_multilayers    = ndef->_multilayers;
  nid->_xfoverride   = ndef->_transform;
  nid->_modcolor     = ndef->_modcolor;

  _nodedatas[ndef->_nodename] = nid;
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
  _camlut                = std::make_shared<CameraDataLut>();
  (*_camlut)["spawncam"] = _camera;
  _drwcache              = std::make_shared<DrawableCache>();

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

void SceneGraphSystem::_instantiateDeclaredNodes() {
  for (auto NID_item : _SGSD._nodedatas) {
    auto NID = NID_item.second;
    if (NID->_drawabledata) {
      auto drwdata               = NID->_drawabledata;
      auto nitem                 = std::make_shared<SceneGraphNodeItem>();
      nitem->_nodename           = NID->_nodename;
      nitem->_data               = NID;
      _nodeitems[NID->_nodename] = nitem;

      auto on_gpu_init = [=]() {
        nitem->_drawable = _drwcache->fetch(drwdata);
  
        if (auto as_instanced = dynamic_pointer_cast<InstancedDrawable>(nitem->_drawable)) {
          auto NODE_ON_LAYER = [=](lev2::scenegraph::layer_ptr_t layer){
            auto node      = layer->createDrawableNode(NID->_nodename, as_instanced);
            nitem->_sgnode = node;
            size_t count   = as_instanced->_count;
            auto idata     = as_instanced->_instancedata;
            for (size_t i = 0; i < count; i++) {

              int ix   = rand() & 0xffff;
              int iz   = rand() & 0xffff;
              float fx = (float(ix) / 32768.0f - 1.0f) * 100.0f;
              float fz = (float(iz) / 32768.0f - 1.0f) * 100.0f;
              fvec3 pos(fx, 0, fz);

              idata->_worldmatrices[i].setColumn(3, pos);
              idata->_modcolors[i] = fvec4(1, 1, 1, 1);
              idata->_pickids[i]   = 0;
              //printf( "init instanced<%d>\n", i );
            }
          };
          if(NID->_multilayers.size()){
            for( auto sub_layer : NID->_multilayers ){
                auto layer = _scene->findLayer(sub_layer);
                NODE_ON_LAYER(layer);
            }
          }
          else{
            auto layer                 = _scene->findLayer(NID->_layername);
                NODE_ON_LAYER(layer);
          }
        } else {
          auto NODE_ON_LAYER = [=](lev2::scenegraph::layer_ptr_t layer){
            auto node       = layer->createDrawableNode(NID->_nodename, nitem->_drawable);
            node->_modcolor = NID->_modcolor;
            nitem->_sgnode  = node;
          };
          if(NID->_multilayers.size()){
            for( auto sub_layer : NID->_multilayers ){
                auto layer = _scene->findLayer(sub_layer);
                NODE_ON_LAYER(layer);
            }
          } else {
              auto layer = _scene->findLayer(NID->_layername);
              NODE_ON_LAYER(layer);
          }
        }
      };

      this->enqueueOnGpuInit(on_gpu_init);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::enqueueOnGpuInit(void_lambda_t L) {
  _onGpuInitOpQueue.atomicOp([L](std::vector<void_lambda_t>& unlocked) { unlocked.push_back(L); });
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_onGpuInit(Simulation* sim, lev2::Context* ctx) { // final

  _scene->_dbufcontext_SG = sim->dbufcontext();

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
  for (auto NID_item : _SGSD._nodedatas) {
    auto NID = NID_item.second;
    if (NID->_drawabledata) {
      _drwcache->fetch(NID->_drawabledata);
    }
  }

  for (auto DRWDATA : _SGSD._drawdatas_prefetchlist) {
    _drwcache->fetch(DRWDATA);
  }

  /////////////////////////////////////////

  _onGpuInitOpQueue.atomicOp([](std::vector<void_lambda_t>& unlocked) {
    for (auto item : unlocked) {
      item();
    }
    unlocked.clear();
  });

  /////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onStageComponent(SceneGraphComponent* component) {
  //////////////////////////////
  // initialize transform
  //////////////////////////////
  auto ent = component->GetEntity();
  //////////////////////////////
  //printf("sgsys stage component<%p>\n", (void*) component);
  this->_components.atomicOp([this,component](SceneGraphSystem::component_set_t& unlocked) { //
    unlocked.insert(component); 
    _numComponents = unlocked.size();                                                        //
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
        /////////////////////////////////////////////////
        // light ?
        /////////////////////////////////////////////////
        auto as_light = dynamic_pointer_cast<LightData>(drwdata);
        if (as_light) {
          auto layer = _scene->findLayer(NID->_layername);
          auto l = dynamic_pointer_cast<Light>(as_light->createDrawable());

          auto nitem                            = std::make_shared<SceneGraphNodeItem>();
          nitem->_drawable                      = l;
          nitem->_sgnode                        = layer->createLightNode(NID->_nodename, l);
          nitem->_nodename                      = NID->_nodename;
          component->_nodeitems[NID->_nodename] = nitem;

          auto ent = component->GetEntity();
          if (NID->_xfoverride) {
            auto static_matrix = NID->_xfoverride->composed();
            l->_xformgenerator = [=]() -> fmtx4 {
              fmtx4 rval;
              auto node = nitem->_sgnode;
              if (node) {
                auto xform = ent->transform();
                rval       = xform->composed() * static_matrix;
                OrkAssert(false);
              }
              return rval;
            };
            OrkAssert(false);
          } else {
            l->_xformgenerator = [=]() -> fmtx4 {
              fmtx4 rval;
              auto node = nitem->_sgnode;
              if (node) {
                auto xform = ent->transform();
                rval       = xform->composed();
              }
              return rval;
            };
          }

          /////////////////////////////////////////////////
          // drawable ?
          /////////////////////////////////////////////////
        } else {
            
            auto DO_ITEM = [=](scenegraph::layer_ptr_t layer) -> sgnodeitem_ptr_t {
                auto nitem                            = std::make_shared<SceneGraphNodeItem>();
                nitem->_drawable                      = _drwcache->fetch(drwdata);
                nitem->_nodename                      = NID->_nodename;
                nitem->_data                          = NID;
                component->_nodeitems[NID->_nodename] = nitem;
                
                if (auto as_instanced = dynamic_pointer_cast<InstancedDrawable>(nitem->_drawable)) {
                    nitem->_sgnode = layer->createDrawableNode(NID->_nodename, as_instanced);
                    OrkAssert(false);
                    // we should not hit this, because the instanced drawable
                    //  should be @ system scope, not component scope
                    
                } else {
                    auto node       = layer->createDrawableNode(NID->_nodename, nitem->_drawable);
                    node->_modcolor = NID->_modcolor;
                    nitem->_sgnode  = node;
                }
                return nitem;
            };
            if(NID->_multilayers.size()){
                auto layer = _scene->findLayer(NID->_multilayers[0]);
                auto item = DO_ITEM(layer);
                for( int i=1; i<NID->_multilayers.size(); i++ ){
                    auto layer = _scene->findLayer(NID->_multilayers[i]);
                    auto drw_node = std::dynamic_pointer_cast<lev2::scenegraph::DrawableNode>(item->_sgnode);
                    layer->addDrawableNode(drw_node);
                }
            } else {
                auto layer = _scene->findLayer(NID->_layername);
                auto item = DO_ITEM(layer);
            }
        }
      }
    }
    // now check for INSTANCE's
    if (COMPDATA._INSTANCEDATA) {
      auto instance        = std::make_shared<lev2::scenegraph::NodeInstance>();
      instance->_groupname = COMPDATA._INSTANCEDATA->_groupname;
      // TODO : defer until nodes created ?
      auto it = _nodeitems.find(instance->_groupname);
      OrkAssert(it != _nodeitems.end());
      sgnodeitem_ptr_t groupitem = it->second;
      auto group_drawable        = std::dynamic_pointer_cast<lev2::InstancedDrawable>(groupitem->_drawable);
      instance->_idrawable       = group_drawable;
      auto idata                 = group_drawable->_instancedata;
      instance->_idata           = idata;
      int ID                     = idata->allocInstance();
      instance->_instance_index  = ID;
      component->_INSTANCE       = instance;
      //printf( "sgc<%p> instanced sg pseudonode id<%d>\n", this, ID );
      if(instance){
        auto ent = component->GetEntity();
        auto sad = ent->_spawnanondata;
        if(sad and sad->_table){
          auto modcolor = (*sad->_table)["modcolor"_tok];
          //.get<fvec4>();
          if(auto as_v4 = modcolor.tryAs<fvec4>()){
            idata->_modcolors[ID] = as_v4.value();
          }
        }
      }
      if(component->_onInstanceCreated){
        component->_onInstanceCreated();
      }
    }
  };
  //////////////////////////////
  auto setxform_op = component->_genTransformOperation();
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
  _components.atomicOp([this,component](SceneGraphSystem::component_set_t& unlocked) {
    auto it = unlocked.find(component);
    OrkAssert(it != unlocked.end());
    unlocked.erase(it);
    _numComponents = unlocked.size();                                                        //
  });
  ///////////////////////////////
  // remove from scenegraph
  ///////////////////////////////
  auto remove_operation = [=]() {
    //////////////////////////////////
    // first remove nodes
    //////////////////////////////////
    for (auto NITEM : component->_nodeitems) {
      auto sgnode = NITEM.second->_sgnode;

      if (sgnode) {
        auto as_lightnode    = std::dynamic_pointer_cast<scenegraph::LightNode>(sgnode);
        auto as_drawablenode = std::dynamic_pointer_cast<scenegraph::DrawableNode>(sgnode);

        if (as_lightnode) {
          _default_layer->removeLightNode(as_lightnode);
        } else if (as_drawablenode) {
            for( auto l : as_drawablenode->_layers ){
                l->removeDrawableNode(as_drawablenode);
            }
            as_drawablenode->_layers.clear();
        } else {
          OrkAssert(false);
        }
      }
    }
    //////////////////////////////////
    // now remove instances...
    //////////////////////////////////
    if (component->_INSTANCE) {
      int index = component->_INSTANCE->_instance_index;
      OrkAssert(index >= 0);
      component->_INSTANCE->_idata->freeInstance(index);
      //OrkAssert(false); // remove instance
    }
  };
  _renderops.push(remove_operation);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onActivateComponent(SceneGraphComponent* component) {
  //printf("sgsys activate component<%p>\n", (void*) component);
  auto setxform_op = component->_genTransformOperation();
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

  _scene = std::make_shared<scenegraph::Scene>(_mergedParams);

  _default_layer = _scene->createLayer("sg_default");
  for (auto item : _SGSD._declaredLayers) {
    _scene->createLayer(item);
  }

  _scene->_staticDrawables = _SGSD._staticDrawables;

  for (auto item : _staticDrawables) {
    _scene->_staticDrawables.push_back(item);
  }
  _instantiateDeclaredNodes();
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
  EASY_BLOCK("SceneGraphSystem::_onUpdate", 0xffa02020);
  if (_scene) {

    EASY_VALUE("NC", _numComponents);
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

void SceneGraphSystem::_onNotify(token_t evID, evdata_t data) {

  switch (evID.hashed()) {
    case ResizeFromMainSurface._hashed: {
      auto resize_op = [=]() {
        if (_scene) {
          _scene->_doResizeFromMainSurface = data.get<bool>();
        }
      };
      _renderops.push(resize_op);
      break;
    }
    case "UpdatePbrCommon"_crcu: {
      const auto& table = *data.getShared<DataTable>();
      const auto& ssaonumsamples   = table["SSAONumSamples"_tok];
        if( auto as_int = ssaonumsamples.tryAs<int>() ){
          int numsamps = as_int.value();
          _scene->_pbr_common->_ssaoNumSamples = numsamps;
        }
      break;
    }
    case UpdateCamera._hashed: {
      const auto& table = *data.getShared<DataTable>();
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
      const auto& table = *data.getShared<DataTable>();
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

void SceneGraphSystem::_onRequest(impl::sys_response_ptr_t response, token_t reqID, evdata_t data) {
  printf("SGSYS REQ<%08llx>\n", reqID._hashed);
  switch (reqID.hashed()) {
    case CreateNode.hashed(): {

      ///////////////////////////////
      // fetch drawabledata / drawable
      ///////////////////////////////

      const auto& table = *data.getShared<DataTable>();
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
    case "onCreateScenegraph"_tok._hashed: {
      printf("onCreateScenegraph <%p>\n", (void*)_scene.get());
      // response->_responseData.set<scenegraph::scene_ptr_t>(_scene);
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
ImplementReflectionX(ork::ecs::SceneGraphSystemData, "SceneGraphSystemData");
ImplementReflectionX(ork::ecs::SceneGraphSystem, "SceneGraphSystem");
