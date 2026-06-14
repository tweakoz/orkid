#!/usr/bin/env python3
###############################################################################
# hypermesh foundation gate — bake a single PrimitiveModule through the GPU mesh
# compute-dataflow (GpuMesh channels + GraphInst-owned pow2 SSBO pool), read the
# channels back, and assert vertex count (CPU + GPU header), bbox (extent), and a
# unit-normal sample. The C++ does the asserts; this is the headless harness.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs


def main():
    # offscreen GPU lifecycle (mirrors test_terrain_ops.py)
    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    print("running hypermesh foundation self-test ...", flush=True)
    fails = lev2.hypermesh_foundation_selftest(ctx)

    ezapp.mainThreadEnd()

    passed = (fails == 0)
    print(f"=== hypermesh foundation {'PASSED' if passed else 'FAILED'} ({fails} failures) ===", flush=True)
    ecs.headless_exit()
    sys.exit(0 if passed else 1)


main()
