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
  clazz->floatProperty("Probability", float_range{0, 100}, &StochWavSound::_probability);
  clazz->floatProperty("PostSilenceMin", float_range{0, 60}, &StochWavSound::_postSilenceMin);
  clazz->floatProperty("PostSilenceMax", float_range{0, 60}, &StochWavSound::_postSilenceMax);
  clazz->floatProperty("PitchVarianceCents", float_range{0, 2400}, &StochWavSound::_pitchVarianceCents);
  clazz->floatProperty("GainMinDB", float_range{-96, 24}, &StochWavSound::_gainMinDB);
  clazz->floatProperty("GainMaxDB", float_range{-96, 24}, &StochWavSound::_gainMaxDB);
  clazz->floatProperty("FadeInTime", float_range{0, 10}, &StochWavSound::_fadeInTime);
  clazz->floatProperty("FadeOutTime", float_range{0, 10}, &StochWavSound::_fadeOutTime);
}

///////////////////////////////////////////////////////////////////////////////
// StochWavSoundEmitterData
///////////////////////////////////////////////////////////////////////////////

void StochWavSoundEmitterData::describeX(ComponentDataClass* clazz) {
  clazz->directObjectMapProperty("Sounds", &StochWavSoundEmitterData::_sounds)
      ->annotate<ConstString>("editor.factorylistbase", "StochWavSound");
  clazz->directObjectProperty("Spatializer", &StochWavSoundEmitterData::_spatializer)
      ->annotate<ConstString>("editor.factorylistbase", "SpatializerData");
  clazz->directProperty("OutputBusName", &StochWavSoundEmitterData::_outputBusName);
  clazz->floatProperty("MasterGainDB", float_range{-96, 24}, &StochWavSoundEmitterData::_masterGainDB);
  clazz->directProperty("MaxVoices", &StochWavSoundEmitterData::_maxVoices);
  clazz->directProperty("Enabled", &StochWavSoundEmitterData::_enabled);
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

  // Look up existing bus by name from synth
  auto syn = synth::instance();
  _bus     = syn->outputBus(_CD._outputBusName);
  if (!_bus) {
    printf("StochWavSoundEmitter: bus '%s' not found\n", _CD._outputBusName.c_str());
    return true;
  }

  // If spatializer configured, install DSP on this bus
  if (_CD._spatializer) {
    _spatializer = _CD._spatializer->createInstance();
    _spatializer->configureBus(_bus);
  }

  // Flatten map values into vector for runtime access
  for (auto& [name, snd] : _CD._sounds) {
    _soundsList.push_back(snd);
  }

  // Build programs for each StochWavSound
  for (auto& snd : _soundsList) {
    _programs.push_back(_buildProgram(snd));
  }

  // Randomize initial trigger time
  std::uniform_real_distribution<float> initDist(0.0f, 1.0f);
  _nextTriggerTime = initDist(_rng);
  _elapsedTime     = 0.0f;

  return true;
}

void StochWavSoundEmitterComponent::_onDeactivate(Simulation* psi) {
  // Release all active voices
  auto syn = synth::instance();
  for (auto& voice : _activeVoices) {
    if (voice._progInst) {
      syn->liveKeyOff(voice._progInst, 60, 0);
    }
  }
  _activeVoices.clear();
  _programs.clear();
  _soundsList.clear();
  _spatializer = nullptr;
  _bus         = nullptr;

  _system->_onDeactivateComponent(this);
}

void StochWavSoundEmitterComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
}

///////////////////////////////////////////////////////////////////////////////
// Program builder
///////////////////////////////////////////////////////////////////////////////

prgdata_constptr_t StochWavSoundEmitterComponent::_buildProgram(const stochwavsnd_ptr_t& snd) {
  auto program = std::make_shared<ProgramData>();
  program->_name = "StochWav";

  auto layer = program->newLayer();

  // DSP stage: pitch + sampler
  auto dspStage = layer->appendStage("DSP");
  dspStage->setNumIos(2, 2);

  auto pchblock = dspStage->appendTypedBlock<PITCH>("pitch");
  layer->_pchBlock = pchblock;

  auto sampler = dspStage->appendTypedBlock<SAMPLER>("sampler");

  // AMP stage
  auto ampStage = layer->appendStage("AMP");
  ampStage->setNumIos(2, 2);
  auto ampblock = ampStage->appendTypedBlock<AMP_MONOIO>("amp");

  // Load WAV file → SampleData → KeyMap
  auto sd = std::make_shared<SampleData>();
  sd->loadFromAudioFile(snd->_wavFilePath.toAbsolute().toStdString());
  sd->_rootKey = 60;
  sd->_loopMode = eLoopMode::NONE;

  // KeyMap: full range mapped to this sample
  auto km = std::make_shared<KeyMapData>();
  km->_name = "km";
  auto region = std::make_shared<KmRegionData>();
  region->_lokey = 0;
  region->_hikey = 127;
  region->_lovel = 0;
  region->_hivel = 127;
  region->_sample = sd;
  km->_regions.push_back(region);
  layer->_keymap = km;

  // Amplitude envelope (fade in/out)
  auto ampenv = layer->appendController<RateLevelEnvData>("AMPENV");
  ampenv->_ampenv = true;

  float fadeIn  = std::max(0.001f, snd->_fadeInTime);
  float fadeOut = std::max(0.001f, snd->_fadeOutTime);

  ampenv->addSegment("atk", fadeIn, 1.0f, 1.0f);
  ampenv->addSegment("sus", 1.0f, 1.0f, 1.0f);
  ampenv->addSegment("rel", fadeOut, 0.0f, 1.0f);
  ampenv->_sustainSegment = 1;
  ampenv->_releaseSegment = 2;

  // Wire amp envelope to amp block
  ampblock->_paramd[0]->_mods->_src1      = ampenv;
  ampblock->_paramd[0]->_mods->_src1Scale = 1.0f;

  return program;
}

///////////////////////////////////////////////////////////////////////////////
// StochWavSoundEmitterSystemData
///////////////////////////////////////////////////////////////////////////////

void StochWavSoundEmitterSystemData::describeX(SystemDataClass* clazz) {
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
    : ork::ecs::System(&data, pinst) {
}

void StochWavSoundEmitterSystem::_onStageComponent(StochWavSoundEmitterComponent* component) {
}

void StochWavSoundEmitterSystem::_onUnstageComponent(StochWavSoundEmitterComponent* component) {
}

void StochWavSoundEmitterSystem::_onActivateComponent(StochWavSoundEmitterComponent* component) {
  _components.insert(component);
}

void StochWavSoundEmitterSystem::_onDeactivateComponent(StochWavSoundEmitterComponent* component) {
  auto it = _components.find(component);
  if (it != _components.end()) {
    _components.erase(it);
  }
}

bool StochWavSoundEmitterSystem::_onLink(Simulation* psi) {
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
}

///////////////////////////////////////////////////////////////////////////////
// System update
///////////////////////////////////////////////////////////////////////////////

void StochWavSoundEmitterSystem::_onUpdate(Simulation* inst) {
  float dt  = inst->deltaTime();
  auto syn  = synth::instance();

  for (auto* c : _components) {
    if (!c->_CD._enabled || c->_programs.empty() || !c->_bus)
      continue;

    c->_elapsedTime += dt;

    // Apply deferred pitch variance once layers are ready
    auto& voices = c->_activeVoices;

    // Clean up finished voices
    voices.erase(
        std::remove_if(
            voices.begin(), voices.end(),
            [](const ActiveVoice& v) {
              if (!v._progInst)
                return true;
              // Check if all layers are done
              for (auto& l : v._progInst->_layers) {
                if (l && !l->isDone())
                  return false;
              }
              return true;
            }),
        voices.end());

    // Trigger new voices
    if (c->_elapsedTime >= c->_nextTriggerTime &&
        (int)voices.size() < c->_CD._maxVoices) {

      // Weighted random selection
      float totalWeight = 0.0f;
      for (auto& snd : c->_soundsList) {
        totalWeight += snd->_probability;
      }
      if (totalWeight <= 0.0f)
        continue;

      std::uniform_real_distribution<float> weightDist(0.0f, totalWeight);
      float pick = weightDist(c->_rng);
      int selectedIdx = 0;
      float accum = 0.0f;
      for (int i = 0; i < (int)c->_soundsList.size(); i++) {
        accum += c->_soundsList[i]->_probability;
        if (pick <= accum) {
          selectedIdx = i;
          break;
        }
      }

      auto& snd = c->_soundsList[selectedIdx];

      // Random pitch variance
      float pitchCents = 0.0f;
      if (snd->_pitchVarianceCents > 0.0f) {
        std::uniform_real_distribution<float> pitchDist(
            -snd->_pitchVarianceCents, snd->_pitchVarianceCents);
        pitchCents = pitchDist(c->_rng);
      }

      // Random gain
      float gainDB = c->_CD._masterGainDB;
      if (snd->_gainMinDB != snd->_gainMaxDB) {
        float lo = std::min(snd->_gainMinDB, snd->_gainMaxDB);
        float hi = std::max(snd->_gainMinDB, snd->_gainMaxDB);
        std::uniform_real_distribution<float> gainDist(lo, hi);
        gainDB += gainDist(c->_rng);
      } else {
        gainDB += snd->_gainMinDB;
      }

      // KeyOnModifiers: route to the target bus
      auto kmod = std::make_shared<KeyOnModifiers>();
      kmod->_outbus_override = c->_bus;

      // Trigger (use liveKeyOn for thread safety — defers keyOn to audio thread)
      int velocity = std::clamp(int(audiomath::decibel_to_linear_amp_ratio(gainDB) * 127.0f), 1, 127);
      auto progInst = syn->liveKeyOn(60, velocity, c->_programs[selectedIdx], kmod);

      if (progInst) {
        ActiveVoice av;
        av._progInst       = progInst;
        av._soundIndex     = selectedIdx;
        av._startTime      = c->_elapsedTime;
        av._pendingPitchCents = pitchCents;
        voices.push_back(av);
      }

      // Schedule next trigger: after silence gap
      float silMin = snd->_postSilenceMin;
      float silMax = std::max(silMin, snd->_postSilenceMax);
      std::uniform_real_distribution<float> silDist(silMin, silMax);
      c->_nextTriggerTime = c->_elapsedTime + silDist(c->_rng);
    }

    // Update spatializer
    if (c->_spatializer) {
      auto entPos = c->GetEntity()->GetEntityPosition();
      c->_spatializer->updateSpatialParams(syn->_listener_matrix, entPos);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
