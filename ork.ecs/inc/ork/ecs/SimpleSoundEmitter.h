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
// Sound definition — lives on SystemData, shared by all components referencing it
///////////////////////////////////////////////////////////////////////////////

struct SimpleSoundData : public ork::Object {
  DeclareConcreteX(SimpleSoundData, ork::Object);

public:
  SimpleSoundData() = default;

  file::Path _wavFilePath;
  bool _looping              = false;
  bool _spatialize           = true;
  float _gainDB              = 0.0f;
  float _pitchOffsetCents    = 0.0f;
  float _fadeInTime          = 0.01f;
  float _fadeOutTime         = 0.05f;
  std::string _outputBusName = "main";
};
using simplesnddata_ptr_t = std::shared_ptr<SimpleSoundData>;

///////////////////////////////////////////////////////////////////////////////
// ComponentData — lightweight, references a sound by name
///////////////////////////////////////////////////////////////////////////////

struct SimpleSoundEmitterData : public ecs::ComponentData {
  DeclareConcreteX(SimpleSoundEmitterData, ecs::ComponentData);

public:
  SimpleSoundEmitterData();

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  std::string _soundName;
  bool _autoPlay             = true;
  bool _enabled              = true;
  float _gainOffsetDB        = 0.0f;
  float _pitchOffsetCents    = 0.0f;
  float _initialFadeGainLinear = 1.0f;
  int _priority              = 0; // voice-steal priority: higher survives longer
};
using simplesoundemitterdata_ptr_t = std::shared_ptr<SimpleSoundEmitterData>;

///////////////////////////////////////////////////////////////////////////////
// SystemData — holds the sound library
///////////////////////////////////////////////////////////////////////////////

struct SimpleSoundEmitterSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(SimpleSoundEmitterSystemData, ork::ecs::SystemData);

public:
  SimpleSoundEmitterSystemData();

  std::map<std::string, simplesnddata_ptr_t> _sounds;
  audio::singularity::spatializerdata_ptr_t _spatializer;

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};
using simplesoundemittersysdata_ptr_t = std::shared_ptr<SimpleSoundEmitterSystemData>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
