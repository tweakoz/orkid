#!/usr/bin/env python3
###############################################################################
# COINCIDENT LEAF CARDS — the mesh-level gate on LeafScatter's phyllotaxis spray.
#
# Card j of a spray sits at azimuth j*roll about the node heading. Any divergence
# that divides 360 closes the spiral, so past 360/roll cards two of them land on
# the SAME base point, near-coplanar, separated only by jitter. In mono that is
# invisible wasted fill; in stereo it is a two-tone flicker (the pair carries two
# different per-leaf hashes, and each eye's projection resolves the depth tie on
# its own). The conifer shipped exactly that: roll 90 x per_node 5 aliased card 4
# onto card 0 at all 151 of its nodes (755 cards, 151 of them redundant).
#
# Checks, on the REAL shipping archetypes (no GPU render, just the bake):
#   1. every placement site carries DISTINCT azimuths — no site holds two
#      near-coplanar overlapping cards (the z-fight pair)
#   2. the scatter DEFENDS ITSELF: a conifer authored with the aliasing
#      per_node 5 still emits 4 cards per site, not 5
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, math, tempfile
from collections import defaultdict
from orkengine import core
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene.content.forest import Broadleaf, Conifer

LEAF_GID = 2      # ls_anim bakes the leaf cards into gid 2
SITE_EPS = 1e-3   # metres — cards of one spray share a base point exactly
PAR_MIN  = 0.85   # |n.n'| above this = the two cards face the same way
PLANE_M  = 0.12   # metres of depth separation below this = they fight for it

TMP = tempfile.mkdtemp(prefix="leafcoinc_")


def _sub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def _dot(a, b): return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
def _cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def _norm(a):
  m = math.sqrt(_dot(a, a)) or 1.0
  return (a[0]/m, a[1]/m, a[2]/m)


def _bake(ctx, cls, seed, label, **overrides):
  """materialize the archetype, read its cards back (OBJ + face gids) as (base, normal, quad)."""
  mesh = cls(seed=seed, **overrides).materialize(ctx)
  path = os.path.join(TMP, label + ".obj")
  lev2.hypermesh.dump_obj(mesh, ctx, path)
  gids = [(int(t) >> 20) & 0xFFF for t in lev2.hypermesh.read_face_tags(mesh, ctx)]
  V, F = [], []
  for line in open(path):
    if line.startswith("v "):
      p = line.split(); V.append((float(p[1]), float(p[2]), float(p[3])))
    elif line.startswith("f "):
      F.append([int(t.split("/")[0]) - 1 for t in line.split()[1:]])
  assert len(gids) == len(F), "face-tag count %d != OBJ face count %d" % (len(gids), len(F))
  cards = []
  for f, g in zip(F, gids):
    if g != LEAF_GID:
      continue
    pts = [V[i] for i in f]
    # LeafScatter winds each card petiole-L, petiole-R, tip-R, tip-L: corners 0,1 are the base edge
    base = tuple((pts[0][k] + pts[1][k]) * 0.5 for k in range(3))
    cards.append((base, _norm(_cross(_sub(pts[1], pts[0]), _sub(pts[2], pts[0]))), pts))
  return cards


def _coincident(cards):
  """pairs of cards on one base point that are near-coplanar AND overlapping."""
  sites = defaultdict(list)
  for i, (b, _n, _p) in enumerate(cards):
    sites[tuple(round(c / SITE_EPS) for c in b)].append(i)
  hits = []
  for idxs in sites.values():
    for a in range(len(idxs)):
      for b in range(a + 1, len(idxs)):
        i, j = idxs[a], idxs[b]
        if abs(_dot(cards[i][1], cards[j][1])) < PAR_MIN:
          continue
        sep = max(abs(_dot(_sub(p, cards[i][0]), cards[i][1])) for p in cards[j][2])
        if sep <= PLANE_M:
          hits.append((i, j, sep))
  return sites, hits


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  fails = []
  # --- 1. the shipping archetypes carry no coincident cards ---
  for label, cls, seed in (("broadleaf", Broadleaf, 100), ("conifer", Conifer, 150)):
    cards = _bake(ctx, cls, seed, label)
    sites, hits = _coincident(cards)
    print("%s: cards=%d sites=%d coincident_pairs=%d"
          % (label, len(cards), len(sites), len(hits)), flush=True)
    if not cards:
      fails.append("%s baked ZERO leaf cards (gid %d) — the canopy is missing" % (label, LEAF_GID))
    if hits:
      fails.append("%s has %d coincident card pairs (first sep=%.2fmm)"
                   % (label, len(hits), hits[0][2] * 1000.0))

  # --- 2. the scatter drops an authored aliasing spray instead of stacking it ---
  cards = _bake(ctx, Conifer, 150, "conifer_alias5", leaf_per_node=5)
  sites, hits = _coincident(cards)
  per = sorted({len(v) for v in sites.values()})
  print("conifer(per_node=5, roll=90): cards=%d sites=%d cards/site=%s coincident_pairs=%d"
        % (len(cards), len(sites), per, len(hits)), flush=True)
  if per != [4]:
    fails.append("aliasing spray emitted %s cards/site — expected [4] (card 4 repeats card 0 at roll 90)" % (per,))
  if hits:
    fails.append("aliasing spray still stacks %d coincident pairs" % len(hits))

  ezapp.mainThreadEnd()
  ecs.headless_exit()
  for f in fails:
    print("FAIL: " + f, flush=True)
  print("LEAF_CARD_COINCIDENCE_RESULT=%s" % ("PASS" if not fails else "FAIL"), flush=True)
  sys.exit(0 if not fails else 1)


main()
