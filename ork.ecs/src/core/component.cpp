////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/ecs/component.h>

#include <ork/ecs/entity.h>
#include <ork/ecs/componenttable.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ecs::ComponentDataClass, "ComponentDataClass");
ImplementReflectionX(ork::ecs::ComponentData, "ComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ecs::Component, "Component");

template class ork::orklut<ork::PoolString, orklist<ork::ecs::Component*>>;
template class ork::orklut<ork::PoolString, orklist<ork::ecs::ComponentData*>>;
template struct ork::reflect::DirectTypedVector<orkvector<ork::PoolString>>;

namespace ork { namespace ecs {

ComponentDataClass::ComponentDataClass(const rtti::RTTIData& data)
    : object::ObjectClass(data) {
}

void ComponentData::describeX(class_t* c) {
}

ComponentData::ComponentData() {
}

PoolString ComponentData::GetFamily() const {
  const ComponentDataClass* clazz = rtti::autocast(GetClass());
  OrkAssert(clazz);
  return clazz->GetFamily();
}

void Component::Describe() {
}

PoolString Component::GetFamily() const {
  return mComponentData ? mComponentData->GetFamily() : PoolString();
}

Component::Component(const ComponentData* data, Entity* pent)
    : mComponentData(data)
    , _entity(pent) {
  // printf( "Component::Component<%p> ent<%p>\n", this, pent );
}

//////////////////////////////////////////////////////////

void Component::_uninitialize(Simulation* psi) {
  _onUninitialize(psi);
}
void Component::_onUninitialize(Simulation* psi) {
}

//////////////////////////////////////////////////////////

bool Component::_link(Simulation* psi) {
  mbValid = _onLink(psi);
  return mbValid;
}
bool Component::_onLink(Simulation* psi) {
  return true;
}
//////////////////////////////////////////////////////////
void Component::_unlink(Simulation* psi) {
  _onUnlink(psi);
  mbValid = false;
}
void Component::_onUnlink(Simulation* psi) {
}
//////////////////////////////////////////////////////////

bool Component::_stage(Simulation* psi) {
  mbValid = _onStage(psi);
  return mbValid;
}
bool Component::_onStage(Simulation* psi) {
  return true;
}
//////////////////////////////////////////////////////////
void Component::_unstage(Simulation* psi) {
  _onUnstage(psi);
  mbValid = false;
}
void Component::_onUnstage(Simulation* psi) {
}
//////////////////////////////////////////////////////////
bool Component::_activate(Simulation* psi) {
  if (mbValid && (false == mbStarted)) {
    mbValid = _onActivate(psi);
    if (mbValid) {
      mbStarted = true;
    }
  }
  return mbValid;
}
bool Component::_onActivate(Simulation* psi) {
  return true;
}
//////////////////////////////////////////////////////////
void Component::_deactivate(Simulation* psi) {
  if (mbStarted) {
    _onDeactivate(psi);
    mbStarted = false;
  }
}
void Component::_onDeactivate(Simulation* psi) {
}
//////////////////////////////////////////////////////////
void Component::_notify(Simulation* psi, token_t evID, svar64_t data) {
  return _onNotify(psi, evID, data);
}
void Component::_onNotify(Simulation* psi, token_t evID, svar64_t data) {
}
//////////////////////////////////////////////////////////
void Component::_request(Simulation* psi, impl::comp_response_ptr_t response, token_t evID, svar64_t data) {
  _onRequest(psi, response, evID, data);
}
void Component::_onRequest(Simulation* psi, impl::comp_response_ptr_t response, token_t evID, svar64_t data) {
}
//////////////////////////////////////////////////////////
//void Component::_update(Simulation* inst) {
  //if (mbValid) {
    //_onUpdate(inst);
  //}
//}
//void Component::_onUpdate(Simulation* inst) {
//}
//////////////////////////////////////////////////////////

const char* Component::GetEntityName() const {
  return _entity->name().c_str();
}

Simulation* Component::sceneInst() const {
  return _entity->simulation();
}

PoolString ComponentDataClass::GetFamily() const {
  return mFamily;
}
void ComponentDataClass::SetFamily(PoolString family) {
  mFamily = family;
}

void ComponentData::RegisterWithScene(SceneComposer& sc) const {
  DoRegisterWithScene(sc);
}

void ComponentData::DoRegisterWithScene(SceneComposer& sc) const {
}

const char* ComponentData::GetShortSelector() const {
  return 0;
}

void Component::SetEntity(Entity* entity) {
  _entity = entity;
}
Entity* Component::GetEntity() {
  return _entity;
}
const Entity* Component::GetEntity() const {
  return _entity;
}
const char* Component::scriptName() {
  return GetClass()->Name().c_str();
}

const char* Component::GetShortSelector() const {
  return (mComponentData != 0) ? mComponentData->GetShortSelector() : 0;
}

}} // namespace ork::ecs
