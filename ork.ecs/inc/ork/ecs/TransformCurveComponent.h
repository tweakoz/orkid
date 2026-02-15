////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/math/transform_curve.h>

#include "component.h"
#include "system.h"
#include "entity.h"

namespace ork::ecs {

///////////////////////////////////////////////////////////////////////////////

struct TransformCurveComponentData : public ecs::ComponentData {
  DeclareConcreteX(TransformCurveComponentData, ecs::ComponentData);

public:
  TransformCurveComponentData();

  ecs::Component* createComponent(ecs::Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(ork::ecs::SceneComposer& sc) const final;

  math::transformcurve_ptr_t _curve;
  float _playbackSpeed = 1.0f;
  bool _looping = false;
};

using transformcurvecomponentdata_ptr_t = std::shared_ptr<TransformCurveComponentData>;

///////////////////////////////////////////////////////////////////////////////

struct TransformCurveSystemData : public ork::ecs::SystemData {
  DeclareConcreteX(TransformCurveSystemData, ork::ecs::SystemData);

public:
  TransformCurveSystemData();

private:
  ork::ecs::System* createSystem(ork::ecs::Simulation* pinst) const final;
};

using transformcurvesystemdata_ptr_t = std::shared_ptr<TransformCurveSystemData>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
