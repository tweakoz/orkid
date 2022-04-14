////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#include <ork/ecs/component.h>
#include <ork/ecs/system.h>

namespace ecstest {
  void ClassInit();
}

struct TestComponentData1 : public ork::ecs::ComponentData {

  DeclareConcreteX(TestComponentData1, ork::ecs::ComponentData);

  ork::ecs::Component* createComponent(ork::ecs::Entity* pent) const final;

};

struct TestComponentData2 : public ork::ecs::ComponentData {

  DeclareConcreteX(TestComponentData2, ork::ecs::ComponentData);

  ork::ecs::Component* createComponent(ork::ecs::Entity* pent) const final;


};

struct TestSystemData1 : public ork::ecs::SystemData {

  DeclareConcreteX(TestSystemData1, ork::ecs::SystemData);

  ork::ecs::System* createSystem(ork::ecs::Simulation* psi) const final;

};

struct TestSystemData2 : public ork::ecs::SystemData {

  DeclareConcreteX(TestSystemData2, ork::ecs::SystemData);

  ork::ecs::System* createSystem(ork::ecs::Simulation* psi) const final;
};

