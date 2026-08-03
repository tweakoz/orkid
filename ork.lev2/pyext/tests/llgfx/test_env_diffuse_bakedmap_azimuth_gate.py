#!/usr/bin/env ork.python
################################################################################
# BAKED-ENVMAP DIFFUSE AZIMUTH gate.
#
# test_env_diffuse_convolution_gate.py proves the diffuse env lookup integrates
# the sky around the shaded normal, but it drives the PROCEDURAL sky, whose
# equirect snapshot this repo also writes. That leaves one hole: the baked .xir
# equirects are authored content, and a mirrored lookup reading mirrored content
# would have looked right by DOUBLE ERROR. This gate closes it on a baked map.
#
# WHAT IS COMPARED. Both sides come out of the same engine, the same frame's
# worth of state, and the same baked .xir:
#
#   the map's bright side   the SKYBOX, which is what a viewer actually sees:
#                           the camera is swept around the horizon and the
#                           frame's sky luminance is measured at each bearing.
#                           The sweep's CIRCULAR FIRST MOMENT is the bright
#                           side, in WORLD azimuth, with no convention assumed
#                           anywhere in this file.
#
#   the ambient's warm side the DIFFUSE ENV alone on a white lambertian ball
#                           (SpecularIntensity 0, AmbientLevel 0), sampled by
#                           forward-projecting known world normals through the
#                           engine's own camera — no camera inverse, no assumed
#                           handedness.
#
# The assertion is that they are the SAME side. Before envtools
# env_equirectangular2 undid the mirrored-U bearing, they were opposite sides:
# the ball's warm hemisphere sat 180 degrees from the sky's glow.
#
# BOTH BEARINGS ARE THE SAME DIPOLE QUANTITY, which is what makes comparing them
# an instrument rather than a coincidence. The sweep moment is the l=1 term of
# the frame-mean-versus-bearing curve: at 12 equally spaced bearings any
# isotropic pedestal cancels exactly, and the statistic is CONTINUOUS instead of
# quantized to the 30-degree sweep grid (the grid argmax this gate used to take
# reads 300.0 where the moment reads 282.4 on blender_sunset — an instrument
# error of a full half-step, charged to the engine).
#
# Three readings of the same fact are reported, because they fail differently:
#   * the WARM BEARING — a dipole fit a + c.n over the sampled normals. A
#     cosine convolution is a low-pass on the sphere, so the field it produces
#     is dominated by its l=1 term and c points where the light came from: the
#     azimuth of c is an ANGLE, and a mirrored lookup moves it by 180 degrees
#     rather than by a few percent. This is the headline number.
#   * hemisphere means — every visible normal, split by the sign of n . b
#     (b = the bright bearing). Bulk, insensitive to any single pixel, but a
#     RATIO: heavily diluted by the isotropic part of a real sky (measured
#     1.30x correct against 1.06x mirrored on blender_sunset).
#   * the mirror probe — one pair of normals at +-45 degrees off the view axis,
#     exact mirror images of each other about the plane b = 0. The kernel's own
#     error is symmetric across that plane, so the pair cancels it.
#
# THE WARM BEARING IS READ FROM A RING OF SIX CAMERAS, not from one. Over a
# camera-facing cap the constant term and the dipole component ALONG the view
# axis are strongly correlated, so a single fit trusts a component it cannot
# condition; measured, that biases one camera's answer by up to +-30 degrees
# (three-camera subsets of this very ring span -37..+26 on a synthetic map
# carrying ONE blob at a known bearing). Only the component PERPENDICULAR to
# each view axis is kept, and the six perpendiculars — spaced 60 degrees, so the
# ring is symmetric both about the candidate bearing and about its opposite, and
# the L2 leak that depends on the light-to-axis angle cancels — are solved
# together for the one horizontal dipole.
#
# THE FIRST RING CAMERA IS PERPENDICULAR to the bright bearing, and its frame is
# the one the hemisphere means and the mirror probe read, so +b and -b are
# equally visible and equally foreshortened there. A camera anywhere else would
# compare a well-lit normal against one nearer the silhouette and call the
# framing an orientation.
#
# THE FLOAT SURFACE (RGBA32F outputRTG through the scenegraph's own compositor)
# and the verdict-before-teardown protocol follow test_env_hdr_range_gate.py.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

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
CAM_DIST      = 7.0
BALL_POS      = vec3(0, 2, 0)
BALL_SCALE    = 1.6

# A sunset map: one horizon glow, everything else dim. The gate does not trust
# that description — SKY_CONTRAST_MIN below makes it an assertion, so swapping
# in an azimuthally flat map fails loudly instead of passing on noise.
ENVMAP        = "<ork_envmaps2>/blender_sunset.xir"

SWEEP_STEP_DEG = 30.0
SWEEP_AZIMUTHS = [i * SWEEP_STEP_DEG for i in range(int(360.0 / SWEEP_STEP_DEG))]

# ball-read camera bearings, relative to the measured bright bearing. Offset 90
# comes FIRST because that frame — the only one with the bright bearing across
# the screen — is also what the hemisphere means and the mirror probe read.
RING_OFFSETS  = [90.0 + k * 60.0 for k in range(6)]

SETTLE_FRAMES = 8
WARM_FRAMES   = 48
WAIT_SECONDS  = 60.0

NORMAL_CANDIDATES = 1200
# a dot against the ball-to-eye axis. Well clear of the silhouette (which sits
# at R/d = 0.23 here) so a FOOTPRINT box never straddles it into the sky, which
# on this gate is BRIGHTER than the receiver.
FACING_MIN        = 0.45
# and clear of the split plane itself, where a normal carries no opinion about
# which side it is on.
SPLIT_FLOOR       = 0.15
FOOTPRINT         = 1
MIN_SAMPLES       = 80

SKY_CONTRAST_MIN  = 1.50   # brightest sweep bearing / dimmest, ratio
WARM_RATIO_MIN    = 1.15   # warm hemisphere mean / cold, and the mirror probe
BALL_LIT_MIN      = 1.0e-5 # the receiver must have received something
# the fitted warm bearing against the map's.
#
# WHY 20. Both bearings are now the same dipole quantity (see the header), which
# is what makes a bar this tight defensible:
#   * the old like-for-like instrument — grid argmax against a SINGLE-camera fit
#     — reads 41 degrees apart on blender_sunset with a correct engine. Every
#     degree of that is instrument: half a sweep step of quantization plus the
#     unconditioned along-axis component of a one-camera fit. A bar under 41
#     therefore cannot be met by loosening anything; it can only be met by
#     measuring better.
#   * the six-camera ring's own spread is under 10 degrees (measured on
#     synthetic single-blob maps whose bright bearing is known by construction:
#     ring-versus-skybox deltas of -7.5 and -9.3 degrees, against +-30 for
#     single cameras). 20 is comfortably outside that, so a correct engine has
#     margin and a flaky read is not a failure.
#   * the failure class this gate exists to catch is the 180-degree mirror, and
#     any bar below 90 names it. 20 additionally catches gross partial
#     misorientations (a swapped axis, a half-texel-scale bake shear) that 45
#     waved through.
BEARING_TOL_DEG   = 20.0


def dir_h(azimuth_deg):
  """unit horizontal world vector at an azimuth; 0 = +Z, matching the sun
  bearing convention used across the sky gates."""
  a = math.radians(azimuth_deg)
  return numpy.array([math.sin(a), 0.0, math.cos(a)])


class FloatOutSGC(StandardSceneGraphComponent):
  """scenegraph that composites into an RGBA32F RtGroup (see the header)."""

  def _onGpuInit(self, ctx):
    rtg = lev2.RtGroup(ctx, WIDTH, HEIGHT)
    rtg.name = "bakedazim_f32"
    rtg.createBuffer(tokens.RGBA32F, tokens.color)
    self.float_rtg = rtg
    self.sg_params.outputRTG = rtg
    super()._onGpuInit(ctx)


class BakedAzimuthApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self._frame = 0
    self._state = 0
    self._state_frame = 0
    self._state_time = time.time()
    self._done = False
    self._exit_code = None
    self._realized = False
    self._inflight = None
    self._shots = {}
    self._sweep = 0
    self._sky = {}
    self._ring = 0
    self._ringcams = []
    self._ringfits = []
    self.SGC = self.addComponent(
        "std_scenegraph", FloatOutSGC,
        eye=vec3(0, 2, -CAM_DIST), tgt=vec3(0, 2, 0), up=vec3(0, 1, 0),
        grid_variant=None,
        sg_params={
          "SkyboxTexPathStr": ENVMAP,
          "SkyboxIntensity":  1.0,
          "DiffuseIntensity": 1.0,
          # SPECULAR OFF: leaves the diffuse env term as the only light on the
          # receiver, so the ball reads MapDiffuseEnv and nothing else.
          "SpecularIntensity": 0.0,
          "AmbientLevel":     vec3(0),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    self.ezapp.topWidget.enableUiDraw()
    SGC = self.SGC
    self.ball = SGC.createBallNode("recv_diffuse", ctx=ctx, position=BALL_POS,
                                   color=vec4(1, 1, 1, 1), metallic=0.0, roughness=1.0,
                                   scale=BALL_SCALE)
    # world radius from the model's own AABB half-extents, not from the framing
    # (see test_env_diffuse_convolution_gate.py: boundingRadius is the CORNER).
    whd = SGC._ball_model.aabb_whd
    self.ball_radius = float(min(whd.x, whd.y, whd.z)) * BALL_SCALE

    # DIRECTION SOURCE ONLY at zero intensity: every photon in the frame is
    # image-based, so nothing but the env map can produce an azimuth.
    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = 0.0
    sun.shadowCaster = False
    self.sun = sun
    sun.lookAt(vec3(0, 100, 0), vec3(0, 0, 0), vec3(0, 0, 1))
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

  def _onUpdate(self, updinfo):
    pass

  ##############################################################

  def _pbr(self):
    return self.SGC.pbr_common

  def _note(self, txt):
    print("[baked-azim] %s" % txt, flush=True)

  def _fail(self, why):
    verdict(False, why)
    self._exit_code = 1
    self._done = True
    self.ezapp.signalExit()

  def _aimCamera(self, azimuth_deg):
    """look along the given world azimuth, ball centred. Mirrors what the UI
    camera event path does — lookAt alone does not reach the render camera."""
    b = dir_h(azimuth_deg)
    c = numpy.array([BALL_POS.x, BALL_POS.y, BALL_POS.z])
    e = c - b * CAM_DIST
    SGC = self.SGC
    SGC.uicam.lookAt(vec3(float(e[0]), float(e[1]), float(e[2])),
                     BALL_POS, vec3(0, 1, 0))
    SGC.uicam.updateMatrices()
    SGC.camera.copyFrom(SGC.uicam.cameradata)

  ##############################################################
  # capture plumbing
  ##############################################################

  def _issueCapture(self, ctx, key):
    buf = lev2.CaptureBuffer()
    fut = ctx.FBI.captureAsFormat(self.SGC.float_rtg.buffer(0), buf, "RGBA32F")
    self._inflight = (fut, buf, key)

  def _collect(self):
    fut, buf, key = self._inflight
    if not bool(fut.is_ready):
      return None
    w, h = buf.width, buf.height
    img = numpy.array(buf, dtype=numpy.float32).reshape(h, w, 4)[..., :3].copy()
    self._shots[key] = img
    self._inflight = None
    return key

  ##############################################################
  # observables
  ##############################################################

  def _pixelOf(self, wpos, w, h):
    ndc = self.SGC.camera.project(float(w) / float(h), wpos)
    return ((ndc.x * 0.5 + 0.5) * w, (ndc.y * 0.5 + 0.5) * h)

  def _ballDisc(self, w, h):
    """projected ball centre and silhouette radius in pixels. A sphere's
    silhouette normals satisfy n.u == R/d, so the radius is predictable from R
    and the camera alone."""
    eye = self.SGC.camera.eye
    c = numpy.array([BALL_POS.x, BALL_POS.y, BALL_POS.z])
    e = numpy.array([eye.x, eye.y, eye.z])
    d = float(numpy.linalg.norm(e - c))
    u = (e - c) / d
    R = self.ball_radius
    ct = R / d
    st = math.sqrt(max(1.0 - ct * ct, 0.0))
    a = numpy.array([0.0, 1.0, 0.0])
    if abs(float(a @ u)) > 0.9:
      a = numpy.array([1.0, 0.0, 0.0])
    t = numpy.cross(u, a)
    t = t / numpy.linalg.norm(t)
    p_sil = c + R * (ct * u + st * t)
    cx, cy = self._pixelOf(vec3(float(c[0]), float(c[1]), float(c[2])), w, h)
    sx, sy = self._pixelOf(vec3(float(p_sil[0]), float(p_sil[1]), float(p_sil[2])), w, h)
    return cx, cy, math.hypot(sx - cx, sy - cy)

  def _skyMean(self, img):
    """mean sky luminance of the frame with the receiver's disc cut out. The cut
    is geometric (projected radius, 1.25x) rather than a brightness threshold,
    so it cannot adapt to the very asymmetry being measured."""
    h, w, _ = img.shape
    cx, cy, rad = self._ballDisc(w, h)
    ys, xs = numpy.mgrid[0:h, 0:w]
    sky = ((xs - cx) ** 2 + (ys - cy) ** 2) > (rad * 1.25) ** 2
    return float(img[sky].mean())

  def _brightBearing(self):
    """the sweep's circular first moment, in world azimuth. The sweep bearings
    are equally spaced, so an azimuthally flat sky contributes exactly nothing
    here and only the l=1 asymmetry survives — the same quantity the ball's
    dipole fit reports, and continuous rather than pinned to the sweep grid."""
    az = numpy.array(sorted(self._sky.keys()))
    s = numpy.array([self._sky[a] for a in az])
    r = numpy.radians(az)
    cx = float((s * numpy.sin(r)).sum())
    cz = float((s * numpy.cos(r)).sum())
    # anisotropy: |first moment| over the mean, i.e. how much of the sweep is
    # the dipole. Reported, not asserted — SKY_CONTRAST_MIN is the assertion.
    aniso = math.hypot(cx, cz) / max(float(s.sum()), 1.0e-12)
    return math.degrees(math.atan2(cx, cz)) % 360.0, aniso

  def _capNormals(self):
    """Fibonacci normals over the camera-facing cap, and the camera axis. Used
    by the ring fits, which need every facing normal (no split plane) and run at
    bearings where the mirror-closed set of _normalSet would not apply."""
    eye = self.SGC.camera.eye
    c = numpy.array([BALL_POS.x, BALL_POS.y, BALL_POS.z])
    u = numpy.array([eye.x, eye.y, eye.z]) - c
    u = u / numpy.linalg.norm(u)
    i = numpy.arange(NORMAL_CANDIDATES) + 0.5
    y = 1.0 - 2.0 * i / NORMAL_CANDIDATES
    r = numpy.sqrt(numpy.maximum(1.0 - y * y, 0.0))
    ga = math.pi * (3.0 - math.sqrt(5.0))
    n = numpy.stack([r * numpy.cos(ga * i), y, r * numpy.sin(ga * i)], axis=1)
    return n[(n @ u) > FACING_MIN], u

  def _fitRingRead(self, img):
    """one ring camera: dipole fit over its cap, of which only the component
    PERPENDICULAR to the view axis is kept (the along-axis component is
    degenerate against the constant term over a cap and carries the bias this
    ring exists to cancel)."""
    normals, u = self._capNormals()
    vals, ok = self._sampleBall(img, normals)
    n = int(ok.sum())
    if n < MIN_SAMPLES:
      self._fail("ring camera %d: only %d ball samples (min %d)"
                 % (self._ring, n, MIN_SAMPLES))
      return
    A = numpy.concatenate([numpy.ones((n, 1)), normals[ok]], axis=1)
    coef, _, _, _ = numpy.linalg.lstsq(A, vals[ok], rcond=None)
    c = coef[1:4]
    t = numpy.cross(numpy.array([0.0, 1.0, 0.0]), u)
    t = t / numpy.linalg.norm(t)
    self._ringfits.append({"cam_az": self._ringcams[self._ring],
                           "t": t, "c_t": float(c @ t), "n": n,
                           "iso": float(coef[0])})
    self._note("ring camera %d at %.1f deg: c.t=%.6g iso=%.6g n=%d"
               % (self._ring, self._ringcams[self._ring], float(c @ t),
                  float(coef[0]), n))

  def _ringBearing(self):
    """solve the six perpendicular components for the one horizontal dipole,
    and report how much the answer depends on any single camera."""
    M = numpy.array([f["t"] for f in self._ringfits])[:, [0, 2]]
    rhs = numpy.array([f["c_t"] for f in self._ringfits])
    cxz, *_ = numpy.linalg.lstsq(M, rhs, rcond=None)
    az = math.degrees(math.atan2(float(cxz[0]), float(cxz[1]))) % 360.0
    spread = 0.0
    for k in range(len(self._ringfits)):
      sel = [j for j in range(len(self._ringfits)) if j != k]
      q, *_ = numpy.linalg.lstsq(M[sel], rhs[sel], rcond=None)
      a = math.degrees(math.atan2(float(q[0]), float(q[1]))) % 360.0
      spread = max(spread, abs((a - az + 180.0) % 360.0 - 180.0))
    return az, spread, cxz

  def _normalSet(self, bright):
    """Fibonacci normals kept where the surface faces the camera squarely and
    sits clearly on one side of the b = 0 plane. The set is MIRROR-CLOSED about
    that plane: each kept normal is emitted with its reflection, so the two
    hemispheres carry identical geometry and the probe below has exact
    partners."""
    eye = self.SGC.camera.eye
    c = numpy.array([BALL_POS.x, BALL_POS.y, BALL_POS.z])
    u = numpy.array([eye.x, eye.y, eye.z]) - c
    u = u / numpy.linalg.norm(u)
    if abs(float(u @ bright)) > 1.0e-4:
      self._fail("camera axis is not perpendicular to the bright bearing "
                 "(u.b=%.6f): the two hemispheres would not be equally visible"
                 % float(u @ bright))
      return numpy.zeros((0, 3))
    i = numpy.arange(NORMAL_CANDIDATES) + 0.5
    y = 1.0 - 2.0 * i / NORMAL_CANDIDATES
    r = numpy.sqrt(numpy.maximum(1.0 - y * y, 0.0))
    ga = math.pi * (3.0 - math.sqrt(5.0))
    n = numpy.stack([r * numpy.cos(ga * i), y, r * numpy.sin(ga * i)], axis=1)
    proj = n @ bright
    keep = ((n @ u) > FACING_MIN) & (proj > SPLIT_FLOOR)
    base = n[keep]
    # reflect about the b = 0 plane: n - 2(n.b)b
    mirror = base - 2.0 * (base @ bright)[:, None] * bright[None, :]
    return numpy.concatenate([base, mirror])

  def _sampleBall(self, img, normals):
    """measured radiance per normal, forward projection only."""
    h, w, _ = img.shape
    c = numpy.array([BALL_POS.x, BALL_POS.y, BALL_POS.z])
    vals = numpy.zeros(len(normals))
    ok = numpy.zeros(len(normals), dtype=bool)
    for k, n in enumerate(normals):
      p = c + self.ball_radius * n
      px, py = self._pixelOf(vec3(float(p[0]), float(p[1]), float(p[2])), w, h)
      ix, iy = int(round(px)), int(round(py))
      if ix < FOOTPRINT or iy < FOOTPRINT or ix >= w - FOOTPRINT or iy >= h - FOOTPRINT:
        continue
      box = img[iy - FOOTPRINT:iy + FOOTPRINT + 1, ix - FOOTPRINT:ix + FOOTPRINT + 1, :3]
      vals[k] = float(box.mean())
      ok[k] = True
    return vals, ok

  ##############################################################
  # state machine
  ##############################################################

  def _restate(self, s):
    self._state = s
    self._state_frame = self._frame
    self._state_time = time.time()

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    self._frame += 1
    if self._done:
      return

    if self._state == 0:                      # warm: the env upload + prefilter
      fade = float(self._pbr().sky_ibl_fade_weight)
      if (self._frame >= WARM_FRAMES) and (fade >= 0.9999):
        self._note("warm at frame %d, sky_ibl_fade_weight=%.4f" % (self._frame, fade))
        self._aimCamera(SWEEP_AZIMUTHS[0])
        self._restate(1)
      elif (time.time() - self._state_time) > WAIT_SECONDS:
        self._fail("env lighting never warmed (sky_ibl_fade_weight=%.4f)" % fade)
      return

    if self._state == 1:                      # settle at this sweep bearing
      self.SGC.scenegraph.renderOnContext(ctx)
      if not self._realized:
        # REALIZE the target: captureAsFormat asserts natively on an unbuilt
        # RtBuffer impl rather than raising.
        ctx.FBI.rtGroupInit(self.SGC.float_rtg)
        self._realized = True
        return
      if (self._frame - self._state_frame) >= SETTLE_FRAMES:
        self._issueCapture(ctx, "sweep_%d" % self._sweep)
        self._restate(2)
      return

    if self._state == 2:                      # collect, advance the sweep
      if self._collect() is None:
        return
      az = SWEEP_AZIMUTHS[self._sweep]
      self._sky[az] = self._skyMean(self._shots["sweep_%d" % self._sweep])
      self._note("sweep azimuth %5.1f deg  sky mean=%.6g" % (az, self._sky[az]))
      self._sweep += 1
      if self._sweep < len(SWEEP_AZIMUTHS):
        self._aimCamera(SWEEP_AZIMUTHS[self._sweep])
        self._restate(1)
        return
      # the bright bearing, then the ring of ball cameras anchored on it
      self._bright_az, self._sky_aniso = self._brightBearing()
      self._note("bright bearing = %.2f deg (circular first moment, "
                 "anisotropy %.4f)" % (self._bright_az, self._sky_aniso))
      self._ringcams = [(self._bright_az + o) % 360.0 for o in RING_OFFSETS]
      self._aimCamera(self._ringcams[0])
      self._restate(3)
      return

    if self._state == 3:                      # settle at this ring bearing
      self.SGC.scenegraph.renderOnContext(ctx)
      if (self._frame - self._state_frame) >= SETTLE_FRAMES:
        self._issueCapture(ctx, "ball_%d" % self._ring)
        self._restate(4)
      return

    if self._collect() is None:               # 4: collect, advance the ring
      return
    self._fitRingRead(self._shots["ball_%d" % self._ring])
    if self._done:
      return
    self._ring += 1
    if self._ring < len(self._ringcams):
      self._aimCamera(self._ringcams[self._ring])
      self._restate(3)
      return
    # back to the perpendicular camera: the hemisphere means and the mirror
    # probe read ball_0, and every projection below goes through this camera.
    self._aimCamera(self._ringcams[0])
    self._emitVerdict()

  ##############################################################

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    img = self._shots["ball_0"]
    bright = dir_h(self._bright_az)

    sky_hi = max(self._sky.values())
    sky_lo = min(self._sky.values())
    contrast = sky_hi / max(sky_lo, 1.0e-12)
    print("=== the map's bright side (skybox sweep) ===", flush=True)
    print("  bright azimuth=%.2f deg (circular first moment, anisotropy %.4f)  "
          "sky mean hi=%.6g lo=%.6g contrast=%.3fx"
          % (self._bright_az, self._sky_aniso, sky_hi, sky_lo, contrast),
          flush=True)
    check("map_has_a_bright_side", contrast > SKY_CONTRAST_MIN,
          "contrast=%.3fx (min %.2fx) — a map this flat cannot judge an azimuth"
          % (contrast, SKY_CONTRAST_MIN))

    normals = self._normalSet(bright)
    if self._done:
      return
    vals, ok = self._sampleBall(img, normals)
    proj = normals @ bright
    warm = ok & (proj > 0)
    cold = ok & (proj < 0)
    print("=== the ambient's warm side (diffuse env on the receiver) ===", flush=True)
    check("samples_present", int(warm.sum()) >= MIN_SAMPLES and int(cold.sum()) >= MIN_SAMPLES,
          "warm n=%d cold n=%d (min %d each)" % (int(warm.sum()), int(cold.sum()), MIN_SAMPLES))
    if failures:
      verdict(False, "insufficient evidence: " + ",".join(failures))
      self._exit_code = 1
      self._done = True
      self.ezapp.signalExit()
      return

    # THE WARM BEARING. Six-camera ring dipole; the azimuth of the gradient is
    # where the light came from.
    warm_az, ring_spread, cxz = self._ringBearing()
    err = abs((warm_az - self._bright_az + 180.0) % 360.0 - 180.0)
    print("=== warm bearing (six-camera ring dipole of the receiver's response) ===",
          flush=True)
    print("  fitted warm azimuth=%.2f deg  map bright azimuth=%.2f deg  error=%.2f deg"
          % (warm_az, self._bright_az, err), flush=True)
    print("  ring c_xz=(%.6g,%.6g)  leave-one-out spread=%.2f deg"
          % (float(cxz[0]), float(cxz[1]), ring_spread), flush=True)
    check("warm_bearing_matches_map_bright_side", err < BEARING_TOL_DEG,
          "error=%.2f deg (bar %.1f); an error near 180 is the mirrored-U defect"
          % (err, BEARING_TOL_DEG))

    warm_mean = float(vals[warm].mean())
    cold_mean = float(vals[cold].mean())
    ratio = warm_mean / max(cold_mean, 1.0e-12)
    print("  hemisphere means  warm=%.6g (n=%d)  cold=%.6g (n=%d)  ratio=%.3fx"
          % (warm_mean, int(warm.sum()), cold_mean, int(cold.sum()), ratio), flush=True)
    check("receiver_is_lit", warm_mean > BALL_LIT_MIN and cold_mean > BALL_LIT_MIN,
          "warm=%.6g cold=%.6g (min %.1e) — a black receiver is the env-lighting "
          "warmup race, not an orientation" % (warm_mean, cold_mean, BALL_LIT_MIN))
    check("warm_side_matches_map_bright_side", ratio > WARM_RATIO_MIN,
          "hemisphere ratio=%.3fx (min %.2fx); below 1 the ambient is warm on "
          "the side the map is DARK on, which is the mirrored-U defect"
          % (ratio, WARM_RATIO_MIN))

    # the mirror probe: one exact reflected pair at 45 degrees off the view axis.
    eye = self.SGC.camera.eye
    c = numpy.array([BALL_POS.x, BALL_POS.y, BALL_POS.z])
    u = numpy.array([eye.x, eye.y, eye.z]) - c
    u = u / numpy.linalg.norm(u)
    pair = numpy.stack([(u + bright) / math.sqrt(2.0), (u - bright) / math.sqrt(2.0)])
    pvals, pok = self._sampleBall(img, pair)
    p_ratio = float(pvals[0]) / max(float(pvals[1]), 1.0e-12)
    print("=== mirror probe (+-45 deg off the view axis, exact reflections) ===",
          flush=True)
    print("  toward=%.6g  away=%.6g  ratio=%.3fx" % (pvals[0], pvals[1], p_ratio),
          flush=True)
    check("probe_pair_sampled", bool(pok[0] and pok[1]), "both members on-frame")
    check("probe_warm_toward_map_bright_side", p_ratio > WARM_RATIO_MIN,
          "ratio=%.3fx (min %.2fx)" % (p_ratio, WARM_RATIO_MIN))

    ok_all = (len(failures) == 0)
    detail = ("envmap=%s bright_az=%.2f sky_contrast=%.3f warm_az=%.2f "
              "bearing_err=%.2f ring_spread=%.2f warm=%.6g cold=%.6g "
              "hemi_ratio=%.3f probe_ratio=%.3f"
              % (os.path.basename(ENVMAP), self._bright_az, contrast, warm_az,
                 err, ring_spread, warm_mean, cold_mean, ratio, p_ratio))
    if failures:
      detail += " failed=" + ",".join(failures)
    verdict(ok_all, detail)                    # VERDICT BEFORE TEARDOWN
    self._exit_code = 0 if ok_all else 1
    self._done = True
    self.ezapp.signalExit()


def main():
  app = BakedAzimuthApp()
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before the gate reached a verdict")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
