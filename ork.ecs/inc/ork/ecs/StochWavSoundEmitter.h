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
  float _probability       = 1.0f;
  float _postSilenceMin    = 0.5f;
  float _postSilenceMax    = 2.0f;
  float _pitchVarianceCents = 0.0f;
  float _gainMinDB         = 0.0f;
  float _gainMaxDB         = 0.0f;
  float _fadeInTime        = 0.0f;
  float _fadeOutTime       = 0.0f;
};
using stochwavsnd_ptr_t = std::shared_ptr<StochWavSound>;

///////////////////////////////////////////////////////////////////////////////

struct StochWavSoundEmitterData : public ecs::ComponentData {
  DeclareConcreteX(StochWavSoundEmitterData, ecs::ComponentData);

public:
  StochWavSoundEmitterData();

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  std::map<std::string, stochwavsnd_ptr_t> _sounds;
  audio::singularity::spatializerdata_ptr_t _spatializer;
  std::string _outputBusName = "main";
  float _masterGainDB        = 0.0f;
  int _maxVoices             = 4;
  bool _enabled              = true;
};
using stochwavemitterdata_ptr_t = std::shared_ptr<StochWavSoundEmitterData>;

///////////////////////////////////////////////////////////////////////////////

struct StochWavSoundEmitterSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(StochWavSoundEmitterSystemData, ork::ecs::SystemData);

public:
  StochWavSoundEmitterSystemData();

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};
using stochwavemittersysdata_ptr_t = std::shared_ptr<StochWavSoundEmitterSystemData>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
