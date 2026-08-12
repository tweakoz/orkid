#!/usr/bin/env python3
###############################################################################
# SKY LIBRARY declaration-parity gate — pure author phase: no device, no window,
# no engine boot beyond the pyext import. It proves that the scenes migrated onto
# the sky library still DECLARE exactly what they declared when they hand-rolled
# it, which is the cheap half of "look-preserving" (the render A/B is the other
# half and needs a GPU).
#
# What it proves:
#   1. dome params   — the declareParams dict + skybox decl kwarg produced by
#                      Scene.sky_dome()/Scene.sky() for scn_procsky is IDENTICAL
#                      (keys, order, values) to the literal scenegraph(...) block
#                      that scene carried.
#   2. append path   — when a base class already declared the scenegraph
#                      (scenegraph() forbids re-declaration), sky_dome() appends
#                      a second declareParams instead of raising, and the later
#                      keys are the ones that win.
#   3. cloud decks   — cloud_decks() emits the same material/mesh constructor
#                      kwargs, the same node declarations and the same launch
#                      transforms as the original inline deck loop (reconstructed
#                      here from the retired scn_cloudgauge, whose deck machinery
#                      the library absorbed).
#  4b. one transmittance — the deck's three Beer-Lambert sites all read the ONE
#                      shared cloud-transmittance term, and the extraction that
#                      made it shared stayed byte-inert (master ruling jul28).
#  4c. fadeout transparent — a fading deck edge un-occludes as fast as it dims
#                      (presence composes LINEARLY, never inside the exponent),
#                      while thick deck stays opaque. Owner regression, jul28.
#   5. forest procsky — the S2 migration: the amended dome params and the raised/
#                      sagged/gain-tinted decks the library declares match the
#                      hand-rolled ones the scene carried at ea485dbbf, and the
#                      scene file itself no longer carries that machinery.
#   6. display stage — WHO gets the ACES display-exposure stage and its per-tick
#                      drive: auto ON for the procedural-sky family with a
#                      celestial ensemble, OFF for a baked gauge or a scene with
#                      no ensemble, both halves declared together, tonemap last
#                      in the chain, and asking for the drive without a sun is a
#                      declaration-time error.
#   7. default surface — the other half of "look-preserving": every kwarg the
#                      library DEFAULTS is, for every migrated scene, either
#                      restated by that scene or equal to what the scene
#                      effectively had before migration (or waived by name).
#                      The signatures are introspected, so a new defaulted kwarg
#                      with no recorded decision is a red gate.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import math
import sys

# workspace-anchored (never cwd-derived): <ws>/ork.lev2/pyext/tests/llgfx/<this>
_WS = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                   "..", "..", "..", ".."))
# PREPENDED on purpose: this gate compares the library and the scenes OF THE
# TREE IT LIVES IN, so a staged copy of ork.hypergraph must not shadow them.
for _p in (os.path.join(_WS, "ork.data/scenes"), os.path.join(_WS, "obt.project/scripts")):
  if _p in sys.path:
    sys.path.remove(_p)
  sys.path.insert(0, _p)

from orkengine import core   # core before lev2
from orkengine.core import vec3

from ork.hypergraph.ecs.scene import Scene, Transform
from ork.hypergraph.ecs.scene._cloud_deck import (CLOUD_TEX, CloudLayerMtl,
                                                  DROP_FRAC, EVO_MULT,
                                                  PLANE_LOCAL_M, _LAYER_LOOK,
                                                  _PLANES)
import _cloudgauge_input as CGI

SKYBOX = "<ork_envmaps2>/desert4k.xir"

# scn_cloudgauge's static gauge sun (the deck radiometry fallback vector)
_se, _sa = math.radians(32.0), math.radians(40.0)
SUN_DIR = (math.cos(_se) * math.sin(_sa),
           math.sin(_se),
           math.cos(_se) * math.cos(_sa))

###############################################################################
# scn_forest's constants, at their shipped defaults (ea485dbbf). Both
# legs of the forest checks read these, so the gate compares mechanisms, not
# values — except CLOUD_GAIN, held OFF its 1.0 default on purpose: at gain 1.0
# the scene's mirrored colors equal CloudLayerMtl's own defaults, so a dropped
# colors= would still fingerprint identical.
###############################################################################

FOREST_SUN_ELEV_DEG   = 55.0     # 3:30 PM Jul 4
FOREST_SUN_AZIM_DEG   = 258.0    # west-southwest
FOREST_DIFF_INTENSITY = 1.5      # ORK_FORESTSKY_DIFFINT default
FOREST_CLOUD_BASE_M   = 1850.0   # AGL spec altitudes -> ASL over the alpine terrain
FOREST_TERRAIN_MEAN_M = 1451.0   # xxx3 baked mean; the dome-sag datum
FOREST_CLOUD_GAIN     = 1.3
FOREST_BASE_COLORS = {
    "lit_color":      (0.60, 0.575, 0.55),
    "shadow_color":   (0.155, 0.170, 0.20),
    "overcast_color": (0.295, 0.305, 0.325),
}
_fe, _fa = math.radians(FOREST_SUN_ELEV_DEG), math.radians(FOREST_SUN_AZIM_DEG)
FOREST_SUN_DIR = (math.cos(_fe) * math.sin(_fa),
                  math.sin(_fe),
                  math.cos(_fe) * math.cos(_fa))


def _fgain(rgb):
  return tuple(c * FOREST_CLOUD_GAIN for c in rgb)


###############################################################################
# fingerprints
###############################################################################

def sg_print(scene):
  d = scene._systems["SceneGraphSystem"]
  out = ["kwargs=%s" % sorted(d.kwargs.items())]
  for call, args, kw in d.sub_calls:
    if call == "declareParams":
      out.append("declareParams:" + ",".join(
          "%s=%s" % (k, v) for k, v in args[0].items()))
    else:
      out.append("%s%s%s" % (call, tuple(str(a) for a in args), kw))
  return "\n".join(out)


def sg_effective(scene):
  """the params the ENGINE ends up with: every declareParams merged in order
  (setUserSceneParam is dict-assignment, later keys win) plus the SystemData decl
  kwargs. This is the right fingerprint for the AMEND path — the library always
  writes the whole exposure block, where a hand-rolled amend wrote only the keys
  it meant to change, so the two are semantically equal but textually not."""
  d = scene._systems["SceneGraphSystem"]
  merged = {}
  for call, args, _kw in d.sub_calls:
    if call == "declareParams":
      merged.update(args[0])
  out = ["kwargs=%s" % sorted(d.kwargs.items())]
  out += ["%s=%s" % (k, v) for k, v in sorted(merged.items())]
  return "\n".join(out)


class _Stub:
  def __init__(self, name):
    self.built = "built:" + name
    self._name = name

  def __repr__(self):
    return "<%s>" % self._name


class _Recorder:
  """stands in for Scene.asset — records constructor kwargs instead of building
  (the gate is about WHAT is declared; building needs a device)."""

  def __init__(self, log):
    self._log = log

  def __getattr__(self, gen):
    def call(name, **kw):
      self._log.append((gen, name, {k: repr(v) for k, v in sorted(kw.items())}))
      return _Stub(name)
    return call


def deck_print(scene):
  out = ["ASSET %s %s %s" % rec for rec in scene.log]
  for name, arch in scene._archetypes.items():
    for comp in arch._components:
      out.append("ARCH %s %s %s" % (name, comp.typename,
                                    [(c, k) for c, _, k in comp.sub_calls]))
  for name, sp in scene._spawners.items():
    xf = getattr(sp, "transform", None)
    out.append("SPAWN %s %s" % (name, "None" if xf is None else
                                ";".join("%s=%s" % (a, getattr(xf, a))
                                         for a in ("translation", "orientation", "scale")
                                         if getattr(xf, a, None) is not None)))
  return "\n".join(out)


def report(label, old, new):
  ok = (old == new)
  print("[%s] %s" % (label, "IDENTICAL" if ok else "DIFFERENT"), flush=True)
  if not ok:
    import difflib
    for line in difflib.unified_diff(old.split("\n"), new.split("\n"),
                                     "hand-rolled", "library", lineterm="", n=1):
      print("  " + line, flush=True)
  return ok


###############################################################################
# 1 — dome params, per migrated scene
###############################################################################

class _OldProcSky(Scene):
  """the literal block, carrying the ONE ruled delta: AmbientLight 0.1 -> 0.0
  (owner, jul28 — the procedural-sky family is lit by sun + IBL alone). Section
  1 stays a MECHANISM check (keys, order, spelling); the flip itself is asserted
  as a value in section 7, where the per-scene decision lives."""
  def __init__(self):
    super().__init__()
    self.scenegraph(preset="ForwardPBR", skybox_path=SKYBOX,
                    SkySource="procedural", SkyboxIntensity=1.0,
                    DiffuseIntensity=1.0, SpecularIntensity=1.0,
                    AmbientLight=vec3(0.0), msaa=2, ssaa=0)


class _NewProcSky(Scene):
  def __init__(self):
    super().__init__()
    self.sky_dome(skybox_path=SKYBOX, msaa=2, ssaa=0)


class _BakedDome(Scene):
  """the BAKED spelling of the same call: sky_source=None leaves the key out
  entirely (engine default = baked). Kept as a MECHANISM leg after the baked
  gauge scene it was written for was retired — the library still supports it and
  a scene may still ask for it."""
  def __init__(self):
    super().__init__()
    self.sky_dome(sky_source=None, skybox_path=SKYBOX, ambient_light=0.1,
                  msaa=2, ssaa=0)


class _OldBakedDome(Scene):
  def __init__(self):
    super().__init__()
    self.scenegraph(preset="ForwardPBR", skybox_path=SKYBOX,
                    SkyboxIntensity=1.0, DiffuseIntensity=1.0,
                    SpecularIntensity=1.0, AmbientLight=vec3(0.1),
                    msaa=2, ssaa=0)


def test_dome_params():
  ok = report("procsky dome", sg_print(_OldProcSky()), sg_print(_NewProcSky()))
  ok &= report("baked dome", sg_print(_OldBakedDome()), sg_print(_BakedDome()))
  return ok


###############################################################################
# 2 — the append path (a base class already declared the scenegraph)
###############################################################################

class _Base(Scene):
  def __init__(self):
    super().__init__()
    self.scenegraph(preset="ForwardPBR", skybox_path=SKYBOX,
                    SkyboxIntensity=1.0, DiffuseIntensity=3.0)


class _Derived(_Base):
  def __init__(self):
    super().__init__()
    self.sky_dome(diffuse_intensity=1.5)


def test_append_path():
  decl = _Derived()._systems["SceneGraphSystem"]
  params = [a[0] for c, a, _ in decl.sub_calls if c == "declareParams"]
  if len(params) != 2:
    print("  expected 2 declareParams (base + appended), got %d" % len(params))
    return False
  ok = (params[0]["DiffuseIntensity"] == 3.0 and
        params[1]["DiffuseIntensity"] == 1.5 and
        params[1]["SkySource"] == "procedural")
  print("[append path] %s (base 3.0 -> appended %s, SkySource %s)" % (
      "OK" if ok else "BAD", params[1]["DiffuseIntensity"], params[1].get("SkySource")))
  return ok


###############################################################################
# 3 — cloud decks
#
# The two reconstructions below are the retired inline loops, each carrying the
# RULED ADDITIONS the library made since (same discipline as section 1's
# AmbientLight flip: a ruled delta is recorded IN the reconstruction with the
# decision that made it, so the leg keeps comparing mechanism and the decision
# stays visible):
#
#   night_ambient     — W15-S1 DECK NIGHT RADIANCE (merged 43909cc30, the
#                       owner-ratified night trio, jul31): the trio's measured
#                       sourceless irradiance (airglow + starlight) replaces the
#                       1%-of-day placeholder that left night decks ~35x
#                       brighter than the whole clear night sky. cloud_decks()
#                       resolves it from the scene's declared atmosphere; these
#                       harnesses declare none, so both sides read the engine
#                       default — the value recorded below.
#   the cookie node   — W11-S1 cloud ground-shadows (7529b91d3): a SECOND node
#                       per deck on the sun-cookie layer. The retired loops'
#                       decks cast nothing; that slice is what made them cast.
#
# RULED SUBTRACTION (haze S9, the cloud-deck unification): three kwargs the
# reconstructions used to carry are GONE from the library, so they are gone from
# both sides here — haze_dist_m (the per-layer e-folding distance), haze_color
# (the frozen horizon color) and day_ref_intensity (with e_pol / CgDayInv, the
# sky-side day normalizer whose only consumer was the haze mix). The deck now
# calls the engine's own aerial-perspective seam, skyAerialPerspective: distance
# falls out of the medium's scale-height physics and the fade target is the
# atmosphere's real in-scatter, so there is no deck-local distance, no deck-local
# horizon color and nothing to normalize a sky radiance against. Section 4b's
# tooth 1 is ruled with it (the deck's one exp(-) haze site became one seam call).
###############################################################################

# W15-S1's night floor at the ENGINE-DEFAULT atmosphere: 2.75e-4 of the daytime
# sky per channel, read back through the engine's float32 atmosphere properties
# (hence the tail digits). Recorded as a literal on purpose — it is the ratified
# number, so a drift in it is a red gate, not a re-record.
DECK_NIGHT_AMBIENT = (0.00027499999362134986,) * 3

# the layer role the forward node fills the transmittance cookie from
DECK_COOKIE_LAYER = "sun_cookie"


class _DeckHarness(Scene):
  def __init__(self, mode):
    super().__init__(default_sg=False)
    self.log = []
    self.asset = _Recorder(self.log)
    getattr(self, "_" + mode)()

  def _library(self):
    self.cloud_decks(sun_dir=SUN_DIR)

  def _inline(self):
    """scn_cloudgauge's deck loop, verbatim as of a23162d21, plus the ruled
    additions recorded in the section header."""
    A  = self.asset
    st = CGI.env_state()
    for ent_name, lkey, texkey in _PLANES:
      spec = CGI.LAYERS[lkey]
      look = _LAYER_LOOK[lkey]
      mtl = A.Ptex3d(ent_name + "_mtl",
                     dsl_class    = CloudLayerMtl,
                     tex_res      = 2048.0 if texkey == "cumulus2k" else 1024.0,
                     tile_base_m  = spec["tile_base_m"],
                     mesh_scale   = spec["base_scale"],
                     alt_m        = spec["alt_m"],
                     sweep_m      = spec["sweep_m"],
                     soft         = look["soft"],
                     erode        = look["erode"],
                     opacity_max  = look["opacity_max"],
                     alpha_sigma  = look["alpha_sigma"],
                     alpha_floor  = look["alpha_floor"],
                     ramp_mix     = look["ramp_mix"],
                     dens_pow     = look["dens_pow"],
                     blur_tx      = look["blur_tx"],
                     blur_px      = look["blur_px"],
                     beer_sigma   = look["beer_sigma"],
                     silver_gain  = look["silver_gain"],
                     sun_dir      = SUN_DIR,
                     wind_mps     = (spec["wind_dir"][0] * spec["wind_mps"] * CGI.WIND_MULT,
                                     spec["wind_dir"][1] * spec["wind_mps"] * CGI.WIND_MULT),
                     evo_uv       = (look["evo_uv"][0] * EVO_MULT,
                                     look["evo_uv"][1] * EVO_MULT),
                     cov_scale    = look["cov_scale"],
                     veil_max     = look.get("veil_max", 0.0),
                     night_ambient     = DECK_NIGHT_AMBIENT, # W15-S1, 43909cc30
                     sampler_textures = {"CloudTex": CLOUD_TEX[texkey]})
      plane = A.CloudShellMesh(ent_name + "_mesh",
                               extent_m = PLANE_LOCAL_M,
                               grid     = 24,
                               drop     = DROP_FRAC * spec["alt_m"] / spec["base_scale"],
                               material = mtl)
      sgc = self.declare_component("SceneGraphComponent")
      sgc.sub_calls.append(("declareNodeOnLayer", (), {
          "name":                ent_name + "_node",
          "drawable":            plane.built,
          "layer":               os.environ.get("ORK_CLOUDGAUGE_NODELAYER", "std_transparent"),
          "drawable_asset_name": ent_name + "_mesh",
          "skip_auto_dpp":       True}))
      # the cookie node (W11-S1 cloud ground-shadows, 7529b91d3)
      sgc.sub_calls.append(("declareNodeOnLayer", (), {
          "name":                ent_name + "_cookienode",
          "drawable":            plane.built,
          "layer":               DECK_COOKIE_LAYER,
          "drawable_asset_name": ent_name + "_mesh",
          "skip_auto_dpp":       True}))
      vis = CGI.layer_visible(CGI.plane_key(ent_name), st["res"], st["mode"])
      y   = CGI.layer_y(spec, st["thresh"]) if vis else CGI.HIDE_Y
      s   = spec["base_scale"] * st["tile"] if vis else CGI.HIDE_SCALE
      self.entity(ent_name,
                  transform  = Transform(translation=vec3(0.0, y, 0.0), scale=s),
                  components = [sgc])

  def _forest_library(self):
    self.cloud_decks(alt_offset_m = FOREST_CLOUD_BASE_M,
                     sag_datum_m  = FOREST_TERRAIN_MEAN_M,
                     sun_dir      = FOREST_SUN_DIR,
                     colors       = {k: _fgain(v)
                                     for k, v in FOREST_BASE_COLORS.items()})

  def _forest_inline(self):
    """scn_forest's deck loop, verbatim as of ea485dbbf (the S1-era
    hand-rolled reference this migration must reproduce), plus the ruled
    additions recorded in the section header."""
    A  = self.asset
    st = CGI.env_state()
    for ent_name, lkey, texkey in _PLANES:
      spec  = CGI.LAYERS[lkey]
      look  = _LAYER_LOOK[lkey]
      alt_m = spec["alt_m"] + FOREST_CLOUD_BASE_M
      mtl = A.Ptex3d(ent_name + "_mtl",
                     dsl_class    = CloudLayerMtl,
                     tex_res      = 2048.0 if texkey == "cumulus2k" else 1024.0,
                     tile_base_m  = spec["tile_base_m"],
                     mesh_scale   = spec["base_scale"],
                     alt_m        = alt_m,
                     sweep_m      = spec["sweep_m"],
                     soft         = look["soft"],
                     erode        = look["erode"],
                     opacity_max  = look["opacity_max"],
                     alpha_sigma  = look["alpha_sigma"],
                     alpha_floor  = look["alpha_floor"],
                     ramp_mix     = look["ramp_mix"],
                     dens_pow     = look["dens_pow"],
                     blur_tx      = look["blur_tx"],
                     blur_px      = look["blur_px"],
                     beer_sigma   = look["beer_sigma"],
                     silver_gain  = look["silver_gain"],
                     lit_color    = _fgain(FOREST_BASE_COLORS["lit_color"]),
                     shadow_color = _fgain(FOREST_BASE_COLORS["shadow_color"]),
                     overcast_color = _fgain(FOREST_BASE_COLORS["overcast_color"]),
                     sun_dir      = FOREST_SUN_DIR,
                     wind_mps     = (spec["wind_dir"][0] * spec["wind_mps"] * CGI.WIND_MULT,
                                     spec["wind_dir"][1] * spec["wind_mps"] * CGI.WIND_MULT),
                     evo_uv       = (look["evo_uv"][0] * EVO_MULT,
                                     look["evo_uv"][1] * EVO_MULT),
                     cov_scale    = look["cov_scale"],
                     veil_max     = look.get("veil_max", 0.0),
                     night_ambient     = DECK_NIGHT_AMBIENT, # W15-S1, 43909cc30
                     sampler_textures = {"CloudTex": CLOUD_TEX[texkey]})
      plane = A.CloudShellMesh(ent_name + "_mesh",
                               extent_m = PLANE_LOCAL_M,
                               grid     = 24,
                               drop     = DROP_FRAC * (alt_m - FOREST_TERRAIN_MEAN_M) / spec["base_scale"],
                               material = mtl)
      sgc = self.declare_component("SceneGraphComponent")
      sgc.sub_calls.append(("declareNodeOnLayer", (), {
          "name":                ent_name + "_node",
          "drawable":            plane.built,
          "layer":               "std_transparent",
          "drawable_asset_name": ent_name + "_mesh",
          "skip_auto_dpp":       True}))
      # the cookie node (W11-S1 cloud ground-shadows, 7529b91d3)
      sgc.sub_calls.append(("declareNodeOnLayer", (), {
          "name":                ent_name + "_cookienode",
          "drawable":            plane.built,
          "layer":               DECK_COOKIE_LAYER,
          "drawable_asset_name": ent_name + "_mesh",
          "skip_auto_dpp":       True}))
      vis = CGI.layer_visible(CGI.plane_key(ent_name), st["res"], st["mode"])
      y   = (CGI.layer_y(spec, st["thresh"]) + FOREST_CLOUD_BASE_M) if vis else CGI.HIDE_Y
      s   = spec["base_scale"] * st["tile"] if vis else CGI.HIDE_SCALE
      self.entity(ent_name,
                  transform  = Transform(translation=vec3(0.0, y, 0.0), scale=s),
                  components = [sgc])


def test_cloud_decks():
  return report("cloud decks",
                deck_print(_DeckHarness("inline")),
                deck_print(_DeckHarness("library")))


def test_cover_knob():
  """cloud_cover is the sky-cover fraction; the deck bus encodes 1-cover as the
  threshold altitude (layer_y). Proves the knob reaches the transforms."""
  h = _DeckHarness.__new__(_DeckHarness)
  Scene.__init__(h, default_sg=False)
  h.log = []
  h.asset = _Recorder(h.log)
  h.cloud_decks(cover=0.90)
  spec = CGI.LAYERS["cumulus"]
  want = CGI.layer_y(spec, 1.0 - 0.90)
  got  = h._spawners["cloud_cumulus_2k"].transform.translation.y
  ok   = abs(got - want) < 1e-3
  print("[cover knob] %s (cover 0.90 -> cumulus y %.1f, want %.1f)" % (
      "OK" if ok else "BAD", got, want))
  return ok


def test_empty_sky_is_empty():
  """EVERY procedural sky carries the decks, and they start EMPTY (owner aug08).

  The whole feature rests on one conversion: a cover of 0 must land on the
  FULLY-CLEAR stop, not on threshold 1.0 where the deck still sits inside its
  sweep band and the material's soft/erode tail leaves a visible veil. And an
  empty sky must PARK its planes rather than draw four transparent shells.

  Also pinned: a nonzero cover keeps the exact mapping it always had, so no
  scene that authored a cover moves; and the launch state reaches the
  scenegraph params, which is where a host reads the rows' baseline from."""
  ok = True
  # 1 the conversion's two ends
  ok &= abs(CGI.cover_to_thresh(0.0) - CGI.THRESH_MAX) < 1e-9
  ok &= CGI.decks_clear(CGI.cover_to_thresh(0.0))
  # 2 an authored cover is untouched by the clear-end special case
  for cover in (0.10, 0.45, 0.62, 1.0):
    ok &= abs(CGI.cover_to_thresh(cover) - (1.0 - cover)) < 1e-9
    ok &= not CGI.decks_clear(CGI.cover_to_thresh(cover))
  # 3 cover 0 parks every plane, and the launch state is published
  h = _DeckHarness.__new__(_DeckHarness)
  Scene.__init__(h, default_sg=False)
  h.log   = []
  h.asset = _Recorder(h.log)
  h.scenegraph()          # the params the launch state is published onto
  h.cloud_decks(cover=0.0)
  parked = all(abs(h._spawners[n].transform.translation.y - CGI.HIDE_Y) < 1e-3
               for n in CGI.PLANE_ENTITIES)
  ok &= parked
  published = {}
  for call, args, _kw in h._systems["SceneGraphSystem"].sub_calls:
    if call == "declareParams" and args and isinstance(args[0], dict):
      published.update(args[0])
  ok &= abs(published.get("CloudCover", -1.0)) < 1e-9
  ok &= "CloudTile" in published and "CloudAltOffset" in published
  print("[empty sky] %s (cover 0 -> thresh %.3f, planes parked %s, CloudCover %s)" % (
      "OK" if ok else "BAD", CGI.cover_to_thresh(0.0), parked,
      published.get("CloudCover")))
  return bool(ok)


def test_authored_decks_win():
  """A SCENE THAT DECLARES DECKS IS THE DECK AUTHOR (owner defect, aug08).

  Every procedural sky carries the implied deck set, and scn_forest/scn_swest
  also call cloud_decks() themselves — two sets on the same asset names, which
  is what the duplicate-asset guard fired on at scene construction. The implied
  set is now STAGED and only lands at Scene.build() if nobody claimed the decks,
  so an authoring scene gets EXACTLY its own decks, no matter which side of its
  sky() call they are declared on. The guard itself must keep firing for a scene
  that declares decks twice — only the implied set yields."""

  def compose(author):
    h = _DeckHarness.__new__(_DeckHarness)
    Scene.__init__(h, default_sg=False)
    h.log   = []
    h.asset = _Recorder(h.log)
    h.scenegraph()
    author(h)
    h.declare_staged_cloud_decks()   # what Scene.build() runs at the Pass-2 head
    return h

  def cover_of(h):
    published = {}
    for call, args, _kw in h._systems["SceneGraphSystem"].sub_calls:
      if call == "declareParams" and args and isinstance(args[0], dict):
        published.update(args[0])
    return published.get("CloudCover")

  ONE = [("cloud_cumulus_2k", "cumulus", "cumulus2k")]
  ok  = True

  # 1 no cloud_decks() call anywhere: the implied set, empty
  implied = compose(lambda h: h.sky(celestial=False))
  ok &= sorted(implied._spawners) == sorted(CGI.PLANE_ENTITIES)
  ok &= abs(cover_of(implied)) < 1e-9

  # 2 authored decks win, declared AFTER the sky and BEFORE it
  for label, author in (
      ("after",  lambda h: (h.sky(celestial=False),
                            h.cloud_decks(specs=ONE, cover=0.35))),
      ("before", lambda h: (h.cloud_decks(specs=ONE, cover=0.35),
                            h.sky(celestial=False)))):
    h = compose(author)
    authored = (list(h._spawners) == ["cloud_cumulus_2k"]
                and abs(cover_of(h) - 0.35) < 1e-6)
    ok &= authored
    if not authored:
      print("  [authored decks] BAD (%s: decks %s, cover %s)" % (
          label, list(h._spawners), cover_of(h)))

  # 3 the sky's own cloud knobs are authoring too: saying it twice still raises
  for author in (lambda h: (h.cloud_decks(cover=0.3), h.cloud_decks(cover=0.6)),
                 lambda h: (h.sky(celestial=False, cloud_cover=0.3),
                            h.cloud_decks(cover=0.6))):
    try:
      compose(author)
      ok = False
      print("  [authored decks] BAD (two authored deck sets did not raise)")
    except ValueError as err:
      ok &= "already declared" in str(err)

  print("[authored decks] %s (implied %d decks at cover %s; authored set wins "
        "either side of sky(); double declaration raises)" % (
        "OK" if ok else "BAD", len(implied._spawners), cover_of(implied)))
  return bool(ok)


###############################################################################
# 4 — the EPHEMERIS at a polar site (the property, not the scene)
#
# Written for the retired scn_sun_test cascade gauge, which stood at latitude
# 89.9 because there the sun's elevation IS the solar declination: constant
# elevation, full uniform azimuth sweep. The gauge is gone; the property is a
# check on _celestial.py itself and outlives it.
###############################################################################
# 4b — THE ONE CLOUD-TRANSMITTANCE TERM (master ruling, jul28)
#
# One Beer-Lambert transmittance serves five consumers (captured IBL, sun disc,
# moon, stars via occlusion, direct light + ground shadows), so the deck's three
# sites (alpha / body shading / silver lining) and its SUN/SKY OCCLUSION alpha
# must all read it from
# ptex3d.functions.beer_transmittance instead of each spelling exp(-core*sigma)
# inline — which is how it got triplicated in the first place (P.func does not
# dedup). Three teeth:
#
#   routing   — swapping the shared function changes the generated surface body
#               at exactly FOUR places (3 look + the occlusion alpha), and NO
#               exp(-) survives outside them. RULED (haze S9): the deck's own
#               atmospheric-haze exp(-) is GONE — the fade is the engine's
#               aerial-perspective seam now, so the leg counts ONE
#               skyAerialPerspective() call instead of one stray exp(-). Both
#               forms say the same thing: exactly one distance-haze site, and it
#               is not a cloud-transmittance term.
#   inertness — the shipped body is BYTE IDENTICAL to the body the pre-extraction
#               inline spelling produced. This is the extraction's digest proof
#               kept live: the refactor was textually inert and must stay so. A
#               DELIBERATE change to the extinction model updates the shared
#               function AND the recorded inline form below, together.
#   source    — no inline Beer expression on `core` survives in the deck module
#               (re-triplication is the regression this whole check exists for).
###############################################################################

_N_SITES = 4          # 3 look terms (alpha/body/silver) + the occlusion alpha.
                      # The look three call beer_transmittance DIRECTLY; the
                      # occlusion alpha reaches the same term THROUGH
                      # fractional_occlusion (the coverage-composition law), so
                      # the routing tooth marks both access paths.
_INLINE_HISTORICAL = 'P.func("exp(-{0})", [core * sigma], rtype="float")'

# The ONE fade the visible opacity may carry that the occlusion must NOT: the
# opacity_max LOOK CAP. It is a ceiling on how bright the deck draws, not a
# statement that there is less deck — the whole point of splitting radiance from
# transmittance. Every OTHER factor must appear on both sides.
_LOOK_CAP_ONLY = {"CgOpacMax"}


def _leaves(node, acc=None):
  """param / atom / sampler leaf NAMES reachable from a SurfNode (structure, not text)."""
  from ork.hypergraph.ptex3d.dsl import Param, CtxRef, TexSample, TexArraySample, Swizzle, Op, Const
  acc = set() if acc is None else acc
  if isinstance(node, Param):
    acc.add(node._pname)
  elif isinstance(node, CtxRef):
    acc.add(node._glsl)
  elif isinstance(node, (TexSample, TexArraySample)):
    acc.add("tex:" + node._sname)
    _leaves(node._uv, acc)
  elif isinstance(node, Swizzle):
    _leaves(node._src, acc)
  elif isinstance(node, Op):
    for a in node._args:
      _leaves(a, acc)
  return acc


def _contains_node(root, needle):
  """True if `needle` (identity) appears anywhere in root's subtree."""
  from ork.hypergraph.ptex3d.dsl import TexSample, TexArraySample, Swizzle, Op
  if root is needle:
    return True
  if isinstance(root, Op):
    return any(_contains_node(a, needle) for a in root._args)
  if isinstance(root, Swizzle):
    return _contains_node(root._src, needle)
  if isinstance(root, (TexSample, TexArraySample)):
    return _contains_node(root._uv, needle)
  return False


def _deck_surface_body():
  """the deck material's generated GLSL surface body — pure author phase (the
  same emit_surface() the .fxv2 materializer feeds, no device, no file)."""
  from ork.hypergraph.ptex3d.dsl import SurfaceCtx, emit_surface
  inst = CloudLayerMtl(SurfaceCtx())
  return emit_surface(inst._channels)[0]


def test_one_transmittance_term():
  import ork.hypergraph.ecs.scene._cloud_deck as CD
  from ork.hypergraph.ptex3d import P
  from ork.hypergraph.ptex3d.functions import beer_transmittance

  shipped = _deck_surface_body()

  # tooth 1 — routing: a marked variant must reach exactly the three cloud sites
  def _marked(core, sigma):
    return P.func("exp(-{0}) /*MARK*/", [core * sigma], rtype="float")
  import ork.hypergraph.ptex3d.functions as FN
  orig     = CD.beer_transmittance
  orig_fn  = FN.beer_transmittance
  try:
    CD.beer_transmittance = _marked   # the three direct look sites
    FN.beer_transmittance = _marked   # the occlusion site, via fractional_occlusion
    marked = _deck_surface_body()
  finally:
    CD.beer_transmittance = orig
    FN.beer_transmittance = orig_fn
  n_mark = marked.count("/*MARK*/")
  n_haze = marked.count("exp(-") - n_mark      # no non-Beer exp(-) may remain
  n_seam = marked.count("skyAerialPerspective(")   # the aerial-perspective seam
  routing_ok = (n_mark == _N_SITES) and (n_haze == 0) and (n_seam == 1)

  # tooth 2 — inertness: shared form vs the pre-extraction inline spelling
  def _inline(core, sigma):
    return eval(_INLINE_HISTORICAL, {"P": P, "core": core, "sigma": sigma})
  try:
    CD.beer_transmittance = _inline
    historical = _deck_surface_body()
  finally:
    CD.beer_transmittance = orig
  inert_ok = (shipped == historical)

  # tooth 3 — source: the deck module owns no inline Beer expression on `core`
  src = inspect.getsource(CD)
  src_ok = ('P.func("exp(-{0})", [core *' not in src) and \
           (src.count("beer_transmittance(core,")
            + src.count("fractional_occlusion(presence,") == _N_SITES)

  ok = routing_ok and inert_ok and src_ok and (beer_transmittance is orig)
  print("[one transmittance] %s (sites %d/%d, stray exp(- %d/0, haze seam %d/1, "
        "inert %s, source %s)" % (
      "OK" if ok else "BAD", n_mark, _N_SITES, n_haze, n_seam, inert_ok, src_ok),
      flush=True)
  return ok


###############################################################################
# 4c — FADEOUTS GO TRANSPARENT, NOT BLACK (regression gate, jul28)
#
# The owner saw black cloud fringes the day the deck split radiance from
# transmittance. Cause: the fade factors were multiplied INTO the optical depth,
# so occlusion saturated exponentially (1-exp(-presence*d*sigma) is still ~0.7 at
# presence 0.3) while the deck's own radiance faded linearly (presence*omax
# ~ 0.29). Mid-fade the deck stopped glowing but kept blocking — a black fringe.
#
# The contract that fixes it, and that this checks STRUCTURALLY (so it holds for
# every parameter value, not just the ones a render happened to sample):
#
#   transparent — occlusion = presence * (...): presence is a MULTIPLICATIVE
#                 OUTER factor, so presence 0 gives occlusion EXACTLY 0. A fading
#                 edge lets the background through whatever its depth.
#   no-fade-in-depth — the presence node does NOT appear inside the exponential's
#                 argument. That containment IS the regression's signature.
#   swept       — the fade leaves of the visible opacity and of the occlusion are
#                 the SAME SET (bar the opacity_max look cap). A fade added to one
#                 and forgotten on the other fails here, which is the point: fix
#                 the class, not the instance.
#   still-occludes — the depth argument keeps the thickness factors (the G proxy,
#                 its floor, the slab slant), so THICK cloud stays opaque and the
#                 free stars/moon/sky occlusion is not traded away for the fix.
###############################################################################

def test_fadeout_transparent():
  import ork.hypergraph.ecs.scene._cloud_deck as CD
  from ork.hypergraph.ptex3d.dsl import SurfaceCtx, Op

  seen = {}
  orig = CD.fractional_occlusion

  def _spy(presence, depth_proxy, sigma):
    seen["presence"] = presence
    seen["depth"]    = depth_proxy
    seen["sigma"]    = sigma
    return orig(presence, depth_proxy, sigma)

  try:
    CD.fractional_occlusion = _spy
    inst = CD.CloudLayerMtl(SurfaceCtx())
  finally:
    CD.fractional_occlusion = orig

  occ  = inst._channels["opacity"]      # the deck's blend alpha
  emis = inst._channels["emissive"]     # col * opac (premultiplied)

  # transparent: alpha is (presence * occlusion_of_full_cloud)
  transparent_ok = (isinstance(occ, Op) and occ._tmpl == "({0} * {1})"
                    and occ._args[0] is seen.get("presence"))

  # no-fade-in-depth: the regression, stated as a containment
  depth_clean = not _contains_node(seen.get("depth"), seen.get("presence"))

  # swept: same fade leaves on both sides, bar the look cap
  opac = emis._args[1] if (isinstance(emis, Op) and len(emis._args) == 2) else None
  p_leaves = _leaves(seen.get("presence"))
  o_leaves = _leaves(opac) if opac is not None else set()
  swept_ok = (opac is not None) and (o_leaves - p_leaves == _LOOK_CAP_ONLY) \
             and (p_leaves - o_leaves == set())

  # still-occludes: thickness factors survive in the depth argument
  d_leaves = _leaves(seen.get("depth"))
  occludes_ok = ("CgOccFlr" in d_leaves) and ("tex:CloudTex" in d_leaves)

  ok = transparent_ok and depth_clean and swept_ok and occludes_ok
  print("[fadeout transparent] %s (outer-presence %s, depth-clean %s, swept %s%s, thick-occludes %s)"
        % ("OK" if ok else "BAD", transparent_ok, depth_clean, swept_ok,
           "" if swept_ok else " opac-only=%s presence-only=%s" % (sorted(o_leaves - p_leaves),
                                                                  sorted(p_leaves - o_leaves)),
           occludes_ok), flush=True)
  return ok


###############################################################################
# 5 — scn_sun_test on the CELESTIAL clock (the polar cascade gauge)
###############################################################################

# The ruled site + clock. At latitude 89.9 the sun's elevation IS the solar
# declination, so the cascade gauge keeps the property it was built on (constant
# elevation, full azimuth sweep) off the ONE sky mechanism — no orbit sun.
SUN_TEST_SITE = {"latitude_deg": 89.9, "day_of_year": 172.0,
                 "time_of_day": 12.0, "time_scale": 1440.0}

# 24h of clock per 60 wall seconds -> a full azimuth turn takes 60s, the period
# the deleted orbit sun ran at.
SUN_TEST_PERIOD_S = 60.0


###############################################################################

POLAR_SITE = {"latitude_deg": 89.9, "day_of_year": 172.0,
              "time_of_day": 12.0, "time_scale": 1440.0}

# 24h of clock per 60 wall seconds -> a full azimuth turn takes 60 seconds.
POLAR_PERIOD_S = 60.0


def test_polar_sun_geometry():
  """Across one simulated day at the pole the sun holds the solstice declination
  in elevation while its azimuth walks a full uniform-rate turn."""
  from ork.hypergraph.ecs.scene._celestial import CelestialModel

  N     = 24
  model = CelestialModel(**POLAR_SITE)
  snaps = [model.at(POLAR_PERIOD_S * i / N) for i in range(N)]
  els   = [s.sun_elevation_deg for s in snaps]
  azs   = [s.sun_azimuth_deg for s in snaps]
  # forward differences taken mod 360: monotonic azimuth => they are all positive
  # and they sum to exactly one turn.
  deltas = [(azs[(i + 1) % N] - azs[i]) % 360.0 for i in range(N)]

  band  = max(abs(e - 23.44) for e in els)
  cover = sum(deltas)
  ratio = max(deltas) / min(deltas)
  ok = (band < 0.6 and abs(cover - 360.0) < 1e-6 and min(deltas) > 0.0
        and ratio < 1.1)
  print("[polar sun] %s (elevation 23.44 +-%.3f, azimuth coverage %.3f deg, "
        "rate max/min %.4f)" % ("OK" if ok else "BAD", band, cover, ratio),
        flush=True)
  return ok


###############################################################################
# 6 — scn_forest (S2): the amend path + the generalized decks
###############################################################################

class _ForestBase(Scene):
  """ForestScene's scenegraph block (scn_forest.py:84) — the exposure-bearing
  keys only; the debug toggles and the postfx chain ride through both legs
  unchanged. AmbientLight 0.0 is the load-bearing one: the amend RESTATES it, so
  this leg proved the value crossed the library independently of what the
  library defaulted to — before the jul28 flip (default 0.1) and after it."""
  def __init__(self):
    super().__init__()
    self.scenegraph(preset="ForwardPBR", skybox_path=SKYBOX,
                    SkyboxIntensity=1.0, DiffuseIntensity=3.0,
                    SpecularIntensity=1.0, AmbientLight=vec3(0.0),
                    msaa=2, ssaa=0)


class _OldForestProcSky(_ForestBase):
  """the hand-rolled amend, verbatim as of ea485dbbf."""
  def __init__(self):
    super().__init__()
    self.SG._decl.sub_calls.append(("declareParams", ({
        "SkySource":        "procedural",
        "DiffuseIntensity": FOREST_DIFF_INTENSITY,
    },), {}))


class _NewForestProcSky(_ForestBase):
  def __init__(self):
    super().__init__()
    self.sky_dome(diffuse_intensity = FOREST_DIFF_INTENSITY,
                  ambient_light     = vec3(0.0))


_FOREST_SCENE = os.path.join(_WS, "ork.data/scenes/scn_forest.py")

# the machinery the migration removed / the calls it put in its place. Ties the
# reconstructed legs above to the file that actually ships (this gate stays a
# pure author-phase check, so it does not import ForestScene's whole world).
_FOREST_GONE = ['sub_calls.append(("declareParams"', "A.Ptex3d(",
                "A.CloudShellMesh(", "for ent_name, lkey, texkey in _PLANES"]
# self.sky() rather than self.sky_dome(): S2b routed the dome call up into the
# one-call sky (which forwards to sky_dome) to pick up the celestial ensemble.
_FOREST_WANT = ["self.sky(", "self.cloud_decks("]


def test_forest_procsky():
  ok = report("forest dome", sg_effective(_OldForestProcSky()),
              sg_effective(_NewForestProcSky()))
  ok &= report("forest decks",
               deck_print(_DeckHarness("forest_inline")),
               deck_print(_DeckHarness("forest_library")))
  src     = open(_FOREST_SCENE).read()
  present = [t for t in _FOREST_GONE if t in src]
  absent  = [t for t in _FOREST_WANT if t not in src]
  src_ok  = (not present) and (not absent)
  print("[forest source] %s%s" % (
      "MIGRATED" if src_ok else "STALE",
      "" if src_ok else " (still has %s; missing %s)" % (present, absent)),
      flush=True)
  return ok and src_ok


###############################################################################
# 7 — the DISPLAY EXPOSURE stage: WHO gets it, and does the declaration carry
#     both halves (the ACES node in the postfx chain + the per-tick drive)?
#
# The default-surface check below records the DECISION; this one asserts the
# RESOLUTION, because the knob is auto (None = on for the procedural-sky family
# with a celestial ensemble) and an auto knob's table entry proves nothing about
# what a scene actually ends up with.
###############################################################################

from ork.hypergraph.ecs.scene._sky_dome import ACES_FX_NAME


def _fx_names(scene):
  decl = scene._systems["SceneGraphSystem"]
  return [a[0] for c, a, _ in decl.sub_calls if c == "addPostFxNode"], \
         [a[0] for c, a, _ in decl.sub_calls if c == "appendPostFxOrder"]


class _SkyHarness(Scene):
  """a whole Scene.sky() call with the DEVICE-touching half stubbed: the
  ensemble's star dome builds a mesh + material, which needs a GPU, so the asset
  factory is the recorder (_build_sun_test's trick). The declaration this check
  reads is pure author phase."""
  def __init__(self, **sky_kwargs):
    super().__init__()
    self.asset = _Recorder([])
    self.sky(skybox_path=SKYBOX, **sky_kwargs)


class _AmendedDome(_Base):
  """the AMEND path (a base class owns the scenegraph): postfx is not a
  declareParams key, so the stage has to arrive as its own sub_calls."""
  def __init__(self):
    super().__init__()
    self.sky_dome(tonemap=True)


def test_tonemap_wiring():
  bad = []

  on = _SkyHarness()
  nodes, order = _fx_names(on)
  if ACES_FX_NAME not in nodes or ACES_FX_NAME not in order:
    bad.append("procedural: no %r stage declared (nodes=%s order=%s)"
               % (ACES_FX_NAME, nodes, order))
  if order and order[-1] != ACES_FX_NAME:
    bad.append("the tonemap must be LAST in the chain, got %s" % order)

  # baked sky = lit by a constant, no night to adapt to. It comes out bare.
  # No CELESTIAL requirement any more: the adaptation is a function of the
  # environment the sky publishes, so a static procedural sky is served too.
  baked = _SkyHarness(sky_source=None)
  if ACES_FX_NAME in _fx_names(baked)[0]:
    bad.append("baked: stage attached anyway")
  nocel = _SkyHarness(celestial=False)
  if ACES_FX_NAME not in _fx_names(nocel)[0]:
    bad.append("procedural without an ensemble: stage NOT attached (it measures "
               "the sky, it does not read a sun elevation)")

  # explicit opt-out on the family that would otherwise get it
  off = _SkyHarness(tonemap=False)
  if ACES_FX_NAME in _fx_names(off)[0]:
    bad.append("tonemap=False did not opt out")

  nodes, order = _fx_names(_AmendedDome())
  if ACES_FX_NAME not in nodes or ACES_FX_NAME not in order:
    bad.append("amend path: stage did not reach the existing decl (nodes=%s order=%s)"
               % (nodes, order))

  print("[tonemap] %s (auto ON for the procedural-sky family, OFF for baked; "
        "chain tail %s)" % ("OK" if not bad else "BAD", order), flush=True)
  for b in bad:
    print("  " + b, flush=True)
  return not bad


###############################################################################
# 8 — the DEFAULTED-PARAMETER SURFACE (standing requirement, jul28)
#
# Everything above compares what each migrated scene SAYS. This compares what it
# now SILENTLY INHERITS. For every kwarg the library supplies a default for, a
# migrated scene must either
#   RESTATE it — the name appears in the scene's own call (so the declaration
#                fingerprints above already compare the value), or
#   MATCH    — the library's effective default equals what that scene
#              effectively had BEFORE migration. Where the old scene never
#              expressed the parameter in any form, that comparison is
#              old-engine-default vs new-library-default and must still be
#              EQUAL, or carry a named waiver.
# A library kwarg with no entry in the tables below FAILS: the signatures are
# introspected, so adding a defaulted kwarg to the library without deciding what
# it means for each migrated scene is a red gate — silent inheritance is the
# exact failure mode this check exists for (an ambient_light default of 0.1
# landing on a scene that had 0.0 is what prompted it).
#
# Old-side values are the pre-migration sources: a23162d21 for the three S1
# scenes, ea485dbbf for scn_forest. Where a value moved by RULING rather
# than by migration, the recorded old-side value is the RULED one and the reason
# names the ruling — the point is that every silently-inherited value has a
# recorded decision behind it, not that nothing may ever change.
###############################################################################

import ast
import glob
import inspect

from ork.hypergraph.ecs.scene._sky import SkyMixin
from ork.hypergraph.ecs.scene._sky_dome import SkyDomeMixin
from ork.hypergraph.ecs.scene._cloud_deck import CloudDeckMixin, CloudLayerMtl
from ork.hypergraph.ecs.scene._celestial_sky import CelestialSkyMixin

_ENTRY = {"sky":           SkyMixin.sky,
          "sky_dome":      SkyDomeMixin.sky_dome,
          "cloud_decks":   CloudDeckMixin.cloud_decks,
          "celestial_sky": CelestialSkyMixin.celestial_sky}

# marker objects for the two non-value verdicts
WAIVE   = ("WAIVE",)
UNTOUCH = "<scenegraph decl left as the base declared it>"


def _defaults(entry):
  """the DEFAULTED kwargs of a library entry point (introspected, so the tables
  below must grow whenever the library does)."""
  sig = inspect.signature(_ENTRY[entry])
  return {n: p.default for n, p in sig.parameters.items()
          if p.default is not inspect.Parameter.empty}


def _mtl_default(name):
  return inspect.signature(CloudLayerMtl.__init__).parameters[name].default


def _sun_default(name):
  """Scene.sun()'s own default for a light property the ensemble can now pass
  through (sun_color / sun_intensity, S2b) — what every ensemble got before the
  passthrough existed, and what None still resolves to."""
  from ork.hypergraph.ecs.scene._sun import SunMixin
  return inspect.signature(SunMixin.sun).parameters[name].default


def _mtl_colors():
  return {k: _mtl_default(k) for k in ("lit_color", "shadow_color",
                                       "overcast_color")}


def _resolve(entry, name, raw):
  """what the library ACTUALLY uses when a kwarg is left unspecified. The None
  sentinels resolve at call time (env state, the module tables, the material's
  own constructor defaults), so the surface has to be compared POST-resolution —
  comparing the literal None would prove nothing."""
  if raw is None:
    if (entry, name) == ("sky_dome", "skybox_path"):
      return UNTOUCH
    if entry in ("sky", "celestial_sky"):
      if name == "sun_color":     return _sun_default("color")
      if name == "sun_intensity": return _sun_default("intensity")
    if entry == "cloud_decks":
      if name == "specs":   return _PLANES
      if name == "looks":   return _LAYER_LOOK
      if name == "layer":
        return os.environ.get("ORK_CLOUDGAUGE_NODELAYER", "std_transparent")
      if name == "cover":   return ("env-thresh", CGI.env_state()["thresh"])
      if name == "tile":    return ("env-tile", CGI.env_state()["tile"])
      if name == "colors":  return _mtl_colors()
      if name == "sun_dir": return _mtl_default("sun_dir")
  return raw


# ambient_light takes a float OR a vec3; sky_dome broadcasts the float, so both
# sides are compared as the triple the engine ends up with.
_BROADCAST = {("sky_dome", "ambient_light")}


def _norm(key, v):
  if hasattr(v, "x") and hasattr(v, "y") and hasattr(v, "z"):
    return (round(v.x, 6), round(v.y, 6), round(v.z, 6))
  if isinstance(v, dict):
    return tuple(sorted((k, _norm(key, x)) for k, x in v.items()))
  if isinstance(v, (list, tuple)):
    return tuple(_norm(key, x) for x in v)
  if isinstance(v, bool) or v is None or isinstance(v, str):
    return v
  if isinstance(v, (int, float)):
    # 6 dp: a vec3 stores float32, so vec3(0.1).x reads back 0.100000001 and a
    # float-vs-vec3 spelling of the same value must not read as a mismatch.
    return round(float(v), 6)
  return v


def _cmp(key, v):
  """comparison form: broadcast the scalar spellings ONCE, at the top level,
  then normalize."""
  if key in _BROADCAST and isinstance(v, (int, float)) and not isinstance(v, bool):
    v = (float(v),) * 3
  return _norm(key, v)


def _call_names(path):
  """the library entry points a shipped scene calls, and every kwarg NAME it
  passes them (cloud_params' keys count as restatements of the deck kwargs they
  forward to). Read off the FILE so the check tracks what ships."""
  tree = ast.parse(open(path).read())
  entries, names = set(), set()
  for node in ast.walk(tree):
    if not isinstance(node, ast.Call):
      continue
    f = node.func
    if not (isinstance(f, ast.Attribute) and isinstance(f.value, ast.Name)
            and f.value.id == "self" and f.attr in _ENTRY):
      continue
    entries.add(f.attr)
    for kw in node.keywords:
      if kw.arg is None:
        continue
      names.add(kw.arg)
      if kw.arg == "cloud_params":
        if isinstance(kw.value, ast.Call):
          names.update(k.arg for k in kw.value.keywords if k.arg)
        elif isinstance(kw.value, ast.Dict):
          names.update(k.value for k in kw.value.keys
                       if isinstance(k, ast.Constant))
  return entries, names


###############################################################################
# the pre-migration effective values, per scene. Value = what the OLD scene
# ended up with; WAIVE = a deliberate, ruled change (reason in the note).
###############################################################################

# The jul28 ambient ruling, per scene. AMBIENT IS NOT ONE NUMBER any more: the
# procedural-sky family is lit by sun + IBL alone (0.0, the new library
# default), while the BAKED cascade gauge restates the 0.1 it was tuned under.
# Recorded here so the flip is ASSERTED (a drift back to 0.1 fails the gate),
# not merely tolerated.
_AMBIENT_PROCSKY = (vec3(0.0),
                    "RULED jul28 (owner): the procedural-sky family drops the "
                    "flat ambient term — entirely IBL-based")

# The TONE stage (jul28). No migrated scene had one, and the sky()
# knob is AUTO — so what is recorded here is a waiver that NAMES the check which
# asserts the resolution (section 7). A plain False against an auto knob would
# read as "nothing happens", which is the opposite of true for two of these
# scenes.
_DISPLAY_STAGE_OLD = {
    "tonemap": (WAIVE,
                "RULED jul28, re-sized W4-S1: the tone stage is NEW — auto ON "
                "for the procedural-sky family (its adaptation measures the "
                "published sky, so no ensemble is required); resolution "
                "asserted in section 7"),
}

_EXPOSURE_OLD = {                      # the literal block all three S1 scenes
    "skybox_intensity":   (1.0, "old scenegraph SkyboxIntensity"),
    "diffuse_intensity":  (1.0, "old scenegraph DiffuseIntensity"),
    "specular_intensity": (1.0, "old scenegraph SpecularIntensity"),
    "ambient_light":      (vec3(0.1), "old scenegraph AmbientLight"),
    "preset":             ("ForwardPBR", "old scenegraph preset"),
}

_CELESTIAL_OLD_DEFAULTS = {            # the ensemble the old scenes inherited
    "latitude_deg": (45.0,   "old celestial_sky() default (never expressed)"),
    "day_of_year":  (220.0,  "old celestial_sky() default (never expressed)"),
    # RULED jul28 (owner): the family clock went 1440 -> 480 (a day in 180 wall
    # seconds) so the sky is judgeable. PROVISIONAL — "for now (until we trust
    # the sky rendering is complete)" — and a mitigation, not a fix: the sun
    # slows ~6 -> ~2 deg per wall second, the publish-chain lag defect stands.
    "time_scale":   (480.0, "RULED jul28: the 180-second day, inherited"),
    "moon":         (True,   "old celestial_sky() default (never expressed)"),
}

# The AUTHORED MOON (W12-S1) — four kwargs the pre-migration scenes could not
# have expressed: they did not exist, and every one of them is inert unless
# declared (the moon stays the ephemeris moon of the declared date). So these
# are MATCHES against the library default, not waivers.
_MOON_PLACEMENT_OLD = {
    "year":   (None, "old scenes never named a year (the config default, 2000)"),
    "phase":  (None, "no declared phase: the ephemeris moon of the date"),
    "moon_initial_elevation": (None, "no declared placement: same"),
    "moon_orbit_rate":        (1.0,  "Earth's moon, the only rate there was"),
}

# The sun-radiance passthrough (S2b): the ensemble aims one sun, but its
# brightness is a per-scene balance, so sky()/celestial_sky() now forward color
# and intensity. None resolves to Scene.sun()'s own defaults — which is exactly
# what every ensemble scene had before the kwargs existed, so these are MATCHES,
# not waivers.
_ENSEMBLE_RADIANCE_OLD = {
    "sun_color":     (_sun_default("color"),
                      "the ensemble's sun took Scene.sun()'s color"),
    "sun_intensity": (_sun_default("intensity"),
                      "the ensemble's sun took Scene.sun()'s intensity"),
    "sun_params":    (None,
                      "the ensemble's sun took Scene.sun()'s remaining defaults "
                      "(the dict is the long-tail passthrough, S2c)"),
}

_NO_DECKS = {
    "clouds":      (False, "old scene declared no cloud decks"),
    "cloud_cover": (None,  "old scene declared no cloud decks"),
    "cloud_params": (None, "old scene declared no cloud decks"),
}

# The DECLARED-CADENCE knobs: the IBL four (procsky cadence sliceA, merged
# 856051205 jul30) and the shadow one (sliceB, merged b24da51a7). Every one of
# them defaults None, and None declares NOTHING — no SkyAtmosphere object is
# published and no cadence key reaches the ensemble's sun — so a scene that
# stays silent keeps exactly the engine behavior it had before the knobs
# existed. That inertness is what these entries ASSERT: a future default that
# starts declaring a cadence for every silent scene fails here, which is the
# whole point of the check.
_CADENCE_OLD = {
    "ibl_snapshot_interval":    (None, "cadence sliceA: undeclared leaves the "
                                       "engine's sun-motion rebake trigger alone"),
    "ibl_level_batches":        (None, "cadence sliceA granularity: undeclared "
                                       "leaves the ORKID_MT_IBL_* env / engine "
                                       "defaults in charge"),
    "ibl_slices_per_frame":     (None, "cadence sliceA granularity, as above"),
    "ibl_mipchain_budget_px":   (None, "cadence sliceA granularity, as above"),
    "shadow_snapshot_interval": (None, "cadence sliceB: undeclared = cascade "
                                       "snapshots every frame, as before"),
}

# The DISTANCE-HAZE knob (aerial perspective, S6). One kwarg carrying a whole
# authoring surface (preset name or knob dict), and its default is INERT the same
# way the cadence knobs are: undeclared resolves to no artist layer, no
# SkyAtmosphere object published, and the engine's own zero artist density —
# which is the pre-haze renderer exactly. So every scene silent about haze still
# renders what it rendered, and a future default that starts hazing every silent
# scene fails here.
_HAZE_OLD = {
    "haze": (None, "haze slice: undeclared declares NOTHING — no haze knobs "
                   "written, no atmosphere published, and the engine's zero "
                   "artist density IS the pre-haze renderer"),
}

# The COOKIE NODE knob (W11-S1 cloud ground-shadows, 7529b91d3). Default-armed:
# every deck declares its second node, and the whole path stays inert until a
# sun declares cloud_shadow_strength > 0 — so the arming is free, and recording
# the role name here is what keeps it from being renamed out from under the
# forward node that looks it up.
_DECK_COOKIE_OLD = {
    "cookie_layer": (DECK_COOKIE_LAYER,
                     "W11-S1 cloud ground-shadows: the second node per deck, "
                     "on the layer role the forward node fills the cookie from"),
}

_DECKS_OLD = {                         # both inline deck loops, pre-migration
    "specs": (_PLANES,      "old loop iterated _PLANES"),
    "looks": (_LAYER_LOOK,  "old loop indexed _LAYER_LOOK"),
    "cover": (("env-thresh", None), "old loop used env_state()['thresh']"),
    "tile":  (("env-tile",  None), "old loop used env_state()['tile']"),
}

_SURFACE = {
  "scn_procsky": {
    "path":    os.path.join(_WS, "ork.data/scenes/scn_procsky.py"),
    "entries": ("sky", "sky_dome"),
    "old": dict(_EXPOSURE_OLD, **_CELESTIAL_OLD_DEFAULTS, **_NO_DECKS,
                **_DISPLAY_STAGE_OLD, **_ENSEMBLE_RADIANCE_OLD, **_CADENCE_OLD,
                **_HAZE_OLD, **_MOON_PLACEMENT_OLD, **{
        "ambient_light": _AMBIENT_PROCSKY,
        "sky_source":  ("procedural", "old scenegraph SkySource"),
        "time_of_day": (4.0,  "old celestial_sky() default (never expressed)"),
        "celestial":   (True, "old scene called celestial_sky()"),
        "stars":       (True, "old celestial_sky() default (never expressed)"),
    })},

  "scn_swest": {
    "path":    os.path.join(_WS, "ork.data/scenes/scn_swest.py"),
    # the decks arrived LIBRARY-NATIVE (W8-S1, 75be48e24: one sparse high-desert
    # cumulus deck composed entirely from cloud_decks(), no hand-rolled loop
    # ever existed here), so this scene's deck entries are not pre-migration
    # values — they are the library defaults it silently ships on, recorded so
    # they cannot move without a decision.
    "entries": ("sky", "sky_dome", "cloud_decks"),
    "old": dict(_NO_DECKS, **_DISPLAY_STAGE_OLD, **_CADENCE_OLD, **_HAZE_OLD,
                **_DECK_COOKIE_OLD, **_MOON_PLACEMENT_OLD, **{
        # RULED (owner ask, jul28): "a sky with sunset and moon visible" — a
        # baked envmap has neither, so the dome went procedural and the static
        # sun became the ensemble. Both are POSE/LOOK changes, declared.
        "sky_source":  (WAIVE, "ruled: baked -> procedural, the owner's ask"),
        "celestial":   (WAIVE, "ruled conversion: old scene hand-posed a static "
                               "sun, no ensemble — section 10 is the coverage"),
        "preset":      ("ForwardPBR", "old scenegraph preset"),
        # inert since the scene started RESTATING its clock (section 10 carries
        # that decision); kept so it still asserts if the restatement is dropped
        "time_scale":  (480.0, "RULED jul28: the family clock, inherited"),
        # the deck knobs W8-S1 left at their library values
        "looks":       (_LAYER_LOOK, "W8-S1 declared specs but not looks: the "
                                     "shipped per-layer look table"),
        "sun_dir":     (_mtl_default("sun_dir"),
                        "W8-S1 weather-data-only: the decks follow the LIVE "
                        "celestial sun, and this FALLBACK vector is read only "
                        "by a scene that declares no directional light"),
        "layer":       (os.environ.get("ORK_CLOUDGAUGE_NODELAYER", "std_transparent"),
                        "the shared deck-layer knob at its shipped default"),
        # the sky()-side cloud knobs stay unused: the decks have their own call
        "clouds":      (False, "the scene declares its deck with its own "
                               "cloud_decks() call, not through sky()"),
        "cloud_cover": (None,  "same: the deck call carries the cover knob"),
        "cloud_params": (None, "same: the deck call carries the launch state"),
    })},

  "scn_forest": {
    "path":    os.path.join(_WS, "ork.data/scenes/scn_forest.py"),
    # S2b routed the dome through sky() (which brought the ensemble with it); the
    # decks keep their own call, so sky()'s cloud knobs stay unused here.
    "entries": ("sky", "sky_dome", "cloud_decks"),
    "old": dict(_DECKS_OLD, **_DISPLAY_STAGE_OLD, **_CADENCE_OLD, **_HAZE_OLD,
                **_DECK_COOKIE_OLD, **_MOON_PLACEMENT_OLD, **{
        "sky_source":  ("procedural", "old amend set SkySource procedural"),
        "celestial":   (WAIVE, "ruled conversion S2b: old scene hand-posed its "
                               "own sun, no ensemble — section 9 is the coverage"),
        # inert since the owner re-declared this scene's clock (section 10
        # carries that decision); kept so it still asserts if 25.0 is dropped
        "time_scale":  (480.0, "RULED jul28: the family clock is the library's "
                               "180-second day; the per-scene 1440 was stripped"),
        "sun_params":  (None,  "the forest states the sun's radiance through the "
                               "two shortcuts; nothing else of its sun is tuned"),
        "clouds":      (False, "the scene declares its decks with its own "
                               "cloud_decks() call, not through sky()"),
        "cloud_cover": (None,  "same: the deck call carries the launch state"),
        "cloud_params": (None, "same: the deck call carries the launch state"),
        "skybox_path": (UNTOUCH, "old amend left ForestScene's skybox_path alone"),
        "skybox_intensity":   (1.0, "ForestScene's declared SkyboxIntensity"),
        "specular_intensity": (1.0, "ForestScene's declared SpecularIntensity"),
        # not consulted on the amend path (the scenegraph already exists) — and
        # equal to ForestScene's preset anyway, so it is asserted, not waived.
        "preset":      ("ForwardPBR", "ForestScene's declared preset"),
        # WIDENING FOUND BY THIS CHECK (S2, reported): the old loop hard-coded
        # "std_transparent"; the library resolves the shared deck knob. Equal at
        # the shipped default, which is what is asserted here.
        "layer":        (os.environ.get("ORK_CLOUDGAUGE_NODELAYER", "std_transparent"),
                         "old loop hard-coded std_transparent (library now reads the knob)"),
    })},
}


def test_default_surface():
  bad, stats = [], []

  # routing consistency: sky() re-declares celestial_sky()'s site/clock kwargs,
  # so a drift between the two default sets would silently move every sky()
  # scene's observer off the one celestial_sky() scenes get.
  sky_d, cel_d = _defaults("sky"), _defaults("celestial_sky")
  drift = [k for k, v in cel_d.items() if k in sky_d and sky_d[k] != v]
  if drift:
    bad.append("sky() defaults drift from celestial_sky(): %s" % drift)

  for scene, tbl in sorted(_SURFACE.items()):
    entries, restated = _call_names(tbl["path"])
    stray = entries - set(tbl["entries"])
    if stray:
      bad.append("%s calls %s — no default-surface expectations recorded"
                 % (scene, sorted(stray)))
    n_restated = n_matched = n_waived = 0
    for entry in tbl["entries"]:
      for name, raw in sorted(_defaults(entry).items()):
        if name in restated:
          n_restated += 1
          continue
        if name not in tbl["old"]:
          bad.append("%s inherits %s.%s with NO recorded pre-migration value"
                     % (scene, entry, name))
          continue
        old, why = tbl["old"][name]
        if old is WAIVE:
          n_waived += 1
          continue
        lib = _resolve(entry, name, raw)
        # the env-sourced deck knobs: both sides read the same live env state,
        # so the assertion is that the library still SOURCES it there.
        if isinstance(old, tuple) and len(old) == 2 and old[1] is None \
           and isinstance(lib, tuple) and len(lib) == 2:
          old = (old[0], lib[1])
        if _cmp((entry, name), lib) != _cmp((entry, name), old):
          bad.append("%s %s.%s: library default %r, pre-migration %r (%s)"
                     % (scene, entry, name, _cmp((entry, name), lib),
                        _cmp((entry, name), old), why))
        else:
          n_matched += 1
    stats.append("%s %d restated/%d matched/%d waived"
                 % (scene.replace("scn_", ""), n_restated, n_matched, n_waived))

  print("[default surface] %s (%s)" % ("OK" if not bad else "BAD",
                                       "; ".join(stats)), flush=True)
  for b in bad:
    print("  " + b, flush=True)
  return not bad


###############################################################################
# 9 — scn_forest (S2b): the CELESTIAL conversion
#
# The forest's hand-posed sun became a SITE + CLOCK. Three things must hold:
#   * POSE PRESERVATION — the solved (latitude, day_of_year, time_of_day) puts
#     the celestial sun exactly where the hand-posed one was at t=0, so the
#     daylight look is preserved BY CONSTRUCTION rather than by tuning. The
#     reference pair is the pose the scene shipped with (FOREST_SUN_ELEV_DEG /
#     FOREST_SUN_AZIM_DEG at the top of this file), read in the DSL convention
#     Scene.sun(elevation=, azimuth=) consumes — which is why the comparison is
#     against sun_light_angles() and not the astronomical bearing.
#   * ONE SKY — moon and star dome declared, on the sun's site and clock (the
#     sharing, not merely the presence; same method as section 5).
#   * NO SURVIVING HAND-POSE — the file must carry no Scene.sun() call and no
#     orbit, or the scene would declare a second, differently-aimed sun.
#
# The reconstruction leg mirrors section 6's: this gate stays pure author phase,
# so it rebuilds ForestScene's scenegraph (including the hsvg node, which the
# tonemap-order check needs) instead of importing the forest's whole world — and
# reads the SHIPPED module's constants so a drift in either file fails.
###############################################################################

FOREST_POSE_TOL_DEG = 0.5      # stated tolerance, per axis

_FOREST_TOD_ENV = "ORK_FORESTSKY_TOD"


def _forest_module():
  import scn_forest
  return scn_forest


def _forest_sky_call():
  """the shipped self.sky(...) call, as {kwarg: source text} — proves the solved
  constants are the ones that actually reach the library."""
  tree = ast.parse(open(_FOREST_SCENE).read())
  for node in ast.walk(tree):
    f = getattr(node, "func", None)
    if (isinstance(node, ast.Call) and isinstance(f, ast.Attribute)
        and f.attr == "sky" and isinstance(f.value, ast.Name)
        and f.value.id == "self"):
      return {kw.arg: ast.unparse(kw.value) for kw in node.keywords if kw.arg}
  return None


class _ForestHSVGBase(Scene):
  """ForestScene's scenegraph block WITH its post-fx chain — the tonemap-order
  check is about what the ACES stage does to an author's existing grade."""
  def __init__(self):
    from orkengine.lev2 import PostFxNodeHSVG
    super().__init__()
    self.scenegraph(preset="ForwardPBR", skybox_path=SKYBOX,
                    SkyboxIntensity=1.0, DiffuseIntensity=3.0,
                    SpecularIntensity=1.0, AmbientLight=vec3(0.0),
                    msaa=2, ssaa=0, postfx=[("hsvg", PostFxNodeHSVG())])


class _ForestCelestial(_ForestHSVGBase):
  """the shipped sky() call, replayed on that base with the star dome's asset
  factory stubbed (it builds a mesh + material, which needs a GPU)."""
  def __init__(self):
    super().__init__()
    self.asset = _Recorder([])
    M = _forest_module()
    self.sky(latitude_deg  = M.SOLVED_LATITUDE_DEG,
             day_of_year   = M.SOLVED_DAY_OF_YEAR,
             time_of_day   = M.SOLVED_TIME_OF_DAY,
             moon          = True,
             stars         = True,
             sun_color     = vec3(1.0, 0.93, 0.80),
             sun_intensity = M.SUN_INTENSITY,
             diffuse_intensity = M.DIFF_INTENSITY,
             ambient_light     = vec3(0.0))


def test_forest_celestial_pose():
  from ork.hypergraph.ecs.scene._celestial import CelestialModel
  M   = _forest_module()
  bad = []

  # the clock is the LIBRARY's now (no per-scene time_scale) — the pose at t=0
  # does not depend on it, but reading it from the library is what keeps this
  # check honest about which clock the scene actually rides.
  model = CelestialModel(latitude_deg = M.SOLVED_LATITUDE_DEG,
                         day_of_year  = M.SOLVED_DAY_OF_YEAR,
                         time_of_day  = M.SOLVED_TIME_OF_DAY,
                         time_scale   = _defaults("sky")["time_scale"])
  el, az = model.at(0.0).sun_light_angles()
  d_el = el - FOREST_SUN_ELEV_DEG
  d_az = (az - FOREST_SUN_AZIM_DEG + 180.0) % 360.0 - 180.0
  if abs(d_el) > FOREST_POSE_TOL_DEG or abs(d_az) > FOREST_POSE_TOL_DEG:
    bad.append("solved pose off the shipped one by (%+.4f el, %+.4f az) deg, "
               "tolerance %.2f" % (d_el, d_az, FOREST_POSE_TOL_DEG))

  # the constants above are only the pose if the scene passes THEM
  call = _forest_sky_call()
  if call is None:
    bad.append("no self.sky(...) call in the scene")
  else:
    for name, want in (("latitude_deg", "SOLVED_LATITUDE_DEG"),
                       ("day_of_year",  "SOLVED_DAY_OF_YEAR"),
                       ("time_of_day",  "TIME_OF_DAY"),
                       ("moon",         "True"),
                       ("stars",        "True")):
      if call.get(name) != want:
        bad.append("sky(%s=%s), want %s" % (name, call.get(name), want))
  # the time-of-day knob defaults to the solve (env override = the owner's, not
  # a drift, so it is only asserted when the knob is unset).
  if _FOREST_TOD_ENV not in os.environ and M.TIME_OF_DAY != M.SOLVED_TIME_OF_DAY:
    bad.append("TIME_OF_DAY %r is not the solved %r"
               % (M.TIME_OF_DAY, M.SOLVED_TIME_OF_DAY))

  print("[forest pose] %s (lat %.4f doy %.1f tod %.5f -> el %.4f az %.4f; "
        "residual %+.5f el %+.5f az, tol %.2f)" % (
        "OK" if not bad else "BAD", M.SOLVED_LATITUDE_DEG, M.SOLVED_DAY_OF_YEAR,
        M.SOLVED_TIME_OF_DAY, el, az % 360.0, d_el, d_az, FOREST_POSE_TOL_DEG),
        flush=True)
  for b in bad:
    print("  " + b, flush=True)
  return not bad


def test_forest_celestial_ensemble():
  from ork.hypergraph.ecs.scene import _celestial
  import json
  bad = []

  os.environ.pop(_celestial.CONFIG_ENV_KEY, None)
  scene = _ForestCelestial()
  table = json.loads(os.environ.get(_celestial.CONFIG_ENV_KEY, "{}"))
  M     = _forest_module()

  sun = table.get("sun")
  if sun is None:
    bad.append("no celestial config published for the sun")
  else:
    site = {"latitude_deg": M.SOLVED_LATITUDE_DEG,
            "day_of_year":  M.SOLVED_DAY_OF_YEAR,
            "time_of_day":  M.SOLVED_TIME_OF_DAY,
            "time_scale":   _defaults("sky")["time_scale"]}
    for k, want in site.items():
      if sun.get(k) != want:
        bad.append("sun %s = %r, want %r" % (k, sun.get(k), want))
    # the scene's rebalanced sun radiance survives the ensemble (S2b passthrough)
    if sun.get("base_intensity") != M.SUN_INTENSITY:
      bad.append("sun base_intensity %r, want the scene's %r"
                 % (sun.get("base_intensity"), M.SUN_INTENSITY))

  for member in ("moon", "stars"):
    cfg = table.get(member)
    if member not in scene._spawners:
      bad.append("no %s entity declared" % member)
    if cfg is None:
      bad.append("no celestial config published for the %s" % member)
    elif sun is not None:
      drift = [k for k in _celestial.MODEL_CONFIG_KEYS if cfg.get(k) != sun.get(k)]
      if drift:
        bad.append("%s site/clock differs from the sun: %s" % (member, drift))

  # the tone stage lands where the S-B design puts it: AFTER the author's
  # grade, last in the chain (the forest's frame is HSVG-graded, then rolled).
  # There is no per-tick drive to look for any more: the adaptation is a
  # function of the sky the engine publishes, evaluated on the stage itself.
  nodes, order = _fx_names(scene)
  if order[:2] != ["hsvg", ACES_FX_NAME]:
    bad.append("postfx order %s, want the author's grade then the tonemap" % order)

  # no hand-posed sun survives anywhere in the shipped file
  src  = open(_FOREST_SCENE).read()
  tree = ast.parse(src)
  for node in ast.walk(tree):
    f = getattr(node, "func", None)
    if (isinstance(node, ast.Call) and isinstance(f, ast.Attribute)
        and f.attr == "sun" and isinstance(f.value, ast.Name)
        and f.value.id == "self"):
      bad.append("scene still declares its own self.sun(%s)"
                 % ",".join(kw.arg or "**" for kw in node.keywords))
  if "animate_orbit" in src:
    bad.append("scene still declares an orbit sun (animate_orbit)")

  print("[forest ensemble] %s (entities %s; sun intensity %s; chain %s)" % (
      "OK" if not bad else "BAD",
      sorted(k for k in table if k in ("sun", "moon", "stars")),
      sun and sun.get("base_intensity"), order), flush=True)
  for b in bad:
    print("  " + b, flush=True)
  return not bad


###############################################################################
# 10 — THE FAMILY INVARIANT (cross-scene, additive to every per-scene check)
#
# Per-scene fingerprints prove each scene still declares what it declared. They
# structurally CANNOT catch the drift class that produced this round: a family
# where one member has a static sun, another a moon but no stars, a third its
# own clock — each internally consistent, the family incoherent. So this check
# is stated over the SET:
#
#   * every scene in ork.data/scenes that declares a sky declares it through
#     Scene.sky() with the CELESTIAL ENSEMBLE — no hand-posed suns;
#   * every member carries the MOON and the STARS;
#   * every member rides ONE CLOCK — the library's. A scene that restates
#     time_scale is not a bug in that scene, it is a FINDING about the family,
#     so it fails here rather than passing quietly;
#   * the roster is CLOSED: anything else in the scenes directory that declares
#     a sun must be named in _FAMILY_WAIVED with a reason. A new scene with a
#     static sun and no entry is a red gate.
#
# A member may DEPART from any of those only where a merged decision says so,
# and then only on the named property (_FAMILY_PROP_WAIVED) — everything else
# about that scene stays asserted. A whole-scene waiver (_FAMILY_WAIVED) is for
# declarations that are not scenes-under-a-sky at all.
###############################################################################

_SCENES_DIR = os.path.join(_WS, "ork.data/scenes")

# Not scenes-under-a-sky: a declaration that exists to light a BAKE has no day,
# no night and no observer.
_FAMILY_WAIVED = {
    "ren_pueblo_bake": "asset-bake harness (ren_*), not a scene under a sky — "
                       "its light is a bake fixture at a fixed pose",
}

# PER-PROPERTY departures, each with the decision that made it. The clock
# entries are the family's one live finding, ADJUDICATED rather than argued
# away: the owner drives these two scenes by eye at the bench, and the rate a
# sky is judged at is a per-scene tuning knob in the same sense the hour is.
_FAMILY_PROP_WAIVED = {
    "scn_forest": {
        "time_scale": "owner bench tweak 1daba3f9b (absorbed at 1bd21cac4): the "
                      "forest runs a 25x clock — slow enough to watch the light "
                      "move across the alpine terrain",
    },
    "scn_swest": {
        "time_scale": "SWEST_TIMESCALE, declared explicit by W8-S1 (75be48e24) "
                      "at the library 480 and re-cut by the owner at the bench "
                      "twice since (354ee4f22 -> 10, 1daba3f9b -> 100)",
    },
    # a MEASUREMENT RIG that lives in the scenes directory: its departures are
    # the instrument (W14-S1, b0fd91030 — the reasons are stated at length in
    # the scene's own header, and each one is load-bearing for the measurement).
    "scn_nightcal": {
        "moon":       "night display calibration reads the DEAD-OF-NIGHT floor: "
                      "a risen moon is orders of magnitude of extra sky radiance, "
                      "so it has to be gone, not merely low",
        "time_scale": "frozen clock (0.0): the library day runs ~2 deg of sun per "
                      "wall second, so an offscreen boot would measure twilight; "
                      "zero is what makes two runs comparable",
    },
}


def _sky_calls(path):
  """{method: {kwarg: source text}} for every self.sky()/self.sun()/
  self.celestial_sky() call in a scene file."""
  out = {}
  for node in ast.walk(ast.parse(open(path).read())):
    f = getattr(node, "func", None)
    if not (isinstance(node, ast.Call) and isinstance(f, ast.Attribute)
            and isinstance(f.value, ast.Name) and f.value.id == "self"
            and f.attr in ("sky", "sun", "celestial_sky")):
      continue
    out.setdefault(f.attr, {}).update(
        {kw.arg: ast.unparse(kw.value) for kw in node.keywords if kw.arg})
  return out


def test_family_invariant():
  bad, roster = [], []

  sky_defaults = _defaults("sky")
  # the invariant leans on these being the library's answer when a scene stays
  # silent, so they are asserted rather than assumed
  for name, want in (("moon", True), ("stars", True), ("celestial", True)):
    if sky_defaults[name] is not want:
      bad.append("library sky() default %s is %r, not %r — the silence of a "
                 "family member no longer means the ensemble"
                 % (name, sky_defaults[name], want))
  family_clock = sky_defaults["time_scale"]

  for path in sorted(glob.glob(os.path.join(_SCENES_DIR, "*.py"))):
    name  = os.path.basename(path)[:-3]
    calls = _sky_calls(path)
    if not calls:
      continue
    if name in _FAMILY_WAIVED:
      roster.append("%s WAIVED" % name)
      continue
    waived = _FAMILY_PROP_WAIVED.get(name, {})
    if "sun" in calls:
      bad.append("%s declares its own self.sun(%s) — the family aims the sun "
                 "from the site and clock" % (name, ",".join(sorted(calls["sun"]))))
    if "sky" not in calls:
      bad.append("%s declares a sky without Scene.sky()" % name)
      continue
    kw = calls["sky"]
    if kw.get("celestial") == "False" and "celestial" not in waived:
      bad.append("%s opts out of the celestial ensemble" % name)
    for member in ("moon", "stars"):
      if kw.get(member) == "False" and member not in waived:
        bad.append("%s drops the %s" % (name, member))
    if "time_scale" in kw and "time_scale" not in waived:
      bad.append("%s restates time_scale=%s — the family runs ONE clock (the "
                 "library's %s); a scene that needs another is a finding"
                 % (name, kw["time_scale"], family_clock))
    roster.append("%s%s%s" % (name, "" if "celestial" not in kw
                              else " (celestial=%s)" % kw["celestial"],
                              "" if not waived
                              else " [waived: %s]" % ",".join(sorted(waived))))

  print("[family invariant] %s (clock %s; %s)" % (
      "OK" if not bad else "BAD", family_clock, "; ".join(roster)), flush=True)
  for b in bad:
    print("  " + b, flush=True)
  return not bad


###############################################################################

def main():
  tests = [test_dome_params, test_append_path, test_cloud_decks,
           test_cover_knob, test_empty_sky_is_empty, test_authored_decks_win,
           test_one_transmittance_term, test_fadeout_transparent,
           test_polar_sun_geometry,
           test_forest_procsky, test_tonemap_wiring,
           test_default_surface,
           test_forest_celestial_pose, test_forest_celestial_ensemble,
           test_family_invariant]
  failed = []
  for t in tests:
    try:
      if not t():
        failed.append(t.__name__)
    except Exception:
      import traceback
      traceback.print_exc()
      failed.append(t.__name__)
  ok = (len(failed) == 0)
  print("=== sky library parity gate %s (%d/%d) ===" % (
      "PASSED" if ok else "FAILED", len(tests) - len(failed), len(tests)), flush=True)
  if failed:
    print("  failed: " + ", ".join(failed), flush=True)
  sys.exit(0 if ok else 1)


main()
