#!/usr/bin/env ork.python
"""
Test: captureAsFormat with RGBA16F vs RGBA8 vs RGBA32F.
Renders a known color to an RTG and captures it back, comparing capture
formats to isolate whether the hang is in the capture/readback pipeline.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import sys
import time
from orkengine.core import vec2, vec3, vec4, mtx4
from orkengine import core
from orkengine import lev2

tokens = core.CrcStringProxy()

TIMEOUT = 10.0  # seconds per capture

def render_solid_color(ctx, fbi, gbi, rtg, pipeline, vw):
    """Render a solid-color quad into the RTG."""
    ctx.beginFrame()
    fbi.rtGroupPush(rtg)
    fbi.rtGroupClear(rtg)

    RCFD = lev2.RenderContextFrameData(ctx)
    RCID = lev2.RenderContextInstData(RCFD)
    RCID.forceTechnique(pipeline._permu.technique)
    RCID.genMatrix(lambda: mtx4())
    pipeline.wrappedDrawCall(RCID, lambda: gbi.drawTriangles(vw))

    fbi.rtGroupPop()
    ctx.endFrame()

def create_fullscreen_quad(gbi, color_rgba):
    """Create a fullscreen quad with a solid vertex color."""
    vtx_t = lev2.VtxV12N12B12T8C4
    vbuf = vtx_t.staticBuffer(6)
    vw = gbi.lock(vbuf, 6)

    normal = vec3(0, 0, 1)
    binormal = vec3(1, 0, 0)
    uv = vec2(0, 0)

    # Two triangles covering full NDC (-1,-1) to (1,1)
    vw.add(vtx_t(vec3(-1, -1, 0), normal, binormal, uv, color_rgba))
    vw.add(vtx_t(vec3( 1,  1, 0), normal, binormal, uv, color_rgba))
    vw.add(vtx_t(vec3( 1, -1, 0), normal, binormal, uv, color_rgba))

    vw.add(vtx_t(vec3(-1, -1, 0), normal, binormal, uv, color_rgba))
    vw.add(vtx_t(vec3(-1,  1, 0), normal, binormal, uv, color_rgba))
    vw.add(vtx_t(vec3( 1,  1, 0), normal, binormal, uv, color_rgba))

    gbi.unlock(vw)
    return vbuf, vw  # must keep vbuf alive

class CaptureFormatPipeline:
    """Holds material/pipeline state to keep objects alive."""

    def __init__(self, ctx):
        self.mtl = lev2.FreestyleMaterial()
        self.mtl.gpuInit(ctx, "orkshader://solid.fxv2")
        self._permu = lev2.FxPipelinePermutation()
        self._permu.rendermodel = "CUSTOM"
        self._permu.technique = self.mtl.shader.technique("vtxcolor")
        self._permu.has_vtxcolors = True
        self.pipeline = self.mtl.fxcache.findPipeline(self._permu)
        self.pipeline.bindParam(self.mtl.param("MatMVP"), mtx4())

    def wrappedDrawCall(self, RCID, fn):
        self.pipeline.wrappedDrawCall(RCID, fn)

def run_capture_test(ctx, ezapp, fbi, gbi, rtg_format, capture_format, pipeline, vbuf, vw, width, height):
    """
    Create RTG with rtg_format, render a red quad, capture as capture_format.
    Returns (success: bool, elapsed: float, error_msg: str).
    """
    label = f"RTG={rtg_format} capture={capture_format}"
    print(f"\n  Testing {label} ...")

    # Create RTG
    rtg = lev2.RtGroup(ctx, width, height)
    rtb = rtg.createBuffer(tokens.__getattr__(rtg_format), tokens.color)
    rtb.clearColor = core.vec4(0, 0, 0, 1)

    # Render a red quad
    render_solid_color(ctx, fbi, gbi, rtg, pipeline, vw)
    ezapp.mainThreadIter()

    # Capture
    capbuf = lev2.CaptureBuffer()
    ctx.beginFrame()
    future = fbi.captureAsFormat(rtb, capbuf, capture_format)
    ctx.endFrame()

    # Wait for capture with timeout
    start = time.time()
    while not future.is_ready:
        ezapp.mainThreadIter()
        elapsed = time.time() - start
        if elapsed > TIMEOUT:
            return False, elapsed, f"TIMEOUT after {elapsed:.1f}s - capture never completed"

    elapsed = time.time() - start

    # Validate capture buffer has data
    if capbuf.length == 0:
        return False, elapsed, "capture buffer is empty (length=0)"

    if capbuf.width != width or capbuf.height != height:
        return False, elapsed, f"capture dimensions {capbuf.width}x{capbuf.height} != expected {width}x{height}"

    return True, elapsed, "OK"

def main():
    print("=" * 60)
    print("Test: captureAsFormat with RGBA8 / RGBA16F / RGBA32F")
    print("=" * 60)

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    fbi = ctx.FBI
    gbi = ctx.GBI

    ezapp.mainThreadBegin()

    width = 64
    height = 64

    # Setup shared material/pipeline
    pipeline = CaptureFormatPipeline(ctx)

    # Create a red fullscreen quad (0xFF0000FF = ABGR for red)
    vbuf, vw = create_fullscreen_quad(gbi, 0xFF0000FF)

    errors = 0

    # Test matrix: (rtg_format, capture_format)
    # Start with known-good combos, then test the new RGBA16F paths
    test_cases = [
        # Baseline: the existing working path
        ("RGBA32F", "RGBA8"),
        # RGBA32F RTG with RGBA32F capture
        ("RGBA32F", "RGBA32F"),
        # RGBA16F RTG with RGBA8 capture (isolates RTG format)
        ("RGBA16F", "RGBA8"),
        # RGBA16F RTG with RGBA16F capture (the new HDR path)
        ("RGBA16F", "RGBA16F"),
        # RGBA16F RTG with RGBA32F capture (cross-format)
        ("RGBA16F", "RGBA32F"),
    ]

    results = []
    for rtg_fmt, cap_fmt in test_cases:
        ok, elapsed, msg = run_capture_test(
            ctx, ezapp, fbi, gbi,
            rtg_fmt, cap_fmt,
            pipeline, vbuf, vw,
            width, height)
        status = "PASS" if ok else "FAIL"
        results.append((rtg_fmt, cap_fmt, status, elapsed, msg))
        if not ok:
            errors += 1
        print(f"    [{status}] {elapsed:.3f}s - {msg}")

    # Summary
    print("\n" + "=" * 60)
    print(f"{'RTG Format':<12} {'Capture Format':<16} {'Status':<6} {'Time':>8}  Notes")
    print("-" * 60)
    for rtg_fmt, cap_fmt, status, elapsed, msg in results:
        print(f"{rtg_fmt:<12} {cap_fmt:<16} {status:<6} {elapsed:>7.3f}s  {msg}")
    print("=" * 60)

    if errors == 0:
        print("PASSED: All capture format combinations work")
    else:
        print(f"FAILED: {errors} combination(s) failed")

    ezapp.mainThreadEnd()
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
