#!/usr/bin/env ork.python
################################################################################
# SKYLIGHT lane B slice B2 — PROCEDURAL SKY ON SCREEN gate (FWD_SKYBOX_PROC).
#
# Four captures in ONE process (one warm shader cache, one static scene) drive
# the sky-source switch through baked -> procedural -> procedural(sun moved) ->
# baked and assert what "the procedural sky reached the screen" means:
#
#   a) GRADIENT   the procedural frame is nonblack and its sky has a vertical
#                 gradient — the band just above the horizon differs materially
#                 from the band at the top of the frame. A constant fill (an
#                 unbound LUT, a clamped UV) has no gradient.
#   b) DISC       the analytic sun disc lands where the sun actually is: the
#                 camera looks along azimuth 0 dead level, the sun sits at
#                 azimuth 0 / elevation SUN_ELEV_DEG, so the brightest pixel
#                 must be horizontally centered and tan(elev)/tan(fovy/2) of a
#                 half-height above center.
#   c) DISC MOVES rotating the sun in azimuth moves the disc horizontally by the
#                 projected amount (and barely at all vertically) — the disc
#                 tracks the sun the LUT was baked with, not a fixed screen spot.
#   d) BYTE-EXACT switching back to the baked source in the SAME scene, with the
#                 sun restored, reproduces the first baked capture byte for byte
#                 (L6: baked stays untouched, no parity window).
#
# The atmosphere is attached for ALL FOUR captures, so (d) isolates the SWITCH:
# arm-vs-disarm inertness is separately covered by test_sky_prologue_inert_gate.
#
# Not ork.testing capture_app: that harness drives ONE mainThreadLoop capture,
# and this gate needs a four-capture state machine inside one warm process. The
# verdict-before-teardown protocol (#57) is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import math
import time
import numpy
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

tokens = CrcStringProxy()

WIDTH, HEIGHT = 512, 384
FOV_DEG       = 45.0      # StandardSceneGraphComponent's setupUiCameraX default (vertical)

SETTLE_FRAMES = 60        # after scene build, before the first capture
ENV_WAIT_SECONDS = 90.0   # ceiling on the baked env map's async load (wall time)
STEP_SETTLE   = 30        # after each live state change

SUN_ELEV_DEG  = 8.0       # low enough to stay well inside a 45 degree frame
SUN_AZIM_A    = 0.0       # dead ahead of the camera
SUN_AZIM_B    = 15.0      # moved for the tracking check


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class SkyProcApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._env_ready = False
    self._t_start = time.time()
    self._shots = {}
    self._inflight = None
    # (capture key, action to run right after that capture is collected)
    self._steps = [
        ("baked_a", lambda: self._setSkySource("procedural")),
        ("proc_a",  lambda: self._aimSun(SUN_AZIM_B)),
        ("proc_b",  lambda: self._restoreBaked()),
        ("baked_b", None),
    ]
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        # dead level, looking down +Z: the sun's elevation is then a pure
        # vertical screen offset and its azimuth a pure horizontal one.
        eye=vec3(0, 2, 0), tgt=vec3(0, 2, 50), up=vec3(0, 1, 0),
        grid_variant="_V4",
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.1),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    SGC.pbr_common.enable_skybox = True
    # armed for every capture — this gate is about the SWITCH, not about arming
    self.atmo = lev2.SkyAtmosphereData()
    # A8 knobs, picked so the DISC is the only feature that reaches 255 in an
    # 8-bit capture: at exposure 1 the near-sun horizon peaks around half scale
    # (the sky-view LUT tops out near 0.54 at this sun elevation), while a disc
    # intensity of 1000 x the ~0.3 horizon transmittance saturates hard. That
    # makes "the set of maximal pixels" an unambiguous disc locator.
    self.atmo.sky_exposure = 1.0
    self.atmo.sun_disc_intensity = 1000.0
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "baked"

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 4.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_AZIM_A)
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)

    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass   # static: the only frame-to-frame differences are the ones we make

  ##############################################################

  def _aimSun(self, azimuth_deg):
    # DirectionalLight::direction() is the light's TRAVEL direction, so aiming
    # it from the sun's position at the origin makes dir_to_sun the negation —
    # exactly what the prologue's LUT step reads.
    d = dir_to_sun(azimuth_deg, SUN_ELEV_DEG)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self._sun_azimuth = azimuth_deg

  def _setSkySource(self, src):
    self.SGC.pbr_common.sky_source = src
    print("[sky-proc] sky_source=%s" % (self.SGC.pbr_common.sky_source,), flush=True)

  def _restoreBaked(self):
    self._aimSun(SUN_AZIM_A)
    self._setSkySource("baked")

  ##############################################################

  def _rtg(self, ctx):
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
    print("[sky-proc] captured %s %dx%d mean=%.4f max=%d" %
          (key, w, h, float(self._shots[key].mean()), int(self._shots[key].max())), flush=True)
    return True

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return
    if not self._built:
      if self._frame >= 2:
        self._built = True
        self._phase_frame = self._frame
      return

    # WAIT FOR THE BAKED ENV MAP, in WALL TIME. The 4k .xir streams in
    # asynchronously while this offscreen context runs thousands of frames per
    # second, so SETTLE_FRAMES is nowhere near a load: without this, baked_a can
    # be captured against a half-resident env map and baked_b (seconds of
    # procedural work later) against the finished one, failing the byte-identity
    # check with a maxdelta of ~3 for reasons that have nothing to do with the
    # sky source.
    if not self._env_ready:
      maps = self.SGC.pbr_common.RadianceMaps
      if (maps is None) or (maps.specular is None):
        if (time.time() - self._t_start) > ENV_WAIT_SECONDS:
          print("[sky-proc] baked env map never became resident", flush=True)
          verdict(False, "baked env map never became resident")
          self._exit_code = 1
          self._done = True
          self.ezapp.signalExit()
        return
      self._env_ready = True
      self._phase_frame = self._frame
      return

    step = self._phase // 2
    if step >= len(self._steps):
      return
    key, action = self._steps[step]

    if (self._phase % 2) == 0:               # settle, then issue
      settle = SETTLE_FRAMES if step == 0 else STEP_SETTLE
      if self._frame >= self._phase_frame + settle:
        self._issueCapture(ctx)
        self._phase += 1
      return

    if self._collect(key):                   # collected: run the step's action
      if action is not None:
        action()
      self._phase += 1
      self._phase_frame = self._frame
      if (self._phase // 2) >= len(self._steps):
        self._emitVerdict()
    return

  ##############################################################
  # observables
  ##############################################################

  def _brightestPixel(self, img):
    """(row, col, peak, count) of the brightest pixel set, centroided over every
    pixel that ties for the max — a saturated disc is a small plateau of texels,
    not a single one. The knobs above keep that plateau EXCLUSIVELY the disc."""
    lum = img.astype(numpy.float32).sum(axis=2)
    mx = float(lum.max())
    rows, cols = numpy.nonzero(lum >= mx - 1.0e-6)
    return float(rows.mean()), float(cols.mean()), mx, int(rows.size)

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    baked_a = self._shots.get("baked_a")
    baked_b = self._shots.get("baked_b")
    proc_a  = self._shots.get("proc_a")
    proc_b  = self._shots.get("proc_b")

    check("captures_present", all(s is not None for s in (baked_a, baked_b, proc_a, proc_b)))
    if any(s is None for s in (baked_a, baked_b, proc_a, proc_b)):
      verdict(False, "missing captures")
      self._exit_code = 1
      self._done = True
      self.ezapp.signalExit()
      return

    # the capture is the SCENEGRAPH VIEWPORT rtgroup, so the whole image is the
    # projection: fovy spans its full height and capture row 0 is the BOTTOM.
    h, w, _ = proc_a.shape
    half_h = h * 0.5
    half_w = w * 0.5
    center_row = (h - 1) * 0.5
    center_col = (w - 1) * 0.5
    tan_half_fov = math.tan(math.radians(FOV_DEG * 0.5))
    aspect = float(w) / float(h)

    ##########################################################
    # a) nonblack + vertical gradient
    ##########################################################
    print("=== procedural sky ===", flush=True)
    check("proc_nonblack", float(proc_a.mean()) > 1.0, "mean=%.4f" % float(proc_a.mean()))
    check("proc_differs_from_baked",
          int((numpy.abs(proc_a.astype(numpy.int16) - baked_a.astype(numpy.int16)).sum(axis=2) > 0).sum()) > (w * h) // 10,
          "differing_pixels=%d" % int((numpy.abs(proc_a.astype(numpy.int16) -
                                                 baked_a.astype(numpy.int16)).sum(axis=2) > 0).sum()))

    # rows above the horizon only; the sun column band is excluded so the gradient
    # is the SKY's, not the disc's glow.
    top_lo, top_hi = int(h * 0.90), int(h * 0.99)          # near the top of the frame
    hor_lo, hor_hi = int(h * 0.53), int(h * 0.60)          # just above the horizon
    col_lo, col_hi = 0, int(w * 0.25)                      # away from the sun
    zenith_band = float(proc_a[top_lo:top_hi, col_lo:col_hi].mean())
    horizon_band = float(proc_a[hor_lo:hor_hi, col_lo:col_hi].mean())
    rel = abs(horizon_band - zenith_band) / max(horizon_band, zenith_band, 1.0e-6)
    check("proc_vertical_gradient", rel > 0.05,
          "zenith_band=%.3f horizon_band=%.3f rel=%.4f" % (zenith_band, horizon_band, rel))
    check("proc_horizon_brighter_than_zenith", horizon_band > zenith_band,
          "%.3f > %.3f" % (horizon_band, zenith_band))

    ##########################################################
    # b) the sun disc lands where the sun is
    ##########################################################
    print("=== sun disc placement ===", flush=True)
    row_a, col_a, lum_a, npix_a = self._brightestPixel(proc_a)
    # predicted, from the camera (level, azimuth 0) and the sun's angles
    pred_dy = math.tan(math.radians(SUN_ELEV_DEG)) / tan_half_fov * half_h
    pred_row_a = center_row + pred_dy      # capture row 0 is the BOTTOM of the frame
    check("disc_is_compact", npix_a < 0.002 * w * h,
          "pixels_at_peak=%d of %d" % (npix_a, w * h))
    check("disc_is_brightest", lum_a > 2.0 * horizon_band * 3.0,
          "peak_lum=%.1f horizon_band_lum=%.1f" % (lum_a, horizon_band * 3.0))
    check("disc_horizontally_centered", abs(col_a - center_col) < 0.03 * w,
          "col=%.1f center=%.1f" % (col_a, center_col))
    check("disc_at_predicted_elevation", abs(row_a - pred_row_a) < 0.08 * half_h,
          "row=%.1f predicted=%.1f (dy=%.1f of half_h=%.1f)" % (row_a, pred_row_a, pred_dy, half_h))

    ##########################################################
    # c) the disc tracks the sun
    ##########################################################
    print("=== sun disc tracking ===", flush=True)
    row_b, col_b, lum_b, npix_b = self._brightestPixel(proc_b)
    pred_dx = (math.tan(math.radians(SUN_AZIM_B)) / (tan_half_fov * aspect)) * half_w
    check("disc_moved_horizontally", abs((col_b - col_a)) > 0.05 * w,
          "col %.1f -> %.1f (predicted shift %.1f)" % (col_a, col_b, pred_dx))
    check("disc_shift_matches_azimuth", abs(abs(col_b - col_a) - pred_dx) < 0.06 * w,
          "measured=%.1f predicted=%.1f" % (abs(col_b - col_a), pred_dx))
    check("disc_stayed_at_elevation", abs(row_b - row_a) < 0.06 * half_h,
          "row %.1f -> %.1f" % (row_a, row_b))

    ##########################################################
    # d) baked mode is untouched (L6)
    ##########################################################
    print("=== baked-mode byte identity ===", flush=True)
    check("baked_nonblack", float(baked_a.mean()) > 1.0, "mean=%.4f" % float(baked_a.mean()))
    diff = baked_a.astype(numpy.int16) - baked_b.astype(numpy.int16)
    diff_pixels = int((numpy.abs(diff).sum(axis=2) > 0).sum())
    check("baked_roundtrip_byte_identical", diff_pixels == 0,
          "differing_pixels=%d maxdelta=%d" % (diff_pixels, int(numpy.abs(diff).max())))

    ok = (len(failures) == 0)
    detail = ("disc=(%.1f,%.1f) grad_rel=%.4f baked_diff_pixels=%d" %
              (row_a, col_a, rel, diff_pixels))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = SkyProcApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before all four captures completed")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
