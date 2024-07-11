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
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_sgcomp = logger()->createChannel("ecs.sgcomp", fvec3(0.9, 0.7, 0));
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::lev2;
using modeldrawable_ptr_t = std::shared_ptr<lev2::ModelDrawableData>;
///////////////////////////////////////////////////////////////////////////////

NodeDef::NodeDef(){
  _modcolor = fvec4(1,1,1,1);
}

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

void SceneGraphComponentData::declareNodeOnLayer( nodedef_ptr_t ndef ) {

  auto nid             = std::make_shared<SceneGraphNodeItemData>();
  nid->_nodename       = ndef->_nodename;
  nid->_drawabledata   = ndef->_drawabledata;
  nid->_layername      = ndef->_layername;
  nid->_xfoverride     = ndef->_transform;
  nid->_modcolor       = ndef->_modcolor;

  _nodedatas[ndef->_nodename] = nid;
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

///////////////////////////////////////////////////////////////////////////////
void_lambda_t SceneGraphComponent::_genTransformOperation() {
  return [=]() {
    auto ent     = this->GetEntity();
    //auto init_xf = ent->data()->_dagnode->_xfnode;
    auto ent_xf  = ent->GetDagNode()->_xfnode;
    //ent_xf->_transform->set(init_xf->_transform);
    this->_currentXF  = ent_xf;
    //auto ent_composed = ent_xf->_transform->composed();
    for (auto NITEM : this->_nodeitems) {
      auto node = NITEM.second->_sgnode;
      if (node) {
        auto ovxf = NITEM.second->_data->_xfoverride;
        if (ovxf) {
          ovxf->_parent                   = ent_xf->_transform;
          node->_dqxfdata._worldTransform = ovxf;
          auto pos = ovxf->_translation;
          printf( "sgc ovxfpos<%g %g %g>\n", pos.x, pos.y, pos.z);
        } else {
          auto pos = ent_xf->_transform->_translation;
          printf( "sgc exfpos<%g %g %g> xfn<%s>\n", pos.x, pos.y, pos.z, ent_xf->_name.c_str() );
          node->_dqxfdata._worldTransform = ent_xf->_transform;
        }
      }
    }
  };
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
void SceneGraphComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
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
      auto modcolor         = data.get<fvec4>();
      if(_INSTANCE){
        int iid = _INSTANCE->_instance_index;
        _INSTANCE->_idata->_modcolors[iid] = modcolor;
      }
      else{
        auto change_operation = [=]() {
          for (auto item : _nodeitems) {
            auto node_item = item.second;
            auto drw       = node_item->_drawable;
            auto node      = node_item->_sgnode;
            if (auto as_drwnode = dynamic_pointer_cast<scenegraph::DrawableNode>(node)) {
              as_drwnode->_modcolor = modcolor;
            }
          }
        };
        _system->_renderops.push(change_operation);
      }
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
          } else if (auto as_bbi = dynamic_pointer_cast<InstancedBillboardStringDrawable>(drw)) {

            /*auto instanced_node = dynamic_pointer_cast<scenegraph::InstancedDrawableNode>(node);
            size_t iid          = instanced_node->_instanced_drawable_id;
            auto instance_data  = as_bbi->_instancedata;
            instance_data->_miscdata[iid].set<std::string>(name);
            instance_data->_modcolors[iid] = fvec4(1, 1, 1, 1);
            instance_data->_pickids[iid]   = 0;*/

            // printf( "setname<%s>\n", name.c_str() );
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
void SceneGraphComponent::_onRequest(Simulation* psi, impl::comp_response_ptr_t response, token_t reqID, evdata_t data) {
  switch (reqID.hashed()) {
    case SceneGraphSystem::CreateNode._hashed: {

      auto makenode_op = [=]() {
        const auto& table = *data.getShared<DataTable>();
        const auto& mdata = table["modeldata"_tok].get<modeldrawable_ptr_t>();
        float scale       = table["uniformScale"_tok].get<float>();
        auto nodename     = table["nodeName"_tok].get<std::string>();

        auto drawable = _system->_drwcache->fetch(mdata);

        ///////////////////////////////
        // create scenegraph node
        ///////////////////////////////

        auto layer           = _system->_default_layer;
        auto sgnode          = layer->createDrawableNode(nodename, drawable);
        auto xform           = _entity->transform();
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
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::SceneGraphNodeItemData, "SceneGraphNodeItemData");
ImplementReflectionX(ork::ecs::SceneGraphComponentData, "SceneGraphComponentData");
ImplementReflectionX(ork::ecs::SceneGraphComponent, "SceneGraphComponent");
