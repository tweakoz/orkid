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
  int _supersample = 0;                         // 0=off, 1-6 = 2x-7x SSAA
  int _temporalFrames = 0;                      // 0=off, 4/8/16/32/64 TAA frames
  // PBR2 Phase 0 — live updates: re-render the cubemap every frame so
  // reflections respond to scene changes (sky swap, geometry motion).
  // Default false → render once then stay static until an explicit
  // bake event re-marks the probe dirty. Authors flip to True for the
  // dynamic per-particle-system reflection case until autobake-at-
  // tojson lands.
  bool _dynamic = false;
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
