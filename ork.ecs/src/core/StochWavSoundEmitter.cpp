////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>

#include "StochWavSoundEmitter_impl.h"
#include <ork/lev2/aud/singularity/konoff.h>
#include <ork/math/audiomath.h>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::StochWavSound, "StochWavSound");
ImplementReflectionX(ork::ecs::StochSoundGroup, "StochSoundGroup");
ImplementReflectionX(ork::ecs::StochWavSoundEmitterData, "StochWavSoundEmitterData");
ImplementReflectionX(ork::ecs::StochWavSoundEmitterComponent, "StochWavSoundEmitterComponent");
ImplementReflectionX(ork::ecs::StochWavSoundEmitterSystemData, "StochWavSoundEmitterSystemData");
ImplementReflectionX(ork::ecs::StochWavSoundEmitterSystem, "StochWavSoundEmitterSystem");

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

using namespace ork::audio::singularity;

///////////////////////////////////////////////////////////////////////////////
// StochWavSound
///////////////////////////////////////////////////////////////////////////////

void StochWavSound::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("WavFile", &StochWavSound::_wavFilePath);
  clazz->floatProperty("BurstRate", float_range{0.001f, 100}, &StochWavSound::_burstRate);
  clazz->directProperty("BurstCountMin", &StochWavSound::_burstCountMin);
  clazz->directProperty("BurstCountMax", &StochWavSound::_burstCountMax);
  clazz->floatProperty("IntraBurstRate", float_range{0.1f, 100}, &StochWavSound::_intraBurstRate);
  clazz->floatProperty("SelectionWeight", float_range{0, 100}, &StochWavSound::_selectionWeight);
  clazz->floatProperty("PitchVarianceCents", float_range{0, 2400}, &StochWavSound::_pitchVarianceCents);
  clazz->floatProperty("GainMinDB", float_range{-96, 24}, &StochWavSound::_gainMinDB);
  clazz->floatProperty("GainMaxDB", float_range{-96, 24}, &StochWavSound::_gainMaxDB);
  clazz->floatProperty("FadeInTime", float_range{0, 10}, &StochWavSound::_fadeInTime);
  clazz->floatProperty("FadeOutTime", float_range{0, 10}, &StochWavSound::_fadeOutTime);
}

///////////////////////////////////////////////////////////////////////////////
// StochSoundGroup
///////////////////////////////////////////////////////////////////////////////

void StochSoundGroup::describeX(object::ObjectClass* clazz) {
  clazz->directObjectMapProperty("Sounds", &StochSoundGroup::_sounds)
      ->annotate<ConstString>("editor.factorylistbase", "StochWavSound");
  clazz->directProperty("OutputBusName", &StochSoundGroup::_outputBusName);
  clazz->floatProperty("MasterGainDB", float_range{-96, 24}, &StochSoundGroup::_masterGainDB);
  clazz->directProperty("MaxVoicesPerGroup", &StochSoundGroup::_maxVoicesPerGroup);
  clazz->floatProperty("MinSpacing", float_range{0, 60}, &StochSoundGroup::_minSpacing);
  clazz->floatProperty("FadeInTime", float_range{0, 10}, &StochSoundGroup::_fadeInTime);
  clazz->floatProperty("FadeOutTime", float_range{0, 10}, &StochSoundGroup::_fadeOutTime);
}

///////////////////////////////////////////////////////////////////////////////
// StochWavSoundEmitterData
///////////////////////////////////////////////////////////////////////////////

void StochWavSoundEmitterData::describeX(ComponentDataClass* clazz) {
  clazz->directProperty("GroupName", &StochWavSoundEmitterData::_groupName);
  clazz->directProperty("Enabled", &StochWavSoundEmitterData::_enabled);
  clazz->floatProperty("PitchOffsetCents", float_range{-2400, 2400}, &StochWavSoundEmitterData::_pitchOffsetCents);
  clazz->floatProperty("GainOffsetDB", float_range{-96, 24}, &StochWavSoundEmitterData::_gainOffsetDB);
  clazz->floatProperty("RateScale", float_range{0, 100}, &StochWavSoundEmitterData::_rateScale);
}

StochWavSoundEmitterData::StochWavSoundEmitterData() {
}

Component* StochWavSoundEmitterData::createComponent(ecs::Entity* pent) const {
  return new StochWavSoundEmitterComponent(*this, pent);
}

object::ObjectClass* StochWavSoundEmitterData::componentClass() {
  return StochWavSoundEmitterComponent::GetClassStatic();
}

void StochWavSoundEmitterData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::StochWavSoundEmitterSystemData>();
}

///////////////////////////////////////////////////////////////////////////////
// StochWavSoundEmitterComponent
///////////////////////////////////////////////////////////////////////////////

void StochWavSoundEmitterComponent::describeX(object::ObjectClass* clazz) {
}

StochWavSoundEmitterComponent::StochWavSoundEmitterComponent(
    const StochWavSoundEmitterData& data,
    ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , _CD(data) {
}

void StochWavSoundEmitterComponent::_onUninitialize(Simulation* psi) {
}

bool StochWavSoundEmitterComponent::_onLink(Simulation* psi) {
  _system = psi->findSystem<StochWavSoundEmitterSystem>();
  return true;
}

void StochWavSoundEmitterComponent::_onUnlink(Simulation* psi) {
}

bool StochWavSoundEmitterComponent::_onStage(Simulation* psi) {
  _system->_onStageComponent(this);
  return true;
}

void StochWavSoundEmitterComponent::_onUnstage(Simulation* psi) {
  _system->_onUnstageComponent(this);
}

bool StochWavSoundEmitterComponent::_onActivate(Simulation* psi) {
  _system->_onActivateComponent(this);

  if (!_CD._enabled)
    return true;

  // Init state machine
  _state           = EmitterState::IDLE;
  _burstRemaining  = 0;
  _currentSoundIndex = -1;
  _elapsedTime     = 0.0f;

  // Randomize initial burst time to desynchronize emitters
  std::uniform_real_distribution<float> initDist(0.0f, 2.0f);
  _nextBurstFireTime = initDist(_rng);

  return true;
}

void StochWavSoundEmitterComponent::_onDeactivate(Simulation* psi) {
  printf("StochWavComponent::_onDeactivate: %p, %zu voices\n", this, _activeVoices.size());
  auto syn = synth::instance();
  for (auto& voice : _activeVoices) {
    if (voice._progInst) {
      printf("  comp keyOff progInst %p\n", voice._progInst);
      syn->liveKeyOff(voice._progInst, 60, 0);
    }
  }
  _activeVoices.clear();

  _system->_onDeactivateComponent(this);
}

void StochWavSoundEmitterComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
}

///////////////////////////////////////////////////////////////////////////////
// StochWavSoundEmitterSystemData
///////////////////////////////////////////////////////////////////////////////

void StochWavSoundEmitterSystemData::describeX(SystemDataClass* clazz) {
  clazz->directObjectMapProperty("SoundGroups", &StochWavSoundEmitterSystemData::_soundGroups)
      ->annotate<ConstString>("editor.factorylistbase", "StochSoundGroup");
  clazz->directObjectProperty("Spatializer", &StochWavSoundEmitterSystemData::_spatializer)
      ->annotate<ConstString>("editor.factorylistbase", "SpatializerData");
}

StochWavSoundEmitterSystemData::StochWavSoundEmitterSystemData() {
}

System* StochWavSoundEmitterSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new StochWavSoundEmitterSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////
// StochWavSoundEmitterSystem
///////////////////////////////////////////////////////////////////////////////

void StochWavSoundEmitterSystem::describeX(object::ObjectClass* clazz) {
}

StochWavSoundEmitterSystem::StochWavSoundEmitterSystem(
    const StochWavSoundEmitterSystemData& data,
    ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst)
    , _SCD(data) {
}

void StochWavSoundEmitterSystem::_onStageComponent(StochWavSoundEmitterComponent* component) {
}

void StochWavSoundEmitterSystem::_onUnstageComponent(StochWavSoundEmitterComponent* component) {
}

void StochWavSoundEmitterSystem::_onActivateComponent(StochWavSoundEmitterComponent* component) {
  _components.insert(component);
  // Lazily ensure group runtime is ready
  if (!component->_CD._groupName.empty()) {
    _ensureGroupRuntime(component->_CD._groupName);
  }
}

void StochWavSoundEmitterSystem::_onDeactivateComponent(StochWavSoundEmitterComponent* component) {
  auto it = _components.find(component);
  if (it != _components.end()) {
    _components.erase(it);
  }
}

bool StochWavSoundEmitterSystem::_onLink(Simulation* psi) {
  _sgSystem = psi->findSystem<SceneGraphSystem>();
  return true;
}

void StochWavSoundEmitterSystem::_onUnLink(Simulation* psi) {
}

bool StochWavSoundEmitterSystem::_onStage(Simulation* psi) {
  return true;
}

void StochWavSoundEmitterSystem::_onUnstage(Simulation* inst) {
}

bool StochWavSoundEmitterSystem::_onActivate(Simulation* psi) {
  return true;
}

void StochWavSoundEmitterSystem::_onDeactivate(Simulation* inst) {
  printf("StochWav::_onDeactivate: %zu components\n", _components.size());
  auto syn = synth::instance();
  for (auto* comp : _components) {
    printf("  comp %p: %zu voices\n", comp, comp->_activeVoices.size());
    for (auto& voice : comp->_activeVoices) {
      if (voice._progInst) {
        printf("    keyOff progInst %p\n", voice._progInst);
        syn->liveKeyOff(voice._progInst, 60, 0);
      }
    }
    comp->_activeVoices.clear();
  }
  _components.clear();
  _groupRuntime.clear();
}

///////////////////////////////////////////////////////////////////////////////
// Shared program builder (SampleData/KeyMap pre-loaded, one program per sound)
// Per-voice panner control uses CustomControllerData ("PAN_ANGLE" / "PAN_DIST")
// resolved to per-voice ControllerInst after liveKeyOn.
///////////////////////////////////////////////////////////////////////////////

prgdata_constptr_t
StochWavSoundEmitterSystem::_buildVoiceProgram(const PreloadedSound& preloaded, const StochSoundGroup& group, bool hasPanner) {
  auto& snd = preloaded._config;

  auto program = std::make_shared<ProgramData>();
  program->_name = "StochWav";

  auto layer = program->newLayer();

  // Center pan so PANNER2D's stereo output passes through to bus unmolested
  // (default _panmode=-1 with _pan=0 produces hard-left in mixToBus)
  layer->_panmode  = 4;     // use _floatPan directly
  layer->_floatPan = 0.0f;  // center

  // DSP stage: pitch + sampler
  auto dspStage = layer->appendStage("DSP");
  dspStage->setNumIos(2, 2);

  auto pchblock = dspStage->appendTypedBlock<PITCH>("pitch");
  layer->_pchBlock = pchblock;

  auto sampler = dspStage->appendTypedBlock<SAMPLER>("sampler");

  // Set anti-click fade times: per-sound overrides group defaults
  auto samplerData = std::static_pointer_cast<SAMPLER_DATA>(sampler);
  samplerData->_fadeInTime  = (snd->_fadeInTime > 0.0f) ? snd->_fadeInTime : group._fadeInTime;
  samplerData->_fadeOutTime = (snd->_fadeOutTime > 0.0f) ? snd->_fadeOutTime : group._fadeOutTime;

  // AMP stage
  auto ampStage = layer->appendStage("AMP");
  ampStage->setNumIos(2, 2);
  auto ampblock = ampStage->appendTypedBlock<AMP_MONOIO>("amp");

  // PANNER2D stage (per-voice spatialization via controllers)
  if (hasPanner) {
    auto panStage = layer->appendStage("PAN");
    panStage->setNumIos(2, 2);
    auto pannerBlock = panStage->appendTypedBlock<PANNER2D>("PANNER");

    // Copy spatializer config onto the PANNER2D_DATA block
    auto pannerData = std::static_pointer_cast<PANNER2D_DATA>(pannerBlock);
    auto spatData = std::static_pointer_cast<PannerSpatializerData>(_SCD._spatializer);
    if (spatData) {
      pannerData->_refDistance    = spatData->_refDistance;
      pannerData->_maxDistance    = spatData->_maxDistance;
      pannerData->_rolloff       = spatData->_rolloff;
      pannerData->_minGainDB     = spatData->_minGainDB;
      pannerData->_headShadowMix = spatData->_headShadowMix;
      pannerData->_iidBaseFreq   = spatData->_iidBaseFreq;
      pannerData->_iidMaxFreq    = spatData->_iidMaxFreq;
    }

    // Create per-voice controllers for panner ANGLE and DISTANCE.
    // Values are set from the game thread via ControllerInst::setFloatValue();
    // the audio thread reads them through the controller_t lambda.
    auto panAngleCtrl = layer->appendController<CustomControllerData>("PAN_ANGLE");
    panAngleCtrl->_oncompute = [](CustomControllerInst*) {};           // no-op: value set externally
    panAngleCtrl->_onkeyon   = [](CustomControllerInst*, const KeyOnInfo&) {}; // don't reset to 0

    auto panDistCtrl = layer->appendController<CustomControllerData>("PAN_DIST");
    panDistCtrl->_oncompute = [](CustomControllerInst*) {};
    panDistCtrl->_onkeyon   = [](CustomControllerInst* cci, const KeyOnInfo&) {
      cci->_value.x = 5.0f;  // reasonable default until game thread updates
    };

    // Wire controllers as src1 on ANGLE and DISTANCE params
    auto angleParam = pannerBlock->paramByName("ANGLE");
    angleParam->_mods->_src1      = panAngleCtrl;
    angleParam->_mods->_src1Scale = 1.0f;
    angleParam->_coarse           = 0.0f;  // controller provides the full value

    auto distParam = pannerBlock->paramByName("DISTANCE");
    distParam->_mods->_src1      = panDistCtrl;
    distParam->_mods->_src1Scale = 1.0f;
    distParam->_coarse           = 0.0f;   // controller provides the full value
  }

  // Use pre-loaded keymap (shared SampleData)
  layer->_keymap = preloaded._keymap;

  // Compute sample duration for drum-style auto-completing envelope
  float sampleRate = preloaded._sampleData->_sampleRate;
  if (sampleRate <= 0.0f) sampleRate = 48000.0f;
  float sampleDur = float(preloaded._sampleData->_blk_end) / sampleRate;

  float fadeIn  = std::max(0.001f, (snd->_fadeInTime > 0.0f) ? snd->_fadeInTime : group._fadeInTime);
  float fadeOut = std::max(0.001f, (snd->_fadeOutTime > 0.0f) ? snd->_fadeOutTime : group._fadeOutTime);
  float holdTime = std::max(0.01f, sampleDur - fadeIn - fadeOut);

  // Drum-style envelope: auto-completes, no sustain.
  // keyOff doesn't affect playback — sample plays through naturally.
  auto ampenv = layer->appendController<RateLevelEnvData>("AMPENV");
  ampenv->_ampenv = true;

  ampenv->addSegment("atk",  fadeIn,   1.0f, 1.0f);  // ramp up
  ampenv->addSegment("hold", holdTime, 1.0f, 1.0f);  // flat at 1.0 for sample duration
  ampenv->addSegment("rel",  fadeOut,  0.0f, 1.0f);  // ramp down
  // No sustainSegment, no releaseSegment — envelope runs to completion autonomously

  // Wire amp envelope to amp block
  ampblock->_paramd[0]->_mods->_src1      = ampenv;
  ampblock->_paramd[0]->_mods->_src1Scale = 1.0f;

  return program;
}

///////////////////////////////////////////////////////////////////////////////
// Lazy group runtime init — pre-loads WAV data, NO bus-level spatializer
///////////////////////////////////////////////////////////////////////////////

void StochWavSoundEmitterSystem::_ensureGroupRuntime(const std::string& groupName) {
  if (_groupRuntime.count(groupName))
    return;

  auto git = _SCD._soundGroups.find(groupName);
  if (git == _SCD._soundGroups.end()) {
    printf("StochWavSoundEmitter: group '%s' not found in SystemData\n", groupName.c_str());
    return;
  }

  auto& groupData = git->second;
  GroupRuntimeData grd;

  // Look up bus
  auto syn = synth::instance();
  grd._bus = syn->outputBus(groupData->_outputBusName);
  if (!grd._bus) {
    printf("StochWavSoundEmitter: bus '%s' not found for group '%s'\n",
           groupData->_outputBusName.c_str(), groupName.c_str());
    return;
  }

  // Record whether this system wants spatialization (per-voice PANNER2D)
  grd._hasSpatializer = (_SCD._spatializer != nullptr);

  // Pre-load WAV files and build keymaps (the expensive part — done once per sound)
  for (auto& [name, snd] : groupData->_sounds) {
    PreloadedSound ps;
    ps._config = snd;

    auto sd = std::make_shared<SampleData>();
    sd->_rootKey = 60;
    sd->_originalPitch = 261.63f * 0.5; // middle C frequency for key 60
    sd->_loopMode = eLoopMode::NONE;
    sd->loadFromAudioFile(snd->_wavFilePath.toAbsolute().toStdString());
    ps._sampleData = sd;

    // Compute sample duration for drum-style voice lifecycle
    float sr = (sd->_sampleRate > 0.0f) ? sd->_sampleRate : 48000.0f;
    ps._sampleDuration = float(sd->_blk_end) / sr;

    auto km = std::make_shared<KeyMapData>();
    km->_name = "km";
    auto region = std::make_shared<KmRegionData>();
    region->_lokey = 0;
    region->_hikey = 127;
    region->_lovel = 0;
    region->_hivel = 127;
    region->_sample = sd;
    km->_regions.push_back(region);
    ps._keymap = km;

    grd._soundsList.push_back(snd);
    grd._preloadedSounds.push_back(ps);
  }

  // Build programs once per sound (shared across all voice triggers)
  for (auto& ps : grd._preloadedSounds) {
    ps._program   = _buildVoiceProgram(ps, *groupData, grd._hasSpatializer);
    ps._hasPanner = grd._hasSpatializer;
  }

  _groupRuntime[groupName] = std::move(grd);
}


///////////////////////////////////////////////////////////////////////////////
// System update — Boids-pattern two-phase + Poisson state machine
///////////////////////////////////////////////////////////////////////////////

void StochWavSoundEmitterSystem::_onUpdate(Simulation* inst) {
  float dt  = inst->deltaTime();
  auto syn  = synth::instance();

  _systemElapsedTime += dt;

  //-----------------------------------------------------------------------
  // Update listener matrix from camera (SceneGraphSystem)
  //-----------------------------------------------------------------------
  if (_sgSystem && _sgSystem->_camera) {
    auto viewMtx = _sgSystem->_camera->computeViewMatrix();
    syn->_inv_listener_matrix = viewMtx;           // world → camera
    syn->_listener_matrix     = viewMtx.inverse();  // camera → world
  }

  // Pre-compute inverse listener matrix for panner updates
  fmtx4 invListenerMtx = syn->_inv_listener_matrix;

  //-----------------------------------------------------------------------
  // Phase 1: Build cache, group by _groupName
  //-----------------------------------------------------------------------
  _stateCache.clear();
  _groupMap.clear();

  for (auto* comp : _components) {
    if (!comp->_CD._enabled)
      continue;
    if (comp->_CD._groupName.empty())
      continue;

    CachedEmitterState s;
    s._component = comp;
    s._position  = comp->GetEntity()->GetEntityPosition();

    size_t idx = _stateCache.size();
    _groupMap[comp->_CD._groupName].push_back(idx);
    _stateCache.push_back(s);
  }

  //-----------------------------------------------------------------------
  // Phase 2: Per-group processing
  //-----------------------------------------------------------------------
  for (auto& [groupName, emitterIndices] : _groupMap) {
    auto grit = _groupRuntime.find(groupName);
    if (grit == _groupRuntime.end())
      continue;
    auto& grd = grit->second;

    if (grd._preloadedSounds.empty() || !grd._bus)
      continue;

    auto git = _SCD._soundGroups.find(groupName);
    if (git == _SCD._soundGroups.end())
      continue;
    auto& groupData = git->second;

    // Count active voices across all emitters in this group
    int groupActiveVoices = 0;
    for (size_t idx : emitterIndices) {
      auto* c = _stateCache[idx]._component;

      // Clean up finished voices — timer-based (drum-style: envelope auto-completes)
      auto& voices = c->_activeVoices;
      voices.erase(
          std::remove_if(
              voices.begin(), voices.end(),
              [c](const ActiveVoice& v) {
                if (!v._progInst)
                  return true;
                // Timer-based cleanup: voice done after sample duration + margin
                if (v._sampleDuration > 0.0f) {
                  float elapsed = c->_elapsedTime - v._startTime;
                  return elapsed >= (v._sampleDuration + 0.2f);
                }
                // Fallback for unknown duration
                return v._keyOffSent && v._progInst->_layers.empty();
              }),
          voices.end());

      groupActiveVoices += (int)voices.size();
    }
    grd._activeVoiceCount = groupActiveVoices;

    // Process each emitter's state machine
    for (size_t idx : emitterIndices) {
      auto* c   = _stateCache[idx]._component;
      auto& pos = _stateCache[idx]._position;

      c->_elapsedTime += dt;

      auto& voices = c->_activeVoices;

      // Apply deferred pitch on pending voices
      for (auto& v : voices) {
        if (!v._pitchApplied && v._progInst && v._pendingPitchCents != 0.0f) {
          for (auto& l : v._progInst->_layers) {
            if (l) {
              l->_curPitchOffsetInCents = v._pendingPitchCents;
              v._pitchApplied = true;
            }
          }
        }
      }

      // Send keyOff after sample duration (envelope auto-completes, no sustain segment).
      // We delay keyOff so that layers (and our controller pointers) stay alive
      // for the full audible playback — keyOff calls _layers.clear().
      for (auto& v : voices) {
        if (!v._keyOffSent && v._progInst && v._sampleDuration > 0.0f) {
          float elapsed = c->_elapsedTime - v._startTime;
          if (elapsed >= v._sampleDuration) {
            syn->liveKeyOff(v._progInst, 60, 0);
            v._keyOffSent = true;
            // Nullify controller pointers — layers will be cleared by keyOff
            v._panAngleCtrl = nullptr;
            v._panDistCtrl  = nullptr;
          }
        }
      }

      // Resolve per-voice panner controller instances (deferred — layers created by audio thread)
      // and update panner positions each frame.
      // Skip after keyOff — _layers.clear() destroys our controller targets.
      if (grd._hasSpatializer) {
        for (auto& v : voices) {
          if (v._keyOffSent)
            continue;  // layers cleared, controller pointers invalid

          // Resolve controller instances once layers are available
          if (!v._panControllersResolved && v._progInst && !v._progInst->_layers.empty()) {
            auto& layer = v._progInst->_layers[0];
            if (layer) {
              auto itA = layer->_controlMap.find("PAN_ANGLE");
              auto itD = layer->_controlMap.find("PAN_DIST");
              if (itA != layer->_controlMap.end()) v._panAngleCtrl = itA->second;
              if (itD != layer->_controlMap.end()) v._panDistCtrl  = itD->second;
            }
            v._panControllersResolved = true;
          }
          // Update panner from emitter/listener positions
          if (v._panAngleCtrl) {
            fvec4 relPos4 = fvec4(pos, 1.0f).transform(invListenerMtx);
            float dist  = std::max(1.0f, fvec3(relPos4.x, relPos4.y, relPos4.z).magnitude());
            float angle = -atan2f(relPos4.x, relPos4.z);
            v._panAngleCtrl->setFloatValue(angle);
            v._panDistCtrl->setFloatValue(dist);
          }
        }
      }

      switch (c->_state) {
        case EmitterState::IDLE: {
          grd._idleChecks++;

          // Compute combined Poisson rate across all sounds in group
          float totalWeight = 0.0f;
          for (auto& snd : grd._soundsList)
            totalWeight += snd->_selectionWeight;
          if (totalWeight <= 0.0f)
            break;

          float combinedRate = 0.0f;
          for (auto& snd : grd._soundsList) {
            combinedRate += snd->_burstRate * c->_CD._rateScale * snd->_selectionWeight / totalWeight;
          }

          // Poisson probability for this timestep
          float p = 1.0f - std::exp(-combinedRate * dt);
          std::uniform_real_distribution<float> roll(0.0f, 1.0f);

          bool poissonHit = roll(c->_rng) < p;
          bool voiceCapOk = grd._activeVoiceCount < groupData->_maxVoicesPerGroup;
          bool spacingOk  = (_systemElapsedTime - grd._lastTriggerTime) >= groupData->_minSpacing;

          if (!poissonHit) {
            grd._rejectedPoisson++;
          } else if (!voiceCapOk) {
            grd._rejectedVoiceCap++;
          } else if (!spacingOk) {
            grd._rejectedMinSpacing++;
          }

          if (poissonHit && voiceCapOk && spacingOk) {

            // Weighted random selection of sound
            std::uniform_real_distribution<float> weightDist(0.0f, totalWeight);
            float pick = weightDist(c->_rng);
            int selectedIdx = 0;
            float accum = 0.0f;
            for (int i = 0; i < (int)grd._soundsList.size(); i++) {
              accum += grd._soundsList[i]->_selectionWeight;
              if (pick <= accum) {
                selectedIdx = i;
                break;
              }
            }

            auto& snd = grd._soundsList[selectedIdx];

            // Random burst count
            int burstCount = snd->_burstCountMin;
            if (snd->_burstCountMax > snd->_burstCountMin) {
              std::uniform_int_distribution<int> burstDist(snd->_burstCountMin, snd->_burstCountMax);
              burstCount = burstDist(c->_rng);
            }

            c->_currentSoundIndex = selectedIdx;
            c->_burstRemaining    = burstCount;
            c->_state             = EmitterState::BURSTING;
            c->_nextBurstFireTime = c->_elapsedTime; // fire first chirp immediately
          }
          break;
        }

        case EmitterState::BURSTING: {
          grd._burstChecks++;

          bool timeReady  = c->_elapsedTime >= c->_nextBurstFireTime && c->_burstRemaining > 0;
          bool bVoiceOk   = grd._activeVoiceCount < groupData->_maxVoicesPerGroup;
          bool bSpacingOk = (_systemElapsedTime - grd._lastTriggerTime) >= groupData->_minSpacing;

          if (timeReady && (!bVoiceOk || !bSpacingOk)) {
            grd._burstBlocked++;
          }

          if (timeReady && bVoiceOk && bSpacingOk) {

            int si = c->_currentSoundIndex;
            auto& snd = grd._soundsList[si];

            // Random pitch variance + per-instance offset
            float pitchCents = c->_CD._pitchOffsetCents;
            if (snd->_pitchVarianceCents > 0.0f) {
              std::uniform_real_distribution<float> pitchDist(
                  -snd->_pitchVarianceCents, snd->_pitchVarianceCents);
              pitchCents += pitchDist(c->_rng);
            }

            // Random gain + group master + per-instance offset
            float gainDB = groupData->_masterGainDB + c->_CD._gainOffsetDB;
            if (snd->_gainMinDB != snd->_gainMaxDB) {
              float lo = std::min(snd->_gainMinDB, snd->_gainMaxDB);
              float hi = std::max(snd->_gainMinDB, snd->_gainMaxDB);
              std::uniform_real_distribution<float> gainDist(lo, hi);
              gainDB += gainDist(c->_rng);
            } else {
              gainDB += snd->_gainMinDB;
            }

            // Use pre-built shared program
            auto& preloaded = grd._preloadedSounds[si];

            // Route to group bus
            auto kmod = std::make_shared<KeyOnModifiers>();
            kmod->_outbus_override = grd._bus;

            // Trigger
            int velocity = std::clamp(int(audiomath::decibel_to_linear_amp_ratio(gainDB) * 127.0f), 1, 127);
            auto progInst = syn->liveKeyOn(60, velocity, preloaded._program, kmod);

            if (progInst) {
              ActiveVoice av;
              av._progInst          = progInst;
              av._soundIndex        = si;
              av._startTime         = c->_elapsedTime;
              av._pendingPitchCents = pitchCents;
              av._sampleDuration    = preloaded._sampleDuration;
              voices.push_back(av);
              grd._activeVoiceCount++;
              grd._lastTriggerTime = _systemElapsedTime;
              grd._totalTriggered++;
            }

            c->_burstRemaining--;

            if (c->_burstRemaining > 0) {
              // Schedule next intra-burst chirp via exponential distribution
              std::exponential_distribution<float> expDist(snd->_intraBurstRate);
              c->_nextBurstFireTime = c->_elapsedTime + expDist(c->_rng);
            } else {
              c->_state = EmitterState::IDLE;
            }
          }
          break;
        }
      }
    }
  }

  // Periodic stats dump
  if ((_systemElapsedTime - _lastStatsPrintTime) >= _statsPrintInterval) {
    _lastStatsPrintTime = _systemElapsedTime;
    printf("=== StochWav Stats @ %.1fs ===\n", _systemElapsedTime);
    for (auto& [groupName, emitterIndices] : _groupMap) {
      auto grit = _groupRuntime.find(groupName);
      if (grit == _groupRuntime.end()) continue;
      auto& grd = grit->second;
      auto git = _SCD._soundGroups.find(groupName);
      if (git == _SCD._soundGroups.end()) continue;
      auto& groupData = git->second;

      // Count emitter states
      int numIdle = 0, numBursting = 0;
      for (size_t idx : emitterIndices) {
        auto* c = _stateCache[idx]._component;
        if (c->_state == EmitterState::IDLE) numIdle++;
        else numBursting++;
      }

      float combinedRate = 0.0f;
      float totalWeight = 0.0f;
      for (auto& snd : grd._soundsList) totalWeight += snd->_selectionWeight;
      if (totalWeight > 0.0f) {
        for (auto& snd : grd._soundsList)
          combinedRate += snd->_burstRate * snd->_selectionWeight / totalWeight;
      }

      printf("  [%s] emitters:%zu (idle:%d burst:%d) sounds:%zu\n",
             groupName.c_str(), emitterIndices.size(), numIdle, numBursting, grd._soundsList.size());
      printf("    config: maxVoices=%d minSpacing=%.2fs burstRate=%.3fHz/emitter\n",
             groupData->_maxVoicesPerGroup, groupData->_minSpacing, combinedRate);
      printf("    voices: active=%d triggered=%d\n",
             grd._activeVoiceCount, grd._totalTriggered);
      printf("    idle_checks=%d poisson_miss=%d voicecap_reject=%d spacing_reject=%d\n",
             grd._idleChecks, grd._rejectedPoisson, grd._rejectedVoiceCap, grd._rejectedMinSpacing);
      printf("    burst_checks=%d burst_blocked=%d\n",
             grd._burstChecks, grd._burstBlocked);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
