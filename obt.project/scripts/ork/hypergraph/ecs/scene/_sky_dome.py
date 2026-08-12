###############################################################################
# _sky_dome.py — ONE call for the SKY/IBL scenegraph params every procedural-sky
# scene was hand-rolling: the sky source plus the four exposure knobs
# (SkyboxIntensity / DiffuseIntensity / SpecularIntensity / AmbientLight). The
# literal block was copy-pasted byte-identical across the gauge scenes; this
# mixin IS that block, with the values as kwargs.
#
# TWO COMPOSITION PATHS, because a scene may inherit its scenegraph:
#   * no SceneGraphSystem declared yet -> this call DECLARES it (the params ride
#     the initial declareParams, exactly as a hand-written scenegraph(...) did).
#   * one already declared (a base-class scene; scenegraph() forbids
#     re-declaration) -> the sky params are APPENDED as a second declareParams
#     sub_call. setUserSceneParam is dict-assignment and sub_calls run in order,
#     so later keys override earlier ones. skybox_path is not a user param (it
#     rides the SystemData decl), so an override there is written to the decl.
#
# DEFAULTS = the procedural-sky family's day: procedural sky source, unit intensities,
# ZERO ambient. sky_source=None leaves the key OUT entirely — the engine default
# is BAKED (pbr_common.h), which is what a baked-envmap gauge wants.
#
# AMBIENT ZERO (owner decision, jul28): the procedural-sky family is lit by the
# sun and the IBL ALONE — a flat additive ambient term is a second, unphysical
# light source that never darkens with the sky, so a night under it can never
# reach night. scn_forest's restated 0.0 was the intended shape all
# along; this is that correction rolled out to the default. A scene that still
# wants the constant restates ambient_light=0.1 explicitly — the BAKED cascade
# gauge did, and was retired jul28 with the rest of the single-purpose gauge
# scenes. The engine-side additive term (fwdtools.i2) is
# untouched — five non-sky consumers rely on it.
###############################################################################

from orkengine.core import vec3

# Chain name of the display-encode stage. Stable because it is the handle a
# scene (or a tool) uses to find the node in _postfx_nodes.
ACES_FX_NAME = "aces"


# The tone stage's authorable knobs, python spelling -> the engine's own
# property name. THE WHOLE ADAPTATION CURVE plus the authored exposure it
# composes with: three luminance ANCHORS (where on the measured-light ladder day,
# early night and the dead of night sit) and the three adaptation VALUES at those
# anchors. They are reflected on PostFxNodeACES, so a value declared here rides
# the .ecs into the zero-python player; before this table they were reachable
# from C++ and from a live pyext session and from NOWHERE a scene could say it.
#
# The dead-of-night value (adapt_floor) is the one a night calibration moves:
# it acts only BELOW the twilight anchor, so raising it cannot disturb the early
# night the owner already accepted.
TONEMAP_KNOBS = {
    "exposure":                "exposure",
    "adapt_day_luminance":     "adapt_day_luminance",
    "adapt_twilight_luminance": "adapt_twilight_luminance",
    "adapt_floor_luminance":   "adapt_floor_luminance",
    "adapt_day":               "adapt_day",
    "adapt_twilight":          "adapt_twilight",
    "adapt_floor":             "adapt_floor",
}


# The ARTIST GROUND-HAZE layer, python spelling -> the SkyAtmosphereData property
# it lands on. The engine's medium is in KM and per-KM (the Hillaire units the
# LUTs bake in); a scene talks in the WORLD METRES it places geometry with, so
# every length here is converted through the atmosphere's own
# _kilometersPerWorldUnit (sky_atmosphere.h) — the single source for that scale,
# read off the object being built rather than restated.
#
# distance_m is the layer's e-folding DISTANCE (how far a ground-level sight line
# runs before the haze has eaten 1/e of it), which is what an author can look at a
# scene and estimate; the engine wants its reciprocal as an extinction
# coefficient. distance_m = 0 means NO artist layer, which is the engine's own
# shipped default — so it writes nothing at all (see _write_haze).
#
# `enable` is NOT part of that layer: it arms/disarms the whole aerial-perspective
# march, geophysical term included, so it is honoured on its own and a scene can
# say haze={"enable": False} to turn distance haze off entirely.
HAZE_KNOBS = {
    "distance_m":     "haze_density",              # world metres -> 1/km
    "scale_height_m": "haze_scale_height",         # world metres -> km
    "tint":           "haze_scatter_tint",         # vec3 / triple
    "phase_g":        "haze_phase_g",
    "inscatter_tint": "haze_inscatter_tint",       # vec3 / triple
    "max_distance_m": "haze_max_distance_km",      # world metres -> km
    "enable":         "aerial_perspective_enable",
    "shadow":         "haze_sun_shadow",           # terrain-shadowed march:
                                                   # the forward aerial march
                                                   # samples the sun cascades
                                                   # per step, so air in a
                                                   # caster's shadow stops
                                                   # in-scattering direct sun.
                                                   # THREE-VALUED — see
                                                   # _HAZE_SHADOW_MODES below
    "shadow_gain":    "haze_sun_shadow_gain",      # artistic accentuation of
                                                   # the shaft term; 1.0 =
                                                   # physical. Applied to the
                                                   # OCCLUSION, so both shadow
                                                   # modes honour it identically
}


# haze={"shadow": ...} — WHERE the shadowed march samples the cascades, never
# whether shafts exist. False/True keep meaning exactly what they always did, so
# no authored scene moves; the string arm names the third mode rather than making
# a scene write a bare 2.0 whose meaning is only in the engine header.
#
#   False / "off"           0.0  no cascade taps
#   True  / "on" / "inline" 1.0  per forward fragment, full res (the reference look)
#   "1/4res" / "quarter"    2.0  marched once at half dims and composited back;
#                                cheaper, softer shaft edges, and it does not
#                                reach transparent or unlit surfaces
_HAZE_SHADOW_MODES = {
    "off":     0.0,
    "on":      1.0,
    "inline":  1.0,
    "1/4res":  2.0,
    "quarter": 2.0,
}


def _haze_shadow_mode(value):
  """The authored 'shadow' knob -> the engine's mode float. Unknown strings are a
  scene-authoring mistake, and a silent fallback to 'on' would hide the very A/B
  the third mode exists for — so they raise."""
  if isinstance(value, str):
    key = value.strip().lower()
    if key not in _HAZE_SHADOW_MODES:
      raise ValueError(
          "haze={'shadow': %r} is not a mode — expected one of %s, or a bool"
          % (value, sorted(_HAZE_SHADOW_MODES)))
    return _HAZE_SHADOW_MODES[key]
  return 1.0 if value else 0.0


# The named looks. STARTING POINTS: these numbers are the plan's opening guesses
# at each look, and the look loop (HAZE.md 3.5 — the render set the owner judges)
# is what RATIFIES them; until it has run, a preset is a place to start tuning
# from, not a blessed value.
#
# clear       — no artist layer at all: the pure geophysical atmosphere, which is
#               also what a scene that says nothing gets.
# hazy_day    — a humid summer afternoon: 25 km visibility-scale, a shallow
#               700 m layer, mildly forward-scattering.
# bladerunner — the stylized end: a 4 km e-fold in a DEEP 1.5 km layer, strongly
#               forward-scattering, warm on both the layer's albedo and the grade
#               applied to its own in-scatter.
HAZE_PRESETS = {
    "clear":       dict(distance_m=0.0),
    "hazy_day":    dict(distance_m=25000.0, scale_height_m=700.0, phase_g=0.55,tint=(0.5, 0.9, 1.0), inscatter_tint=(0.4, 0.75, 1.1)),
    "hazy_day2":    dict(distance_m=25000.0, scale_height_m=700.0, phase_g=0.55,tint=(0.9, 0.9, 1.0), inscatter_tint=(0.7, 0.75, 1.1)),
    "bladerunner": dict(distance_m=4000.0, scale_height_m=1500.0, phase_g=0.75,
                        tint=(1.0, 0.9, 0.75), inscatter_tint=(1.1, 0.75, 0.45)),
}


def _haze_vocabulary():
  return ("haze knobs: %s; presets: %s"
          % (", ".join(sorted(HAZE_KNOBS)), ", ".join(sorted(HAZE_PRESETS))))


def _resolve_haze(haze):
  """the haze declaration, resolved to {knob: value} over HAZE_KNOBS.

  None / a preset NAME / a dict (which may carry "preset" to take a named look as
  its BASE and override individual knobs on top of it). An unknown preset or an
  unknown knob RAISES with the full vocabulary — the same reason _aces_node
  raises: nothing here reaches declareParams as a key, so a misspelling would
  author a sky that silently has no haze in it."""
  if haze is None:
    return {}
  if isinstance(haze, str):
    haze = {"preset": haze}
  if not isinstance(haze, dict):
    raise ValueError(
      "Scene.sky(haze=%r) — haze takes None, a preset name, or a dict of knobs "
      "(which may name a \"preset\" to base on). %s" % (haze, _haze_vocabulary()))

  decl = dict(haze)
  preset = decl.pop("preset", None)
  knobs = {}
  if preset is not None:
    if preset not in HAZE_PRESETS:
      raise ValueError(
        "Scene.sky(haze=%r) — no such haze preset. %s" % (preset, _haze_vocabulary()))
    knobs.update(HAZE_PRESETS[preset])
  for key, value in decl.items():
    if key not in HAZE_KNOBS:
      raise ValueError(
        "Scene.sky(haze={%r: ...}) — no such haze knob. %s" % (key, _haze_vocabulary()))
    knobs[key] = value
  return knobs


def _write_haze(atmo, knobs):
  """author the resolved knobs onto a SkyAtmosphereData. Returns whether ANYTHING
  was written — a "clear" sky writes nothing and needs no object, because zero
  density IS the shipped C++ default and an object published to say so would only
  clobber a medium tuned elsewhere (the whole-pointer law below)."""
  if not knobs:
    return False
  wrote = False
  if "enable" in knobs:
    atmo.aerial_perspective_enable = bool(knobs["enable"])
    wrote = True
  # shadow writes in the HEADER section (before the distance early-out): it
  # shadows the whole aerial-perspective march — the geophysical in-scatter
  # included — so it is meaningful with no artist haze layer declared at all.
  if "shadow" in knobs:
    atmo.haze_sun_shadow = _haze_shadow_mode(knobs["shadow"])
    wrote = True
  # gain writes in the HEADER section for the same reason 'shadow' does, and
  # independently of it: an authored gain must survive a mode A/B driven from
  # the HUD, which never touches the scene.
  if "shadow_gain" in knobs:
    atmo.haze_sun_shadow_gain = max(float(knobs["shadow_gain"]), 0.0)
    wrote = True

  distance_m = float(knobs.get("distance_m", 0.0))
  if distance_m <= 0.0:
    return wrote

  # the atmosphere's OWN world scale, so a scene built in feet or in kilometres
  # converts by the same number the shader marches with.
  km_per_wu = float(atmo.kilometers_per_world_unit)
  atmo.haze_density = 1.0 / (distance_m * km_per_wu)
  if "scale_height_m" in knobs:
    atmo.haze_scale_height = float(knobs["scale_height_m"]) * km_per_wu
  if "max_distance_m" in knobs:
    atmo.haze_max_distance_km = float(knobs["max_distance_m"]) * km_per_wu
  if "phase_g" in knobs:
    atmo.haze_phase_g = float(knobs["phase_g"])
  for key in ("tint", "inscatter_tint"):
    if key in knobs:
      v = knobs[key]
      setattr(atmo, HAZE_KNOBS[key],
              v if hasattr(v, "x") else vec3(float(v[0]), float(v[1]), float(v[2])))
  return True


# The scene-param vocabulary a caller may hand through **sg_params, and the ONLY
# reason this set is written down: everything past this call is a DICT ASSIGNMENT
# (setUserSceneParam) that the engine reads key by key, so a key nobody reads is
# dropped without a word. A tone-stage floor spelled as a bare sky() kwarg died
# exactly that way — it looked declared and rendered the engine default.
#
# Sourced from the readers, not invented: scenegraph.cpp (the sky/exposure/SSAO/
# compositor block), SceneGraphSystem.cpp (msaa + the Vr* block), plus the four
# keys SceneGraphHandle itself consumes python-side (skybox_path/skybox_probe/
# postfx/aux_channels) and scenegraph()'s own preset/layers. A key the engine
# grows belongs here the day it grows.
SCENE_PARAM_KEYS = frozenset((
    # sky / exposure / IBL
    "SkySource", "SkyboxTexPathStr", "SkyAtmosphere", "SkyboxIntensity",
    "DiffuseIntensity", "SpecularIntensity", "EnvironmentIntensity",
    "AmbientLight", "enable_skybox", "diffuse_brdf",
    # frame / compositor
    "preset", "layers", "msaa", "ssaa", "clearcolor", "use_float_color_buffer",
    "DepthPrepass", "dppZbias", "DepthFogDistance", "CullFrustumScale",
    "AuxChannels", "PostFxChain", "compositordata", "outputRTG",
    "onRenderComplete", "dbufcontext",
    # SSAO
    "SSAOBias", "SSAOFeedback", "SSAOPower", "SSAORadius", "SSAOWeight",
    # VR (SceneGraphSystem)
    "VrCant", "VrDepthPublish", "VrDistortion", "VrDistortShader", "VrEyeRot",
    "VrFar", "VrFov", "VrIPD", "VrLensCenter", "VrNear", "VrPredAhead",
    # consumed python-side by SceneGraphHandle / scenegraph()
    "skybox_path", "skybox_probe", "postfx", "aux_channels",
))


def _sky_kwarg_names():
  """Every named kwarg of the sky surface, INTROSPECTED — the error below names
  the alternatives, and a hand-written list of them would go stale silently."""
  import inspect
  from ork.hypergraph.ecs.scene._sky import SkyMixin
  names = set(inspect.signature(SkyMixin.sky).parameters)
  names |= set(inspect.signature(SkyDomeMixin.sky_dome).parameters)
  return names - {"self", "sg_params", "dome_params"}


def _reject_unknown_params(params):
  """The passthrough's self-defense. Anything not in SCENE_PARAM_KEYS is a
  MISSPELLING or a knob that lives on a named kwarg, and either way the frame
  would render as if it had never been said."""
  for key in params:
    if key not in SCENE_PARAM_KEYS:
      raise ValueError(
        "Scene.sky(%s=...) / Scene.sky_dome(%s=...) — no such kwarg, and no "
        "such scene param: it would be forwarded to declareParams, where an "
        "unread key is DROPPED SILENTLY. Named kwargs of the sky surface: %s. "
        "Scene params it forwards: %s."
        % (key, key,
           ", ".join(sorted(_sky_kwarg_names())),
           ", ".join(sorted(SCENE_PARAM_KEYS))))


def _aces_node(knobs=None):
  """The tone stage: ACES filmic tonemap + output dither (framefx.fxv2
  ps_aces), and the seat of the SCENE ADAPTATION term. Constructed at
  declaration time and REFLECTED into the .ecs, so the zero-python player
  rebuilds its own instance from the same authored knobs.

  `knobs` is a dict over TONEMAP_KNOBS. An unknown key RAISES rather than
  falling through: this dict does not reach declareParams (where unknown keys
  are silently dropped), and a mis-spelled adaptation knob that looked accepted
  and did nothing is the exact failure this surface exists to prevent."""
  from orkengine.lev2 import PostFxNodeACES
  node = PostFxNodeACES()
  # The AUTHORED exposure — 1.0 = leave the grade to the tone curve. The
  # adaptation term composes with this per frame and never replaces it, so a
  # scene that writes another value here keeps it.
  node.exposure = 1.0
  for key, value in (knobs or {}).items():
    if key not in TONEMAP_KNOBS:
      raise ValueError(
        "Scene.sky(tonemap={%r: ...}) — no such tone-stage knob. The stage "
        "takes: %s." % (key, ", ".join(sorted(TONEMAP_KNOBS))))
    setattr(node, TONEMAP_KNOBS[key], float(value))
  return node


class SkyDomeMixin:

  def sky_dome(self, *,
               sky_source         = "procedural",
               skybox_path        = None,
               skybox_intensity   = 1.0,
               diffuse_intensity  = 1.0,
               specular_intensity = 1.0,
               ambient_light      = 0.0,
               tonemap            = False,
               haze               = None,
               ibl_snapshot_interval = None,
               ibl_snapshot_extent   = None,
               ibl_level_batches     = None,
               ibl_slices_per_frame  = None,
               ibl_mipchain_budget_px = None,
               preset             = "ForwardPBR",
               **sg_params):
    """Declare (or amend) the scene's sky/IBL scenegraph params.

    sky_source         — "procedural" (Hillaire sky-view LUT + analytic disc),
                         "baked" (the skybox envmap), or None to leave the key
                         unset (engine default = baked).
    skybox_path        — IBL source .xir; also the procedural warm-up fallback.
    skybox_intensity   — visible-sky gain.
    diffuse_intensity  — IBL diffuse gain.
    specular_intensity — IBL specular gain.
    ambient_light      — float (broadcast to vec3) or a vec3. 0.0 by decision
                         (see the header); restate a value to opt back in.
    tonemap            — attach the ACES tone stage ("aces") to the postfx
                         chain. The stage carries its own scene adaptation,
                         driven engine-side from the measured environment;
                         Scene.sky() turns it on for the procedural-sky family.
                         A DICT attaches it AND authors its knobs (TONEMAP_KNOBS
                         above — the three luminance anchors, the three
                         adaptation values, and the authored exposure). An
                         unknown key raises.
    haze               — DISTANCE HAZE (aerial perspective): the artist ground
                         layer that rides under the same Beer-Lambert law as the
                         geophysical atmosphere. A preset NAME ("clear",
                         "hazy_day", "bladerunner"), a DICT of knobs (HAZE_KNOBS
                         above; it may name a "preset" to base on and override
                         individual knobs), or None. Lengths are WORLD METRES —
                         distance_m is the layer's e-folding sight distance —
                         converted through the atmosphere's own world scale. An
                         unknown preset or knob raises.
                         None, and "clear", declare NOTHING: zero artist density
                         is the engine's shipped default, so a silent scene keeps
                         the pure geophysical aerial perspective it already had.
                         haze={"enable": False} turns distance haze off outright,
                         geophysical term included. Same whole-pointer caveat as
                         the cadence knobs below — a declaration here and an IBL
                         declaration share ONE published atmosphere.
    ibl_snapshot_interval
                       — seconds between procedural-IBL rebakes, DECLARED. Above
                         zero it replaces the engine's sun-motion trigger
                         outright (SkyAtmosphereData._iblSnapshotInterval), so the
                         rebake rate stops following the scene's day-cycle rate;
                         the trade is IBL lag on a moving sun. FLOORED at the
                         measured cycle+fade span (a rebake landing mid-crossfade
                         pops); a declaration under that floor runs at the floor
                         and logs the shortfall once. None (the default)
                         leaves the sun-angle behavior alone and publishes NO
                         atmosphere object — the key is only written when a value
                         is declared, because the engine reads the atmosphere as a
                         whole pointer and would otherwise clobber a medium tuned
                         elsewhere with this call's defaults.
    ibl_snapshot_extent
                       — (width, height) of the equirect sky snapshot the IBL
                         feed refilters (SkyAtmosphereData._iblSnapshotWidth /
                         _iblSnapshotHeight; engine default 512x256). The
                         per-cycle refilter cost is linear in this AREA, so
                         halving both dimensions quarters it — the trade is
                         angular detail in the reflections and the SH ambient,
                         which a sky of broad gradients does not miss. None
                         (the default) leaves the engine extent alone; same
                         whole-pointer caveat as the cadence knobs below.
    ibl_level_batches, ibl_slices_per_frame, ibl_mipchain_budget_px
                       — IBL GRANULARITY: how the sliced prefilter CUTS UP its
                         work (GPU submits per level; how many of the job's
                         slices one frame may run; one publish-chain slice's CPU
                         downsample budget in output pixels). They change pacing,
                         never the filtered result. All three are extent-
                         dependent measurements — the shipped 1 / 1 / 12288 were
                         measured at the 512x256 snapshot — so a scene that runs
                         a different snapshot extent is the one with a reason to
                         restate them. None (the default) declares nothing and
                         leaves the ORKID_MT_IBL_* env vars / engine defaults in
                         charge; same whole-pointer caveat as above applies.
    preset             — compositor preset; only consulted when this call is the
                         one that declares the scenegraph.
    **sg_params        — passed through to scenegraph() / the appended
                         declareParams (msaa, ssaa, postfx, aux_channels, ...).
                         CHECKED against SCENE_PARAM_KEYS: a key the engine does
                         not read raises here instead of being dropped by
                         declareParams three layers down.

    Returns the SceneGraphHandle."""

    _reject_unknown_params(sg_params)

    if isinstance(ambient_light, (int, float)):
      ambient_light = vec3(float(ambient_light))

    params = {}
    if sky_source is not None:
      params["SkySource"] = sky_source
    params["SkyboxIntensity"]   = float(skybox_intensity)
    params["DiffuseIntensity"]  = float(diffuse_intensity)
    params["SpecularIntensity"] = float(specular_intensity)
    params["AmbientLight"]      = ambient_light

    # The medium rides the params as a REFLECTED OBJECT (varmap object codec,
    # lev2_init.cpp), not a string. Written only when something was declared: the
    # engine's _atmosphere is a whole-pointer assignment, so an unconditional
    # default here would silently overwrite a tuned medium — and null is the
    # engine's own "attach the earth-like default" signal, which stays intact.
    #
    # That same whole-pointer law is why the IBL cadence and the haze layer share
    # ONE object: two declarations, two SkyAtmosphereData under the one key, and
    # whichever was written second would erase the other's knobs along with the
    # rest of the medium. So the object is built ONCE here, both authors write
    # into it, and it is published only if either of them actually wrote.
    if ibl_snapshot_extent is not None:
      try:
        _snap_w, _snap_h = (int(v) for v in ibl_snapshot_extent)
      except (TypeError, ValueError):
        raise ValueError(
          "Scene.sky(ibl_snapshot_extent=%r) — declare the equirect snapshot as "
          "a (width, height) pair." % (ibl_snapshot_extent,))
    ibl_decls = (ibl_snapshot_interval, ibl_snapshot_extent, ibl_level_batches,
                 ibl_slices_per_frame, ibl_mipchain_budget_px)
    haze_knobs = _resolve_haze(haze)      # raises here, at declaration time
    if any(v is not None for v in ibl_decls) or haze_knobs:
      from orkengine.lev2 import SkyAtmosphereData
      atmo = SkyAtmosphereData()
      # each knob written only if declared: an undeclared one must keep the
      # object's own default (0 = unset on the granularity three), which is what
      # the engine reads as "env var / shipped default".
      if ibl_snapshot_interval is not None:
        atmo.ibl_snapshot_interval = float(ibl_snapshot_interval)
      if ibl_snapshot_extent is not None:
        atmo.ibl_snapshot_width  = _snap_w
        atmo.ibl_snapshot_height = _snap_h
      if ibl_level_batches is not None:
        atmo.ibl_level_batches = int(ibl_level_batches)
      if ibl_slices_per_frame is not None:
        atmo.ibl_slices_per_frame = int(ibl_slices_per_frame)
      if ibl_mipchain_budget_px is not None:
        atmo.ibl_mipchain_budget_px = int(ibl_mipchain_budget_px)
      wrote_haze = _write_haze(atmo, haze_knobs)
      if any(v is not None for v in ibl_decls) or wrote_haze:
        params["SkyAtmosphere"] = atmo

    params.update(sg_params)

    # diffuse_brdf resolves HERE as well as in SceneGraphHandle, because the
    # amend path below does not go through it — it appends declareParams
    # straight onto an already-declared scenegraph. Resolving a name is
    # idempotent, so the scenegraph() path simply resolves it twice.
    if "diffuse_brdf" in params:
      from ork.hypergraph.ecs.scene import _resolve_diffuse_brdf
      params["diffuse_brdf"] = _resolve_diffuse_brdf(params["diffuse_brdf"])

    # The ACES stage goes on the END of whatever chain the scene already has: a
    # tonemap is the last thing that touches a frame, so an author's grade
    # (the forest content's hsvg) runs on HDR, as it did before this stage existed.
    # A dict is BOTH the switch and the knobs (see _aces_node); a bool is the
    # switch alone.
    aces = _aces_node(tonemap if isinstance(tonemap, dict) else None) if tonemap else None

    if "SceneGraphSystem" not in self._systems:
      if skybox_path is not None:
        params["skybox_path"] = skybox_path
      if aces is not None:
        params["postfx"] = list(params.get("postfx") or []) + [(ACES_FX_NAME, aces)]
      return self.scenegraph(preset=preset, **params)

    # Already declared upstream — amend it (scn_forest's mechanism).
    # postfx is not a declareParams key (SceneGraphHandle lowers it to sub_calls
    # at declaration time), so the amend path writes those sub_calls itself.
    handle = self.SG
    if skybox_path is not None:
      handle._decl.kwargs["skybox_path"] = skybox_path
    handle._decl.sub_calls.append(("declareParams", (params,), {}))
    if aces is not None:
      handle._decl.sub_calls.append(("addPostFxNode", (ACES_FX_NAME, aces), {}))
      handle._decl.sub_calls.append(("appendPostFxOrder", (ACES_FX_NAME,), {}))
    return handle
