#!/usr/bin/env ork.python
################################################################################
# SKY HORIZON GRADIENT GATE — the sky must read as ONE gradient through the
# horizon, not as a lit sky sitting on a dead band.
#
# THE DEFECT (owner, 2026-07-30): below the horizon skySampleSkyView faded the
# clamped horizon radiance to a FLAT lambertian ground tint within a quarter of
# the horizon->nadir span. Two things showed: the lower hemisphere went dead
# (constant) a few degrees down, and its edge — the place where a gradient meets
# a flat region — read as a drawn line wherever terrain did not cover the
# skyline. test_sky_floor_gate already forbids BLACK and forbids a STEP across
# the horizon; neither can see a flat band or a slope break, which is what an
# eye actually catches.
#
# WHAT IS MEASURED — sky-only frames (no geometry: the frame IS the sky), eye
# at 2 m, camera pitched down so one frame spans from above the horizon to near
# the nadir. Every pixel's elevation comes from the ENGINE's camera (the same
# derivation test_sky_floor_gate uses, for the same reason: a hand-rolled
# fov*pitch formula silently drifts from the projection that actually rendered).
#
#   A) NO FLAT BAND   every 3-degree window from -6 to -30 must change by at
#                     least 6% of its own level. The fill is an air-column
#                     grade, so it keeps moving; the flat tint it replaced is
#                     already constant by -22 (its fade completes a quarter of
#                     the way to the nadir) and changes by 0. The band stops at
#                     -30 because an 8-bit capture cannot resolve percentages
#                     out of the 3-4 levels the deep floor sits on.
#   B) TURNOVER       the horizon is the brightest part of the sky and the
#                     radiance must turn over there SMOOTHLY: the drop one
#                     degree BELOW it may not exceed 25% of the drop one degree
#                     ABOVE it. A fill that leaves the horizon at full slope
#                     reverses the derivative inside one texel and draws a
#                     bright line on the skyline even though the value itself is
#                     continuous (measured 0.49 unshaped, 0.13 shaped), down a
#                     vertical cut through the middle of the frame.
#   C) MONOTONE       no ridge or valley below the horizon — the profile only
#                     darkens with depression.
#
# DAY AND NIGHT both, because the fill is required to be time-of-day free: the
# night leg runs the same three checks with the sun 12 degrees down. It needs a
# far higher sky exposure only so that an 8-bit capture can resolve a moonless
# sky at all — the SHAPE is what is gated, and the shape is what must not care
# what hour it is.
#
# Not ork.testing capture_app: that harness drives ONE mainThreadLoop capture
# and cannot host a scenegraph app (see test_sky_floor_gate.py); this gate needs
# two captures with a live sun move in one warm process. The verdict-before-
# teardown protocol (#57) is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import math
import numpy
from PIL import Image as PILImage
from orkengine.core import vec2, vec3
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict

WIDTH, HEIGHT = 512, 512
FOVY_DEG      = 90.0
PITCH_DEG     = -20.0
EYE_Y         = 2.0
CAM_NEAR, CAM_FAR = 0.5, 20000.0

SETTLE_FRAMES = 90          # after scene build, before the first capture
STEP_SETTLE   = 40          # after the sun move + exposure change

SUN_ELEV_DAY   = 20.0
SUN_ELEV_NIGHT = -12.0
EXPOSURE_DAY   = 4.0        # the shipped default
EXPOSURE_NIGHT = 3000.0     # 8-bit headroom for a moonless sky (shape, not level)

BAND_LO, BAND_HI, BAND_STEP = -30.0, -6.0, 3.0
FLAT_MIN_REL   = 0.06       # measured worst window 0.17 (day) / 0.10 (night)
TURNOVER_MAX   = 0.25       # measured 0.13; 0.49 with the grade unshaped
TURN_WINDOW    = 1.0        # degrees either side of the horizon
TURN_STRIP     = 0.125      # ... down a VERTICAL CUT this wide (fraction of the
                            # frame). Averaging the turnover over every azimuth
                            # in a 116-degree-wide frame mixes cuts whose peaks
                            # sit at different elevations and buries the shape.

OUT_DIR = os.environ.get("SKYGRAD_OUT", "/tmp/sky_horizon_gradient")


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


def elevation_map(camera, h, w):
  """per-pixel view-ray elevation in DEGREES, from the ENGINE's own camera.
  Three projected rays pin the pinhole basis (center, right edge, bottom edge).
  projectDepthRay's 2d coordinate has v=0 at the TOP; the capture array's row 0
  is the BOTTOM, so the row axis is flipped back here."""
  mats = camera.computeMatrices(float(w) / float(h))

  def ray(u, v):
    d = mats.projectDepthRayAsTuple(vec2(u, v))[0]
    return numpy.array([d.x, d.y, d.z], dtype=numpy.float64)

  F = ray(0.5, 0.5)
  A = 2.0 * (ray(1.0, 0.5) / numpy.dot(F, ray(1.0, 0.5)) - F)
  B = 2.0 * (ray(0.5, 1.0) / numpy.dot(F, ray(0.5, 1.0)) - F)
  us = (((numpy.arange(w, dtype=numpy.float64) + 0.5) / w) - 0.5)
  vs = (((numpy.arange(h, dtype=numpy.float64) + 0.5) / h) - 0.5)
  V, U = numpy.meshgrid(-vs, us, indexing="ij")
  dirs = F[None, None, :] + U[..., None] * A[None, None, :] + V[..., None] * B[None, None, :]
  dirs /= numpy.linalg.norm(dirs, axis=2, keepdims=True)
  return numpy.degrees(numpy.arcsin(numpy.clip(dirs[..., 1], -1.0, 1.0)))


class SkyGradientApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._shots = {}
    self._inflight = None
    self._steps = [("day", self._toNight), ("night", None)]
    p = math.radians(PITCH_DEG)
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, EYE_Y, 0),
        tgt=vec3(0, EYE_Y + 100.0 * math.sin(p), 100.0 * math.cos(p)),
        up=vec3(0, 1, 0),
        grid_variant=None,          # NO geometry: every pixel is sky
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
    self.atmo = lev2.SkyAtmosphereData()
    self.atmo.sky_exposure = EXPOSURE_DAY
    self.atmo.sun_disc_intensity = 1.0   # the DISC is not what this gate reads
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 1.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_ELEV_DAY)
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

    p = math.radians(PITCH_DEG)
    SGC.camera.perspective(CAM_NEAR, CAM_FAR, FOVY_DEG)
    SGC.camera.lookAt(
        vec3(0, EYE_Y, 0),
        vec3(0, EYE_Y + 100.0 * math.sin(p), 100.0 * math.cos(p)),
        vec3(0, 1, 0))

  def _onUpdate(self, updinfo):
    pass   # static: the only frame-to-frame differences are the ones we make

  def _aimSun(self, elevation_deg):
    # DirectionalLight::direction() is the light's TRAVEL direction, so aiming it
    # from the sun's position at the origin makes dir_to_sun the negation.
    d = dir_to_sun(0.0, elevation_deg)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))

  def _toNight(self):
    self._aimSun(SUN_ELEV_NIGHT)
    self.atmo.sky_exposure = EXPOSURE_NIGHT
    self.SGC.pbr_common.atmosphere = self.atmo

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
    img = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3].copy()
    self._shots[key] = img
    self._inflight = None
    try:
      os.makedirs(OUT_DIR, exist_ok=True)
      PILImage.fromarray(img).transpose(PILImage.FLIP_TOP_BOTTOM).save(
          os.path.join(OUT_DIR, "%s.png" % key))
    except Exception as e:
      print("[sky-grad] png write failed: %r" % (e,), flush=True)
    print("[sky-grad] captured %s %dx%d mean=%.4f max=%d" %
          (key, w, h, float(img.mean()), int(img.max())), flush=True)
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

    step = self._phase // 2
    if step >= len(self._steps):
      return
    key, action = self._steps[step]

    if (self._phase % 2) == 0:                # settle, then issue
      settle = SETTLE_FRAMES if step == 0 else STEP_SETTLE
      if self._frame >= self._phase_frame + settle:
        self._issueCapture(ctx)
        self._phase += 1
      return

    if self._collect(key):
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

  def _bandMean(self, lum, elev, lo, hi):
    m = (elev >= lo) & (elev < hi)
    return float(lum[m].mean()) if m.any() else float("nan")

  def _leg(self, key, lum, elev, check):
    """the three checks on one capture."""
    # A) no flat band: every window must move by its own 3%
    xs = numpy.arange(BAND_LO, BAND_HI - 1.0e-9, BAND_STEP)
    prof = [self._bandMean(lum, elev, x, x + BAND_STEP) for x in xs]
    print("[sky-grad] %s band " % key +
          " ".join("%+.0f:%.2f" % (x + 0.5 * BAND_STEP, v) for x, v in zip(xs, prof)), flush=True)
    rels = [abs(prof[i + 1] - prof[i]) / max(prof[i], 1.0e-6) for i in range(len(prof) - 1)]
    worst = min(rels)
    check("%s_no_flat_band" % key, worst >= FLAT_MIN_REL,
          "worst_window_rel_change=%.4f (floor %.3f)" % (worst, FLAT_MIN_REL))

    # C) monotone: darker with every step down (prof runs deepest-first)
    mono = all(prof[i + 1] > prof[i] for i in range(len(prof) - 1))
    check("%s_monotone_below_horizon" % key, mono, "bins=%d" % len(prof))

    # B) turnover: the slope must nearly flatten at the horizon
    w     = lum.shape[1]
    c0    = int(0.5 * w * (1.0 - TURN_STRIP))
    c1    = int(0.5 * w * (1.0 + TURN_STRIP))
    # one value per PIXEL ROW down the cut, read at the three elevations by
    # interpolation: a fixed-width elevation bin can fall between two rows near
    # the horizon and come back empty.
    e_row = elev[:, c0:c1].mean(axis=1)
    l_row = lum[:, c0:c1].mean(axis=1)
    order = numpy.argsort(e_row)
    e_row, l_row = e_row[order], l_row[order]
    above = float(numpy.interp(TURN_WINDOW, e_row, l_row))
    at    = float(numpy.interp(0.0, e_row, l_row))
    below = float(numpy.interp(-TURN_WINDOW, e_row, l_row))
    slope_above = at - above
    slope_below = at - below
    ratio = abs(slope_below) / max(abs(slope_above), 1.0e-6)
    check("%s_horizon_turnover" % key, ratio <= TURNOVER_MAX,
          "below/above slope=%.3f (max %.2f) above=%.2f at=%.2f below=%.2f" %
          (ratio, TURNOVER_MAX, above, at, below))
    return ratio, worst

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    day   = self._shots.get("day")
    night = self._shots.get("night")
    check("captures_present", all(s is not None for s in (day, night)))
    if any(s is None for s in (day, night)):
      verdict(False, "missing captures")
      self._exit_code = 1
      self._done = True
      self.ezapp.signalExit()
      return

    h, w, _ = day.shape
    elev = elevation_map(self.SGC.camera, h, w)
    print("[sky-grad] frame spans elevation %.2f .. %.2f deg (camera fovy=%.1f)" %
          (float(elev.min()), float(elev.max()), float(self.SGC.camera.fovy)), flush=True)
    # the night leg is worthless if its exposure change never landed
    check("night_leg_has_signal", float(night.max()) > 4.0,
          "night_max=%d" % int(night.max()))

    r_day, f_day = self._leg("day", day.astype(numpy.float64).mean(axis=2), elev, check)
    r_ngt, f_ngt = self._leg("night", night.astype(numpy.float64).mean(axis=2), elev, check)

    ok = (len(failures) == 0)
    detail = ("turnover day=%.3f night=%.3f flat_worst day=%.4f night=%.4f" %
              (r_day, r_ngt, f_day, f_ngt))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = SkyGradientApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before both captures completed")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
