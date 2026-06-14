#!/usr/bin/env python3
###############################################################################
# A.2 gate — hypermesh SAVE -> LOAD -> BAKE round-trip (the python-decoupled core, mirroring the
# terrain precedent test_terrain_asset.py). Proves a serialized hypermesh graph COMPUTES identically
# after reload with NO DSL re-run — not merely that its fields survive (that is the A.1 gate,
# test_hypermesh_reflection.py).
#
#   1. AUTHOR once: trace a multi-op asset (box -> select -> transform -> multi-segment extrude with
#      predicate GLSL + expr params -> inset) and BAKE it -> OBJ #1.
#   2. Hypermesh.save(path) -> the JSON artifact (post-trace GLSL, baked matrix, masks, params).
#   3. load_graphdata(path) -> a FRESH graphdata (no Python DSL, no asset class) -> BAKE -> OBJ #2.
#   4. RAW compare: OBJ #1 == OBJ #2 byte-identical (ratified decision #1: stored output is
#      deterministic, so raw compare is valid — no canonicalization). Catches dropped fields,
#      dropped CONNECTIONS (silent-edge-drop would change topology), plug-value drift, ABI breaks.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORK_DFLOW_ENFORCE_TYPED_CONNECT"] = "1"   # gates ENFORCE typed connections (runtime default = WARN)
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs    # headless_appinit (full class registration)
from orkengine.core import vec3

from ork.hypergraph.dflow.hypermesh import (Hypermesh, S, isolate, group, POLY, param,
                                            load_graphdata, materialize_graph)

OBJ1 = "/tmp/hm_roundtrip_authored.obj"
OBJ2 = "/tmp/hm_roundtrip_reloaded.obj"
ART  = "/tmp/hm_roundtrip.hmgraph.json"


class RoundTripAsset(Hypermesh):
  def __init__(self):
    super().__init__()
    self._amp = param("amp", 0.25)                       # expr-param vec4 slot (serialized value)
    n = self.box(size=1.0, mask=isolate(group(7)))
    n = self.select(n, (S.N.dot(vec3(0, 1, 0)) > 0.6) & ~S.tag(3),
                    domain=POLY, op=isolate(group(2)))
    n = self.transform(n, translate=(0.3, 0.0, 0.1), slot=2)
    n = self.extrude_faces(n,
                           distance=S.t * 0.15 + self._amp,
                           segments=3,
                           twist=0.5 * S.t,
                           scale=1.0 - 0.3 * S.t,
                           slot=2,
                           mask_cap=isolate(group(4)))
    n = self.inset(n, amount=0.1, sides=8, slot=4, mask_inner=isolate(group(5)))
    n = self.face_normals(n)
    self.output(n)


def main():
  for p in (OBJ1, OBJ2, ART):
    if os.path.exists(p):
      os.remove(p)
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  try:
    return _body(ctx)
  finally:
    ezapp.mainThreadEnd()
    core.coreappexit()         # ALWAYS — a skipped coreappexit masks a failed assert as a teardown hang


def _body(ctx):
  # 1. AUTHOR + BAKE the traced graph
  a = RoundTripAsset()
  mesh1 = a.materialize(ctx)
  assert mesh1.num_verts > 0 and mesh1.num_faces > 0, "authored bake produced an empty mesh"
  lev2.hypermesh.dump_obj(mesh1, ctx, OBJ1)
  print(f"authored bake: verts={mesh1.num_verts} faces={mesh1.num_faces}", flush=True)

  # 2. SAVE the artifact
  a.save(ART)
  print(f"artifact JSON bytes={os.path.getsize(ART)} -> {ART}", flush=True)

  # 3. LOAD with no DSL re-run + BAKE the deserialized graph
  g2 = load_graphdata(ART)
  assert g2 is not None, "load_graphdata returned null"
  mesh2 = materialize_graph(g2, ctx)
  assert mesh2.num_verts == mesh1.num_verts and mesh2.num_faces == mesh1.num_faces, \
      (f"reloaded bake counts differ: verts {mesh1.num_verts}->{mesh2.num_verts} "
       f"faces {mesh1.num_faces}->{mesh2.num_faces} (dropped connection/field?)")
  lev2.hypermesh.dump_obj(mesh2, ctx, OBJ2)

  # 4. RAW compare (decision #1: deterministic stored output -> byte equality is the right gate)
  b1 = open(OBJ1, "rb").read()
  b2 = open(OBJ2, "rb").read()
  assert len(b1) > 0, "authored OBJ empty"
  assert b1 == b2, (f"BAKE MISMATCH after round-trip: {OBJ1} ({len(b1)}B) != {OBJ2} ({len(b2)}B) — "
                    "a serialized field or connection does not reproduce the authored compute")

  print(f"bake-compare: {len(b1)} bytes byte-identical", flush=True)
  print("HYPERMESH_ROUNDTRIP_RESULT=PASS", flush=True)
  return 0


if __name__ == "__main__":
  sys.exit(main())
