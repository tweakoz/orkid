#!/usr/bin/env python3
###############################################################################
# Per-node cook-cache gate. Bakes a CACHEABLE terrain graph twice:
#   COLD bake -> computes every node, stores its field anonymously in the
#               content-addressed DataBlockCache (keyed by the node's Merkle hash)
#   WARM bake -> same graph -> same node hashes -> loads the fields from cache;
#               must produce a byte-identical result and report cache hits for
#               the 3 compute nodes (fbm/remap/terrace); only Capture recomputes.
# The C++ side asserts both (field-equality + warm hits >= 3); 0 failures == pass.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core   # core before lev2
from orkengine import lev2
from orkengine import ecs


def main():
    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    print("running terrain cook-cache gate (dim=16384) ...", flush=True)
    fails = lev2.terrain_cache_test(ctx, 16384)

    ezapp.mainThreadEnd()

    passed = (fails == 0)
    print(f"=== terrain cache {'PASSED' if passed else 'FAILED'} ({fails} failures) ===", flush=True)
    ecs.headless_exit()
    sys.exit(0 if passed else 1)


main()
