###############################################################################
# walk_input_system — the DEFAULT walker input script (a PythonSystem scene script).
#
# THE TRANSLATION LAYER (owner-ratified): the host forwards RAW key transitions as
# InputKey {key, down} controller messages to the PythonSystem; THIS script owns the
# keymap and translates user actions into SEMANTIC CharacterControllerSystem messages
# (MoveInput / TurnInput / PitchInput / Jump). Customize a scene's controls by pointing
# the PythonSystem at your own copy — no recompiles, the keymap is scene data.
#
# Default map: W/S forward/back · A/D strafe · cursor L/R turn · cursor U/D camera
# pitch (UP looks up = camera lowers) · SPACE jump · / shoot (needs a scene-declared
# "ball_spawner"; silently disabled otherwise — the spawner HANDLE is fetched once
# at link, no name lookups at event time).
###############################################################################
from orkengine.ecssim import *

tokens = CrcStringProxy()

# GLFW cursor keycodes (what ezapp uievents carry); letters are ASCII uppercase.
KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP = 262, 263, 264, 265
KEY_SLASH = 47  # '/' -> shoot
KEY_LSHIFT, KEY_RSHIFT = 340, 344   # shift held -> sprint (SetSprint scales move_force + max_speed)
KEY_CAPSLOCK = 280                  # caps lock = AUTOWALK toggle (hands-free auto-forward). GLFW reports it
                                    # held while the LED is on (down on lock-on, up on lock-off), so
                                    # held(280) == autowalk engaged; W/S still override it.
SPRINT = 5.0                        # sprint multiplier; shared default so a VR locomotion layer + walker agree

# SHOOT TUNING (script-owned, like PARAMS)
SHOOT_SPEED   = 30.0   # m/s muzzle speed
SHOOT_OFFSET  = 1.0    # spawn this far along the ray (clear the capsule)
SHOOT_SCALE   = 0.25   # instance scale (the ball model is ~1m radius)
# topspin about the ray's lateral axis (cross(up, dir) = rolling-forward
# sign): surface speed > linear speed, so a frictional contact accelerates
# the ball FORWARD. Pure roll at r=0.25 is speed/r = 120 rad/s; 2x = overspin.
SHOOT_SPIN    = 240.0  # rad/s


# RUNTIME TUNING — these override the scene's walker() values via SetParams (any
# subset; remove a key to fall back to the scene/reflected value). Edit + relaunch:
# no C++ recompile, no scene change.
PARAMS = {
  #"max_speed":      28.0,    # 40 mph
  #"move_force":     11400.0, # scaled with speed (reach max in ~0.25s)
  #"brake":          5.0,     # release -> stop in ~0.3s
  #"turn_rate":      3.5,
  #"jump_impulse":   1600.0,
  #"turn_decay":     6.0,     # angular ease-out after key release (~3/decay s tail)
  #"rest_friction":  2.0,     # idle contact friction (sit still on hills; tan⁻¹(2)≈63°)
  #"drive_friction": 0.0,     # contact friction while driving (0 = the zero-drag feel)
}


class WalkInput:
  def __init__(self):
    self.keys = set()
    self.dbg_events = 0


def onSystemInit(simulation):
  print("[walk_input] onSystemInit", flush=True)
  simulation.vars.walk = WalkInput()


def onSystemLink(simulation):
  W = simulation.vars.walk
  W.charctl = simulation.findSystemByName("CharacterControllerSystem")
  W.charctl.notify(tokens.SetParams,
                   {getattr(tokens, k): float(v) for k, v in PARAMS.items()})
  # the spawner HANDLE, resolved ONCE (the only string lookup); None = scene
  # declares no ball_spawner and / does nothing.
  W.ball_spawner = simulation.findSpawner("ball_spawner")
  print("[walk_input] onSystemLink charctl=%s params=%s spawner=%s" % (
      W.charctl, PARAMS, W.ball_spawner), flush=True)


def _send_state(simulation):
  W    = simulation.vars.walk
  held = W.keys.__contains__
  mz   = (5.0 if held(ord("W")) else 0.0) - (5.0 if held(ord("S")) else 0.0)
  if held(KEY_CAPSLOCK) and mz == 0.0:   # AUTOWALK: caps lock -> auto-forward; W adds, S brakes/reverses
    mz = 5.0
  mx   = (1.0 if held(ord("D")) else 0.0) - (1.0 if held(ord("A")) else 0.0)
  turn = (0.3 if held(KEY_RIGHT) else 0.0) - (0.3 if held(KEY_LEFT) else 0.0)
  # cursor UP = look up (positive semantic pitch = view/camera rises)
  pitch = (0.3 if held(KEY_UP) else 0.0) - (0.3 if held(KEY_DOWN) else 0.0)
  W.charctl.notify(tokens.MoveInput,  {tokens.x: float(mx), tokens.z: float(mz)})
  W.charctl.notify(tokens.TurnInput,  {tokens.rate: float(turn)})
  W.charctl.notify(tokens.PitchInput, {tokens.rate: float(pitch)})
  # SHIFT -> sprint: scale move_force + max_speed (the C++ controller SETS the scale, not accumulates,
  # so in a VR+walker scene where a VR locomotion layer also sends this, the matching value is harmless).
  sprint = SPRINT if (held(KEY_LSHIFT) or held(KEY_RSHIFT)) else 1.0
  W.charctl.notify(tokens.SetSprint, {tokens.scale: float(sprint)})


def _shoot(simulation):
  # SYNCHRONOUS query (system script = update thread): pull the camera ray from
  # charctl, then spawn through the link-time spawner handle. The SCRIPT owns
  # the projectile policy (spawner, speed, offset).
  W = simulation.vars.walk
  if not W.ball_spawner:  # falsy when the scene declares no ball_spawner
    return
  ray = W.charctl.request(tokens.CameraRay, {})
  if ray is None:
    return
  pos  = ray[tokens.pos]
  dir  = ray[tokens.dir]
  axis = vec3(0, 1, 0).cross(dir)        # topspin axis (zero if aiming straight up/down)
  spin = axis.normalized * SHOOT_SPIN if axis.length > 1e-3 else vec3(0, 0, 0)
  ent  = W.ball_spawner.spawn(pos=pos + dir * SHOOT_OFFSET,
                              vel=dir * SHOOT_SPEED,
                              avel=spin,
                              scale=SHOOT_SCALE)
  if W.dbg_events < 24:
    W.dbg_events += 1
    print("[walk_input] shoot %s @ %s" % (ent, pos), flush=True)


def onSystemNotify(simulation, evID, table):
  if evID.hashed == tokens.Collision.hashed:
    # CONTACT INTERCEPTION (gameplay reacts here: damage, sfx, footstep surfaces...).
    # payload: nameA/nameB (entity names), pointA/pointB, normalOnB, groupA/groupB.
    W = simulation.vars.walk
    if W.dbg_events < 24:
      W.dbg_events += 1
      print("[walk_input] Collision %s <-> %s @ %s" % (
          table[tokens.nameA], table[tokens.nameB], table[tokens.pointA]), flush=True)
    return
  if evID.hashed == tokens.InputKey.hashed:
    W    = simulation.vars.walk
    key  = table[tokens.key]
    down = table[tokens.down]
    if W.dbg_events < 8:  # first few transitions: prove the host->script hop
      W.dbg_events += 1
      print("[walk_input] InputKey key=%s down=%s" % (key, down), flush=True)
    if down:
      W.keys.add(key)
      if key == 32:  # SPACE -> one-shot jump
        W.charctl.notify(tokens.Jump, {})
      if key == KEY_SLASH:  # '/' -> shoot (synchronous CameraRay query + spawn)
        _shoot(simulation)
    else:
      W.keys.discard(key)
    _send_state(simulation)


_selftest = {"mode": __import__("os").environ.get("ORK_WALK_SELFTEST", ""),
             "count": 0}


def onSystemUpdate(simulation):
  if _selftest["mode"]:
    _selftest["count"] += 1
    if _selftest["count"] == 1000:
      print("[walk_input] SELFTEST shoot", flush=True)
      _shoot(simulation)
    # mode "2": RAPID FIRE — a spawn/despawn stressor for the trail
    # lifecycle (graph link on the update thread vs slots rendering).
    elif _selftest["mode"] == "2" and _selftest["count"] > 1000 \
         and _selftest["count"] % 160 == 0:
      _shoot(simulation)
