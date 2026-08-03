#!/usr/bin/env python3
###############################################################################
# meshlet PARTITIONER gate, CPU only — no device, no window, no scene. The partitioner is pure
# index arithmetic, so its whole contract is checkable on any machine (including one with no GPU
# access at all), and a device-bound gate is the wrong place to learn that the packing regressed.
#
# What it proves, over synthetic topologies whose right answer is known by construction:
#   1. coverage + caps  — every triangle in exactly one bucket, <=256 verts / <=256 prims
#   2. determinism      — repeated builds are byte-identical
#   3. slicing          — a small per-slice budget converges to the single-shot partition
#   4. append           — prefix-preserving growth carries prior buckets VERBATIM; a churned prefix
#                         is detected and forces a full rebuild
#   5. ISLAND PACKING   — a mesh of many DISCONNECTED islands still fills its buckets. This is the
#                         organic-content case (a swept tube or branchy plant is hundreds of small
#                         adjacency islands: rings, segments, caps). A partitioner that closes a
#                         bucket when the adjacency frontier runs dry gives one island per bucket
#                         and ~5% fill; only the CAPS may close a bucket.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from collections import Counter

from orkengine import core   # core before lev2
from orkengine import lev2

hm = lev2.hypermesh

MAX_VERTS = hm.meshlet_max_verts   # live builder caps (platform-derived, see meshlet.h)
MAX_PRIMS = hm.meshlet_max_prims


###############################################################################
# synthetic topologies
###############################################################################

def grid_indices(n):
  """one connected sheet: an (n-1)^2 quad grid, fan-triangulated per quad."""
  idx = []
  for y in range(n - 1):
    for x in range(n - 1):
      a = y * n + x
      idx += [a, a + 1, a + n + 1]
      idx += [a, a + n + 1, a + n]
  return idx, n * n


def island_indices(islands, ring):
  """`islands` DISCONNECTED closed bands, each `ring` segments (2*ring verts, 2*ring tris). No
  vertex is shared between islands — the adjacency frontier dies at every island boundary."""
  idx = []
  vbase = 0
  for _ in range(islands):
    for s in range(ring):
      a0 = vbase + s
      a1 = vbase + (s + 1) % ring
      b0 = a0 + ring
      b1 = a1 + ring
      idx += [a0, a1, b1]
      idx += [a0, b1, b0]
    vbase += 2 * ring
  return idx, vbase


###############################################################################

def bucket_tris(part):
  vl, pi = part.vertex_list, part.prim_indices
  out = []
  for (voff, vcnt, poff, pcnt) in part.descriptors:
    for p in range(pcnt):
      k = pi[poff + p]
      a, b, c = k & 255, (k >> 8) & 255, (k >> 16) & 255
      assert a < vcnt and b < vcnt and c < vcnt, "local prim index outside its bucket vertex list"
      out.append((vl[voff + a], vl[voff + b], vl[voff + c]))
  return out


def src_tris(topo):
  i = topo.tri_indices
  return [(i[k], i[k + 1], i[k + 2]) for k in range(0, len(i), 3)]


def check_caps(part):
  for (voff, vcnt, poff, pcnt) in part.descriptors:
    assert 0 < vcnt <= MAX_VERTS, f"vertex cap violated: {vcnt}"
    assert 0 < pcnt <= MAX_PRIMS, f"prim cap violated: {pcnt}"
    assert voff + vcnt <= len(part.vertex_list), "vertex slice overruns the vertex list"
    assert poff + pcnt <= len(part.prim_indices), "prim slice overruns the prim index array"


def raw(part):
  return (list(part.descriptors), list(part.vertex_list), list(part.prim_indices))


def achievable_fill(verts_per_unit, tris_per_unit):
  """(vertex_fill, prim_fill, units_per_bucket) reachable for content made of indivisible units of
  this size, at the LIVE caps. Whole units only — an island shares no vertex with its neighbours,
  so a bucket holds floor() of them and the leftover cap is unreachable, not wasted.

  Derived, never a constant: the caps are platform-budget-derived (64/128 on Metal vs 256/256
  elsewhere — see meshlet.h), and the two axes bind DIFFERENTLY. At 64 verts / 128 prims, 14-vertex
  islands run out of vertex cap after 4 units = 56 prims, so 0.44 prim fill IS full packing there
  while the same content reaches 0.98 at 256/256. A fixed floor tests the platform, not the code."""
  fit = min(MAX_VERTS // verts_per_unit, MAX_PRIMS // tris_per_unit)
  assert fit >= 1, "a single unit does not fit the caps — the synthetic content is mis-sized"
  return (float(fit * verts_per_unit) / MAX_VERTS,
          float(fit * tris_per_unit) / MAX_PRIMS,
          fit)


G = {}


###############################################################################

def test_coverage_and_caps():
  idx, nv = grid_indices(64)
  topo = hm.meshletTopologyFromIndices(idx, nv)
  G["grid_topo"] = topo
  part = hm.meshletBuild(topo)
  G["grid_part"] = part
  check_caps(part)
  assert Counter(bucket_tris(part)) == Counter(src_tris(topo)), \
      "bucketed triangles are not exactly the source triangles (dropped/duplicated)"
  assert part.topology.snapshot_id == topo.snapshot_id, "partition lost its topology pairing"
  assert part.immutable_count == 0, "a from-scratch build must carry nothing"
  print(f"  grid: {topo.num_tris} tris -> {part.meshlet_count} buckets, each placed exactly once")
  return True


def test_determinism():
  a = hm.meshletBuild(G["grid_topo"])
  b = hm.meshletBuild(G["grid_topo"])
  assert raw(a) == raw(b), "two builds of one snapshot diverged"
  assert raw(a) == raw(G["grid_part"]), "build diverged from the coverage test's partition"
  print(f"  identical across 3 builds ({len(a.vertex_list)} vertex refs)")
  return True


def test_resumable_slicing():
  b = hm.meshletBuilder(G["grid_topo"])
  steps, last = 0, 0.0
  while b.step(64):
    steps += 1
    assert b.progress >= last, "progress went backwards"
    last = b.progress
    assert not b.done and b.partition is None, "partition visible before the build finished"
    assert steps < 100000, "slicing failed to converge"
  assert b.done and b.progress == 1.0
  assert raw(b.partition) == raw(G["grid_part"]), "sliced build diverged from the single-shot build"
  print(f"  converged in {steps + 1} slices of 64 tris, identical to the single-shot partition")
  return True


def test_append_fast_path():
  topo = G["grid_topo"]
  idx  = topo.tri_indices
  k    = (topo.num_tris * 3) // 5
  base  = hm.meshletTopologyFromIndices(idx[:k * 3], topo.num_verts)
  pbase = hm.meshletBuild(base)
  grown = hm.meshletTopologyFromIndices(idx, topo.num_verts)
  assert grown.isAppendOf(base), "grown snapshot should read as an append of the partial one"
  b = hm.meshletBuilder(grown, pbase)
  assert b.append_path and b.start_tri == k, f"append path not taken ({b.append_path}, {b.start_tri})"
  while b.step(512):
    pass
  pg = b.partition
  check_caps(pg)
  nb = pbase.meshlet_count
  assert pg.immutable_count == nb, f"carried {pg.immutable_count} buckets, expected {nb}"
  assert list(pg.descriptors)[:nb] == list(pbase.descriptors), "carried descriptors changed"
  assert list(pg.vertex_list)[:len(pbase.vertex_list)] == list(pbase.vertex_list), \
      "carried vertex lists changed"
  assert list(pg.prim_indices)[:len(pbase.prim_indices)] == list(pbase.prim_indices), \
      "carried prim indices changed"
  assert pg.meshlet_count > nb, "growth appended no new buckets"
  assert Counter(bucket_tris(pg)) == Counter(src_tris(grown)), \
      "appended partition does not cover the grown triangle list exactly"
  churn = list(idx)
  churn[0:3], churn[3:6] = churn[3:6], churn[0:3]
  ct = hm.meshletTopologyFromIndices(churn, topo.num_verts)
  assert not ct.isAppendOf(base), "a reordered prefix must not read as an append"
  cb = hm.meshletBuilder(ct, pbase)
  assert (not cb.append_path) and cb.start_tri == 0, "churn must force a full rebuild"
  print(f"  {nb} buckets carried verbatim, {pg.meshlet_count - nb} appended; churn rebuilds")
  return True


def test_island_packing():
  """the organic-content case: many disconnected islands must still fill their buckets."""
  ISLANDS, RING = 240, 7          # 14 verts / 14 tris per island, 3360 tris total
  idx, nv = island_indices(ISLANDS, RING)
  topo = hm.meshletTopologyFromIndices(idx, nv)
  part = hm.meshletBuild(topo)
  check_caps(part)
  assert Counter(bucket_tris(part)) == Counter(src_tris(topo)), \
      "island partition does not cover the triangle list exactly"
  st = part.stats
  print("  " + st.report("islands"))
  per_island = 2 * RING                       # verts per island == tris per island for a closed band
  ideal_v, ideal_p, fit = achievable_fill(per_island, per_island)
  assert st.meshlet_count <= (ISLANDS + fit - 1) // fit + 1, \
      f"{st.meshlet_count} buckets for {ISLANDS} islands — buckets are closing per island, not per cap"
  assert st.avg_prim_fill > 0.9 * ideal_p, \
      f"prim fill {st.avg_prim_fill:.3f} vs achievable {ideal_p:.3f} at caps {MAX_VERTS}/{MAX_PRIMS}"
  assert st.avg_vertex_fill > 0.9 * ideal_v, \
      f"vertex fill {st.avg_vertex_fill:.3f} vs achievable {ideal_v:.3f} at caps {MAX_VERTS}/{MAX_PRIMS}"
  print(f"  {fit} islands/bucket at caps {MAX_VERTS}/{MAX_PRIMS} "
        f"(achievable v={ideal_v:.3f} p={ideal_p:.3f})")
  return True


def test_mixed_islands_and_sheet():
  """a sheet plus a swarm of islands — the real shape of a plant asset (trunk band + leaf cards)."""
  ISL_RING = 5
  gidx, gnv = grid_indices(24)
  iidx, inv = island_indices(120, ISL_RING)
  idx = list(gidx) + [v + gnv for v in iidx]
  topo = hm.meshletTopologyFromIndices(idx, gnv + inv)
  part = hm.meshletBuild(topo)
  check_caps(part)
  assert Counter(bucket_tris(part)) == Counter(src_tris(topo)), "mixed partition lost triangles"
  st = part.stats
  print("  " + st.report("mixed"))
  # the islands are the limiting component (the sheet packs denser), so their achievable prim fill
  # is the floor that tracks the caps; the vertex axis is the binding cap and holds a flat floor.
  _, ideal_p, _ = achievable_fill(2 * ISL_RING, 2 * ISL_RING)
  assert st.avg_prim_fill > 0.9 * ideal_p, \
      f"prim fill {st.avg_prim_fill:.3f} vs achievable {ideal_p:.3f} at caps {MAX_VERTS}/{MAX_PRIMS}"
  assert st.avg_vertex_fill > 0.5, f"vertex fill {st.avg_vertex_fill:.3f} below the floor"
  assert st.vertex_reuse > 1.0, f"vertex reuse {st.vertex_reuse:.3f} means no locality was found"
  return True


###############################################################################

def main():
  tests = [
      test_coverage_and_caps,
      test_determinism,
      test_resumable_slicing,
      test_append_fast_path,
      test_island_packing,
      test_mixed_islands_and_sheet,
  ]
  failed = []
  for t in tests:
    print(f"[{t.__name__}]", flush=True)
    try:
      if not t():
        failed.append(t.__name__)
    except Exception:
      import traceback
      traceback.print_exc()
      failed.append(t.__name__)
  ok = (len(failed) == 0)
  print("=== hypermesh meshlet cpu gate %s (%d/%d) ===" % (
      "PASSED" if ok else "FAILED", len(tests) - len(failed), len(tests)), flush=True)
  if failed:
    print("  failed: " + ", ".join(failed), flush=True)
  sys.exit(0 if ok else 1)


main()
