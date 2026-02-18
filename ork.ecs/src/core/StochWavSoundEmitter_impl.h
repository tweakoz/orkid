////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/ecs/StochWavSoundEmitter.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/aud/singularity/controller.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <random>

namespace ork::ecs {

using namespace ork::audio::singularity;

///////////////////////////////////////////////////////////////////////////////

enum class EmitterState : int { IDLE = 0, BURSTING = 1 };

///////////////////////////////////////////////////////////////////////////////

struct ActiveVoice {
  programInst* _progInst         = nullptr;
  int _soundIndex                = -1;
  float _startTime               = 0.0f;
  float _pendingPitchCents       = 0.0f;
  bool _pitchApplied             = false;

  // Per-voice panner controller instances (resolved after liveKeyOn's deferred keyOn)
  ControllerInst* _panAngleCtrl  = nullptr;
  ControllerInst* _panDistCtrl   = nullptr;
  bool _panControllersResolved   = false;  // false until layer controllers found

  // One-shot duration tracking — triggers keyOff when sample finishes
  float _sampleDuration          = 0.0f;  // seconds (0 = unknown/looping)
  bool _keyOffSent               = false;
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

  const StochWavSoundEmitterData& _CD;
  StochWavSoundEmitterSystem* _system = nullptr;

  std::vector<ActiveVoice> _activeVoices;
  float _elapsedTime     = 0.0f;
  std::mt19937 _rng{std::random_device{}()};

  // Poisson burst state machine
  EmitterState _state        = EmitterState::IDLE;
  int _burstRemaining        = 0;
  int _currentSoundIndex     = -1;
  float _nextBurstFireTime   = 0.0f;
};

///////////////////////////////////////////////////////////////////////////////

// Pre-loaded sound data (WAV + keymap + program) — loaded once per sound, shared across voices
struct PreloadedSound {
  sample_ptr_t _sampleData;
  keymap_ptr_t _keymap;
  stochwavsnd_ptr_t _config;
  prgdata_constptr_t _program;
  bool _hasPanner       = false;
  float _sampleDuration = 0.0f;  // seconds
};

///////////////////////////////////////////////////////////////////////////////

struct GroupRuntimeData {
  outbus_ptr_t _bus;
  bool _hasSpatializer       = false;  // whether to add PANNER2D to voice programs
  std::vector<stochwavsnd_ptr_t> _soundsList;
  std::vector<PreloadedSound> _preloadedSounds;
  float _lastTriggerTime   = -1000.0f;
  int _activeVoiceCount    = 0;

  // Debug stats
  int _totalTriggered      = 0;
  int _rejectedVoiceCap    = 0;  // blocked by maxVoicesPerGroup
  int _rejectedMinSpacing  = 0;  // blocked by minSpacing
  int _rejectedPoisson     = 0;  // Poisson roll failed
  int _idleChecks          = 0;  // total IDLE state evaluations
  int _burstChecks         = 0;  // total BURSTING state evaluations
  int _burstBlocked        = 0;  // BURSTING blocked by voice cap or spacing
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

  // Build a shared program referencing pre-loaded sound data.
  // If hasPanner is true, PANNER2D is added with CustomController-based per-voice control.
  prgdata_constptr_t _buildVoiceProgram(const PreloadedSound& preloaded, const StochSoundGroup& group, bool hasPanner);

  void _ensureGroupRuntime(const std::string& groupName);

  const StochWavSoundEmitterSystemData& _SCD;
  std::unordered_set<StochWavSoundEmitterComponent*> _components;
  std::unordered_map<std::string, GroupRuntimeData> _groupRuntime;

  // Camera access for listener matrix
  SceneGraphSystem* _sgSystem = nullptr;

  // Per-frame transient cache
  struct CachedEmitterState {
    StochWavSoundEmitterComponent* _component;
    fvec3 _position;
  };
  std::vector<CachedEmitterState> _stateCache;
  std::unordered_map<std::string, std::vector<size_t>> _groupMap;
  float _systemElapsedTime = 0.0f;
  float _lastStatsPrintTime = 0.0f;
  static constexpr float _statsPrintInterval = 5.0f;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
