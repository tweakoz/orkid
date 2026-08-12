#!/usr/bin/env ork.python
################################################################################
# THE FIFTH SUN CASCADE — km-scale shadow coverage.
#
# WHAT THE FEATURE CLAIMS. The sun's shadow bands are nested world spheres about
# the viewer with geometric radii (10/40/160/640 m at the stock knobs), and the
# outermost one used to be the last word: a fragment — or a step of the haze
# march — further out than the outer radius selects no band and is returned
# UNSHADOWED by construction. So a ridge, a cloud deck's worth of canopy, or any
# caster a kilometre away threw nothing at all, on the ground or through the air.
# Raising the storage ceiling to five continues the same ladder to 2560 m with no
# new radius knob; the count stays AUTHORED (cascades=), so a scene that declares
# nothing renders exactly what it rendered before.
#
# THE RIG. An empty near field, a wide ground body whose visible surface sits
# ~1200 m out, and ONE occluder resting on it at ~950 m — everything the test
# cares about is beyond the 4-band outer radius and nothing is inside it. Every
# leg is then a comparison of the SAME scene at cascades=4 and cascades=5, which
# is what makes the 4-band arm an exact negative control rather than a second rig.
#
# LEGS — MONO
#   (B1) THE FAR SHADOW EXISTS   at cascades=5 a compact region of the far ground
#        AT 5 BANDS ONLY         goes measurably darker than the same pixels at
#                                cascades=4. This is the defect the band fixes,
#                                stated as a number.
#   (B2) IT IS THE OCCLUDER,     with the occluder banished out of every band, the
#        NOT THE BAND            two counts must agree to within a hair. Without
#                                this control "5 bands darkened the far ground"
#                                could equally be far-band shadow acne, which
#                                would darken it whether or not anything casts.
#   (B3) THE AIR IS SHADOWED     the aerial-perspective march's per-step sun taps
#        TOO, AT 5 BANDS ONLY    read the same bands. Measured ONLY on pixels
#                                whose SURFACE shading did not move between the
#                                counts (B1's mask inverted): whatever changes
#                                there when the haze shadow is armed came through
#                                the air, not off the ground.
#   (B4) AND NOT AT 4            the same air measurement at cascades=4 must be
#                                ~zero — beyond 640 m every march step selects no
#                                band and reports lit, so arming the haze shadow
#                                changes nothing. The control that makes B3 mean
#                                what it says.
#
# LEGS — STEREO (child process, single-pass node)
#   (C1) FIVE RADII ON THE WIRE  ORKID_SUNSNAP_TRACE's publish line must carry
#                                FIVE band radii, the fifth at the geometric
#                                ladder's 2560 m. A trace that stopped at four
#                                could not tell the two ladders apart.
#   (C2) BOTH EYES, AMORTIZED    the far shadow appears in BOTH eyes under the
#                                single-pass stereo node, with the fifth band
#                                drawn through the existing two-bands-per-frame
#                                cadence (which also double-buffers the slice
#                                range) — no new cadence machinery.
#   (C3) CLEAN                   zero validation errors in the stereo arm.
#
# Self-configuring: no arguments, no environment.
#   ork.python test_sun_cascade_5band.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import json
import math
import re
import subprocess
import tempfile

WIDTH, HEIGHT     = 640, 480
CAM_EYE_Y         = 60.0
CAM_NEAR, CAM_FAR = 1.0, 8000.0
FOVY_DEG          = 45.0
MSAA              = 1

# THE LADDER. Stock knobs: radius[i] = BAND_RADIUS * BAND_RATIO^i, so
# 10/40/160/640 at four bands and a fifth at 2560. Everything the gate measures
# lives in the gap between those two numbers — that gap IS the feature.
BAND_RADIUS       = 10.0
BAND_RATIO        = 4.0
BAND4_OUTER       = BAND_RADIUS * (BAND_RATIO ** 3)  # 640 m — the old edge of the world
BAND5_OUTER       = BAND_RADIUS * (BAND_RATIO ** 4)  # 2560 m — the new one

SHADOW_MAP_SIZE   = 2048
# CASTER CEILING, generous: the occluder stands ~80 m off a ground body whose own
# relief is hundreds of metres, and the far band's toward-light extrusion is what
# has to clear both.
SHADOW_MAX_DIST   = 600.0
# WORLD METRES of depth slack. Band 4's texels are 2*2560/2048 = 2.5 m across, so
# the slope-induced depth error over one texel of a curved ground body is metres,
# not centimetres — the near-band default would read as acne over the whole far
# surface (and leg B2 is precisely the instrument that would catch it).
SHADOW_BIAS       = 2.0

# GROUND: one large body whose upper surface passes through ~1200 m out, i.e.
# comfortably outside the 4-band radius and comfortably inside the 5-band one.
GROUND_POS        = (0.0, -600.0, 1200.0)
GROUND_SCALE      = 600.0
# OCCLUDER: resting on that surface at ~950 m, big enough that its shadow spans
# tens of far-band texels.
OCCLUDER_POS      = (0.0, 25.0, 950.0)
OCCLUDER_SCALE    = 80.0
# ...and where it goes for the control: outside every band, so it casts into no
# map at all while the rest of the scene is untouched.
OCCLUDER_BANISHED = (0.0, 25.0, 60000.0)

# AHEAD of the camera and moderately high: shadows come back TOWARD the eye (a
# sun behind the camera hides its own shadows behind the caster) and the far
# surface still faces the light hard enough to read.
SUN_ELEV_DEG      = 35.0
SUN_AZIM_DEG      = 18.0
SUN_INTENSITY     = 6.0
SKY_EXPOSURE      = 6.0

# THE TWO LEGS WANT OPPOSITE HAZE, so they each get their own and the density is
# the only thing that moves between them. A surface shadow 1.2 km away is read
# THROUGH the haze column, so thick haze washes it out (measured: at 3/km the
# far shadow shrank from 1287 px to 47); the air term is proportional to how much
# haze the ray crosses, so thin haze makes it noise. Neither number is a fudge —
# each leg is a comparison of two captures taken at the SAME density.
HAZE_DENSITY_SURF = 0.5     # 1/km at layer base — the far ground stays legible
HAZE_DENSITY_AIR  = 3.0     # ... and the march has something to be shadowed in
HAZE_SCALE_HEIGHT = 0.60    # km
HAZE_PHASE_G      = 0.72
HAZE_MAX_DIST_KM  = 40.0

SETTLE_FRAMES     = 110     # cascades + IBL settle before the first reading
STEP_SETTLE       = 60      # a count change is a RIG change: refit + realloc + redraw

# ------------------------------------------------------------------ thresholds
# (B1) the far shadow, in luma over its own mask.
SHADOW_MIN_PX     = 300
SHADOW_MIN_DELTA  = 0.020
# (B2) the no-occluder control. Not byte-equality: the two counts allocate
# different slice ranges and refit, so this is "no region went dark", measured
# the same way B1 measures that one did.
CONTROL_MAX_PX    = 200
CONTROL_MAX_MEAN  = 0.004
# (B3)/(B4) the air term over the surface-unchanged pixels around the shadow.
AIR_RING_PX       = 30
AIR_MIN_DELTA     = 0.004
AIR_CTL_MAX       = 0.0010
AIR_MIN_RATIO     = 4.0
# (C1) the fifth radius, inset off the fit radius by the shader's own select
# margin (fwdnode_impl_sub) — a few texels out of 2048, so 2% is generous.
RADIUS_TOL        = 0.02
# (C2) per eye, on a smaller stereo target.
STEREO_MIN_PX     = 100

OUT_DEFAULT = os.path.join(tempfile.gettempdir(), "sun_cascade_5band")

HAZE_OFF, HAZE_INLINE = 0.0, 1.0


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return (math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


def _luma(rgb):
  import numpy
  a = rgb.astype(numpy.float64)
  return (0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]) / 255.0


def _dilate(mask, radius):
  """separable box dilation — the neighbourhood the air measurement lives in.
  Wrapping shifts are harmless here: the shadow region sits well inside the
  frame, so nothing rolls in off an edge."""
  import numpy
  m = mask
  for axis in (1, 0):
    acc = numpy.zeros_like(m)
    for d in range(-radius, radius + 1):
      acc |= numpy.roll(m, d, axis=axis)
    m = acc
  return m


def _configure_sun(sun, vec3, cascades):
  sun.data.color = vec3(1, 1, 1)
  sun.data.intensity = SUN_INTENSITY
  sun.data.shadowBias = SHADOW_BIAS
  sun.data.shadowMapSize = SHADOW_MAP_SIZE
  sun.data.shadowCascadeCount = cascades
  sun.data.shadowMaxDistance = SHADOW_MAX_DIST
  sun.data.shadowBandRadius = BAND_RADIUS
  sun.data.shadowBandRatio = BAND_RATIO
  sun.shadowCaster = True
  d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
  sun.lookAt(vec3(d[0], d[1], d[2]) * 4000.0, vec3(0, 0, 0), vec3(0, 1, 0))


def _make_atmosphere(lev2):
  atmo = lev2.SkyAtmosphereData()
  atmo.sky_exposure = SKY_EXPOSURE
  atmo.ibl_crossfade_frames = 0   # a half-faded IBL is not a reading
  atmo.aerial_perspective_enable = True
  atmo.haze_density = HAZE_DENSITY_SURF
  atmo.haze_scale_height = HAZE_SCALE_HEIGHT
  atmo.haze_phase_g = HAZE_PHASE_G
  atmo.haze_max_distance_km = HAZE_MAX_DIST_KM
  atmo.haze_sun_shadow = HAZE_OFF
  return atmo


################################################################################
# MONO PHASE
################################################################################


def _mono(outdir):
  import numpy
  from PIL import Image as PILImage
  from orkengine.core import vec3, vec4
  from orkengine import lev2
  from ork.app.application import ComponentizedApplication
  from ork.app.std_scenegraph import StandardSceneGraphComponent
  from ork.testing import Watchdog

  class FiveBandApp(ComponentizedApplication):

    def __init__(self):
      super().__init__()
      self._frame = 0
      self._built = False
      self._phase = 0
      self._phase_frame = 0
      self._done = False
      self._shots = {}
      self._inflight = None
      self._report = None
      # (tag, the state to leave behind for the NEXT capture). Ordered so the
      # cascade count moves as few times as it can: each move is a structural
      # refit plus a depth-array reallocation.
      self._steps = [
          # SURFACE arm — thin haze, no haze shadow: the far ground, read at
          # both counts, and then again with nothing out there to cast.
          ("n4_off",    lambda: self._bands(5)),
          ("n5_off",    lambda: self._control(4)),
          ("ctl_n4",    lambda: self._bands(5)),
          # AIR arm — thick haze, the occluder back, the march's shadow armed
          # and disarmed at each count. Four captures, two differences.
          ("ctl_n5",    lambda: self._airArm(4, HAZE_OFF)),
          ("haze4_off", lambda: self._airArm(4, HAZE_INLINE)),
          ("haze4_on",  lambda: self._airArm(5, HAZE_OFF)),
          ("haze5_off", lambda: self._airArm(5, HAZE_INLINE)),
          ("haze5_on",  None),
      ]
      self.SGC = self.addComponent(
          "std_scenegraph", StandardSceneGraphComponent,
          eye=vec3(0, CAM_EYE_Y, 0), tgt=vec3(0, 0, 1200), up=vec3(0, 1, 0),
          explicit_near_far=True, near=CAM_NEAR, far=CAM_FAR,
          grid_variant=None,
          msaa=MSAA,
          sg_params={
              "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
              "SkyboxIntensity": 1.0,
              "DiffuseIntensity": 1.0,
              "SpecularIntensity": 1.0,
              "AmbientLevel": vec3(0.10),
          })
      self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                       use_subsystems=['opq', 'core', 'gpu', 'lev2'])

    ############################################################

    def _onGpuInit(self, ctx):
      SGC = self.SGC
      self.ezapp.topWidget.enableUiDraw()
      SGC.pbr_common.enable_skybox = True

      self.atmo = _make_atmosphere(lev2)
      SGC.pbr_common.atmosphere = self.atmo
      SGC.pbr_common.sky_source = "procedural"

      model = lev2.XgmModel("data://tests/pbr_calib.glb")

      def body(name, pos, scale):
        d = model.createDrawable()
        n = SGC.scenegraph.createDrawableNodeOnLayers(SGC.fwd_layers, name, d)
        n.worldTransform.translation = vec3(*pos)
        n.worldTransform.scale = scale
        return n

      self._ground = body("ground", GROUND_POS, GROUND_SCALE)
      self._occluder = body("occluder", OCCLUDER_POS, OCCLUDER_SCALE)

      sun = lev2.DynamicDirectionalLight()
      _configure_sun(sun, vec3, 4)
      self.sun = sun
      self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
      SGC.scenegraph.lightingmanager.gpuInit(ctx)

      SGC.camera.perspective(CAM_NEAR, CAM_FAR, FOVY_DEG)
      SGC.camera.lookAt(vec3(0, CAM_EYE_Y, 0), vec3(0, 0, 1200), vec3(0, 1, 0))

    def _onUpdate(self, updinfo):
      pass  # static: the only frame-to-frame differences are the ones we make

    ############################################################
    # the states

    def _bands(self, n):
      self.sun.data.shadowCascadeCount = n

    def _control(self, n):
      # the occluder leaves every band. Nothing else moves, so whatever the two
      # counts still disagree about after this is the BANDS' own doing.
      self._occluder.worldTransform.translation = vec3(*OCCLUDER_BANISHED)
      self.sun.data.shadowCascadeCount = n

    def _airArm(self, n, mode):
      # the occluder comes back and the haze thickens: from here on the only
      # thing under test is what the march's per-step sun taps see.
      self._occluder.worldTransform.translation = vec3(*OCCLUDER_POS)
      self.atmo.haze_density = HAZE_DENSITY_AIR
      self.atmo.haze_sun_shadow = mode
      self.sun.data.shadowCascadeCount = n

    ############################################################

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
        os.makedirs(outdir, exist_ok=True)
        PILImage.fromarray(img[::-1, ::-1]).save(os.path.join(outdir, "%s.png" % key))
      except Exception as e:
        print("[5band] png write failed: %r" % (e,), flush=True)
      print("[5band] captured %-8s %dx%d mean=%.4f" % (key, w, h, float(img.mean())),
            flush=True)
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
      if (self._phase % 2) == 0:
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
          self._measure()
      return

    ############################################################

    def _measure(self):
      """Reduce the captures to the numbers the driver gates on, BEFORE teardown
      (a wedged shutdown must not be able to swallow the verdict).

      THE SHADOW REGION IS FOUND, NOT GUESSED: it is defined as the pixels the
      fifth band DARKENS, which makes B1 a statement about how strong the far
      shadow is and makes every later leg measurable exactly where it acts (and
      exactly where it does not).
      """
      import numpy
      S = self._shots
      lum = {k: _luma(v) for k, v in S.items()}
      h, w = S["n4_off"].shape[:2]

      # the SKY band is geometric — the absence of geometry, never anything the
      # feature does. Excluded from every surface/air statistic below.
      sky_rows = int(h * 0.30)
      geo = numpy.zeros((h, w), dtype=bool)
      geo[sky_rows:, :] = True

      darker = (lum["n4_off"] - lum["n5_off"])   # +ve where the fifth band shadows
      shadow = geo & (darker > 0.010)
      n_shadow = int(shadow.sum())

      ctl_darker = (lum["ctl_n4"] - lum["ctl_n5"])
      ctl_mask = geo & (numpy.abs(ctl_darker) > 0.010)

      # AIR: the shadow's NEIGHBOURHOOD, minus the shadow itself — pixels whose
      # SURFACE shading the fifth band did not move, so anything that changes
      # there when the haze shadow is armed arrived through the march. The
      # neighbourhood is where the shadow VOLUME is (it runs from the caster
      # back toward the eye); averaging over the whole frame instead would
      # divide a local effect by a hundred thousand untouched pixels.
      air = _dilate(shadow, AIR_RING_PX) & geo & (~shadow) & (numpy.abs(darker) <= 0.002)
      n_air = int(air.sum())
      air5 = numpy.abs(lum["haze5_on"] - lum["haze5_off"])
      air4 = numpy.abs(lum["haze4_on"] - lum["haze4_off"])

      out = {}
      out["dims"] = [w, h]
      out["shadow_px"] = n_shadow
      out["shadow_mean"] = float(darker[shadow].mean()) if n_shadow else 0.0
      out["shadow_frac_of_geo"] = float(n_shadow) / float(max(int(geo.sum()), 1))
      out["control_px"] = int(ctl_mask.sum())
      out["control_mean"] = float(numpy.abs(ctl_darker)[geo].mean())
      out["control_max"] = float(numpy.abs(ctl_darker)[geo].max())
      out["air_px"] = n_air
      out["air5"] = float(air5[air].mean()) if n_air else 0.0
      out["air4"] = float(air4[air].mean()) if n_air else 0.0
      out["mean"] = {k: float(v.mean()) for k, v in lum.items()}

      try:
        PILImage.fromarray((shadow.astype(numpy.uint8) * 255)[::-1, ::-1]).save(
            os.path.join(outdir, "mask_far_shadow.png"))
        PILImage.fromarray(
            numpy.clip(darker * 900.0, 0, 255).astype(numpy.uint8)[::-1, ::-1]).save(
            os.path.join(outdir, "delta_far_shadow.png"))
      except Exception:
        pass

      self._report = out
      self._finish(0)

    def _finish(self, rc):
      self._exit_code = rc
      self._done = True
      self.ezapp.signalExit()

  wd = Watchdog(420.0, label="sun_cascade_5band_mono").arm()
  app = FiveBandApp()
  app.ezapp.mainThreadLoop()
  wd.disarm()
  app.ezapp.shutdown()
  return app._report


################################################################################
# STEREO PHASE — the single-pass node, in a child process (one XR device per
# process; the mono phase above has already owned this one). The publish TRACE
# is read off this same run: it is the arm that authors five bands.
################################################################################


def _stereo(outdir):
  import numpy
  from PIL import Image
  from orkengine import lev2
  from orkengine.core import vec3, vec4, mtx4, VarMap
  from ork.testing import headless_app, ensure_parent_dir

  W, H, FOVD, IPD = 512, 384, 55.0, 0.064
  out = {}

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2'], width=W, height=H) as app:
    ctx = app.ctx
    out["multiview"] = bool(ctx.supports_multiview)
    if not ctx.supports_multiview:
      return out

    vrdev = lev2.orkidvr.novr_device()
    vrdev.width, vrdev.height = W, H
    vrdev.FOVD, vrdev.IPD = FOVD, IPD
    vrdev.near, vrdev.far = CAM_NEAR, CAM_FAR
    vrdev.setPoseMatrix("hmd", mtx4.lookAt(vec3(0, CAM_EYE_Y, 0), vec3(0, 0, 1200),
                                           vec3(0, 1, 0)))

    params = VarMap()
    params.preset = "FWDPBRSPVR"
    params.SkyboxIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.DiffuseIntensity = 1.0
    params.AmbientLight = vec3(0.10)
    params.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    # THE SUN CASCADE PASS DRAWS THIS LAYER (fwdnode_impl_sub, layersForRole
    # "depth_prepass"), so a caster that is not on it casts nothing and the far
    # band has nothing to prove.
    dpp_layer = scene.createLayer("depth_prepass")

    pbc = scene.pbr_common
    pbc.enable_skybox = True
    atmo = _make_atmosphere(lev2)
    pbc.atmosphere = atmo
    pbc.sky_source = "procedural"

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    nrm = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    def body(name, pos, scale):
      drawable = model.createDrawable()
      for si in drawable.modelinst.submeshinsts:
        mtl = si.material.clone()
        mtl.assignImages(ctx, color=white, normal=nrm, mtlruf=white, doConform=True)
        mtl.baseColor = vec4(0.8, 0.8, 0.8, 1)
        mtl.metallicFactor = 0.0
        mtl.roughnessFactor = 0.55
        si.overrideMaterial(mtl)
      n = scene.createDrawableNodeOnLayers([layer, dpp_layer], name, drawable)
      n.worldTransform.translation = vec3(*pos)
      n.worldTransform.scale = scale
      return n

    keep = [body("ground", GROUND_POS, GROUND_SCALE),
            body("occluder", OCCLUDER_POS, OCCLUDER_SCALE)]

    sun = lev2.DynamicDirectionalLight()
    _configure_sun(sun, vec3, 4)
    _n = layer.createLightNode("sun", sun)
    scene.lightingmanager.gpuInit(ctx)

    camlut = lev2.CameraDataLut()
    cam = lev2.CameraData()
    cam.perspective(CAM_NEAR, CAM_FAR, FOVD)
    cam.lookAt(vec3(0, CAM_EYE_Y, 0), vec3(0, 0, 1200), vec3(0, 1, 0))
    # the per-view cull fan-out resolves "spawncam" outside XR presentation
    camlut.addCamera("spawncam", cam)

    shots = {}

    def take(tag, frames):
      for _ in range(frames):
        scene.updateScene(camlut)
        ctx.beginFrame()
        scene.renderOnContext(ctx)
        ctx.endFrame()
      outnode = scene.compositoroutputnode
      ctx.beginFrame()
      caps, futs = {}, []
      for (nm, left) in (("L", True), ("R", False)):
        rtg = outnode.downsampledEyeRtGroup(left)
        cb = lev2.CaptureBuffer()
        caps[nm] = cb
        futs.append((nm, ctx.FBI.captureAsFormat(rtg.buffer(0), cb, "RGBA8")))
      ctx.endFrame()
      for (nm, fut) in futs:
        ok = fut.wait(caps[nm])
        assert ok, "5band stereo: capture never landed for %s %s" % (tag, nm)
        cb = caps[nm]
        arr = numpy.array(cb, dtype=numpy.uint8).reshape(cb.height, cb.width, 4)[..., :3]
        shots["%s_%s" % (tag, nm)] = arr.copy()
        path = os.path.join(outdir, "stereo_%s_%s.png" % (tag, nm))
        ensure_parent_dir(path)
        Image.fromarray(arr).save(path)
      print("[5band] stereo captured %s" % tag, flush=True)

    take("n4", SETTLE_FRAMES)
    # ...and the fifth band arrives through the EXISTING amortization cadence,
    # two bands per frame: a five-band round-robin spans three frames and
    # double-buffers into a second slice range, so the far shadow appearing
    # below also says the slice stride and the band scheduler both count to
    # five. No new cadence knob is involved.
    sun.data.shadowCascadeCount = 5
    sun.data.shadowSnapshotBandsPerFrame = 2
    take("n5", STEP_SETTLE)

    lum = {k: _luma(v) for k, v in shots.items()}
    hh = lum["n4_L"].shape[0]
    geo = numpy.zeros_like(lum["n4_L"], dtype=bool)
    geo[int(hh * 0.30):, :] = True
    for nm in ("L", "R"):
      d = lum["n4_%s" % nm] - lum["n5_%s" % nm]
      m = geo & (d > 0.010)
      out["shadow_px_%s" % nm] = int(m.sum())
      out["shadow_mean_%s" % nm] = float(d[m].mean()) if m.any() else 0.0

    out["validation_errors"] = int(ctx.validation_errors)

  return out


################################################################################
# DRIVER
################################################################################

# the publish line the engine emits under ORKID_SUNSNAP_TRACE. bands<N> is the
# live count and splits<...> carries exactly that many select radii.
_TRACE_RE = re.compile(r"bands<(\d+)>\s+splits<([^>]*)>")


def _parse_trace_radii(text, want_bands):
  """the LAST publish that declared `want_bands` bands, as a list of floats."""
  found = None
  for line in text.splitlines():
    m = _TRACE_RE.search(line)
    if m is None:
      continue
    if int(m.group(1)) != want_bands:
      continue
    try:
      vals = [float(x) for x in m.group(2).split()]
    except ValueError:
      continue
    if len(vals) == want_bands:
      found = vals
  return found


def main():
  outdir = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else OUT_DEFAULT
  os.makedirs(outdir, exist_ok=True)

  if "--stereo-child" in sys.argv:
    rep = _stereo(outdir)
    with open(os.path.join(outdir, "stereo.json"), "w") as f:
      json.dump(rep, f, indent=1)
    return 0

  from ork.testing import verdict

  failures = []

  def check(label, ok, detail=""):
    print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      failures.append(label)

  print("[5band] MONO phase — cascades 4 vs 5 on one rig", flush=True)
  R = _mono(outdir)
  if R is None:
    return verdict(False, "mono phase produced no measurement (loop exited early)")

  check("B1_fifth_band_shadows_the_far_ground",
        R["shadow_px"] >= SHADOW_MIN_PX and R["shadow_mean"] >= SHADOW_MIN_DELTA,
        "mask=%d px (floor %d, %.1f%% of geometry) mean darkening=%.4f (floor %.4f) "
        "— occluder %.0f m out, past the 4-band radius of %.0f m"
        % (R["shadow_px"], SHADOW_MIN_PX, 100.0 * R["shadow_frac_of_geo"],
           R["shadow_mean"], SHADOW_MIN_DELTA, OCCLUDER_POS[2], BAND4_OUTER))

  check("B2_control_no_occluder_no_difference",
        R["control_px"] <= CONTROL_MAX_PX and R["control_mean"] <= CONTROL_MAX_MEAN,
        "occluder banished to %.0f m: differing px=%d (ceil %d) mean|delta|=%.5f "
        "(ceil %.5f) max=%.4f — a far band that shadows nothing must change nothing"
        % (OCCLUDER_BANISHED[2], R["control_px"], CONTROL_MAX_PX,
           R["control_mean"], CONTROL_MAX_MEAN, R["control_max"]))

  ratio = (R["air5"] / R["air4"]) if R["air4"] > 1e-9 else float("inf")
  check("B3_fifth_band_shadows_the_air",
        R["air5"] >= AIR_MIN_DELTA,
        "over %d surface-unchanged px: |haze_on - haze_off| at 5 bands = %.5f "
        "(floor %.5f)" % (R["air_px"], R["air5"], AIR_MIN_DELTA))

  check("B4_control_four_bands_leave_the_far_air_lit",
        R["air4"] <= AIR_CTL_MAX and ratio >= AIR_MIN_RATIO,
        "same pixels at 4 bands = %.5f (ceil %.5f), ratio 5/4 = %.1fx (floor %.1fx) "
        "— beyond %.0f m every march step selects no band and reports lit"
        % (R["air4"], AIR_CTL_MAX, ratio, AIR_MIN_RATIO, BAND4_OUTER))

  # ---- STEREO + the publish trace, in a child process
  print("[5band] STEREO phase (single-pass node, child process, snapshot trace on)",
        flush=True)
  env = dict(os.environ)
  env["ORKID_SUNSNAP_TRACE"] = "1"
  proc = subprocess.run([sys.executable, os.path.abspath(__file__), outdir, "--stereo-child"],
                        env=env, capture_output=True, text=True)
  sys.stdout.write(proc.stdout)
  spath = os.path.join(outdir, "stereo.json")
  if proc.returncode != 0 or not os.path.exists(spath):
    check("C_stereo_child_completed", False,
          "rc=%d json=%s\n%s" % (proc.returncode, os.path.exists(spath), proc.stderr[-2000:]))
  else:
    with open(spath) as f:
      S = json.load(f)

    radii = _parse_trace_radii(proc.stdout, 5)
    if radii is None:
      check("C1_trace_publishes_five_radii", False,
            "no [sunsnap] publish line carrying five band radii was emitted")
    else:
      err = abs(radii[-1] - BAND5_OUTER) / BAND5_OUTER
      check("C1_trace_publishes_five_radii", err <= RADIUS_TOL,
            "radii=%s — fifth = %.1f m vs the ladder's %.0f m (rel err %.4f, tol %.2f)"
            % (" ".join("%.1f" % r for r in radii), radii[-1], BAND5_OUTER, err, RADIUS_TOL))

    if not S.get("multiview"):
      print("  SKIP stereo legs — device reports no multiview support", flush=True)
    else:
      check("C2_far_shadow_in_both_eyes",
            min(S["shadow_px_L"], S["shadow_px_R"]) >= STEREO_MIN_PX,
            "L=%d px (mean %.4f) R=%d px (mean %.4f), floor %d"
            % (S["shadow_px_L"], S["shadow_mean_L"],
               S["shadow_px_R"], S["shadow_mean_R"], STEREO_MIN_PX))
      check("C3_stereo_no_validation_errors", S["validation_errors"] == 0,
            "validation_errors=%d" % S["validation_errors"])

  detail = ("shadow=%d px @ %.4f | control=%d px @ %.5f | air 5b=%.5f 4b=%.5f | out=%s"
            % (R["shadow_px"], R["shadow_mean"], R["control_px"], R["control_mean"],
               R["air5"], R["air4"], outdir))
  if failures:
    return verdict(False, "%d leg(s) failed: %s | %s" % (len(failures), ",".join(failures), detail))
  return verdict(True, "five-band sun cascade | %s" % detail)


if __name__ == "__main__":
  sys.exit(main())
