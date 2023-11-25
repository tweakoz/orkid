////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/test/harness.h>
#include <ork/application/application.h>
#include <utpp/UnitTest++.h>
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/file/file.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>

#include <ork/ecs/entity.h>
#include <ork/ecs/simulation.h>

#include "ecstest.inl"
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>
#include <ork/ecs/archetype.inl>

///////////////////////////////////////////////////////////

ImplementReflectionX(TestComponentData1, "TestComponentData1");
ImplementReflectionX(TestComponentData2, "TestComponentData2");
ImplementReflectionX(TestSystemData1, "TestSystemData1");
ImplementReflectionX(TestSystemData2, "TestSystemData2");

using namespace ork::ecs;

struct TestComponent1;
struct TestComponent2;
struct TestSystem1;
struct TestSystem2;

namespace ecstest {

  void ClassInit(){
      TestComponentData1::GetClassStatic();
      TestComponentData2::GetClassStatic();
      TestSystemData1::GetClassStatic();
      TestSystemData2::GetClassStatic();
  }
}

///////////////////////////////////////////////////////////

void TestComponentData1::describeX(ComponentDataClass* clazz){
}
void TestComponentData2::describeX(ComponentDataClass* clazz){
}
void TestSystemData1::describeX(SystemDataClass* clazz){
}
void TestSystemData2::describeX(SystemDataClass* clazz){
}

///////////////////////////////////////////////////////////

struct TestSystem1 : public System {
  TestSystem1(const SystemData* scd, Simulation* pinst) 
    : System(scd,pinst){}
  std::unordered_set<TestComponent1*> _components;
  static constexpr systemkey_t SystemType = "TestComponentData1";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }
};

struct TestSystem2 : public System {
  TestSystem2(const SystemData* scd, Simulation* pinst) 
    : System(scd,pinst){}
  std::unordered_set<TestComponent2*> _components;
  static constexpr systemkey_t SystemType = "TestComponentData2";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }
};

///////////////////////////////////////////////////////////

struct TestComponent1 : public Component {
  TestComponent1(const ComponentData* data, Entity* entity) 
    : Component(data,entity){
    }
    bool _onLink(Simulation* psi) final;
    void _onUnlink(Simulation* psi) final;
};

///////////////////////////////////////////////////////////

Component* TestComponentData1::createComponent(Entity* pent) const {
  return new TestComponent1(this,pent);
};

///////////////////////////////////////////////////////////

bool TestComponent1::_onLink(Simulation* psi) {
  auto sys = psi->findSystem<TestSystem1>();
  OrkAssert(sys);
  sys->_components.insert(this);
  return true;
}

///////////////////////////////////////////////////////////

void TestComponent1::_onUnlink(Simulation* psi) {
  auto sys = psi->findSystem<TestSystem1>();
  OrkAssert(sys);
  auto it = sys->_components.find(this);
  OrkAssert(it!=sys->_components.end());
  sys->_components.erase(it);
}

///////////////////////////////////////////////////////////

struct TestComponent2 : public Component {
  TestComponent2(const ComponentData* data, Entity* entity) 
    : Component(data,entity){}
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
};

Component* TestComponentData2::createComponent(Entity* pent) const {
  return new TestComponent2(this,pent);
};

///////////////////////////////////////////////////////////

bool TestComponent2::_onLink(Simulation* psi) {
  auto sys = psi->findSystem<TestSystem2>();
  OrkAssert(sys);
  sys->_components.insert(this);
  return true;
}

///////////////////////////////////////////////////////////

void TestComponent2::_onUnlink(Simulation* psi) {
  auto sys = psi->findSystem<TestSystem2>();
  OrkAssert(sys);
  auto it = sys->_components.find(this);
  OrkAssert(it!=sys->_components.end());
  sys->_components.erase(it);
}

///////////////////////////////////////////////////////////

System* TestSystemData1::createSystem(Simulation* psi) const {
  return new TestSystem1(this,psi);
}

///////////////////////////////////////////////////////////

System* TestSystemData2::createSystem(Simulation* psi) const {
  return new TestSystem2(this,psi);
}
