////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>

#include "component.h"
#include "system.h"
#include "entity.h"

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

enum class BoidsMode : int {
  AIR = 0,
  LAND = 1,
};

///////////////////////////////////////////////////////////////////////////////

struct BoidsComponentData : public ecs::ComponentData {
  DeclareConcreteX(BoidsComponentData, ecs::ComponentData);

public:
  BoidsComponentData();

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  int _flockID = 0;
  BoidsMode _mode = BoidsMode::AIR;

  float _separationWeight = 1.5f;
  float _separationRadius = 3.0f;

  float _alignmentWeight = 1.0f;
  float _alignmentRadius = 8.0f;

  float _cohesionWeight = 1.0f;
  float _cohesionRadius = 8.0f;

  float _maxForce = 10.0f;
  float _maxSpeed = 5.0f;

  float _wanderStrength = 0.5f;
  float _groundHeight = 0.0f;

  float _homeWeight = 1.0f;        // strength of pull towards spawn origin
  float _homeRadius = 20.0f;       // beyond this distance, home force kicks in
};

using boidscomponentdata_ptr_t = std::shared_ptr<BoidsComponentData>;

///////////////////////////////////////////////////////////////////////////////

struct BoidsSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(BoidsSystemData, ork::ecs::SystemData);

public:
  BoidsSystemData();

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};

using boidssystemdata_ptr_t = std::shared_ptr<BoidsSystemData>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
