#!/usr/bin/env python3
###############################################################################
# E.5 gate — concave triangulation oracle. Hand-built polygons (convex quad,
# dart quad, L-hexagon, comb octagon) go through the REAL render triangulator
# (MeshRenderTri dispatch); the C++ selftest reads the index buffer back and
# asserts the AREA ORACLE: every output triangle keeps the polygon's
# orientation (a folded fan emits flipped tris) and the summed triangle area
# equals the polygon area (a folded fan double-covers). This is the case the
# pre-E.5 corner-0 fan provably failed on every concave input.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"

  print("running hypermesh triangulation oracle ...", flush=True)
  fails = lev2.hypermesh_triangulation_selftest(ctx)

  ezapp.mainThreadEnd()
  passed = (fails == 0)
  print(f"=== hypermesh triangulation gate {'PASSED' if passed else 'FAILED'} ({fails} failures) ===", flush=True)
  ecs.headless_exit()
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
