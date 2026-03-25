////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/registerX.inl>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.h>
#include <ork/ecs/ReferenceArchetype.h>

ImplementReflectionX(ork::ecs::Archetype, "EcsArchetype");
// ImplementReflectionX(ork::ecs::CompositeArchetype, "EcsCompositeArchetype");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
using namespace ::ork;
using namespace ::ork::object;
using namespace ::ork::reflect;
using namespace ::ork::rtti;

///////////////////////////////////////////////////////////////////////////////
void Archetype::describeX(SceneObjectClass* clazz) {
  ArrayString<64> arrstr;
  MutableString mutstr(arrstr);
  mutstr.format("/arch/Archetype");
  clazz->SetPreferredName(arrstr);

  clazz->directObjectMapProperty("Components", &Archetype::mComponentDatas)->annotate("editor.map.policy.const", ConstString("true"));
}
///////////////////////////////////////////////////////////////////////////////
Archetype::Archetype()
    : mComponentDatas(EKEYPOLICY_MULTILUT) {
}
///////////////////////////////////////////////////////////////////////////////
bool Archetype::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) {
  // Compose();
  for (auto it : mComponentDatas) {
    auto k = it.first->Name();
    auto v = it.second;

    //printf("got component <%s> : %p\n", k.c_str(), (void*) v.get());
  }
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::uninitializeEntity(Simulation* psi, Entity* pent) const {
}
///////////////////////////////////////////////////////////////////////////////
// Non-virtual wrappers (NVI pattern) — handle invariants, then delegate to do*
///////////////////////////////////////////////////////////////////////////////
void Archetype::composeEntity(Simulation* psi, Entity* pent) const {
  if (pent) {
    doComposeEntity(psi, pent);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::decomposeEntity(Simulation* psi, Entity* pent) const {
  doDecomposeEntity(psi, pent);
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::linkEntity(Simulation* psi, Entity* pent) const {
  doLinkEntity(psi, pent);
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::unlinkEntity(Simulation* psi, Entity* pent) const {
  doUnlinkEntity(psi, pent);
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::stageEntity(Simulation* psi, Entity* pent) const {
  doStageEntity(psi, pent);
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::unstageEntity(Simulation* psi, Entity* pent) const {
  doUnstageEntity(psi, pent);
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::activateEntity(Simulation* psi, Entity* pent) const {
  deactivateEntity(psi, pent);   // invariant: ensure clean state before activation
  doActivateEntity(psi, pent);
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::deactivateEntity(Simulation* psi, Entity* pent) const {
  doDeactivateEntity(psi, pent);
}
///////////////////////////////////////////////////////////////////////////////
// Virtual do* methods — base implementations (overridable by ReferenceArchetype etc.)
///////////////////////////////////////////////////////////////////////////////
void Archetype::doComposeEntity(Simulation* psi, Entity* pent) const {
  auto& clut = mComponentDatas;
  for (auto it : clut) {
    auto pcompdata = it.second;
    if (pcompdata) {
      auto pinst = pcompdata->createComponent(pent);
      if (pinst) {
        pent->GetComponents().AddComponent(pinst);
      }
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::doDecomposeEntity(Simulation* psi, Entity* pent) const {
  pent->_deleteComponents();
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::doLinkEntity(Simulation* psi, Entity* pent) const {
  const ComponentTable::LutType& lut = pent->GetComponents().GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    Component* inst = (*it).second;
    inst->_link(psi);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::doUnlinkEntity(Simulation* psi, Entity* pent) const {
  const ComponentTable::LutType& lut = pent->GetComponents().GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    Component* inst = (*it).second;
    inst->_unlink(psi);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::doStageEntity(Simulation* psi, Entity* pent) const {
  const ComponentTable::LutType& lut = pent->GetComponents().GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    Component* inst = (*it).second;
    inst->_stage(psi);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::doUnstageEntity(Simulation* psi, Entity* pent) const {
  const ComponentTable::LutType& lut = pent->GetComponents().GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    Component* inst = (*it).second;
    inst->_unstage(psi);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::doActivateEntity(Simulation* psi, Entity* pent) const {
  auto& comp_table = pent->GetComponents();
  for (auto item : comp_table.GetComponents()) {
    auto comp = item.second;
    comp->_activate(psi);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::doDeactivateEntity(Simulation* psi, Entity* pent) const {
  const ComponentTable::LutType& lut = pent->GetComponents().GetComponents();
  for (ComponentTable::LutType::const_iterator it = lut.begin(); it != lut.end(); it++) {
    Component* inst = (*it).second;
    inst->_deactivate(psi);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Archetype::deleteComponents() {
  mComponentDatas.clear();
}
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<ComponentData> Archetype::addComponentWithClassName(std::string clazzname){
  auto clazz = (ork::object::ObjectClass*) ork::rtti::Class::FindClass("Ecs" + clazzname + "Data");
  if(clazz==nullptr){
    // try appending Data
    clazz = (ork::object::ObjectClass*) ork::rtti::Class::FindClass(clazzname + "Data");
    if( clazz == nullptr ){
      clazz = (ork::object::ObjectClass*) ork::rtti::Class::FindClass(clazzname);
      // try prepending Ecs and appending Data
    }
  }
  OrkAssert(clazz!=nullptr);
  auto pobj = std::dynamic_pointer_cast<ComponentData>(clazz->createShared());
  mComponentDatas.AddSorted(clazz, pobj);
  return pobj;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
