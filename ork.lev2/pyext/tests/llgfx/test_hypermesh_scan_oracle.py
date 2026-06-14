#!/usr/bin/env python3
###############################################################################
# C.2 gate — MeshScan parallel-path oracle, driven through MeshSort (32 scans per sort,
# collision-heavy 8-bit keys via SortTest). The EXACT expected result is recomputed here
# with numpy (same hash, stable argsort), so this asserts full end-to-end correctness:
# keys non-decreasing, payload = the stable permutation, nothing lost or duplicated.
# Sizes straddle every boundary of the parallel scan: the 256-element block, the 64-thread
# dispatch group row (64*256=16384), 65536, and a quarter-million element top size.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, tempfile
import numpy as np
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.hypermesh import Hypermesh

SIZES = [1, 3, 255, 256, 257, 4096, 16384, 16385, 65537, 262144]


def expected_xy(n):
  i = np.arange(n, dtype=np.uint64)
  h = ((i * np.uint64(2654435761)) & np.uint64(0xFFFFFFFF)).astype(np.uint32) ^ (np.arange(n, dtype=np.uint32) >> 3)
  keys = (h & np.uint32(255)).astype(np.uint32)
  order = np.argsort(keys, kind="stable").astype(np.uint32)
  return keys[order], order


def parse_xy(path, n):
  xs, ys = [], []
  with open(path) as f:
    for line in f:
      if line.startswith("v "):
        p = line.split()
        xs.append(float(p[1])); ys.append(float(p[2]))
        if len(xs) == n:
          break
  return np.asarray(xs, dtype=np.uint32), np.asarray(ys, dtype=np.uint32)


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  ok = False
  try:
    tmp = tempfile.mkdtemp(prefix="hm_scan_oracle_")
    for n in SIZES:
      class _Sort(Hypermesh):
        def __init__(self, _n=n):
          super().__init__()
          self.output(self.sorttest(_n))
      live = _Sort().materialize_live(ctx)
      path = os.path.join(tmp, "sort_%d.obj" % n)
      lev2.hypermesh.dump_obj(live.mesh, ctx, path)
      gx, gy = parse_xy(path, n)
      ex, ey = expected_xy(n)
      assert len(gx) == n, "n=%d: dumped %d verts" % (n, len(gx))
      assert np.array_equal(gx, ex), "n=%d: sorted keys mismatch (scan broke the radix split)" % n
      assert np.array_equal(gy, ey), "n=%d: payload permutation mismatch (stability/scatter broke)" % n
      print("SCAN_ORACLE n=%-7d PASS" % n, flush=True)
      live = None    # release the GraphInst/pool before the next size
    ok = True
  finally:
    ezapp.mainThreadEnd()
    print("=== hypermesh scan oracle %s ===" % ("PASSED" if ok else "FAILED"), flush=True)
    ecs.headless_exit()
    sys.exit(0 if ok else 1)


main()
