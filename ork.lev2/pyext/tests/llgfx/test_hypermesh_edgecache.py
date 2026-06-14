#!/usr/bin/env python3
###############################################################################
# C.1c gate — MeshEdges topology-keyed cache: re-evaluating a live graph with UNCHANGED
# topology must hit the cached EDGE table and yield identical results. Catches the
# published-count class of bug (e_ne lives GPU-side in the control block cs_build writes;
# a cache-hit frame must not clobber it — ensure() once zeroed it every call, which only
# worked because build() used to re-run unconditionally).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, tempfile
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import Hypermesh, sel_dihedral_gt, LINE, replace, group


class EdgeAsset(Hypermesh):
  def __init__(self):
    super().__init__()
    w = self.smooth_normals(self.box(size=1.0))
    s = self.select(w, sel_dihedral_gt(45.0), domain=LINE, op=replace(group(0)))
    self.output(self.edgetest(s))      # encodes each edge as a vert: (va, vb, count+10*selected)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    tmp = tempfile.mkdtemp(prefix="hm_edgecache_")
    o1, o2 = os.path.join(tmp, "cold.obj"), os.path.join(tmp, "hit.obj")
    live = EdgeAsset().materialize_live(ctx)         # eval #1: cold MeshEdges build
    lev2.hypermesh.dump_obj(live.mesh, ctx, o1)
    live.recompute(ctx)                              # eval #2: cache HIT (same topology)
    lev2.hypermesh.dump_obj(live.mesh, ctx, o2)
    a, b = open(o1).read(), open(o2).read()
    assert a == b, "cache-hit eval diverged from cold build"
    n12 = sum(1 for l in a.splitlines() if l.startswith("v ") and l.endswith(" 12"))
    assert n12 == 12, "expected 12 selected cube edges (z=12), got %d" % n12
    print("EDGECACHE_HIT=PASS (12 edges, identical across cold/hit evals)", flush=True)
    ok = True
  finally:
    ezapp.mainThreadEnd()
    print("=== hypermesh edgecache gate %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
