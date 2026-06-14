#!/usr/bin/env python3
###############################################################################
# E.6/2.19 gate — hypermesh per-node cook cache (the terrain pattern).
#
#   1. COLD: a static DSL chain (unique-per-run identity, so leftover disk state
#      can't fake a hit) materializes with every node COMPUTED + STORED.
#   2. WARM: RE-AUTHORING the same chain (fresh python objects -> fresh uuids!)
#      restores every node from the disk cache — the CONTENT-ONLY hash contract
#      (uuids are git identity, never cook identity).
#   3. PARITY: the warm (loaded) mesh is byte-identical to the cold (computed)
#      one — full dump_obj text compare (positions/normals/uvs/faces) + tags.
#      Covers the subdivide topology-cascade path: the cold store happens AFTER
#      the cascade, the warm load skips compute AND table builds entirely.
#   4. INVALIDATION: changing one mid-chain param recomputes that node and its
#      DOWNSTREAM (Merkle), while the upstream still hits.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import struct
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import Hypermesh

# unique-per-run identity: guarantees run 1 in THIS process is genuinely cold even
# if an earlier identical gate run primed <staging>/dflowcache.
SALT = struct.unpack("<I", os.urandom(4))[0] % 1000
SIZE = 1.0 + SALT * 1e-4


class Chain(Hypermesh):
  # gid AFTER subdivide — subdivide does not (yet) propagate face __tags to its
  # children, so tags assigned upstream of it are lost (the ren_scatter ordering).
  # This ordering ALSO exercises the GidAssign re-eval path: the topology cascade
  # re-runs writeParams with a 4x face count -> the created channel re-acquires.
  def __init__(self, gid=5):
    super().__init__()
    b = self.box(size=SIZE)
    s = self.subdivide(b, level=1)     # exercises the topology-readback cascade
    g = self.assign_gid(s, gid=gid)
    self.output(g)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  hm = lev2.hypermesh
  try:
    _body(ezapp, ctx, hm)
  except SystemExit:
    raise                       # _body already tore down; don't double-teardown
  except BaseException:
    import traceback; traceback.print_exc()
    # ALWAYS tear down — a skipped headless_exit spins the process forever
    # (the coreappexit teardown trap)
    ezapp.mainThreadEnd()
    ecs.headless_exit()
    sys.exit(1)


def _obj(live, ctx, tag):
  path = "/tmp/hm_cook_%s.obj" % tag
  hm = lev2.hypermesh
  hm.dump_obj(live.mesh, ctx, path)
  with open(path) as f:
    return f.read()


def _body(ezapp, ctx, hm):

  ##############################################################
  # 1) COLD — everything computes + stores
  ##############################################################
  g1 = Chain().generatedflow()
  assert g1.cacheable, "DSL did not mark the static graph cacheable"
  live1 = hm.materialize_live(g1, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 0, "cold run claims %d hits (unique-per-run salt broken?)" % hits
  assert stores == 3, "cold run stored %d nodes, expected 3 (box/subdiv/gid)" % stores
  tags1 = list(hm.read_face_tags(live1.mesh, ctx))
  assert set((t >> 20) & 0xFFF for t in tags1) == {5}, \
      "cold chain carries wrong gids: %s" % tags1[:4]
  obj1  = _obj(live1, ctx, "cold")
  print("cook COLD PASS (3 computed+stored, %d faces)" % live1.mesh.num_faces, flush=True)

  ##############################################################
  # 2+3) WARM — re-authored (FRESH uuids) -> all nodes load; full parity
  ##############################################################
  g2 = Chain().generatedflow()
  live2 = hm.materialize_live(g2, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 3, "warm run hit %d/3 — content-only hashing broken (uuid leak?)" % hits
  assert stores == 0, "warm run stored %d nodes, expected 0" % stores
  tags2 = list(hm.read_face_tags(live2.mesh, ctx))
  assert tags2 == tags1, "warm tags diverged from cold"
  obj2 = _obj(live2, ctx, "warm")
  assert obj2 == obj1, "warm geometry diverged from cold (dump_obj text mismatch)"
  print("cook WARM PASS (3/3 loaded, geometry byte-identical)", flush=True)

  ##############################################################
  # 4) INVALIDATION — one mid-chain param: upstream hits, node+downstream miss
  ##############################################################
  g3 = Chain(gid=6).generatedflow()
  live3 = hm.materialize_live(g3, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 2, "invalidation: expected box+subdivide to hit, got %d" % hits
  assert stores == 1, "invalidation: expected ONLY gidassign to recompute+store, got %d" % stores
  gids3 = set((t >> 20) & 0xFFF for t in hm.read_face_tags(live3.mesh, ctx))
  assert gids3 == {6}, "recomputed terminal carries wrong gids: %s" % gids3
  print("cook INVALIDATION PASS (2 hits / 1 recomputed)", flush=True)

  ezapp.mainThreadEnd()
  print("=== hypermesh cookcache gate PASSED ===", flush=True)
  ecs.headless_exit()
  sys.exit(0)


if __name__ == "__main__":
  main()
