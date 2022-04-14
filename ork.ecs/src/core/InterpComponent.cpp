////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/util/md5.h>

#include <cxxabi.h>
#include <iostream>
#include <sstream>

#include <ork/reflect/properties/registerX.inl>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>

#include "InterpComponent_impl.h"

ImplementReflectionX(ork::ecs::InterpComponentData, "InterpComponentData");
ImplementReflectionX(ork::ecs::InterpComponent, "InterpComponent");
ImplementReflectionX(ork::ecs::InterpSystemData, "InterpSystemData");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ecs {
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;

void InterpComponentData::describeX(ComponentDataClass* clazz) {
  clazz->floatProperty("InterpolationRate", float_range{0.00001,0.001}, &InterpComponentData::_interpolation_rate);
}

///////////////////////////////////////////////////////////////////////////////

InterpComponentData::InterpComponentData() {
  // printf("InterpComponentData::InterpComponentData() this: %p\n", this);
}

///////////////////////////////////////////////////////////////////////////////

Component* InterpComponentData::createComponent(ecs::Entity* pent) const {
  return new InterpComponent(*this, pent);
}

object::ObjectClass* InterpComponentData::componentClass() {
  return InterpComponent::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

void InterpComponentData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::InterpSystemData>();
}

///////////////////////////////////////////////////////////////////////////////

void InterpComponent::describeX(ObjectClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

InterpComponent::InterpComponent(const InterpComponentData& data, ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , mCD(data) {

}

///////////////////////////////////////////////////////////////////////////////

void InterpComponent::_onUninitialize(Simulation* psi){

}

///////////////////////////////////////////////////////////////////////////////

bool InterpComponent::_onLink(Simulation* psi){
  _system = psi->findSystem<InterpSystem>();
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void InterpComponent::_onUnlink(Simulation* psi){

}

///////////////////////////////////////////////////////////////////////////////

bool InterpComponent::_onStage(Simulation* psi){
  _system->_onStageComponent(this);
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void InterpComponent::_onUnstage(Simulation* psi){
  _system->_onUnstageComponent(this);
}

///////////////////////////////////////////////////////////////////////////////

bool InterpComponent::_onActivate(Simulation* psi){
  _system->_onActivateComponent(this);
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void InterpComponent::_onDeactivate(Simulation* psi){
  _system->_onDeactivateComponent(this);
}

///////////////////////////////////////////////////////////////////////////////

void InterpComponent::_onNotify(Simulation* psi, token_t evID, svar64_t data ){
  auto system = psi->findSystem<InterpSystem>();
  if (system) {
  	switch(evID._hashed){
  		case "SETPOS"_crcu:
  			_target_pos = data.get<fvec3>();
  			break;
  		default:
  			OrkAssert(false);
  			break;
  	}
  }
}

///////////////////////////////////////////////////////////////////////////////

void InterpSystemData::describeX(SystemDataClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

InterpSystemData::InterpSystemData() {
}

///////////////////////////////////////////////////////////////////////////////

System* InterpSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new InterpSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

InterpSystem::InterpSystem(const InterpSystemData& data, ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst){

    }

///////////////////////////////////////////////////////////////////////////////

void InterpSystem::_onStageComponent(InterpComponent* component){

}

///////////////////////////////////////////////////////////////////////////////

void InterpSystem::_onUnstageComponent(InterpComponent* component){

}

///////////////////////////////////////////////////////////////////////////////

void InterpSystem::_onActivateComponent(InterpComponent* component){
	_components.insert(component);
}

///////////////////////////////////////////////////////////////////////////////

void InterpSystem::_onDeactivateComponent(InterpComponent* component){

	auto it = _components.find(component);
	if(it!=_components.end()){
		_components.erase(it);
	}

}

///////////////////////////////////////////////////////////////////////////////

bool InterpSystem::_onLink(Simulation* psi){
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void InterpSystem::_onUnLink(Simulation* psi){
}

///////////////////////////////////////////////////////////////////////////////

void InterpSystem::_onUpdate(Simulation* inst){

	for( auto c : _components ){
		auto e = c->GetEntity();

    fvec3 delta = c->_target_pos-c->_current_pos;
		c->_current_pos += delta*c->mCD._interpolation_rate;

		auto tpos = c->_current_pos;
		e->transform()->_translation = tpos;
	}

}

///////////////////////////////////////////////////////////////////////////////

bool InterpSystem::_onStage(Simulation* psi){
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void InterpSystem::_onUnstage(Simulation* inst){
}

///////////////////////////////////////////////////////////////////////////////

bool InterpSystem::_onActivate(Simulation* psi){
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void InterpSystem::_onDeactivate(Simulation* inst){
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ecs {
