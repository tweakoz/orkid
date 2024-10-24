////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/ecs/system.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>

#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::ecs::SystemData, "SystemData");
ImplementReflectionX(ork::ecs::SystemDataClass, "SystemDataClass");
ImplementReflectionX(ork::ecs::SystemFragmentData, "SystemFragmentData");
ImplementReflectionX(ork::ecs::SystemFragmentDataClass, "SystemFragmentDataClass");
ImplementReflectionX(ork::ecs::System, "System");

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {
using namespace ::ork;
using namespace ::ork::object;
using namespace ::ork::reflect;
using namespace ::ork::rtti;

///////////////////////////////////////////////////////////////////////////////

void SystemFragmentDataClass::describeX(ork::rtti::Category* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

void SystemFragmentData::describeX(SystemFragmentDataClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

SystemFragmentDataClass::SystemFragmentDataClass(const rtti::RTTIData& data)
    : object::ObjectClass(data) {
}

///////////////////////////////////////////////////////////////////////////////

void SystemDataClass::describeX(ork::rtti::Category* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

SystemDataClass::SystemDataClass(const rtti::RTTIData& data)
    : object::ObjectClass(data) {
}

///////////////////////////////////////////////////////////////////////////////

PoolString SystemData::GetFamily() const {
  const SystemDataClass* clazz = rtti::autocast(GetClass());
  OrkAssert(clazz);
  return clazz->GetFamily();
}

///////////////////////////////////////////////////////////////////////////////

void SystemData::describeX(SystemDataClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

void System::describeX(object::ObjectClass* clazz) {
}

System::System(const SystemData* scd, Simulation* pinst)
    : _systemData(scd)
    , _simulation(pinst)
    , _started(false) {
    
    _varmap = std::make_shared<varmap::VarMap>();
}

///////////////////////////////////////////////////////////////////////////////

System::~System(){};

///////////////////////////////////////////////////////////////////////////////

void System::_onGpuInit(Simulation* psi, lev2::Context* ctx) {
}
void System::_onGpuLink(Simulation* psi, lev2::Context* ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void System::_onGpuExit(Simulation* psi, lev2::Context* ctx) {
}

///////////////////////////////////////////////////////////////////////////////

bool System::_onInitialize(Simulation* psi) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void System::_onUninitialize(Simulation* psi) {
}


///////////////////////////////////////////////////////////////////////////////

bool System::_onLink(Simulation* psi) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void System::_onUnLink(Simulation* psi) {
}

///////////////////////////////////////////////////////////////////////////////

bool System::_onStage(Simulation* psi) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void System::_onUnstage(Simulation* psi) {
}

///////////////////////////////////////////////////////////////////////////////

bool System::_onActivate(Simulation* psi) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void System::_onDeactivate(Simulation* psi) {
}


///////////////////////////////////////////////////////////////////////////////

void System::_onUpdate(Simulation* inst) {
}

///////////////////////////////////////////////////////////////////////////////

void System::_onRender(Simulation* psi, ui::drawevent_constptr_t drwev) {
}

///////////////////////////////////////////////////////////////////////////////

void System::_onRenderWithStandardCompositorFrame(Simulation* psi, lev2::standardcompositorframe_ptr_t sframe) {
}

///////////////////////////////////////////////////////////////////////////////

void System::_onNotify(token_t evID, evdata_t data) {
}

///////////////////////////////////////////////////////////////////////////////

void System::_onRequest(impl::sys_response_ptr_t response, token_t evID, evdata_t data) {
}

///////////////////////////////////////////////////////////////////////////////

bool System::_initialize(Simulation* psi) {
  return _onInitialize(psi);
}

///////////////////////////////////////////////////////////////////////////////

void System::_uninitialize(Simulation* psi) {
  _onUninitialize(psi);
}

///////////////////////////////////////////////////////////////////////////////

bool System::_link(Simulation* psi) {
  return _onLink(psi);
}

///////////////////////////////////////////////////////////////////////////////

void System::_unlink(Simulation* psi) {
  _onUnLink(psi);
}

///////////////////////////////////////////////////////////////////////////////

bool System::_stage(Simulation* psi) {
  return _onStage(psi);
}

///////////////////////////////////////////////////////////////////////////////

void System::_unstage(Simulation* psi) {
  _onUnstage(psi);
}

///////////////////////////////////////////////////////////////////////////////

bool System::_activate(Simulation* psi) {
  return _onActivate(psi);
}

///////////////////////////////////////////////////////////////////////////////

void System::_deactivate(Simulation* psi) {
  _onDeactivate(psi);
}

///////////////////////////////////////////////////////////////////////////////

void System::_update(Simulation* psi) {
  _onUpdate(psi);
}

///////////////////////////////////////////////////////////////////////////////

void System::_render(Simulation* psi, ui::drawevent_constptr_t drwev) {
  _onRender(psi, drwev);
}

///////////////////////////////////////////////////////////////////////////////

void System::_renderWithStandardCompositorFrame(Simulation* psi, lev2::standardcompositorframe_ptr_t sframe) {
  _onRenderWithStandardCompositorFrame(psi, sframe);
}

///////////////////////////////////////////////////////////////////////////////

void System::_notify(token_t evID, evdata_t data) {
  //printf( "System::_notify<%08x>\n", evID._hashed );
  _onNotify(evID, data);
}

///////////////////////////////////////////////////////////////////////////////

void System::_request(impl::sys_response_ptr_t response, token_t evID, evdata_t data) {
  printf( "System::_request<%08x>\n", evID._hashed );
  this->_onRequest(response, evID, data);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
