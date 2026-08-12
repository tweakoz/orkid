#!/usr/bin/env ork.python
################################################################################
# OUTPUT DITHER gate — the final 8-bit encodes must not contour a smooth sky.
#
# The render chain is float end to end; the quantizers are the 8-bit stores, and
# they round with no error diffusion. Undithered, a smooth display-space
# gradient lands as 1-LSB contours: on the procedural sky's upper gradient EVERY
# step is exactly 1 LSB, per channel, and most sky pixels sit inside a
# completely flat neighborhood. dithertools.i2 answers that with a
# triangular-PDF (+-1 output LSB) offset added by the LAST shader before each
# store.
#
# There are TWO such stores and they are independent code paths, so this gate
# runs TWO LEGS as child processes (one GPU app per process):
#
#   leg "postfx"  procedural sky through a PostFxNodeACES chain, captured off
#                 the scenegraph viewport RTG (float -> RGBA8 readback). Covers
#                 framefx.fxv2 ps_aces / ps_hsvg.
#                 A/B on this scene (shader stashed vs applied, same binary —
#                 fxv2 is JIT): undithered flat7x7=0.8435 run_p95=102.0 rows
#                 (rc=1) vs dithered 0.0000 / 6.0 (rc=0).
#   leg "eye"     the SAME sky with NO postfx chain, rendered by a VR (stereo)
#                 scene and captured off a per-eye SSAA buffer — RGBA8, and the
#                 exact texture handed to the OpenXR runtime. Covers blit.fxv2
#                 (ps_blit / ps_blituv / ps_downsample*, gated per call site by
#                 DitherAmt), which is the same shader that writes main_rtg
#                 (BGRA8) for the desktop window and the offscreen player
#                 snapshot.
#                 A/B on this scene (DitherAmt bound 0.0 vs 1.0, rebuilt):
#                 undithered flat7x7=0.8662 run_p95=67.0 rows (rc=1) vs
#                 dithered 0.0000 / 6.0 (rc=0). End-to-end on the desktop path,
#                 the same shader takes the scn_procsky player snapshot from
#                 flat7x7=0.6958 / run_p95=69.2 to 0.0000 / 6.0.
#
# Each leg asserts, in its own band of sky:
#
#   a) NO CONTOURS  the fraction of pixels whose whole 7x7 neighborhood is a
#                   single constant value collapses, and the 95th-percentile
#                   vertical constant-run drops to a few rows. The thresholds
#                   sit between the measured undithered and dithered values, so
#                   removing either dither FAILS this gate (teeth proven by the
#                   A/B above, not asserted).
#   b) NOISE, NOT   no pixel differs from its 3x3 neighbourhood by more than a
#      STRUCTURE    couple of LSB: the contours were replaced by sub-LSB noise,
#                   not by an artifact with its own structure.
#   c) DETERMINISM  a second capture of the same static frame is BYTE IDENTICAL
#                   to the first. Every dither hashes gl_FragCoord and NOTHING
#                   time- or frame-varying; the blessed byte-identity canaries
#                   and every in-run A/B gate depend on that. A frame-varying
#                   hash (a time uniform, a frame counter, a jittered sample
#                   position) fails HERE.
#
# Not ork.testing capture_app: that harness drives ONE mainThreadLoop capture,
# and each leg needs two captures in one warm process. The
# verdict-before-teardown protocol (#57) is honoured in both legs.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import math
import subprocess
import time
import numpy
from numpy.lib.stride_tricks import sliding_window_view
from orkengine.core import vec3, mtx4, CrcStringProxy, VarMap, lev2_pyexdir
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCameraX
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

LEGS = ["postfx", "eye"]
LEG_TIMEOUT = 300.0       # a leg boots a GPU scene; a hang is a failure, not a wait

WIDTH, HEIGHT = 1024, 768

SETTLE_FRAMES = 60        # after scene build, before the first capture
ENV_WAIT_SECONDS = 90.0   # ceiling on the baked env map's async load (wall time)
STEP_SETTLE   = 20        # between the two captures

SUN_ELEV_DEG  = 8.0       # low: keeps the disc out of the measured band
SUN_AZIM_DEG  = 0.0       # dead ahead of the camera

# measurement band = the zenith end of the sky gradient, well above the sun disc.
# The two surfaces store OPPOSITE row order: the scenegraph viewport RTG comes
# back BOTTOM-up (capture row 0 is the bottom of the frame, as test_sky_proc_gate
# documents), the per-eye VR buffer comes back TOP-down. Same patch of sky.
BAND_FRACS    = {"postfx": (0.80, 0.97), "eye": (0.03, 0.20)}
COL_MARGIN    = 0.04
FLAT_WIN      = 7

# thresholds — see the measured A/B in the header
MAX_FLAT_FRAC = 0.05      # undithered 0.8435
MAX_RUN_P95   = 12.0      # undithered 102.0 rows
MAX_LOCAL_DEV = 4         # 3x3 max-min in the band (dither is bounded)


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


def flat_fraction(band):
  """fraction of pixels whose whole FLAT_WIN x FLAT_WIN neighbourhood is one
  constant value, worst channel — the direct measure of a banded plateau."""
  worst = 0.0
  for ch in range(3):
    win = sliding_window_view(band[..., ch], (FLAT_WIN, FLAT_WIN))
    flat = (win.max(axis=(2, 3)) == win.min(axis=(2, 3)))
    worst = max(worst, float(flat.mean()))
  return worst


def run_p95(band):
  """95th percentile of the vertical constant-run length (rows), worst channel."""
  worst = 0.0
  h, w, _ = band.shape
  for ch in range(3):
    lengths = []
    for c in range(0, w, 4):
      col = band[:, c, ch]
      edges = numpy.concatenate(([-1], numpy.nonzero(numpy.diff(col))[0], [h - 1]))
      lengths += numpy.diff(edges).tolist()
    worst = max(worst, float(numpy.percentile(numpy.array(lengths), 95)))
  return worst


def local_deviation(band):
  """max 3x3 (max-min) over the band, worst channel."""
  worst = 0
  for ch in range(3):
    win = sliding_window_view(band[..., ch], (3, 3))
    worst = max(worst, int((win.max(axis=(2, 3)) - win.min(axis=(2, 3))).max()))
  return worst


class OutputDitherApp(ComponentizedApplication):

  def __init__(self, leg):
    super().__init__()
    self._leg = leg
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._env_ready = False
    self._t_start = time.time()
    self._shots = {}
    self._inflight = None
    self._keys = ["shot_a", "shot_b"]

    self.SGC = None
    if leg == "postfx":
      # viewport-widget scenegraph: its compositor ends in an RtGroup output
      # node (float), so the tonemap node is the last shader before the RGBA8
      # capture readback.
      self.aces = lev2.PostFxNodeACES()
      self.aces.exposure = 1.0
      self.SGC = self.addComponent(
          "std_scenegraph", StandardSceneGraphComponent,
          # dead level, looking down +Z: the sky gradient is then a pure vertical
          # ramp across the frame.
          eye=vec3(0, 2, 0), tgt=vec3(0, 2, 50), up=vec3(0, 1, 0),
          grid_variant="_V4",
          post_nodes=[self.aces],
          sg_params={
            "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
            "SkyboxIntensity":  1.0,
            "DiffuseIntensity": 1.0,
            "SpecularIntensity": 1.0,
            "AmbientLevel":     vec3(0.1),
          })
    else:
      # eye leg: a VR (single-pass stereo) scene with NO postfx chain. Its per-eye
      # SSAA buffers are RGBA8 (the VR output node's _format) and are the textures
      # handed to the XR runtime, so the blit that fills them (blit.fxv2, gated
      # by DitherAmt) is the last shader before those 8 bits — the same shader
      # and the same call-site switch the desktop ScreenOutput blit into
      # main_rtg (BGRA8) uses. Capturing an eye buffer is how that shader is
      # reachable from a headless gate (test_spvr_capture_gate's mechanism).
      self.cameralut = lev2.CameraDataLut()

    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    if self._leg == "eye":
      return self._onGpuInitScreen(ctx)
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = True
    # the procedural sky is the smoothest gradient the engine can put on
    # screen — the signal banding shows up on first. Disc intensity left at a
    # sane value; the measured band is nowhere near it.
    self.atmo = lev2.SkyAtmosphereData()
    self.atmo.sky_exposure = 1.0
    self.atmo.sun_disc_intensity = 1000.0
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 4.0
    sun.shadowCaster = False
    self.sun = sun
    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  ##############################################################

  def _onGpuInitScreen(self, ctx):
    self.ctx = ctx
    # NoVR device: a static head pose, level, looking down +Z — the same view
    # the postfx leg uses, so both legs measure the same patch of sky.
    self.vrdev = lev2.orkidvr.novr_device()
    self.vrdev.width  = WIDTH
    self.vrdev.height = HEIGHT
    self.vrdev.FOVD   = 45.0
    self.vrdev.IPD    = 0.065
    self.vrdev.near   = 0.1
    self.vrdev.far    = 1e4
    self.vrdev.setPoseMatrix("hmd", mtx4.lookAt(vec3(0, 2, 0), vec3(0, 2, 50), vec3(0, 1, 0)))

    vars = VarMap()
    vars.SkyboxIntensity = 1.0
    vars.DiffuseIntensity = 1.0
    vars.SpecularIntensity = 1.0
    vars.AmbientLevel = vec3(0.1)
    vars.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    vars.ssaa = 0
    createSceneGraph(app=self, rendermodel="FWDPBRVRDM", vars=vars)
    self.outputnode = self.scene.compositoroutputnode

    self.scene.pbr_common.enable_skybox = True
    self.atmo = lev2.SkyAtmosphereData()
    self.atmo.sky_exposure = 1.0
    self.atmo.sun_disc_intensity = 1000.0
    self.scene.pbr_common.atmosphere = self.atmo
    self.scene.pbr_common.sky_source = "procedural"

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 4.0
    sun.shadowCaster = False
    self.sun = sun
    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun_node = self.layer1.createLightNode("sun", sun)

    self.scene.lightingmanager.gpuInit(ctx)

    # WITHOUT a UI widget tree the app's onGpuPostFrame never fires (ezapp.cpp
    # leaves ctx->_onGpuPostFrame commented out — it reaches an app only through
    # the UI draw path; the same onDraw footgun test_spvr_capture_gate
    # documents), and the compositor's onBeginAssemble/onEndAssemble hooks are
    # wired by the VR output node ONLY. onGpuUpdate is hooked
    # unconditionally and runs on the GPU thread before the frame, where the
    # PRIOR frame's main_rtg is complete and no render pass is open.
    # the VR output node is the one node that invokes the compositor assemble
    # hooks; onBeginAssemble runs on the GPU thread with the PRIOR frame's eye
    # buffers complete and no render pass open (the readback rule
    # test_spvr_capture_gate established).
    self.outputnode.onBeginAssemble(lambda cdd: self._pump(self.ctx))

  def _onUpdate(self, updinfo):
    if self._leg == "eye":
      self.scene.updateScene(self.cameralut)   # static pose: no frame variance
      return
    pass   # static: the two captures must differ in NOTHING

  ##############################################################

  def _rtg(self, ctx):
    # screen leg: main_rtg IS the 8-bit surface (BGRA8, fbi.cpp _ensureMainRtg)
    # the window presents and the offscreen player snapshots — the store the
    # blit dither exists for. postfx leg: the viewport RTG, which is float and
    # is quantized by the capture readback below.
    if self._leg == "eye":
      return self.outputnode.downsampledEyeRtGroup(True)
    rtg = getattr(getattr(self.SGC, "SGVPW", None), "rtgroup", None)
    if rtg is None or rtg.numBuffers < 1:
      rtg = ctx.FBI.main_RTG
    return rtg

  def _issueCapture(self, ctx):
    buf = lev2.CaptureBuffer()
    fut = ctx.FBI.captureAsFormat(self._rtg(ctx).buffer(0), buf, "RGBA8")
    self._inflight = (fut, buf)

  def _collect(self, key):
    fut, buf = self._inflight
    if not bool(fut.is_ready):
      return False
    w, h = buf.width, buf.height
    self._shots[key] = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3].copy()
    self._inflight = None
    print("[dither] captured %s %dx%d mean=%.4f max=%d" %
          (key, w, h, float(self._shots[key].mean()), int(self._shots[key].max())), flush=True)
    return True

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    if self._leg == "eye":
      return          # driven by the compositor hook instead (see _onGpuInitScreen)
    self._pump(ctx)

  def _pump(self, ctx):
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
      return

    # WAIT FOR THE BAKED ENV MAP in WALL TIME (the 4k .xir streams in async
    # while this offscreen context runs thousands of frames per second) — the
    # scene must be fully settled before either capture, or the two captures
    # differ for reasons that have nothing to do with dither.
    # the screen leg has no lit geometry — only the procedural sky, which needs
    # no baked env map — so it skips the residency wait entirely.
    if (not self._env_ready) and (self._leg == "eye"):
      self._env_ready = True
      self._phase_frame = self._frame
      return
    if not self._env_ready:
      maps = self.SGC.pbr_common.RadianceMaps
      if (maps is None) or (maps.specular is None):
        if (time.time() - self._t_start) > ENV_WAIT_SECONDS:
          verdict(False, "baked env map never became resident")
          self._exit_code = 1
          self._done = True
          self.ezapp.signalExit()
        return
      self._env_ready = True
      self._phase_frame = self._frame
      return

    step = self._phase // 2
    if step >= len(self._keys):
      return

    if (self._phase % 2) == 0:               # settle, then issue
      settle = SETTLE_FRAMES if step == 0 else STEP_SETTLE
      if self._frame >= self._phase_frame + settle:
        self._issueCapture(ctx)
        self._phase += 1
      return

    if self._collect(self._keys[step]):
      self._phase += 1
      self._phase_frame = self._frame
      if (self._phase // 2) >= len(self._keys):
        self._emitVerdict()
    return

  ##############################################################

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    a = self._shots.get("shot_a")
    b = self._shots.get("shot_b")
    check("captures_present", (a is not None) and (b is not None))
    if (a is None) or (b is None):
      verdict(False, "[%s] missing captures" % self._leg)
      self._exit_code = 1
      self._done = True
      self.ezapp.signalExit()
      return

    h, w, _ = a.shape
    # inset horizontally so a frame edge can never masquerade as a plateau.
    lo_frac, hi_frac = BAND_FRACS[self._leg]
    r0, r1 = int(h * lo_frac), int(h * hi_frac)
    c0, c1 = int(w * COL_MARGIN), int(w * (1.0 - COL_MARGIN))
    band = a[r0:r1, c0:c1, :].astype(numpy.int16)

    print("=== [%s] sky band (rows %d..%d, cols %d..%d) ===" % (self._leg, r0, r1, c0, c1), flush=True)
    check("band_nonblack", float(band.mean()) > 1.0, "mean=%.3f" % float(band.mean()))

    ff = flat_fraction(band)
    check("no_flat_plateaus", ff < MAX_FLAT_FRAC,
          "flat%dx%d_frac=%.4f (limit %.2f, undithered 0.84)" % (FLAT_WIN, FLAT_WIN, ff, MAX_FLAT_FRAC))

    rp = run_p95(band)
    check("no_constant_runs", rp < MAX_RUN_P95,
          "vert_const_run_p95=%.1f rows (limit %.1f, undithered 102.0)" % (rp, MAX_RUN_P95))

    ld = local_deviation(band)
    check("dither_is_bounded", ld <= MAX_LOCAL_DEV,
          "max_3x3_span=%d LSB (limit %d)" % (ld, MAX_LOCAL_DEV))

    print("=== determinism ===", flush=True)
    diff = a.astype(numpy.int16) - b.astype(numpy.int16)
    diff_pixels = int((numpy.abs(diff).sum(axis=2) > 0).sum())
    check("frame_to_frame_byte_identical", diff_pixels == 0,
          "differing_pixels=%d maxdelta=%d" % (diff_pixels, int(numpy.abs(diff).max())))

    ok = (len(failures) == 0)
    detail = "leg=%s flat_frac=%.4f run_p95=%.1f local_span=%d diff_pixels=%d" % (
        self._leg, ff, rp, ld, diff_pixels)
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def run_leg(leg):
  app = OutputDitherApp(leg)
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "[%s] loop exited before both captures completed" % leg)
    rc = 1
  return rc


def main():
  """orchestrator: one GPU app per process, so each leg is a child run
  (test_teardown_rc_gate's shape). Only THIS process emits TESTVERDICT."""
  results = []
  ok = True
  for leg in LEGS:
    try:
      proc = subprocess.run([sys.executable, os.path.abspath(__file__), "--leg", leg],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=LEG_TIMEOUT)
      rc, log = proc.returncode, proc.stdout.decode("utf-8", "replace")
    except subprocess.TimeoutExpired:
      rc, log = None, ""
    detail = ""
    for line in log.splitlines():
      if line.startswith("  PASS ") or line.startswith("  FAIL ") or line.startswith("==="):
        print(line, flush=True)
      if "TESTVERDICT=" in line:
        detail = line.split("TESTVERDICT=", 1)[1]
    leg_ok = (rc == 0)
    ok = ok and leg_ok
    results.append("%s rc=%s %s" % (leg, "TIMEOUT" if rc is None else rc, detail))
    if not leg_ok:
      sys.stdout.write(log[-4000:] if log else "(no child output)\n")
      sys.stdout.flush()
  return verdict(ok, " | ".join(results))


if __name__ == "__main__":
  if "--leg" in sys.argv:
    sys.exit(run_leg(sys.argv[sys.argv.index("--leg") + 1]))
  sys.exit(main())
