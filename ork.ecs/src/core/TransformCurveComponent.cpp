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

#include "TransformCurveComponent_impl.h"

ImplementReflectionX(ork::ecs::TransformCurveComponentData, "TransformCurveComponentData");
ImplementReflectionX(ork::ecs::TransformCurveComponent, "TransformCurveComponent");
ImplementReflectionX(ork::ecs::TransformCurveSystemData, "TransformCurveSystemData");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ecs {
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;

void TransformCurveComponentData::describeX(ComponentDataClass* clazz) {
  clazz->floatProperty("PlaybackSpeed", float_range{-10, 10}, &TransformCurveComponentData::_playbackSpeed);
  clazz->directProperty("Looping", &TransformCurveComponentData::_looping);
  clazz->directObjectProperty("Curve", &TransformCurveComponentData::_curve)
      ->annotate<ConstString>("editor.custom", "transformcurveeditor");
}

///////////////////////////////////////////////////////////////////////////////

TransformCurveComponentData::TransformCurveComponentData() {
  _curve = std::make_shared<math::TransformCurve>();
}

///////////////////////////////////////////////////////////////////////////////

Component* TransformCurveComponentData::createComponent(ecs::Entity* pent) const {
  return new TransformCurveComponent(*this, pent);
}

object::ObjectClass* TransformCurveComponentData::componentClass() {
  return TransformCurveComponent::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveComponentData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::TransformCurveSystemData>();
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveComponent::describeX(ObjectClass* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

TransformCurveComponent::TransformCurveComponent(const TransformCurveComponentData& data, ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , _CD(data) {
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveComponent::_onUninitialize(Simulation* psi) {
}

bool TransformCurveComponent::_onLink(Simulation* psi) {
  _system = psi->findSystem<TransformCurveSystem>();
  return true;
}

void TransformCurveComponent::_onUnlink(Simulation* psi) {
}

bool TransformCurveComponent::_onStage(Simulation* psi) {
  _system->_onStageComponent(this);
  return true;
}

void TransformCurveComponent::_onUnstage(Simulation* psi) {
  _system->_onUnstageComponent(this);
}

bool TransformCurveComponent::_onActivate(Simulation* psi) {
  _system->_onActivateComponent(this);
  return true;
}

void TransformCurveComponent::_onDeactivate(Simulation* psi) {
  _system->_onDeactivateComponent(this);
}

void TransformCurveComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
  switch (evID._hashed) {
    case "SETTIME"_crcu:
      _curveTime = data.get<float>();
      break;
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void TransformCurveSystemData::describeX(SystemDataClass* clazz) {
}

TransformCurveSystemData::TransformCurveSystemData() {
}

System* TransformCurveSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new TransformCurveSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

TransformCurveSystem::TransformCurveSystem(const TransformCurveSystemData& data, ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst) {
}

void TransformCurveSystem::_onStageComponent(TransformCurveComponent* component) {
}

void TransformCurveSystem::_onUnstageComponent(TransformCurveComponent* component) {
}

void TransformCurveSystem::_onActivateComponent(TransformCurveComponent* component) {
  _components.insert(component);
}

void TransformCurveSystem::_onDeactivateComponent(TransformCurveComponent* component) {
  auto it = _components.find(component);
  if (it != _components.end()) {
    _components.erase(it);
  }
}

bool TransformCurveSystem::_onLink(Simulation* psi) {
  return true;
}

void TransformCurveSystem::_onUnLink(Simulation* psi) {
}

void TransformCurveSystem::_onUpdate(Simulation* inst) {
  float dt = inst->deltaTime();
  for (auto c : _components) {
    auto curve = c->_CD._curve;
    if (!curve || curve->numPoints() < 2)
      continue;

    c->_curveTime += dt * c->_CD._playbackSpeed;

    float maxTime = curve->getPoint(curve->numPoints() - 1)->_time;
    float minTime = curve->getPoint(0)->_time;

    if (c->_CD._looping) {
      float range = maxTime - minTime;
      if (range > 0.0f) {
        while (c->_curveTime > maxTime)
          c->_curveTime -= range;
        while (c->_curveTime < minTime)
          c->_curveTime += range;
      }
    } else {
      c->_curveTime = std::clamp(c->_curveTime, minTime, maxTime);
    }

    auto sample = curve->sample(c->_curveTime);
    auto e = c->GetEntity();
    auto xf = e->transform();
    xf->_translation = sample._position;
    xf->_rotation = sample._rotation;
    if (curve->_useNonUniformScale) {
      xf->_useNonUniformScale = true;
      xf->_nonUniformScale = sample._scale;
    } else {
      xf->_useNonUniformScale = false;
      xf->_uniformScale = sample._scale.x;
    }
  }
}

bool TransformCurveSystem::_onStage(Simulation* psi) {
  return true;
}

void TransformCurveSystem::_onUnstage(Simulation* inst) {
}

bool TransformCurveSystem::_onActivate(Simulation* psi) {
  return true;
}

void TransformCurveSystem::_onDeactivate(Simulation* inst) {
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ecs
