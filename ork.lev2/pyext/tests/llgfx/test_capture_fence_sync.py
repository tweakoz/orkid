#!/usr/bin/env ork.python
"""
Test: Fence synchronization for capture readback.
Renders, captures, and immediately reads back data in rapid succession
to stress-test the GPU fence / staging buffer synchronization path.
Multiple rapid captures can expose missing fence waits.
"""

import sys
import time
from orkengine.core import vec2, vec3, vec4, mtx4
from orkengine import core
from orkengine import lev2

tokens = core.CrcStringProxy()

TIMEOUT = 10.0  # seconds per capture

def create_fullscreen_quad(gbi, color_rgba):
    """Create a fullscreen quad with a solid vertex color."""
    vtx_t = lev2.VtxV12N12B12T8C4
    vbuf = vtx_t.staticBuffer(6)
    vw = gbi.lock(vbuf, 6)
    normal = vec3(0, 0, 1)
    binormal = vec3(1, 0, 0)
    uv = vec2(0, 0)
    vw.add(vtx_t(vec3(-1, -1, 0), normal, binormal, uv, color_rgba))
    vw.add(vtx_t(vec3( 1,  1, 0), normal, binormal, uv, color_rgba))
    vw.add(vtx_t(vec3( 1, -1, 0), normal, binormal, uv, color_rgba))
    vw.add(vtx_t(vec3(-1, -1, 0), normal, binormal, uv, color_rgba))
    vw.add(vtx_t(vec3(-1,  1, 0), normal, binormal, uv, color_rgba))
    vw.add(vtx_t(vec3( 1,  1, 0), normal, binormal, uv, color_rgba))
    gbi.unlock(vw)
    return vbuf, vw

def main():
    print("=" * 60)
    print("Test: Rapid sequential captures (fence sync stress test)")
    print("=" * 60)

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    fbi = ctx.FBI
    gbi = ctx.GBI

    ezapp.mainThreadBegin()

    width = 64
    height = 64
    num_iterations = 10

    # Setup material
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInit(ctx, "orkshader://solid.fxv2")
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = mtl.shader.technique("vtxcolor")
    permu.has_vtxcolors = True
    pipeline = mtl.fxcache.findPipeline(permu)
    pipeline.bindParam(mtl.param("MatMVP"), mtx4())

    # Red quad
    vbuf, vw = create_fullscreen_quad(gbi, 0xFF0000FF)

    errors = 0

    ###########################################################################
    # Test: Rapid sequential RGBA8 captures (baseline)
    ###########################################################################
    print(f"\n[1] {num_iterations} rapid RGBA8 captures from RGBA32F RTG (baseline)...")
    rtg = lev2.RtGroup(ctx, width, height)
    rtb = rtg.createBuffer(tokens.RGBA32F, tokens.color)
    rtb.clearColor = core.vec4(0, 0, 0, 1)

    start = time.time()
    for i in range(num_iterations):
        # Render
        ctx.beginFrame()
        fbi.rtGroupPush(rtg)
        fbi.rtGroupClear(rtg)
        RCFD = lev2.RenderContextFrameData(ctx)
        RCID = lev2.RenderContextInstData(RCFD)
        RCID.forceTechnique(permu.technique)
        RCID.genMatrix(lambda: mtx4())
        pipeline.wrappedDrawCall(RCID, lambda: gbi.drawTriangles(vw))
        fbi.rtGroupPop()

        # Capture in same frame
        capbuf = lev2.CaptureBuffer()
        future = fbi.captureAsFormat(rtb, capbuf, "RGBA8")
        ctx.endFrame()

        # Wait
        t0 = time.time()
        while not future.is_ready:
            ezapp.mainThreadIter()
            if time.time() - t0 > TIMEOUT:
                print(f"    FAIL - capture {i} timed out")
                errors += 1
                break

    elapsed = time.time() - start
    print(f"    Completed in {elapsed:.3f}s ({elapsed/num_iterations:.3f}s/capture)")

    ###########################################################################
    # Test: Rapid sequential RGBA16F captures from RGBA16F RTG
    ###########################################################################
    print(f"\n[2] {num_iterations} rapid RGBA16F captures from RGBA16F RTG...")
    rtg16 = lev2.RtGroup(ctx, width, height)
    rtb16 = rtg16.createBuffer(tokens.RGBA16F, tokens.color)
    rtb16.clearColor = core.vec4(0, 0, 0, 1)

    start = time.time()
    for i in range(num_iterations):
        ctx.beginFrame()
        fbi.rtGroupPush(rtg16)
        fbi.rtGroupClear(rtg16)
        RCFD = lev2.RenderContextFrameData(ctx)
        RCID = lev2.RenderContextInstData(RCFD)
        RCID.forceTechnique(permu.technique)
        RCID.genMatrix(lambda: mtx4())
        pipeline.wrappedDrawCall(RCID, lambda: gbi.drawTriangles(vw))
        fbi.rtGroupPop()

        capbuf = lev2.CaptureBuffer()
        future = fbi.captureAsFormat(rtb16, capbuf, "RGBA16F")
        ctx.endFrame()

        t0 = time.time()
        while not future.is_ready:
            ezapp.mainThreadIter()
            if time.time() - t0 > TIMEOUT:
                print(f"    FAIL - capture {i} timed out")
                errors += 1
                break

    elapsed = time.time() - start
    print(f"    Completed in {elapsed:.3f}s ({elapsed/num_iterations:.3f}s/capture)")

    ###########################################################################
    # Test: Interleaved RGBA8 and RGBA16F captures (format switching)
    ###########################################################################
    print(f"\n[3] {num_iterations} interleaved RGBA8/RGBA16F captures...")
    start = time.time()
    for i in range(num_iterations):
        use_16f = (i % 2 == 1)
        cur_rtg = rtg16 if use_16f else rtg
        cur_rtb = rtb16 if use_16f else rtb
        cap_fmt = "RGBA16F" if use_16f else "RGBA8"

        ctx.beginFrame()
        fbi.rtGroupPush(cur_rtg)
        fbi.rtGroupClear(cur_rtg)
        RCFD = lev2.RenderContextFrameData(ctx)
        RCID = lev2.RenderContextInstData(RCFD)
        RCID.forceTechnique(permu.technique)
        RCID.genMatrix(lambda: mtx4())
        pipeline.wrappedDrawCall(RCID, lambda: gbi.drawTriangles(vw))
        fbi.rtGroupPop()

        capbuf = lev2.CaptureBuffer()
        future = fbi.captureAsFormat(cur_rtb, capbuf, cap_fmt)
        ctx.endFrame()

        t0 = time.time()
        while not future.is_ready:
            ezapp.mainThreadIter()
            if time.time() - t0 > TIMEOUT:
                print(f"    FAIL - capture {i} ({cap_fmt}) timed out")
                errors += 1
                break

    elapsed = time.time() - start
    print(f"    Completed in {elapsed:.3f}s ({elapsed/num_iterations:.3f}s/capture)")

    ###########################################################################
    # Done
    ###########################################################################
    ezapp.mainThreadEnd()

    print("\n" + "=" * 60)
    if errors == 0:
        print("PASSED: All rapid capture sequences completed without hanging")
    else:
        print(f"FAILED: {errors} capture(s) timed out")
    print("=" * 60)
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
