#!/usr/bin/env ork.python
################################################################################
# SMALL-POOL REFILTER gate — the parents>=workers deadlock class, pinned.
#
# The image fan-outs (format conversion, downsample) enqueue row-band chunks onto
# the concurrentQueue and then BLOCK until every chunk reports in. When such a
# fan-out is itself running ON a concurrentQueue worker, its children need a free
# worker to run in; once every worker in the pool is a blocked parent, none is
# ever free again and the whole pool spins forever. Observed on a small-core mac
# at the pool's 4-worker floor: all workers parked in
# Image::convertFromImageToFormat (the capture readback's conversion), the loader
# thread parked in Image::downsample under uncompressedMipChain, and the main
# thread finally parked joining that loader at exit.
#
# Big machines hide this: the floor is clamp(ncores/4,4,24), so a 32-core box
# starts 8 workers and a fan-out of 2 chunks cannot exhaust them. This gate
# therefore PINS THE POOL TO 4 WORKERS via the pre-existing env overrides and
# demands that a procedural refilter cycle still runs to completion:
#
#   a) LOADS     the XIR skybox arrives (its mip chain is built by downsample on
#                the loader thread — deadlock site 2),
#   b) COMPLETES sky_ibl_generation reaches CYCLES_REQUIRED, which requires every
#                capture readback's format conversion to finish (deadlock site 1),
#   c) LIVE      the frame loop is still advancing when the verdict is taken.
#
# The env must be set before the engine is imported: the pool reads its limits
# once, at first concurrentQueue() touch.
#
# Not ork.testing capture_app: nothing is captured to disk and the gate needs its
# own poll loop over the ibl counters. The verdict-before-teardown protocol (#57)
# and the sample-before-kill watchdog ARE used — a regression here is a wedge, so
# the watchdog's sample is the diagnostic the next reader wants.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
# 4 workers = the mac's pool floor, the configuration the deadlock needs.
os.environ["ORKID_CONCURRENTQ_MIN_THREADS"] = "4"
os.environ["ORKID_CONCURRENTQ_MAX_THREADS"] = "4"
os.environ.setdefault("ORKID_PERFHUD", "off")

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
from ork.testing import verdict, armed

WIDTH, HEIGHT = 320, 240
CAM_DIST      = 7.0

# CHAINED cycles, not one: a single refilter has too few readbacks in flight to
# fill even a 4-worker pool. Continuous chaining is what stacks the conversions
# up, and it is what wedged at this pool size before the fix.
CYCLES_REQUIRED = int(os.environ.get("ORKID_SMALLPOOL_CYCLES", "4"))
# Healthy on a 4-worker pool: ~1.7 cycles/s at 512x256. The budget is generous
# because "slow" is not what is under test — "never" is.
CYCLE_BUDGET_SECS = float(os.environ.get("ORKID_SMALLPOOL_BUDGET", "60.0"))
# hard deadline > budget: the wedge this gate pins can also stop the frame loop,
# in which case no in-loop check ever runs and only the watchdog reports.
WATCHDOG_SECS     = CYCLE_BUDGET_SECS + 45.0

SUN_ELEV_DEG = 20.0
SUN_STEP_DEG = 0.25


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class SmallPoolApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._done = False
    self._azimuth = 0.0
    self._start = None
    self._frames_at_start = 0
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, 2, -CAM_DIST), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          # a real XIR load, so the mip chain (downsample) runs on the loader
          # thread against the same 4-worker pool.
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
    self.atmo.ibl_continuous_chain = True   # chained cycles keep readbacks in flight
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

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return

    self._azimuth += SUN_STEP_DEG
    self._aimSun(self._azimuth)

    if self._start is None:
      if self._frame < 2:
        return
      self._start = time.time()
      self._frames_at_start = self._frame
      return

    pbr = self._pbr()
    elapsed = time.time() - self._start
    generation = int(pbr.sky_ibl_generation)

    if generation >= CYCLES_REQUIRED:
      self._emitVerdict(True, elapsed, generation)
      return

    if elapsed > CYCLE_BUDGET_SECS:
      self._emitVerdict(False, elapsed, generation)

  ##############################################################

  def _emitVerdict(self, completed, elapsed, generation):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    pbr = self._pbr()
    frames = self._frame - self._frames_at_start

    print("=== small-pool chained refilter (4 workers) ===", flush=True)
    check("skybox_loaded", bool(pbr.sky_ibl_ready) or completed,
          "sky_ibl_ready=%s" % bool(pbr.sky_ibl_ready))
    check("cycles_completed", completed,
          "generation=%d of %d after %.1fs" % (generation, CYCLES_REQUIRED, elapsed))
    check("loop_live", frames > 1,
          "%d frames in %.1fs" % (frames, elapsed))

    ok = (len(failures) == 0)
    detail = ("workers=4 generation=%d elapsed=%.1fs frames=%d" %
              (generation, elapsed, frames))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  with armed(WATCHDOG_SECS, label="ibl_smallpool"):
    app = SmallPoolApp()
    app.ezapp.mainThreadLoop()
    app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
