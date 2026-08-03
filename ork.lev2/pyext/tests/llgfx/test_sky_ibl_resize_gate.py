#!/usr/bin/env ork.python
################################################################################
# Gate — the IBL snapshot extent is a LIVE knob: editing IblSnapshotWidth/Height
# between cycles reallocates the equirect target and the feed keeps publishing.
#
# The extent decides the whole feed's per-slice cost, so it is the one IBL knob a
# scene is most likely to move at runtime (a quality preset, a VR-vs-desktop
# branch). The realloc is only legal because renderEquirectSnapshot runs at CYCLE
# START, behind the _inflight gate — no prefilter job can be sampling the old
# target at that moment, and the crossfade blends published RADIANCE MAPS rather
# than the snapshot, so a resize can never land under a live fade either. This
# gate is what holds that construction in place. Asserts:
#
#   a) DEFAULT   the shipped extent is what the engine actually snapshots at
#                (a silent default drift is a silent perf change on every
#                procedural-sky scene).
#   b) RESIZE    an extent edit between cycles is picked up by the NEXT cycle:
#                the snapshot RTG comes back at the new extent, and the maps
#                publish (generation advances) rather than hanging or dropping.
#   c) SURVIVES  the receiver stays lit across the resize — the dual-bind fade
#                never sampled a target that was being reallocated under it.
#
# The shrink direction is the one measured here because it is the one that frees
# the old target while the previous cycle's shared_ptr may still name it.
#
# Not ork.testing capture_app: the gate needs a multi-cycle state machine with
# waits on engine-driven async completion inside one warm process. The
# verdict-before-teardown protocol (#57) is honoured.
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
import numpy
from orkengine.core import vec3, vec4
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

WIDTH, HEIGHT = 320, 240
CAM_DIST      = 7.0
BALL_POS      = vec3(0, 2, 0)

# the shipped extent, and the one the mid-run edit moves to. RESIZED is the
# pre-COMFORT-2 default, i.e. the budget escape hatch this knob exists for.
DEFAULT_WH = (512, 256)
RESIZED_WH = (256, 128)

SUN_ELEV_DEG = 20.0
SUN_AZIM_A   = 0.0
SUN_AZIM_B   = 25.0   # well past the refilter threshold

# SETTLES ARE IN FRAMES, WAITS ARE IN SECONDS (see test_sky_ibl_gate for why).
STEP_SETTLE      = 8
WAIT_SECONDS     = 120.0
MAX_CYCLE_FRAMES = 20000


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class ResizeApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._phase_time = time.time()
    self._done = False
    self._marks = {}
    self._inflight = None
    self._shots = {}
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
    # the sky itself stays out of frame: this gate reads the IBL off a receiver.
    SGC.pbr_common.enable_skybox = False

    self.atmo = lev2.SkyAtmosphereData()
    self._marks["default_wh"] = (self.atmo.ibl_snapshot_width, self.atmo.ibl_snapshot_height)
    # the sky-view LUT sits near 2e-2 mid-sky at this elevation; the exposure
    # lifts a lit ball clear of an 8-bit capture's noise floor.
    self.atmo.sky_exposure = 40.0
    # chaining OFF: this gate wants exactly one cycle per sun move, so that the
    # extent edit lands strictly BETWEEN cycles.
    self.atmo.ibl_continuous_chain = False
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self.ball = SGC.createBallNode("recv", ctx=ctx, position=BALL_POS,
                                   color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0,
                                   scale=1.2)

    # DIRECTION SOURCE ONLY (intensity 0): every photon in frame is image-based,
    # while the prologue still bakes the LUT the snapshot resamples.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_AZIM_A)
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

  def _note(self, txt):
    print("[ibl-resize] %s" % txt, flush=True)

  ##############################################################
  # capture plumbing
  ##############################################################

  def _rtg(self, ctx):
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _issueCapture(self, ctx, key):
    buf = lev2.CaptureBuffer()
    fut = ctx.FBI.captureAsFormat(self._rtg(ctx).buffer(0), buf, "RGBA8")
    self._inflight = (fut, buf, key)

  def _collect(self):
    fut, buf, key = self._inflight
    if not bool(fut.is_ready):
      return None
    w, h = buf.width, buf.height
    img = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3].copy()
    self._shots[key] = img
    self._inflight = None
    return key

  def _ballMean(self, key):
    """mean of a box over the center ball, sized from the capture (the viewport
    RTG is inset from the requested extent)."""
    img = self._shots[key]
    h, w, _ = img.shape
    half = int(h * 0.14)
    return float(img[h // 2 - half:h // 2 + half, w // 2 - half:w // 2 + half].mean())

  def _snapshotExtent(self):
    rtg = self._pbr().sky_ibl_snapshot_rtgroup
    if rtg is None:
      return None
    tex = rtg.buffer(0).texture
    return (int(tex.width), int(tex.height))

  ##############################################################
  # phases
  ##############################################################

  def _phases(self):
    return [
        # 0: cycle 1 at the shipped extent
        dict(name="default", wait=lambda: int(self._pbr().sky_ibl_generation) >= 1,
             after=self._resizeAndMoveSun),
        # 1: cycle 2, after the live extent edit
        dict(name="resized", wait=lambda: int(self._pbr().sky_ibl_generation) >= 2,
             after=None),
    ]

  def _resizeAndMoveSun(self):
    self.atmo.ibl_snapshot_width = RESIZED_WH[0]
    self.atmo.ibl_snapshot_height = RESIZED_WH[1]
    self._marks["f_trigger2"] = self._frame
    self._marks["t_trigger2"] = time.time()
    self._aimSun(SUN_AZIM_B)
    self._note("extent -> %dx%d, sun -> azimuth %.1f deg (cycle 2)" %
               (RESIZED_WH + (SUN_AZIM_B,)))

  ##############################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
        self._phase_time = time.time()
        self._marks["f_trigger1"] = self._frame
        self._marks["t_trigger1"] = time.time()
      return

    phases = self._phases()
    step = self._phase // 3
    if step >= len(phases):
      return
    ph = phases[step]
    sub = self._phase % 3

    ########################################
    # sub 0: wait for the cycle to publish, then settle
    ########################################
    if sub == 0:
      if not ph["wait"]():
        if (self._frame - self._phase_frame) > MAX_CYCLE_FRAMES:
          self._fail("%s cycle never completed in %d frames" % (ph["name"], MAX_CYCLE_FRAMES))
        elif (time.time() - self._phase_time) > WAIT_SECONDS:
          self._fail("%s cycle never completed in %.1f s" %
                     (ph["name"], time.time() - self._phase_time))
        return
      if "f_ready_" + ph["name"] not in self._marks:
        self._marks["f_ready_" + ph["name"]] = self._frame
        self._marks["t_ready_" + ph["name"]] = time.time()
        self._phase_frame = self._frame
        return
      if (self._frame - self._phase_frame) >= STEP_SETTLE:
        # sampled with the capture, before the phase's action can move it
        self._marks["extent_" + ph["name"]] = self._snapshotExtent()
        self._marks["gen_" + ph["name"]] = int(self._pbr().sky_ibl_generation)
        self._issueCapture(ctx, ph["name"])
        self._phase += 1
      return

    ########################################
    # sub 1: collect
    ########################################
    if sub == 1:
      if self._collect() is None:
        return
      self._note("%s: snapshot %s ball_mean=%.2f" %
                 (ph["name"], self._marks["extent_" + ph["name"]], self._ballMean(ph["name"])))
      self._phase += 2
      self._phase_frame = self._frame
      self._phase_time = time.time()
      if ph["after"] is not None:
        ph["after"]()
      if (self._phase // 3) >= len(self._phases()):
        self._emitVerdict()
      return

  ##############################################################

  def _fail(self, why):
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
    frames2 = m["f_ready_resized"] - m["f_trigger2"]
    mean_a = self._ballMean("default")
    mean_b = self._ballMean("resized")

    print("=== snapshot extent knob ===", flush=True)
    check("shipped_default", m["default_wh"] == DEFAULT_WH,
          "engine default=%dx%d expected=%dx%d" % (m["default_wh"] + DEFAULT_WH))
    check("cycle1_snapshot_at_default", m["extent_default"] == DEFAULT_WH,
          "snapshot=%s knob=%s" % (m["extent_default"], DEFAULT_WH))
    check("cycle2_snapshot_resized", m["extent_resized"] == RESIZED_WH,
          "snapshot=%s knob=%s" % (m["extent_resized"], RESIZED_WH))

    print("=== the feed survives the resize ===", flush=True)
    check("resized_cycle_published", m["gen_resized"] >= 2,
          "generation=%d" % m["gen_resized"])
    check("resized_cycle_bounded_frames", frames2 <= MAX_CYCLE_FRAMES,
          "frames=%d limit=%d" % (frames2, MAX_CYCLE_FRAMES))
    check("receiver_lit_before", mean_a > 2.0, "ball_mean=%.3f" % mean_a)
    check("receiver_lit_after", mean_b > 2.0, "ball_mean=%.3f" % mean_b)

    ok = (len(failures) == 0)
    detail = ("default=%dx%d resized=%dx%d gen=%d frames2=%d ball=%.2f->%.2f" %
              (m["extent_default"] + m["extent_resized"] +
               (m["gen_resized"], frames2, mean_a, mean_b)))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = ResizeApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
