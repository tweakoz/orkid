////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>

#include "component.h"
#include "system.h"
#include "entity.h"
#include <ork/lev2/aud/spatializer.h>
#include <ork/file/path.h>

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct StochWavSound : public ork::Object {
  DeclareConcreteX(StochWavSound, ork::Object);

public:
  StochWavSound() = default;

  file::Path _wavFilePath;
  float _burstRate          = 0.1f;   // Hz — avg one burst every 10s
  int   _burstCountMin      = 1;
  int   _burstCountMax      = 3;
  float _intraBurstRate     = 3.0f;   // Hz — chirps/sec within burst
  float _selectionWeight    = 1.0f;   // relative weight for sound selection
  float _pitchVarianceCents = 0.0f;
  float _gainMinDB          = 0.0f;
  float _gainMaxDB          = 0.0f;
  float _fadeInTime         = 0.0f;  // 0 = use group default
  float _fadeOutTime        = 0.0f;  // 0 = use group default
};
using stochwavsnd_ptr_t = std::shared_ptr<StochWavSound>;

///////////////////////////////////////////////////////////////////////////////

struct StochSoundGroup : public ork::Object {
  DeclareConcreteX(StochSoundGroup, ork::Object);

public:
  StochSoundGroup() = default;

  std::map<std::string, stochwavsnd_ptr_t> _sounds;
  std::string _outputBusName   = "main";
  float _masterGainDB          = 0.0f;
  int _maxVoicesPerGroup       = 5;
  float _minSpacing            = 0.3f;  // seconds between any two triggers in group
  float _fadeInTime            = 0.01f; // group-level default fade-in (seconds)
  float _fadeOutTime           = 0.05f; // group-level default fade-out (seconds)
};
using stochsoundgroup_ptr_t = std::shared_ptr<StochSoundGroup>;

///////////////////////////////////////////////////////////////////////////////

struct StochWavSoundEmitterData : public ecs::ComponentData {
  DeclareConcreteX(StochWavSoundEmitterData, ecs::ComponentData);

public:
  StochWavSoundEmitterData();

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  std::string _groupName;
  float _pitchOffsetCents  = 0.0f;
  float _gainOffsetDB      = 0.0f;
  float _rateScale         = 1.0f;
  bool _enabled            = true;
};
using stochwavemitterdata_ptr_t = std::shared_ptr<StochWavSoundEmitterData>;

///////////////////////////////////////////////////////////////////////////////

struct StochWavSoundEmitterSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(StochWavSoundEmitterSystemData, ork::ecs::SystemData);

public:
  StochWavSoundEmitterSystemData();

  std::map<std::string, stochsoundgroup_ptr_t> _soundGroups;
  audio::singularity::spatializerdata_ptr_t _spatializer;

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};
using stochwavemittersysdata_ptr_t = std::shared_ptr<StochWavSoundEmitterSystemData>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
