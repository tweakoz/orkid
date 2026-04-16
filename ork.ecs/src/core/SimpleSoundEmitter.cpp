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

#include "SimpleSoundEmitter_impl.h"
#include <ork/lev2/aud/singularity/konoff.h>
#include <ork/math/audiomath.h>
#include <ork/file/path.h>
#include <ork/ecs/datatable.h>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::SimpleSoundData, "SimpleSoundData");
ImplementReflectionX(ork::ecs::SimpleSoundEmitterData, "SimpleSoundEmitterData");
ImplementReflectionX(ork::ecs::SimpleSoundEmitterComponent, "SimpleSoundEmitterComponent");
ImplementReflectionX(ork::ecs::SimpleSoundEmitterSystemData, "SimpleSoundEmitterSystemData");
ImplementReflectionX(ork::ecs::SimpleSoundEmitterSystem, "SimpleSoundEmitterSystem");

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

using namespace ork::audio::singularity;

///////////////////////////////////////////////////////////////////////////////
// SimpleSoundData
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("WavFile", &SimpleSoundData::_wavFilePath)
      ->annotate("editor.filetype", "wav")
      ->annotate("editor.filebase", "<assetcache>");
  clazz->directProperty("Looping", &SimpleSoundData::_looping);
  clazz->directProperty("Spatialize", &SimpleSoundData::_spatialize);
  clazz->floatProperty("GainDB", float_range{-96, 24}, &SimpleSoundData::_gainDB);
  clazz->floatProperty("PitchOffsetCents", float_range{-2400, 2400}, &SimpleSoundData::_pitchOffsetCents);
  clazz->floatProperty("FadeInTime", float_range{0, 10}, &SimpleSoundData::_fadeInTime);
  clazz->floatProperty("FadeOutTime", float_range{0, 10}, &SimpleSoundData::_fadeOutTime);
  clazz->directProperty("OutputBusName", &SimpleSoundData::_outputBusName);
}

///////////////////////////////////////////////////////////////////////////////
// SimpleSoundEmitterData
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundEmitterData::describeX(ComponentDataClass* clazz) {
  clazz->directProperty("SoundName", &SimpleSoundEmitterData::_soundName);
  clazz->directProperty("AutoPlay", &SimpleSoundEmitterData::_autoPlay);
  clazz->directProperty("Enabled", &SimpleSoundEmitterData::_enabled);
  clazz->floatProperty("GainOffsetDB", float_range{-96, 24}, &SimpleSoundEmitterData::_gainOffsetDB);
  clazz->floatProperty("PitchOffsetCents", float_range{-2400, 2400}, &SimpleSoundEmitterData::_pitchOffsetCents);
}

SimpleSoundEmitterData::SimpleSoundEmitterData() {
}

Component* SimpleSoundEmitterData::createComponent(ecs::Entity* pent) const {
  return new SimpleSoundEmitterComponent(*this, pent);
}

object::ObjectClass* SimpleSoundEmitterData::componentClass() {
  return SimpleSoundEmitterComponent::GetClassStatic();
}

void SimpleSoundEmitterData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::SimpleSoundEmitterSystemData>();
}

///////////////////////////////////////////////////////////////////////////////
// SimpleSoundEmitterComponent
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundEmitterComponent::describeX(object::ObjectClass* clazz) {
}

SimpleSoundEmitterComponent::SimpleSoundEmitterComponent(
    const SimpleSoundEmitterData& data,
    ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , _CD(data) {
}

void SimpleSoundEmitterComponent::_onUninitialize(Simulation* psi) {
}

bool SimpleSoundEmitterComponent::_onLink(Simulation* psi) {
  _system = psi->findSystem<SimpleSoundEmitterSystem>();
  return true;
}

void SimpleSoundEmitterComponent::_onUnlink(Simulation* psi) {
}

bool SimpleSoundEmitterComponent::_onStage(Simulation* psi) {
  return true;
}

void SimpleSoundEmitterComponent::_onUnstage(Simulation* psi) {
}

bool SimpleSoundEmitterComponent::_onActivate(Simulation* psi) {
  _system->_onActivateComponent(this);
  return true;
}

void SimpleSoundEmitterComponent::_onDeactivate(Simulation* psi) {
  printf("SimpleSoundComponent::_onDeactivate: %p, %zu voices, playing=%d\n", this, _activeVoices.size(), _playing);
  _system->_stopVoices(this);
  _system->_onDeactivateComponent(this);
}

void SimpleSoundEmitterComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
  // Could respond to "play"/"stop" events here in the future
}

void SimpleSoundEmitterComponent::fadeToGain(float targetLinear, float duration) {
  targetLinear = std::clamp(targetLinear, 0.0f, 1.0f);
  for (auto& voice : _activeVoices) {
    voice._fadeTargetLinear = targetLinear;
    if (duration <= 0.0f) {
      voice._fadeGainLinear = targetLinear;
      voice._fadeRatePerSec = 0.0f;
    } else {
      float delta = targetLinear - voice._fadeGainLinear;
      voice._fadeRatePerSec = delta / duration;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// SimpleSoundEmitterSystemData
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundEmitterSystemData::describeX(SystemDataClass* clazz) {
  clazz->directObjectMapProperty("Sounds", &SimpleSoundEmitterSystemData::_sounds)
      ->annotate<ConstString>("editor.factorylistbase", "SimpleSoundData");
  clazz->directObjectProperty("Spatializer", &SimpleSoundEmitterSystemData::_spatializer)
      ->annotate<ConstString>("editor.factorylistbase", "SpatializerData");
}

SimpleSoundEmitterSystemData::SimpleSoundEmitterSystemData() {
}

System* SimpleSoundEmitterSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new SimpleSoundEmitterSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////
// SimpleSoundEmitterSystem
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundEmitterSystem::describeX(object::ObjectClass* clazz) {
}

SimpleSoundEmitterSystem::SimpleSoundEmitterSystem(
    const SimpleSoundEmitterSystemData& data,
    ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst)
    , _SCD(data) {
}

void SimpleSoundEmitterSystem::_onActivateComponent(SimpleSoundEmitterComponent* component) {
  _components.insert(component);
  if (!component->_CD._soundName.empty()) {
    _ensureSoundLoaded(component->_CD._soundName);
  }
}

void SimpleSoundEmitterSystem::_onDeactivateComponent(SimpleSoundEmitterComponent* component) {
  _components.erase(component);
}

bool SimpleSoundEmitterSystem::_onLink(Simulation* psi) {
  _sgSystem = psi->findSystem<SceneGraphSystem>();
  return true;
}

void SimpleSoundEmitterSystem::_onUnLink(Simulation* psi) {
}

bool SimpleSoundEmitterSystem::_onStage(Simulation* psi) {
  return true;
}

void SimpleSoundEmitterSystem::_onUnstage(Simulation* inst) {
}

bool SimpleSoundEmitterSystem::_onActivate(Simulation* psi) {
  return true;
}

void SimpleSoundEmitterSystem::_onNotify(token_t evID, evdata_t data) {
  if (evID.hashed() == "FADE_GAIN"_crcu) {
    auto table = data.getShared<DataTable>();
    float targetLinear = table->operator[]("gain"_tok).get<float>();
    float duration     = table->operator[]("duration"_tok).get<float>();
    for (auto* comp : _components) {
      comp->fadeToGain(targetLinear, duration);
    }
  }
}

void SimpleSoundEmitterSystem::_onDeactivate(Simulation* inst) {
  printf("SimpleSound::_onDeactivate: %zu components\n", _components.size());
  auto syn = synth::instance();
  for (auto* comp : _components) {
    printf("  comp %p: %zu voices, playing=%d\n", comp, comp->_activeVoices.size(), comp->_playing);
    for (auto& voice : comp->_activeVoices) {
      if (voice._progInst) {
        printf("    keyOff progInst %p\n", voice._progInst);
        syn->liveKeyOff(voice._progInst, 60, 0);
      }
    }
    comp->_activeVoices.clear();
    comp->_playing = false;
  }
  _components.clear();
  _preloadedSounds.clear();
}

///////////////////////////////////////////////////////////////////////////////
// Per-voice program builder
///////////////////////////////////////////////////////////////////////////////

SimpleSoundEmitterSystem::VoiceProgramResult
SimpleSoundEmitterSystem::_buildVoiceProgram(const PreloadedSimpleSound& preloaded) {
  VoiceProgramResult result;
  auto& snd = preloaded._config;

  auto program = std::make_shared<ProgramData>();
  program->_name = "SimpleSound";

  auto layer = program->newLayer();

  // Center pan so PANNER2D's stereo output passes through to bus
  layer->_panmode  = 4;
  layer->_floatPan = 0.0f;

  // DSP stage: pitch + sampler
  auto dspStage = layer->appendStage("DSP");
  dspStage->setNumIos(2, 2);

  auto pchblock = dspStage->appendTypedBlock<PITCH>("pitch");
  layer->_pchBlock = pchblock;

  auto sampler = dspStage->appendTypedBlock<SAMPLER>("sampler");

  // Set anti-click fade times on the sampler from sound config
  auto samplerData = std::static_pointer_cast<SAMPLER_DATA>(sampler);
  samplerData->_fadeInTime  = snd->_fadeInTime;
  samplerData->_fadeOutTime = snd->_fadeOutTime;

  // AMP stage
  auto ampStage = layer->appendStage("AMP");
  ampStage->setNumIos(2, 2);
  auto ampblock = ampStage->appendTypedBlock<AMP_MONOIO>("amp");

  // PANNER2D stage (per-voice spatialization)
  if (snd->_spatialize && _SCD._spatializer) {
    auto panStage = layer->appendStage("PAN");
    panStage->setNumIos(2, 2);
    auto pannerBlock = panStage->appendTypedBlock<PANNER2D>("PANNER");
    result._pannerBlock = pannerBlock;

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
  }

  // Use pre-loaded keymap
  layer->_keymap = preloaded._keymap;

  // Amplitude envelope
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

  result._program = program;
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// Pre-load WAV data
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundEmitterSystem::_ensureSoundLoaded(const std::string& soundName) {
  if (_preloadedSounds.count(soundName))
    return;

  auto sit = _SCD._sounds.find(soundName);
  if (sit == _SCD._sounds.end()) {
    printf("SimpleSoundEmitter: sound '%s' not found in SystemData\n", soundName.c_str());
    return;
  }

  auto& sndData = sit->second;
  PreloadedSimpleSound ps;
  ps._config = sndData;

  // Look up bus
  auto syn = synth::instance();
  ps._bus = syn->outputBus(sndData->_outputBusName);
  if (!ps._bus) {
    printf("SimpleSoundEmitter: bus '%s' not found for sound '%s'\n",
           sndData->_outputBusName.c_str(), soundName.c_str());
    return;
  }

  // Load WAV
  auto sd = std::make_shared<SampleData>();
  sd->_rootKey = 60;
  sd->_originalPitch = 261.63f * 0.5;
  sd->loadFromAudioFile(file::Path::expandPathString(sndData->_wavFilePath.toStdString()));

  // Set loop mode
  if (sndData->_looping) {
    sd->_loopMode = eLoopMode::FWD;
    sd->_blk_loopstart = sd->_blk_start;
    sd->_blk_loopend   = sd->_blk_end;
  } else {
    sd->_loopMode = eLoopMode::NONE;
  }

  ps._sampleData = sd;

  // Build keymap
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

  _preloadedSounds[soundName] = std::move(ps);
}

///////////////////////////////////////////////////////////////////////////////
// Trigger a voice
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundEmitterSystem::_triggerVoice(
    SimpleSoundEmitterComponent* comp,
    const fmtx4& invListenerMtx,
    float dt) {

  auto pit = _preloadedSounds.find(comp->_CD._soundName);
  if (pit == _preloadedSounds.end())
    return;

  auto& preloaded = pit->second;
  auto& sndData   = preloaded._config;

  auto voiceResult = _buildVoiceProgram(preloaded);

  // Set initial panner position (snap)
  fvec3 pos = comp->GetEntity()->GetEntityPosition();
  if (voiceResult._pannerBlock) {
    fvec4 relPos4 = fvec4(pos, 1.0f).transform(invListenerMtx);
    float dist  = std::max(1.0f, fvec3(relPos4.x, relPos4.y, relPos4.z).magnitude());
    float angle = -atan2f(relPos4.x, relPos4.z);
    auto ap = voiceResult._pannerBlock->paramByName("ANGLE");
    auto dp = voiceResult._pannerBlock->paramByName("DISTANCE");
    if (ap) ap->_coarse = angle;
    if (dp) dp->_coarse = dist;
  }

  // Route to bus
  auto kmod = std::make_shared<KeyOnModifiers>();
  kmod->_outbus_override = preloaded._bus;

  // Combined gain
  float gainDB = sndData->_gainDB + comp->_CD._gainOffsetDB;
  int velocity = std::clamp(int(audiomath::decibel_to_linear_amp_ratio(gainDB) * 127.0f), 1, 127);

  auto syn = synth::instance();
  auto progInst = syn->liveKeyOn(60, velocity, voiceResult._program, kmod);

  if (progInst) {
    ActiveSimpleVoice av;
    av._progInst         = progInst;
    av._program          = voiceResult._program;
    av._pannerBlock      = voiceResult._pannerBlock;
    av._startTime        = 0.0f;
    av._fadeGainLinear   = comp->_CD._initialFadeGainLinear;
    av._fadeTargetLinear = av._fadeGainLinear;
    progInst->_fadeGainLinear = av._fadeGainLinear;
    // Compute one-shot sample duration for auto-keyOff (skip for looping sounds)
    if (!sndData->_looping && preloaded._sampleData && preloaded._sampleData->_sampleRate > 0.0f) {
      av._sampleDuration = float(preloaded._sampleData->_blk_end - preloaded._sampleData->_blk_start)
                         / preloaded._sampleData->_sampleRate;
    }

    // Initialize smoothed panner state
    if (voiceResult._pannerBlock) {
      fvec4 rp = fvec4(pos, 1.0f).transform(invListenerMtx);
      av._smoothedAngle    = -atan2f(rp.x, rp.z);
      av._smoothedDistance = std::max(1.0f, fvec3(rp.x, rp.y, rp.z).magnitude());
      av._pannerSmoothed   = true;
    }

    // Apply pitch offset
    float pitchCents = sndData->_pitchOffsetCents + comp->_CD._pitchOffsetCents;
    if (pitchCents != 0.0f) {
      for (auto& l : progInst->_layers) {
        if (l) {
          l->_curPitchOffsetInCents = pitchCents;
        }
      }
    }

    comp->_activeVoices.push_back(av);
    comp->_playing = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Stop voices (with release envelope)
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundEmitterSystem::_stopVoices(SimpleSoundEmitterComponent* comp) {
  auto syn = synth::instance();
  for (auto& voice : comp->_activeVoices) {
    if (voice._progInst) {
      syn->liveKeyOff(voice._progInst, 60, 0);
    }
  }
  // Don't clear voices immediately — let them release naturally.
  // They'll be cleaned up in _onUpdate when isDone() is true.
  comp->_playing = false;
}

///////////////////////////////////////////////////////////////////////////////
// Panner smoothing (same algorithm as StochWavSoundEmitter)
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundEmitterSystem::_setPannerParams(
    ActiveSimpleVoice& voice,
    const fmtx4& invListenerMatrix,
    const fvec3& emitterPos,
    float dt) {
  if (!voice._pannerBlock)
    return;

  fvec4 relPos4 = fvec4(emitterPos, 1.0f).transform(invListenerMatrix);
  fvec3 relPos(relPos4.x, relPos4.y, relPos4.z);

  float targetDistance = std::max(1.0f, relPos.magnitude());
  // Negate: PANNER2D convention has positive angle = left, but view-space +X = right
  float targetAngle   = -atan2f(relPos.x, relPos.z);

  if (!voice._pannerSmoothed) {
    voice._smoothedAngle    = targetAngle;
    voice._smoothedDistance  = targetDistance;
    voice._pannerSmoothed   = true;
  } else {
    constexpr float kSmoothRate = 20.0f;
    float alpha = 1.0f - std::exp(-kSmoothRate * dt);

    // Wrap-aware angle interpolation
    float angleDiff = targetAngle - voice._smoothedAngle;
    constexpr float PI = 3.14159265f;
    while (angleDiff >  PI) angleDiff -= 2.0f * PI;
    while (angleDiff < -PI) angleDiff += 2.0f * PI;
    voice._smoothedAngle += angleDiff * alpha;

    voice._smoothedDistance += (targetDistance - voice._smoothedDistance) * alpha;
  }

  auto angleParam = voice._pannerBlock->paramByName("ANGLE");
  auto distParam  = voice._pannerBlock->paramByName("DISTANCE");
  if (angleParam)
    angleParam->_coarse = voice._smoothedAngle;
  if (distParam)
    distParam->_coarse = voice._smoothedDistance;
}

///////////////////////////////////////////////////////////////////////////////
// System update
///////////////////////////////////////////////////////////////////////////////

void SimpleSoundEmitterSystem::_onUpdate(Simulation* inst) {
  float dt  = inst->deltaTime();
  auto syn  = synth::instance();

  //-----------------------------------------------------------------------
  // Update listener matrix from camera
  //-----------------------------------------------------------------------
  if (_sgSystem && _sgSystem->_camera) {
    auto viewMtx = _sgSystem->_camera->computeViewMatrix();
    syn->_inv_listener_matrix = viewMtx;
    syn->_listener_matrix     = viewMtx.inverse();
  }

  fmtx4 invListenerMtx = syn->_inv_listener_matrix;

  //-----------------------------------------------------------------------
  // Process each component
  //-----------------------------------------------------------------------
  for (auto* comp : _components) {
    if (!comp->_CD._enabled)
      continue;
    if (comp->_CD._soundName.empty())
      continue;

    // Clean up finished voices
    auto& voices = comp->_activeVoices;
    voices.erase(
        std::remove_if(
            voices.begin(), voices.end(),
            [](const ActiveSimpleVoice& v) {
              if (!v._progInst)
                return true;
              // layers may be empty if audio thread hasn't processed keyOn yet
              if (v._progInst->_layers.empty())
                return false;
              for (auto& l : v._progInst->_layers) {
                if (l && !l->isDone())
                  return false;
              }
              return true;
            }),
        voices.end());

    // Auto-play: trigger voice if autoPlay and not yet playing
    if (comp->_CD._autoPlay && voices.empty() && !comp->_playing) {
      _triggerVoice(comp, invListenerMtx, dt);
    }

    // For non-looping sounds that finished naturally, allow re-trigger
    auto pit = _preloadedSounds.find(comp->_CD._soundName);
    if (pit != _preloadedSounds.end() && !pit->second._config->_looping) {
      if (voices.empty()) {
        comp->_playing = false;
      }
    }

    // Update panner positions and fade gain for active voices
    fvec3 pos = comp->GetEntity()->GetEntityPosition();
    for (auto& v : voices) {
      if (v._pannerBlock) {
        _setPannerParams(v, invListenerMtx, pos, dt);
      }
      // Tick fade gain ramp
      if (v._fadeRatePerSec != 0.0f) {
        v._fadeGainLinear += v._fadeRatePerSec * dt;
        bool done = (v._fadeRatePerSec > 0.0f)
                      ? (v._fadeGainLinear >= v._fadeTargetLinear)
                      : (v._fadeGainLinear <= v._fadeTargetLinear);
        if (done) {
          v._fadeGainLinear = v._fadeTargetLinear;
          v._fadeRatePerSec = 0.0f;
        }
      }
      // Apply to synth voice
      if (v._progInst) {
        v._progInst->_fadeGainLinear = v._fadeGainLinear;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
