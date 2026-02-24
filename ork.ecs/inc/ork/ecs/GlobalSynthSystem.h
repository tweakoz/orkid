////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include "system.h"
#include <ork/lev2/aud/singularity/synth.h>

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct SynthBusConfig : public ork::Object {
  DeclareConcreteX(SynthBusConfig, ork::Object);

public:
  SynthBusConfig() = default;

  std::string _effectPreset = "none";
  float _gainDB   = 0.0f;
  float _pan      = 0.0f;
  bool  _mute     = false;
  bool  _solo     = false;
};
using synthbuscfg_ptr_t = std::shared_ptr<SynthBusConfig>;

///////////////////////////////////////////////////////////////////////////////

struct GlobalSynthSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(GlobalSynthSystemData, ork::ecs::SystemData);

public:
  GlobalSynthSystemData();

  std::map<std::string, synthbuscfg_ptr_t> _busConfigs;
  float _masterGainDB = 0.0f;

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};
using globalsynthsysdata_ptr_t = std::shared_ptr<GlobalSynthSystemData>;

///////////////////////////////////////////////////////////////////////////////

struct GlobalSynthSystem final : public ork::ecs::System {
  DeclareAbstractX(GlobalSynthSystem, ork::ecs::System);

public:
  static constexpr systemkey_t SystemType = "GlobalSynthSystem";
  systemkey_t systemTypeDynamic() final { return SystemType; }

  GlobalSynthSystem(const GlobalSynthSystemData& data, ork::ecs::Simulation* pinst);

private:
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  const GlobalSynthSystemData& _SCD;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
