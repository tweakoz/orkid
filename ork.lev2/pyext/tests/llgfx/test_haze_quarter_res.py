#!/usr/bin/env ork.python
################################################################################
# QUARTER-RES SUN SHAFTS — the third haze-in-shadow mode, and the shaft gain.
#
# WHAT THE FEATURE CLAIMS. The shadowed aerial-perspective march costs 32 cascade
# fetches on every hazed fragment. Mode 2 moves ONLY the shadow arm to a half-res
# target and subtracts it back off the finished opaque image, on the strength of
# an algebraic identity: the per-step occlusion enters the march linearly on the
# direct term and not at all in the transmittance, so
#
#     shadowed_inscatter == unshadowed_inscatter - loss
#
# exactly (skytools.i2). The claim is therefore NOT "mode 2 looks similar" — it is
# "mode 2 is the same radiometry, resolved more coarsely at shaft edges". That is
# a falsifiable statement about means, and this gate holds it to one.
#
# THE RIG. A wide lit floor, a bank of tall pillars between the sun and it, thick
# low haze, and a low sun raking across so the pillars throw long shadow columns
# through the air. Two image bands are measured: a SHADOWED column region (air in
# the pillars' shadow — where the whole effect lives) and a LIT region beside it.
# A rig with no occluder is impossible to A/B, so the gate proves the shafts are
# there before it says anything about them.
#
# LEGS — MONO (msaa 4, so multisampling is not a separate untested arm)
#   (A) MODES ARE NOT NO-OPS   mode 1 and mode 2 each move the shadowed region
#                              measurably away from mode 0. A mode that renders
#                              nothing would pass every parity check for free.
#   (B) MODE PARITY            mode 2 vs mode 1: the shadowed region's MEAN LUMA
#                              must agree within a few percent. Band-limited on
#                              purpose — a quarter-res shadow arm is ALLOWED to
#                              soften shaft edges and is NOT allowed to change
#                              how dark the shafts are. Per-pixel equality would
#                              gate the feature's whole reason to exist.
#   (C) SKY IS UNTOUCHED       the sky dome takes its haze from the sky overlay,
#                              never from the surface march, so a loss subtracted
#                              where there is no scene depth would darken sky the
#                              inline mode leaves alone. The sky band must be
#                              BYTE-IDENTICAL across all three modes.
#   (D) NO SAMPLE SPLITTING    at msaa 4 the composite is one per-pixel value over
#                              every sample, so it cannot introduce the discrete
#                              per-sample tone banding a depth-replacing prepass
#                              can (test_cutout_prepass_depth_gate's defect). The
#                              observable: mode 2's local roughness over the
#                              shadowed region may not EXCEED mode 1's.
#   (E) GAIN IS FREE AT 1      the shaft-gain default must not move a pixel. The
#                              shader bypasses the remap at exactly 1.0, so an
#                              explicitly-written 1.0 is byte-identical to the
#                              untouched default, in both modes.
#   (F) GAIN DEEPENS SHAFTS    gain 2 darkens the shadowed region measurably in
#                              BOTH modes, while the LIT region stays put — gain
#                              must be provably a multiplier on the loss and not
#                              a global exposure change.
#   (G) GAIN TOUCHES NOTHING   with the sun demoted to a non-caster there is no
#       WITHOUT AN OCCLUDER    occlusion to gain: gain 2 must be BYTE-IDENTICAL to
#                              gain 1 in both modes. This is the negative control
#                              for (F) — without it, "gain darkened the frame"
#                              could be gain darkening everything.
#   (H) GAIN PARITY            mode 2 vs mode 1 at gain 2 stays inside the same
#                              tolerance as (B): the gain is applied at the shared
#                              per-step tap, so the two modes cannot diverge under
#                              it.
#
# LEGS — STEREO (child process, single-pass node)
#   (I) EYES DIFFER            the two eye images differ in the shafted region:
#                              the march is view-dependent through the ray
#                              direction, so a shared (cross-eye) loss buffer
#                              would show up as suspiciously equal eyes.
#   (J) PER-EYE PARITY         each eye's mode-2 image matches THAT EYE's mode-1
#                              image inside the mono tolerance. An eye reading the
#                              wrong layer fails here and only here.
#
# Self-configuring: no arguments, no environment.
#   ork.python test_haze_quarter_res.py [outdir]
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import json
import math
import subprocess
import tempfile

WIDTH, HEIGHT = 640, 480
EYE_Y         = 3.0
CAM_NEAR, CAM_FAR = 0.5, 3000.0
FOVY_DEG      = 55.0
MSAA          = 4

# THE SHAFT GEOMETRY, and it is the whole rig. The shadowed-air term integrates
# occlusion along the EYE RAY, so a sun behind the camera throws its shadow
# volumes away from the eye and the term is ~zero no matter how thick the haze is
# (measured: 1e-4 luma, indistinguishable from nothing). The sun therefore sits
# AHEAD of the camera and low, just off the view axis: the pillars stand between
# it and the eye, their shadow volumes come back TOWARD the eye, and the forward
# Mie lobe puts the in-scatter the shafts cut out of exactly where we are looking.
# Off-axis rather than dead-on so the sky does not clip to white and swallow the
# measurement.
SUN_ELEV_DEG  = 10.0
SUN_AZIM_DEG  = 22.0
SUN_INTENSITY = 9.0
SKY_EXPOSURE  = 7.0

# thick, ground-hugging: the shaft term is proportional to how much haze the ray
# crosses, so a thin layer would make every measurement noise.
HAZE_DENSITY      = 8.0     # 1/km at layer base
HAZE_SCALE_HEIGHT = 0.40    # km
HAZE_PHASE_G      = 0.72
HAZE_MAX_DIST_KM  = 40.0

SHADOW_MAP_SIZE   = 2048
SHADOW_CASCADES   = 3
SHADOW_MAX_DIST   = 400.0

# straddling the view axis at eye height, close enough that their shadow columns
# occupy a large solid angle of the frame.
PILLAR = [(-7.0, 30.0), (6.0, 45.0), (-5.0, 65.0), (8.0, 95.0), (-9.0, 130.0)]
PILLAR_SCALE  = 5.0

SETTLE_FRAMES = 110         # cascades + IBL settle before the first reading
STEP_SETTLE   = 45

# ------------------------------------------------------------------ thresholds
# (A) a mode that does nothing is the failure this floor catches. In luma, over
# the SHAFT MASK (see _bands): the mask is defined by the inline mode's own
# effect, so the floor is a statement about how strong the rig's shafts are.
MODE_MIN_MOVE     = 0.008
MIN_SHAFT_PX      = 2000    # the mask must be a region, not a speckle
# (B)/(H) the shadowed region's mean luma, as a fraction of the mode-1 mean.
# OWNER-ACCEPTED at 7% (2026-08-10): quarter-res carries a characterized residual —
# ~23% more total shaft energy removed than inline (sum(m0-m2)/sum(m0-m1)=1.232),
# near-CONSTANT in absolute luma across a 20x range of shaft strength (0.014-0.032,
# corr with strength 0.35), edges only 30% hotter than interior, unshafted geometry
# byte-level untouched. Measured NOT to be: unlit-pixel subtraction, upsample
# spreading (integral not conserved), or depth-filtering bias (texel-center snap
# moved B/H by 0.000). Owner accepted as band-limited constant offset, live-trimmable
# via the shaft-gain dial. Still nothing like enough headroom for a wrong-magnitude
# or missing shaft, which is what this leg exists to catch.
PARITY_MEAN_TOL   = 0.07
# (D) local roughness ratio ceiling. Sample splitting RAISES high-frequency
# energy; 1.25 allows the quarter-res arm's own edge dither and forbids banding.
ROUGHNESS_CEIL    = 1.25
# (F) gain 2 must move the shadowed region by at least this, and must leave the
# lit region inside LIT_STABLE_TOL.
GAIN_MIN_MOVE     = 0.012
LIT_STABLE_TOL    = 0.006
# (I) the eyes must differ by at least this in the shafted region.
STEREO_MIN_EYE_DELTA = 0.002

OUT_DEFAULT = os.path.join(tempfile.gettempdir(), "haze_quarter_res")

MODE_OFF, MODE_INLINE, MODE_QUARTER = 0.0, 1.0, 2.0


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return (math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


################################################################################
# measurement helpers — shared by the mono driver and the stereo child
################################################################################


def _luma(rgb):
  import numpy
  a = rgb.astype(numpy.float64)
  return (0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]) / 255.0


def _roughness(luma):
  """mean absolute horizontal gradient — a scale-free stand-in for how much
  high-frequency energy the band carries. Sample splitting adds some; a coarser
  shadow arm removes some."""
  import numpy
  return float(numpy.abs(numpy.diff(luma, axis=1)).mean())


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

  class QuarterResApp(ComponentizedApplication):

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
      # (tag, state to leave behind for the NEXT capture). The default-gain
      # capture comes first and never writes the gain member at all: leg (E)
      # compares an explicitly-written 1.0 against a value nobody assigned.
      self._steps = [
          ("m0",          lambda: self._mode(MODE_INLINE)),
          # m1/m1_again and m2/m2_again are the SAME STATE captured twice with
          # nothing touched in between: the repeat is the instrument that says
          # whether a mode is frame-stable at rest, independently of any knob.
          ("m1",          lambda: None),
          ("m1_again",    lambda: self._mode(MODE_QUARTER)),
          ("m2",          lambda: None),
          ("m2_again",    lambda: self._modeGain(MODE_INLINE, 1.0)),
          ("m1_gain1",    lambda: self._modeGain(MODE_QUARTER, 1.0)),
          ("m2_gain1",    lambda: self._modeGain(MODE_INLINE, 2.0)),
          ("m1_gain2",    lambda: self._modeGain(MODE_QUARTER, 2.0)),
          ("m2_gain2",    lambda: self._noCaster(MODE_INLINE, 1.0)),
          ("m1_nocast_g1", lambda: self._noCaster(MODE_INLINE, 2.0)),
          ("m1_nocast_g2", lambda: self._noCaster(MODE_QUARTER, 1.0)),
          ("m2_nocast_g1", lambda: self._noCaster(MODE_QUARTER, 2.0)),
          ("m2_nocast_g2", None),
      ]
      self.SGC = self.addComponent(
          "std_scenegraph", StandardSceneGraphComponent,
          eye=vec3(0, EYE_Y, 0), tgt=vec3(0, EYE_Y, 100), up=vec3(0, 1, 0),
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

      atmo = lev2.SkyAtmosphereData()
      atmo.sky_exposure = SKY_EXPOSURE
      atmo.ibl_crossfade_frames = 0     # a half-faded IBL is not a reading
      atmo.aerial_perspective_enable = True
      atmo.haze_density = HAZE_DENSITY
      atmo.haze_scale_height = HAZE_SCALE_HEIGHT
      atmo.haze_phase_g = HAZE_PHASE_G
      atmo.haze_max_distance_km = HAZE_MAX_DIST_KM
      # DELIBERATELY NOT SET: haze_sun_shadow_gain. Capture m0 renders whatever
      # the C++ default is, which is what leg (E) compares an explicit 1.0 to.
      atmo.haze_sun_shadow = MODE_OFF
      SGC.pbr_common.atmosphere = atmo
      SGC.pbr_common.sky_source = "procedural"
      self.atmo = atmo

      model = lev2.XgmModel("data://tests/pbr_calib.glb")

      # the receiving floor: a wide flat slab the shadow columns fall across.
      floor = model.createDrawable()
      node = SGC.scenegraph.createDrawableNodeOnLayers(SGC.fwd_layers, "floor", floor)
      node.worldTransform.translation = vec3(0, -60.0, 150.0)
      node.worldTransform.scale = 60.0
      self._floor = node

      # the casters. Tall and close together across the sun's path, so the air
      # between them and the eye is genuinely in shadow rather than grazed.
      self._pillars = []
      for i, (px, z) in enumerate(PILLAR):
        d = model.createDrawable()
        n = SGC.scenegraph.createDrawableNodeOnLayers(SGC.fwd_layers, "pillar%d" % i, d)
        n.worldTransform.translation = vec3(px, EYE_Y + 2.0, z)
        n.worldTransform.scale = PILLAR_SCALE
        self._pillars.append(n)

      sun = lev2.DynamicDirectionalLight()
      sun.data.color = vec3(1, 1, 1)
      sun.data.intensity = SUN_INTENSITY
      sun.data.shadowBias = 0.06
      sun.data.shadowMapSize = SHADOW_MAP_SIZE
      sun.data.shadowCascadeCount = SHADOW_CASCADES
      sun.data.shadowMaxDistance = SHADOW_MAX_DIST
      sun.shadowCaster = True
      d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
      sun.lookAt(vec3(d[0], d[1], d[2]) * 1000.0, vec3(0, 0, 0), vec3(0, 1, 0))
      self.sun = sun
      self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
      SGC.scenegraph.lightingmanager.gpuInit(ctx)

      SGC.camera.perspective(CAM_NEAR, CAM_FAR, FOVY_DEG)
      SGC.camera.lookAt(vec3(0, EYE_Y, 0), vec3(0, EYE_Y, 100), vec3(0, 1, 0))

    def _onUpdate(self, updinfo):
      pass  # static: the only frame-to-frame differences are the ones we make

    ############################################################
    # the states

    def _mode(self, m):
      self.atmo.haze_sun_shadow = m

    def _modeGain(self, m, g):
      self.atmo.haze_sun_shadow = m
      self.atmo.haze_sun_shadow_gain = g

    def _noCaster(self, m, g):
      # the negative control for gain: no cascades => every per-step lit fraction
      # is 1 => there is no loss for a gain to scale.
      self.sun.shadowCaster = False
      self.atmo.haze_sun_shadow = m
      self.atmo.haze_sun_shadow_gain = g

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
        print("[hzs] png write failed: %r" % (e,), flush=True)
      print("[hzs] captured %-14s %dx%d mean=%.4f" % (key, w, h, float(img.mean())),
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
      """Reduce the captures to the numbers the driver gates on. Measurement
      happens HERE, before teardown, so a wedged shutdown cannot swallow the
      verdict (#57).

      THE REGIONS ARE FOUND, NOT GUESSED. Where the shafts fall is a function of
      the sun, the pillars and the camera, and a hand-placed rectangle that
      drifts off them turns every downstream claim into a comparison of two
      pieces of empty floor. The SHAFT MASK is therefore defined by the inline
      mode's OWN effect — the pixels mode 1 moves away from mode 0 — which makes
      leg (A) a statement about the rig ("the shafts exist and are this strong")
      and every later leg a statement measured exactly where the feature acts.
      The LIT set is its complement among the pixels that carry scene geometry.
      """
      import numpy
      S = self._shots
      h, w = S["m0"].shape[:2]
      lum = {k: _luma(v) for k, v in S.items()}

      # the SKY band stays geometric: it is defined by the absence of geometry,
      # not by anything the feature does, and it must stay independent of the
      # mask (a sky the mask reached would make leg (C) circular).
      sky = (slice(0, int(h * 0.16)), slice(0, w))

      delta_inline = numpy.abs(lum["m1"] - lum["m0"])
      shaft = delta_inline > 0.004
      n_shaft = int(shaft.sum())
      # geometry that the shafts did NOT touch — the "did gain move the whole
      # frame?" control surface. Excludes the sky band by construction.
      geo = numpy.zeros_like(shaft)
      geo[int(h * 0.16):, :] = True
      lit = geo & (~shaft)

      out = {}
      out["dims"] = [w, h]
      out["shaft_px"] = n_shaft
      out["shaft_strength"] = float(delta_inline[shaft].mean()) if n_shaft else 0.0
      out["mean"] = {k: float(v.mean()) for k, v in lum.items()}
      out["shadowed_mean"] = {k: (float(v[shaft].mean()) if n_shaft else 0.0)
                              for k, v in lum.items()}
      out["lit_mean"] = {k: (float(v[lit].mean()) if int(lit.sum()) else 0.0)
                         for k, v in lum.items()}

      # roughness over the shaft mask's bounding box (a gradient needs adjacency,
      # which a scattered boolean set does not have)
      if n_shaft:
        ys, xs = numpy.nonzero(shaft)
        bb = (slice(int(ys.min()), int(ys.max()) + 1), slice(int(xs.min()), int(xs.max()) + 1))
      else:
        bb = (slice(0, h), slice(0, w))
      out["rough"] = {k: _roughness(lum[k][bb]) for k in ("m1", "m2")}

      def eqd(a, b, band=None):
        """(equal?, max channel delta, differing pixel count) — the deltas are
        printed even when the claim passes, because 'byte-identical' is only
        evidence if the number behind it is on the record."""
        x, y = S[a], S[b]
        if band is not None:
          x, y = x[band[0], band[1]], y[band[0], band[1]]
        d = numpy.abs(x.astype(numpy.int32) - y.astype(numpy.int32))
        return [bool(numpy.array_equal(x, y)), int(d.max()), int(d.any(axis=2).sum())]

      out["sky_equal"] = {"m0_m1": eqd("m0", "m1", sky),
                          "m0_m2": eqd("m0", "m2", sky),
                          "m1_m2": eqd("m1", "m2", sky)}
      out["sky_is_outside_the_mask"] = int(shaft[sky[0], sky[1]].sum())
      out["shadowed_equal_m1_m2"] = eqd("m1", "m2")
      out["repeat_stability"] = {"m1": eqd("m1", "m1_again"), "m2": eqd("m2", "m2_again")}
      # WHERE the repeat differs, if it does. A quarter-res upsample that flips
      # its nearest-tap fallback differs on SILHOUETTES (large depth steps); an
      # input that is genuinely moving differs in the interior too. The
      # silhouette proxy is the inline frame's own luma gradient, which needs no
      # extra capture and no depth readback.
      import numpy as _np
      g = _np.abs(_np.gradient(lum["m0"])[0]) + _np.abs(_np.gradient(lum["m0"])[1])
      edge = g > 0.02
      for tag, a, b in (("m1", "m1", "m1_again"), ("m2", "m2", "m2_again")):
        d = _np.abs(S[a].astype(_np.int32) - S[b].astype(_np.int32)).any(axis=2)
        n = int(d.sum())
        out["repeat_edge_frac_%s" % tag] = (float((d & edge).sum()) / n) if n else 0.0
        out["repeat_shaft_frac_%s" % tag] = (float((d & shaft).sum()) / n) if n else 0.0
        if n:
          try:
            PILImage.fromarray((d.astype(numpy.uint8) * 255)[::-1, ::-1]).save(
                os.path.join(outdir, "repeatdiff_%s.png" % tag))
          except Exception:
            pass
      out["gain1_is_default"] = {"m1": eqd("m1", "m1_gain1"), "m2": eqd("m2", "m2_gain1")}
      out["nocast_gain_noop"] = {"m1": eqd("m1_nocast_g1", "m1_nocast_g2"),
                                 "m2": eqd("m2_nocast_g1", "m2_nocast_g2")}
      out["nocast_mean"] = float(lum["m1_nocast_g1"].mean())
      self._report = out
      self._finish(0)

    def _finish(self, rc):
      self._exit_code = rc
      self._done = True
      self.ezapp.signalExit()

  wd = Watchdog(420.0, label="haze_quarter_res_mono").arm()
  app = QuarterResApp()
  app.ezapp.mainThreadLoop()
  wd.disarm()
  app.ezapp.shutdown()
  return app._report


################################################################################
# STEREO PHASE — the single-pass node, in a child process (one XR device per
# process; the mono phase above has already owned this one).
################################################################################


def _stereo(outdir):
  import numpy
  from PIL import Image
  from orkengine import lev2
  from orkengine.core import vec3, vec4, mtx4, VarMap
  from ork.testing import headless_app, ensure_parent_dir

  W, H, FOVD, IPD = 512, 384, 65.0, 0.064
  out = {}

  with headless_app(subsystems=['opq', 'core', 'gpu', 'lev2'], width=W, height=H) as app:
    ctx = app.ctx
    out["multiview"] = bool(ctx.supports_multiview)
    if not ctx.supports_multiview:
      return out

    vrdev = lev2.orkidvr.novr_device()
    vrdev.width, vrdev.height = W, H
    vrdev.FOVD, vrdev.IPD = FOVD, IPD
    vrdev.near, vrdev.far = 0.5, 3000.0
    vrdev.setPoseMatrix("hmd", mtx4.lookAt(vec3(0, EYE_Y, 0), vec3(0, EYE_Y, 100),
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
    # "depth_prepass"), so a caster that is not on it casts nothing and the rig
    # silently has no shadowed air to measure — in EVERY mode, which is what
    # makes the failure look like a feature bug instead of a rig bug.
    dpp_layer = scene.createLayer("depth_prepass")

    pbc = scene.pbr_common
    pbc.enable_skybox = True
    atmo = lev2.SkyAtmosphereData()
    atmo.sky_exposure = SKY_EXPOSURE
    atmo.ibl_crossfade_frames = 0
    atmo.aerial_perspective_enable = True
    atmo.haze_density = HAZE_DENSITY
    atmo.haze_scale_height = HAZE_SCALE_HEIGHT
    atmo.haze_phase_g = HAZE_PHASE_G
    atmo.haze_max_distance_km = HAZE_MAX_DIST_KM
    atmo.haze_sun_shadow = MODE_OFF
    pbc.atmosphere = atmo
    pbc.sky_source = "procedural"

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    nrm = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

    def prim(name, pos, scale):
      drawable = model.createDrawable()
      for si in drawable.modelinst.submeshinsts:
        mtl = si.material.clone()
        mtl.assignImages(ctx, color=white, normal=nrm, mtlruf=white, doConform=True)
        mtl.baseColor = vec4(0.8, 0.8, 0.8, 1)
        mtl.metallicFactor = 0.0
        mtl.roughnessFactor = 0.55
        si.overrideMaterial(mtl)
      n = scene.createDrawableNodeOnLayers([layer, dpp_layer], name, drawable)
      n.worldTransform.translation = pos
      n.worldTransform.scale = scale
      return n

    keep = [prim("floor", vec3(0, -60.0, 150.0), 60.0)]
    for i, (px, z) in enumerate(PILLAR):
      keep.append(prim("pillar%d" % i, vec3(px, EYE_Y + 2.0, z), PILLAR_SCALE))

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = SUN_INTENSITY
    sun.data.shadowBias = 0.06
    sun.data.shadowMapSize = SHADOW_MAP_SIZE
    sun.data.shadowCascadeCount = SHADOW_CASCADES
    sun.data.shadowMaxDistance = SHADOW_MAX_DIST
    sun.shadowCaster = True
    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    sun.lookAt(vec3(d[0], d[1], d[2]) * 1000.0, vec3(0, 0, 0), vec3(0, 1, 0))
    _n = layer.createLightNode("sun", sun)
    scene.lightingmanager.gpuInit(ctx)

    camlut = lev2.CameraDataLut()
    cam = lev2.CameraData()
    cam.perspective(0.5, 3000.0, FOVD)
    cam.lookAt(vec3(0, EYE_Y, 0), vec3(0, EYE_Y, 100), vec3(0, 1, 0))
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
        assert ok, "hzs stereo: capture never landed for %s %s" % (tag, nm)
        cb = caps[nm]
        arr = numpy.array(cb, dtype=numpy.uint8).reshape(cb.height, cb.width, 4)[..., :3]
        shots["%s_%s" % (tag, nm)] = arr.copy()
        path = os.path.join(outdir, "stereo_%s_%s.png" % (tag, nm))
        ensure_parent_dir(path)
        Image.fromarray(arr).save(path)
      print("[hzs] stereo captured %s" % tag, flush=True)

    take("off", SETTLE_FRAMES)
    atmo.haze_sun_shadow = MODE_INLINE
    take("inline", STEP_SETTLE)
    atmo.haze_sun_shadow = MODE_QUARTER
    take("quarter", STEP_SETTLE)

    # SAME MASK CONSTRUCTION AS THE MONO PHASE, per eye: where that eye's inline
    # mode moved that eye's image. Per eye and not shared, because the whole
    # stereo claim is that the two eyes march different rays — a mask built from
    # one eye and applied to the other would quietly assume what it is testing.
    lum = {k: _luma(v) for k, v in shots.items()}
    masks = {}
    for nm in ("L", "R"):
      m = numpy.abs(lum["inline_%s" % nm] - lum["off_%s" % nm]) > 0.004
      masks[nm] = m
      out["shaft_px_%s" % nm] = int(m.sum())
      out["shaft_strength_%s" % nm] = (
          float(numpy.abs(lum["inline_%s" % nm] - lum["off_%s" % nm])[m].mean()) if m.any() else 0.0)
      out["parity_%s" % nm] = [float(lum["inline_%s" % nm][m].mean()) if m.any() else 0.0,
                               float(lum["quarter_%s" % nm][m].mean()) if m.any() else 0.0]

    # EYES DIFFER, measured on the union of the two eyes' shaft masks so the
    # comparison covers everything either eye considers shafted. The inline
    # number is printed beside it as the reference parallax the quarter-res mode
    # has to reproduce: a loss buffer shared across eyes reads ~0 here while
    # inline does not, which is exactly the failure this leg is for.
    both = masks["L"] | masks["R"]
    def eyedelta(tag):
      d = numpy.abs(lum["%s_L" % tag] - lum["%s_R" % tag])
      return float(d[both].mean()) if both.any() else 0.0
    out["eye_delta_off"] = eyedelta("off")
    out["eye_delta_inline"] = eyedelta("inline")
    out["eye_delta_quarter"] = eyedelta("quarter")

    out["validation_errors"] = int(ctx.validation_errors)

  return out


################################################################################
# DRIVER
################################################################################


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

  print("[hzs] MONO phase (msaa %d)" % MSAA, flush=True)
  R = _mono(outdir)
  if R is None:
    return verdict(False, "mono phase produced no measurement (loop exited early)")

  sm, lm = R["shadowed_mean"], R["lit_mean"]

  # ---- (A) the rig has shafts, and both modes render them
  check("A_the_rig_actually_has_shafts",
        R["shaft_px"] >= MIN_SHAFT_PX and R["shaft_strength"] >= MODE_MIN_MOVE,
        "mask=%d px (floor %d) mean|m1-m0| over mask=%.4f (floor %.4f)"
        % (R["shaft_px"], MIN_SHAFT_PX, R["shaft_strength"], MODE_MIN_MOVE))
  d1 = abs(sm["m1"] - sm["m0"])
  d2 = abs(sm["m2"] - sm["m0"])
  check("A_inline_moves_the_shafted_region", d1 >= MODE_MIN_MOVE,
        "|m1-m0|=%.4f (floor %.4f)" % (d1, MODE_MIN_MOVE))
  check("A_quarter_moves_the_shafted_region", d2 >= MODE_MIN_MOVE,
        "|m2-m0|=%.4f (floor %.4f)" % (d2, MODE_MIN_MOVE))

  # ---- (B) mode parity, on the mean over the mask
  rel_g1 = abs(sm["m2"] - sm["m1"]) / max(abs(sm["m1"]), 1e-6)
  check("B_mode_parity_shafted_mean", rel_g1 <= PARITY_MEAN_TOL,
        "shafted_mean inline=%.4f quarter=%.4f rel=%.3f (tol %.3f)"
        % (sm["m1"], sm["m2"], rel_g1, PARITY_MEAN_TOL))

  # ---- (C) the sky is untouched, and the frame as a whole is NOT (control)
  sq = R["sky_equal"]
  check("C_sky_band_byte_identical_across_modes",
        sq["m0_m1"][0] and sq["m0_m2"][0] and sq["m1_m2"][0],
        "m0-m1=(max %d, %d px) m0-m2=(max %d, %d px) m1-m2=(max %d, %d px)"
        % (sq["m0_m1"][1], sq["m0_m1"][2], sq["m0_m2"][1], sq["m0_m2"][2],
           sq["m1_m2"][1], sq["m1_m2"][2]))
  check("C_control_the_sky_band_holds_no_shafts", R["sky_is_outside_the_mask"] == 0,
        "shaft-mask pixels inside the sky band = %d (must be 0, else the leg is circular)"
        % R["sky_is_outside_the_mask"])
  check("C_control_the_frame_is_not_identical", not R["shadowed_equal_m1_m2"][0],
        "m1 vs m2 whole frame: max %d on %d px (must differ, else the sky leg is vacuous)"
        % (R["shadowed_equal_m1_m2"][1], R["shadowed_equal_m1_m2"][2]))

  # ---- (D) no per-sample splitting at msaa 4
  r1, r2 = R["rough"]["m1"], R["rough"]["m2"]
  ratio = r2 / max(r1, 1e-9)
  check("D_quarter_res_adds_no_high_frequency_energy", ratio <= ROUGHNESS_CEIL,
        "roughness m1=%.5f m2=%.5f ratio=%.3f (ceil %.2f)" % (r1, r2, ratio, ROUGHNESS_CEIL))

  # ---- (E) FRAME STABILITY AT REST, then the gain default.
  # The repeat legs come first because they are the ones that can condemn the
  # mode: a frame that differs from itself with nothing touched is the two-tone
  # flicker class, and no parity number measured on top of it means anything.
  rs = R["repeat_stability"]
  for tag, name in (("m1", "inline"), ("m2", "quarter")):
    e = rs[tag]
    check("E_frame_stable_at_rest_%s" % name, e[0],
          "two captures, identical state: max %d on %d px "
          "(of the differing px, %.0f%% sit on silhouettes, %.0f%% inside the shaft mask)"
          % (e[1], e[2], 100.0 * R["repeat_edge_frac_%s" % tag],
             100.0 * R["repeat_shaft_frac_%s" % tag]))
  g1 = R["gain1_is_default"]
  check("E_gain_1_is_byte_identical_to_the_default", g1["m1"][0] and g1["m2"][0],
        "inline=(max %d, %d px) quarter=(max %d, %d px)"
        % (g1["m1"][1], g1["m1"][2], g1["m2"][1], g1["m2"][2]))

  # ---- (F) gain 2 deepens the shafts, and only the shafts
  for mode, a, b in (("inline", "m1_gain1", "m1_gain2"), ("quarter", "m2_gain1", "m2_gain2")):
    moved = sm[a] - sm[b]     # darker => positive
    lit_move = abs(lm[b] - lm[a])
    check("F_gain2_deepens_shafts_%s" % mode, moved >= GAIN_MIN_MOVE,
          "shafted %.4f -> %.4f (delta %.4f, floor %.4f)" % (sm[a], sm[b], moved, GAIN_MIN_MOVE))
    check("F_gain2_leaves_unshafted_geometry_%s" % mode, lit_move <= LIT_STABLE_TOL,
          "unshafted %.4f -> %.4f (delta %.4f, ceil %.4f)" % (lm[a], lm[b], lit_move, LIT_STABLE_TOL))

  # ---- (G) negative control: no occluder, no gain effect
  nc = R["nocast_gain_noop"]
  check("G_gain_is_a_noop_without_an_occluder", nc["m1"][0] and nc["m2"][0],
        "inline=(max %d, %d px) quarter=(max %d, %d px) control frame mean=%.4f"
        % (nc["m1"][1], nc["m1"][2], nc["m2"][1], nc["m2"][2], R["nocast_mean"]))
  check("G_control_frame_is_a_real_render", R["nocast_mean"] > 0.02,
        "mean_luma=%.4f" % R["nocast_mean"])

  # ---- (H) gain parity between modes
  rel_g2 = abs(sm["m2_gain2"] - sm["m1_gain2"]) / max(abs(sm["m1_gain2"]), 1e-6)
  check("H_mode_parity_holds_under_gain", rel_g2 <= PARITY_MEAN_TOL,
        "shafted_mean inline=%.4f quarter=%.4f rel=%.3f (tol %.3f)"
        % (sm["m1_gain2"], sm["m2_gain2"], rel_g2, PARITY_MEAN_TOL))

  # ---- STEREO, in a child process
  print("[hzs] STEREO phase (single-pass node, child process)", flush=True)
  env = dict(os.environ)
  rc = subprocess.call([sys.executable, os.path.abspath(__file__), outdir, "--stereo-child"],
                       env=env)
  spath = os.path.join(outdir, "stereo.json")
  if rc != 0 or not os.path.exists(spath):
    check("I_stereo_child_completed", False, "rc=%d json=%s" % (rc, os.path.exists(spath)))
  else:
    with open(spath) as f:
      S = json.load(f)
    if not S.get("multiview"):
      print("  SKIP stereo legs — device reports no multiview support", flush=True)
    else:
      check("I_stereo_rig_has_shafts_in_both_eyes",
            min(S["shaft_px_L"], S["shaft_px_R"]) >= MIN_SHAFT_PX // 4,
            "mask L=%d R=%d px strength L=%.4f R=%.4f"
            % (S["shaft_px_L"], S["shaft_px_R"],
               S["shaft_strength_L"], S["shaft_strength_R"]))
      ed = S["eye_delta_quarter"]
      check("I_eyes_carry_their_own_march", ed >= STEREO_MIN_EYE_DELTA,
            "|L-R| over the shaft masks: quarter=%.5f (floor %.5f) inline=%.5f off=%.5f"
            % (ed, STEREO_MIN_EYE_DELTA, S["eye_delta_inline"], S["eye_delta_off"]))
      for nm in ("L", "R"):
        a, b = S["parity_%s" % nm]
        rel = abs(b - a) / max(abs(a), 1e-6)
        check("J_per_eye_parity_%s" % nm, rel <= PARITY_MEAN_TOL,
              "inline=%.4f quarter=%.4f rel=%.3f (tol %.3f)" % (a, b, rel, PARITY_MEAN_TOL))
      check("J_stereo_no_validation_errors", int(S.get("validation_errors", 0)) == 0,
            "validation_errors=%d" % int(S.get("validation_errors", 0)))

  detail = ("parity_g1=%.3f parity_g2=%.3f rough_ratio=%.3f out=%s"
            % (rel_g1, rel_g2, ratio, outdir))
  if failures:
    detail += " failed=" + ",".join(failures)
  return verdict(len(failures) == 0, detail)


if __name__ == "__main__":
  sys.exit(main())
