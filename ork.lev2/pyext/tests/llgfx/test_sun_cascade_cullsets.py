#!/usr/bin/env ork.python
################################################################################
# SUN CASCADE CULLSETS — one cull volume and one survivor list PER NAMED SET.
#
# WHAT THE FEATURE CLAIMS. The sun's cascade bands used to share ONE union cull
# volume (the outermost band's reach) and ONE survivor list, so every caster
# inside that reach entered every band's depth pass. Push the outer band out to
# kilometre scale and a scattered canopy — a hundred thousand instances — is
# dragged into the near bands' draws as well as its own.
#
# A CULLSET is a NAMED list of caster FAMILIES (terrain / instanced / other);
# each band subscribes to exactly one. A set is culled ONCE against the union of
# only ITS bands' fit radii, and its bands draw only its families. Author
# near={terrain,instanced,other} on the inner bands and far={terrain} on the
# outer one, and the canopy stays in the near set's small volume while the far
# band draws mountains alone.
#
# THE RIG. A large gently-curved ground body and three floating occluders whose
# shadows are read independently by banishing them one at a time:
#   FT  far, tagged "terrain"    — beyond every near band, inside the far one
#   FI  far, tagged "instanced"  — same distance, same size, different FAMILY
#   NI  near, tagged "instanced" — inside the near bands
# Two arms of the same rig: UNAUTHORED (no cullsets — the compatibility path)
# and AUTHORED (near/far as above). Every pixel leg compares the two arms.
#
# LEGS — MONO
#   (M1) ALL THREE CAST WHEN     the anti-vacuity control. Each occluder's mask is
#        NOTHING IS FILTERED     found in the UNAUTHORED arm by banishing it; all
#                                three must be real, sizeable shadows there. Absent
#                                this, "the far tree does not shadow" could just as
#                                well mean "nothing shadows out there at all".
#   (M2) THE FAR BAND DROPS THE  over FI's own mask, the AUTHORED arm must match the
#        INSTANCED FAMILY        capture taken with FI physically absent — and must
#                                be brighter than the unauthored arm by essentially
#                                the whole shadow. The filter, proven negatively.
#   (M3) ...AND STILL DRAWS      over FT's mask, the AUTHORED arm must still match
#        THE TERRAIN ONE         the unauthored arm. Without this, M2 would also
#                                pass if the far band had simply stopped drawing.
#   (M4) NEAR RANGE IS UNTOUCHED over NI's mask, the AUTHORED arm must still match
#                                the unauthored arm: an instanced caster inside the
#                                near set shadows exactly as it did. This is the
#                                "no look regression where trees should shadow" leg.
#   (M5) SURGICAL                everywhere OUTSIDE the three masks, the two arms
#                                agree. A cullset rig may not move pixels it was
#                                not authored to move.
#
# LEGS — STEREO + CENSUS (child process, single-pass node)
#   (S1) PER-SET VOLUMES         the engine's own [cullbox] lines must show TWO sets
#                                with DIFFERENT extents, the near set's strictly
#                                smaller. One volume per set is the whole premise.
#   (S2) PER-BAND DRAW TRAFFIC   the [cullset] census must show the far band
#                                enqueueing terrain casters and ZERO instanced ones,
#                                while a near band enqueues both. This is the
#                                draw-traffic claim as a count, not as a pixel.
#   (S3) CLEAN STEREO            zero validation errors under the single-pass node,
#                                over frames that actually rendered something.
#
# LEG — COMPATIBILITY (child processes)
#   (C1) THE UNAUTHORED SUITE    test_haze_quarter_res / test_sun_cascade_5band /
#        STAYS GREEN             test_sun_cascades_gate / test_sun_snapshot_amortize
#                                are re-run verbatim and must all still pass. None of
#                                them authors a cullset, so they are the compatibility
#                                contract for the unauthored path.
#
# Self-configuring: no arguments, no environment.
#   ork.python test_sun_cascade_cullsets.py [outdir]
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
# THE MONO CAMERA is the standard scenegraph component's ui camera, whose
# effective vertical field is ~16.5 degrees (measured off this rig's own render:
# a 37.5 m ball at 357 m filled 343 of 473 rows). Every position below is placed
# for THAT field — a wider one guessed from a fovy argument put the far shadows
# off the bottom of the frame and the near occluder across half of it.
CAM_EYE           = (0.0, 350.0, -50.0)
CAM_TGT           = (0.0, -14.0, 950.0)   # ~20 degrees down
CAM_NEAR, CAM_FAR = 1.0, 20000.0

# THE LADDER. radius[i] = BAND_RADIUS * BAND_RATIO^i -> 11/50/234/1100/5171 m.
# Bands 0..3 are the NEAR set (outer 1100 m), band 4 alone is the FAR set. The
# visible ground straddles that boundary: everything nearer than ~1100 m from
# the EYE is in the near set, everything past it is the far band's alone.
BAND_RADIUS       = 10.6
BAND_RATIO        = 4.7
CASCADES          = 5
NEAR_OUTER        = BAND_RADIUS * (BAND_RATIO ** 3)  # ~1100 m
FAR_OUTER         = BAND_RADIUS * (BAND_RATIO ** 4)  # ~5171 m

SHADOW_MAP_SIZE   = 2048
# CASTER CEILING, generous: the toward-light extrusion has to reach casters that
# stand a hundred metres off the band they cast into.
SHADOW_MAX_DIST   = 500.0
# WORLD METRES of depth slack. The far band's texels are 2*5171/2048 = 5 m
# across, so the slope-induced error over one texel of a curved ground body is
# metres, not centimetres.
SHADOW_BIAS       = 1.0

# GROUND. The asset is a UNIT sphere (verified against the glb's own POSITION
# bounds), so a node scale of R is a ball of radius R metres. R = 25 km centred
# 25 km down puts its top at y = 0 and the camera 350 m above it, with a horizon
# (4.2 km) past everything measured. Get this wrong by even 2x and the camera
# ends up INSIDE the ground body: the frame renders with no ground in it and
# every mask comes back empty.
GROUND_RADIUS     = 25000.0
GROUND_POS        = (0.0, -GROUND_RADIUS, 150.0)

# OCCLUDERS — spheres floating over the ground they shadow. The sun travels +X
# at 45 degrees, so a caster h metres above the ground drops its shadow h metres
# to the +X of itself: caster and shadow are disjoint on screen.
#
# THEY ARE PAINTED RED, and every measurement below is restricted to pixels that
# are neutral (i.e. GROUND) in every capture. Banishing a caster stops it
# covering its own pixels too, and those would otherwise enter its "shadow" mask
# and dominate it — measured: a 0.39 luma "shadow" that was the sphere itself.
OCC_Y             = 100.0
OCC_RADIUS        = 37.5
OCC_COLOR         = (1.0, 0.04, 0.04, 1.0)
GROUND_COLOR      = (0.8, 0.8, 0.8, 1.0)
FT_POS            = (-312.0, OCC_Y, 1400.0)  # far, "terrain"   -> shadow ~1500 m from the eye
FI_POS            = (+48.0,  OCC_Y, 1400.0)  # far, "instanced" -> shadow ~1500 m from the eye
NI_POS            = (-48.0,  OCC_Y,  800.0)  # near,"instanced" -> shadow ~920 m from the eye
# ...and where each goes to be read out: far enough to be in no band at all, so
# the capture taken with it banished is the UNSHADOWED reference for its own
# mask. Nothing else in the scene moves.
BANISHED          = (0.0, OCC_Y, 90000.0)

SUN_ELEV_DEG      = 45.0
SUN_AZIM_DEG      = -90.0   # light TRAVELS +X (see dir_to_sun)
SUN_INTENSITY     = 6.0
SKY_EXPOSURE      = 6.0

# THE AUTHORED RIG under test. "other" rides the near set because props are
# near-field casters that a kilometre-scale band has no business drawing.
CULLSETS_SPEC     = "near=terrain,instanced,other;far=terrain"
BAND_SPEC         = "near,near,near,near,far"

# The refresh gate is DISARMED (all three thresholds 0 = refit every frame).
# This rig's A/B moves occluders without changing the caster COUNT, which is one
# of the three premises the gate reads — armed, it would hold the snapshot taken
# before the move and every capture after the first would be the same shadow.
SETTLE_FRAMES     = 90
STEP_FRAMES       = 40

# ------------------------------------------------------------------ thresholds
# (M1) each occluder's shadow, in luma over its own mask.
MASK_THRESH       = 0.010   # a pixel is "in the shadow" if banishing brightens it this much
MIN_MASK_PX       = 120
CASTER_HALO       = 10      # px of caster silhouette excluded from every mask (see _measure)
MIN_MASK_DELTA    = 0.020
# (M2) the far instanced caster must come back essentially all the way to lit.
LIFT_MIN_FRAC     = 0.70    # of its own unauthored shadow depth
LIFT_RESIDUAL     = 0.006   # ...and land this close to the physically-absent capture
# (M3)/(M4) the survivors must not move.
KEEP_MAX_DELTA    = 0.006
# (M5) everything else.
SURGICAL_MAX_MEAN = 0.0015
SURGICAL_MAX_PX_FRAC = 0.02

OUT_DEFAULT = os.path.join(tempfile.gettempdir(), "sun_cascade_cullsets")

# the compatibility suite — re-run verbatim; none of them authors a cullset.
COMPAT_SUITE = [
    "test_haze_quarter_res.py",
    "test_sun_cascade_5band.py",
    "test_sun_cascades_gate.py",
    "test_sun_snapshot_amortize_gate.py",
]


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return (math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


def _luma(rgb):
  """relative luminance. Byte captures are normalised; the mono arm's float
  captures are the compositor's LINEAR output and are read as they stand."""
  import numpy
  a = rgb.astype(numpy.float64)
  y = 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]
  return y / 255.0 if rgb.dtype == numpy.uint8 else y


################################################################################
# THE RIG. One populate() builds the casters, the ground and the sun into
# whichever scene it is handed, so the mono and stereo arms cannot drift apart —
# they differ only in the compositor that draws them.
################################################################################


def populate(scene, ctx, layers):
  """Build ground + the three occluders + the sun. Returns (nodes, home, sun)."""
  from orkengine import lev2
  from orkengine.core import vec3, vec4

  pbc = scene.pbr_common
  pbc.enable_skybox = True
  atmo = lev2.SkyAtmosphereData()
  atmo.sky_exposure = SKY_EXPOSURE
  atmo.ibl_crossfade_frames = 0   # a half-faded IBL is not a reading
  pbc.atmosphere = atmo
  pbc.sky_source = "procedural"

  model = lev2.XgmModel("data://tests/pbr_calib.glb")
  white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
  nrm = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")

  def body(name, pos, radius_m, family, color):
    drawable = model.createDrawable()
    # MATTE, FLAT-COLOURED: every leg is a luma difference over the receiving
    # ground, and the asset's calibration chart would turn a shadow measurement
    # into a texture measurement.
    for si in drawable.modelinst.submeshinsts:
      mtl = si.material.clone()
      mtl.assignImages(ctx, color=white, normal=nrm, mtlruf=white, doConform=True)
      mtl.baseColor = vec4(*color)
      mtl.metallicFactor = 0.0
      mtl.roughnessFactor = 0.55
      si.overrideMaterial(mtl)
    # THE CLASSIFICATION SEAM under test: a drawable declares its sun-cullset
    # caster family. Producers stamp their own (terrain, the instanced cull
    # paths); here the scene says so directly, which is what lets this gate
    # exercise the filter with no terrain system and no scatterer.
    drawable.shadowFamily = family
    n = scene.createDrawableNodeOnLayers(layers, name, drawable)
    n.worldTransform.translation = vec3(*pos)
    n.worldTransform.scale = radius_m   # unit-sphere asset: scale IS the radius
    return n

  nodes = {
      "ground": body("ground", GROUND_POS, GROUND_RADIUS, "terrain", GROUND_COLOR),
      "FT": body("FT", FT_POS, OCC_RADIUS, "terrain", OCC_COLOR),
      "FI": body("FI", FI_POS, OCC_RADIUS, "instanced", OCC_COLOR),
      "NI": body("NI", NI_POS, OCC_RADIUS, "instanced", OCC_COLOR),
  }
  home = {"FT": FT_POS, "FI": FI_POS, "NI": NI_POS}
  only(nodes, home, None)   # the first capture is the empty-sky reference

  sun = lev2.DynamicDirectionalLight()
  sun.data.color = vec3(1, 1, 1)
  sun.data.intensity = SUN_INTENSITY
  sun.data.shadowBias = SHADOW_BIAS
  sun.data.shadowMapSize = SHADOW_MAP_SIZE
  sun.data.shadowCascadeCount = CASCADES
  sun.data.shadowMaxDistance = SHADOW_MAX_DIST
  sun.data.shadowBandRadius = BAND_RADIUS
  sun.data.shadowBandRatio = BAND_RATIO
  sun.data.shadowRefreshAngleDeg = 0.0   # gate DISARMED — see SETTLE_FRAMES
  sun.data.shadowRefreshDistance = 0.0
  sun.data.shadowRefreshMaxSecs = 0.0
  sun.shadowCaster = True
  d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
  sun.lookAt(vec3(d[0], d[1], d[2]) * 20000.0, vec3(0, 0, 0), vec3(0, 1, 0))
  layers[0].createLightNode("sun", sun)
  scene.lightingmanager.gpuInit(ctx)
  return nodes, home, sun


def only(nodes, home, keep):
  """leave ONE occluder standing and banish the rest out of every band (keep=None
  empties the sky). The capture taken with nothing standing is the UNSHADOWED
  reference every mask is found against."""
  from orkengine.core import vec3
  for k, pos in home.items():
    nodes[k].worldTransform.translation = vec3(*(pos if k == keep else BANISHED))


def authored(sun, on):
  """arm/disarm the cullset rig. An edit to it is a STRUCTURAL change to the sun
  snapshot, so the next frame refits rather than holding one fit for the other
  partition."""
  sun.data.shadowCullSets = CULLSETS_SPEC if on else ""
  sun.data.shadowBandCullSets = BAND_SPEC if on else ""


################################################################################
# MONO PHASE — the standard offscreen scenegraph app (the capture path every
# llgfx pixel gate uses: a real viewport, its own RtGroup, an async capture
# collected on a later frame).
################################################################################


def _mono(outdir):
  import numpy
  from PIL import Image as PILImage
  from orkengine.core import vec3
  from orkengine import lev2
  from ork.app.application import ComponentizedApplication
  from ork.app.std_scenegraph import StandardSceneGraphComponent
  from ork.testing import Watchdog

  class CullSetApp(ComponentizedApplication):

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
      # (tag, the state to leave behind for the NEXT capture).
      #
      # ONE OCCLUDER AT A TIME. Each caster is measured in a scene that contains
      # only it, against a reference frame that contains none — so a mask is that
      # caster's shadow and nothing else. Reading three casters out of one frame
      # by banishing them one by one looked equivalent and was not: the masks
      # picked up each other's edges and a leg could pass on the wrong region.
      self._steps = [
          ("none",    lambda: self._only("FT")),
          ("only_FT", lambda: self._only("FI")),
          ("only_FI", lambda: self._only("NI")),
          ("only_NI", lambda: (authored(self.sun, True), self._only("FT"))),
          ("aut_FT",  lambda: self._only("FI")),
          ("aut_FI",  lambda: self._only("NI")),
          ("aut_NI",  None),
      ]
      self.SGC = self.addComponent(
          "std_scenegraph", StandardSceneGraphComponent,
          eye=vec3(*CAM_EYE), tgt=vec3(*CAM_TGT), up=vec3(0, 1, 0),
          explicit_near_far=True, near=CAM_NEAR, far=CAM_FAR,
          grid_variant=None,
          msaa=1,
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
      self._nodes, self._home, self.sun = populate(SGC.scenegraph, ctx, SGC.fwd_layers)
      authored(self.sun, False)

    def _onUpdate(self, updinfo):
      pass  # static: the only frame-to-frame differences are the ones we make

    def _only(self, keep):
      """everything banished except `keep` (None = an empty sky)."""
      only(self._nodes, self._home, keep)

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
        print("[cullsets] png write failed: %r" % (e,), flush=True)
      print("[cullsets] captured %-8s %dx%d mean=%.4f" % (key, w, h, float(img.mean())),
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
        settle = SETTLE_FRAMES if step == 0 else STEP_FRAMES
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
          self._report = _measure(self._shots, outdir)
          self._exit_code = 0
          self._done = True
          self.ezapp.signalExit()
      return

  wd = Watchdog(420.0, label="sun_cascade_cullsets_mono").arm()
  app = CullSetApp()
  app.ezapp.mainThreadLoop()
  wd.disarm()
  app.ezapp.shutdown()
  return app._report


def _measure(S, outdir):
  """Reduce the captures to the numbers the driver gates on.

  EACH SHADOW'S MASK IS FOUND, NOT GUESSED: it is the set of GROUND pixels that
  the presence of that one caster darkens, in a scene that holds nothing else.
  Every statement below is therefore made exactly where that caster acts."""
  import numpy
  from PIL import Image
  lum = {k: _luma(v) for k, v in S.items()}
  h, w = S["none"].shape[:2]

  # GROUND ONLY — every pixel that is not a RED caster, nor within CASTER_HALO of
  # one, in any capture.
  #  Not a NEUTRALITY test: ground in shadow is lit by the sky alone and is
  # strongly BLUE, so "neutral" threw away most of every shadow it was supposed
  # to measure (measured: an 817 px shadow read as 62 px).
  #  And the HALO is not slack: a sphere's own unlit limb and its antialiased rim
  # are not red, they brighten when the sphere is banished, and they sit exactly
  # where the sphere was — so without the halo a caster's silhouette enters its
  # own "shadow" mask as a second blob that no cullset can lift (measured: 318 of
  # FI's 752 px, which read as a 55% filter on a filter that had worked).
  caster = numpy.zeros((h, w), dtype=bool)
  for k, v in S.items():
    a = v.astype(numpy.int32)
    caster |= ((a[..., 0] - a[..., 1]) > 30) & ((a[..., 0] - a[..., 2]) > 30)
  for axis in (1, 0):
    acc = numpy.zeros_like(caster)
    for d in range(-CASTER_HALO, CASTER_HALO + 1):
      acc |= numpy.roll(caster, d, axis=axis)
    caster = acc
  geo = ~caster

  out = {"dims": [w, h], "geo_px": int(geo.sum())}
  masks = {}
  for tag in ("FT", "FI", "NI"):
    lift = lum["none"] - lum["only_%s" % tag]   # +ve where this caster shadows
    m = geo & (lift > MASK_THRESH)
    masks[tag] = m
    n = int(m.sum())
    out["%s_px" % tag] = n
    out["%s_depth" % tag] = float(lift[m].mean()) if n else 0.0
    # ...and what the AUTHORED arm did over that same mask: how much of the
    # shadow it lifted, and how far it still is from the unshadowed reference.
    out["%s_lift" % tag] = float((lum["aut_%s" % tag] - lum["only_%s" % tag])[m].mean()) if n else 0.0
    out["%s_residual" % tag] = float((lum["none"] - lum["aut_%s" % tag])[m].mean()) if n else 0.0

  # (M5) everywhere the rig was NOT authored to act — read on the FT arm, the one
  # whose caster the authored rig keeps.
  rest = geo & ~(masks["FT"] | masks["FI"] | masks["NI"])
  d_rest = numpy.abs(lum["aut_FT"] - lum["only_FT"])
  n_rest = int(rest.sum())
  out["rest_px"] = n_rest
  out["rest_mean"] = float(d_rest[rest].mean()) if n_rest else 0.0
  out["rest_moved_frac"] = (float(int((rest & (d_rest > MASK_THRESH)).sum())) /
                            float(max(n_rest, 1)))
  out["mean"] = {k: float(v.mean()) for k, v in lum.items()}

  try:
    for tag, m in masks.items():
      Image.fromarray((m.astype(numpy.uint8) * 255)[::-1, ::-1]).save(
          os.path.join(outdir, "mask_%s.png" % tag))
  except Exception:
    pass
  return out


################################################################################
# STEREO + CENSUS PHASE — the single-pass node in a child process (one XR device
# per process). Runs the AUTHORED rig with the engine's own per-set cull-volume
# trace and per-band draw census on, so the parent can read the counts the
# feature is judged on off this run's stdout.
################################################################################


def _stereo(outdir):
  import numpy
  from PIL import Image
  from orkengine import lev2
  from orkengine.core import vec3, mtx4, VarMap
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
    vrdev.setPoseMatrix("hmd", mtx4.lookAt(vec3(*CAM_EYE), vec3(*CAM_TGT), vec3(0, 1, 0)))

    params = VarMap()
    params.preset = "FWDPBRSPVR"
    params.SkyboxIntensity = 1.0
    params.SpecularIntensity = 1.0
    params.DiffuseIntensity = 1.0
    params.AmbientLight = vec3(0.10)
    params.SkyboxTexPathStr = "<ork_envmaps2>/cold4k.xir"
    scene = lev2.scenegraph.Scene(params)
    layer = scene.createLayer("std_forward")
    # THE SUN CASCADE PASS DRAWS THIS LAYER (layersForRole "depth_prepass"), so a
    # caster that is not on it casts nothing at all.
    dpp_layer = scene.createLayer("depth_prepass")
    nodes, home, sun = populate(scene, ctx, [layer, dpp_layer])
    authored(sun, True)

    camlut = lev2.CameraDataLut()
    cam = lev2.CameraData()
    cam.perspective(CAM_NEAR, CAM_FAR, FOVD)
    cam.lookAt(vec3(*CAM_EYE), vec3(*CAM_TGT), vec3(0, 1, 0))
    # the per-view cull fan-out resolves "spawncam" outside XR presentation
    camlut.addCamera("spawncam", cam)

    for _ in range(SETTLE_FRAMES):
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
      assert ok, "cullsets stereo: capture never landed for %s" % nm
      cb = caps[nm]
      arr = numpy.array(cb, dtype=numpy.uint8).reshape(cb.height, cb.width, 4)[..., :3]
      path = os.path.join(outdir, "stereo_%s.png" % nm)
      ensure_parent_dir(path)
      Image.fromarray(arr).save(path)
      # a black eye is not a stereo pass — the census legs would read as clean
      # over a frame that rendered nothing at all.
      out["eye_mean_%s" % nm] = float(arr.mean())

    out["validation_errors"] = int(ctx.validation_errors)

  return out


################################################################################
# TRACE / CENSUS PARSERS — the engine's own instrumentation, read off the child.
################################################################################

# [cullbox] frame<N> set<name> mask<0x..> ... hr<..> hu<..> depth<..>
_CULLBOX_RE = re.compile(
    r"\[cullbox\].*?set<([^>]*)>\s+mask<0x([0-9a-fA-F]+)>.*?hr<([0-9.eE+-]+)>\s+"
    r"hu<([0-9.eE+-]+)>\s+depth<([0-9.eE+-]+)>")
# [cullset] band<N> layer<name> mask<0x..> enqueued terrain<a> instanced<b> other<c> total<d>
_CENSUS_RE = re.compile(
    r"\[cullset\]\s+band<(\d+)>\s+layer<([^>]*)>\s+mask<0x([0-9a-fA-F]+)>\s+enqueued\s+"
    r"terrain<(\d+)>\s+instanced<(\d+)>\s+other<(\d+)>\s+total<(\d+)>")


def _parse_cullboxes(text):
  """last-seen extents per set name -> {name: (mask, hr, hu, depth)}"""
  seen = {}
  for m in _CULLBOX_RE.finditer(text):
    seen[m.group(1)] = (int(m.group(2), 16), float(m.group(3)),
                        float(m.group(4)), float(m.group(5)))
  return seen


def _parse_census(text):
  """last-seen enqueue counts per band -> {band: (mask, terrain, instanced, other, total)}"""
  seen = {}
  for m in _CENSUS_RE.finditer(text):
    seen[int(m.group(1))] = (int(m.group(3), 16), int(m.group(4)),
                             int(m.group(5)), int(m.group(6)), int(m.group(7)))
  return seen


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

  from ork.testing import verdict, read_verdict

  failures = []

  def check(label, ok, detail=""):
    print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      failures.append(label)

  ##########################################################################
  print("[cullsets] MONO phase — unauthored vs authored on one rig", flush=True)
  R = _mono(outdir)

  ok_m1 = True
  detail_m1 = []
  for tag, where in (("FT", "far/terrain"), ("FI", "far/instanced"), ("NI", "near/instanced")):
    good = R["%s_px" % tag] >= MIN_MASK_PX and R["%s_depth" % tag] >= MIN_MASK_DELTA
    ok_m1 = ok_m1 and good
    detail_m1.append("%s(%s)=%d px @ %.4f" % (tag, where, R["%s_px" % tag], R["%s_depth" % tag]))
  check("M1_all_three_cast_when_unfiltered", ok_m1,
        "%s (floors %d px / %.4f) — near set out to %.0f m, far band to %.0f m"
        % ("  ".join(detail_m1), MIN_MASK_PX, MIN_MASK_DELTA, NEAR_OUTER, FAR_OUTER))

  need = LIFT_MIN_FRAC * R["FI_depth"]
  check("M2_far_band_drops_the_instanced_family",
        (R["FI_px"] >= MIN_MASK_PX) and (R["FI_lift"] >= need)
        and (abs(R["FI_residual"]) <= LIFT_RESIDUAL),
        "over FI's own %d px shadow the authored arm lifts %+.4f (floor +%.4f = %.0f%% of its "
        "%.4f depth) and lands %+.4f from the capture with FI absent (tol %.4f)"
        % (R["FI_px"], R["FI_lift"], need, 100.0 * LIFT_MIN_FRAC, R["FI_depth"],
           R["FI_residual"], LIFT_RESIDUAL))

  check("M3_far_band_still_draws_the_terrain_family",
        (R["FT_px"] >= MIN_MASK_PX) and abs(R["FT_lift"]) <= KEEP_MAX_DELTA,
        "over FT's %d px shadow the authored arm moves %+.4f (tol %.4f) while that shadow is "
        "%.4f deep — the far band did not simply stop drawing"
        % (R["FT_px"], R["FT_lift"], KEEP_MAX_DELTA, R["FT_depth"]))

  check("M4_near_range_instanced_still_shadows",
        (R["NI_px"] >= MIN_MASK_PX) and abs(R["NI_lift"]) <= KEEP_MAX_DELTA,
        "over NI's %d px shadow the authored arm moves %+.4f (tol %.4f), shadow %.4f deep — "
        "an instanced caster inside the near set is untouched"
        % (R["NI_px"], R["NI_lift"], KEEP_MAX_DELTA, R["NI_depth"]))

  check("M5_surgical_outside_the_three_masks",
        R["rest_mean"] <= SURGICAL_MAX_MEAN and R["rest_moved_frac"] <= SURGICAL_MAX_PX_FRAC,
        "over %d remaining ground px: mean|delta|=%.5f (ceil %.5f), moved frac=%.4f "
        "(ceil %.4f)" % (R["rest_px"], R["rest_mean"], SURGICAL_MAX_MEAN,
                         R["rest_moved_frac"], SURGICAL_MAX_PX_FRAC))

  ##########################################################################
  print("[cullsets] STEREO + CENSUS phase (single-pass node, child process)", flush=True)
  env = dict(os.environ)
  env["ORKID_SHADOWCULL_TRACE"] = "1"
  env["ORKID_SUN_CULLSET_CENSUS"] = "1"
  proc = subprocess.run([sys.executable, os.path.abspath(__file__), outdir, "--stereo-child"],
                        env=env, capture_output=True, text=True)
  spath = os.path.join(outdir, "stereo.json")
  if proc.returncode != 0 or not os.path.exists(spath):
    check("S_stereo_child_completed", False,
          "rc=%d json=%s\n%s" % (proc.returncode, os.path.exists(spath), proc.stderr[-2000:]))
  else:
    with open(spath) as f:
      S = json.load(f)

    boxes = _parse_cullboxes(proc.stdout)
    if ("near" in boxes) and ("far" in boxes):
      nm, nhr, nhu, nd = boxes["near"]
      fm, fhr, fhu, fd = boxes["far"]
      check("S1_two_sets_two_volumes",
            (nhr < fhr) and (nhu < fhu) and (nm != fm),
            "near mask<0x%x> hr=%.1f hu=%.1f depth=%.1f | far mask<0x%x> hr=%.1f hu=%.1f "
            "depth=%.1f — the near set is culled against its OWN %.0f m reach, not the "
            "far band's %.0f m" % (nm, nhr, nhu, nd, fm, fhr, fhu, fd, NEAR_OUTER, FAR_OUTER))
    else:
      check("S1_two_sets_two_volumes", False,
            "the engine emitted cull volumes for sets %s — expected 'near' and 'far'"
            % sorted(boxes.keys()))

    census = _parse_census(proc.stdout)
    far_band = CASCADES - 1
    if (far_band in census) and (0 in census):
      fmask, fter, finst, foth, ftot = census[far_band]
      nmask, nter, ninst, noth, ntot = census[0]
      check("S2_far_band_draws_no_instanced_caster",
            (finst == 0) and (fter >= 1) and (ninst >= 1) and (nter >= 1),
            "band %d (far set): terrain<%d> instanced<%d> other<%d> total<%d> | "
            "band 0 (near set): terrain<%d> instanced<%d> other<%d> total<%d> — the far "
            "band's depth pass never enqueues the instanced family, the near band does"
            % (far_band, fter, finst, foth, ftot, nter, ninst, noth, ntot))
    else:
      check("S2_far_band_draws_no_instanced_caster", False,
            "no per-band census for bands 0 and %d (saw %s)" % (far_band, sorted(census.keys())))

    if not S.get("multiview"):
      print("  SKIP stereo legs — device reports no multiview support", flush=True)
    else:
      check("S3_stereo_clean",
            int(S.get("validation_errors", -1)) == 0
            and float(S.get("eye_mean_L", 0.0)) > 1.0
            and float(S.get("eye_mean_R", 0.0)) > 1.0,
            "validation_errors=%s eye means L=%.2f R=%.2f (a black eye would make the "
            "census legs vacuous)" % (S.get("validation_errors"),
                                      S.get("eye_mean_L", 0.0), S.get("eye_mean_R", 0.0)))

  ##########################################################################
  # COMPATIBILITY — the unauthored suite, verbatim. None of these tests knows
  # what a cullset is, which is exactly why they are the compatibility contract.
  print("[cullsets] COMPATIBILITY phase — re-running the unauthored suite", flush=True)
  here = os.path.dirname(os.path.abspath(__file__))
  compat_ok = True
  compat_detail = []
  for name in COMPAT_SUITE:
    path = os.path.join(here, name)
    if not os.path.exists(path):
      compat_ok = False
      compat_detail.append("%s=MISSING" % name)
      continue
    p = subprocess.run([sys.executable, path], capture_output=True, text=True,
                       cwd=here, env=dict(os.environ))
    tok = read_verdict(p.stdout, p.returncode)
    compat_detail.append("%s=%s" % (name, tok))
    if tok not in ("PASS", "PASS_WITH_TEARDOWN_BUG"):
      compat_ok = False
      sys.stdout.write(p.stdout[-4000:])
      sys.stdout.write(p.stderr[-2000:])
  check("C1_unauthored_suite_still_green", compat_ok, "  ".join(compat_detail))

  ##########################################################################
  detail = "failures=%s" % (",".join(failures) if failures else "none")
  return verdict(len(failures) == 0, detail)


if __name__ == "__main__":
  sys.exit(main())
