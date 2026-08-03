#!/usr/bin/env ork.python
################################################################################
# SKY FLOOR GATE — the procedural sky must never render BLACK below the horizon.
#
# THE DEFECT (fixed 2026-07-25): FWD_SKYBOX_PROC sampled the Hillaire sky-view
# LUT with the raw view elevation. The LUT's rows below the horizon hold the
# GROUND-OCCLUDED raymarch — at eye level that integral is a few meters of air,
# i.e. zero — so every view ray that dipped below the horizon returned black.
# Wherever terrain did not reach the horizon line the frame showed a black band,
# and the IBL equirect snapshot (which renders through the same
# skySampleSkyView) carried a black lower hemisphere into ambient/reflections.
#
# THE FIX (skytools.i2 skySampleSkyView): the elevation lookup is clamped to the
# LAST ABOVE-HORIZON TEXEL ROW and the reused radiance fades toward the ground it
# stands in for (SkyGroundAlbedo x the 45-degree sky radiance) over the first
# quarter of the horizon->nadir span. Both the visible sky and the IBL snapshot
# get it; no LUT bake changes; baked sky mode is not on this path at all.
#
# WHAT IS MEASURED — one warm process, no geometry at all (the frame IS the sky),
# camera at eye height pitched 40 degrees down, so a single frame spans from well
# above the horizon to the nadir. Every pixel's true elevation comes from the
# ENGINE's camera, so bands are selected by ANGLE, not by row fraction:
#
#   A) FLOOR      sun high (+20, out of frame). Below the horizon: not one black
#                 pixel, and the near-nadir band is DIMMER than the band just
#                 under the horizon (the fade exists) but not vanishing (it is a
#                 floor, not a black hole). The seam is a STEP DETECTOR on a
#                 half-degree vertical cut through the horizon: the step across
#                 elevation 0 must not dwarf the sky's own biggest step in the
#                 same window. Clamping to the v=0.5 row BOUNDARY instead of the
#                 texel center trips it (bilinear blends the black row in and
#                 halves the horizon radiance: measured step 35 vs 2.5), as does
#                 the unfixed shader (step 77, and the whole lower hemisphere
#                 black).
#   B) NO DISC    sun 3 degrees BELOW the horizon, dead ahead and in frame. The
#                 analytic disc is planet-clipped (skyEvalSunDisc), so the
#                 below-horizon sky must stay smooth: no compact bright spot.
#   C) TEETH      sun 3 degrees ABOVE the horizon, same frame region. The disc
#                 MUST appear — this is what proves check (B) can see a disc at
#                 all rather than passing on a dim frame.
#
# Not ork.testing capture_app: that harness drives ONE mainThreadLoop capture and
# cannot yet host a scenegraph app (same reason as test_sun_cascades_gate.py);
# this gate needs three captures with live sun moves in one warm process. The
# verdict-before-teardown protocol (#57) is honoured.
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

WIDTH, HEIGHT = 512, 384
FOVY_DEG      = 90.0        # a single frame from over the horizon to near-nadir
PITCH_DEG     = -40.0       # camera dip
EYE_Y         = 2.0
CAM_NEAR, CAM_FAR = 0.5, 20000.0

SETTLE_FRAMES = 90          # after scene build, before the first capture
STEP_SETTLE   = 40          # after each live sun move

SUN_ELEV_A    = 20.0        # high sun, disc out of frame
SUN_ELEV_B    = -3.0        # below the horizon (disc must be clipped)
SUN_ELEV_C    = 3.0         # above the horizon (disc must appear)

# elevation bands, degrees (negative = below the horizon)
SEAM_BELOW    = (-4.0, -1.0)
DEEP_BAND     = (-84.0, -70.0)
HORIZON_GUARD = 1.0         # ignore +-1 degree around the horizon (LUT texel blend)
SEAM_WINDOW   = 4.0         # the vertical cut runs +-4 degrees around the horizon
SEAM_BIN      = 0.5         # ... in half-degree bins
FLOOR_MIN_MEAN = 2.0        # 8-bit; measured 3.3 with the fix, exactly 0.0 without it
DISC_SAT_LEVEL = 250        # all three channels at/above this = the analytic disc

OUT_DIR = os.environ.get("SKYFLOOR_OUT", "/tmp/sky_floor_gate")


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


def elevation_map(camera, h, w):
  """per-pixel view-ray elevation in DEGREES, taken from the ENGINE's own camera.

  Three projected rays pin the pinhole basis exactly (center, right edge, bottom
  edge), so this cannot drift from whatever fov / pitch the camera actually
  renders with — which a hand-rolled fov*pitch formula silently does the moment
  the ui-camera rewrites the projection. projectDepthRay's 2d coordinate is the
  UNIT square with v=0 at the TOP; the capture array's row 0 is the BOTTOM, so
  the row axis is flipped back here.
  The disc-elevation check below is what proves this mapping (a mis-mapped
  frame puts the sun at the wrong angle and fails loudly)."""
  mats = camera.computeMatrices(float(w) / float(h))

  def ray(u, v):
    d = mats.projectDepthRayAsTuple(vec2(u, v))[0]
    return numpy.array([d.x, d.y, d.z], dtype=numpy.float64)

  F = ray(0.5, 0.5)
  # the off-axis basis vectors: perpendicular to F, so the scale that puts each
  # normalized edge ray back on the image plane is 1/dot(F,d).
  A = 2.0 * (ray(1.0, 0.5) / numpy.dot(F, ray(1.0, 0.5)) - F)   # +u (right)
  B = 2.0 * (ray(0.5, 1.0) / numpy.dot(F, ray(0.5, 1.0)) - F)   # +v (down)
  us = (((numpy.arange(w, dtype=numpy.float64) + 0.5) / w) - 0.5)
  vs = (((numpy.arange(h, dtype=numpy.float64) + 0.5) / h) - 0.5)
  V, U = numpy.meshgrid(-vs, us, indexing="ij")   # -vs: capture row 0 is the BOTTOM
  dirs = F[None, None, :] + U[..., None] * A[None, None, :] + V[..., None] * B[None, None, :]
  dirs /= numpy.linalg.norm(dirs, axis=2, keepdims=True)
  return numpy.degrees(numpy.arcsin(numpy.clip(dirs[..., 1], -1.0, 1.0)))


class SkyFloorApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._shots = {}
    self._inflight = None
    self._steps = [
        ("floor",   lambda: self._aimSun(SUN_ELEV_B)),
        ("sun_dn",  lambda: self._aimSun(SUN_ELEV_C)),
        ("sun_up",  None),
    ]
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
    # the SHIPPED exposure default, restated so the floor's measured level is the
    # one the engine actually presents; the disc runs at 1000x the illuminance so
    # that saturation means "disc" and nothing else (checks B and C).
    self.atmo.sky_exposure = 4.0
    self.atmo.sun_disc_intensity = 1000.0
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 1.0
    sun.shadowCaster = False
    self.sun = sun
    self._aimSun(SUN_ELEV_A)
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

    # setupUiCameraX's editor near/far would pin the projection behind our back;
    # pin it directly (no UI event reaches this offscreen app).
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
    # from the sun's position at the origin makes dir_to_sun the negation — which
    # is what the prologue's LUT step reads.
    d = dir_to_sun(0.0, elevation_deg)
    self.sun.lookAt(vec3(d.x, d.y, d.z) * 100.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self._sun_elev = elevation_deg

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
      print("[sky-floor] png write failed: %r" % (e,), flush=True)
    print("[sky-floor] captured %s %dx%d mean=%.4f max=%d" %
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

    if self._collect(key):                    # collected: run the step's action
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

  def _bandMean(self, img, elev, lo, hi):
    m = (elev >= lo) & (elev <= hi)
    return float(img[m].mean())

  def _profile(self, img, elev, lo, hi, step):
    """mean luminance per elevation bin — the vertical cut the seam test reads."""
    out = []
    e = lo
    while e < hi - 1.0e-9:
      m = (elev >= e) & (elev < e + step)
      out.append((e + 0.5 * step, float(img[m].mean()) if m.any() else float("nan")))
      e += step
    return out

  def _discPixels(self, img):
    """(count, mean elevation) of SATURATED pixels — the disc and nothing else.
    The disc runs at 1000x the illuminance, so it pins all three channels at 255
    while the sky at this exposure stays far below; a sky that could reach 250 on
    its own would break the oracle, which is why check (C) proves the count is
    zero only when the disc is genuinely gone."""
    sat = (img >= DISC_SAT_LEVEL).all(axis=2)
    return int(sat.sum()), sat

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    floor  = self._shots.get("floor")
    sun_dn = self._shots.get("sun_dn")
    sun_up = self._shots.get("sun_up")
    check("captures_present", all(s is not None for s in (floor, sun_dn, sun_up)))
    if any(s is None for s in (floor, sun_dn, sun_up)):
      verdict(False, "missing captures")
      self._exit_code = 1
      self._done = True
      self.ezapp.signalExit()
      return

    h, w, _ = floor.shape
    elev = elevation_map(self.SGC.camera, h, w)
    below = elev < -HORIZON_GUARD
    print("[sky-floor] frame spans elevation %.2f .. %.2f deg (camera fovy=%.1f)" %
          (float(elev.min()), float(elev.max()), float(self.SGC.camera.fovy)), flush=True)

    ##########################################################
    # C) teeth FIRST: the disc oracle must see a disc, and see it at the
    #    elevation the sun was actually placed at — which is also what proves
    #    the elevation map every other check reads.
    ##########################################################
    print("=== sun disc oracle (calibration + teeth) ===", flush=True)
    n_up, sat_up = self._discPixels(sun_up)
    check("disc_visible_above_horizon", n_up > 0, "saturated_px=%d" % n_up)
    disc_elev = float(elev[sat_up].mean()) if n_up else float("nan")
    check("disc_at_the_suns_elevation", abs(disc_elev - SUN_ELEV_C) < 1.0,
          "disc_elev=%.2f sun_elev=%.2f" % (disc_elev, SUN_ELEV_C))

    ##########################################################
    # B) and it never renders below the horizon
    ##########################################################
    n_dn, _ = self._discPixels(sun_dn)
    check("no_disc_below_horizon", n_dn == 0,
          "saturated_px=%d with the sun at %.1f deg" % (n_dn, SUN_ELEV_B))

    ##########################################################
    # A) the floor
    ##########################################################
    print("=== below-horizon floor ===", flush=True)
    lum_below = floor.astype(numpy.int32).sum(axis=2)[below]
    black_px = int((lum_below == 0).sum())
    check("no_black_pixels_below_horizon", black_px == 0,
          "black=%d of %d below-horizon px (min_lum=%d)" %
          (black_px, int(lum_below.size), int(lum_below.min())))

    # the vertical cut through the horizon: a hard seam is a STEP that dwarfs
    # every other step in the same window.
    prof = self._profile(floor, elev, -SEAM_WINDOW, SEAM_WINDOW, SEAM_BIN)
    print("[sky-floor] seam profile " +
          " ".join("%+.2f:%.1f" % (e, v) for e, v in prof), flush=True)
    steps = [abs(prof[i + 1][1] - prof[i][1]) for i in range(len(prof) - 1)]
    cross = len(prof) // 2 - 1              # the bin pair straddling elevation 0
    cross_step = steps[cross]
    other = sorted(steps[:cross] + steps[cross + 1:])[-1]
    check("no_step_at_the_horizon", cross_step < 2.0 * max(other, 0.5),
          "step_across_horizon=%.2f largest_other_step=%.2f" % (cross_step, other))

    seam_below = self._bandMean(floor, elev, *SEAM_BELOW)
    deep       = self._bandMean(floor, elev, *DEEP_BAND)
    check("floor_darkens_with_dip", deep < 0.80 * seam_below,
          "deep=%.2f seam_below=%.2f ratio=%.3f" %
          (deep, seam_below, deep / max(seam_below, 1.0e-6)))
    check("floor_is_not_black", deep > FLOOR_MIN_MEAN,
          "deep_band_mean=%.2f (8-bit, floor %.1f)" % (deep, FLOOR_MIN_MEAN))

    ok = (len(failures) == 0)
    detail = ("horizon_step=%.2f deep=%.2f seam_below=%.2f disc_px_above=%d disc_px_below=%d" %
              (cross_step, deep, seam_below, n_up, n_dn))
    if failures:
      detail += " failed=" + ",".join(failures)
    # VERDICT BEFORE TEARDOWN (#57)
    verdict(ok, detail)
    self._exit_code = 0 if ok else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = SkyFloorApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before all three captures completed")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
