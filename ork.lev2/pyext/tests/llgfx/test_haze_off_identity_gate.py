#!/usr/bin/env ork.python
################################################################################
# HAZE OFF-IDENTITY GATE (G1) — the two ways of saying "no artist haze" must be
# the SAME FRAME, bit for bit.
#
# The haze layer ships OFF: zero density, and every other knob at a value that
# only means something once density is nonzero. Two claims follow from that, and
# neither is provable by looking at a picture — a knob that leaks a fraction of a
# level is invisible to an eye and fatal to the "disabled => unchanged" law the
# whole feature was designed under (HAZE.md, repo laws). So both are compared
# with numpy.array_equal, on captures taken in ONE warm process so the comparison
# cannot pick up a build, a cache or a driver difference:
#
#   A  atmosphere attached, NO haze knob touched — the shipped defaults, which is
#      what every scene in the tree renders today.
#   B  the same, with haze_density EXPLICITLY zero and every OTHER haze knob
#      cranked (deep warm layer, hard forward lobe, tinted both ways). Zero
#      density must make all of them unreachable: A == B.
#   Z  aerial_perspective_enable FALSE, every haze knob at its default.
#   C  aerial_perspective_enable FALSE, density cranked HARD and every other knob
#      cranked with it. The disarmed uniform must make the whole march
#      unreachable, knobs and all: Z == C.
#
# WHY C IS COMPARED AGAINST Z AND NOT AGAINST A. At zero artist density the seam
# still integrates the GEOPHYSICAL medium — that is a REAL rendered effect, the
# one the horizon-convergence gate (G2) measures over its 51 km sphere ladder —
# so armed-at-zero and disarmed are not the same frame, and asserting they were
# would gate a bug that is not one. Measured on this scene: A vs Z is 1 level on
# 270 px of 470k at these near-field distances, and 5 levels on 879 px with the
# row moved out 10x. It SCALES WITH DISTANCE, which is what identifies it as the
# geophysical march rather than as numerical noise. The residual is therefore
# PRINTED as evidence, on the same footing as G2's v1 seam, and the identity
# claim is made where it is exactly true: between two disarmed frames, and
# between two zero-density frames.
#
#   D  the DSL LEG, and the only capture that is allowed to differ: the haze is
#      authored through Scene.sky(haze={"preset": "bladerunner",
#      "distance_m": 200}) — the new authoring surface — and the resulting
#      SkyAtmosphereData's own haze fields are transplanted onto the live
#      atmosphere. D != A proves the DSL's WORLD-METRE numbers survive the unit
#      conversion, the preset merge and the publish and land on actual pixels; a
#      DSL that authored nothing would pass an identity check trivially.
#
# The DSL is also checked in pure author phase (no device): haze=None and
# haze="clear" must publish NO atmosphere object at all — which is what makes
# capture A itself the "clear" frame — the metre->kilometre conversions must
# match the atmosphere's own world scale, an unknown preset and an unknown knob
# must RAISE, and an IBL declaration alongside a haze declaration must produce
# ONE merged object (two would clobber each other: the engine reads _atmosphere
# as a whole pointer).
#
# medium_hash is asserted identical across every configuration: the haze knobs
# are presentation-tier by design (HAZE.md 3.6 risk 3), and a haze value that
# reached the hash would silently re-bake the static LUT chain on every live
# look tweak.
#
# Not ork.testing capture_app: that harness drives ONE mainThreadLoop capture and
# cannot host a scenegraph app (see test_sky_floor_gate.py); this gate needs five
# captures with live parameter changes in one warm process. The
# verdict-before-teardown protocol (#57) is honoured.
################################################################################
import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)

import math
import numpy
from PIL import Image as PILImage
from orkengine.core import vec3, vec4
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.testing import verdict, Watchdog

WIDTH, HEIGHT = 800, 600
EYE_Y         = 2.0
CAM_NEAR, CAM_FAR = 0.5, 4000.0
FOVY_DEG      = 45.0

SUN_ELEV_DEG  = 25.0
SUN_AZIM_DEG  = 180.0      # behind the camera: front-lit spheres, smooth sky
SUN_INTENSITY = 8.0
SKY_EXPOSURE  = 7.0

SETTLE_FRAMES = 90         # after scene build, before the first capture
STEP_SETTLE   = 40         # after each parameter change

# The cranked layer. Density is far past any authored look on purpose: at 100 m
# it is an optical depth of 0.8, so a leak through either gate is not a subtle
# one. Scale height covers the whole scene at eye level, and the two tints are
# pulled apart so a leak of either shows as a colour shift, not a level shift.
CRANK_DENSITY      = 8.0    # 1/km at layer base
CRANK_SCALE_HEIGHT = 2.0    # km
CRANK_PHASE_G      = 0.85
CRANK_SCATTER_TINT = (1.00, 0.72, 0.40)
CRANK_INSCAT_TINT  = (1.20, 0.60, 0.25)
CRANK_MAX_DIST_KM  = 160.0

# the DSL config capture D authors, and what it must resolve to (metres -> 1/km
# through the atmosphere's own _kilometersPerWorldUnit).
DSL_HAZE = {"preset": "bladerunner", "distance_m": 200.0}

BALL_ANG_TAN = 0.020       # angular radius as a tangent: equal size for all
BALL_MODEL_R = 1.09        # pbr_calib.glb ball radius at scale 1

# NEAR FIELD, by design (see the header). Saturated blue-free hues so the sky
# cannot be mistaken for geometry by the coverage check.
BALLS = [("d25",   25.0, -0.10, (0.90, 0.07, 0.07)),
         ("d50",   50.0,  0.00, (0.88, 0.85, 0.06)),
         ("d100", 100.0,  0.10, (0.10, 0.80, 0.12))]

CHROMA_MIN   = 30.0     # 8-bit max-min, on a warm-dominant pixel: the sky ahead
                        # is blue-dominant, so this classifies geometry alone
MIN_GEO_PX   = 400      # coloured pixels the identity claim is actually about
DSL_MIN_MOVE = 12.0     # levels the DSL-authored haze must move the geometry

OUT_DIR = os.environ.get("HAZE_G1_OUT", "/tmp/haze_off_identity")


def dir_to_sun(azimuth_deg, elevation_deg):
  """unit vector pointing TOWARD the sun (world, Y-up); azimuth 0 = +Z."""
  a = math.radians(azimuth_deg)
  e = math.radians(elevation_deg)
  return vec3(math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e))


################################################################################
# the DSL leg — pure author phase, no device
################################################################################

def _dsl_atmo(**sky_kwargs):
  """the SkyAtmosphereData a Scene.sky(...) call publishes, or None. The star
  dome builds a mesh and a material, which needs a GPU, so the asset factory is
  stubbed (test_sky_library_parity's trick) — the declaration this reads is pure
  author phase."""
  from ork.hypergraph.ecs.scene import Scene

  class _Stub:
    def __init__(self, name):
      self.built = "built:" + name

  class _Recorder:
    def __getattr__(self, gen):
      return lambda name, **kw: _Stub(name)

  class _Harness(Scene):
    def __init__(self):
      super().__init__()
      self.asset = _Recorder()
      self.sky(skybox_path="<ork_envmaps2>/desert4k.xir", **sky_kwargs)

  decl = _Harness()._systems["SceneGraphSystem"]
  merged = {}
  for call, args, _kw in decl.sub_calls:
    if call == "declareParams":
      merged.update(args[0])
  return merged.get("SkyAtmosphere")


def dsl_checks(check):
  """everything about the haze authoring surface that does not need pixels."""
  from ork.hypergraph.ecs.scene._sky_dome import HAZE_KNOBS, HAZE_PRESETS

  # SILENCE. Undeclared and "clear" are the same statement: no artist layer,
  # therefore no object — the engine's own zero density is already that.
  quiet = [(k, _dsl_atmo(**kw)) for k, kw in
           (("absent", {}), ("None", dict(haze=None)), ("clear", dict(haze="clear")))]
  check("dsl_silence_publishes_nothing", all(a is None for _k, a in quiet),
        "published=%s" % [k for k, a in quiet if a is not None])

  # UNITS. distance_m is an e-folding distance in WORLD METRES; the engine wants
  # 1/km, converted through the atmosphere's own world scale.
  a = _dsl_atmo(haze=DSL_HAZE)
  kpw = a.kilometers_per_world_unit if a is not None else 0.0
  want_d = 1.0 / (DSL_HAZE["distance_m"] * kpw) if kpw else 0.0
  want_h = HAZE_PRESETS["bladerunner"]["scale_height_m"] * kpw
  ok = (a is not None
        and abs(a.haze_density - want_d) < 1.0e-4
        and abs(a.haze_scale_height - want_h) < 1.0e-6
        and abs(a.haze_phase_g - HAZE_PRESETS["bladerunner"]["phase_g"]) < 1.0e-6)
  check("dsl_units_and_preset_merge", ok,
        "km_per_wu=%.2e density=%.4f (want %.4f) scale_h=%.4f (want %.4f) g=%.3f"
        % (kpw, a.haze_density if a else -1, want_d,
           a.haze_scale_height if a else -1, want_h, a.haze_phase_g if a else -1))

  # ONE OBJECT. The engine reads _atmosphere as a whole pointer, so a scene that
  # declares both an IBL cadence and a haze look must get them MERGED.
  m = _dsl_atmo(haze="bladerunner", ibl_snapshot_interval=5.0)
  check("dsl_ibl_and_haze_merge_into_one_object",
        m is not None and abs(m.ibl_snapshot_interval - 5.0) < 1.0e-6
        and m.haze_density > 0.0,
        "ibl_snapshot_interval=%.2f haze_density=%.4f"
        % (m.ibl_snapshot_interval if m else -1, m.haze_density if m else -1))

  # LOUD. Neither a preset name nor a knob name reaches declareParams, so a
  # misspelling would author a sky with no haze in it and say nothing.
  raised = []
  for label, bad in (("preset", "smog"), ("knob", {"distance": 100.0}),
                     ("type", 4.0)):
    try:
      _dsl_atmo(haze=bad)
      raised.append("%s:SILENT" % label)
    except ValueError as e:
      full = all(n in str(e) for n in
                 (sorted(HAZE_KNOBS)[0], sorted(HAZE_PRESETS)[0]))
      raised.append("%s:%s" % (label, "raised" if full else "raised-but-terse"))
  check("dsl_unknown_names_raise_with_vocabulary",
        all(r.endswith(":raised") for r in raised), " ".join(raised))

  return a


################################################################################

class HazeOffIdentityApp(ComponentizedApplication):

  def __init__(self, dsl_atmo):
    super().__init__()
    self._dsl_atmo = dsl_atmo
    self._frame = 0
    self._built = False
    self._phase = 0
    self._phase_frame = 0
    self._done = False
    self._shots = {}
    self._hashes = {}
    self._inflight = None
    # capture order, and the state each capture leaves behind for the next
    self._steps = [("A_default",  self._zeroDensityCranked),
                   ("B_zerodens", self._disarmedDefaults),
                   ("Z_disarmed", self._disarmedCranked),
                   ("C_disarmed", self._applyDslHaze),
                   ("D_dsl",      None)]
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
          "AmbientLevel":     vec3(0.15),
        })
    self.createEzApp(width=WIDTH, height=HEIGHT, offscreen=True,
                     use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    self.ezapp.topWidget.enableUiDraw()
    SGC.pbr_common.enable_skybox = True

    # capture A's medium: NOTHING haze-related is said here, so what renders is
    # the shipped default every scene in the tree already gets.
    self.atmo = lev2.SkyAtmosphereData()
    self.atmo.sky_exposure = SKY_EXPOSURE
    self.atmo.ibl_crossfade_frames = 0   # a half-faded IBL is not a reading
    SGC.pbr_common.atmosphere = self.atmo
    SGC.pbr_common.sky_source = "procedural"

    for name, dist, off, col in BALLS:
      SGC.createBallNode(
          name, ctx=ctx,
          position=vec3(dist * off, EYE_Y, dist),
          scale=BALL_ANG_TAN * dist / BALL_MODEL_R,
          color=vec4(col[0], col[1], col[2], 1), metallic=0.0, roughness=0.55)

    sun = lev2.DynamicDirectionalLight()
    sun.data.color = vec3(1, 1, 1)
    sun.data.intensity = SUN_INTENSITY
    sun.shadowCaster = False
    d = dir_to_sun(SUN_AZIM_DEG, SUN_ELEV_DEG)
    # DirectionalLight::direction() is the light's TRAVEL direction, so aiming it
    # from the sun's position at the origin makes dir_to_sun the negation.
    sun.lookAt(vec3(d.x, d.y, d.z) * 1000.0, vec3(0, 0, 0), vec3(0, 1, 0))
    self.sun = sun
    self.sun_node = SGC.layer_fwd.createLightNode("sun", sun)
    SGC.scenegraph.lightingmanager.gpuInit(ctx)

    # the editor camera's derived projection is not the one handed to
    # perspective() (test_skybox_depth_gate's finding); pin it directly.
    SGC.camera.perspective(CAM_NEAR, CAM_FAR, FOVY_DEG)
    SGC.camera.lookAt(vec3(0, EYE_Y, 0), vec3(0, EYE_Y, 100), vec3(0, 1, 0))

  def _onUpdate(self, updinfo):
    pass   # static: the only frame-to-frame differences are the ones we make

  ##############################################################
  # the five states

  def _crankAllButDensity(self):
    a = self.atmo
    a.haze_scale_height = CRANK_SCALE_HEIGHT
    a.haze_phase_g = CRANK_PHASE_G
    a.haze_scatter_tint = vec3(*CRANK_SCATTER_TINT)
    a.haze_inscatter_tint = vec3(*CRANK_INSCAT_TINT)
    a.haze_max_distance_km = CRANK_MAX_DIST_KM

  def _zeroDensityCranked(self):
    """B: density said out loud as zero, everything else cranked."""
    self.atmo.haze_density = 0.0
    self._crankAllButDensity()

  def _restoreDefaults(self):
    """the shipped haze values, read off a VIRGIN object rather than restated —
    a default that moves in C++ must move here too, not diverge silently."""
    a, d = self.atmo, lev2.SkyAtmosphereData()
    a.haze_density = d.haze_density
    a.haze_scale_height = d.haze_scale_height
    a.haze_phase_g = d.haze_phase_g
    a.haze_scatter_tint = d.haze_scatter_tint
    a.haze_inscatter_tint = d.haze_inscatter_tint
    a.haze_max_distance_km = d.haze_max_distance_km

  def _disarmedDefaults(self):
    """Z: the march disarmed, nothing else said."""
    self._restoreDefaults()
    self.atmo.aerial_perspective_enable = False

  def _disarmedCranked(self):
    """C: still disarmed, density cranked hard behind the closed gate."""
    self.atmo.aerial_perspective_enable = False
    self.atmo.haze_density = CRANK_DENSITY
    self._crankAllButDensity()

  def _applyDslHaze(self):
    """D: re-armed, carrying the haze the DSL authored (the numbers came from
    Scene.sky(haze=...) in author phase — this is the leg that proves they reach
    a pixel)."""
    a, src = self.atmo, self._dsl_atmo
    a.aerial_perspective_enable = True
    a.haze_density = src.haze_density
    a.haze_scale_height = src.haze_scale_height
    a.haze_phase_g = src.haze_phase_g
    a.haze_scatter_tint = src.haze_scatter_tint
    a.haze_inscatter_tint = src.haze_inscatter_tint
    a.haze_max_distance_km = src.haze_max_distance_km

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
    self._hashes[key] = int(self.atmo.medium_hash)
    self._inflight = None
    try:
      os.makedirs(OUT_DIR, exist_ok=True)
      PILImage.fromarray(img[::-1, ::-1]).save(os.path.join(OUT_DIR, "%s.png" % key))
    except Exception as e:
      print("[haze-g1] png write failed: %r" % (e,), flush=True)
    print("[haze-g1] captured %s %dx%d mean=%.3f max=%d medium_hash=%d"
          % (key, w, h, float(img.mean()), int(img.max()), self._hashes[key]),
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

  def _emitVerdict(self):
    failures = []

    def check(label, ok, detail=""):
      print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
      if not ok:
        failures.append(label)

    need = [k for k, _a in self._steps]
    if any(self._shots.get(k) is None for k in need):
      self._finish(verdict(False, "missing captures"))
      return

    A, B, Z, C, D = (self._shots[k] for k in need)

    # the frame has to CONTAIN the geometry the identity claim is about: an empty
    # sky would compare equal for free. Warm-dominant AND chromatic: the ball
    # palette is red/yellow/green, the sky ahead is blue-dominant.
    fA = A.astype(numpy.int32)
    lit = ((fA.max(axis=2) - fA.min(axis=2)) > CHROMA_MIN) \
        & (numpy.argmax(fA, axis=2) != 2)
    n_geo = int(lit.sum())
    check("geometry_is_in_frame", n_geo >= MIN_GEO_PX,
          "coloured_px=%d (floor %d)" % (n_geo, MIN_GEO_PX))

    def diff(x, y):
      d = numpy.abs(x.astype(numpy.int32) - y.astype(numpy.int32))
      return int(d.max()), int((d.any(axis=2)).sum())

    ab_max, ab_px = diff(A, B)
    zc_max, zc_px = diff(Z, C)
    az_max, az_px = diff(A, Z)
    check("A_equals_B_zero_density_makes_every_knob_unreachable",
          numpy.array_equal(A, B),
          "max_channel_delta=%d differing_px=%d" % (ab_max, ab_px))
    check("Z_equals_C_disarmed_makes_every_knob_unreachable",
          numpy.array_equal(Z, C),
          "max_channel_delta=%d differing_px=%d" % (zc_max, zc_px))

    # the DSL leg: the authored haze has to REACH the geometry, or the identity
    # checks above are only proving that nothing works at all.
    ad = numpy.abs(D.astype(numpy.int32) - A.astype(numpy.int32))
    moved = float(ad[lit].max()) if n_geo else 0.0
    check("D_dsl_authored_haze_reaches_the_geometry", moved >= DSL_MIN_MOVE,
          "max_channel_move_on_geometry=%.0f (floor %.0f) density=%.4f"
          % (moved, DSL_MIN_MOVE, self._dsl_atmo.haze_density))

    # presentation tier: no haze value may reach the static-LUT bake key.
    hashes = [self._hashes[k] for k in need]
    check("medium_hash_never_moves", len(set(hashes)) == 1,
          "medium_hash=" + " ".join(str(h) for h in hashes))

    # PRINTED, NOT GATED: what the GEOPHYSICAL march costs at zero artist
    # density over this near-field row — the term G2 gates in the far field, and
    # the reason C is compared against Z (see the header).
    print("[haze-g1] geophysical residual (armed at zero density vs disarmed): "
          "max_channel_delta=%d differing_px=%d of %d"
          % (az_max, az_px, A.shape[0] * A.shape[1]), flush=True)

    ok = (len(failures) == 0)
    detail = ("A-B=(%d,%dpx) Z-C=(%d,%dpx) geophys_residual=(%d,%dpx) "
              "dsl_move=%.0f medium_hash=%d"
              % (ab_max, ab_px, zc_max, zc_px, az_max, az_px, moved, hashes[0]))
    if failures:
      detail += " failed=" + ",".join(failures)
    self._finish(verdict(ok, detail))

  def _finish(self, rc):
    self._exit_code = rc
    self._done = True
    self.ezapp.signalExit()


def main():
  failures = []

  def check(label, ok, detail=""):
    print("  %s %s %s" % ("PASS" if ok else "FAIL", label, detail), flush=True)
    if not ok:
      failures.append(label)

  print("[haze-g1] DSL authoring surface (author phase, no device)", flush=True)
  dsl_atmo = dsl_checks(check)
  if failures or dsl_atmo is None:
    return verdict(False, "dsl leg: " + ",".join(failures or ["no atmosphere authored"]))

  wd = Watchdog(300.0, label="haze_off_identity").arm()
  app = HazeOffIdentityApp(dsl_atmo)
  app.ezapp.mainThreadLoop()
  wd.disarm()
  app.ezapp.shutdown()
  rc = getattr(app, "_exit_code", None)
  if rc is None:
    verdict(False, "loop exited before all five captures completed")
    rc = 1
  return rc


if __name__ == "__main__":
  sys.exit(main())
