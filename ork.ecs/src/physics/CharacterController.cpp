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
#include <ork/lev2/vr/vr.h> // VR view-direction locomotion: raw HMD gaze -> move basis
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
  clazz->directProperty("KillZDrop", &CharacterControllerComponentData::_killZDrop);
  clazz->directProperty("SpawnAboveGround", &CharacterControllerComponentData::_spawnAboveGround);
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
    case MoveBasisYaw._hashed: { // VR: override the MOVE basis with a world yaw (gaze + playspace)
      const auto& table = *data.getShared<DataTable>();
      _moveBasisYaw   = table["yaw"_tok].get<float>();
      _moveBasisValid = true;
      break;
    }
    case SetAimDir._hashed: { // VR: override the look dir CameraRay returns (gaze + lob pitch)
      const auto& table = *data.getShared<DataTable>();
      _aimLook = fvec3(table["dx"_tok].get<float>(),
                       table["dy"_tok].get<float>(),
                       table["dz"_tok].get<float>()).normalized();
      _aimValid = true;
      break;
    }
    case SetSprint._hashed: { // transient boost on move_force + max_speed (e.g. shift)
      const auto& table = *data.getShared<DataTable>();
      _sprintScale = table["scale"_tok].get<float>();
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
    case TurnStep._hashed: { // DISCRETE yaw step (positional camera-yaw controls) — no rate integration
      const auto& table = *data.getShared<DataTable>();
      const float dyaw  = table["radians"_tok].get<float>();
      for (auto c : _components) // update-thread owned, same thread as this notify
        c->_heading += dyaw;
      break;
    }
    case PitchStep._hashed: { // DISCRETE pitch step — same ±1.2 clamp the PitchInput rate honors
      const auto& table  = *data.getShared<DataTable>();
      const float dpitch = table["radians"_tok].get<float>();
      for (auto c : _components) {
        c->_pitch += dpitch;
        c->_pitch = std::min(std::max(c->_pitch, -1.2f), 1.2f);
      }
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
      (*table)["dir"_tok] = _aimValid ? _aimLook : _lastLook; // VR: gaze+lob override (SetAimDir)
      response->_responseData.set<datatable_ptr_t>(table);
      break;
    }
    default:
      System::_onRequest(response, evID, data);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////
// hard teleport: absolute placement + zero all motion (kill-Z respawn + spawn
// ground-snap share it — bullet needs the interpolation state cleared too or
// the render transform springs back)
///////////////////////////////////////////////////////////////////////////////

static void _teleportBody(btRigidBody* body, const fvec3& pos) {
  btTransform xf = body->getWorldTransform();
  xf.setOrigin(btVector3(pos.x, pos.y, pos.z));
  body->setWorldTransform(xf);
  body->setInterpolationWorldTransform(xf);
  body->setLinearVelocity(btVector3(0, 0, 0));
  body->setAngularVelocity(btVector3(0, 0, 0));
  body->setInterpolationLinearVelocity(btVector3(0, 0, 0));
  body->setInterpolationAngularVelocity(btVector3(0, 0, 0));
  body->clearForces();
  body->activate(true);
  if (auto ms = body->getMotionState())
    ms->setWorldTransform(xf); // sync the live entity transform (the camera reads it)
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

    ///////////////////////////////////////////////////////////////////////////
    // SELF-DEFENSE: a character that falls out of the world must NAME itself and
    // recover — never free-fall forever (that silent footgun read as an input bug
    // for hours). KILL-Z teleports the body home; the ground-contact edge logs +
    // the 5s no-contact WARN make a missing/mismatched terrain collider self-evident.
    ///////////////////////////////////////////////////////////////////////////
    const btVector3 bpos = body->getWorldTransform().getOrigin();
    if (!c->_spawnValid) { // first resolve ≈ spawn (the capsule is born at the entity xf)
      c->_spawnPos   = fvec3(bpos.x(), bpos.y(), bpos.z());
      c->_spawnValid = true;
    }
    c->_spawnAge        += dt;
    c->_contactLogAccum += dt;
    c->_killZLogAccum   += dt;

    // SPAWN GROUND-SNAP (owner-directed): when a terrain floor exists, the authored spawn Y
    // is ADVISORY — raycast straight DOWN at spawn XZ against the STATIC world and place the
    // capsule feet SpawnAboveGround metres above the hit. One mechanism kills BOTH observed
    // polarity failures: an underground spawn (scn_eflow — never contacts) and a sky spawn
    // (scn_xxx3 — terminal velocity TUNNELS through the heightfield triangles; a 5m drop
    // lands at ~10 m/s, well inside contact resolution). Retries until the terrain collider
    // has registered (shape creation publishes the span), gives up loudly after 2s.
    if (not c->_spawnSnapDone) {
      if (c->_CCD._spawnAboveGround <= 0.0f) {
        c->_spawnSnapDone = true; // disabled: authored spawn is exact
      } else if (_bulletsys and _bulletsys->_hasTerrainFloor) {
        if (auto world = _bulletsys->BulletWorld()) {
          const btVector3 from(c->_spawnPos.x, _bulletsys->_terrainMaxY + 100.0f, c->_spawnPos.z);
          const btVector3 to(c->_spawnPos.x, _bulletsys->_terrainMinY - 100.0f, c->_spawnPos.z);
          // statics only: ignore dynamics (including this very capsule sitting in the ray)
          struct StaticClosest : public btCollisionWorld::ClosestRayResultCallback {
            using ClosestRayResultCallback::ClosestRayResultCallback;
            btScalar addSingleResult(btCollisionWorld::LocalRayResult& r, bool nws) override {
              if (not r.m_collisionObject->isStaticObject())
                return 1.0f; // skip, keep scanning the full ray
              return ClosestRayResultCallback::addSingleResult(r, nws);
            }
          } rcb(from, to);
          world->rayTest(from, to, rcb);
          if (rcb.hasHit()) {
            // feet = capsule bottom: center rides half the shape's own AABB height above the hit
            btVector3 smin, smax;
            body->getCollisionShape()->getAabb(btTransform::getIdentity(), smin, smax);
            const float half_h  = 0.5f * (smax.getY() - smin.getY());
            const float groundY = rcb.m_hitPointWorld.getY();
            const fvec3 snapped(c->_spawnPos.x, groundY + c->_CCD._spawnAboveGround + half_h, c->_spawnPos.z);
            _teleportBody(body, snapped);
            logchan_charctl->log(
                "[walker] spawn ground-snap: static ground y=%.1f at spawn XZ — spawned %.1fm above "
                "(authored y=%.1f -> y=%.1f)",
                groundY, c->_CCD._spawnAboveGround, c->_spawnPos.y, snapped.y);
            c->_spawnPos      = snapped; // kill-Z respawns land on the snap, not the authored guess
            c->_spawnSnapDone = true;
          } else if (c->_spawnAge >= 2.0f) {
            c->_spawnSnapDone = true; // give up LOUDLY: terrain floor registered but not under here
            logchan_charctl->warn(
                "[walker] WARN: spawn ground-snap found NO static ground under spawn XZ (%.1f, %.1f) — "
                "keeping authored spawn y=%.1f",
                c->_spawnPos.x, c->_spawnPos.z, c->_spawnPos.y);
          }
        }
      } else if (c->_spawnAge >= 2.0f) {
        c->_spawnSnapDone = true; // no terrain floor in this scene (flat-plane demos etc) — authored spawn rules
      }
    }

    // UNDERGROUND SPAWN (proven cause of the scn_forest fall-through): the terrain collider
    // publishes its world-Y surface span at load; a spawn BELOW the terrain minimum can never
    // contact the heightfield — name it once, and route the recovery ABOVE the terrain max so
    // the respawn lands on walkable ground instead of refalling forever. (Also latches the
    // generic 5s no-contact WARN — this names the same failure more precisely.) The ground-snap
    // normally repairs this before it can fire — reaching here means snap gave up or is disabled.
    const bool underground = _bulletsys and _bulletsys->_hasTerrainFloor and
                             c->_spawnPos.y <= _bulletsys->_terrainMinY;
    if (underground and c->_spawnSnapDone and not c->_warnedUnderground) {
      c->_warnedUnderground = true;
      c->_warnedNoContact   = true;
      logchan_charctl->warn(
          "[walker] WARN: spawn y=%.1f is BELOW the terrain's minimum height %.1f — spawning "
          "underground, ground contact impossible (fix the scene spawn; terrain manifest has min/max)",
          c->_spawnPos.y, _bulletsys->_terrainMinY);
    }

    // KILL-Z (O(1) every frame): fell more than KillZDrop below spawn -> teleport
    // home, zero all velocity, loud log (throttled 1/2s so repeated respawns stay
    // visible without spamming — a REPEATED respawn is itself the proof the collider
    // is genuinely absent). Underground spawn -> recover ABOVE the terrain max (margin)
    // so the session self-recovers to walkable ground rather than refalling to the same hole.
    const float kill_drop = c->_CCD._killZDrop;
    const float drop      = c->_spawnPos.y - bpos.y();
    if (kill_drop > 0.0f && drop > kill_drop) {
      fvec3 home = c->_spawnPos;
      if (underground)
        home.y = _bulletsys->_terrainMaxY + 2.0f; // 2m clearance above the highest terrain
      _teleportBody(body, home);
      if (c->_killZLogAccum >= 2.0f) {
        c->_killZLogAccum = 0.0f;
        logchan_charctl->warn(
            "[walker] KILL-Z: fell %.0fm below spawn — respawned at (%.1f,%.1f,%.1f)",
            drop, home.x, home.y, home.z);
      }
    }

    // GROUND CONTACT (~10Hz — edges don't need per-frame resolution): the REAL
    // "am I touching the world" signal via the manifold set (the same scan the
    // friction refresh uses; the jump gate only approximates it with vel.y).
    // Edge -> one-shot log (rate-limited 1/s); never-acquired-within-5s -> one WARN.
    c->_contactScanAccum += dt;
    if (c->_contactScanAccum >= 0.1f) {
      c->_contactScanAccum = 0.0f;
      bool contact = false;
      if (_bulletsys) {
        if (auto world = _bulletsys->BulletWorld()) {
          auto dispatcher = world->getDispatcher();
          const int nman  = dispatcher->getNumManifolds();
          for (int i = 0; i < nman and not contact; i++) {
            auto manifold = dispatcher->getManifoldByIndexInternal(i);
            if ((manifold->getBody0() == body or manifold->getBody1() == body) and
                manifold->getNumContacts() > 0)
              contact = true;
          }
        }
      }
      if (contact)
        c->_everGrounded = true;
      if (contact != c->_grounded) {
        c->_grounded = contact;
        if (c->_contactLogAccum >= 1.0f) {
          c->_contactLogAccum = 0.0f;
          logchan_charctl->log(
              "[walker] ground contact %s (y=%.3f)", contact ? "ACQUIRED" : "LOST", bpos.y());
        }
      }
      if (not c->_everGrounded and not c->_warnedNoContact and c->_spawnAge >= 5.0f) {
        c->_warnedNoContact = true;
        logchan_charctl->warn(
            "[walker] WARN: no ground contact within 5s of spawn — terrain collider missing/mismatched at spawn XZ?");
      }
    }

    // EFFECTIVE tuning: script overrides (SetParams) win over the reflected data —
    // the input script owns the feel without recompiles or re-serializes.
    // sprint (SetSprint) scales BOTH so the body can actually REACH the higher speed clamp.
    const float eff_force = ((_ovrMoveForce >= 0.0f) ? _ovrMoveForce : c->_CCD._moveForce) * _sprintScale;
    const float eff_maxspd = ((_ovrMaxSpeed >= 0.0f) ? _ovrMaxSpeed : c->_CCD._maxSpeed) * _sprintScale;
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

    // MOVE basis: the per-character heading, OR (VR) a world yaw fed via MoveBasisYaw so WASD
    // follows the HMD gaze. Only the DRIVE uses it; the camera/_lastLook below stay on _heading.
    const float move_yaw = _moveBasisValid ? _moveBasisYaw : c->_heading;
    const fvec3 drive_fwd(sinf(move_yaw), 0.0f, -cosf(move_yaw));
    const fvec3 drive_right(cosf(move_yaw), 0.0f, sinf(move_yaw));

    // drive the declared walk force (mutating the force DATA is the established
    // live-poke channel — the inst reads it every physics step): z along the
    // move basis, x strafe; diagonals normalized so they don't outrun max force.
    fvec3 drive = drive_fwd * _moveZ + drive_right * _moveX;
    float dmag  = drive.magnitude();
    // INPUT MAGNITUDE = SPEED SCALE (owner call). The old unit clamp made every
    // magnitude >= 1 IDENTICAL (dpad 5 vs stick 10 felt the same). Now: |drive| in
    // [0,1] = analog fraction of base speed (unchanged); above 1 it multiplies BOTH
    // the drive force and the max-speed cap, up to kSpeedScaleMax. The input script
    // owns the per-source scales (keyboard 1x, dpad 2x, stick 4x).
    constexpr float kSpeedScaleMax = 4.0f;
    float speed_scale = 1.0f;
    if (dmag > 1.0f) {
      speed_scale = std::min(dmag, kSpeedScaleMax);
      drive       = drive * (1.0f / dmag);
      dmag        = 1.0f;
    }
    auto& fmap = c->_bullet->data()._forcedatas;
    auto itf   = fmap.find(c->_CCD._forceName);
    if (itf != fmap.end()) {
      if (auto df = std::dynamic_pointer_cast<DirectionalForceData>(itf->second)) {
        df->_direction = (dmag > 0.001f) ? (drive * (1.0f / dmag)) : fwd;
        df->_force     = dmag * eff_force * speed_scale;
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
    } else if (hspeed > eff_maxspd * speed_scale) {
      hvel = hvel * (eff_maxspd * speed_scale / hspeed);
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
      // eye_height measures from the FEET (capsule bottom = ground contact), so the kwarg
      // means what it says: eye height above the ground. pos is the capsule CENTER — the
      // old code added eye_height to it, silently gaining half the capsule (~1.4m) and
      // making every walker view the world from giant height (the toy-scale finding).
      btVector3 smin, smax;
      body->getCollisionShape()->getAabb(btTransform::getIdentity(), smin, smax);
      const fvec3 feet = pos - up * (0.5f * (smax.getY() - smin.getY()));
      fvec3 eye, tgt;
      if (c->_CCD._camDistance > 0.01f) { // follow: pitch orbits the camera about the character
        eye = feet - fwd * (c->_CCD._camDistance * cp) + up * (c->_CCD._eyeHeight + c->_CCD._camDistance * sp);
        tgt = feet + up * c->_CCD._eyeHeight;
      } else { // first person: pitch tilts the view
        eye = feet + up * c->_CCD._eyeHeight;
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
// VR VIEW-DIRECTION LOCOMOTION (render-tick producer). The render FSM fans _gpuUpdate
// out to every system each frame; here we co-locate the HMD gaze read with the device
// camera build (the SceneGraphSystem::_applyHmdPose precedent). ONLY when a device owns
// HMD presentation (real XR runtime) do we feed the move basis; desktop / NoVR never
// satisfies the predicate, so _moveBasisValid stays false and the drive basis is the
// per-character heading exactly as before (byte-identical).
///////////////////////////////////////////////////////////////////////////////

void CharacterControllerSystem::_onGpuUpdate(Simulation* psi, lev2::Context* ctx) {
  // VERDICT-EDGE diag (printf: always visible — this seam misfired silently once):
  // one line per verdict TRANSITION, silent while steady. Locomotion + aim both
  // rig-confirmed 2026-07-15, so the 5s heartbeat retired; a regression still names
  // itself the moment the verdict flips.
  static int         s_diag_frames  = 0;
  static const char* s_last_verdict = nullptr;
  auto _diag = [&](const char* verdict, float gy, float basis, size_t ncomp) {
    s_diag_frames++;
    if (verdict != s_last_verdict) { // literal identity: each verdict is a distinct string constant
      s_last_verdict = verdict;
      printf("[walker] gaze-basis diag: %s gaze_yaw=%.3f basis=%.3f comps=%zu frame=%d\n",
             verdict, double(gy), double(basis), ncomp, s_diag_frames);
      fflush(stdout);
    }
  };
  auto dev = ::ork::lev2::orkidvr::device();
  if (not(dev and dev->_active and dev->ownsHmdPresentation())) {
    _moveBasisValid = false; // session end / desktop fallback -> return to the heading basis
    _aimValid       = false; // CameraRay falls back to the walker camera look (_lastLook)
    _diag("PREDICATE-OFF (desktop/no-XR)", 0.f, 0.f, _components.size());
    return;
  }

  // GAZE FROM THE RENDERED CENTER CAMERA (not the raw posemap). _centercamera is
  // the composed world camera (usermtx*base*hmd — the walker heading is ALREADY inside),
  // i.e. literally what the user sees with: deriving from it CANNOT disagree with
  // perception, where the raw-pose re-derivation shipped a convention skew ("hard to
  // describe, fucked up" on the rig). EXTRACTION RECIPES (empirically pinned against
  // this engine's lookAt — see the startup fixture below). CRITICAL matrix trap: this
  // engine's row() has TRANSPOSE semantics vs the row-vector basis intuition —
  // IV.row(2).xyz is (xaxis.z, yaxis.z, zaxis.z), NOT a basis vector. The VIEW matrix
  // row IS one:
  //   view forward (world) = -normalize(V.row(2).xyz)     [V = world->eye]
  //   gaze yaw             = atan2(fwd.x, -fwd.z)          [matches fwd(yaw) identity]
  // (The retired IV-row yaw recipe was only exact at pitch 0 — it skewed 1.7deg at the
  //  default 0.34 walker pitch, 7.7deg at 0.7.) One-frame staleness (this fan-out runs
  // before the device's frame update) is fine.

  // startup SIGN FIXTURE (one-shot, fail-loud): build view matrices from known
  // heading+pitch pairs via the SAME lookAt construction the walker camera uses, run
  // both extractions, assert identity. A silent sign/transpose bug in exactly this
  // math already shipped TWICE (raw-pose yaw; IV-row pitch skew).
  static bool s_fixture_ran = false;
  if (not s_fixture_ran) {
    s_fixture_ran = true;
    const float tests[][2] = {
        {0.0f, 0.0f}, {float(PI * 0.5), 0.0f}, {float(-PI * 0.5), 0.3f}, {float(PI * 0.25), -0.4f}, {2.5f, 0.6f}};
    for (auto& t : tests) {
      const float h = t[0], p = t[1];
      const fvec3 fwd_hp(sinf(h) * cosf(p), sinf(p), -cosf(h) * cosf(p)); // controller fwd(yaw,pitch) identity
      const fvec3 eye(10.0f, 5.0f, -3.0f);
      fmtx4 v;
      v.lookAt(eye, eye + fwd_hp, fvec3(0, 1, 0));
      const fvec4 vr2 = v.row(2);
      fvec3 d(-vr2.x, -vr2.y, -vr2.z);
      d.normalizeInPlace();
      const float extracted = atan2f(d.x, -d.z);
      const float yaw_err   = fabsf(atan2f(sinf(extracted - h), cosf(extracted - h)));
      const float dir_err   = (d - fwd_hp).magnitude();
      if (yaw_err > 1e-3f or dir_err > 1e-3f) {
        printf(
            "[walker] FATAL gaze fixture FAIL: h=%.4f p=%.4f yaw=%.4f dir_err=%.5f\n",
            double(h), double(p), double(extracted), double(dir_err));
        fflush(stdout);
        OrkAssert(false);
      }
    }
    logchan_charctl->log("[walker] gaze extraction fixture PASS (5 heading/pitch pairs)");
  }

  auto cc = dev->_centercamera;
  if (not cc) {
    _moveBasisValid = false;
    _aimValid       = false;
    _diag("NO-CENTERCAM", 0.f, 0.f, _components.size());
    return;
  }
  const fvec4 vr2 = cc->GetVMatrix().row(2);
  fvec3 gaze_dir(-vr2.x, -vr2.y, -vr2.z); // WORLD view forward (heading + head pitch included)
  gaze_dir.normalizeInPlace();
  const float gaze_yaw = atan2f(gaze_dir.x, -gaze_dir.z); // exact at any pitch

  if (_components.empty()) {
    _diag("NO-COMPONENTS (basis never set)", gaze_yaw, 0.f, 0);
    return;
  }
  _moveBasisYaw   = gaze_yaw; // world basis, used directly — no heading addition
  _moveBasisValid = true;
  // AIM = the full 3D gaze: CameraRay returns it, so '/' and R2 fire along VIEWSPACE
  // Z-OUT. Same producer/thread pattern as the move basis.
  _aimLook  = gaze_dir;
  _aimValid = true;
  if (not _viewLocoLogged) {
    _viewLocoLogged = true;
    logchan_charctl->log("[walker] view-direction locomotion ENGAGED (gaze drives move basis + aim)");
  }
  _diag("ACTIVE", gaze_yaw, _moveBasisYaw, _components.size());
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
