////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/fileenv.h>
#include <ork/rtti/RTTIX.inl>

#include "component.h"
#include "system.h"
#include "entity.h"

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct InterpComponentData : public ecs::ComponentData {
  DeclareConcreteX(InterpComponentData, ecs::ComponentData);

public:
  InterpComponentData();

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  float _interpolation_rate = 0.0001;

};



///////////////////////////////////////////////////////////////////////////////

struct InterpSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(InterpSystemData, ork::ecs::SystemData);

public:
  ///////////////////////////////////////////////////////
  InterpSystemData();
  ///////////////////////////////////////////////////////

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};


///////////////////////////////////////////////////////////////////////////////

} //namespace ork { namespace ecs {
