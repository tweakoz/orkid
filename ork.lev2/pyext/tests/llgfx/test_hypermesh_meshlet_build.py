#!/usr/bin/env python3
###############################################################################
# Meshlet BUILD gate — the CPU partitioner that buckets a hypermesh triangle snapshot into
# mesh-shader-ready meshlets (<=256 unique verts, <=256 prims each). Build side only: no upload,
# no draw. Runs against a REAL generated mesh (the CaneCholla l-system swept tube) plus the
# live-render hook that rebuilds a partition when topology moves.
#
# What it proves:
#   1. snapshot     — a live GpuMesh fan-triangulates to a deterministic CPU triangle list
#   2. coverage     — every triangle lands in exactly ONE bucket, and the caps always hold
#   3. determinism  — two builds of the same snapshot are byte-identical
#   4. append       — prefix-preserving growth carries every prior bucket VERBATIM; a churned
#                     prefix is detected and forces a full rebuild
#   5. stats        — bucket count / fill factors / vertex reuse are computed and sane
#   6. slicing      — the builder is resumable under a small budget and converges to the same
#                     partition a single-shot build produces
#   7. live hook    — a rendering live hypermesh publishes a (topology, partition) PAIR
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORKID_HYPERMESH_MESHLETS"] = "1"  # arms the live-render hook (test 7)
import sys, time
from collections import Counter

from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3, VarMap

from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY, GpuMeshRenderSource
from ork.hypergraph.dflow.lsystem.examples import CaneCholla

hm = lev2.hypermesh

MAX_VERTS = 256
MAX_PRIMS = 256

G = {}   # cross-test state (the baked mesh + its snapshot + partitions)


###############################################################################

def bucket_triangles(part):
  """every bucket's prims expanded back to GLOBAL vertex triples, in bucket order."""
  vl, pi = part.vertex_list, part.prim_indices
  out = []
  for (voff, vcnt, poff, pcnt) in part.descriptors:
    for p in range(pcnt):
      packed = pi[poff + p]
      a, b, c = packed & 255, (packed >> 8) & 255, (packed >> 16) & 255
      assert a < vcnt and b < vcnt and c < vcnt, "local prim index outside its bucket vertex list"
      out.append((vl[voff + a], vl[voff + b], vl[voff + c]))
  return out


def source_triangles(topo):
  idx = topo.tri_indices
  return [(idx[i], idx[i + 1], idx[i + 2]) for i in range(0, len(idx), 3)]


def check_caps(part):
  for (voff, vcnt, poff, pcnt) in part.descriptors:
    assert 0 < vcnt <= MAX_VERTS, f"vertex cap violated: {vcnt}"
    assert 0 < pcnt <= MAX_PRIMS, f"prim cap violated: {pcnt}"
    assert voff + vcnt <= len(part.vertex_list), "vertex slice overruns the vertex list"
    assert poff + pcnt <= len(part.prim_indices), "prim slice overruns the prim index array"


def raw(part):
  """the byte-comparable content of a partition."""
  return (list(part.descriptors), list(part.vertex_list), list(part.prim_indices))


###############################################################################

def test_snapshot(ctx):
  m = Hypermesh()
  n = m.lsystem(
      grammar     = CaneCholla,
      seg_len     = 0.16,
      base_radius = 0.020,
      sides       = 7,
      jitter      = 0.12,
      jit_azimuth = 0.6,
      jit_pitch   = 0.35,
      tropism     = 0.02)
  m.output(n)
  mesh = m.materialize(ctx)
  G["mesh"] = mesh
  topo = hm.meshletTopologyFromMesh(mesh, ctx)
  G["topo"] = topo
  print(f"  cholla: verts={mesh.num_verts} faces={mesh.num_faces} -> tris={topo.num_tris}")
  assert mesh.num_faces > 500, f"cholla baked too small to be a real test: {mesh.num_faces} faces"
  assert topo.num_tris >= mesh.num_faces, "fan triangulation produced fewer tris than faces"
  assert topo.num_verts == mesh.num_verts
  assert max(topo.tri_indices) < topo.num_verts, "triangle index outside the vertex range"
  # the snapshot itself must be reproducible (it anchors every partition below)
  topo2 = hm.meshletTopologyFromMesh(mesh, ctx)
  assert topo2.tri_indices == topo.tri_indices, "two snapshots of one static mesh diverged"
  assert topo2.snapshot_id != topo.snapshot_id, "snapshot ids must be distinct per snapshot"
  return True


def test_coverage_and_caps(ctx):
  part = hm.meshletBuild(G["topo"])
  G["part"] = part
  check_caps(part)
  got  = Counter(bucket_triangles(part))
  want = Counter(source_triangles(G["topo"]))
  assert got == want, "bucketed triangles are not exactly the source triangles (dropped/duplicated)"
  assert sum(got.values()) == G["topo"].num_tris
  assert part.topology.snapshot_id == G["topo"].snapshot_id, "partition lost its topology pairing"
  assert part.immutable_count == 0, "a from-scratch build must carry nothing"
  print(f"  {part.meshlet_count} buckets, every one of {G['topo'].num_tris} tris placed exactly once")
  return True


def test_determinism(ctx):
  a = hm.meshletBuild(G["topo"])
  b = hm.meshletBuild(G["topo"])
  assert raw(a) == raw(b), "two builds of one snapshot diverged"
  assert raw(a) == raw(G["part"]), "build diverged from the partition built in the coverage test"
  # the printed digest makes determinism checkable ACROSS PROCESS RUNS too (diff two invocations)
  import hashlib
  digest = hashlib.sha1(repr(raw(a)).encode()).hexdigest()[:16]
  print(f"  identical across 3 builds ({len(a.vertex_list)} vertex refs, "
        f"{len(a.prim_indices)} prims) digest={digest}")
  return True


def test_append_fast_path(ctx):
  topo = G["topo"]
  idx  = topo.tri_indices
  ntri = topo.num_tris
  ktri = (ntri * 3) // 5                      # partial mesh, then the same mesh grown
  base = hm.meshletTopologyFromIndices(idx[:ktri * 3], topo.num_verts)
  pbase = hm.meshletBuild(base)
  check_caps(pbase)

  grown = hm.meshletTopologyFromIndices(idx, topo.num_verts)
  assert grown.isAppendOf(base), "grown snapshot should read as an append of the partial one"
  builder = hm.meshletBuilder(grown, pbase)
  assert builder.append_path, "append fast path not taken on prefix-preserving growth"
  assert builder.start_tri == ktri, f"append started at {builder.start_tri}, expected {ktri}"
  while builder.step(512):
    pass
  pgrown = builder.partition
  check_caps(pgrown)

  # the carried buckets must be BYTE-IDENTICAL, not merely equivalent
  nb = pbase.meshlet_count
  assert pgrown.immutable_count == nb, f"carried {pgrown.immutable_count} buckets, expected {nb}"
  assert list(pgrown.descriptors)[:nb] == list(pbase.descriptors), "carried descriptors changed"
  nv, np_ = len(pbase.vertex_list), len(pbase.prim_indices)
  assert list(pgrown.vertex_list)[:nv] == list(pbase.vertex_list), "carried vertex lists changed"
  assert list(pgrown.prim_indices)[:np_] == list(pbase.prim_indices), "carried prim indices changed"
  assert pgrown.meshlet_count > nb, "growth appended no new buckets"
  assert Counter(bucket_triangles(pgrown)) == Counter(source_triangles(grown)), \
      "appended partition does not cover the grown triangle list exactly"

  # CHURN: an edited prefix is not an append — it must rebuild from scratch
  churned = list(idx)
  churned[0:3], churned[3:6] = churned[3:6], churned[0:3]
  ctopo = hm.meshletTopologyFromIndices(churned, topo.num_verts)
  assert not ctopo.isAppendOf(base), "a reordered prefix must not read as an append"
  cb = hm.meshletBuilder(ctopo, pbase)
  assert not cb.append_path and cb.start_tri == 0, "churn must force a full rebuild"
  print(f"  {nb} buckets carried verbatim, {pgrown.meshlet_count - nb} appended; churn rebuilds")
  return True


def test_stats(ctx):
  st = G["part"].stats
  print("  " + st.report("cholla"))
  assert st.meshlet_count == G["part"].meshlet_count
  assert st.tri_count == G["topo"].num_tris
  assert st.vertex_ref_count == len(G["part"].vertex_list)
  assert st.avg_prim_fill > 0.5, f"prim fill {st.avg_prim_fill:.3f} too low on organic content"
  assert st.avg_vertex_fill > 0.5, f"vertex fill {st.avg_vertex_fill:.3f} too low on organic content"
  assert st.vertex_reuse > 1.0, f"vertex reuse {st.vertex_reuse:.3f} means the clustering found no locality"
  return True


def test_resumable_slicing(ctx):
  builder = hm.meshletBuilder(G["topo"])
  steps, last = 0, 0.0
  while builder.step(64):
    steps += 1
    assert builder.progress >= last, "progress went backwards"
    last = builder.progress
    assert not builder.done and builder.partition is None, "partition visible before the build finished"
    assert steps < 100000, "slicing failed to converge"
  assert builder.done and builder.progress == 1.0
  assert raw(builder.partition) == raw(G["part"]), "sliced build diverged from the single-shot build"
  print(f"  converged in {steps + 1} slices of 64 tris, identical to the single-shot partition")
  return True


def test_live_hook_publishes(ctx):
  """the re-pool refresh hook requests a rebuild; the microtask publishes the pair."""
  from ork.hypergraph.ecs.scene.assets import Hypermesh as HmAsset, Ptex3d
  from ork.hypergraph.assets.materials.terrain.solid import Solid

  class AnimAsset(Hypermesh):
    def __init__(self):
      super().__init__()
      n = self.box(size=1.0)
      n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
      n = self.extrude_faces(n, distance=0.3 + 0.1 * S.sin(S.time), slot=2)
      self.output(n)

  hwrap = HmAsset(dsl_class=AnimAsset)
  hwrap.gendata.asset_name = "meshlet_mesh"
  mwrap = Ptex3d(dsl_class=Solid, vertex_source=GpuMeshRenderSource(), roughness=0.4)
  mwrap.gendata.asset_name = "meshlet_mat"
  hmdd = hwrap.drawable_data(material=mwrap)

  asys = ecs.AssetSystemData()
  asys.declareAssetGen(mwrap.gendata)
  artifacts = asys.materializeAll(ctx)
  hmdd.resolved_material = artifacts["meshlet_mat"]

  params = VarMap()
  params.preset = "ForwardPBR"
  scene = lev2.scenegraph.Scene(params)
  layer = scene.createLayer("std_forward")
  layer.createDrawableNodeFromData("hm_meshlet", hmdd)

  cam = lev2.CameraData()
  cam.perspective(0.1, 100.0, 45.0)
  cam.lookAt(vec3(3, 2, 3), vec3(0, 0, 0), vec3(0, 1, 0))
  camlut = lev2.CameraDataLut()
  camlut.addCamera("spawncam", cam)

  part = None
  for _ in range(12):
    scene.updateScene(camlut)
    ctx.beginFrame()
    scene.renderOnContext(ctx)
    ctx.endFrame()
    time.sleep(0.005)
    live = hmdd.live
    if live is not None and live.meshlet_partition is not None:
      part = live.meshlet_partition
      break
  assert part is not None, "the live render hook never published a meshlet partition"
  check_caps(part)
  live = hmdd.live
  assert part.topology is not None, "published partition without its topology (pair broken)"
  assert part.stats.tri_count == part.topology.num_tris
  assert part.topology.num_verts == live.mesh.num_verts, "published pair does not match the live mesh"
  print("  " + part.stats.report("live-hook"))
  return True


###############################################################################

def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  tests = [
      test_snapshot,
      test_coverage_and_caps,
      test_determinism,
      test_append_fast_path,
      test_stats,
      test_resumable_slicing,
      test_live_hook_publishes,
  ]
  failed = []
  for t in tests:
    print(f"[{t.__name__}]", flush=True)
    try:
      if not t(ctx):
        failed.append(t.__name__)
    except Exception:
      import traceback
      traceback.print_exc()
      failed.append(t.__name__)
  ok = (len(failed) == 0)
  print("=== hypermesh meshlet build gate %s (%d/%d) ===" % (
      "PASSED" if ok else "FAILED", len(tests) - len(failed), len(tests)), flush=True)
  if failed:
    print("  failed: " + ", ".join(failed), flush=True)
  ezapp.mainThreadEnd()
  ecs.headless_exit()
  sys.exit(0 if ok else 1)


main()
