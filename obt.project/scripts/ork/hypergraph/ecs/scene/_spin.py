###############################################################################
# _spin.py — shared PythonComponent BEHAVIOR script: auto-rotate the entity
# about world +Y at a constant rate. Attach via Scene.spinner(), which declares
# a PythonComponent (scriptFile=this) and ensures the host PythonSystem.
#
# onUpdate(comp, updinfo) runs every frame inside the engine's PythonSystem
# update, BEFORE the SceneGraph reads the entity transform — so setting the
# orientation here spins the rendered model in place. There is NO per-frame
# Python at render time: the engine owns the loop; this is data.
#
# Runs in the ECS sim SUBINTERPRETER (the restricted orkengine.ecssim API):
#   comp            — the SimComponent; comp.entity reaches the Entity
#   updinfo.abstime — absolute game time (s); spin off this (drift-free), not dt
###############################################################################
from orkengine.ecssim import vec3, quat

_SPIN_RATE = 0.25          # radians / second, about world +Y


def onUpdate(comp, updinfo):
    # absolute angle (no drift): rate * elapsed time about +Y.
    angle = updinfo.abstime * _SPIN_RATE
    comp.entity.orientation = quat(vec3(0.0, 1.0, 0.0), angle)
