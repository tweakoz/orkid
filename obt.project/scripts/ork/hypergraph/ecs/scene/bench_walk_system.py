###############################################################################
# bench_walk_system — the DETERMINISTIC BENCH WALK (a composable PythonSystem
# scene script). It publishes the camera itself, every system tick, as a PURE
# FUNCTION of simulation.gameTime: no input, no physics, no wall clock — the same
# gameTime always yields the same eye/heading, so two bench runs traverse the SAME
# tour and their frame-time distributions are comparable location for location.
#
# It does NOT drive the Bullet walker (a force-driven capsule integrates against
# a variable dt and is therefore not replayable). The scene's own
# CharacterControllerComponent still publishes its (standing) camera each tick;
# the system LUT is sorted by name, so CharacterControllerSystem < PythonSystem <
# SceneGraphSystem — this script's UpdateCamera is the LAST write before the
# scenegraph enqueues the frame, and it wins. Nothing else needs disabling.
#
# THE PATH lives in ork/bench/walk_path.py (a segmented grand tour: K locations of
# different content, each walked at human pace while the view sweeps the full
# compass, instant teleports between them, eye riding a baked ground patch). It
# is plain python with no engine imports precisely so the ANALYZER can import the
# same module and say where the eye was for any frame it scores.
#
# ENV KNOBS: see ork.bench.walk_path (WALK_SEGMENTS / WALK_SEG_SECONDS / WALK_RADIUS
# / WALK_SPEED / WALK_TURNS / WALK_EYE_HEIGHT / WALK_TOUR), plus
#   WALK_WITNESS_LOG  <path>: determinism witness (see below)
#
# THE WITNESS is what proves replayability. Lines are emitted on a FIXED sim-time
# grid (WITNESS_DT), computed AT the grid time rather than at the tick's actual
# gameTime, so two runs with different frame pacing still produce byte-identical
# files up to the shorter run's length:
#
#   <gameTime> <eye.x> <eye.y> <eye.z> <heading_deg> <segment> <lap>
#
# It is also the bench runner's SIM-IS-LIVE signal and its clock: the runner
# reads the last line to know how far along the tour the sim is (that, against
# the frame log, is how frames get attributed to locations).
###############################################################################
import math
import os

from orkengine.ecssim import *

from ork.bench import walk_path as WP

tokens = CrcStringProxy()

# camera, matching what the scene's walker publishes (scene/_terrain.py's walker_kw)
# so the bench measures the SHIPPED framing, not a bench-specific one.
CAM_NEAR = 0.5
CAM_FAR  = 100000.0
CAM_FOVY = 65.0 * math.pi / 180.0

WITNESS_DT = 0.25   # witness grid, sim seconds


class BenchWalk:
  def __init__(self):
    self.sys_sg = None
    self.witness = None
    path = os.environ.get("WALK_WITNESS_LOG", "")
    if path:
      self.witness = open(path, "w", buffering=1)
    self.witness_next = 0   # next grid index to emit
    self.seg_announced = -1


def onSystemInit(simulation):
  simulation.vars.bench_walk = BenchWalk()
  print("[bench_walk] %s" % WP.describe(), flush=True)
  for i, seg in enumerate(WP.SEGMENTS):
    m = seg["metrics"]
    print("[bench_walk]   seg %d %-15s %-16s xz=(%.1f,%.1f) ground=%.1f trees300=%d "
          "trees1250=%d relief100=%.1f prom=%.1f" % (
              i, seg["name"], seg["cls"], seg["xz"][0], seg["xz"][1], m["ground_m"],
              m["trees_300m"], m["trees_1250m"], m["relief_100m"], m["prominence_1km"]),
          flush=True)


def onSystemLink(simulation):
  B = simulation.vars.bench_walk
  B.sys_sg = simulation.findSystemByName("SceneGraphSystem")
  if B.sys_sg is None:
    raise RuntimeError("bench_walk: no SceneGraphSystem in this scene — nothing to drive")


def onSystemUpdate(simulation):
  B = simulation.vars.bench_walk
  t = simulation.gameTime
  S = WP.state(t)
  ex, ey, ez = S["eye"]
  dx, dy, dz = S["dir"]
  eye = vec3(ex, ey, ez)
  B.sys_sg.notify(tokens.UpdateCamera, {
      tokens.eye:  eye,
      tokens.tgt:  eye + vec3(dx, dy, dz),
      tokens.up:   vec3(0, 1, 0),
      tokens.near: CAM_NEAR,
      tokens.far:  CAM_FAR,
      tokens.fovy: CAM_FOVY,
  })

  # a TELEPORT is a discontinuity in the measurement, so it is announced in the run
  # log — a partial run stays scoreable because the reader can see which segments it
  # actually reached.
  key = S["lap"] * WP.NUM_SEGMENTS + S["seg"]
  if key != B.seg_announced:
    B.seg_announced = key
    print("[bench_walk] TELEPORT t=%.2f lap=%d seg=%d %s xz=(%.1f,%.1f) eye_y=%.1f" % (
        t, S["lap"], S["seg"], S["name"], ex, ez, ey), flush=True)

  if B.witness is not None:
    # emit every grid point the tick crossed (a long frame must not drop one, or the
    # two runs' witnesses would misalign by a line instead of by a suffix)
    while (B.witness_next * WITNESS_DT) <= t:
      gt = B.witness_next * WITNESS_DT
      G = WP.state(gt)
      B.witness.write("%.4f %.4f %.4f %.4f %.3f %d %d\n" % (
          gt, G["eye"][0], G["eye"][1], G["eye"][2], G["heading"], G["seg"], G["lap"]))
      B.witness_next += 1
