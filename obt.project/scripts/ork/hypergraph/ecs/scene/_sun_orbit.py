###############################################################################
# _sun_orbit.py — shared PythonComponent BEHAVIOR script: sweep a directional
# SUN through a full azimuth orbit at its declared elevation. Attach via
# Scene.sun(animate_orbit=<seconds>), which declares a PythonComponent
# (scriptFile=this) alongside the sun's SceneGraphComponent + ensures the host
# PythonSystem. Modeled directly on _spin.py.
#
# onUpdate(comp, updinfo) runs every frame inside the engine's PythonSystem
# update, BEFORE the SceneGraph syncs the entity transform onto the light node —
# so re-aiming the entity here re-aims the rendered sun. There is NO per-frame
# Python at render time: the engine owns the loop; this is data.
#
# The START POSE (elevation + azimuth) reaches this script through the ENTITY
# TRANSFORM: Scene.sun() sets the entity orientation from (elevation, azimuth),
# so on the first frame we capture it as q0 and thereafter premultiply a world-Y
# rotation — that spins the azimuth while preserving the elevation baked into q0.
# The orbit PERIOD is the one value that CANNOT ride a channel: PythonComponentData
# reflects only a script path (same limit _spin.py documents for its rate), so it
# lives here as a module constant.
#
# Runs in the ECS sim SUBINTERPRETER (the restricted orkengine.ecssim API):
#   comp            — the SimComponent; comp.entity reaches the Entity
#   updinfo.abstime — absolute game time (s); orbit off this (drift-free), not dt
###############################################################################
import math

from orkengine.ecssim import vec3, quat

_ORBIT_PERIOD = 60.0        # seconds per full 360° azimuth orbit

# Per-entity captured start orientation (elevation+azimuth as declared). Keyed by
# entity id so one shared script serves multiple suns without cross-talk.
_START = {}


def onUpdate(comp, updinfo):
    ent = comp.entity
    q0 = _START.get(ent.id)
    if q0 is None:
        q0 = ent.orientation           # the declared elevation/azimuth pose
        _START[ent.id] = q0
    # absolute azimuth angle (no drift): full turn about world +Y per period.
    angle = updinfo.abstime * (2.0 * math.pi / _ORBIT_PERIOD)
    spin = quat(vec3(1.0, 0.0, 0.0), angle)
    ent.orientation = spin * q0
