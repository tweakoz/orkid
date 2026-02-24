////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/ecs/SimpleSoundEmitter.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/envelope.h>
#include <ork/lev2/gfx/camera/cameradata.h>

namespace ork::ecs {

using namespace ork::audio::singularity;

///////////////////////////////////////////////////////////////////////////////

struct ActiveSimpleVoice {
  programInst* _progInst         = nullptr;
  prgdata_constptr_t _program;    // prevent program from being freed
  dspblkdata_ptr_t _pannerBlock;  // per-voice panner (null = no spatialization)

  // Smoothed panner state
  float _smoothedAngle           = 0.0f;
  float _smoothedDistance        = 1.0f;
  bool _pannerSmoothed           = false;

  // One-shot duration tracking — triggers keyOff when sample finishes
  float _sampleDuration          = 0.0f;  // seconds (0 = looping/unknown)
  float _startTime               = 0.0f;
  bool _keyOffSent               = false;
};

///////////////////////////////////////////////////////////////////////////////

// Pre-loaded sound data — loaded once per sound, shared across voices
struct PreloadedSimpleSound {
  sample_ptr_t _sampleData;
  keymap_ptr_t _keymap;
  simplesnddata_ptr_t _config;
  outbus_ptr_t _bus;
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleSoundEmitterSystem;

struct SimpleSoundEmitterComponent : public ecs::Component {
  DeclareAbstractX(SimpleSoundEmitterComponent, ecs::Component);

public:
  SimpleSoundEmitterComponent(const SimpleSoundEmitterData& cd, ecs::Entity* pent);
  const SimpleSoundEmitterData& GetCD() const {
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

  const SimpleSoundEmitterData& _CD;
  SimpleSoundEmitterSystem* _system = nullptr;

  std::vector<ActiveSimpleVoice> _activeVoices;
  bool _playing = false;
};

///////////////////////////////////////////////////////////////////////////////

struct SimpleSoundEmitterSystem final : public ork::ecs::System {
  DeclareAbstractX(SimpleSoundEmitterSystem, ork::ecs::System);

public:
  static constexpr systemkey_t SystemType = "SimpleSoundEmitterSystem";
  systemkey_t systemTypeDynamic() final {
    return SystemType;
  }

  SimpleSoundEmitterSystem(const SimpleSoundEmitterSystemData& data,
                           ork::ecs::Simulation* pinst);

private:
  friend struct SimpleSoundEmitterComponent;

  void _onActivateComponent(SimpleSoundEmitterComponent* component);
  void _onDeactivateComponent(SimpleSoundEmitterComponent* component);

  bool _onLink(Simulation* psi) override;
  void _onUnLink(Simulation* psi) override;
  void _onUpdate(Simulation* inst) override;
  bool _onStage(Simulation* psi) override;
  void _onUnstage(Simulation* inst) override;
  bool _onActivate(Simulation* psi) override;
  void _onDeactivate(Simulation* inst) override;

  // Build a per-voice program referencing pre-loaded sound data.
  struct VoiceProgramResult {
    prgdata_constptr_t _program;
    dspblkdata_ptr_t _pannerBlock; // null when no spatialization
  };
  VoiceProgramResult _buildVoiceProgram(const PreloadedSimpleSound& preloaded);

  void _ensureSoundLoaded(const std::string& soundName);

  // Trigger a voice for a component
  void _triggerVoice(SimpleSoundEmitterComponent* comp, const fmtx4& invListenerMtx, float dt);

  // Stop all voices on a component (with release)
  void _stopVoices(SimpleSoundEmitterComponent* comp);

  // Smoothly update panner params on a voice
  static void _setPannerParams(ActiveSimpleVoice& voice,
                               const fmtx4& invListenerMatrix,
                               const fvec3& emitterPos,
                               float dt);

  const SimpleSoundEmitterSystemData& _SCD;
  std::unordered_set<SimpleSoundEmitterComponent*> _components;
  std::unordered_map<std::string, PreloadedSimpleSound> _preloadedSounds;

  // Camera access for listener matrix
  SceneGraphSystem* _sgSystem = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
