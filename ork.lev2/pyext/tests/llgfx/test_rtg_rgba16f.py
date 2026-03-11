#!/usr/bin/env ork.python
"""
Test: RGBA16F render target creation and basic render/clear.
Verifies that RGBA16F render targets can be created, cleared, and rendered to
without hanging or crashing. This isolates render target format issues from
capture format issues.
"""

import sys
import time
from orkengine import core
from orkengine import lev2

tokens = core.CrcStringProxy()

TIMEOUT = 10.0  # seconds

def main():
    print("=" * 60)
    print("Test: RGBA16F render target create/clear/render")
    print("=" * 60)

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    fbi = ctx.FBI

    ezapp.mainThreadBegin()

    errors = 0
    width = 64
    height = 64

    ###########################################################################
    # Sub-test 1: Create RGBA16F render target
    ###########################################################################
    print("\n[1] Creating RGBA16F render target...")
    rtg_16f = lev2.RtGroup(ctx, width, height)
    rtb_16f = rtg_16f.createBuffer(tokens.RGBA16F, tokens.color)
    rtb_16f.clearColor = core.vec4(0.5, 0.25, 0.125, 1.0)
    print("    OK - RGBA16F RTG created")

    ###########################################################################
    # Sub-test 2: Create RGBA32F render target (known-good baseline)
    ###########################################################################
    print("[2] Creating RGBA32F render target (baseline)...")
    rtg_32f = lev2.RtGroup(ctx, width, height)
    rtb_32f = rtg_32f.createBuffer(tokens.RGBA32F, tokens.color)
    rtb_32f.clearColor = core.vec4(0.5, 0.25, 0.125, 1.0)
    print("    OK - RGBA32F RTG created")

    ###########################################################################
    # Sub-test 3: Clear RGBA32F (baseline - should not hang)
    ###########################################################################
    print("[3] Clear + render frame on RGBA32F (baseline)...")
    start = time.time()
    ctx.beginFrame()
    fbi.rtGroupPush(rtg_32f)
    fbi.rtGroupClear(rtg_32f)
    fbi.rtGroupPop()
    ctx.endFrame()
    ezapp.mainThreadIter()
    elapsed = time.time() - start
    print(f"    OK - RGBA32F frame completed in {elapsed:.3f}s")

    ###########################################################################
    # Sub-test 4: Clear RGBA16F (the format under test)
    ###########################################################################
    print("[4] Clear + render frame on RGBA16F...")
    start = time.time()
    ctx.beginFrame()
    fbi.rtGroupPush(rtg_16f)
    fbi.rtGroupClear(rtg_16f)
    fbi.rtGroupPop()
    ctx.endFrame()
    ezapp.mainThreadIter()
    elapsed = time.time() - start
    if elapsed > TIMEOUT:
        print(f"    FAIL - RGBA16F frame took {elapsed:.3f}s (timeout)")
        errors += 1
    else:
        print(f"    OK - RGBA16F frame completed in {elapsed:.3f}s")

    ###########################################################################
    # Sub-test 5: Multiple consecutive RGBA16F frames
    ###########################################################################
    print("[5] Multiple consecutive RGBA16F frames (5 iterations)...")
    start = time.time()
    for i in range(5):
        ctx.beginFrame()
        fbi.rtGroupPush(rtg_16f)
        fbi.rtGroupClear(rtg_16f)
        fbi.rtGroupPop()
        ctx.endFrame()
        ezapp.mainThreadIter()
    elapsed = time.time() - start
    if elapsed > TIMEOUT:
        print(f"    FAIL - 5 RGBA16F frames took {elapsed:.3f}s (timeout)")
        errors += 1
    else:
        print(f"    OK - 5 RGBA16F frames completed in {elapsed:.3f}s")

    ###########################################################################
    # Done
    ###########################################################################
    ezapp.mainThreadEnd()

    print("\n" + "=" * 60)
    if errors == 0:
        print("PASSED: RGBA16F render target creation and clearing works")
    else:
        print(f"FAILED: {errors} sub-test(s) failed")
    print("=" * 60)
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
