////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/scene.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/rtti/downcast.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedVector.h>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>

#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/imgui/imgui_ged.inl>

ImplementReflectionX(ork::ecs::SceneObjectClass, "SceneObjectClass") ImplementReflectionX(ork::ecs::SceneObject, "EcsSceneObject");
ImplementReflectionX(ork::ecs::SceneGroup, "EcsSceneGroup");
ImplementReflectionX(ork::ecs::SceneDagObject, "EcsSceneDagObject");

ImplementReflectionX(ork::ecs::DagNodeData, "EcsDagNodeData");

using namespace ::ork::lev2::editor;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ecs {
void SceneObjectClass::describeX(ork::rtti::Category* clazz)
{
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SceneObject::describeX(SceneObjectClass* clazz) {
  ArrayString<64> arrstr;
  MutableString mutstr(arrstr);
  mutstr.format("SceneObject");
  clazz->SetPreferredName(arrstr);

  // Name must be registered for the case that a SceneObject does not live inside a Scene and exists only by Reference from a
  // Spawner
  //clazz
    //  ->directProperty("Name", &SceneObject::mName) //
      //->annotate("editor.visible", "false");
  // reflect::RegisterFunctor("GetName", &SceneObject::GetName);
}

SceneObject::SceneObject() {
}

void SceneObject::SetName(PoolString name) {
  mName = name;
}
void SceneObject::SetName(const char* name) {
  mName = AddPooledString(name);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SceneDagObject::describeX(SceneObjectClass* clazz) {
  clazz->annotate("editor.3dpickable", ConstString("true"));
  clazz->annotate("editor.3dxfable", true);
  clazz->annotate("editor.3dxfinterface", ConstString("SceneDagObjectManipInterface"));
  clazz->directProperty("Parent", &SceneDagObject::_parentName);


  /////////////////////
  prophandler_t xfhandler = [](const EditorContext& ctx, //
                                       object_ptr_t obj, //
                                       const reflect::ObjectProperty* prop){

      auto typed_prop = dynamic_cast<const reflect::DirectObjectBase*>(prop);

      auto child = typed_prop->getObject(obj);

      dagnodedata_ptr_t dnd = std::dynamic_pointer_cast<DagNodeData>(child);
      xfnode_ptr_t xfn = dnd->_xfnode;

      imgui::DirectTransformPropUI(ctx, xfn);
    };

  clazz->directObjectProperty("DagNodeData", &SceneDagObject::_dagnode)
      ->annotate("editor.prop.handler",xfhandler);

  //reflect::annotatePropertyForEditor<SceneDagObject>("DagNode", "editor.visible", "false");
  reflect::annotatePropertyForEditor<SceneDagObject>("Parent", "editor.visible", "false");
}
SceneDagObject::SceneDagObject()
    : _parentName(AddPooledString("scene")) {
  _dagnode = std::make_shared<DagNodeData>(this);
}
SceneDagObject::~SceneDagObject() {
}
void SceneDagObject::SetParentName(const PoolString& pname) {
  _parentName = pname;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SceneGroup::describeX(SceneObjectClass* clazz) {
  clazz->annotate("editor.3dpickable", ConstString("true"));
  clazz->annotate("editor.3dxfable", true);
  clazz->annotate("editor.3dxfinterface", ConstString("SceneDagObjectManipInterface"));
}
///////////////////////////////////////////////////////////////////////////////
SceneGroup::SceneGroup() {
}
///////////////////////////////////////////////////////////////////////////////
SceneGroup::~SceneGroup() {
}
///////////////////////////////////////////////////////////////////////////////
void SceneGroup::addChild(scenedagobject_ptr_t pobj) {
  _children.push_back(pobj);
  _dagnode->addChild(pobj->_dagnode);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGroup::removeChild(scenedagobject_ptr_t pobj) {
  auto it = std::find(_children.begin(), _children.end(), pobj);
  OrkAssert(it != _children.end());
  _children.erase(it);
  _dagnode->removeChild(pobj->_dagnode);
  pobj->SetParentName(AddPooledString("scene"));
}
///////////////////////////////////////////////////////////////////////////////
void SceneGroup::ungroupAll() {
  const PoolString& parname = GetParentName();

  while (_children.empty() == false) {
    auto child = *_children.begin();
    removeChild(child);

    if (0 == strcmp(parname.c_str(), "scene")) {

    } else {
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void DagNodeData::describeX(class_t* c) {
  c->directObjectProperty("TransformNode", &DagNodeData::_xfnode);
}
///////////////////////////////////////////////////////////////////////////////
DagNodeData::DagNodeData(const ork::rtti::ICastable* powner)
    : _owner(powner) {
  _xfnode = std::make_shared<TransformNode>();
}
///////////////////////////////////////////////////////////////////////////////
void DagNodeData::addChild(dagnodedata_ptr_t pobj) {
  _children.push_back(pobj);
  pobj->_xfnode->_parent = _xfnode;
}
///////////////////////////////////////////////////////////////////////////////
void DagNodeData::removeChild(dagnodedata_ptr_t pobj) {
  auto it = std::find(_children.begin(), _children.end(), pobj);
  OrkAssert(it != _children.end());
  _children.erase(it);
  pobj->_xfnode->_parent = nullptr;
}
}} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////
