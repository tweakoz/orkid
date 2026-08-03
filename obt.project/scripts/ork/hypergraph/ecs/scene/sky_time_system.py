###############################################################################
# sky_time_system — LIVE TIME-OF-DAY CONTROLS: scrub / pause / speed / date on
# a running scene, as a composable PythonSystem scene script.
#
# It replaces a LAUNCH-TIME workflow. Until now the only way to see a different
# hour was to bake one at author time (SWEST_TOD / SWEST_TIMESCALE and the other
# scene env knobs) and relaunch; the sky was whatever the scene was declared
# with. This script publishes the ONE shared clock (_sky_time.SkyTimeControl) on
# the simulation varmap, and every celestial body (_celestial_orbit.py) reads
# its instant from there — so an hour set here moves the sun, the moon, the star
# dome, the shadows and the sky's own light together, on the next frame.
#
# THE KEYMAP IS DATA (this table, in this file). A scene that wants different
# keys points its PythonSystem at its own copy of this script — no recompile,
# same as walk_input_system.py's map. Chosen from the FREE pool; the taken set
# at the time of writing is camera Z/X/C/V, walk W/A/S/D + cursors + SPACE + '/'
# + SHIFT + CAPSLOCK, and the player's own `/~, P, Cmd+arrows, Cmd+R plus the
# --devkeys block E/G/T/H/M/B/R (registered in ork.ecs/examples/c++/player/
# main.cpp, whose option string + keyshud legend is the fleet's de-facto key
# registry — the sky keys are listed there too).
#
#   ]  scrub the sky FORWARD while held      (SCRUB_HOURS_PER_SEC clock hrs/sec)
#   [  scrub the sky BACKWARD while held
#   \  pause / resume the sky clock          (the scene keeps running)
#   =  faster  (x RATE_STEP, capped RATE_MAX)
#   -  slower  (/ RATE_STEP, floored RATE_MIN)
#   '  next day        ;  previous day       (same hour, next/prev date)
#   0  reset to the hour, date and speed the scene was authored with
#
# THE SAME CONTROLS PROGRAMMATICALLY — what a tool, a gate or a scripted movie
# uses (the keys are implemented by calling these, so the two can never drift):
#
#   controller.systemNotify(pysystem, tokens.SkyTimeSet,    {tokens.hour: 18.5})
#   controller.systemNotify(pysystem, tokens.SkyTimeSetDay, {tokens.day: 355.0})
#   controller.systemNotify(pysystem, tokens.SkyTimeStep,   {tokens.hours: -1.5})
#   controller.systemNotify(pysystem, tokens.SkyTimeStepDay,{tokens.days: 1.0})
#   controller.systemNotify(pysystem, tokens.SkyTimePause,  {tokens.paused: 1})
#   controller.systemNotify(pysystem, tokens.SkyTimeRate,   {tokens.rate: 8.0})
#   controller.systemNotify(pysystem, tokens.SkyTimeScrub,  {tokens.rate: 1.2})
#   controller.systemNotify(pysystem, tokens.SkyTimeReset,  {})
#
# TELEMETRY — one line, printed on every control change and on a beacon, which
# is the readback a gate parses and a human watches:
#
#   [sky_time] t=<sim secs> hour=<0..24> day=<epoch days> paused=<0|1>
#              rate=<x> scrub=<hrs/s> changes=<n> <body>_el=<deg> <body>_az=<deg>
#
# The body angles are the ones the bodies published on the PREVIOUS tick (system
# scripts run before component scripts), so a change is reported one tick late —
# deliberately, see onSystemUpdate.
#
# Runs in the ECS sim SUBINTERPRETER: `simulation` is the SimSystem's Simulation
# and _sky_time.py is loaded BY PATH for the reason _celestial_orbit.py
# documents (a dotted import would pull orkengine.core/lev2, which do not exist
# here).
###############################################################################
import os
import sys

from orkengine.ecssim import CrcStringProxy

tokens = CrcStringProxy()

_SKYTIME_RELPATH = os.path.join("ork", "hypergraph", "ecs", "scene", "_sky_time.py")

###############################################################################
# THE TUNING (data — edit here, or point the scene at your own copy)
###############################################################################

# held-scrub speed, in CLOCK HOURS per WALL SECOND. 1.2 sweeps a full day in
# 20 seconds of holding: fast enough to get anywhere, slow enough that a human
# can stop on a sunset.
SCRUB_HOURS_PER_SEC = 1.2
RATE_STEP = 2.0        # [=] / [-] multiply / divide the authored speed by this
RATE_MIN  = 0.0625
RATE_MAX  = 256.0
BEACON_SECS = 2.0      # sim seconds between telemetry lines while nothing changes

KEYMAP = {
    93: ("scrub", +1.0),    # ]
    91: ("scrub", -1.0),    # [
    92: ("pause", 0.0),     # \
    61: ("rate", +1.0),     # =
    45: ("rate", -1.0),     # -
    39: ("day", +1.0),      # '
    59: ("day", -1.0),      # ;
    48: ("reset", 0.0),     # 0
}

###############################################################################

_LOADED = {}


def _load_by_path(relpath, modname):
  module = _LOADED.get(relpath)
  if module is not None:
    return module
  import importlib.util
  for root in sys.path:
    candidate = os.path.join(root or ".", relpath)
    if os.path.exists(candidate):
      spec = importlib.util.spec_from_file_location(modname, candidate)
      module = importlib.util.module_from_spec(spec)
      spec.loader.exec_module(module)
      _LOADED[relpath] = module
      return module
  raise ImportError(
      "sky-time module <%s> not found on sys.path — the sim subinterpreter "
      "cannot see obt.project/scripts" % relpath)


def _skytime():
  return _load_by_path(_SKYTIME_RELPATH, "_ork_sky_time")


class SkyTimeState:
  def __init__(self, control):
    self.control = control
    self.scrub_keys = {}     # keycode -> direction, for held-scrub arbitration
    self.next_beacon = 0.0
    self.last_changes = -1
    self.pending = False     # a control op is waiting for the bodies to re-aim


def _state(simulation):
  return simulation.vars.sky_time_state


def _emit(simulation, why):
  S = _state(simulation)
  ctrl = S.control
  t = simulation.gameTime
  st = ctrl.state(t)
  bodies = ""
  for name in sorted(ctrl.report):
    r = ctrl.report[name]
    bodies += " %s_el=%.4f %s_az=%.4f" % (r["body"], r["elevation"],
                                          r["body"], r["azimuth"])
  print("[sky_time] t=%.3f hour=%.4f day=%.4f paused=%d rate=%.6g scrub=%.6g "
        "changes=%d%s (%s)"
        % (t, st["hour"], st["epoch_days"], int(st["paused"]), st["rate"],
           st["scrub"], int(st["changes"]), bodies, why), flush=True)
  S.last_changes = ctrl.changes
  S.next_beacon = t + BEACON_SECS


###############################################################################
# lifecycle
###############################################################################


def onSystemInit(simulation):
  skytime = _skytime()
  control = skytime.SkyTimeControl()
  # THE PUBLISH — system init runs before any component update, so the first
  # celestial frame already sees the block and adopts the authored clock onto it.
  setattr(simulation.vars, skytime.CONTROL_KEY, control)
  simulation.vars.sky_time_state = SkyTimeState(control)
  print("[sky_time] live sky clock armed — keys: ] [ scrub · \\ pause · = - "
        "speed · ' ; day · 0 reset (scrub %.3g h/s)" % SCRUB_HOURS_PER_SEC,
        flush=True)


def onSystemUpdate(simulation):
  S = _state(simulation)
  t = simulation.gameTime
  # THE ONE-TICK WAIT. System scripts run BEFORE component scripts in a tick, so
  # the body angles in the block are always the ones the LAST tick's bodies
  # published. A line printed on the tick that notices a control op would
  # therefore pair the new hour with the OLD sun. Notice, let the bodies aim,
  # report next tick — the post-op state, whole.
  if S.pending:
    S.pending = False
    _emit(simulation, "changed")
  elif S.control.changes != S.last_changes:
    S.pending = True
  elif t >= S.next_beacon:
    _emit(simulation, "beacon")


###############################################################################
# the control surface — ONE implementation, driven by keys and by messages
###############################################################################


def _apply_scrub(simulation):
  """Held-scrub arbitration: sum the directions of every scrub key currently
  down, so pressing both cancels and releasing one of two keeps scrubbing."""
  S = _state(simulation)
  direction = sum(S.scrub_keys.values())
  S.control.set_scrub(simulation.gameTime, direction * SCRUB_HOURS_PER_SEC)


def _bump_rate(simulation, direction):
  S = _state(simulation)
  rate = S.control.rate * (RATE_STEP if direction > 0 else 1.0 / RATE_STEP)
  S.control.set_rate(simulation.gameTime, min(RATE_MAX, max(RATE_MIN, rate)))


def _onKey(simulation, key, down):
  action = KEYMAP.get(key)
  if action is None:
    return
  kind, arg = action
  S = _state(simulation)
  if kind == "scrub":
    if down:
      S.scrub_keys[key] = arg
    else:
      S.scrub_keys.pop(key, None)
    _apply_scrub(simulation)
    return
  if not down:      # every other binding acts on the PRESS edge only
    return
  if kind == "pause":
    S.control.toggle_paused(simulation.gameTime)
  elif kind == "rate":
    _bump_rate(simulation, arg)
  elif kind == "day":
    S.control.step_days(simulation.gameTime, arg)
  elif kind == "reset":
    S.control.reset(simulation.gameTime)


def _field(table, tok, evname, fieldname):
  """A control message's field, or a LOUD error naming the message and the
  field. A DataTable read of an absent key yields an empty value rather than
  raising, so an under-filled message would otherwise scrub the sky to zero."""
  try:
    val = float(table[tok])
  except Exception:
    raise KeyError("%s message is missing its required '%s' field "
                   "(send {tokens.%s: <float>})" % (evname, fieldname, fieldname))
  return val


def onSystemNotify(simulation, evID, table):
  h = evID.hashed
  S = _state(simulation)
  ctrl = S.control
  t = simulation.gameTime

  if h == tokens.InputKey.hashed:
    _onKey(simulation, table[tokens.key], table[tokens.down])
    return
  if h == tokens.SkyTimeSet.hashed:
    ctrl.set_hour(t, _field(table, tokens.hour, "SkyTimeSet", "hour"))
  elif h == tokens.SkyTimeSetDay.hashed:
    ctrl.set_day_of_year(t, _field(table, tokens.day, "SkyTimeSetDay", "day"))
  elif h == tokens.SkyTimeStep.hashed:
    ctrl.step_hours(t, _field(table, tokens.hours, "SkyTimeStep", "hours"))
  elif h == tokens.SkyTimeStepDay.hashed:
    ctrl.step_days(t, _field(table, tokens.days, "SkyTimeStepDay", "days"))
  elif h == tokens.SkyTimePause.hashed:
    p = int(_field(table, tokens.paused, "SkyTimePause", "paused"))
    if p < 0:
      ctrl.toggle_paused(t)
    else:
      ctrl.set_paused(t, p != 0)
  elif h == tokens.SkyTimeRate.hashed:
    ctrl.set_rate(t, _field(table, tokens.rate, "SkyTimeRate", "rate"))
  elif h == tokens.SkyTimeScrub.hashed:
    ctrl.set_scrub(t, _field(table, tokens.rate, "SkyTimeScrub", "rate"))
  elif h == tokens.SkyTimeReset.hashed:
    ctrl.reset(t)
  # NO telemetry from here. A control lands BETWEEN frames, so the bodies have
  # not re-aimed yet and a line printed now would carry the previous instant's
  # angles. Every op bumps control.changes, which makes the next
  # onSystemUpdate emit the full post-op state — one voice, never stale.
