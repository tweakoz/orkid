###############################################################################
# _star_dome.py — PythonComponent BEHAVIOR script: turn the STAR DOME with the
# sidereal clock. Attached by Scene.stars(); sibling of _celestial_orbit.py and
# same contract in every respect except what it computes.
#
# WHAT IT SETS: ent.orientation only — the dome mesh is authored with its local
# +Y at the NORTH CELESTIAL POLE and its star field shaded from OBJECT space, so
# the entity orientation IS the sky's aim:
#     tilt(+X, latitude - 90)  *  spin(+Y, -sidereal_angle)
# i.e. tip local +Y to the pole (elevation = observer latitude, due north), then
# spin by the accumulated sidereal angle — NEGATIVE, because the sky's apparent
# turn is the reverse of the earth's. This is the port of
# CelestialSnapshot.star_dome_quat() onto the sim's ecssim quats (the snapshot's
# own helper needs orkengine.core, which does not exist in this subinterpreter).
#
# CONFIG: the same $ORK_CELESTIAL_CONFIG table _celestial_orbit.py reads, keyed
# by ENTITY NAME. Scene.stars() publishes the SUN's site+clock verbatim under the
# dome's name (body stays "sun" — the dome aims no light and this script never
# branches on body; only latitude and the clock matter to the sidereal angle).
#
# NO NIGHT POLICY, no light bridge, no varmap publication: the dome is not a
# light. The twilight fade lives in the MATERIAL (assets/materials/star_dome.py),
# which reads the engine's live sun uniform per fragment — no per-frame python.
#
# Runs in the ECS sim SUBINTERPRETER: engine access is the restricted
# orkengine.ecssim API, and _celestial.py is loaded BY PATH (a dotted import
# would execute the scene package __init__ chain, which pulls orkengine.core /
# lev2 — absent here). See _celestial_orbit.py's header for the full rationale.
###############################################################################
import json
import math
import os
import sys

from orkengine.ecssim import vec3, quat

_SCENE_RELDIR  = os.path.join("ork", "hypergraph", "ecs", "scene")
_MODEL_RELPATH = os.path.join(_SCENE_RELDIR, "_celestial.py")

_MODELS = {}
_LOADED = {}


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


def _model_for(comp):
    ent = comp.entity
    model = _MODELS.get(ent.id)
    if model is not None:
        return model
    celestial = _load_by_path(_MODEL_RELPATH, "_ork_celestial")
    carried = getattr(comp, "script_data", "")
    if carried:
        cfg, source = json.loads(carried), "component scriptData"
    else:
        blob = os.environ.get(celestial.CONFIG_ENV_KEY, "")
        table = json.loads(blob) if blob else {}
        cfg = table.get(ent.name)
        source = "$%s (declared: %s)" % (celestial.CONFIG_ENV_KEY, sorted(table))
    if cfg is None:
        raise KeyError(
            "no celestial config for star dome entity <%s> — neither on the "
            "component (scriptData, which is what rides the .ecs) nor in %s. "
            "Scene.stars() attaches the sun's site and clock at "
            "scene-declaration time" % (ent.name, source))
    model = celestial.CelestialModel.from_config(celestial.normalize_config(cfg))
    _MODELS[ent.id] = model
    return model


def onUpdate(comp, updinfo):
    ent  = comp.entity
    snap = _model_for(comp).at(updinfo.abstime)
    # The model's sidereal angle ACCUMULATES (it is seam-free by construction);
    # WRAP it before it reaches a quat, because the angle crosses the python →
    # engine boundary as a FLOAT32: at the thousands-of-radians an unwrapped
    # angle reaches, single precision alone costs tenths of a degree of sky.
    spin_deg = snap.sidereal_angle_deg % 360.0
    tilt = quat(vec3(1.0, 0.0, 0.0),
                math.radians(snap.pole_elevation_deg - 90.0))
    spin = quat(vec3(0.0, 1.0, 0.0), math.radians(-spin_deg))
    ent.orientation = tilt * spin
