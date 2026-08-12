#!/usr/bin/env ork.python
################################################################################
# HAZE / IBL COUPLING GATE (G6) — the reflected sky is the sky overhead.
#
# The visible skybox runs the artist haze layer (skyHazeSkyOverlay). The IBL
# tier is fed by a SECOND rendering of the same sky — the equirect snapshot the
# sliced radiance prefilter and the SH probe both consume — and until this slice
# that snapshot resampled the raw sky-view LUT. The result was one sky with two
# appearances: a smoggy dome above and a clean blue one inside every shiny
# surface. That is what this gate measures, and it measures it against the
# scene's OWN numbers rather than a hand-picked level:
#
#   (a) MIRROR SPHERE vs THE SKY IT REFLECTS. For a distant metallic-1 sphere
#       at eye level, the point w radii above its centre reflects the direction
#       of elevation 2*asin(w) on the BACKWARD side (r = v - 2(v.n)n with a
#       parallel view ray; w = 0.5 gives 60 degrees up and behind). The sky in
#       that same direction is read from a second capture aimed straight at it —
#       its CENTRE pixel is the view axis whatever the fov, aspect or capture
#       orientation turn out to be, so no projection model is hand-rolled here
#       (test_haze_horizon_convergence's finding).
#
#       THE PROBE DIRECTION IS ANTI-SOLAR ON PURPOSE. A sphere compresses the
#       whole 360 degrees into its radius (~2.5 deg per pixel here), so any
#       patch on it averages a wide cone of sky; pointed at the sun side that
#       average is dominated by the forward-scattering gradient and the
#       comparison measures the gradient instead of the haze. With the sun in
#       FRONT of the camera the sphere's well-resolved backward hemisphere is
#       the smooth anti-solar sky, and the residual angular offset (the view ray
#       at that point is not quite parallel to the axis) is a systematic that
#       shows up in err_baseline below.
#
#       Three distances, all in 8-bit levels:
#          err_baseline = |mirror(haze off)  - probe sky(haze off) |
#          ref_prefix   = |mirror(haze off)  - probe sky(haze ON)  |
#          err_post     = |mirror(haze ON)   - probe sky(haze ON)  |
#       ref_prefix IS the pre-fix mismatch: before this slice the reflection was
#       the un-hazed sky while the dome was hazed, which is exactly those two
#       terms. The gate is err_post <= POST_FRACTION * ref_prefix, i.e. the
#       mismatch must collapse to a small fraction of the thing it replaces, and
#       err_baseline (the harness's own systematic: prefilter blur, the mirror's
#       few degrees of angular offset from the probe direction) has to sit under the same
#       bar or the comparison was never valid.
#
#   (b) MATTE SPHERE AMBIENT. A diffuse sphere 3 m from the eye. At that range
#       the FORWARD aerial-perspective march contributes essentially nothing
#       (0.003 km at 1/km is 0.3% optical depth), so any shift it shows is the
#       ambient tier moving — the SH projection of the same snapshot. Direction
#       and magnitude are checked, loosely: this is a "the diffuse tier heard
#       about it too" leg, not a bound.
#
#   (c) DISARMED DETERMINISM. Haze is cranked and then disarmed again, which
#       re-bakes the snapshot through the DISARMED branch. The SKY pixels must
#       come back byte for byte — the branch is untaken, not approximately
#       untaken — and the lit spheres within 1 LSB on a handful of pixels, which
#       is what the SH readback and the crossfade bookkeeping carry across the
#       cranked interval (measured: 1 pixel of 470k). Same-process determinism
#       only; cross-build byte identity is not claimed (S2 adjudication).
#
#   (d) PROPAGATION LATENCY. A haze knob is presentation tier: it must never
#       re-bake a LUT (mediumHash does not see it), but it does have to reach
#       reflections. The route is the IBL feed's snapshot trigger, which now
#       reads SkyAtmosphereData::hazePresentationHash beside mediumHash. The
#       frames and wall-clock seconds between the knob edit and the first frame
#       whose AMBIENT has moved are measured here and gated under a ceiling, so a
#       future change that quietly drops the stamp (leaving haze to wait for the
#       sun to move, i.e. forever in a still scene) fails loudly. Note the sun is
#       frozen and the scene is static: with the stamp gone there is no other
#       trigger and this leg never completes.
#
# SCENE: procedural sky, sun 45 deg up and ahead of the camera, no ground (the
# grid shader is not in the forward lighting funnel), NO reflection probe — the
# specular and the SH ambient can only come from the sky maps.
#
# Not ork.testing capture_app: that harness drives ONE capture and cannot host a
# scenegraph app; this gate needs seven captures plus a polling loop in one warm
# process. The verdict-before-teardown protocol is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import math
import time
import numpy
from PIL import Image as PILImage
from orkengine.core import vec3, vec4
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict, Watchdog

WIDTH, HEIGHT     = 800, 600
EYE_Y             = 2.0
CAM_NEAR, CAM_FAR = 0.5, 20000.0
FOVY_DEG          = 45.0

SUN_ELEV_DEG  = 45.0
SUN_AZIM_DEG  = 0.0       # AHEAD of the camera, so the sphere's well-resolved
                          # backward hemisphere is the smooth anti-solar sky
SUN_INTENSITY = 3.0
SKY_EXPOSURE  = 4.0       # both spheres and the sky must stay off the 255 rail

# the owner's kind of config: a thick, tall, isotropic layer
HAZE_DENSITY      = 1.0    # 1/km at layer base
HAZE_SCALE_HEIGHT = 2.0    # km — reaches the upper sky, not just the skyline
HAZE_PHASE_G      = 0.0
HAZE_SCATTER_TINT = (1.00, 0.95, 0.85)
HAZE_INSCAT_TINT  = (1.00, 0.94, 0.82)
HAZE_MAX_DIST_KM  = 160.0

# THE MIRROR SPHERE IS NEAR, and its angular size is what the reflection maths
# actually depends on (parallel view rays across the disc, not distance). At 5 m
# the FORWARD aerial-perspective march between eye and sphere is worth well under
# a level, so leg (a) reads the reflected sky rather than the reflected sky plus
# 40 m of airlight (measured: that airlight was 4.4 of a 7.1 level residual).
MIRROR_DIST   = 5.0
MIRROR_ANGTAN = 0.070      # angular radius as a tangent (~4 deg, ~53 px): the
                           # sphere's angular resolution is ~180 deg / radius_px,
                           # and the probe patch must not average a wide cone
MATTE_DIST    = 3.0        # near enough that the forward march is negligible
MATTE_ANGTAN  = 0.12
MATTE_COLOR   = (0.55, 0.55, 0.55)
BALL_MODEL_R  = 1.09       # pbr_calib.glb ball radius at scale 1
AWAY_POS      = vec3(0.0, EYE_Y, -4000.0)   # behind the camera: out of frame

SETTLE_FRAMES = 120        # after scene build, before the first capture
STEP_SETTLE   = 60         # after each camera / transform change
# A HAZE EDIT IS NOT A CAMERA MOVE: it has to travel through a whole IBL refilter
# cycle (leg d measures that at ~130 frames here), so the captures that follow one
# wait an order of magnitude longer than the ones that follow a transform.
IBL_SETTLE    = 600
POLL_LIMIT    = 1200       # frames the latency poll may run before giving up

# ---- bars (all measured on this scene; see the verdict detail line) ----------
PROBE_W         = 0.5      # radius fraction above the sphere centre; reflects
                           # ~58 degrees up and BACKWARD (see _probeElevDeg)
PATCH_R         = 2        # half-width of every sampled patch, px (~5 deg of
                           # reflected cone on the sphere, ~0.3 deg on the sky)
MIN_MASK_PX     = 200      # a sphere the diff mask cannot see is a FAIL
POST_FRACTION   = 0.30     # leg a: err_post / ref_prefix ceiling. MEASURED
                           # 0.132 (2.3 of a 17.6 level pre-fix mismatch); the
                           # rest is the probe cone the sphere averages over,
                           # which the clear-sky baseline below prices at 0.4.
BASE_FRACTION   = 0.30     # leg a: err_baseline / ref_prefix ceiling (harness's
                           # own systematic; measured 0.023)
REF_MIN_LEVELS  = 12.0     # leg a: the haze must actually move the probe sky,
                           # or the whole comparison is vacuous
MATTE_MIN_MOVE  = 3.0      # leg b: 8-bit levels of ambient shift
CLIP_MAX        = 250.0    # no patch this gate reads may sit on the rail
LATENCY_MAX_S   = 5.0      # leg d ceiling, wall clock. Measured 0.49 s (132
                           # frames): one refilter cycle at the default snapshot
                           # extent, offscreen. The ceiling is an order of
                           # magnitude above it because the cycle is paced by
                           # GPU work, not by this gate.

OUT_DIR = os.environ.get("HAZE_G6_OUT", "/tmp/haze_ibl_coupling")


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


class HazeIblCouplingApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._shots = {}
    self._inflight = None
    self._nodes = {}
    self._latency = None
    self._poll_state = None
    # capture order; each entry's action leaves the state the NEXT one reads
    self._steps = [("off_full",     self._hideMirror),
                   ("off_nomirror", self._showMirrorHideMatte),
                   ("off_nomatte",  self._showMatteAimProbe),
                   ("off_probe",    self._aimForward),
                   ("hazy_full",    self._aimProbe),
                   ("hazy_probe",   self._disarmAndAimForward),
                   ("off_again",    None)]
    self.SGC = self.addComponent(
        "std_scenegraph", StandardSceneGraphComponent,
        eye=vec3(0, EYE_Y, 0), tgt=vec3(0, EYE_Y, 100), up=vec3(0, 1, 0),
        explicit_near_far=True, near=CAM_NEAR, far=CAM_FAR,
        grid_variant=None,          # no ground: the grid is not in the funnel
        sg_params={
          "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          "SpecularIntensity": 1.0,
          "AmbientLevel":     vec3(0.0),   # every ambient level here is the sky's
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    self.ezapp.topWidget.enableUiDraw()
    SGC.pbr_common.enable_skybox = True

    self.atmo = lev2.SkyAtmosphereData()
    self.atmo.sky_exposure = SKY_EXPOSURE
    self.atmo.ibl_crossfade_frames = 0   # a half-faded IBL is not a reading
    self.atmo.aerial_perspective_enable = False
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    self._nodes["mirror"] = SGC.createBallNode(
        "mirror", ctx=ctx,
        position=vec3(0, EYE_Y, MIRROR_DIST),
        scale=MIRROR_ANGTAN * MIRROR_DIST / BALL_MODEL_R,
        color=vec4(1, 1, 1, 1), metallic=1.0, roughness=0.02)
    self._nodes["matte"] = SGC.createBallNode(
        "matte", ctx=ctx,
        position=vec3(-1.3, EYE_Y, MATTE_DIST),
        scale=MATTE_ANGTAN * MATTE_DIST / BALL_MODEL_R,
        color=vec4(MATTE_COLOR[0], MATTE_COLOR[1], MATTE_COLOR[2], 1),
        metallic=0.0, roughness=0.85)
    self._home = {k: vec3(n.worldTransform.translation)
                  for k, n in self._nodes.items()}

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = SUN_INTENSITY
    sun.shadowCaster = False
    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    # DirectionalLight::direction() is the light's TRAVEL direction, so aiming
    # it from the sun's position at the origin makes dir_to_sun the negation.
    sun.lookAt(vec3(d.x, d.y, d.z) * 1000.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

    SGC.camera.perspective(CAM_NEAR, CAM_FAR, FOVY_DEG)
    self._aimForward()

  def _onUpdate(self, updinfo):
    pass   # static: the only frame-to-frame differences are the ones we make

  ##############################################################
  # the states

  def _aimForward(self):
    self.SGC.camera.lookAt(vec3(0, EYE_Y, 0), vec3(0, EYE_Y, 100), vec3(0, 1, 0))

  def _aimProbe(self):
    """straight down the direction the sphere's probe point reflects: up and
    BACKWARD. The centre pixel of this capture is that direction exactly."""
    e = math.radians(self._probeElevDeg())
    self.SGC.camera.lookAt(
        vec3(0, EYE_Y, 0),
        vec3(0, EYE_Y + 100.0 * math.sin(e), -100.0 * math.cos(e)),
        vec3(0, 1, 0))

  def _park(self, key, away):
    self._nodes[key].worldTransform.translation = AWAY_POS if away else self._home[key]

  def _hideMirror(self):
    self._park("mirror", True)

  def _showMirrorHideMatte(self):
    self._park("mirror", False)
    self._park("matte", True)

  def _showMatteAimProbe(self):
    self._park("matte", False)
    # the two locator captures are in hand now, so the probe point (and with it
    # the direction the probe camera has to be aimed at) is known.
    self._mask = self._locate()
    self._aimProbe()

  def _crank(self):
    a = self.atmo
    a.aerial_perspective_enable = True
    a.haze_density = HAZE_DENSITY
    a.haze_scale_height = HAZE_SCALE_HEIGHT
    a.haze_phase_g = HAZE_PHASE_G
    a.haze_scatter_tint = vec3(*HAZE_SCATTER_TINT)
    a.haze_inscatter_tint = vec3(*HAZE_INSCAT_TINT)
    a.haze_max_distance_km = HAZE_MAX_DIST_KM

  def _disarmAndAimForward(self):
    # the knobs stay cranked: only the ARMED gate moves, which is what makes the
    # last capture a test of the disarmed branch rather than of zero density.
    self.atmo.aerial_perspective_enable = False
    self._aimForward()

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

  def _reap(self):
    """None until the in-flight capture is readable; then the HxWx3 array."""
    fut, buf = self._inflight
    if not bool(fut.is_ready):
      return None
    w, h = buf.width, buf.height
    img = numpy.array(buf, dtype=numpy.uint8).reshape(h, w, 4)[..., :3].copy()
    self._inflight = None
    return img

  def _collect(self, key):
    img = self._reap()
    if img is None:
      return False
    self._shots[key] = img
    try:
      os.makedirs(OUT_DIR, exist_ok=True)
      PILImage.fromarray(img[::-1, ::-1]).save(os.path.join(OUT_DIR, "%s.png" % key))
    except Exception as e:
      print("[haze-g6] png write failed: %r" % (e,), flush=True)
    print("[haze-g6] captured %-13s mean=%.3f max=%d"
          % (key, float(img.mean()), int(img.max())), flush=True)
    return True

  ##############################################################
  # frame driver: settle -> capture -> act, with the latency poll spliced in
  # ahead of the hazy_full capture.
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
      return

    step = self._phase // 2
    if step >= len(self._steps):
      return
    key, action = self._steps[step]

    # THE LATENCY POLL. It replaces the fixed settle in front of hazy_full: the
    # knob is cranked at the top of it and every frame after that is sampled
    # until the reflection moves.
    if key == "hazy_full" and (self._phase % 2) == 0:
      # the poll owns both halves of this step (settle AND capture), so it
      # advances the phase by two.
      if self._pollForPropagation(ctx):
        if action is not None:
          action()
        self._phase += 2
        self._phase_frame = self._frame
      return

    if (self._phase % 2) == 0:                # settle, then issue
      # off_again follows a haze DISARM, which is an IBL edit like the crank was
      settle = SETTLE_FRAMES if step == 0 else (IBL_SETTLE if key == "off_again" else STEP_SETTLE)
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

  def _pollForPropagation(self, ctx):
    """crank the haze, then sample the frame until the AMBIENT tier moves.
    Returns True once it has moved and settled.

    THE PROBE IS THE NEAR MATTE SPHERE, not the mirror: the forward
    aerial-perspective march reaches the mirror sphere (40 m, ~4 levels) on the
    very next frame, and a poll watching it would time the FORWARD path and
    capture the frame long before the IBL cycle it is supposed to be waiting
    for. At 3 m the march is worth half a level, so a 3-level move there is the
    ambient tier and nothing else."""
    if self._poll_state is None:
      base = self._shots["off_full"].astype(numpy.float64)
      self._poll_ref = self._mattePatch(base)
      self._poll_state = "search"
      self._crank()
      self._poll_t0 = time.time()
      self._poll_f0 = self._frame
      self._settle_after = None
      print("[haze-g6] haze knobs cranked at frame %d" % self._frame, flush=True)
      return False

    if self._poll_state == "search":
      if self._inflight is None:
        self._issueCapture(ctx)
        return False
      img = self._reap()
      if img is None:
        return False
      p = self._mattePatch(img.astype(numpy.float64))
      moved = float(numpy.abs(p - self._poll_ref).max())
      if moved >= MATTE_MIN_MOVE:
        self._latency = (self._frame - self._poll_f0, time.time() - self._poll_t0)
        print("[haze-g6] ambient moved %.1f levels after %d frames / %.3f s"
              % (moved, self._latency[0], self._latency[1]), flush=True)
      elif (self._frame - self._poll_f0) > POLL_LIMIT:
        self._latency = (-1, -1.0)          # gated as a failure below
      if self._latency is not None:
        self._poll_state = "settle"
        self._settle_after = self._frame
      return False

    # moved: let the cycle finish and the maps settle, then hand the frame over
    if self._poll_state == "settle":
      if self._frame < self._settle_after + STEP_SETTLE:
        return False
      self._issueCapture(ctx)
      self._poll_state = "final"
      return False

    return self._collect("hazy_full")

  ##############################################################
  # observables
  ##############################################################

  def _locate(self):
    """{key: (cx, cy, radius)} for each sphere, from the difference between the
    all-present capture and the capture that parked that sphere behind the
    camera. No projection model and no silhouette assumption — the mask IS the
    pixels the sphere occupies."""
    full = self._shots["off_full"].astype(numpy.float64)
    out = {}
    notes = []
    for key, shot in (("mirror", "off_nomirror"), ("matte", "off_nomatte")):
      diff = numpy.abs(full - self._shots[shot].astype(numpy.float64)).max(axis=2)
      m = diff > 4.0
      n = int(m.sum())
      if n < MIN_MASK_PX:
        out[key] = None
        notes.append("%s:%dpx" % (key, n))
        continue
      ys, xs = numpy.nonzero(m)
      cx, cy = int(round(xs.mean())), int(round(ys.mean()))
      rad = max(2, int(round(0.25 * ((ys.max() - ys.min()) + (xs.max() - xs.min())))))
      out[key] = (cx, cy, rad)
      notes.append("%s:%dpx@(%d,%d)r%d" % (key, n, cx, cy, rad))
    self._locate_detail = " ".join(notes)
    print("[haze-g6] sphere masks: %s" % self._locate_detail, flush=True)
    return out

  def _imageUp(self):
    """row step that walks toward the sky, from the sky's own above/below-horizon
    asymmetry in the forward-looking capture."""
    off = self._shots["off_full"].astype(numpy.float64)
    q = off.shape[0] // 4
    top, bot = float(off[:q].mean()), float(off[-q:].mean())
    return (-1 if top > bot else +1), top, bot

  def _probePoint(self):
    cx, cy, rad = self._mask["mirror"]
    up, _t, _b = self._imageUp()
    return cx, int(round(cy + up * PROBE_W * rad))

  def _probeElevDeg(self):
    """elevation of the direction the probe PIXEL actually reflects.

    The parallel-ray form (2*asin(w)) is short by the screen angle of the point
    itself: the ray through a point w radii up the disc is already tilted up by
    beta = asin(w*sin(alpha)), and reflecting a tilted ray about the same normal
    lands at 2*asin(w) - beta. alpha is the sphere's own angular radius
    (atan(R/D), from its placement) — this is a model of the SPHERE, not of the
    camera, so it costs nothing in the projection-model discipline. Uncorrected
    it is a ~2 degree bias, which is free under a clear sky and worth ~2 levels
    under a thick one, where the sky's elevation gradient is steep.

    w is recomputed from the ROUNDED pixel row rather than assumed to be
    PROBE_W: the row is where the patch is actually read."""
    cx, cy, rad = self._mask["mirror"]
    px, py = self._probePoint()
    w = min(abs(py - cy) / float(rad), 1.0)
    alpha = math.atan(MIRROR_ANGTAN)
    beta = math.asin(min(w * math.sin(alpha), 1.0))
    return math.degrees(2.0 * math.asin(w) - beta)

  def _mattePatch(self, img):
    mcx, mcy, _mrad = self._mask["matte"]
    return self._patch(img, mcx, mcy, r=6)

  @staticmethod
  def _patch(img, cx, cy, r=PATCH_R):
    return img[cy - r:cy + r + 1, cx - r:cx + r + 1].reshape(-1, 3).mean(axis=0)

  def _centrePatch(self, img):
    h, w, _ = img.shape
    return self._patch(img, w // 2, h // 2)

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    need = ("off_full", "off_nomirror", "off_nomatte", "off_probe",
            "hazy_full", "hazy_probe", "off_again")
    if any(self._shots.get(k) is None for k in need):
      self._finish(verdict(False, "missing captures"))
      return

    off      = self._shots["off_full"].astype(numpy.float64)
    hazy     = self._shots["hazy_full"].astype(numpy.float64)
    sky_off_shot = self._shots["off_probe"].astype(numpy.float64)
    sky_hzy_shot = self._shots["hazy_probe"].astype(numpy.float64)

    check("both_spheres_located",
          self._mask.get("mirror") is not None and self._mask.get("matte") is not None,
          self._locate_detail)
    up, top, bot = self._imageUp()
    check("image_orientation_decisive", max(top, bot) / max(min(top, bot), 1e-6) >= 1.15,
          "top=%.1f bottom=%.1f up_row_step=%+d" % (top, bot, up))
    if failures:
      self._finish(verdict(False, "setup: " + ",".join(failures)))
      return

    zx, zy = self._probePoint()
    mir_off  = self._patch(off, zx, zy)
    mir_hzy  = self._patch(hazy, zx, zy)
    sky_off  = self._centrePatch(sky_off_shot)
    sky_hzy  = self._centrePatch(sky_hzy_shot)

    err_base = float(numpy.abs(mir_off - sky_off).mean())
    ref_pre  = float(numpy.abs(mir_off - sky_hzy).mean())
    err_post = float(numpy.abs(mir_hzy - sky_hzy).mean())

    print("[haze-g6] mirror probe px=(%d,%d)  mirror_off=(%5.1f,%5.1f,%5.1f) "
          "mirror_hazy=(%5.1f,%5.1f,%5.1f)" % (zx, zy, mir_off[0], mir_off[1],
          mir_off[2], mir_hzy[0], mir_hzy[1], mir_hzy[2]), flush=True)
    print("[haze-g6] probe sky    off=(%5.1f,%5.1f,%5.1f) hazy=(%5.1f,%5.1f,%5.1f)"
          % (sky_off[0], sky_off[1], sky_off[2],
             sky_hzy[0], sky_hzy[1], sky_hzy[2]), flush=True)

    hottest = max(float(numpy.max(p)) for p in (mir_off, mir_hzy, sky_off, sky_hzy))
    check("no_patch_is_clipped", hottest <= CLIP_MAX, "hottest=%.1f" % hottest)

    # ---- leg a: the reflection is the sky
    check("a_haze_moves_the_probe_sky", ref_pre >= REF_MIN_LEVELS,
          "pre_fix_mismatch=%.1f levels (floor %.1f)" % (ref_pre, REF_MIN_LEVELS))
    check("a_harness_baseline_valid", err_base <= BASE_FRACTION * max(ref_pre, 1e-6),
          "err_baseline=%.1f (%.3f of pre-fix)" % (err_base, err_base / max(ref_pre, 1e-6)))
    check("a_reflection_matches_hazed_sky", err_post <= POST_FRACTION * max(ref_pre, 1e-6),
          "err_post=%.1f (%.3f of pre-fix %.1f, ceiling %.2f)"
          % (err_post, err_post / max(ref_pre, 1e-6), ref_pre, POST_FRACTION))

    # ---- leg b: the diffuse tier heard about it too
    mcx, mcy, mrad = self._mask["matte"]
    mat_off = self._mattePatch(off)
    mat_hzy = self._mattePatch(hazy)
    mat_move = float(numpy.abs(mat_hzy - mat_off).max())
    mat_lift = float(mat_hzy.mean() - mat_off.mean())
    print("[haze-g6] matte px=(%d,%d) off=(%5.1f,%5.1f,%5.1f) hazy=(%5.1f,%5.1f,%5.1f) "
          "move=%.1f lift=%+.1f" % (mcx, mcy, mat_off[0], mat_off[1], mat_off[2],
          mat_hzy[0], mat_hzy[1], mat_hzy[2], mat_move, mat_lift), flush=True)
    check("b_matte_ambient_shifted", mat_move >= MATTE_MIN_MOVE,
          "max_channel_move=%.1f levels (floor %.1f)" % (mat_move, MATTE_MIN_MOVE))
    check("b_matte_ambient_lifted", mat_lift > 0.0,
          "mean_lift=%+.2f levels" % mat_lift)

    # ---- leg c: the disarmed branch is untaken, not nearly untaken.
    # The SKY has to come back byte for byte — those pixels are the snapshot's
    # own sibling pass, and a disarmed branch that touched them would show here
    # first. The lit spheres are held to 1 LSB on a handful of pixels instead:
    # they arrive through the SH readback and the crossfade bookkeeping, which
    # carry history across the cranked interval (measured: exactly 1 pixel at 1
    # level, on the matte sphere).
    again = self._shots["off_again"].astype(numpy.float64)
    delta = numpy.abs(off - again).max(axis=2)
    dmax = int(delta.max())
    ndiff = int((delta > 0).sum())
    sky_mask = numpy.ones(delta.shape, dtype=bool)
    for key in ("mirror", "matte"):
      cx, cy, rad = self._mask[key]
      r = rad + 4
      sky_mask[max(cy - r, 0):cy + r, max(cx - r, 0):cx + r] = False
    sky_dmax = int(delta[sky_mask].max())
    check("c_disarmed_sky_is_byte_identical", sky_dmax == 0,
          "sky_max_delta=%d after a cranked-then-disarmed snapshot re-bake" % sky_dmax)
    check("c_disarmed_frame_is_deterministic", dmax <= 1 and ndiff <= 16,
          "max_delta=%d on %d px of %d" % (dmax, ndiff, delta.size))

    # ---- leg d: a live haze edit reaches the reflection, and how fast
    lat_f, lat_s = self._latency if self._latency else (-1, -1.0)
    check("d_haze_edit_propagates", lat_f >= 0,
          "frames=%d wall=%.3f s" % (lat_f, lat_s))
    check("d_propagation_within_ceiling", 0 <= lat_s <= LATENCY_MAX_S,
          "wall=%.3f s (ceiling %.1f s)" % (lat_s, LATENCY_MAX_S))

    ok = (len(failures) == 0)
    detail = ("err_base=%.1f ref_prefix=%.1f err_post=%.1f (%.3f) matte_move=%.1f "
              "matte_lift=%+.1f disarmed_delta=%d latency=%df/%.2fs"
              % (err_base, ref_pre, err_post, err_post / max(ref_pre, 1e-6),
                 mat_move, mat_lift, dmax, lat_f, lat_s))
    if failures:
      detail += " failed=" + ",".join(failures)
    self._finish(verdict(ok, detail))

  def _finish(self, rc):
    self._exit_code = rc
    self._done = True
    self.ezapp.signalExit()


def main():
  wd = Watchdog(300.0, label="haze_ibl_coupling").arm()
  app = HazeIblCouplingApp()
  app.ezapp.mainThreadLoop()
  wd.disarm()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before every capture completed")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
