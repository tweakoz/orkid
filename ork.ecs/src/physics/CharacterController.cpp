////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// CharacterController — see CharacterController.h. The walk-on-terrain behavior as a
// reusable data-declared component: input arrives as controller messages; the system
// drives the sibling bullet capsule's declared DirectionalForce and publishes the camera.
//
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/util/logger.h>
#include <ork/ecs/physics/CharacterController.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/datatable.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>
#include "bullet_impl.h"
#include "../core/message_private.h"

ImplementReflectionX(ork::ecs::CharacterControllerComponentData, "EcsCharacterControllerComponentData");
ImplementReflectionX(ork::ecs::CharacterControllerComponent, "EcsCharacterControllerComponent");
ImplementReflectionX(ork::ecs::CharacterControllerSystemData, "EcsCharacterControllerSystemData");
ImplementReflectionX(ork::ecs::CharacterControllerSystem, "EcsCharacterControllerSystem");

namespace ork::ecs {

static logchannel_ptr_t logchan_charctl = logger()->configureChannel("ecs.charctl", fvec3(0.3, 0.9, 0.6),false);

///////////////////////////////////////////////////////////////////////////////

void CharacterControllerComponentData::describeX(ComponentDataClass* clazz) {
  clazz->directProperty("MoveForce", &CharacterControllerComponentData::_moveForce);
  clazz->directProperty("MaxSpeed", &CharacterControllerComponentData::_maxSpeed);
  clazz->directProperty("JumpImpulse", &CharacterControllerComponentData::_jumpImpulse);
  clazz->directProperty("TurnRate", &CharacterControllerComponentData::_turnRate);
  clazz->directProperty("Brake", &CharacterControllerComponentData::_brake);
  clazz->directProperty("TurnDecay", &CharacterControllerComponentData::_turnDecay);
  clazz->directProperty("DriveFriction", &CharacterControllerComponentData::_driveFriction);
  clazz->directProperty("RestFriction", &CharacterControllerComponentData::_restFriction);
  clazz->directProperty("EyeHeight", &CharacterControllerComponentData::_eyeHeight);
  clazz->directProperty("CamDistance", &CharacterControllerComponentData::_camDistance);
  clazz->directProperty("FovyDeg", &CharacterControllerComponentData::_fovyDeg);
  clazz->directProperty("CamNear", &CharacterControllerComponentData::_camNear);
  clazz->directProperty("CamFar", &CharacterControllerComponentData::_camFar);
  clazz->directProperty("ForceName", &CharacterControllerComponentData::_forceName);
}

CharacterControllerComponentData::CharacterControllerComponentData() {
}

Component* CharacterControllerComponentData::createComponent(ecs::Entity* pent) const {
  return new CharacterControllerComponent(*this, pent);
}

void CharacterControllerComponentData::DoRegisterWithScene(ecs::SceneComposer& sc) const {
  sc.Register<CharacterControllerSystemData>();
}

object::ObjectClass* CharacterControllerComponentData::componentClass() {
  return CharacterControllerComponent::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

void CharacterControllerComponent::describeX(object::ObjectClass* clazz) {
}

CharacterControllerComponent::CharacterControllerComponent(const CharacterControllerComponentData& cd, ecs::Entity* pent)
    : Component(&cd, pent)
    , _CCD(cd) {
}

bool CharacterControllerComponent::_onLink(Simulation* psi) {
  _system = psi->findSystem<CharacterControllerSystem>();
  return true;
}

bool CharacterControllerComponent::_onStage(Simulation* psi) {
  if (!_system) {
    logchan_charctl->log(
        "CharacterControllerComponent::_onStage: NO CharacterControllerSystem in this scene — "
        "declare it (self.system_data(\"CharacterControllerSystem\")); component inert");
    return true;
  }
  _system->_onStageComponent(this);
  return true;
}

void CharacterControllerComponent::_onUnstage(Simulation* psi) {
  if (_system)
    _system->_onUnstageComponent(this);
}

///////////////////////////////////////////////////////////////////////////////

void CharacterControllerSystemData::describeX(SystemDataClass* clazz) {
}

CharacterControllerSystemData::CharacterControllerSystemData() {
}

System* CharacterControllerSystemData::createSystem(ecs::Simulation* psi) const {
  return new CharacterControllerSystem(*this, psi);
}

///////////////////////////////////////////////////////////////////////////////

void CharacterControllerSystem::describeX(object::ObjectClass* clazz) {
}

CharacterControllerSystem::CharacterControllerSystem(const CharacterControllerSystemData& data, Simulation* psi)
    : System(&data, psi)
    , _CCSD(data) {
}

bool CharacterControllerSystem::_onLink(Simulation* psi) {
  _sgsys     = psi->findSystem<SceneGraphSystem>();
  _bulletsys = psi->findSystem<BulletSystem>();
  return true;
}

void CharacterControllerSystem::_onStageComponent(CharacterControllerComponent* c) {
  _components.insert(c);
}
void CharacterControllerSystem::_onUnstageComponent(CharacterControllerComponent* c) {
  _components.erase(c);
}

///////////////////////////////////////////////////////////////////////////////
// input messages — host-agnostic: every host forwards its key events the same way
// (controller->systemNotify(sysref, InputKey, {key, down})), delivered here on the
// update thread through the standard event queue.
///////////////////////////////////////////////////////////////////////////////

void CharacterControllerSystem::_onNotify(token_t evID, evdata_t data) {
  switch (evID.hashed()) {
    case MoveInput._hashed: {
      const auto& table = *data.getShared<DataTable>();
      _moveX = table["x"_tok].get<float>();
      _moveZ = table["z"_tok].get<float>();
      if (_dbgNotifies < 8) { // first few: prove the script->semantic hop
        _dbgNotifies++;
        logchan_charctl->log("semantic MoveInput x<%g> z<%g>", _moveX, _moveZ);
      }
      break;
    }
    case TurnInput._hashed: {
      const auto& table = *data.getShared<DataTable>();
      _turn = table["rate"_tok].get<float>();
      break;
    }
    case PitchInput._hashed: {
      const auto& table = *data.getShared<DataTable>();
      _pitchRate = table["rate"_tok].get<float>();
      break;
    }
    case Jump._hashed: {
      _jumpTTL = 0.25f; // BUFFERED request: jump fires on the next near-ground frame
      break;            // within the window (a strict same-frame gate drops most presses)
    }
    case SetParams._hashed: { // runtime tuning from the input script (any subset of keys)
      const auto& table = *data.getShared<DataTable>();
      auto ovr = [&table](const char* key, float& slot) {
        DataKey k;
        k._encoded.set<CrcString>(CrcString(key));
        auto v = table.find(k);
        if (v.valid())
          slot = v._encoded.get<float>();
      };
      ovr("move_force", _ovrMoveForce);
      ovr("max_speed", _ovrMaxSpeed);
      ovr("brake", _ovrBrake);
      ovr("turn_rate", _ovrTurnRate);
      ovr("jump_impulse", _ovrJumpImpulse);
      ovr("turn_decay", _ovrTurnDecay);
      ovr("drive_friction", _ovrDriveFriction);
      ovr("rest_friction", _ovrRestFriction);
      logchan_charctl->log(
          "SetParams force<%g> maxspd<%g> brake<%g> turn<%g> jump<%g> tdecay<%g> dfric<%g> rfric<%g> (-1 = data default)",
          _ovrMoveForce, _ovrMaxSpeed, _ovrBrake, _ovrTurnRate, _ovrJumpImpulse,
          _ovrTurnDecay, _ovrDriveFriction, _ovrRestFriction);
      break;
    }
    default:
      System::_onNotify(evID, data);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// synchronous queries — the system SCRIPT runs on the update thread, so
// system.request() lands here as a direct call (no controller round-trip).
///////////////////////////////////////////////////////////////////////////////

void CharacterControllerSystem::_onRequest(impl::sys_response_ptr_t response, token_t evID, evdata_t data) {
  switch (evID.hashed()) {
    case CameraRay._hashed: { // the current camera ray (stashed by the camera publish)
      auto table          = std::make_shared<DataTable>();
      (*table)["pos"_tok] = _lastEye;
      (*table)["dir"_tok] = _lastLook;
      response->_responseData.set<datatable_ptr_t>(table);
      break;
    }
    default:
      System::_onRequest(response, evID, data);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void CharacterControllerSystem::_onUpdate(Simulation* psi) {
  const float dt = psi->deltaTime();

  for (auto c : _components) {
    // resolve the sibling bullet capsule lazily (its rigid body exists after activation)
    if (!c->_bullet)
      c->_bullet = c->GetEntity()->typedComponent<BulletObjectComponent>(true);
    if (!c->_bullet || !c->_bullet->_rigidbody)
      continue;
    auto body = c->_bullet->_rigidbody;

    // EFFECTIVE tuning: script overrides (SetParams) win over the reflected data —
    // the input script owns the feel without recompiles or re-serializes.
    const float eff_force = (_ovrMoveForce >= 0.0f) ? _ovrMoveForce : c->_CCD._moveForce;
    const float eff_maxspd = (_ovrMaxSpeed >= 0.0f) ? _ovrMaxSpeed : c->_CCD._maxSpeed;
    const float eff_brake = (_ovrBrake >= 0.0f) ? _ovrBrake : c->_CCD._brake;
    const float eff_turn = (_ovrTurnRate >= 0.0f) ? _ovrTurnRate : c->_CCD._turnRate;
    const float eff_jump = (_ovrJumpImpulse >= 0.0f) ? _ovrJumpImpulse : c->_CCD._jumpImpulse;
    const float eff_tdecay = (_ovrTurnDecay >= 0.0f) ? _ovrTurnDecay : c->_CCD._turnDecay;
    const float eff_dfric = (_ovrDriveFriction >= 0.0f) ? _ovrDriveFriction : c->_CCD._driveFriction;
    const float eff_rfric = (_ovrRestFriction >= 0.0f) ? _ovrRestFriction : c->_CCD._restFriction;

    // heading yaw + camera pitch from the SEMANTIC action state (the scene's input
    // script owns the keymap). The capsule itself is rotation-locked (angularFactor);
    // heading/pitch are controller state, like the FPS example's playerforce_rot.
    // ANGULAR INERTIA: instant attack while the key is held; exponential tail on
    // release (TurnDecay 1/s -> ~3/decay seconds to coast to a stop).
    if (fabsf(_turn) > 0.001f)
      _turnVel = _turn;
    else
      _turnVel *= expf(-eff_tdecay * dt);
    if (fabsf(_pitchRate) > 0.001f)
      _pitchVel = _pitchRate;
    else
      _pitchVel *= expf(-eff_tdecay * dt);
    c->_heading += _turnVel * eff_turn * dt;
    c->_pitch   += _pitchVel * eff_turn * 0.6f * dt;
    c->_pitch = std::min(std::max(c->_pitch, -1.2f), 1.2f);
    const fvec3 fwd(sinf(c->_heading), 0.0f, -cosf(c->_heading));
    const fvec3 right(cosf(c->_heading), 0.0f, sinf(c->_heading)); // fwd x up

    // drive the declared walk force (mutating the force DATA is the established
    // live-poke channel — the inst reads it every physics step): z along the
    // heading, x strafe; diagonals normalized so they don't outrun max force.
    fvec3 drive = fwd * _moveZ + right * _moveX;
    float dmag  = drive.magnitude();
    if (dmag > 1.0f)
      drive = drive * (1.0f / dmag), dmag = 1.0f;
    auto& fmap = c->_bullet->data()._forcedatas;
    auto itf   = fmap.find(c->_CCD._forceName);
    if (itf != fmap.end()) {
      if (auto df = std::dynamic_pointer_cast<DirectionalForceData>(itf->second)) {
        df->_direction = (dmag > 0.001f) ? (drive * (1.0f / dmag)) : fwd;
        df->_force     = dmag * eff_force;
      }
    }
    body->activate(true); // keep the capsule awake while controlled

    // ASYMMETRIC CONTACT FRICTION: ~0 while driving (zero-drag feel — with the
    // static colliders now carrying friction 1.0 the capsule's own value IS the
    // effective contact friction), HIGH at rest so the character sits still on
    // hills (the exp brake alone leaves a creep of g·sinθ/brake).
    const float want_fric = (dmag > 0.01f) ? eff_dfric : eff_rfric;
    if (want_fric != c->_appliedFriction) {
      c->_appliedFriction = want_fric;
      body->setFriction(want_fric);
      // bullet caches combinedFriction PER CONTACT POINT at point creation; a
      // RESTING contact's points persist indefinitely, so setFriction alone
      // never reaches them (the walker kept creeping at g·sinθ/brake). Rewrite
      // the cached values in place — the manifold-scan idiom, switch-only cost.
      if (_bulletsys) {
        if (auto world = _bulletsys->BulletWorld()) {
          auto dispatcher = world->getDispatcher();
          const int nman  = dispatcher->getNumManifolds();
          for (int i = 0; i < nman; i++) {
            auto manifold = dispatcher->getManifoldByIndexInternal(i);
            auto a        = manifold->getBody0();
            auto b        = manifold->getBody1();
            if (a != body and b != body)
              continue;
            const float combined = a->getFriction() * b->getFriction();
            for (int p = 0; p < manifold->getNumContacts(); p++)
              manifold->getContactPoint(p).m_combinedFriction = combined;
          }
        }
      }
    }

    // horizontal speed clamp + ASYMMETRIC drag: while DRIVING the controller adds no
    // brake (full force goes to motion); on RELEASE the horizontal velocity decays
    // hard (exp at `Brake` 1/s -> stop in ~3/Brake seconds), vertical untouched.
    btVector3 vel = body->getLinearVelocity();
    fvec3 hvel(vel.x(), 0.0f, vel.z());
    float hspeed = hvel.magnitude();
    if (dmag < 0.01f and hspeed > 0.001f) {
      const float decay = expf(-eff_brake * dt);
      hvel = hvel * decay;
      body->setLinearVelocity(btVector3(hvel.x, vel.y(), hvel.z));
    } else if (hspeed > eff_maxspd) {
      hvel = hvel * (eff_maxspd / hspeed);
      body->setLinearVelocity(btVector3(hvel.x, vel.y(), hvel.z));
    }

    // jump: buffered request + near-rest vertical velocity (cheap groundedness; the
    // threshold tolerates slope-walking jitter). Consumed on fire.
    if (_jumpTTL > 0.0f and eff_jump > 0.0f and fabsf(vel.y()) < 0.75f) {
      body->applyCentralImpulse(btVector3(0.0f, eff_jump, 0.0f));
      _jumpTTL = 0.0f;
    }

    // probe telemetry: rate-limited live pose (gates + field diagnosis read this)
    _logAccum += dt;
    if (_logAccum > 2.0f) {
      _logAccum  = 0.0f;
      auto xfl   = c->GetEntity()->transform();
      auto& P    = xfl->_translation;
      logchan_charctl->log("charctl pos<%.3f %.3f %.3f> heading<%.3f>", P.x, P.y, P.z, c->_heading);
    }

    // publish the camera from the live entity transform (the renderer reads the
    // PUBLISHED camera — same UpdateCamera message every host uses)
    if (_sgsys) {
      auto xf         = c->GetEntity()->transform();
      const fvec3 pos = xf->_translation;
      const fvec3 up(0, 1, 0);
      const float cp = cosf(c->_pitch), sp = sinf(c->_pitch);
      fvec3 eye, tgt;
      if (c->_CCD._camDistance > 0.01f) { // follow: pitch orbits the camera about the character
        eye = pos - fwd * (c->_CCD._camDistance * cp) + up * (c->_CCD._eyeHeight + c->_CCD._camDistance * sp);
        tgt = pos + up * c->_CCD._eyeHeight;
      } else { // first person: pitch tilts the view
        eye = pos + up * c->_CCD._eyeHeight;
        tgt = eye + fwd * cp + up * sp;
      }
      _lastEye  = eye; // stashed for Shoot (the script asks for the current ray)
      _lastLook = (fwd * cp + up * sp).normalized();
      auto table       = std::make_shared<DataTable>();
      (*table)["eye"_tok]  = eye;
      (*table)["tgt"_tok]  = tgt;
      (*table)["up"_tok]   = up;
      (*table)["near"_tok] = c->_CCD._camNear;
      (*table)["far"_tok]  = c->_CCD._camFar;
      (*table)["fovy"_tok] = float(c->_CCD._fovyDeg * (3.14159265f / 180.0f));
      evdata_t ev;
      ev.setShared<DataTable>(table);
      _sgsys->_onNotify(SceneGraphSystem::UpdateCamera._token, ev); // compile-time (this runs per tick)
    }
  }
  _jumpTTL = std::max(0.0f, _jumpTTL - dt); // the buffered request decays
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
