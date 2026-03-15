////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include "component.h"
#include "system.h"
#include "entity.h"

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct ProbeComponentData : public ecs::ComponentData {
  DeclareConcreteX(ProbeComponentData, ecs::ComponentData);

public:
  ProbeComponentData();

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  int _imageDim = 1024;
  std::string _outputFolder;
  std::string _outputPrefix = "probe";
  std::string _renderLayer = "std_forward";
  lev2::ProbeActivationMode _activationMode = lev2::ProbeActivationMode::ALWAYS;
};

using probecompdata_ptr_t = std::shared_ptr<ProbeComponentData>;

///////////////////////////////////////////////////////////////////////////////

struct ProbeSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(ProbeSystemData, ork::ecs::SystemData);

public:
  ProbeSystemData();

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};

using probesysdata_ptr_t = std::shared_ptr<ProbeSystemData>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
