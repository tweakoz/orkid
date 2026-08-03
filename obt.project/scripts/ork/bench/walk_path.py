###############################################################################
# ork.bench.walk_path — THE TOUR PATH, as pure python, so the SIM SCRIPT that flies
# the camera and the ANALYZER that scores the frames agree on where the eye was BY
# CONSTRUCTION rather than by two implementations that drift apart. Everything here
# is a PURE FUNCTION of simulation.gameTime.
#
# It lives in ork/bench rather than beside the bench scene scripts because importing
# ork.hypergraph.ecs.scene runs the whole scene library (and needs a project env):
# this module has NO engine imports, and a plain-python analyzer must be able to
# import it anywhere.
#
# THE TOUR is a segmented grand tour of the map: K segments at DIVERSE locations
# (dense stands, sparse ground, elevation change, long sightlines — see the
# baked tour's per-segment rationale), each walked for SEG_SECONDS, with an
# INSTANT teleport between them. Inside a segment the eye walks a small circle at
# human pace WHILE the view yaw sweeps a full turn, so every segment reports the
# cost of its location across ALL directions — "framerate depends on location"
# and "direction matters also" are separable in the result.
#
# GROUND FOLLOWING: a fixed altitude cannot survive a tour that changes elevation
# by 1000 m. Each segment carries a BAKED GROUND PATCH (a local heightfield
# window lifted from the terrain bake by ork.bench.walktour.py: max-filtered then
# smoothed, so the eye rides above the surface instead of grazing it) sampled
# here with Catmull-Rom, which is C1 — a bilinear patch would kink the eye's
# vertical velocity once per cell and read as a stutter, not a walk.
#
# ENV KNOBS (all optional; the defaults ARE the bench)
#   WALK_TOUR          tour json (default walk_tour.json beside this file)
#   WALK_SEGMENTS      how many of the tour's segments to walk (default: all)
#   WALK_SEG_SECONDS   seconds per segment (default 40)
#   WALK_TURNS         view-yaw turns per segment (default 1.0)
#   WALK_HEADINGS      heading bins the analyzer reports (default 8) — with the
#                      default turn rate that is SEG_SECONDS/8 = 5 s per bin,
#                      i.e. every bin holds several rolling-1s windows
#   WALK_RADIUS        local walk-circle radius, meters (default 20)
#   WALK_SPEED         walk speed along that circle, m/s (default 1.8)
#   WALK_EYE_HEIGHT    eye above the baked ground, meters (default 1.7)
#
# The tour LOOPS: t beyond K*SEG_SECONDS starts lap 1 of the same path, so a
# longer run stays comparable to a shorter one segment for segment.
###############################################################################
import json
import math
import os

_HERE = os.path.dirname(os.path.abspath(__file__))


def _envf(key, default):
  try:
    return float(os.environ.get(key, default))
  except ValueError:
    return float(default)


def _envi(key, default):
  try:
    return int(os.environ.get(key, default))
  except ValueError:
    return int(default)


TOUR_PATH   = os.environ.get("WALK_TOUR", os.path.join(_HERE, "walk_tour.json"))
SEG_SECONDS = max(1.0, _envf("WALK_SEG_SECONDS", 40.0))
TURNS       = _envf("WALK_TURNS", 1.0)
HEADINGS    = max(1, _envi("WALK_HEADINGS", 8))
RADIUS      = _envf("WALK_RADIUS", 20.0)
SPEED       = _envf("WALK_SPEED", 1.8)
EYE_HEIGHT  = _envf("WALK_EYE_HEIGHT", 1.7)

with open(TOUR_PATH) as _f:
  TOUR = json.load(_f)

_all_segments = TOUR["segments"]
_want = _envi("WALK_SEGMENTS", len(_all_segments))
if _want < 1 or _want > len(_all_segments):
  raise ValueError("ork.bench.walk_path: WALK_SEGMENTS=%d out of range — %s has %d segments" % (
      _want, TOUR_PATH, len(_all_segments)))
SEGMENTS = _all_segments[:_want]
NUM_SEGMENTS = len(SEGMENTS)
TOUR_SECONDS = NUM_SEGMENTS * SEG_SECONDS

PATCH_SPACING = float(TOUR["patch"]["spacing_m"])
PATCH_DIM     = int(TOUR["patch"]["dim"])
PATCH_HALF    = 0.5 * (PATCH_DIM - 1) * PATCH_SPACING

# Catmull-Rom needs one ring of neighbours outside the sampled point, so the walk
# circle has to stay a cell inside the baked window. A radius that overruns it would
# silently clamp the eye onto the patch border and walk a wall — refuse instead.
if RADIUS + 2.0 * PATCH_SPACING > PATCH_HALF:
  raise ValueError(
      "ork.bench.walk_path: WALK_RADIUS=%.1fm exceeds the baked ground patch (half-extent "
      "%.1fm, spacing %.1fm) — re-bake with a larger radius via ork.bench.walktour.py "
      "--patch-half" % (RADIUS, PATCH_HALF, PATCH_SPACING))


def _cr(p0, p1, p2, p3, t):
  """Catmull-Rom on a uniform grid."""
  t2 = t * t
  t3 = t2 * t
  return 0.5 * ((2.0 * p1) + (-p0 + p2) * t + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3)


def ground(seg, x, z):
  """baked ground elevation under (x,z), meters ASL."""
  g  = seg["ground"]
  fx = (x - seg["xz"][0] + PATCH_HALF) / PATCH_SPACING
  fz = (z - seg["xz"][1] + PATCH_HALF) / PATCH_SPACING
  ix = int(math.floor(fx))
  iz = int(math.floor(fz))
  tx = fx - ix
  tz = fz - iz
  cols = []
  for dz in range(-1, 3):
    jz = min(max(iz + dz, 0), PATCH_DIM - 1)
    row = g[jz]
    p = []
    for dx in range(-1, 3):
      jx = min(max(ix + dx, 0), PATCH_DIM - 1)
      p.append(row[jx])
    cols.append(_cr(p[0], p[1], p[2], p[3], tx))
  return _cr(cols[0], cols[1], cols[2], cols[3], tz)


def state(t):
  """THE PATH. Returns a dict describing the eye at sim time t — pure, replayable.

     seg/lap/t_seg locate the tour, eye is the camera position, dir is the unit
     look direction (level), heading is that direction as a compass angle with
     +Z = 0 deg and +X = 90 deg."""
  if t < 0.0:
    t = 0.0
  lap   = int(t // TOUR_SECONDS)
  ttour = t - lap * TOUR_SECONDS
  si    = min(int(ttour // SEG_SECONDS), NUM_SEGMENTS - 1)
  tseg  = ttour - si * SEG_SECONDS
  seg   = SEGMENTS[si]

  # local walk: a circle at human pace, phase 0 at the segment's start
  a  = (SPEED * tseg) / RADIUS
  x  = seg["xz"][0] + RADIUS * math.sin(a)
  z  = seg["xz"][1] + RADIUS * math.cos(a)
  y  = ground(seg, x, z) + EYE_HEIGHT

  # view: a full sweep of the compass per segment, independent of the motion
  phi = 2.0 * math.pi * TURNS * (tseg / SEG_SECONDS)
  dx  = math.sin(phi)
  dz  = math.cos(phi)
  return {
      "t": t, "lap": lap, "seg": si, "t_seg": tseg, "name": seg["name"],
      "eye": (x, y, z), "dir": (dx, 0.0, dz),
      "heading": math.degrees(phi) % 360.0,
  }


def heading_bin(heading):
  """index of the heading bin (0 spans the compass origin)."""
  width = 360.0 / HEADINGS
  return int(((heading + 0.5 * width) % 360.0) // width)


_COMPASS8 = ("N", "NE", "E", "SE", "S", "SW", "W", "NW")


def heading_label(idx):
  """+Z is called N here purely so the tables read like directions."""
  if HEADINGS == 8:
    return _COMPASS8[idx % 8]
  return "h%d" % idx


def env_state():
  """the knobs that DEFINE this path, as the environment that reproduces it — a run
     log stamps these so a later re-analysis rebuilds the same tour instead of
     silently scoring the frames against the default one."""
  return [("WALK_TOUR", TOUR_PATH), ("WALK_SEGMENTS", "%d" % NUM_SEGMENTS),
          ("WALK_SEG_SECONDS", "%.4f" % SEG_SECONDS), ("WALK_TURNS", "%.4f" % TURNS),
          ("WALK_HEADINGS", "%d" % HEADINGS), ("WALK_RADIUS", "%.4f" % RADIUS),
          ("WALK_SPEED", "%.4f" % SPEED), ("WALK_EYE_HEIGHT", "%.4f" % EYE_HEIGHT)]


def describe():
  return ("tour=%s segments=%d seg_seconds=%.1f tour_seconds=%.1f radius=%.1fm speed=%.2fm/s "
          "eye_height=%.2fm turns=%.2f headings=%d" % (
              os.path.basename(TOUR_PATH), NUM_SEGMENTS, SEG_SECONDS, TOUR_SECONDS,
              RADIUS, SPEED, EYE_HEIGHT, TURNS, HEADINGS))
