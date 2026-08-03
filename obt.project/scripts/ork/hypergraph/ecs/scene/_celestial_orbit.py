###############################################################################
# _celestial_orbit.py — shared PythonComponent BEHAVIOR script: aim a directional
# SUN *or* MOON from the CELESTIAL MODEL (_celestial.py) every frame, so the light
# walks a real observer-frame arc — the right seasonal elevation for the latitude,
# not the azimuth-only spin _sun_orbit.py does. Attach via
# Scene.sun(celestial={...}) / Scene.moon().
#
# WHICH BODY: the config's `body` key ("sun" | "moon"). One script serves both —
# the only differences are which snapshot angles aim the entity and which half of
# the night policy drives the light.
#
# THE NIGHT POLICY (_night_policy.py) is evaluated here, by BOTH bodies, every
# frame, and applied through the entity-varmap light bridge
# (SceneGraphSystem::_updateLightBridge):
#     ent.vars.light_intensity      = base_intensity * that body's scale
#     ent.vars.light_casts_shadows  = 1.0 / 0.0 per the policy
# The policy guarantees the two bodies never both claim the cascade, so the
# renderer's single-sun law (fwdnode_impl_sub.cpp sun selection) is never in the
# "first wins" fallback.
#
# Modeled on _sun_orbit.py; same contract:
#   onUpdate(comp, updinfo) runs each frame inside the PythonSystem update,
#   BEFORE the SceneGraph syncs the entity transform onto the light node, so
#   re-aiming the entity here re-aims the rendered sun. There is NO per-frame
#   python at render time.
#
# Runs in the ECS sim SUBINTERPRETER (the restricted orkengine.ecssim API):
#   comp            — the SimComponent; comp.entity reaches the Entity
#   updinfo.abstime — absolute game time (s); the model is evaluated off this
#                     (stateless → drift-free, and scrubbing is exact)
#
# TWO THINGS HAVE TO CROSS INTO THE SUBINTERPRETER, and neither can ride a
# reflected channel (PythonComponentData carries only a script path):
#
#   the CONFIG — arrives as JSON on THIS COMPONENT, comp.script_data
#     (PythonComponentData::_scriptData, reflected). That is the only channel
#     that survives the shipped playback recipe, which AUTHORS the scene in one
#     process (ork.scene.tojson.py) and PLAYS it in another
#     (ork.ecs.player.exe): the config has to be in the .ecs, because nothing
#     else crosses. The $ORK_CELESTIAL_CONFIG environment table stays as a
#     name-keyed FALLBACK for callers that drive this script directly (the
#     unit tests) — a light with neither is a mis-wired scene and raises rather
#     than defaulting to some other site's sky.
#
#   the MODEL CODE — _celestial.py and _night_policy.py are loaded BY PATH off
#     sys.path rather than imported as ork.hypergraph.ecs.scene.*, because that
#     dotted import would execute the package __init__ chain, which pulls
#     orkengine.core/lev2 — modules that do not exist in this subinterpreter.
#     Both files are pure math (one imports `math`, the other nothing), so
#     loading the files alone is safe.
#
# SNAPSHOT EXPOSURE: the full evaluation is stashed on the ENTITY VARMAP each
# frame as sun_elevation / sun_azimuth / moon_elevation / moon_azimuth /
# moon_phase / moon_illumination / sidereal_angle (degrees, astronomical
# azimuths) — that is the retrieval point for the consumers that follow (star
# dome, exposure curve), alongside the policy readout night_sun_scale /
# night_moon_scale / night_sun_casts / night_moon_casts.
# ent.vars.celestial_valid flags that a frame has been evaluated at all.
###############################################################################
import json
import math
import os
import sys

from orkengine.ecssim import vec3, quat

_SCENE_RELDIR = os.path.join("ork", "hypergraph", "ecs", "scene")
_MODEL_RELPATH  = os.path.join(_SCENE_RELDIR, "_celestial.py")
_POLICY_RELPATH = os.path.join(_SCENE_RELDIR, "_night_policy.py")
_SKYTIME_RELPATH = os.path.join(_SCENE_RELDIR, "_sky_time.py")

# Per-entity model + config, keyed by entity id so one shared script serves
# several lights (a sun and a moon over the same site are two entities).
_MODELS = {}
_CONFIGS = {}
_LOADED = {}
_ADOPTED = set()   # entity ids that have handed their clock to the live control


def _load_by_path(relpath, modname):
    module = _LOADED.get(relpath)
    if module is not None:
        return module
    import importlib.util
    for root in sys.path:
        candidate = os.path.join(root or ".", relpath)
        if os.path.exists(candidate):
            spec = importlib.util.spec_from_file_location(modname, candidate)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            _LOADED[relpath] = module
            return module
    raise ImportError(
        "celestial module <%s> not found on sys.path — the sim "
        "subinterpreter cannot see obt.project/scripts" % relpath)


def _celestial_module():
    return _load_by_path(_MODEL_RELPATH, "_ork_celestial")


def _policy_module():
    return _load_by_path(_POLICY_RELPATH, "_ork_night_policy")


def _sky_time_module():
    return _load_by_path(_SKYTIME_RELPATH, "_ork_sky_time")


def _live_clock(comp, model):
    """The scene's LIVE sky clock, or None when the scene declares no control
    surface (sky_time_system.py publishes the block on the simulation varmap at
    system init, which runs before any component update).

    None is not a degraded path: it is the authored stateless clock this script
    has always evaluated, kept as the untouched arithmetic so a scene without
    the control surface renders exactly the sky it did before."""
    skytime = _sky_time_module()
    simvars = comp.sim.vars
    if skytime.CONTROL_KEY not in simvars:
        return None
    ctrl = getattr(simvars, skytime.CONTROL_KEY)
    ent_id = comp.entity.id
    if ent_id not in _ADOPTED:
        _ADOPTED.add(ent_id)
        ctrl.adopt(model)   # first body in wins; a mismatched second is loud
    return ctrl


def _config_for(comp, ent, celestial):
    """This component's celestial config: the .ecs-borne payload first, the
    author-time environment table second. Returns None when neither has one."""
    carried = getattr(comp, "script_data", "")
    if carried:
        return json.loads(carried), "component scriptData"
    blob = os.environ.get(celestial.CONFIG_ENV_KEY, "")
    table = json.loads(blob) if blob else {}
    return table.get(ent.name), "$%s (declared: %s)" % (
        celestial.CONFIG_ENV_KEY, sorted(table))


def _model_for(comp):
    ent = comp.entity
    model = _MODELS.get(ent.id)
    if model is not None:
        return model, _CONFIGS[ent.id]
    celestial = _celestial_module()
    cfg, source = _config_for(comp, ent, celestial)
    if cfg is None:
        raise KeyError(
            "no celestial config for light entity <%s> — neither on the "
            "component (scriptData, which is what rides the .ecs) nor in %s. "
            "Scene.sun(celestial=...) / Scene.moon() attach it at "
            "scene-declaration time" % (ent.name, source))
    cfg = celestial.normalize_config(cfg)
    model = celestial.CelestialModel.from_config(cfg)
    _MODELS[ent.id] = model
    _CONFIGS[ent.id] = cfg
    return model, cfg


def onUpdate(comp, updinfo):
    ent = comp.entity
    model, cfg = _model_for(comp)
    # THE CLOCK. With a live control surface every body evaluates the SAME
    # scrubbed instant (one block, read here, never integrated per-body); with
    # none, the authored stateless clock, untouched.
    clock = _live_clock(comp, model)
    snap = (model.at(updinfo.abstime) if clock is None
            else model.evaluate_days(clock.days_at(updinfo.abstime)))
    is_moon = (cfg["body"] == "moon")

    # Same composition as _helpers.elevation_azimuth_quat, rebuilt on ecssim
    # types (orkengine.core is not importable here): azimuth(+Y) * elevation(+X).
    elevation, azimuth = (snap.moon_light_angles() if is_moon
                          else snap.sun_light_angles())
    q_el = quat(vec3(1.0, 0.0, 0.0), math.radians(elevation))
    q_az = quat(vec3(0.0, 1.0, 0.0), math.radians(azimuth))
    ent.orientation = q_az * q_el

    # The day/night handoff. Both bodies evaluate the WHOLE policy (it is a
    # handful of polynomials) and each applies its own half, so the two lights
    # agree on who holds the cascade without talking to each other.
    policy = _policy_module().night_policy(
        snap.sun_elevation_deg, snap.moon_elevation_deg, snap.moon_illumination)
    scale = policy["moon_intensity_scale" if is_moon else "sun_intensity_scale"]
    casts = policy["moon_casts" if is_moon else "sun_casts"]

    v = ent.vars
    # light bridge (SceneGraphSystem::_updateLightBridge) — floats only
    v.light_intensity     = cfg["base_intensity"] * scale
    v.light_casts_shadows = 1.0 if casts else 0.0

    v.sun_elevation    = snap.sun_elevation_deg
    v.sun_azimuth      = snap.sun_azimuth_deg
    v.moon_elevation   = snap.moon_elevation_deg
    v.moon_azimuth     = snap.moon_azimuth_deg
    v.moon_phase       = snap.moon_phase
    v.moon_illumination = snap.moon_illumination
    v.sidereal_angle   = snap.sidereal_angle_deg
    v.celestial_valid  = 1.0
    # policy readout — the observable a night-handoff gate samples
    v.night_sun_scale  = policy["sun_intensity_scale"]
    v.night_moon_scale = policy["moon_intensity_scale"]
    v.night_sun_casts  = 1.0 if policy["sun_casts"] else 0.0
    v.night_moon_casts = 1.0 if policy["moon_casts"] else 0.0

    # WHAT TIME THIS BODY THINKS IT IS — published whether or not a control
    # surface exists, so a consumer (HUD, gate, tool) reads one key rather than
    # branching on how the clock is driven.
    v.sky_epoch_days = snap.epoch_days
    v.sky_hour       = (snap.epoch_days - math.floor(snap.epoch_days)) * 24.0
    if clock is not None:
        # the control block's telemetry, per body: sky_time_system.py prints it
        # (one voice for the whole ensemble) and a gate reads it back.
        clock.report[ent.name] = {
            "hour": v.sky_hour,
            "epoch_days": snap.epoch_days,
            "elevation": (snap.moon_elevation_deg if is_moon
                          else snap.sun_elevation_deg),
            "azimuth": (snap.moon_azimuth_deg if is_moon
                        else snap.sun_azimuth_deg),
            "body": cfg["body"]}
