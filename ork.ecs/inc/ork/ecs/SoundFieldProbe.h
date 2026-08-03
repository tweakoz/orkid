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
#include <ork/file/path.h>

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////
// SoundFieldProbe: a streamed 4-channel AmbiX loop anchored in the world,
//  scaled by a listener-position-derived weight and summed into the engine's
//  one B-format mix point (audio::singularity::SoundField).
//
//  RADIAL is the only mode this slice: the probe's position is its ENTITY's
//  position, weight is a smoothstep falloff between refDistance and
//  maxDistance. ZONE (box/sphere volumes, priority tiers, ducking) is SF3.
///////////////////////////////////////////////////////////////////////////////

enum class SoundFieldProbeMode : int {
  RADIAL = 0,
};

struct SoundFieldProbeData : public ecs::ComponentData {
  DeclareConcreteX(SoundFieldProbeData, ecs::ComponentData);

public:
  SoundFieldProbeData();

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  file::Path _ambixAsset;
  float _gain        = 1.0f;
  bool _loop         = true;
  bool _startPaused  = false;
  int _mode          = int(SoundFieldProbeMode::RADIAL);
  float _refDistance = 5.0f;  // full weight at or inside this radius
  float _maxDistance = 50.0f; // silent at or beyond it
  float _rolloff     = 1.0f;  // exponent on the smoothstep falloff
};
using soundfieldprobedata_ptr_t = std::shared_ptr<SoundFieldProbeData>;

///////////////////////////////////////////////////////////////////////////////

struct SoundFieldSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(SoundFieldSystemData, ork::ecs::SystemData);

public:
  SoundFieldSystemData();

  float _masterGainDB = 0.0f;
  float _slewTime     = 0.25f; // weight hysteresis, seconds

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};
using soundfieldsysdata_ptr_t = std::shared_ptr<SoundFieldSystemData>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
