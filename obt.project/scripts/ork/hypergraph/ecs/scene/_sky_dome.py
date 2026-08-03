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
    "AmbientLight", "enable_skybox",
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
               ibl_snapshot_interval = None,
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
    ibl_decls = (ibl_snapshot_interval, ibl_level_batches,
                 ibl_slices_per_frame, ibl_mipchain_budget_px)
    if any(v is not None for v in ibl_decls):
      from orkengine.lev2 import SkyAtmosphereData
      atmo = SkyAtmosphereData()
      # each knob written only if declared: an undeclared one must keep the
      # object's own default (0 = unset on the granularity three), which is what
      # the engine reads as "env var / shipped default".
      if ibl_snapshot_interval is not None:
        atmo.ibl_snapshot_interval = float(ibl_snapshot_interval)
      if ibl_level_batches is not None:
        atmo.ibl_level_batches = int(ibl_level_batches)
      if ibl_slices_per_frame is not None:
        atmo.ibl_slices_per_frame = int(ibl_slices_per_frame)
      if ibl_mipchain_budget_px is not None:
        atmo.ibl_mipchain_budget_px = int(ibl_mipchain_budget_px)
      params["SkyAtmosphere"] = atmo

    params.update(sg_params)

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
