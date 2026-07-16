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
import math
import time

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

# GAMEPAD (S1): the host forwards the pad as abstract tokens — GamepadButton {button, down}
# (edge, like InputKey) + GamepadAxes {lx,ly,rx,ry,l2,r2,connected} (per-tick analog). THIS
# script owns the pad mapping (DUAL-DPAD scheme): LEFT stick + LEFT dpad = locomotion;
# FACE BUTTONS = a right-dpad of DISCRETE camera-angle steps; R1 = jump; right stick unbound.
# Button ids arrive as crcstring tokens; cache their hashes once.
GP_CROSS      = tokens.CROSS.hashed
GP_TRIANGLE   = tokens.TRIANGLE.hashed
GP_SQUARE     = tokens.SQUARE.hashed
GP_CIRCLE     = tokens.CIRCLE.hashed
GP_L1         = tokens.L1.hashed
GP_R1         = tokens.R1.hashed
GP_DPAD_UP    = tokens.DPAD_UP.hashed
GP_DPAD_DOWN  = tokens.DPAD_DOWN.hashed
GP_DPAD_LEFT  = tokens.DPAD_LEFT.hashed
GP_DPAD_RIGHT = tokens.DPAD_RIGHT.hashed
GP_DEADZONE   = 0.15   # stick deadzone before analog contributes

# FACE BUTTONS as a RIGHT-DPAD of DISCRETE CAMERA STEPS (positional, per PRESS EDGE — not
# held-repeat; held-repeat is a possible follow-up). TRIANGLE(top)=pitch up, CROSS(bottom)=
# pitch down, SQUARE(left)=yaw left, CIRCLE(right)=yaw right. The C++ controller consumes
# these as TurnStep/PitchStep (absolute radian deltas), frame-timing robust where a rate is not.
YAW_STEP_DEG   = 15.0  # per-press camera yaw step (SQUARE / CIRCLE)
PITCH_STEP_DEG = 10.0  # per-press camera pitch step (TRIANGLE / CROSS)
YAW_STEP_RAD   = YAW_STEP_DEG * math.pi / 180.0
PITCH_STEP_RAD = PITCH_STEP_DEG * math.pi / 180.0

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
    self.gp_axes = None      # latest analog snapshot (dict) while a pad is connected, else None
    self.gp_buttons = set()  # currently-held gamepad button hashes
    self.r2_held = False     # R2 trigger-as-button latch (fire on press edge, hysteresis)
    # STAGE-3 liveness (script): per ~5s counters of gamepad messages received + charctl
    # sends dispatched. Prints only after a gamepad message has been seen (silent with no
    # pad), only when nonzero OR just transitioned to zero. rx dies -> notify dispatch
    # stopped upstream; sends alive but no motion -> character controller / sim backlog.
    self.gp_seen = False
    self.rx_btn = 0
    self.rx_axes = 0
    self.sends = 0
    self.live_t0 = None
    self.rx_btn_wasnz = False
    self.rx_axes_wasnz = False
    self.sends_wasnz = False


def onSystemInit(simulation):
  print("[walk_input] onSystemInit", flush=True)
  simulation.vars.walk = WalkInput()


def onSystemLink(simulation):
  W = simulation.vars.walk
  # spawn-scouting aid: resolved once; onSystemUpdate prints the walker position
  # every ~10s so a scene author can walk somewhere and copy the coordinates into
  # terrain(spawn=vec3(...)). Falsy when the scene has no "walker" entity.
  W.walker_ent = simulation.findEntityByName("walker")
  W.pos_print_next = 10.0
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
  # LOCOMOTION — |MoveInput| IS the SPEED SCALE (controller semantic: <=1 analog fraction
  # of base speed; >1 multiplies force AND max speed, clamped 4x controller-side).
  # Per-source scales (owner call): KEYBOARD 1x · L-DPAD 2x · L-STICK 4x(*deflection).
  # Each source's direction is normalized BEFORE scaling so diagonals don't outrun it.
  def _dirscale(x, z, scale):
    m = math.sqrt(x * x + z * z)
    if m < 1e-6: return (0.0, 0.0)
    return (x / m * scale, z / m * scale)
  kz = (1.0 if held(ord("W")) else 0.0) - (1.0 if held(ord("S")) else 0.0)
  if held(KEY_CAPSLOCK) and kz == 0.0:   # AUTOWALK: caps lock -> auto-forward; W adds, S brakes/reverses
    kz = 1.0
  kx = (1.0 if held(ord("D")) else 0.0) - (1.0 if held(ord("A")) else 0.0)
  kx, kz = _dirscale(kx, kz, 1.0)        # keyboard: 1x base speed
  turn = (0.3 if held(KEY_RIGHT) else 0.0) - (0.3 if held(KEY_LEFT) else 0.0)
  # cursor UP = look up (positive semantic pitch = view/camera rises)
  pitch = (0.3 if held(KEY_UP) else 0.0) - (0.3 if held(KEY_DOWN) else 0.0)
  ax = W.gp_axes
  gpb = W.gp_buttons
  # L-DPAD: continuous, 2x
  dx = (1.0 if GP_DPAD_RIGHT in gpb else 0.0) - (1.0 if GP_DPAD_LEFT in gpb else 0.0)
  dz = (1.0 if GP_DPAD_UP in gpb else 0.0) - (1.0 if GP_DPAD_DOWN in gpb else 0.0)
  dx, dz = _dirscale(dx, dz, 2.0)
  # L-STICK: analog, up to 4x at full deflection (direction from the stick, magnitude
  # = 4 * deflection; square-gate diagonals clamped to deflection 1).
  sx = sz = 0.0
  if ax:
    def _dz(v): return 0.0 if -GP_DEADZONE < v < GP_DEADZONE else v
    rx, rz = _dz(ax["lx"]), -_dz(ax["ly"])   # stick up (ly<0) -> forward
    m = math.sqrt(rx * rx + rz * rz)
    if m > 1e-6:
      sx, sz = (rx / m) * 4.0 * min(m, 1.0), (rz / m) * 4.0 * min(m, 1.0)
    # RIGHT STICK: unbound — camera angle is the FACE BUTTONS (discrete TurnStep/PitchStep).
  mx = kx + dx + sx
  mz = kz + dz + sz                     # stacked sources exceed 4? controller clamps at 4x
  turn  = max(-0.3, min(0.3, turn))
  pitch = max(-0.3, min(0.3, pitch))
  W.sends += 1  # STAGE-3 liveness: script -> character-controller dispatch
  W.charctl.notify(tokens.MoveInput,  {tokens.x: float(mx), tokens.z: float(mz)})
  W.charctl.notify(tokens.TurnInput,  {tokens.rate: float(turn)})
  W.charctl.notify(tokens.PitchInput, {tokens.rate: float(pitch)})
  # SPRINT: keyboard SHIFT only (the C++ controller SETS the scale, not accumulates). Gamepad
  # sprint is unbound — R1 is JUMP now; L3 (left-stick click) is the ready pad-sprint candidate:
  #   if GP_L3 in gpb: sprint = SPRINT
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
    return
  if evID.hashed == tokens.GamepadButton.hashed:  # GAMEPAD: abstract button edge (token id)
    W = simulation.vars.walk
    W.gp_seen = True
    W.rx_btn += 1
    h = table[tokens.button].hashed
    down = table[tokens.down]
    if down:
      W.gp_buttons.add(h)
      if h == GP_R1:  # R1 -> one-shot jump (was CROSS; CROSS is now a camera-down step)
        W.charctl.notify(tokens.Jump, {})
      # FACE BUTTONS as a right-dpad of DISCRETE CAMERA STEPS (per press edge):
      elif h == GP_TRIANGLE:  # top    -> pitch step UP
        W.charctl.notify(tokens.PitchStep, {tokens.radians: float(PITCH_STEP_RAD)})
      elif h == GP_CROSS:     # bottom -> pitch step DOWN
        W.charctl.notify(tokens.PitchStep, {tokens.radians: float(-PITCH_STEP_RAD)})
      elif h == GP_SQUARE:    # left   -> yaw step LEFT
        W.charctl.notify(tokens.TurnStep, {tokens.radians: float(-YAW_STEP_RAD)})
      elif h == GP_CIRCLE:    # right  -> yaw step RIGHT
        W.charctl.notify(tokens.TurnStep, {tokens.radians: float(YAW_STEP_RAD)})
    else:
      W.gp_buttons.discard(h)
    if W.dbg_events < 8:
      W.dbg_events += 1
      print("[walk_input] GamepadButton hash=0x%x down=%s" % (h, down), flush=True)
    _send_state(simulation)
    return
  if evID.hashed == tokens.GamepadAxes.hashed:  # GAMEPAD: per-tick analog snapshot
    W = simulation.vars.walk
    W.gp_seen = True
    W.rx_axes += 1
    if table[tokens.connected]:
      W.gp_axes = {"lx": table[tokens.lx], "ly": table[tokens.ly],
                   "rx": table[tokens.rx], "ry": table[tokens.ry],
                   "l2": table[tokens.l2], "r2": table[tokens.r2]}
      # R2 TRIGGER = FIRE (same _shoot as '/'): analog trigger as a button — edge on
      # press with HYSTERESIS (>0.5 fires, must fall below 0.3 to re-arm) so a held
      # half-squeeze can't machine-gun on axis jitter.
      r2 = W.gp_axes["r2"]
      if not W.r2_held and r2 > 0.5:
        W.r2_held = True
        _shoot(simulation)
      elif W.r2_held and r2 < 0.3:
        W.r2_held = False
    else:  # pad unplugged -> zero everything the pad was driving
      W.gp_axes = None
      W.gp_buttons.clear()
      W.r2_held = False
    _send_state(simulation)


_selftest = {"mode": __import__("os").environ.get("ORK_WALK_SELFTEST", ""),
             "count": 0}


def onSystemUpdate(simulation):
  # position beacon (throttled): gameTime-based so pause stalls it with the sim.
  W = simulation.vars.walk
  # STAGE-3 liveness heartbeat (throttled ~5s wall-clock; silent until a pad is seen).
  if W.gp_seen:
    now = time.monotonic()
    if W.live_t0 is None:
      W.live_t0 = now
    if now - W.live_t0 >= 5.0:
      W.live_t0 = now
      if (W.rx_btn or W.rx_axes or W.sends
          or W.rx_btn_wasnz or W.rx_axes_wasnz or W.sends_wasnz):
        print("[walk_input] rx btn=%d axes=%d sends=%d/5s" % (W.rx_btn, W.rx_axes, W.sends), flush=True)
      W.rx_btn_wasnz = W.rx_btn > 0
      W.rx_axes_wasnz = W.rx_axes > 0
      W.sends_wasnz = W.sends > 0
      W.rx_btn = 0
      W.rx_axes = 0
      W.sends = 0
  if W.walker_ent:
    t = simulation.gameTime
    if t >= W.pos_print_next:
      W.pos_print_next = t + 10.0
      p = W.walker_ent.translation
      print("[walker] t=%.0fs pos = vec3(%.1f, %.1f, %.1f)   # terrain(spawn=...)" % (t, p.x, p.y, p.z), flush=True)
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
