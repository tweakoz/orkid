#!/usr/bin/env python3
###############################################################################
# A.4 gate — the Pivot-0 ORACLES. The acceptance infrastructure the Stage-C GPU ports (subdivide/
# extrude/inset topology -> GPU) cannot start without:
#
#   1. DETERMINISM (ratified decision #1): the same graph bakes BIT-IDENTICAL stored output —
#      (a) the same graphdata materialized twice (fresh GraphInst each), and (b) a freshly re-traced
#      asset. This is the premise under every raw-compare gate (A.2 round-trip, meshvet --bless);
#      if it ever breaks (e.g. an atomic-append leaking into stored buffers), every gate downstream
#      is invalid — so it is asserted here, on a representative multi-op asset.
#
#   2. EQUIVALENCE HARNESS, proven on the one op that ALREADY has both implementations: inset's
#      cpu=True (pure-CPU reference positions) vs cpu=False (the cs_inner GPU path). compare_meshes()
#      tiers the verdict: BYTE-IDENTICAL, or EPSILON (same counts + same face topology + per-vertex
#      deviation < tol — cross-impl float ops legally differ in low bits). The Stage-C ports reuse
#      exactly this: flip the impl switch, bake both, compare_meshes().
#
# POLICY (ratified): the CPU paths of extrude/inset/subdivide are RETAINED as reference
# implementations when the GPU ports land — test-only, never deleted with the port.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORK_DFLOW_ENFORCE_TYPED_CONNECT"] = "1"   # gates ENFORCE typed connections (runtime default = WARN)
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs
from orkengine.core import vec3

from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY

TMP = "/tmp"


class OracleAsset(Hypermesh):
  """multi-op coverage: generator mask -> predicate select -> matrix transform -> multi-segment
  extrude (predicate GLSL + twist/scale) -> inset (cpu= switchable) -> flat normals."""
  def __init__(self, inset_cpu=True):
    super().__init__()
    n = self.box(size=1.0, mask=isolate(group(7)))
    n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
    n = self.transform(n, translate=(0.2, 0.0, 0.1), slot=2)
    n = self.extrude_faces(n, distance=0.4, segments=3, twist=0.4 * S.t, scale=1.0 - 0.25 * S.t,
                           slot=2, mask_cap=isolate(group(4)))
    n = self.inset(n, amount=0.15, sides=8, slot=4, mask_inner=isolate(group(5)), cpu=inset_cpu)
    n = self.face_normals(n)
    self.output(n)


def _bake_to_obj(graphdata, ctx, path):
  from ork.hypergraph.dflow.hypermesh import materialize_graph
  mesh = materialize_graph(graphdata, ctx)
  assert mesh.num_verts > 0 and mesh.num_faces > 0, "bake produced an empty mesh"
  lev2.hypermesh.dump_obj(mesh, ctx, path)
  return mesh.num_verts, mesh.num_faces


def _load_obj(path):
  verts, faces = [], []
  with open(path) as f:
    for line in f:
      if line.startswith("v "):
        p = line.split(); verts.append((float(p[1]), float(p[2]), float(p[3])))
      elif line.startswith("f "):
        faces.append(tuple(int(c.split("/")[0]) for c in line.split()[1:]))
  return verts, faces


def compare_meshes(obj_a, obj_b, rel_tol=1e-5):
  """THE equivalence oracle the GPU ports are accepted against. Returns ('IDENTICAL', 0.0) on byte
  equality, ('EPSILON', maxdev_rel) when counts+topology match and positions agree within rel_tol of
  the bbox diagonal (cross-implementation float divergence), else raises with the first divergence."""
  a = open(obj_a, "rb").read(); b = open(obj_b, "rb").read()
  if a == b:
    return "IDENTICAL", 0.0
  va, fa = _load_obj(obj_a)
  vb, fb = _load_obj(obj_b)
  assert len(va) == len(vb), f"vert count differs: {len(va)} vs {len(vb)}"
  assert len(fa) == len(fb), f"face count differs: {len(fa)} vs {len(fb)}"
  assert fa == fb, "face topology differs (same counts, different indices) — NOT an epsilon case"
  lo = [min(v[i] for v in va) for i in range(3)]
  hi = [max(v[i] for v in va) for i in range(3)]
  diag = sum((hi[i] - lo[i]) ** 2 for i in range(3)) ** 0.5 or 1.0
  maxdev = max(sum((pa[i] - pb[i]) ** 2 for i in range(3)) ** 0.5 for pa, pb in zip(va, vb))
  rel = maxdev / diag
  assert rel < rel_tol, f"positions diverge {rel:.3g} x diag (tol {rel_tol:.1g})"
  return "EPSILON", rel


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx
  try:
    return _body(ctx)
  finally:
    ezapp.mainThreadEnd()
    core.coreappexit()


def _body(ctx):
  # ---- 1. DETERMINISM: same graphdata twice + a fresh re-trace -> all bit-identical ----
  a1 = OracleAsset()
  o1, o2, o3 = (os.path.join(TMP, "hm_oracle_det%d.obj" % i) for i in (1, 2, 3))
  nv1, nf1 = _bake_to_obj(a1.graphdata, ctx, o1)
  _bake_to_obj(a1.graphdata, ctx, o2)                    # same graph, fresh GraphInst
  _bake_to_obj(OracleAsset().graphdata, ctx, o3)         # fresh trace of the same source
  b1, b2, b3 = (open(p, "rb").read() for p in (o1, o2, o3))
  assert b1 == b2, "DETERMINISM BROKEN: same graphdata baked differently run-to-run"
  assert b1 == b3, "DETERMINISM BROKEN: a fresh trace of identical source baked differently"
  print(f"oracle[determinism] PASS — verts={nv1} faces={nf1}, 3 bakes bit-identical", flush=True)

  # ---- 2. EQUIVALENCE: inset CPU reference vs GPU path through the harness ----
  oc = os.path.join(TMP, "hm_oracle_cpu.obj")
  og = os.path.join(TMP, "hm_oracle_gpu.obj")
  _bake_to_obj(OracleAsset(inset_cpu=True).graphdata, ctx, oc)
  _bake_to_obj(OracleAsset(inset_cpu=False).graphdata, ctx, og)
  tier, rel = compare_meshes(oc, og, rel_tol=1e-5)
  print(f"oracle[equivalence] PASS — inset cpu-vs-gpu: {tier}"
        + (f" (maxdev {rel:.3g} x diag)" if tier == "EPSILON" else ""), flush=True)

  # ---- 3. PARAM-DRIVE (B.4): expression-param PLUG values must actually REACH the shader.
  # The round-trip gate can't see this (both sides share the upload path) — so assert positively:
  # two bakes differing ONLY in a param default must differ; same default must be bit-identical.
  from ork.hypergraph.dflow.hypermesh import param as _param

  class _PD(Hypermesh):
    def __init__(self, ampval):
      super().__init__()
      self._amp = _param("amp", ampval)
      n = self.box(size=1.0)
      n = self.select(n, S.N.dot(vec3(0, 1, 0)) > 0.6, domain=POLY, op=isolate(group(2)))
      n = self.extrude_faces(n, distance=S.t * 0.1 + self._amp, segments=2, slot=2)
      self.output(n)

  pa, pb, pc = (os.path.join(TMP, "hm_oracle_pd%d.obj" % i) for i in (1, 2, 3))
  _bake_to_obj(_PD(0.25).graphdata, ctx, pa)
  _bake_to_obj(_PD(0.75).graphdata, ctx, pb)
  _bake_to_obj(_PD(0.25).graphdata, ctx, pc)
  ba, bb, bc = (open(x, "rb").read() for x in (pa, pb, pc))
  assert ba != bb, "param value does NOT reach the shader (EXPRP plug upload broken)"
  assert ba == bc, "param-drive determinism broken"
  print("oracle[param-drive] PASS — param value reaches the shader; same value bit-identical", flush=True)

  print("HYPERMESH_ORACLES_RESULT=PASS", flush=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
