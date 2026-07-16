////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// CharacterControllerComponent (E.2-walk) — the REUSABLE walk-on-terrain behavior,
// as DATA. Pairs with a sibling BulletObjectComponent (capsule shape, angularFactor
// (0,1,0) upright lock, a declared DirectionalForce named `_forceName` — the proven
// FPS-example recipe) and drives it from INPUT MESSAGES:
//
//   INPUT enters the ECS as controller messages (the UpdateCamera pattern), in TWO hops
//   (owner-ratified): the HOST forwards RAW key events to the scene's PYTHONSYSTEM
//   (InputKey {key,down}); the scene's input SCRIPT (walker() ships a default —
//   walk_input_system.py) translates user actions into SEMANTIC messages on THIS system
//   (MoveInput / TurnInput / PitchInput / Jump) — so the keymap is scene data. Per
//   update the system integrates heading/pitch, drives the walk force, clamps
//   horizontal speed, applies the jump impulse (near-rest only), and PUBLISHES the
//   camera (fps or follow) from the character entity's live transform — a host that
//   detects this system simply stops pushing its own camera.
//
// Everything reflected -> the whole behavior round-trips in the scene JSON and plays
// in the pure-C++ player with zero scripting. The scene-DSL `walker()` helper declares
// the full bundle in one call.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/ecs/component.h>
#include <ork/ecs/system.h>
#include <set>

namespace ork::ecs {

struct CharacterControllerComponentData;
struct CharacterControllerComponent;
struct CharacterControllerSystemData;
struct CharacterControllerSystem;
struct BulletObjectComponent;
struct BulletSystem;
struct SceneGraphSystem;

///////////////////////////////////////////////////////////////////////////////

struct CharacterControllerComponentData : public ComponentData {
  DeclareConcreteX(CharacterControllerComponentData, ComponentData);

public:
  CharacterControllerComponentData();

  Component* createComponent(Entity* pent) const final;
  static object::ObjectClass* componentClass();
  void DoRegisterWithScene(SceneComposer& sc) const final;

  float _moveForce   = 600.0f; // newtons along the heading while W/S held
  float _maxSpeed    = 6.0f;   // horizontal speed clamp (m/s)
  float _jumpImpulse = 0.0f;   // central impulse on SPACE (0 = jumping disabled)
  float _turnRate    = 2.5f;   // heading yaw rate (rad/s) while A/D held
  float _brake       = 10.0f;  // horizontal velocity decay (1/s) while NO move input
                               // (asymmetric drag: stop fast on release, free while driving)
  float _turnDecay   = 6.0f;   // turn/pitch rate decay (1/s) after key release — angular
                               // inertia ease-out (~3/decay seconds to stop); huge = snap
  // ASYMMETRIC CONTACT FRICTION (the controller owns the capsule's body friction):
  // ~0 while DRIVING (the ratified zero-drag feel; brake alone can't hold a slope —
  // gravity wins a creep speed of g·sinθ/brake), HIGH at rest so the character
  // SITS STILL on hills (tan⁻¹(2) ≈ 63° holdable). NOTE bullet combines contact
  // friction multiplicatively — static colliders must carry ~1.0 for these to read
  // as the effective values.
  float _driveFriction = 0.0f;
  float _restFriction  = 2.0f;
  float _eyeHeight   = 1.6f;   // camera eye above the capsule origin (m)
  float _camDistance = 8.0f;   // follow distance behind the character (0 = first person)
  float _fovyDeg     = 45.0f;
  float _camNear     = 0.1f;
  float _camFar      = 1000.0f;
  float _killZDrop   = 0.0f;    // KILL-Z self-defense (OPT-IN, <=0 = DISABLED — owner call after a
                                // too-tight default preempted a legitimate long fall): if >0 and the
                                // body falls more than this many
                                // metres below its SPAWN y it is teleported home with zeroed
                                // velocity (the fell-out-of-the-world guard; 0 = disabled)
  float _spawnAboveGround = 5.0f; // SPAWN GROUND-SNAP (ON by default, <=0 = exact authored spawn):
                                  // when the scene has a terrain floor, the authored spawn Y is
                                  // ADVISORY — a downward raycast at spawn XZ places the capsule
                                  // FEET this many metres above the actual static ground. Kills
                                  // both spawn-height footguns at once: underground spawns (never
                                  // contact) and sky spawns (terminal-velocity tunneling THROUGH
                                  // the heightfield).
  std::string _forceName = "walkforce"; // the declared DirectionalForce on the bullet component
};

using charactercontrollercomponentdata_ptr_t = std::shared_ptr<CharacterControllerComponentData>;

///////////////////////////////////////////////////////////////////////////////

struct CharacterControllerComponent : public Component {
  DeclareAbstractX(CharacterControllerComponent, Component);

public:
  CharacterControllerComponent(const CharacterControllerComponentData& cd, Entity* pent);

  bool _onLink(Simulation* psi) final;
  bool _onStage(Simulation* psi) final;
  void _onUnstage(Simulation* psi) final;

  const CharacterControllerComponentData& _CCD;
  CharacterControllerSystem* _system = nullptr;
  BulletObjectComponent* _bullet     = nullptr; // resolved lazily (rigid body exists post-activate)
  float _heading                     = 0.0f;    // yaw (radians; 0 = -Z forward)
  float _pitch                       = 0.34f;   // camera pitch (radians; clamped ±1.2)
  float _appliedFriction             = -1.0f;   // last friction pushed to the body (switch detect)
  // SELF-DEFENSE runtime state (not reflected — derived at play time):
  fvec3 _spawnPos;                   // the KILL-Z respawn target (captured on first body resolve,
                                     // REWRITTEN by the ground-snap so respawns land on the snap)
  bool  _spawnValid      = false;
  bool  _spawnSnapDone   = false;    // ground-snap resolved (snapped, gave up, or disabled)
  bool  _grounded        = false;    // ground-contact state, edge-detected for the transition logs
  bool  _everGrounded    = false;    // has contact EVER been acquired (gates the 5s no-contact WARN)
  bool  _warnedNoContact = false;    // one-shot latch for the 5s no-contact WARN
  bool  _warnedUnderground = false;  // one-shot latch for the spawn-below-terrain-min WARN
  float _spawnAge        = 0.0f;     // seconds since spawn (drives the 5s no-contact WARN)
  float _contactScanAccum = 0.0f;    // ~10Hz throttle for the manifold contact scan
  float _contactLogAccum = 1000.0f;  // rate-limit (1/s) for contact-edge logs (big = first edge logs)
  float _killZLogAccum   = 1000.0f;  // rate-limit (2s) for KILL-Z respawn logs (big = first fires)
};

///////////////////////////////////////////////////////////////////////////////

struct CharacterControllerSystemData : public SystemData {
  DeclareConcreteX(CharacterControllerSystemData, SystemData);

public:
  CharacterControllerSystemData();

protected:
  System* createSystem(Simulation* psi) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct CharacterControllerSystem : public System {
  DeclareAbstractX(CharacterControllerSystem, System);

public:
  static constexpr systemkey_t SystemType = "CharacterControllerSystem";
  systemkey_t systemTypeDynamic() final { return SystemType; }

  // SEMANTIC ACTION MESSAGES (owner-ratified): the host forwards RAW keys to the scene's
  // PythonSystem; the scene's input SCRIPT translates user actions into THESE — so the
  // keymap is scene data, and this system knows nothing about keys.
  //   MoveInput  {x: float, z: float}   drive in heading space (z fwd/back, x strafe), -1..1
  //   MoveBasisYaw {yaw: float}         VR ONLY: override the MOVE basis with this world yaw
  //              (radians) instead of the per-character heading — so WASD follows the HMD GAZE
  //              (the VR layer feeds gaze_yaw + playspace_yaw). The camera/heading are untouched;
  //              desktop never sends it. Sticky once set (resent each tick by the VR script).
  //   TurnInput  {rate: float}          heading yaw rate scale, -1..1
  //   PitchInput {rate: float}          camera pitch rate scale, -1..1 (positive = camera up)
  //   TurnStep   {radians: float}       DISCRETE heading step — adds directly to _heading (no
  //              rate integration). For positional camera-yaw controls (e.g. face-button dpad);
  //              frame-timing robust where a rate pulse is not.
  //   PitchStep  {radians: float}       DISCRETE pitch step — adds to _pitch, clamped to the same
  //              ±1.2 limits the PitchInput rate honors. Positive = camera up.
  //   Jump       {}                     one-shot
  //   SetParams  {move_force?, max_speed?, brake?, turn_rate?, jump_impulse?,
  //               turn_decay?, drive_friction?, rest_friction?: float}
  //              runtime OVERRIDES of the reflected tuning (any subset) — the input
  //              script owns the FEEL, no C++ recompile, no scene re-serialize.
  //   SetAimDir  {dx, dy, dz: float}   override the look direction CameraRay returns.
  //              In XR the system PRODUCES this itself (_onGpuUpdate feeds the rendered
  //              center-camera gaze every render tick — '/'-shoot aims down viewspace Z-out
  //              with no VR knowledge in the input script), so a script override only wins
  //              on desktop; desktop otherwise returns the real walker camera (_lastLook).
  //   SetSprint  {scale: float}        transient multiplier on BOTH move_force and max_speed
  //              (a sprint/boost held by an input script — e.g. shift). 1.0 = normal; resent
  //              each tick (a script drives it from the live key state).
  // SYNCHRONOUS QUERIES (system-script context — the script runs on the update
  // thread, so system.request() is a direct call, not a controller round-trip):
  //   CameraRay  {} -> {pos: vec3, dir: vec3}  the current eye + full 3D look
  //              (incl. pitch). The SCRIPT owns what to do with the ray (spawn
  //              a projectile, raycast, ...) — this system only owns the camera
  //              math it already computes.
  static constexpr auto MoveInput  = "MoveInput"_ecstok;
  static constexpr auto MoveBasisYaw = "MoveBasisYaw"_ecstok;
  static constexpr auto TurnInput  = "TurnInput"_ecstok;
  static constexpr auto PitchInput = "PitchInput"_ecstok;
  static constexpr auto TurnStep   = "TurnStep"_ecstok;
  static constexpr auto PitchStep  = "PitchStep"_ecstok;
  static constexpr auto Jump       = "Jump"_ecstok;
  static constexpr auto SetParams  = "SetParams"_ecstok;
  static constexpr auto SetAimDir  = "SetAimDir"_ecstok;
  static constexpr auto SetSprint  = "SetSprint"_ecstok;
  static constexpr auto CameraRay  = "CameraRay"_ecstok;

  CharacterControllerSystem(const CharacterControllerSystemData& data, Simulation* psi);

  void _onStageComponent(CharacterControllerComponent* c);
  void _onUnstageComponent(CharacterControllerComponent* c);

protected:
  bool _onLink(Simulation* psi) final;
  void _onUpdate(Simulation* psi) final;
  // VR view-direction locomotion PRODUCER (render-tick): when a device owns HMD presentation,
  // reads the raw HMD gaze yaw and feeds _moveBasisYaw so WASD/stick drive follows the head.
  // Co-located on the render thread with the device camera build (the _applyHmdPose precedent);
  // desktop/NoVR falls through the predicate -> _moveBasisValid stays false -> heading basis.
  void _onGpuUpdate(Simulation* psi, lev2::Context* ctx) final;
  void _onNotify(token_t evID, evdata_t data) final;
  void _onRequest(impl::sys_response_ptr_t response, token_t evID, evdata_t data) final;

  const CharacterControllerSystemData& _CCSD;
  SceneGraphSystem* _sgsys = nullptr;
  BulletSystem* _bulletsys = nullptr; // for the manifold refresh on friction switch
  std::set<CharacterControllerComponent*> _components; // update-thread only (notify + update + stage)
  // semantic action state (written by _onNotify from the input script, read by _onUpdate)
  float _moveX = 0.0f, _moveZ = 0.0f; // strafe / fwd drive, -1..1
  float _turn = 0.0f, _pitchRate = 0.0f;
  // VR view-relative drive: when valid (MoveBasisYaw message), the walk force uses _moveBasisYaw
  // as the move basis instead of the per-character heading — so WASD follows the HMD gaze. The
  // camera/heading are unchanged; desktop never sends it so _moveBasisValid stays false.
  float _moveBasisYaw   = 0.0f;
  bool  _moveBasisValid = false;
  bool  _viewLocoLogged = false; // one-shot latch for the view-direction-locomotion ENGAGED log
  float _sprintScale    = 1.0f; // SetSprint: transient multiplier on move_force + max_speed (shift-boost)
  // smoothed turn/pitch rates: instant attack while held, exp tail (TurnDecay) on release
  float _turnVel = 0.0f, _pitchVel = 0.0f;
  float _jumpTTL = 0.0f; // buffered jump request (seconds remaining)
  // script-set runtime overrides of the reflected tuning (<0 = unset, use the data)
  float _ovrMoveForce = -1.0f, _ovrMaxSpeed = -1.0f, _ovrBrake = -1.0f;
  float _ovrTurnRate = -1.0f, _ovrJumpImpulse = -1.0f;
  float _ovrTurnDecay = -1.0f, _ovrDriveFriction = -1.0f, _ovrRestFriction = -1.0f;
  float _logAccum = 0.0f;                               // probe-telemetry rate limit
  int _dbgNotifies = 0;                                 // first-N semantic-message log
  // latest camera ray (stashed each update by the camera publish; consumed by Shoot)
  fvec3 _lastEye, _lastLook = fvec3(0, 0, -1);
  // VR aim override (SetAimDir): when valid, CameraRay returns this look dir (HMD gaze + lob)
  // instead of _lastLook. The eye stays _lastEye (the walker eye — fine, the ball spawns ahead).
  fvec3 _aimLook        = fvec3(0, 0, -1);
  bool  _aimValid       = false;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ecs
