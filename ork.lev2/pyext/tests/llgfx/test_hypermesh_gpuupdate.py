#!/usr/bin/env python3
###############################################################################
# E4 gate — the family-neutral gpuUpdate() seam (hypermesh).
#
#   1. COLD: a static DSL chain (unique-per-run identity, so leftover disk state
#      can't fake a hit) BAKES via the normal one-shot materialize -> every node
#      COMPUTED + STORED. The bake stamps a GpuUpdateStamp on the durable GraphData.
#   2. GPUUPDATE: lev2.dflow.gpuUpdate(graph, ctx) resolves that stamp and re-dispatches
#      the SAME hypermesh bake with the SAME params. On the cacheable graph every
#      node loads from the WARM disk cache — hits==3, stores==0 (no spurious store).
#   3. PARITY: the gpuUpdate mesh is BYTE-IDENTICAL to the first bake (full dump_obj
#      text compare + face tags), proving gpuUpdate reproduces the bake exactly.
#   4. OVERRIDE: gpuUpdate honors a vtx_budget override kwarg (still byte-identical
#      geometry — the budget is pool sizing only, not a cook identity input).
#
# gpuUpdate() must NOT go through GraphInst::compute on a cacheable graph (the core's
# synchronous cachedCompute branch is wrong for GPU graphs) — it re-runs bakeMesh,
# whose skipping-dispatch cook path is the same one the WARM cookcache gate proves.
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
  def __init__(self, gid=5):
    super().__init__()
    b = self.box(size=SIZE)
    s = self.subdivide(b, level=1)     # exercises the topology-readback cascade
    g = self.assign_gid(s, gid=gid)
    self.output(g)


def _obj(mesh, ctx, tag):
  path = "/tmp/hm_gpuupdate_%s.obj" % tag
  lev2.hypermesh.dump_obj(mesh, ctx, path)
  with open(path) as f:
    return f.read()


def _body(ezapp, ctx):
  hm = lev2.hypermesh

  ##############################################################
  # 1) COLD — normal one-shot bake: everything computes + stores; stamp recorded
  ##############################################################
  g = Chain().generatedflow()
  assert g.cacheable, "DSL did not mark the static graph cacheable"
  mesh1 = hm.materialize(g, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 0, "cold bake claims %d hits (unique-per-run salt broken?)" % hits
  assert stores == 3, "cold bake stored %d nodes, expected 3 (box/subdiv/gid)" % stores
  obj1  = _obj(mesh1, ctx, "cold")
  tags1 = list(hm.read_face_tags(mesh1, ctx))
  print("gpuUpdate COLD PASS (3 computed+stored, %d faces)" % mesh1.num_faces, flush=True)

  ##############################################################
  # 2+3) GPUUPDATE — family-neutral entry re-dispatches the SAME bake; WARM + parity
  ##############################################################
  mesh2 = lev2.dflow.gpuUpdate(g, ctx)
  hits, stores = hm.last_cook_stats()
  assert hits == 3, "gpuUpdate hit %d/3 — WARM cache not served (gpuUpdate re-ran cold?)" % hits
  assert stores == 0, "gpuUpdate stored %d nodes, expected 0 (spurious store)" % stores
  obj2 = _obj(mesh2, ctx, "warm")
  assert obj2 == obj1, "gpuUpdate geometry diverged from first bake (dump_obj text mismatch)"
  assert list(hm.read_face_tags(mesh2, ctx)) == tags1, "gpuUpdate face tags diverged"
  print("gpuUpdate WARM PASS (3/3 loaded, geometry byte-identical)", flush=True)

  ##############################################################
  # 4) OVERRIDE — kwarg overrides the stored param; geometry still identical
  ##############################################################
  mesh3 = lev2.dflow.gpuUpdate(g, ctx, vtx_budget=1 << 18)
  hits, stores = hm.last_cook_stats()
  assert hits == 3 and stores == 0, "override gpuUpdate not WARM: hits=%d stores=%d" % (hits, stores)
  obj3 = _obj(mesh3, ctx, "override")
  assert obj3 == obj1, "override gpuUpdate geometry diverged (vtx_budget must not affect cook identity)"
  print("gpuUpdate OVERRIDE PASS (vtx_budget kwarg honored, byte-identical)", flush=True)

  ##############################################################
  # 5) LOUD on an unbaked graph — gpuUpdate has no stamp to dispatch
  ##############################################################
  fresh = Chain().generatedflow()
  raised = False
  try:
    lev2.dflow.gpuUpdate(fresh, ctx)
  except Exception as e:
    raised = "gpuUpdate stamp" in str(e) or "bake it once first" in str(e)
  assert raised, "gpuUpdate of an unbaked graph must fail LOUDLY"
  print("gpuUpdate UNBAKED-GUARD PASS (loud failure)", flush=True)

  ezapp.mainThreadEnd()
  print("=== hypermesh gpuUpdate gate PASSED ===", flush=True)
  ecs.headless_exit()
  sys.exit(0)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  try:
    _body(ezapp, ctx)
  except SystemExit:
    raise                       # _body already tore down; don't double-teardown
  except BaseException:
    import traceback; traceback.print_exc()
    ezapp.mainThreadEnd()
    ecs.headless_exit()
    sys.exit(1)


if __name__ == "__main__":
  main()
