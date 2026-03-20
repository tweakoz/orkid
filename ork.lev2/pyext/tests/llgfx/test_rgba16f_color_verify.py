#!/usr/bin/env ork.python
"""
Test: Verify RGBA16F capture preserves actual COLOR data.
Renders a known solid color, captures as RGBA16F, reads back pixels,
and verifies R != G != B (not grayscale).

This test exists because the existing capture tests only check for
no-crash and non-empty buffers, but don't verify pixel correctness.
"""

import os, sys, struct, time
os.environ["PYTHONUNBUFFERED"] = "1"
sys.stdout.reconfigure(line_buffering=True)
from orkengine.core import vec2, vec3, vec4, mtx4
from orkengine import core, lev2

tokens = core.CrcStringProxy()

def create_fullscreen_quad(gbi, color_rgba):
    vtx_t = lev2.VtxV12N12B12T8C4
    vbuf = vtx_t.staticBuffer(6)
    vw = gbi.lock(vbuf, 6)
    n = vec3(0, 0, 1)
    b = vec3(1, 0, 0)
    uv = vec2(0, 0)
    vw.add(vtx_t(vec3(-1, -1, 0), n, b, uv, color_rgba))
    vw.add(vtx_t(vec3( 1,  1, 0), n, b, uv, color_rgba))
    vw.add(vtx_t(vec3( 1, -1, 0), n, b, uv, color_rgba))
    vw.add(vtx_t(vec3(-1, -1, 0), n, b, uv, color_rgba))
    vw.add(vtx_t(vec3(-1,  1, 0), n, b, uv, color_rgba))
    vw.add(vtx_t(vec3( 1,  1, 0), n, b, uv, color_rgba))
    gbi.unlock(vw)
    return vbuf, vw

def half_to_float(h):
    """Convert IEEE 754 half-precision uint16 to Python float."""
    sign = (h >> 15) & 1
    exp = (h >> 10) & 0x1F
    mant = h & 0x3FF
    if exp == 0:
        if mant == 0:
            return (-1)**sign * 0.0
        # denormalized
        return (-1)**sign * (2**-14) * (mant / 1024.0)
    elif exp == 31:
        return float('inf') if mant == 0 else float('nan')
    else:
        return (-1)**sign * (2**(exp - 15)) * (1.0 + mant / 1024.0)

def read_rgba16f_pixel(image, x, y):
    """Read a pixel from an RGBA16F image and return (R,G,B,A) as floats."""
    w = image.width
    bpp = 8  # 4 channels * 2 bytes
    offset = (y * w + x) * bpp
    data = bytes(image.data.bytes)
    raw = data[offset:offset+8]
    if len(raw) < 8:
        return None
    r, g, b, a = struct.unpack('<HHHH', raw)
    return (half_to_float(r), half_to_float(g), half_to_float(b), half_to_float(a))

def read_rgba8_pixel(image, x, y):
    """Read a pixel from an RGBA8 image and return (R,G,B,A) as 0-255 ints."""
    w = image.width
    offset = (y * w + x) * 4
    data = bytes(image.data.bytes)
    raw = data[offset:offset+4]
    if len(raw) < 4:
        return None
    return struct.unpack('BBBB', raw)

def test_capture_color(ctx, ezapp, fbi, gbi, pipeline, permu, rtg_fmt, cap_fmt, vertex_color, expected_desc, width, height):
    """
    Render vertex_color to rtg_fmt, capture as cap_fmt, verify pixels have color (not grayscale).
    vertex_color is uint32 ABGR packed.
    Returns (pass, message).
    """
    label = f"RTG={rtg_fmt} cap={cap_fmt} color={expected_desc}"

    # Create RTG
    rtg = lev2.RtGroup(ctx, width, height)
    rtb = rtg.createBuffer(tokens.__getattr__(rtg_fmt), tokens.color)
    rtb.clearColor = vec4(0, 0, 0, 1)

    # Create quad with given color
    vbuf, vw = create_fullscreen_quad(gbi, vertex_color)

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
    ctx.endFrame()
    ezapp.mainThreadIter()

    # Capture
    capbuf = lev2.CaptureBuffer()
    ctx.beginFrame()
    future = fbi.captureAsFormat(rtb, capbuf, cap_fmt)
    ctx.endFrame()

    start = time.time()
    while not future.is_ready:
        ezapp.mainThreadIter()
        if time.time() - start > 10:
            return False, f"{label}: TIMEOUT"

    img = capbuf.image
    if not img or img.width == 0:
        return False, f"{label}: no image data"

    # Read center pixel
    cx, cy = width // 2, height // 2

    if cap_fmt == "RGBA16F":
        pixel = read_rgba16f_pixel(img, cx, cy)
        if pixel is None:
            return False, f"{label}: failed to read pixel"
        r, g, b, a = pixel
        fmt_info = f"R={r:.4f} G={g:.4f} B={b:.4f} A={a:.4f}"
    elif cap_fmt == "RGBA8":
        pixel = read_rgba8_pixel(img, cx, cy)
        if pixel is None:
            return False, f"{label}: failed to read pixel"
        r, g, b, a = [x / 255.0 for x in pixel]
        fmt_info = f"R={pixel[0]} G={pixel[1]} B={pixel[2]} A={pixel[3]}"
    else:
        return False, f"{label}: unsupported cap format for verification"

    print(f"  {label}: {fmt_info}")

    # Verify not all zeros
    if abs(r) < 0.01 and abs(g) < 0.01 and abs(b) < 0.01:
        return False, f"{label}: ALL BLACK - {fmt_info}"

    # Verify not grayscale (for non-gray test colors)
    if expected_desc == "RED":
        # Red should have R >> G and R >> B
        if r < 0.5:
            return False, f"{label}: RED too low ({fmt_info})"
        if g > 0.1 or b > 0.1:
            return False, f"{label}: GRAYSCALE detected ({fmt_info})"
        return True, f"{label}: OK ({fmt_info})"
    elif expected_desc == "GREEN":
        if g < 0.5:
            return False, f"{label}: GREEN too low ({fmt_info})"
        if r > 0.1 or b > 0.1:
            return False, f"{label}: GRAYSCALE detected ({fmt_info})"
        return True, f"{label}: OK ({fmt_info})"
    elif expected_desc == "BLUE":
        if b < 0.5:
            return False, f"{label}: BLUE too low ({fmt_info})"
        if r > 0.1 or g > 0.1:
            return False, f"{label}: GRAYSCALE detected ({fmt_info})"
        return True, f"{label}: OK ({fmt_info})"
    else:
        return True, f"{label}: OK ({fmt_info})"

def main():
    print("=" * 70)
    print("Test: RGBA16F Capture Color Verification")
    print("=" * 70)

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    fbi = ctx.FBI
    gbi = ctx.GBI
    ezapp.mainThreadBegin()

    W, H = 64, 64

    # Setup pipeline
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInit(ctx, "orkshader://solid.fxv2")
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = mtl.shader.technique("vtxcolor")
    permu.has_vtxcolors = True
    pipeline = mtl.fxcache.findPipeline(permu)
    pipeline.bindParam(mtl.param("MatMVP"), mtx4())

    errors = 0

    # Vertex colors in ABGR packed format
    RED   = 0xFF0000FF  # A=FF B=00 G=00 R=FF
    GREEN = 0xFF00FF00  # A=FF B=00 G=FF R=00
    BLUE  = 0xFFFF0000  # A=FF B=FF G=00 R=00

    test_cases = [
        # (rtg_format, capture_format, vertex_color, color_name)
        # Baseline: RGBA32F RTG, RGBA8 capture (known working)
        ("RGBA32F", "RGBA8", RED,   "RED"),
        ("RGBA32F", "RGBA8", GREEN, "GREEN"),
        ("RGBA32F", "RGBA8", BLUE,  "BLUE"),
        # RGBA32F RTG, RGBA16F capture
        ("RGBA32F", "RGBA16F", RED,   "RED"),
        ("RGBA32F", "RGBA16F", GREEN, "GREEN"),
        ("RGBA32F", "RGBA16F", BLUE,  "BLUE"),
        # RGBA16F RTG, RGBA16F capture
        ("RGBA16F", "RGBA16F", RED,   "RED"),
        ("RGBA16F", "RGBA16F", GREEN, "GREEN"),
        ("RGBA16F", "RGBA16F", BLUE,  "BLUE"),
    ]

    for rtg_fmt, cap_fmt, color, color_name in test_cases:
        ok, msg = test_capture_color(ctx, ezapp, fbi, gbi, pipeline, permu,
                                      rtg_fmt, cap_fmt, color, color_name, W, H)
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {msg}")
        if not ok:
            errors += 1

    print("\n" + "=" * 70)
    if errors == 0:
        print("ALL PASSED: RGBA16F capture preserves color correctly")
    else:
        print(f"FAILED: {errors} test(s) failed - color data is corrupted")
    print("=" * 70)

    ezapp.mainThreadEnd()
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
