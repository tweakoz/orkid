////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>

#include <ork/ecs/GlobalSynthSystem.h>
#include <ork/math/audiomath.h>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::SynthBusConfig, "SynthBusConfig");
ImplementReflectionX(ork::ecs::GlobalSynthSystemData, "GlobalSynthSystemData");
ImplementReflectionX(ork::ecs::GlobalSynthSystem, "GlobalSynthSystem");

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

using namespace ork::audio::singularity;

///////////////////////////////////////////////////////////////////////////////
// SynthBusConfig
///////////////////////////////////////////////////////////////////////////////

using choices_provider_t = std::function<std::vector<std::string>()>;

void SynthBusConfig::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("EffectPreset", &SynthBusConfig::_effectPreset)
      ->annotate<choices_provider_t>(
          "editor.choiceprovider",
          []() -> std::vector<std::string> {
            std::vector<std::string> names;
            auto syn = synth::instance();
            if (syn) {
              for (auto& p : syn->_fxpresets) {
                names.push_back(p->_name);
              }
            }
            return names;
          });
  clazz->floatProperty("GainDB", float_range{-96, 24}, &SynthBusConfig::_gainDB);
  clazz->floatProperty("Pan", float_range{-1, 1}, &SynthBusConfig::_pan);
  clazz->directProperty("Mute", &SynthBusConfig::_mute);
  clazz->directProperty("Solo", &SynthBusConfig::_solo);
}

///////////////////////////////////////////////////////////////////////////////
// GlobalSynthSystemData
///////////////////////////////////////////////////////////////////////////////

void GlobalSynthSystemData::describeX(SystemDataClass* clazz) {
  clazz->directObjectMapProperty("BusConfigs", &GlobalSynthSystemData::_busConfigs)
      ->annotate<ConstString>("editor.factorylistbase", "SynthBusConfig");
  clazz->floatProperty("MasterGainDB", float_range{-96, 24}, &GlobalSynthSystemData::_masterGainDB);
  clazz->intProperty("VoiceStealPolicy", int_range{0, 3}, &GlobalSynthSystemData::_voiceStealPolicy);
  clazz->intProperty("VoiceHeadroom", int_range{0, kmaxlayerspersynth / 2}, &GlobalSynthSystemData::_voiceHeadroom);
}

GlobalSynthSystemData::GlobalSynthSystemData() {
}

System* GlobalSynthSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new GlobalSynthSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////
// GlobalSynthSystem
///////////////////////////////////////////////////////////////////////////////

void GlobalSynthSystem::describeX(object::ObjectClass* clazz) {
}

GlobalSynthSystem::GlobalSynthSystem(
    const GlobalSynthSystemData& data,
    ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst)
    , _SCD(data) {
}

bool GlobalSynthSystem::_onActivate(Simulation* psi) {
  auto syn = synth::instance();

  for (auto& [busName, cfg] : _SCD._busConfigs) {
    auto bus = syn->outputBus(busName);
    if (!bus) {
      bus = syn->createOutputBus(busName);
    }

    auto cfgCopy = cfg;
    syn->addEvent(0, [syn, bus, cfgCopy]() {
      bus->_prog_gain = cfgCopy->_gainDB;
      bus->_pan       = cfgCopy->_pan;
      bus->_mute      = cfgCopy->_mute;
      if (cfgCopy->_solo && !bus->_solo) {
        syn->_num_soloed++;
      }
      bus->_solo = cfgCopy->_solo;
    });

    if (!cfg->_effectPreset.empty() && cfg->_effectPreset != "none") {
      syn->setEffect(bus, cfg->_effectPreset);
    }
  }

  float masterLin = ork::audiomath::decibel_to_linear_amp_ratio(_SCD._masterGainDB);
  syn->addEvent(0, [syn, masterLin]() {
    syn->_masterGain = masterLin;
  });

  // the knobs are read by the audio thread on every allocLayer, so they land
  //  through the event queue at a control-pass boundary like the master gain.
  auto policy   = VoiceStealPolicy(_SCD._voiceStealPolicy);
  int headroom  = _SCD._voiceHeadroom;
  syn->addEvent(0, [syn, policy, headroom]() {
    syn->_stealPolicy   = policy;
    syn->_voiceHeadroom = headroom;
  });

  return true;
}

void GlobalSynthSystem::_onDeactivate(Simulation* inst) {
  auto syn = synth::instance();
  for (auto& [busName, cfg] : _SCD._busConfigs) {
    auto bus = syn->outputBus(busName);
    if (bus) {
      syn->addEvent(0, [syn, bus]() {
        bus->_prog_gain = 0.0f;
        bus->_pan       = 0.0f;
        bus->_mute      = false;
        if (bus->_solo)
          syn->_num_soloed--;
        bus->_solo = false;
      });
      syn->setEffect(bus, "none");
    }
  }
  syn->addEvent(0, [syn]() {
    syn->_masterGain    = 1.0f;
    syn->_stealPolicy   = VoiceStealPolicy::PRIORITY;
    syn->_voiceHeadroom = 0;
  });
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
