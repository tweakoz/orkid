#!/usr/bin/env ork.python
###############################################################################
# GATE — the LIVE SKY CLOCK: does a running scene obey time-of-day commands?
#
# The controls under test (sky_time_system.py + _sky_time.py, read by every
# celestial body through _celestial_orbit.py) replace a LAUNCH-TIME workflow:
# until now the only way to see another hour was to author one (SWEST_TOD and
# its siblings) and relaunch. The claim is that hour, speed, pause and date are
# now RUNTIME state — and that the sun actually moves to where it is told.
#
# TWO HALVES, because the mechanism has two:
#
#   A. THE CLOCK ITSELF (in-process, no engine). SkyTimeControl is pure math, so
#      its semantics are checked directly — including the one property the whole
#      design rests on: an UNTOUCHED clock is BIT-IDENTICAL to the stateless
#      CelestialModel.at() every scene has always evaluated. That is what makes
#      shipping the control surface ON by default safe for the byte-identity
#      gates; assert it as equality, not as a tolerance.
#
#   B. THE LIVE SCENE (ork.ecs.player.exe, offscreen). A real scene
#      (scn_procsky — the lightest shipped celestial ensemble) is authored to an
#      .ecs and played, while --pysysnotify feeds the plan below on the SAME
#      controller-message channel a keyboard uses. Both surfaces are exercised:
#      SkyTime* messages (what a tool sends) AND raw InputKey transitions (what
#      the ']' scrub key and the '\' pause key are). The readback is the sim's
#      own telemetry line — where the sun IS, printed by the sky-time system —
#      and the sun's elevation is checked against an ephemeris this gate
#      evaluates INDEPENDENTLY from the config carried in the .ecs.
#
# WHY THE PLAYER AND NOT AN IN-PROCESS HOST: PythonSystem SYSTEM scripts wedge
# under a python host on this platform (an extra script deadlocks the update
# thread before its first tick; a primary one SIGSEGVs) — reproduced with a
# five-line script that does nothing, i.e. a pre-existing engine hazard, not
# this feature's. The C++ player releases the embedding GIL explicitly
# (main.cpp: PyEval_SaveThread) and is the proven host for scene scripts.
#
# WHAT MAKES THE LEGS UN-FAKEABLE: every control op bumps the block's `changes`
# counter and the telemetry carries it, so a leg's evidence is identified by the
# sim's own state rather than by log position. A clock that reported an hour
# without re-aiming the light fails the elevation check by tens of degrees; a
# pause that merely stopped PRINTING fails because the lines keep coming with an
# advancing sim time and an identical instant is what is asserted.
###############################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import json
import math
import re
import shutil
import subprocess
import sys
import time

sys.stdout.reconfigure(line_buffering=True)

_WS = os.path.abspath(__file__)
for _ in range(5):
  _WS = os.path.dirname(_WS)
sys.path.insert(0, os.path.join(_WS, "obt.project", "scripts"))

from ork.testing import verdict
from ork.hypergraph.ecs.scene import _celestial
from ork.hypergraph.ecs.scene import _sky_time
from ork.hypergraph.ecs.scene import sky_time_system as _skysys

SCENE = "scn_procsky"
TMP   = os.path.join(_WS, ".tmp")
ECS   = os.path.join(TMP, "sky_time_gate.ecs")

AUTHOR_TIMEOUT = 600
PLAY_TIMEOUT   = 240          # wall ceiling on the player leg
PLAY_SECONDS   = 62.0         # sim seconds the plan needs (the player is killed after)

SCRUB_KEY = 93                # ']' — from the script's own KEYMAP (asserted below)
PAUSE_KEY = 92                # '\'
FASTER_KEY = 61               # '='
RESET_KEY = 48                # '0'

# THE PLAN — (abstime, event, fields, bumps_the_change_counter). Times are the
# PLAYER's abstime (app start), the telemetry's is sim gameTime (simulation
# start); they differ by the boot, which is why nothing here is identified by
# time. Only presses bump the counter: the sky script acts on the press edge for
# everything except the held scrub, which acts on both edges.
PLAN = [
    (8.0,  "SkyTimeSet",    {"hour": 6.0},               True),   # change 1
    (13.0, "SkyTimeSet",    {"hour": 12.0},              True),   # change 2
    (18.0, "SkyTimeStepDay", {"days": 30.0},             True),   # change 3
    (23.0, "SkyTimePause",  {"paused": 1},               True),   # change 4
    (31.0, "InputKey",      {"key": PAUSE_KEY, "down": 1}, True), # change 5 (resume, by KEY)
    (31.1, "InputKey",      {"key": PAUSE_KEY, "down": 0}, False),
    (39.0, "InputKey",      {"key": SCRUB_KEY, "down": 1}, True), # change 6 (scrub on)
    (47.0, "InputKey",      {"key": SCRUB_KEY, "down": 0}, True), # change 7 (scrub off)
    (48.0, "InputKey",      {"key": FASTER_KEY, "down": 1}, True),# change 8 (rate x2)
    (48.1, "InputKey",      {"key": FASTER_KEY, "down": 0}, False),
    (57.0, "InputKey",      {"key": RESET_KEY, "down": 1}, True), # change 9 (reset)
    (57.1, "InputKey",      {"key": RESET_KEY, "down": 0}, False),
]

HOUR_TOL      = 0.05      # hours
ELEV_TOL_DEG  = 0.05      # the sim's sun vs the ephemeris at the SAME instant
CMD_ELEV_TOL  = 0.60      # ... vs the ephemeris at the COMMANDED hour
RATE_TOL_FRAC = 0.08

_TELEM = re.compile(
    r"\[sky_time\] t=(?P<t>[-\d.]+) hour=(?P<hour>[-\d.]+) day=(?P<day>[-\d.]+) "
    r"paused=(?P<paused>\d+) rate=(?P<rate>[-\d.eE+]+) scrub=(?P<scrub>[-\d.eE+]+) "
    r"changes=(?P<changes>\d+)(?P<bodies>[^(]*)\((?P<why>\w+)\)")
_SUN = re.compile(r"sun_el=([-\d.]+) sun_az=([-\d.]+)")


###############################################################################
# A. the clock itself — pure math, no engine
###############################################################################


def clock_checks():
  fails, notes = [], []

  model = _celestial.CelestialModel(latitude_deg=45.0, day_of_year=220.0,
                                    time_of_day=4.0, time_scale=480.0)
  ctrl = _sky_time.SkyTimeControl()
  ctrl.adopt(model)

  # THE LOAD-BEARING ONE: untouched, the live clock IS the authored clock, to
  # the bit — the whole reason this can ship on by default.
  worst = 0
  for t in (0.0, 0.5, 1.0, 7.25, 60.0, 123.456, 3600.0):
    live = ctrl.days_at(t)
    authored = model._epoch_days + float(t) * model.time_scale / 86400.0
    if live != authored:
      worst += 1
  if worst:
    fails.append("untouched clock differs from the authored clock at %d/7 times"
                 % worst)
  else:
    notes.append("untouched clock bit-identical to CelestialModel.at at 7 times")

  # set_hour lands on the hour, keeps the date
  ctrl.set_hour(10.0, 18.5)
  d = ctrl.days_at(10.0)
  if abs((d - math.floor(d)) * 24.0 - 18.5) > 1e-9:
    fails.append("set_hour(18.5) -> hour %.9f" % ((d - math.floor(d)) * 24.0))
  else:
    notes.append("set_hour exact")

  # pause holds the instant no matter how much sim time passes
  ctrl.set_paused(10.0, True)
  if ctrl.days_at(10.0) != ctrl.days_at(9999.0):
    fails.append("paused clock still advances")
  else:
    notes.append("pause holds the instant")

  # rate multiplies the AUTHORED speed, exactly
  ctrl.set_paused(10.0, False)
  ctrl.set_rate(10.0, 4.0)
  moved = (ctrl.days_at(20.0) - ctrl.days_at(10.0)) * 86400.0 / 10.0
  if abs(moved - 4.0 * 480.0) > 1e-6:
    fails.append("rate 4: clock runs %.6f sim-sec/sec, expected %.6f"
                 % (moved, 4.0 * 480.0))
  else:
    notes.append("rate multiplies the authored speed exactly")

  # scrub adds hours per second ON TOP, and survives a pause
  ctrl.set_rate(20.0, 1.0)
  ctrl.set_paused(20.0, True)
  ctrl.set_scrub(20.0, 2.0)
  hours = (ctrl.days_at(21.0) - ctrl.days_at(20.0)) * 24.0
  if abs(hours - 2.0) > 1e-9:
    fails.append("scrub 2 h/s while paused moved %.9f h in a second" % hours)
  else:
    notes.append("scrub works while paused (2 h/s)")

  # step_days moves the date and keeps the hour; reset returns everything
  ctrl.set_scrub(21.0, 0.0)
  before = ctrl.days_at(21.0)
  ctrl.step_days(21.0, 30.0)
  after = ctrl.days_at(21.0)
  if abs((after - before) - 30.0) > 1e-9:
    fails.append("step_days(30) moved %.9f days" % (after - before))
  else:
    notes.append("step_days exact")
  ctrl.reset(21.0)
  if ctrl.days_at(21.0) != model._epoch_days or ctrl.rate != 1.0 or ctrl.paused:
    fails.append("reset did not return the authored instant/state")
  else:
    notes.append("reset returns the authored instant exactly")

  # a negative rate is a mis-command, and must be loud rather than a still sky
  try:
    ctrl.set_rate(22.0, -1.0)
    fails.append("a negative rate was accepted silently")
  except ValueError:
    notes.append("negative rate refused loudly")

  # the keymap this gate drives is the keymap the script ships
  for key, kind in ((SCRUB_KEY, "scrub"), (PAUSE_KEY, "pause"),
                    (FASTER_KEY, "rate"), (RESET_KEY, "reset")):
    if _skysys.KEYMAP.get(key, (None,))[0] != kind:
      fails.append("keymap: %d is not the %s binding" % (key, kind))
  if not fails or all("keymap" not in f for f in fails):
    notes.append("keymap bindings match the shipped script")

  return fails, notes


###############################################################################
# B. the live scene
###############################################################################


def _scene_clock(ecs_path):
  """The site+clock the AUTHORED scene carries (PythonComponentData ScriptData),
  so the gate's expectations come from the .ecs and not from a copy of the
  scene's literals."""
  js = json.load(open(ecs_path))
  objs = js["root"]["object"]["properties"]["SceneObjects"]
  for _archname, archnode in objs.items():
    props = archnode.get("object", {}).get("properties", {})
    for _cname, cnode in (props.get("Components", {}) or {}).items():
      cprops = cnode.get("object", {}).get("properties", {})
      blob = cprops.get("ScriptData", "")
      if not blob:
        continue
      cfg = json.loads(blob)
      if cfg.get("body") == "sun":
        return _celestial.normalize_config(cfg)
  return None


def _run_player(script):
  exe = shutil.which("ork.ecs.player.exe")
  if not exe:
    return None, "ork.ecs.player.exe not on PATH"
  cmd = [exe, ECS, "--offscreen-forever", "--pysysnotify", script]
  print("[gate] %s" % " ".join(cmd[:3] + ["--pysysnotify", "<plan>"]), flush=True)
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, bufsize=1)
  lines = []
  t0 = time.time()
  last_change = sum(1 for e in PLAN if e[3])
  seen_last = 0.0
  while time.time() - t0 < PLAY_TIMEOUT:
    line = proc.stdout.readline()
    if not line:
      break
    lines.append(line)
    m = _TELEM.search(line)
    if m and int(m.group("changes")) >= last_change:
      # the final leg has landed; give it a couple of beacons, then stop
      if not seen_last:
        seen_last = time.time()
      elif time.time() - seen_last > 5.0:
        break
  proc.kill()
  try:
    proc.wait(timeout=30)
  except Exception:
    pass
  return "".join(lines), None


def _parse(text):
  buckets = {}
  for line in text.splitlines():
    m = _TELEM.search(line)
    if not m:
      continue
    rec = {"t": float(m.group("t")), "hour": float(m.group("hour")),
           "day": float(m.group("day")), "paused": int(m.group("paused")),
           "rate": float(m.group("rate")), "scrub": float(m.group("scrub")),
           "changes": int(m.group("changes"))}
    s = _SUN.search(m.group("bodies") or "")
    rec["sun_el"] = float(s.group(1)) if s else None
    buckets.setdefault(rec["changes"], []).append(rec)
  return buckets


def _slope(recs):
  """clock hours per sim second across a bucket (from epoch days, which never
  wraps the way an hour does)."""
  usable = [r for r in recs if r["sun_el"] is not None]
  if len(usable) < 2:
    return None
  dt = usable[-1]["t"] - usable[0]["t"]
  if dt <= 0.0:
    return None
  return (usable[-1]["day"] - usable[0]["day"]) * 24.0 / dt


def scene_checks(text, cfg):
  fails, notes = [], []
  buckets = _parse(text)
  model = _celestial.CelestialModel.from_config(cfg)
  base_hps = cfg["time_scale"] / 3600.0

  def bucket(n, label):
    recs = buckets.get(n)
    if not recs:
      fails.append("%s: no telemetry with changes=%d — the command never landed"
                   % (label, n))
    return recs

  def sun_agrees(rec, label):
    """the light is where the ephemeris puts it AT THE INSTANT the clock
    reports — the check a clock that lied to the sun cannot pass."""
    if rec["sun_el"] is None:
      fails.append("%s: no body angles in the telemetry" % label)
      return
    want = model.evaluate_days(rec["day"]).sun_elevation_deg
    if abs(rec["sun_el"] - want) > ELEV_TOL_DEG:
      fails.append("%s: sun at %.4f deg, ephemeris says %.4f at the reported "
                   "instant" % (label, rec["sun_el"], want))

  # ---- commanded hours -------------------------------------------------------
  for n, hour in ((1, 6.0), (2, 12.0)):
    recs = bucket(n, "set_hour %g" % hour)
    if not recs:
      continue
    r = recs[0]
    if abs(r["hour"] - hour) > HOUR_TOL:
      fails.append("set_hour %g: the clock reports %.4f" % (hour, r["hour"]))
    sun_agrees(r, "set_hour %g" % hour)
    want_cmd = model.evaluate_days(math.floor(r["day"]) + hour / 24.0).sun_elevation_deg
    if r["sun_el"] is not None and abs(r["sun_el"] - want_cmd) > CMD_ELEV_TOL:
      fails.append("set_hour %g: sun at %.3f deg, the commanded hour is %.3f"
                   % (hour, r["sun_el"], want_cmd))
    notes.append("set_hour %g -> hour %.4f sun_el %.3f (commanded %.3f)"
                 % (hour, r["hour"], r["sun_el"] or -999.0, want_cmd))

  # ---- the date --------------------------------------------------------------
  before = buckets.get(2)
  after = bucket(3, "step_day 30")
  if before and after:
    b, a = before[-1], after[0]
    jump = a["day"] - b["day"]
    if not (29.5 < jump < 30.5):
      fails.append("step_day 30: the date moved %.4f days" % jump)
    if abs(a["hour"] - b["hour"]) > 0.2:
      fails.append("step_day 30: the hour moved (%.4f -> %.4f)"
                   % (b["hour"], a["hour"]))
    sun_agrees(a, "step_day 30")
    notes.append("step_day 30 -> +%.4f days, hour %.4f -> %.4f, sun_el %.3f"
                 % (jump, b["hour"], a["hour"], a["sun_el"] or -999.0))

  # ---- pause -----------------------------------------------------------------
  recs = bucket(4, "pause")
  if recs:
    if len(recs) < 3:
      fails.append("pause: only %d telemetry line(s) — the sim stopped ticking, "
                   "so a frozen clock would prove nothing" % len(recs))
    else:
      spread = max(r["day"] for r in recs) - min(r["day"] for r in recs)
      dt = recs[-1]["t"] - recs[0]["t"]
      if spread != 0.0:
        fails.append("pause: the clock moved %.9g days while held" % spread)
      if dt <= 1.0:
        fails.append("pause: sim time advanced only %.3f s — nothing was held" % dt)
      if any(r["paused"] != 1 for r in recs):
        fails.append("pause: the block does not report itself held")
      # EXACTLY one elevation: zero of them would be a leg passing on absent
      # evidence (a consumer that stopped publishing looks "frozen" too).
      els = set(r["sun_el"] for r in recs if r["sun_el"] is not None)
      if len(els) != 1:
        fails.append("pause: expected ONE sun elevation across the held window, "
                     "got %d (%s)" % (len(els), sorted(els)))
      notes.append("pause: %d lines over %.2f sim s, clock spread %g, one sun "
                   "elevation" % (len(recs), dt, spread))

  # ---- resume BY KEY + the baseline speed ------------------------------------
  recs = bucket(5, "resume key")
  hps_base = _slope(recs or [])
  if recs:
    if recs[0]["paused"] != 0:
      fails.append("resume key: still held")
    if hps_base is None:
      fails.append("resume key: not enough telemetry to measure the base speed")
    elif abs(hps_base - base_hps) > RATE_TOL_FRAC * base_hps:
      fails.append("resume key: clock runs %.5f h/s, the scene declares %.5f"
                   % (hps_base, base_hps))
    else:
      notes.append("resume by key [\\] -> running at %.5f h/s (declared %.5f)"
                   % (hps_base, base_hps))

  # ---- the scrub KEY ---------------------------------------------------------
  recs = bucket(6, "scrub key down")
  hps_scrub = _slope(recs or [])
  if recs:
    want = _skysys.SCRUB_HOURS_PER_SEC + base_hps
    if abs(recs[0]["scrub"] - _skysys.SCRUB_HOURS_PER_SEC) > 1e-6:
      fails.append("scrub key: the block reports scrub %g" % recs[0]["scrub"])
    if hps_scrub is None:
      fails.append("scrub key: not enough telemetry to measure a slope")
    elif abs(hps_scrub - want) > RATE_TOL_FRAC * want:
      fails.append("scrub key: clock runs %.5f h/s, expected %.5f (scrub + "
                   "authored)" % (hps_scrub, want))
    else:
      notes.append("scrub key []] held -> %.5f h/s (want %.5f)" % (hps_scrub, want))
    if recs[-1]["sun_el"] is not None:
      sun_agrees(recs[-1], "scrub key")

  recs = bucket(7, "scrub key up")
  if recs and abs(recs[0]["scrub"]) > 1e-6:
    fails.append("scrub key release: the scrub did not stop (%g)" % recs[0]["scrub"])
  elif recs:
    notes.append("scrub key released -> scrub 0")

  # ---- the speed KEY ---------------------------------------------------------
  recs = bucket(8, "faster key")
  hps_fast = _slope(recs or [])
  if recs:
    if abs(recs[0]["rate"] - _skysys.RATE_STEP) > 1e-6:
      fails.append("faster key: rate is %g, one step is %g"
                   % (recs[0]["rate"], _skysys.RATE_STEP))
    if hps_fast is None or hps_base is None:
      fails.append("faster key: not enough telemetry to measure the ratio")
    else:
      ratio = hps_fast / hps_base if hps_base else 0.0
      if abs(ratio - _skysys.RATE_STEP) > RATE_TOL_FRAC * _skysys.RATE_STEP:
        fails.append("faster key: %.5f h/s vs baseline %.5f = ratio %.3f, one "
                     "step is %g" % (hps_fast, hps_base, ratio, _skysys.RATE_STEP))
      else:
        notes.append("faster key [=] -> %.5f h/s (ratio %.3f)" % (hps_fast, ratio))

  # ---- reset -----------------------------------------------------------------
  recs = bucket(9, "reset key")
  if recs:
    r = recs[0]
    if abs(r["hour"] - cfg["time_of_day"]) > HOUR_TOL:
      fails.append("reset key: hour %.4f, the scene was authored at %.4f"
                   % (r["hour"], cfg["time_of_day"]))
    if r["paused"] != 0 or abs(r["rate"] - 1.0) > 1e-6 or abs(r["scrub"]) > 1e-6:
      fails.append("reset key: state not authored (paused=%d rate=%g scrub=%g)"
                   % (r["paused"], r["rate"], r["scrub"]))
    sun_agrees(r, "reset key")
    notes.append("reset key [0] -> hour %.4f rate %g (authored %.4f)"
                 % (r["hour"], r["rate"], cfg["time_of_day"]))

  return fails, notes


###############################################################################


def main():
  fails, notes = clock_checks()

  log_arg = None
  for i, a in enumerate(sys.argv):
    if a == "--log" and i + 1 < len(sys.argv):
      log_arg = sys.argv[i + 1]

  os.makedirs(TMP, exist_ok=True)
  if not log_arg:
    rc = subprocess.call(["ork.scene.tojson.py", "-i", SCENE, "-o", ECS],
                         timeout=AUTHOR_TIMEOUT)
    if rc != 0:
      for n in notes:
        print("[gate] %s" % n, flush=True)
      return verdict(False, detail="authoring %s failed rc=%d" % (SCENE, rc))

  cfg = _scene_clock(ECS)
  if cfg is None:
    fails.append("the authored .ecs carries no celestial sun config")
    cfg = _celestial.normalize_config({})

  if log_arg:
    text = open(log_arg).read()
  else:
    script = ";".join("%g:%s%s" % (t, ev,
                                   (":" + ",".join("%s=%g" % (k, v)
                                                   for k, v in f.items())) if f else "")
                      for (t, ev, f, _bump) in PLAN)
    text, err = _run_player(script)
    if err:
      return verdict(False, detail=err)
    open(os.path.join(TMP, "sky_time_controls_player.log"), "w").write(text)

  sfails, snotes = scene_checks(text, cfg)
  fails += sfails
  notes += snotes

  for n in notes:
    print("[gate] %s" % n, flush=True)
  for f in fails:
    print("[gate] FAIL %s" % f, flush=True)
  return verdict(not fails,
                 detail=("%d checks pass, %d fail%s"
                         % (len(notes), len(fails),
                            "" if not fails else " :: " + " | ".join(fails))))


if __name__ == "__main__":
  sys.exit(main())
