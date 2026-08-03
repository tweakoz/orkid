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

#include "SoundFieldProbe_impl.h"
#include <ork/ecs/GlobalSynthSystem.h>
#include <ork/math/audiomath.h>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::SoundFieldProbeData, "SoundFieldProbeData");
ImplementReflectionX(ork::ecs::SoundFieldProbeComponent, "SoundFieldProbeComponent");
ImplementReflectionX(ork::ecs::SoundFieldSystemData, "SoundFieldSystemData");
ImplementReflectionX(ork::ecs::SoundFieldSystem, "SoundFieldSystem");

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

using namespace ork::audio::singularity;

///////////////////////////////////////////////////////////////////////////////
// SoundFieldProbeData
///////////////////////////////////////////////////////////////////////////////

void SoundFieldProbeData::describeX(ComponentDataClass* clazz) {
  clazz->directProperty("AmbixAsset", &SoundFieldProbeData::_ambixAsset)
      ->annotate("editor.filetype", "wav")
      ->annotate("editor.filebase", "<assetcache>");
  clazz->floatProperty("Gain", float_range{0, 4}, &SoundFieldProbeData::_gain);
  clazz->directProperty("Loop", &SoundFieldProbeData::_loop);
  clazz->directProperty("StartPaused", &SoundFieldProbeData::_startPaused);
  clazz->intProperty("Mode", int_range{0, 0}, &SoundFieldProbeData::_mode);
  clazz->floatProperty("RefDistance", float_range{0, 10000}, &SoundFieldProbeData::_refDistance);
  clazz->floatProperty("MaxDistance", float_range{0, 10000}, &SoundFieldProbeData::_maxDistance);
  clazz->floatProperty("Rolloff", float_range{0.01f, 8}, &SoundFieldProbeData::_rolloff);
}

SoundFieldProbeData::SoundFieldProbeData() {
}

Component* SoundFieldProbeData::createComponent(ecs::Entity* pent) const {
  return new SoundFieldProbeComponent(*this, pent);
}

object::ObjectClass* SoundFieldProbeData::componentClass() {
  return SoundFieldProbeComponent::GetClassStatic();
}

void SoundFieldProbeData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::SoundFieldSystemData>();
  sc.Register<ork::ecs::GlobalSynthSystemData>();
}

///////////////////////////////////////////////////////////////////////////////
// SoundFieldProbeComponent
///////////////////////////////////////////////////////////////////////////////

void SoundFieldProbeComponent::describeX(object::ObjectClass* clazz) {
}

SoundFieldProbeComponent::SoundFieldProbeComponent(
    const SoundFieldProbeData& data,
    ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , _CD(data) {
}

void SoundFieldProbeComponent::_onUninitialize(Simulation* psi) {
}

bool SoundFieldProbeComponent::_onLink(Simulation* psi) {
  _system = psi->findSystem<SoundFieldSystem>();
  // DoRegisterWithScene below WOULD auto-register the system, but it has no
  //  live call site - a scene that omits it used to SEGV in _onActivate.
  OrkAssertI(_system != nullptr, "SoundFieldProbeComponent: no SoundFieldSystem in this "
                                "simulation - declare SoundFieldSystemData in the scene");
  return true;
}

void SoundFieldProbeComponent::_onUnlink(Simulation* psi) {
}

bool SoundFieldProbeComponent::_onStage(Simulation* psi) {
  return true;
}

void SoundFieldProbeComponent::_onUnstage(Simulation* psi) {
}

bool SoundFieldProbeComponent::_onActivate(Simulation* psi) {
  _system->_onActivateComponent(this);
  return true;
}

void SoundFieldProbeComponent::_onDeactivate(Simulation* psi) {
  _system->_onDeactivateComponent(this);
}

void SoundFieldProbeComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
}

///////////////////////////////////////////////////////////////////////////////
// SoundFieldSystemData
///////////////////////////////////////////////////////////////////////////////

void SoundFieldSystemData::describeX(SystemDataClass* clazz) {
  clazz->floatProperty("MasterGainDB", float_range{-96, 24}, &SoundFieldSystemData::_masterGainDB);
  clazz->floatProperty("SlewTime", float_range{0, 10}, &SoundFieldSystemData::_slewTime);
}

SoundFieldSystemData::SoundFieldSystemData() {
}

System* SoundFieldSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new SoundFieldSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////
// SoundFieldSystem
///////////////////////////////////////////////////////////////////////////////

void SoundFieldSystem::describeX(object::ObjectClass* clazz) {
}

SoundFieldSystem::SoundFieldSystem(
    const SoundFieldSystemData& data,
    ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst)
    , _SCD(data) {
}

bool SoundFieldSystem::_onLink(Simulation* psi) {
  return true;
}

void SoundFieldSystem::_onUnLink(Simulation* psi) {
}

bool SoundFieldSystem::_onActivate(Simulation* psi) {
  // instancing the field is what creates the dedicated "soundfield" OutputBus
  //  and starts the probe feeder thread; both orders against GlobalSynthSystem's
  //  bus-config pass work, since each side is a get-or-create.
  _field              = SoundField::instance();
  _field->_masterGain = ork::audiomath::decibel_to_linear_amp_ratio(_SCD._masterGainDB);
  _field->_slewTau    = _SCD._slewTime;
  return true;
}

void SoundFieldSystem::_onDeactivate(Simulation* inst) {
  if (_field) {
    for (auto* comp : _components) {
      if (comp->_probeID >= 0) {
        _field->destroyProbe(comp->_probeID);
        comp->_probeID = -1;
      }
    }
  }
  _components.clear();
  _field = nullptr;
}

void SoundFieldSystem::_onActivateComponent(SoundFieldProbeComponent* component) {
  _components.insert(component);
}

void SoundFieldSystem::_onDeactivateComponent(SoundFieldProbeComponent* component) {
  if (_field and component->_probeID >= 0) {
    _field->destroyProbe(component->_probeID);
    component->_probeID = -1;
  }
  _components.erase(component);
}

///////////////////////////////////////////////////////////////////////////////
// update: push the authored probe params + entity positions into the field and
//  let it do the weight solve. the LISTENER is read straight off the synth's
//  _listener_matrix inside SoundField::update - this system owns no pose
//  plumbing (the HMD-aware listener helper feeds that same matrix).
///////////////////////////////////////////////////////////////////////////////

void SoundFieldSystem::_onUpdate(Simulation* inst) {
  if (not _field)
    return;

  float dt = inst->deltaTime();

  for (auto* comp : _components) {
    auto& cd = comp->_CD;

    if (cd._startPaused and comp->_probeID < 0)
      continue;

    if (comp->_probeID < 0) {
      // createProbe is fail-loud by design: a missing or non-4-channel asset
      //  throws here rather than becoming silence on the audio thread.
      comp->_probeID = _field->createProbe(cd._ambixAsset.toStdString(), cd._loop);
    }

    SoundFieldProbeParams params;
    params._position    = comp->GetEntity()->GetEntityPosition();
    params._refDistance = cd._refDistance;
    params._maxDistance = cd._maxDistance;
    params._rolloff     = cd._rolloff;
    params._gain        = cd._gain;
    _field->setProbeParams(comp->_probeID, params);
  }

  _field->update(dt);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
