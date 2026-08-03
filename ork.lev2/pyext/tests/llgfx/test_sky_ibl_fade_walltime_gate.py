#!/usr/bin/env ork.python
################################################################################
# Sky IBL CROSSFADE WALL-CLOCK BOUND gate (SkyAtmosphereData.ibl_crossfade_max_secs).
#
# The publish crossfade was sized in FRAMES only — a fixed window, or in chaining
# mode the measured frame span of the last cycle. Frames are not a duration: on a
# fast offscreen loop the same window is milliseconds and on a slow one it is
# seconds, so "how long does the lighting stay half-blended after a republish"
# depended entirely on the frame rate. ibl_crossfade_max_secs is the wall-clock
# bound on that window; the fade ends at whichever bound lands FIRST.
#
# The gate runs the SAME fade at two frame rates inside one warm process (the
# slow rate is a real per-frame sleep, so it is the frame loop that is slow, not
# a simulated clock) and asserts
#
#   a) DEFAULT      an undeclared atmosphere ships the 0.5 s budget,
#   b) DEFECT       with the bound OFF (0 = frames only) the slow-loop fade runs
#                   well past the budget — the behaviour this knob exists for,
#   c) BOUND SLOW   with the budget declared, the same fade finishes inside it,
#   d) BOUND FAST   and so does the fast-loop fade,
#   e) NOT FRAMES   while those two fades span wildly different FRAME counts —
#                   the point of the knob, stated as a measurement,
#   f) OVERRIDE     a declared budget below the default binds at its own value,
#   g) HARD SWAP    ibl_crossfade_frames = 0 still disables the fade outright
#                   (weight pinned at 1.0 across a publish), which no budget may
#                   resurrect.
#
# Chaining is OFF and the sun is parked: a republish is forced by one sun nudge
# past the refilter angle, so exactly one cycle (and one fade) happens per phase
# and nothing starts a second one underneath the measurement.
#
# Not ork.testing capture_app: no capture, and the gate is a phase machine over
# one warm process. The verdict-before-teardown protocol (#57) is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing
# resolves from the same tree as this test (worktree-shadowing idiom).
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

import math
import time
from orkengine.core import vec3
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

WIDTH, HEIGHT = 320, 240
CAM_DIST      = 7.0

# the shipped default this gate also pins
DEFAULT_BUDGET = 0.5
# the declared override phase (f); materially under the default
OVERRIDE_BUDGET = 0.15

# The frame window every phase runs with: big enough that on a THROTTLED loop it
# overruns the budget by more than any scheduling noise (120 frames * 10 ms).
FADE_FRAMES = 120
SLOW_SLEEP  = 0.010   # per-frame sleep => ~100 fps, the "slow loop"

# One throttled frame of quantisation (the weight is sampled per frame) plus 25%.
def _slack(budget):
  return budget * 0.25 + SLOW_SLEEP * 2.0

SUN_ELEV_DEG  = 20.0
SUN_NUDGE_DEG = 4.0    # > ibl_refilter_angle_deg (2 deg) => one cycle per nudge
WAIT_SECONDS  = 60.0   # per-phase watchdog


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


# phase table: (name, budget_secs, fade_frames, throttled)
PHASES = [
  ("frames_only_slow", 0.0,             FADE_FRAMES, True),
  ("bounded_slow",     DEFAULT_BUDGET,  FADE_FRAMES, True),
  ("bounded_fast",     DEFAULT_BUDGET,  FADE_FRAMES, False),
  ("override_slow",    OVERRIDE_BUDGET, FADE_FRAMES, True),
  ("hard_swap",        DEFAULT_BUDGET,  0,           False),
]


class FadeWallTimeApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._done = False
    self._azimuth = 0.0
    self._phase = -1          # -1 = waiting for the first publish
    self._phase_time = time.time()
    self._measuring = False
    self._throttle = False
    self._results = {}
    self._default_budget_seen = None
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, 2, -CAM_DIST), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    # (a) the UNDECLARED value, read before anything on this object is set
    self._default_budget_seen = float(self.atmo.ibl_crossfade_max_secs)
    self.atmo.ibl_continuous_chain = False  # one cycle per sun nudge, no chaining
    self.atmo.ibl_crossfade_frames = FADE_FRAMES
    # cheap cycles: the gate measures the FADE, never the filter quality, and a
    # short cycle keeps the whole phase machine inside the run budget.
    self.atmo.ibl_snapshot_width   = 256
    self.atmo.ibl_snapshot_height  = 128
    self.atmo.ibl_specular_samples = 256
    self.atmo.ibl_slices_per_frame = 64
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(0.0)
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass

  ##############################################################

  def _aimSun(self, azimuth_deg):
    d = dir_to_sun(azimuth_deg, SUN_ELEV_DEG)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _pbr(self):
    return self.SGC.pbr_common

  def _generation(self):
    return int(self._pbr().sky_ibl_generation)

  def _weight(self):
    return float(self._pbr().sky_ibl_fade_weight)

  ##############################################################

  def _startPhase(self, index):
    name, budget, frames, throttled = PHASES[index]
    self._phase = index
    self._throttle = throttled
    self._phase_time = time.time()
    self.atmo.ibl_crossfade_max_secs = budget
    self.atmo.ibl_crossfade_frames = frames
    self._gen_at_nudge = self._generation()
    self._azimuth += SUN_NUDGE_DEG    # the republish trigger, once
    self._aimSun(self._azimuth)
    self._measuring = False
    print("[fadeclock] phase %s budget=%.3fs frames=%d throttled=%s" %
          (name, budget, frames, throttled), flush=True)

  def _finishPhase(self, secs, frames, weights):
    name = PHASES[self._phase][0]
    self._results[name] = {
      "secs": secs,
      "frames": frames,
      "weight_min": min(weights) if weights else 1.0,
      "weight_end": weights[-1] if weights else 1.0,
    }
    print("[fadeclock]   %s: fade %.3f s over %d frames (min weight %.3f)" %
          (name, secs, frames, self._results[name]["weight_min"]), flush=True)
    if (self._phase + 1) < len(PHASES):
      self._startPhase(self._phase + 1)
    else:
      self._emitVerdict()

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if self._throttle:
      time.sleep(SLOW_SLEEP)

    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_time = time.time()
      return

    now = time.time()
    if (now - self._phase_time) > WAIT_SECONDS:
      self._fail("phase %d stalled (generation %d)" % (self._phase, self._generation()))
      return

    ########################################
    # warm-up: the FIRST publish has no outgoing set, so it never fades
    ########################################
    if self._phase < 0:
      if self._generation() >= 1:
        self._startPhase(0)
      return

    ########################################
    # a phase: wait for the publish this phase's nudge triggered, then sample
    # the fade weight every frame until it settles at 1.0
    ########################################
    if not self._measuring:
      if self._generation() <= self._gen_at_nudge:
        return
      self._measuring = True
      self._t_publish = now
      self._f_publish = self._frame
      self._weights = []
      # the hard-swap phase has no fade to wait for: sample a fixed window and
      # assert the weight never leaves 1.0
      self._swap_check = (PHASES[self._phase][2] == 0)

    w = self._weight()
    self._weights.append(w)
    if self._swap_check:
      if len(self._weights) >= 30:
        self._finishPhase(now - self._t_publish, self._frame - self._f_publish, self._weights)
      return
    if w >= 1.0:
      self._finishPhase(now - self._t_publish, self._frame - self._f_publish, self._weights)

  ##############################################################

  def _fail(self, why):
    print("FAIL " + why, flush=True)
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    r = self._results
    unbounded = r["frames_only_slow"]
    slow      = r["bounded_slow"]
    fast      = r["bounded_fast"]
    override  = r["override_slow"]
    swap      = r["hard_swap"]

    print("=== IBL crossfade wall-clock bound ===", flush=True)
    for name, _, _, _ in PHASES:
      d = r[name]
      print("  %-16s %6.3f s / %4d frames (min weight %.3f)" %
            (name, d["secs"], d["frames"], d["weight_min"]), flush=True)

    check("default_is_half_second",
          abs(self._default_budget_seen - DEFAULT_BUDGET) < 1e-6,
          "undeclared ibl_crossfade_max_secs = %.3f" % self._default_budget_seen)
    check("frames_only_overruns_budget",
          unbounded["secs"] > (DEFAULT_BUDGET * 1.5),
          "%.3f s with the bound off vs a %.3f s budget" % (unbounded["secs"], DEFAULT_BUDGET))
    check("budget_bounds_slow_loop",
          slow["secs"] <= (DEFAULT_BUDGET + _slack(DEFAULT_BUDGET)),
          "%.3f s <= %.3f s" % (slow["secs"], DEFAULT_BUDGET + _slack(DEFAULT_BUDGET)))
    check("budget_bounds_fast_loop",
          fast["secs"] <= (DEFAULT_BUDGET + _slack(DEFAULT_BUDGET)),
          "%.3f s <= %.3f s" % (fast["secs"], DEFAULT_BUDGET + _slack(DEFAULT_BUDGET)))
    check("frames_are_not_the_unit",
          fast["frames"] >= (slow["frames"] * 2),
          "%d frames fast vs %d slow, both inside the same budget" %
          (fast["frames"], slow["frames"]))
    check("declared_override_binds",
          (override["secs"] <= (OVERRIDE_BUDGET + _slack(OVERRIDE_BUDGET))) and
          (override["secs"] < (slow["secs"] * 0.75)),
          "%.3f s at a %.3f s declaration vs %.3f s at the default" %
          (override["secs"], OVERRIDE_BUDGET, slow["secs"]))
    check("hard_swap_preserved",
          swap["weight_min"] >= 1.0,
          "min weight %.3f across %d frames after a publish with frames=0" %
          (swap["weight_min"], swap["frames"]))

    ok = (len(failures) == 0)
    detail = ("unbounded=%.3fs bounded_slow=%.3fs(%df) bounded_fast=%.3fs(%df) "
              "override=%.3fs hardswap_minw=%.3f" %
              (unbounded["secs"], slow["secs"], slow["frames"],
               fast["secs"], fast["frames"], override["secs"], swap["weight_min"]))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = FadeWallTimeApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
