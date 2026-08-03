////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/ecs/SoundFieldProbe.h>
#include <ork/lev2/aud/singularity/soundfield.h>
#include <ork/lev2/aud/singularity/synth.h>

namespace ork::ecs {

using namespace ork::audio::singularity;

///////////////////////////////////////////////////////////////////////////////

struct SoundFieldSystem;

struct SoundFieldProbeComponent : public ecs::Component {
  DeclareAbstractX(SoundFieldProbeComponent, ecs::Component);

public:
  SoundFieldProbeComponent(const SoundFieldProbeData& cd, ecs::Entity* pent);
  const SoundFieldProbeData& GetCD() const {
    return _CD;
  }

  void _onUninitialize(Simulation* psi) final;
  bool _onLink(Simulation* psi) final;
  void _onUnlink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;
  bool _onActivate(Simulation* psi) final;
  void _onDeactivate(Simulation* psi) final;
  void _onNotify(Simulation* psi, token_t evID, evdata_t data) final;

  const SoundFieldProbeData& _CD;
  SoundFieldSystem* _system = nullptr;

  // the field's probe handle, or -1 while the component is not contributing.
  int _probeID = -1;
};

///////////////////////////////////////////////////////////////////////////////

struct SoundFieldSystem final : public ork::ecs::System {
  DeclareAbstractX(SoundFieldSystem, ork::ecs::System);

public:
  static constexpr systemkey_t SystemType = "SoundFieldSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  SoundFieldSystem(const SoundFieldSystemData& data, ork::ecs::Simulation* pinst);

private:
  friend struct SoundFieldProbeComponent;

  void _onActivateComponent(SoundFieldProbeComponent* component);
  void _onDeactivateComponent(SoundFieldProbeComponent* component);

  bool _onLink(Simulation* psi) override;
  void _onUnLink(Simulation* psi) override;
  void _onUpdate(Simulation* inst) override;
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  const SoundFieldSystemData& _SCD;
  std::unordered_set<SoundFieldProbeComponent*> _components;
  soundfield_ptr_t _field;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
