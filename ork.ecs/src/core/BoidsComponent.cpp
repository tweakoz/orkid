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
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/enum_serializer.inl>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>

#include "BoidsComponent_impl.h"

#include <random>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::BoidsComponentData, "BoidsComponentData");
ImplementReflectionX(ork::ecs::BoidsComponent, "BoidsComponent");
ImplementReflectionX(ork::ecs::BoidsSystemData, "BoidsSystemData");
ImplementReflectionX(ork::ecs::BoidsSystem, "BoidsSystem");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

BeginEnumRegistration(BoidsMode);
RegisterEnum(BoidsMode, AIR);
RegisterEnum(BoidsMode, LAND);
EndEnumRegistration();

} // namespace ork::ecs

ImplementEnumSerializer(ork::ecs::BoidsMode);

namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

static thread_local std::mt19937 _rng{std::random_device{}()};
static thread_local std::uniform_real_distribution<float> _rng_dist(-1.0f, 1.0f);

static fvec3 _randomUnitVector() {
  return fvec3(_rng_dist(_rng), _rng_dist(_rng), _rng_dist(_rng)).normalized();
}

//////////////////////////////////////////////////////////////////////////////
// BoidsComponentData
//////////////////////////////////////////////////////////////////////////////

void BoidsComponentData::describeX(ComponentDataClass* clazz) {
  InvokeEnumRegistration(BoidsMode);
  clazz->directProperty("FlockID", &BoidsComponentData::_flockID);
  clazz->directEnumProperty("Mode", &BoidsComponentData::_mode);

  clazz->floatProperty("SeparationWeight", float_range{0, 100}, &BoidsComponentData::_separationWeight);
  clazz->floatProperty("SeparationRadius", float_range{0.1, 1000}, &BoidsComponentData::_separationRadius);

  clazz->floatProperty("AlignmentWeight", float_range{0, 100}, &BoidsComponentData::_alignmentWeight);
  clazz->floatProperty("AlignmentRadius", float_range{0.1, 1000}, &BoidsComponentData::_alignmentRadius);

  clazz->floatProperty("CohesionWeight", float_range{0, 100}, &BoidsComponentData::_cohesionWeight);
  clazz->floatProperty("CohesionRadius", float_range{0.1, 1000}, &BoidsComponentData::_cohesionRadius);

  clazz->floatProperty("MaxForce", float_range{0, 10000}, &BoidsComponentData::_maxForce);
  clazz->floatProperty("MaxSpeed", float_range{0, 1000}, &BoidsComponentData::_maxSpeed);

  clazz->floatProperty("WanderStrength", float_range{0, 100}, &BoidsComponentData::_wanderStrength);
  clazz->floatProperty("GroundHeight", float_range{-10000, 10000}, &BoidsComponentData::_groundHeight);

  clazz->floatProperty("HomeWeight", float_range{0, 100}, &BoidsComponentData::_homeWeight);
  clazz->floatProperty("HomeRadius", float_range{0, 10000}, &BoidsComponentData::_homeRadius);
}

BoidsComponentData::BoidsComponentData() {
}

Component* BoidsComponentData::createComponent(ecs::Entity* pent) const {
  return new BoidsComponent(*this, pent);
}

object::ObjectClass* BoidsComponentData::componentClass() {
  return BoidsComponent::GetClassStatic();
}

void BoidsComponentData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::BoidsSystemData>();
  sc.Register<ork::ecs::BulletSystemData>();
}

//////////////////////////////////////////////////////////////////////////////
// BoidsComponent
//////////////////////////////////////////////////////////////////////////////

void BoidsComponent::describeX(object::ObjectClass* clazz) {
}

BoidsComponent::BoidsComponent(const BoidsComponentData& data, ork::ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , _CD(data) {
}

void BoidsComponent::_onUninitialize(Simulation* psi) {
}

bool BoidsComponent::_onLink(Simulation* psi) {
  _system = psi->findSystem<BoidsSystem>();
  return true;
}

void BoidsComponent::_onUnlink(Simulation* psi) {
}

bool BoidsComponent::_onStage(Simulation* psi) {
  _system->_onStageComponent(this);
  return true;
}

void BoidsComponent::_onUnstage(Simulation* psi) {
  _system->_onUnstageComponent(this);
}

bool BoidsComponent::_onActivate(Simulation* psi) {
  // find sibling BulletObjectComponent on the same entity
  _bulletComponent = GetEntity()->typedComponent<BulletObjectComponent>();
  _spawnPosition = GetEntity()->GetEntityPosition();
  _system->_onActivateComponent(this);
  return true;
}

void BoidsComponent::_onDeactivate(Simulation* psi) {
  _system->_onDeactivateComponent(this);
}

void BoidsComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
}

//////////////////////////////////////////////////////////////////////////////
// BoidsSystemData
//////////////////////////////////////////////////////////////////////////////

void BoidsSystemData::describeX(SystemDataClass* clazz) {
}

BoidsSystemData::BoidsSystemData() {
}

System* BoidsSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new BoidsSystem(*this, pinst);
}

//////////////////////////////////////////////////////////////////////////////
// BoidsSystem
//////////////////////////////////////////////////////////////////////////////

void BoidsSystem::describeX(object::ObjectClass* clazz) {
}

BoidsSystem::BoidsSystem(const BoidsSystemData& data, ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst) {
}

void BoidsSystem::_onStageComponent(BoidsComponent* component) {
}

void BoidsSystem::_onUnstageComponent(BoidsComponent* component) {
}

void BoidsSystem::_onActivateComponent(BoidsComponent* component) {
  _components.insert(component);
}

void BoidsSystem::_onDeactivateComponent(BoidsComponent* component) {
  _components.erase(component);
}

bool BoidsSystem::_onLink(Simulation* psi) {
  _bulletSystem = psi->findSystem<BulletSystem>();
  return true;
}

void BoidsSystem::_onUnLink(Simulation* psi) {
}

bool BoidsSystem::_onStage(Simulation* psi) {
  return true;
}

void BoidsSystem::_onUnstage(Simulation* inst) {
}

bool BoidsSystem::_onActivate(Simulation* psi) {
  return true;
}

void BoidsSystem::_onDeactivate(Simulation* inst) {
}

//////////////////////////////////////////////////////////////////////////////

void BoidsSystem::_onUpdate(Simulation* inst) {
  float dt = inst->deltaTime();
  if (dt <= 0.0f)
    return;
  _computeBoidsForces(inst, dt);
}

//////////////////////////////////////////////////////////////////////////////

void BoidsSystem::_computeBoidsForces(Simulation* inst, float dt) {

  // 1. build per-frame state cache
  _stateCache.clear();
  _flockIndices.clear();

  for (auto* comp : _components) {
    auto* bulletComp = comp->_bulletComponent;
    if (!bulletComp || !bulletComp->_rigidbody)
      continue;

    auto* rb = bulletComp->_rigidbody;

    btTransform xf;
    rb->getMotionState()->getWorldTransform(xf);

    BoidState state;
    state._component = comp;
    state._position = btv3toorkv3(xf.getOrigin());
    state._velocity = btv3toorkv3(rb->getLinearVelocity());
    state._spawnPosition = comp->_spawnPosition;
    state._rigidBody = rb;

    size_t idx = _stateCache.size();
    _stateCache.push_back(state);
    _flockIndices[comp->_CD._flockID].push_back(idx);
  }

  // 2. compute and apply boids forces per flock
  for (auto& [flockID, indices] : _flockIndices) {
    size_t count = indices.size();
    if (count < 2)
      continue;

    for (size_t ii = 0; ii < count; ii++) {
      auto& self = _stateCache[indices[ii]];
      const auto& cd = self._component->_CD;

      fvec3 separation(0, 0, 0);
      fvec3 alignment(0, 0, 0);
      fvec3 cohesion(0, 0, 0);
      int sepCount = 0;
      int aliCount = 0;
      int cohCount = 0;

      float sepRadSq = cd._separationRadius * cd._separationRadius;
      float aliRadSq = cd._alignmentRadius * cd._alignmentRadius;
      float cohRadSq = cd._cohesionRadius * cd._cohesionRadius;

      for (size_t jj = 0; jj < count; jj++) {
        if (ii == jj)
          continue;

        auto& other = _stateCache[indices[jj]];
        fvec3 delta = self._position - other._position;
        float distSq = delta.dotWith(delta);

        // separation: steer away from nearby flockmates
        if (distSq < sepRadSq && distSq > 0.0001f) {
          float dist = sqrtf(distSq);
          separation += delta * (1.0f / dist); // weight inversely by distance
          sepCount++;
        }

        // alignment: steer towards average heading
        if (distSq < aliRadSq) {
          alignment += other._velocity;
          aliCount++;
        }

        // cohesion: steer towards average position
        if (distSq < cohRadSq) {
          cohesion += other._position;
          cohCount++;
        }
      }

      fvec3 totalForce(0, 0, 0);

      // separation force
      if (sepCount > 0) {
        separation = separation * (1.0f / sepCount);
        float len = separation.magnitude();
        if (len > 0.0001f)
          separation = separation * (cd._maxSpeed / len); // desired velocity
        separation = separation - self._velocity;         // steering
        totalForce += separation * cd._separationWeight;
      }

      // alignment force
      if (aliCount > 0) {
        alignment = alignment * (1.0f / aliCount);
        float len = alignment.magnitude();
        if (len > 0.0001f)
          alignment = alignment * (cd._maxSpeed / len);
        alignment = alignment - self._velocity;
        totalForce += alignment * cd._alignmentWeight;
      }

      // cohesion force
      if (cohCount > 0) {
        cohesion = cohesion * (1.0f / cohCount);
        fvec3 desired = cohesion - self._position; // direction to center
        float len = desired.magnitude();
        if (len > 0.0001f)
          desired = desired * (cd._maxSpeed / len);
        fvec3 steer = desired - self._velocity;
        totalForce += steer * cd._cohesionWeight;
      }

      // wander: random jitter to prevent lockstep
      if (cd._wanderStrength > 0.0f) {
        totalForce += _randomUnitVector() * cd._wanderStrength;
      }

      // home: steer back towards spawn position when beyond homeRadius
      if (cd._homeWeight > 0.0f) {
        fvec3 toHome = self._spawnPosition - self._position;
        float dist = toHome.magnitude();
        if (dist > cd._homeRadius) {
          // force scales with how far beyond the radius we are
          float overshoot = (dist - cd._homeRadius) / cd._homeRadius;
          fvec3 desired = toHome * (cd._maxSpeed / dist);
          fvec3 steer = desired - self._velocity;
          totalForce += steer * cd._homeWeight * overshoot;
        }
      }

      // land mode: zero out vertical force, add ground-following
      if (cd._mode == BoidsMode::LAND) {
        totalForce.y = 0.0f;

        // ground constraint: push back towards ground height
        float heightDelta = cd._groundHeight - self._position.y;
        totalForce.y += heightDelta * 5.0f; // spring-like ground force
      }

      // clamp total force
      float forceMag = totalForce.magnitude();
      if (forceMag > cd._maxForce) {
        totalForce = totalForce * (cd._maxForce / forceMag);
      }

      // apply force to bullet rigid body
      self._rigidBody->activate();
      self._rigidBody->applyCentralForce(orkv3tobtv3(totalForce));

      // clamp velocity to max speed
      btVector3 vel = self._rigidBody->getLinearVelocity();
      float speed = vel.length();
      if (speed > cd._maxSpeed) {
        self._rigidBody->setLinearVelocity(vel * (cd._maxSpeed / speed));
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
