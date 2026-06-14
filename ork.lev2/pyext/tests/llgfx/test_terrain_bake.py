#!/usr/bin/env python3
###############################################################################
# First-slice exerciser for the heightfield compute-dataflow (terrain family).
# Builds a 2-node fbm -> capture GraphData in C++, dispatches the fbm compute
# shader into an SSBO, reads it back, and writes an EXR. Verifies the file lands
# and carries non-trivial float data.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

OUT = "/tmp/terrain_height.exr"


def main():
    if os.path.exists(OUT):
        os.remove(OUT)

    # offscreen GPU lifecycle (mirrors ork.scene.viewer.py): subsystem-mode init
    # + bindGfxToCurrentThread so inline compute/readback runs on a bound context.
    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    print(f"baking heightfield -> {OUT} ...", flush=True)
    lev2.terrain_bake_test(ctx, OUT, 4096)

    ezapp.mainThreadEnd()

    ok = os.path.exists(OUT)
    sz = os.path.getsize(OUT) if ok else 0
    passed = ok and sz > 1000
    print(f"=== terrain bake {'PASSED' if passed else 'FAILED'} ===", flush=True)
    print(f"    {OUT} exists={ok} size={sz}", flush=True)
    ecs.headless_exit()
    sys.exit(0 if passed else 1)


main()
