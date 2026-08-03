#!/usr/bin/env ork.python
################################################################################
# Sky IBL CHAIN CADENCE CEILING gate (SkyAtmosphereData.ibl_chain_max_hz).
#
# Continuous chaining paces itself on the crossfade settling plus a minimum sun
# step, and neither of those is a RATE: on a fast day cycle over cheap frames
# the feed starts a new refilter cycle as often as the frame loop physically
# allows (measured ~4.5/s at 2560x1280 offscreen on an RTX 5090, ~10/s before
# the refilter was actually spread across frames). ibl_chain_max_hz is the
# ceiling for that, in Hz, start-to-start. 0 = uncapped; the shipped default is
# a measured 2 Hz, so this gate sets its uncapped window explicitly.
#
# SELF-CALIBRATING, because "how many cycles per second does this machine
# manage" is not a constant: window A measures the UNCAPPED rate on the machine
# running the gate, then the ceiling is set to a third of that and window B
# measures again. Asserts
#
#   a) CEILING   the capped rate stays at or under the ceiling (small tolerance
#                for the window boundaries — a cycle can start on the first
#                frame of the window having been permitted by the previous one),
#   b) STILL RUNS the capped feed keeps completing cycles: a rate limiter that
#                stops the feed is a broken feed, not a paced one,
#   c) EFFECTIVE the capped rate is materially below the uncapped one measured
#                on THIS machine moments earlier — the knob has to do something.
#
# The sun sweeps a fixed step every frame so the chain's angle floor is never
# what is limiting; the only thing between the feed and its maximum rate is the
# ceiling under test.
#
# Not ork.testing capture_app: no capture, and the gate needs a two-window state
# machine inside one warm process. The verdict-before-teardown protocol (#57) is
# honoured.
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

WINDOW_SECS   = float(os.environ.get("ORKID_IBL_RATE_WINDOW", "6.0"))
WARMUP_CYCLES = 2      # cold JIT + the first publish are not a cadence measurement
WAIT_SECONDS  = 120.0

SUN_ELEV_DEG    = 20.0
SUN_STEP_DEG    = 0.25 # per frame — far past ibl_chain_min_angle_deg every frame

# The ceiling is set to UNCAPPED/CAP_DIVISOR, so the two windows are separated
# by a factor the frame-rate noise between them cannot close.
CAP_DIVISOR   = 3.0
# A window boundary can carry one already-permitted cycle, and the sampling is
# wall-clock over a handful of cycles, so the ceiling check gets one cycle plus
# 25% of slack. Tight enough that an unenforced ceiling (rate == uncapped ==
# 3x the cap) fails by a mile.
CEILING_SLACK = 1.25


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class ChainRateApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_time = time.time()
    self._done = False
    self._azimuth = 0.0
    self._marks = {}
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
    self.atmo.ibl_continuous_chain = True   # the mode the ceiling governs
    self.atmo.ibl_chain_max_hz     = 0.0    # window A: uncapped (NOT the default)
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

  def _cycles(self):
    return int(self._pbr().sky_ibl_cycles_started)

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return

    # the sun never stops moving: the angle floor must never be the limiter.
    self._azimuth += SUN_STEP_DEG
    self._aimSun(self._azimuth)

    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_time = time.time()
      return

    now = time.time()

    ########################################
    # 0: warm up — the first cycles carry the process's one-time JIT/BRDF costs
    ########################################
    if self._phase == 0:
      if self._cycles() >= WARMUP_CYCLES:
        self._marks["a_cycles"] = self._cycles()
        self._marks["a_time"] = now
        self._phase = 1
      elif (now - self._phase_time) > WAIT_SECONDS:
        self._fail("feed never started %d cycles to warm up" % WARMUP_CYCLES)
      return

    ########################################
    # 1: window A — uncapped, on THIS machine
    ########################################
    if self._phase == 1:
      if (now - self._marks["a_time"]) < WINDOW_SECS:
        return
      span = now - self._marks["a_time"]
      started = self._cycles() - self._marks["a_cycles"]
      self._marks["rate_uncapped"] = started / span
      if started <= 0:
        self._fail("uncapped chaining started no cycles in %.1f s" % span)
        return
      cap = self._marks["rate_uncapped"] / CAP_DIVISOR
      self._marks["cap_hz"] = cap
      self.atmo.ibl_chain_max_hz = cap
      print("[chainrate] uncapped=%.2f cycles/s over %.2fs -> ceiling %.3f Hz" %
            (self._marks["rate_uncapped"], span, cap), flush=True)
      # the running cycle finishes under the new ceiling before window B opens,
      # so B never counts a cycle the uncapped policy permitted.
      self._marks["b_pending"] = True
      self._phase = 2
      return

    ########################################
    # 2: let one cycle pass under the ceiling, then open window B
    ########################################
    if self._phase == 2:
      if self._marks["b_pending"]:
        self._marks["b_at"] = self._cycles() + 1
        self._marks["b_pending"] = False
      if self._cycles() < self._marks["b_at"]:
        return
      self._marks["b_cycles"] = self._cycles()
      self._marks["b_time"] = now
      self._phase = 3
      return

    ########################################
    # 3: window B — capped
    ########################################
    if self._phase == 3:
      if (now - self._marks["b_time"]) < WINDOW_SECS:
        return
      span = now - self._marks["b_time"]
      started = self._cycles() - self._marks["b_cycles"]
      self._marks["rate_capped"] = started / span
      self._marks["b_span"] = span
      self._marks["b_started"] = started
      self._emitVerdict()
      return

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

    m = self._marks
    cap = m["cap_hz"]
    uncapped = m["rate_uncapped"]
    capped = m["rate_capped"]
    # one window-boundary cycle plus the sampling slack
    ceiling = cap * CEILING_SLACK + (1.0 / m["b_span"])

    print("=== chain cadence ceiling ===", flush=True)
    print("  uncapped=%.3f cycles/s | ceiling=%.3f Hz | capped=%.3f cycles/s (%d in %.2fs)" %
          (uncapped, cap, capped, m["b_started"], m["b_span"]), flush=True)

    check("ceiling_respected", capped <= ceiling,
          "capped=%.3f/s allowed=%.3f/s (ceiling %.3f Hz)" % (capped, ceiling, cap))
    check("feed_still_running", m["b_started"] > 0,
          "%d cycles started under the ceiling in %.2fs" % (m["b_started"], m["b_span"]))
    check("ceiling_is_effective", capped < (uncapped * 0.75),
          "capped=%.3f/s vs uncapped=%.3f/s" % (capped, uncapped))

    ok = (len(failures) == 0)
    detail = ("uncapped=%.3f/s ceiling=%.3fHz capped=%.3f/s cycles=%d window=%.2fs" %
              (uncapped, cap, capped, m["b_started"], m["b_span"]))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = ChainRateApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
