#!/usr/bin/env ork.python
###############################################################################
# ork.bench.walktour.py — BAKE THE GRAND TOUR the stereo walk bench walks.
#
#   obt.env.launch.py --stagedir ~/.staging-jul03 --project ~/orkid \
#     --command 'ork.bench.walktour.py --print'          # table only
#   ... --command 'ork.bench.walktour.py'                # rewrite the tour json
#
# WHY A BAKE instead of sampling at sim time: the terrain height is a 8192x8192
# EXR (233 MB) and the scatter is a 150k-point ScatterSet — neither is reachable
# from a sim script (no terrain-height query is pyexposed), and re-reading them
# per run would put a quarter-gigabyte of IO in front of every bench. The tour
# json this writes is the whole path definition: locations, their measured
# content, and a small ground patch per location. Re-run it after a terrain
# re-bake (the cookhash in the assetcache changes) — nothing else invalidates it.
#
# SEGMENT SELECTION IS EVIDENCE, NOT TASTE. Two measured fields drive it:
#   * the HEIGHT bake  -> local relief, walkable slope, prominence (how far the
#                         eye can see: elevation minus the 1 km neighbourhood)
#   * trees.ogeo       -> the ACTUAL 150000 placed tree positions (the placement
#                         artifact the renderer instances), counted in a 300 m
#                         box (near-field draw load) and a 1250 m box (the
#                         impostor LOD radius, i.e. the instanced far load)
# Each class below is an argmax/argmin of those fields under stated predicates,
# picked in order with a minimum separation so the tour cannot collapse onto one
# hillside. Two global constraints apply to every class:
#   * a border margin, so a segment's walk circle is never at the map edge
#   * an elevation ceiling BELOW the scene's cloud deck base — a camera inside
#     the cumulus deck is a different renderer regime, not a forest walk
###############################################################################

import argparse
import json
import math
import os
import sys

import numpy
from orkengine import lev2
from obt import path as obt_path

# Provenance paths recorded into walk_tour.json are rewritten onto these anchors,
# newest-first so the most specific prefix wins. Same "<token>/rest" convention the
# asset manifests already use (ork.data/asset_manifests/*.json). Without this the
# tour json records a fully-expanded absolute path — which pins the baking user's
# home directory into a file that is committed and read on other machines.
_PATH_ANCHORS = (
    ("<obt-global>", lambda: obt_path.user_global()),
    ("<stage>",      lambda: obt_path.stage()),
)


def portable_path(p):
  """Absolute path -> anchored token form, for values RECORDED into json.

  Falls back to ~-collapsed, then to the path itself: provenance is best-effort
  and must never fail a bake."""
  try:
    ap = os.path.abspath(os.path.expanduser(str(p)))
    for token, get in _PATH_ANCHORS:
      try:
        root = os.path.abspath(str(get()))
      except Exception:
        continue
      if root and (ap == root or ap.startswith(root + os.sep)):
        return token + ap[len(root):]
    home = os.path.expanduser("~")
    if ap == home or ap.startswith(home + os.sep):
      return "~" + ap[len(home):]
    return ap
  except Exception:
    return str(p)


# the scene's own walker spawn (content/forest.py) — segment 0 is the reference
# location, so the tour stays comparable with the single-basin bench that
# preceded it.
ANCHOR_XZ = (-8398.7, 7889.3)
# scn_forest_procsky's ORK_FORESTSKY_CLOUDBASE default, meters ASL.
CLOUD_BASE_M = 1850.0


def parse_args():
  ap = argparse.ArgumentParser(description="bake the bench walk's grand tour")
  ap.add_argument("--terrain", default=os.path.expanduser(
      "~/.obt-global/assetcache/terrain/forest_terra"),
      help="baked terrain directory (height.exr + <sink>.ogeo)")
  ap.add_argument("--sink", default="trees", help="scatter sink name (default trees)")
  ap.add_argument("--out", default=os.path.join(
      os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
      "scripts", "ork", "bench", "walk_tour.json"),
      help="tour json to write")
  ap.add_argument("--coarse", type=int, default=2048,
                  help="analysis grid resolution (default 2048 = 16 m/cell)")
  ap.add_argument("--patch-half", type=float, default=80.0,
                  help="ground patch half-extent per segment, meters (default 80)")
  ap.add_argument("--patch-spacing", type=float, default=5.0,
                  help="ground patch sample spacing, meters (default 5)")
  ap.add_argument("--min-separation", type=float, default=1500.0,
                  help="minimum distance between segments, meters (default 1500)")
  ap.add_argument("--border", type=float, default=1200.0,
                  help="map-edge exclusion, meters (default 1200)")
  ap.add_argument("--print", dest="only_print", action="store_true",
                  help="print the table without writing the json")
  return ap.parse_args()


def load_height(terrain_dir):
  path = os.path.join(terrain_dir, "height.exr")
  if not os.path.exists(path):
    raise SystemExit("ork.bench.walktour: no height bake at %s" % path)
  img = lev2.Image.createFromFile(path)
  arr = numpy.array(img.numpy, dtype=numpy.float32)
  if arr.ndim == 3:
    arr = arr[..., 0]
  return arr


def load_scatter(terrain_dir, sink, extent):
  """The ScatterSet .ogeo is a chunkfile whose DATA payload is a
     STRUCTURE-OF-ARRAYS of the fields its header names, in that order:
     P(3f) proxy_dims(3f) proxy_kind(i) type_id(i) variant_seed(i) xform(16f)
     = 25 words per point. Only P and type_id are read here.

     The chunkfile header is a variable-length name table, so its length is not
     assumed: every offset that leaves a whole number of points is decoded and
     REJECTED unless the arrays land in range (positions inside the terrain
     extent, type ids inside a byte). A layout change fails loudly here rather
     than quietly selecting a tour from garbage."""
  path = os.path.join(terrain_dir, "%s.ogeo" % sink)
  if not os.path.exists(path):
    raise SystemExit("ork.bench.walktour: no scatter artifact at %s" % path)
  raw = numpy.fromfile(path, dtype=numpy.uint8)
  words = 25
  for hdr in range(64, 1024):
    if (raw.size - hdr) % (words * 4):
      continue
    n = (raw.size - hdr) // (words * 4)
    f = raw[hdr:].view(numpy.float32)
    i = raw[hdr:].view(numpy.int32)
    pos = f[:n * 3].reshape(n, 3)
    tid = i[7 * n:8 * n]              # P(3n) proxy_dims(3n) proxy_kind(n) | type_id(n)
    if (numpy.isfinite(pos).all() and abs(pos[:, 0]).max() <= 0.5 * extent
        and abs(pos[:, 2]).max() <= 0.5 * extent and tid.min() >= 0 and tid.max() < 256):
      return pos, tid
  raise SystemExit("ork.bench.walktour: %s does not decode as a ScatterSet — the .ogeo "
                   "layout changed; re-read the chunkfile header" % path)


def maxfilt(a, r):
  b = numpy.maximum.reduce([numpy.roll(a, d, 0) for d in range(-r, r + 1)])
  return numpy.maximum.reduce([numpy.roll(b, d, 1) for d in range(-r, r + 1)])


def minfilt(a, r):
  b = numpy.minimum.reduce([numpy.roll(a, d, 0) for d in range(-r, r + 1)])
  return numpy.minimum.reduce([numpy.roll(b, d, 1) for d in range(-r, r + 1)])


def boxsum(a, r):
  ii = numpy.pad(a.astype(numpy.float64), ((1, 0), (1, 0))).cumsum(0).cumsum(1)
  R, C = a.shape
  rr = numpy.arange(R)
  cc = numpy.arange(C)
  r0 = numpy.clip(rr - r, 0, R)
  r1 = numpy.clip(rr + r + 1, 0, R)
  c0 = numpy.clip(cc - r, 0, C)
  c1 = numpy.clip(cc + r + 1, 0, C)
  return (ii[numpy.ix_(r1, c1)] - ii[numpy.ix_(r0, c1)]
          - ii[numpy.ix_(r1, c0)] + ii[numpy.ix_(r0, c0)]).astype(numpy.float32)


def boxmean(a, r):
  return boxsum(a, r) / boxsum(numpy.ones_like(a), r)


def main():
  args = parse_args()
  meta_path = None
  for name in os.listdir(args.terrain):
    if name.endswith(".terrain.json"):
      meta_path = os.path.join(args.terrain, name)
  if meta_path is None:
    raise SystemExit("ork.bench.walktour: no <asset>.terrain.json in %s" % args.terrain)
  meta = json.load(open(meta_path))
  extent = float(meta["scale"]["extent_m"])
  dim = int(meta["scale"]["dim"])

  H = load_height(args.terrain)
  if H.shape != (dim, dim):
    raise SystemExit("ork.bench.walktour: height.exr is %s, manifest says %d" % (
        H.shape, dim))
  pos, tid = load_scatter(args.terrain, args.sink, extent)
  print("terrain %s  extent %.0fm dim %d  height %.1f..%.1f  scatter %d points, %d types" % (
      os.path.basename(args.terrain), extent, dim, H.min(), H.max(), len(pos),
      int(tid.max()) + 1), flush=True)

  # ---- analysis grid -------------------------------------------------------
  D = args.coarse
  if dim % D:
    raise SystemExit("ork.bench.walktour: --coarse %d does not divide the bake dim %d" % (D, dim))
  f = dim // D
  h = H.reshape(D, f, D, f).mean((1, 3)).astype(numpy.float32)
  mpc = extent / D            # meters per coarse cell

  cx = ((pos[:, 0] / extent + 0.5) * D).astype(numpy.int32).clip(0, D - 1)
  cz = ((pos[:, 2] / extent + 0.5) * D).astype(numpy.int32).clip(0, D - 1)
  cnt = numpy.zeros((D, D), numpy.float32)
  numpy.add.at(cnt, (cz, cx), 1.0)

  n300  = boxsum(cnt, int(300.0 / mpc))     # near-field trees
  n1250 = boxsum(cnt, int(1250.0 / mpc))    # trees inside the impostor LOD radius
  r100 = int(100.0 / mpc)
  relief100 = maxfilt(h, r100) - minfilt(h, r100)
  walk50 = maxfilt(h, 1) - minfilt(h, 1)    # local slope over ~3 cells
  prom = h - boxmean(h, int(1000.0 / mpc))  # how much the eye stands above its km
  redge = int(100.0 / mpc)
  edge = maxfilt(n300, redge) - minfilt(n300, redge)   # forest boundary contrast

  def cell_of(x, z):
    return (int((z / extent + 0.5) * D), int((x / extent + 0.5) * D))

  def world_of(r, c):
    return (((c + 0.5) / D - 0.5) * extent, ((r + 0.5) / D - 0.5) * extent)

  M = int(args.border / mpc)
  inb = numpy.zeros((D, D), bool)
  inb[M:D - M, M:D - M] = True
  LOW = h < (CLOUD_BASE_M - 100.0)
  WALK = walk50 < 25.0

  chosen = []

  def pick(name, cls, mask, score, why):
    cand = numpy.argwhere(mask & inb)
    if len(cand) == 0:
      raise SystemExit("ork.bench.walktour: no candidate satisfies '%s' — the terrain bake "
                       "changed shape; re-tune the predicate" % name)
    s = score[cand[:, 0], cand[:, 1]]
    for idx in numpy.argsort(-s, kind="stable"):
      r, c = int(cand[idx][0]), int(cand[idx][1])
      x, z = world_of(r, c)
      if all((x - p["xz"][0]) ** 2 + (z - p["xz"][1]) ** 2 > args.min_separation ** 2
             for p in chosen):
        chosen.append(dict(name=name, cls=cls, xz=(x, z), rc=(r, c), why=why))
        return
    raise SystemExit("ork.bench.walktour: every '%s' candidate is within %.0fm of an "
                     "already-chosen segment" % (name, args.min_separation))

  ar, ac = cell_of(*ANCHOR_XZ)
  chosen.append(dict(name="spawn_basin", cls="reference", xz=ANCHOR_XZ, rc=(ar, ac),
                     why="the scene's own walker spawn — the reference location, kept so "
                         "this tour stays comparable with the single-basin bench"))
  pick("dense_near", "dense stand",
       LOW & WALK & (n1250 > 1200), n300,
       "most trees within 300 m of any walkable low-elevation cell — the heaviest "
       "near-field instanced draw on the map")
  pick("dense_deep", "dense stand",
       LOW & WALK & (n300 > 60), n1250,
       "most trees inside the 1250 m impostor radius — the deepest forest column, "
       "near-field cost plus the largest far-instance set")
  pick("sparse_open", "open ground",
       LOW & WALK & (n1250 > 1200), -n300,
       "FEWEST trees within 300 m while still surrounded by forest at distance — the "
       "map's plantable band has no true clearing, so this is its open ground")
  pick("slope_treed", "elevation change",
       LOW & (walk50 > 12.0) & (walk50 < 30.0) & (n300 > 40), relief100,
       "the biggest relief inside 100 m that a walker can still climb, with trees on it "
       "— the ground-following leg")
  pick("ridge_view", "long sightline",
       LOW & WALK & (n1250 > 600), prom,
       "highest prominence over its 1 km neighbourhood that still sits under the cloud "
       "deck and above forest — the long view out")
  pick("gorge_occluded", "long sightline",
       LOW & WALK, -prom,
       "deepest cell relative to its 1 km neighbourhood — the terrain-occluded opposite "
       "of the ridge, where most of the map is hidden")
  pick("treeline_edge", "open ground",
       LOW & WALK & (n300 > 15) & (n300 < 90) & (prom > 50.0), edge,
       "sharpest forest/bare boundary — half the sweep looks into canopy, half into "
       "open rock, in one location")

  # ---- ground patches ------------------------------------------------------
  # lifted at FULL bake resolution, MAX-filtered over a cell so the eye rides above
  # the surface rather than grazing it, then smoothed so the walk has no steps.
  npatch = int(round(2.0 * args.patch_half / args.patch_spacing)) + 1
  half = 0.5 * (npatch - 1) * args.patch_spacing
  tpm = dim / extent                     # texels per meter
  for seg in chosen:
    x0, z0 = seg["xz"]
    g = numpy.empty((npatch, npatch), numpy.float64)
    for jz in range(npatch):
      wz = z0 - half + jz * args.patch_spacing
      tz = int(round((wz / extent + 0.5) * dim))
      for jx in range(npatch):
        wx = x0 - half + jx * args.patch_spacing
        tx = int(round((wx / extent + 0.5) * dim))
        z1 = max(tz - 1, 0)
        z2 = min(tz + 2, dim)
        x1 = max(tx - 1, 0)
        x2 = min(tx + 2, dim)
        g[jz, jx] = H[z1:z2, x1:x2].max()
    for _ in range(2):                   # separable [1,2,1], twice
      g = (numpy.pad(g, ((1, 0), (0, 0)), mode="edge")[:-1] + 2.0 * g
           + numpy.pad(g, ((0, 1), (0, 0)), mode="edge")[1:]) * 0.25
      g = (numpy.pad(g, ((0, 0), (1, 0)), mode="edge")[:, :-1] + 2.0 * g
           + numpy.pad(g, ((0, 0), (0, 1)), mode="edge")[:, 1:]) * 0.25
    seg["ground"] = [[round(float(v), 2) for v in row] for row in g]
    r, c = seg["rc"]
    seg["metrics"] = dict(
        ground_m=round(float(g[npatch // 2, npatch // 2]), 1),
        trees_300m=int(n300[r, c]), trees_1250m=int(n1250[r, c]),
        relief_100m=round(float(relief100[r, c]), 1),
        slope_50m=round(float(walk50[r, c]), 1),
        prominence_1km=round(float(prom[r, c]), 1),
        patch_relief=round(float(g.max() - g.min()), 1))

  hdr = "%-15s %-16s %11s %11s %8s %7s %8s %8s %7s %7s" % (
      "segment", "class", "x", "z", "ground", "tr300", "tr1250", "relief", "slope", "prom")
  print(hdr)
  print("-" * len(hdr))
  for seg in chosen:
    m = seg["metrics"]
    print("%-15s %-16s %11.1f %11.1f %8.1f %7d %8d %8.1f %7.1f %7.1f" % (
        seg["name"], seg["cls"], seg["xz"][0], seg["xz"][1], m["ground_m"],
        m["trees_300m"], m["trees_1250m"], m["relief_100m"], m["slope_50m"],
        m["prominence_1km"]))

  if args.only_print:
    return 0

  tour = dict(
      terrain=dict(dir=portable_path(args.terrain), extent_m=extent, dim=dim, sink=args.sink,
                   scatter_points=int(len(pos))),
      patch=dict(spacing_m=args.patch_spacing, dim=npatch, half_extent_m=half,
                 note="max-filtered over one bake texel then smoothed twice; sample with "
                      "Catmull-Rom (ork.bench.walk_path.ground)"),
      segments=[dict(name=s["name"], cls=s["cls"], xz=[round(s["xz"][0], 1), round(s["xz"][1], 1)],
                     why=s["why"], metrics=s["metrics"], ground=s["ground"])
                for s in chosen])
  # one line per segment: the ground patches are 1089 numbers each, and a
  # pretty-printed dump would make every re-bake a 9000-line diff.
  with open(args.out, "w") as f:
    f.write("{\n \"terrain\": %s,\n \"patch\": %s,\n \"segments\": [\n" % (
        json.dumps(tour["terrain"]), json.dumps(tour["patch"])))
    for i, seg in enumerate(tour["segments"]):
      f.write("  %s%s\n" % (json.dumps(seg), "," if i + 1 < len(tour["segments"]) else ""))
    f.write(" ]\n}\n")
  print("wrote %s (%d segments, %dx%d ground patches)" % (
      args.out, len(chosen), npatch, npatch))
  return 0


if __name__ == "__main__":
  sys.exit(main())
