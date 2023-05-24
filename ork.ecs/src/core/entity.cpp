////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//module;

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTypedVector.h>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>


#include <ork/kernel/orklut.hpp>
#include <ork/rtti/Class.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/ui/event.h>

//export module ecs.entity;

#include <ork/ecs/ReferenceArchetype.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.h>

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::ecs::SpawnData, "EcsSpawnData");
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
using namespace ::ork;
using namespace ::ork::object;
using namespace ::ork::reflect;
using namespace ::ork::rtti;
///////////////////////////////////////////////////////////////////////////////
void SpawnData::describeX(SceneObjectClass* clazz) {
  ArrayString<64> arrstr;
  MutableString mutstr(arrstr);
  mutstr.format("EntData");
  GetClassStatic()->SetPreferredName(arrstr);

  clazz->annotate("editor.3dpickable", true);
  clazz->annotate("editor.3dxfable", true);
  clazz->annotate("editor.3dxfinterface", ConstString("SceneDagObjectManipInterface"));

  clazz
      ->accessorProperty(
          "Archetype",               //
          &SpawnData::archetypeGetter, //
          &SpawnData::archetypeSetter)
      ->annotate("editor.choicelist", "archetype")
      ->annotate("editor.factorylistbase", "EcsArchetype");

  clazz->directMapProperty("UserProperties", &SpawnData::mUserProperties);
}
///////////////////////////////////////////////////////////////////////////////
ConstString SpawnData::GetUserProperty(const ConstString& key) const {
  ConstString rval("");
  orklut<ConstString, ConstString>::const_iterator it = mUserProperties.find(key);
  if (it != mUserProperties.end())
    rval = (*it).second;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
bool SpawnData::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) {
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void SpawnData::SetArchetype(archetype_constptr_t parch) {
  _archetype = parch;
}
///////////////////////////////////////////////////////////////////////////////
SpawnData::SpawnData() {
}
///////////////////////////////////////////////////////////////////////////////
SpawnData::~SpawnData() {
}
///////////////////////////////////////////////////////////////////////////////
void SpawnData::archetypeGetter(object_ptr_t& val) const {
  auto nonconst = std::const_pointer_cast<Archetype>(_archetype);
  val           = std::dynamic_pointer_cast<Object>(nonconst);
}
///////////////////////////////////////////////////////////////////////////////
void SpawnData::archetypeSetter(object_ptr_t const& val) {
  archetype_constptr_t typed;
  if (val) {
    typed = std::dynamic_pointer_cast<Archetype>(val);
  }
  SetArchetype(typed);
}
///////////////////////////////////////////////////////////////////////////////
Component* Entity::GetComponentByClass(rtti::Class* clazz) {
  const ComponentTable::LutType& lut = mComponentTable.GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    Component* cinst = (*it).second;
    if (cinst->GetClass() == clazz)
      return cinst;
  }
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
Component* Entity::GetComponentByClassName(std::string classname) {
  if (rtti::Class* clazz = rtti::Class::FindClass(classname))
    return GetComponentByClass(clazz);
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
Entity::Entity(spawndata_constptr_t edata, Simulation* inst, int entref)
    : _entref(entref)
    , _entdata(edata)
    , mSimulation(inst)
    , _components(EKEYPOLICY_MULTILUT)
    , mComponentTable(_components) {
  OrkAssert(edata != nullptr);

  static const auto noname = "noname"_pool;

  _name = _entdata ? _entdata->GetName() : noname;

  _dagnode             = std::make_shared<DagNodeData>(_entdata.get());
  (*_dagnode->_xfnode) = (*_entdata->_dagnode->_xfnode);
}
///////////////////////////////////////////////////////////////////////////////
PoolString Entity::name() const {
  return _name;
}
///////////////////////////////////////////////////////////////////////////////
void Entity::_deleteComponents(){
  for (ComponentTable::LutType::const_iterator it = _components.begin(); it != _components.end(); it++) {
    Component* pinst = it->second;
    delete pinst;
  }
  _components.clear();
}
///////////////////////////////////////////////////////////////////////////////
void Entity::setTransform(decompxf_ptr_t xf) {
  _dagnode->_xfnode->_transform = xf;
}
void Entity::setTransform(const fvec3& pos, const fquat& rot, float uscale) {
  transform()->set(pos, rot, uscale);
}
///////////////////////////////////////////////////////////////////////////////
void Entity::setRotAxisAngle(fvec4 axisang) {
  transform()->_rotation.fromAxisAngle(axisang);
}
///////////////////////////////////////////////////////////////////////////////
void Entity::setRotation(fquat newrot) {
  transform()->_rotation = newrot;
}
///////////////////////////////////////////////////////////////////////////////
void Entity::setPos(fvec3 newpos) {
  transform()->_translation = newpos;
}
///////////////////////////////////////////////////////////////////////////////
void Entity::setScale(float scale) {
  transform()->_uniformScale = scale;
}
///////////////////////////////////////////////////////////////////////////////
Entity::~Entity() {
  _deleteComponents();
}
///////////////////////////////////////////////////////////////////////////////
fvec3 Entity::GetEntityPosition() const {
  return transform()->_translation;
}
///////////////////////////////////////////////////////////////////////////////
void Entity::PrintName() {
  orkprintf("EntityName:%s: \n", name().c_str());
}
///////////////////////////////////////////////////////////////////////////////
/*void Entity::notify(const ComponentEvent& event) {
  for (auto it : mComponentTable.GetComponents()) {
    // Component* inst = (*it).second;
    it.second->notify(event);
  }
}*/
///////////////////////////////////////////////////////////////////////////////
ComponentTable& Entity::GetComponents() {
  return mComponentTable;
}
///////////////////////////////////////////////////////////////////////////////
void Entity::addDrawableToDefaultLayer(lev2::drawable_ptr_t pdrw) {
  std::string layername = "Default";
  if (auto ED = data()) {
    ConstString layer = ED->GetUserProperty("DrawLayer");
    if (strlen(layer.c_str()) != 0) {
      layername = layer.c_str();
    }
  }
  printf("layername<%s>\n", layername.c_str());
  _addDrawable(layername, pdrw);
}
///////////////////////////////////////////////////////////////////////////////
void Entity::addDrawableToLayer(lev2::drawable_ptr_t pdrw, const std::string& layername) {
  _addDrawable(layername, pdrw);
}
///////////////////////////////////////////////////////////////////////////////
const ComponentTable& Entity::GetComponents() const {
  return mComponentTable;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////
