#!/usr/bin/env python3
###############################################################################
# Bread-and-butter terrain-op self-test (Const / Gradient / Combine / Terrace).
# Each case is driven by Const inputs (or a closed-form Gradient), so the field
# min/max/mean is known analytically and asserted in C++. This is the canary for
# the multi-SSBO compute path: Combine binds three SSBOs (out=0, a=1, b=2).
# Writes /tmp/terrain_selftest_<case>.exr per case for visual inspection.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

# 256 is a multiple of 8 (exact dispatch); 257 is NOT — it forces the compute
# dispatch to round its workgroup count UP, launching threads past the field edge.
# The analytic assertions must hold at BOTH, which proves the per-thread OOB guard
# (without it, 257's over-range threads corrupt the buffer and the checks fail).
DIMS = [256, 257]


def main():
    # offscreen GPU lifecycle (mirrors ork.scene.viewer.py / test_terrain_bake.py)
    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    fails = 0
    for dim in DIMS:
        print(f"running terrain-op self-test (dim={dim}) ...", flush=True)
        fails += lev2.terrain_ops_selftest(ctx, dim)

    # serialize -> deserialize -> bake round-trip gate (proves the graph is a
    # portable, python-decoupled artifact: baked scalars + plug values + edges).
    print("running terrain round-trip gate (dim=256) ...", flush=True)
    fails += lev2.terrain_roundtrip_test(ctx, 256)

    # SubGraphModule gate: a LoopModule (nested body = thermal erode) round-trips
    # subgraph+count+promotions, its clone bakes identically, the N->N+k per-iteration
    # cache oracle holds, and composite bypass splices it out.
    print("running terrain subgraph/loop gate (dim=256) ...", flush=True)
    fails += lev2.terrain_subgraph_test(ctx, 256)

    ezapp.mainThreadEnd()

    passed = (fails == 0)
    print(f"=== terrain ops {'PASSED' if passed else 'FAILED'} ({fails} failures; dims {DIMS} + round-trip) ===", flush=True)
    ecs.headless_exit()
    sys.exit(0 if passed else 1)


main()
