###############################################################################
# _spatial_audio_showcase_sys.py — PythonSystem script for
# scn_spatial_audio_showcase: RUNTIME EMITTER SPAWNING.
#
# Runs on the simulation update thread in the ecssim sub-interpreter, in EVERY
# host (python demo, headless harness, pure-C++ ork.ecs.player.exe) — so the
# runtime-spawn proof is host-independent scene behavior, not harness code.
#
# From CHIME_START_TIME onward, spawns one short-lived "chime" entity every
# CHIME_PERIOD seconds at a seeded-pseudorandom position on a ring around
# CHIME_CENTER. The entity carries a StochWavSoundEmitterData (group "chimes")
# — it starts ringing on activation and is auto-despawned by the spawner's
# lifetime, which keyOffs its voices through component deactivation.
#
# CONSTANTS duplicated from scn_spatial_audio_showcase.py (the sub-interpreter
# cannot import the scene module) — KEEP IN SYNC.
###############################################################################

import math

from orkengine.ecssim import *

tokens = CrcStringProxy()

CHIME_CENTER = vec3(25.0, 2.0, -14.0)
CHIME_START_TIME = 45.0
CHIME_PERIOD = 6.0
CHIME_RADIUS_MIN = 4.0
CHIME_RADIUS_MAX = 12.0

# deterministic counter-hash position sequence (no stateful RNG — the
# determinism law): golden-angle ring walk.
GOLDEN = 2.399963229728653


class _ChimeState:
  def __init__(self):
    self.spawner = None
    self.count = 0
    self.next_time = CHIME_START_TIME


def onSystemInit(simulation):
  simulation.vars.chimes = _ChimeState()


def onSystemLink(simulation):
  st = simulation.vars.chimes
  st.spawner = simulation.findSpawner("chime_spawner")
  if not st.spawner:
    print("spatial_audio_showcase_sys: chime_spawner NOT FOUND (no runtime chimes)")


def onSystemActivate(simulation):
  pass


def onSystemStage(simulation):
  pass


def onSystemNotify(simulation, evID, table):
  pass


def onSystemUpdate(simulation):
  st = simulation.vars.chimes
  if st.spawner is None or not st.spawner:
    return
  gt = simulation.gameTime
  while gt >= st.next_time:
    k = st.count
    ang = k * GOLDEN
    rad = CHIME_RADIUS_MIN + (CHIME_RADIUS_MAX - CHIME_RADIUS_MIN) * (
        (k * 0.6180339887) % 1.0)
    pos = vec3(CHIME_CENTER.x + rad * math.cos(ang),
               CHIME_CENTER.y,
               CHIME_CENTER.z + rad * math.sin(ang))
    st.spawner.spawn(pos=pos)
    st.count += 1
    st.next_time += CHIME_PERIOD
    print("spatial_audio_showcase_sys: spawned chime %d at t=%.1f pos=<%.1f %.1f %.1f>"
          % (st.count, gt, pos.x, pos.y, pos.z))
