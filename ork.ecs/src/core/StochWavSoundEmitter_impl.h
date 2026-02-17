////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license-mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/ecs/StochWavSoundEmitter.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <random>

namespace ork::ecs {

using namespace ork::audio::singularity;

///////////////////////////////////////////////////////////////////////////////

struct ActiveVoice {
  programInst* _progInst    = nullptr;
  int _soundIndex           = -1;
  float _startTime          = 0.0f;
  float _pendingPitchCents  = 0.0f;
  bool _pitchApplied        = false;
};

///////////////////////////////////////////////////////////////////////////////

struct StochWavSoundEmitterSystem;

struct StochWavSoundEmitterComponent : public ecs::Component {
  DeclareAbstractX(StochWavSoundEmitterComponent, ecs::Component);

public:
  StochWavSoundEmitterComponent(const StochWavSoundEmitterData& cd, ecs::Entity* pent);
  const StochWavSoundEmitterData& GetCD() const {
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

  prgdata_constptr_t _buildProgram(const stochwavsnd_ptr_t& snd);

  const StochWavSoundEmitterData& _CD;
  StochWavSoundEmitterSystem* _system = nullptr;

  audio::singularity::spatializer_ptr_t _spatializer;
  outbus_ptr_t _bus;
  std::vector<ActiveVoice> _activeVoices;
  float _nextTriggerTime = 0.0f;
  float _elapsedTime     = 0.0f;
  std::mt19937 _rng{std::random_device{}()};
  std::vector<prgdata_constptr_t> _programs;
  std::vector<stochwavsnd_ptr_t> _soundsList; // flattened from _CD._sounds map at activation
};

///////////////////////////////////////////////////////////////////////////////

struct StochWavSoundEmitterSystem final : public ork::ecs::System {
  DeclareAbstractX(StochWavSoundEmitterSystem, ork::ecs::System);

public:
  static constexpr systemkey_t SystemType = "StochWavSoundEmitterSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  StochWavSoundEmitterSystem(const StochWavSoundEmitterSystemData& data,
                             ork::ecs::Simulation* pinst);

private:
  friend struct StochWavSoundEmitterComponent;

  void _onStageComponent(StochWavSoundEmitterComponent* component);
  void _onUnstageComponent(StochWavSoundEmitterComponent* component);
  void _onActivateComponent(StochWavSoundEmitterComponent* component);
  void _onDeactivateComponent(StochWavSoundEmitterComponent* component);

  bool _onLink(Simulation* psi) override;
  void _onUnLink(Simulation* psi) override;
  void _onUpdate(Simulation* inst) override;
  bool _onStage(Simulation* psi) override;
  void _onUnstage(Simulation* inst) override;
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  std::unordered_set<StochWavSoundEmitterComponent*> _components;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
