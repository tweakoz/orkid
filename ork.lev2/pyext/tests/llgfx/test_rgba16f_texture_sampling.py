#!/usr/bin/env ork.python
"""
Test: Verify RGBA16F textures sample correctly on GPU.

Creates RGBA16F and RGBA8 textures with known solid colors,
renders textured quads, captures the output, and verifies
pixel colors are correct (not grayscale, not garbled).

This tests the GPU texture sampling path that the XIR/PBR
pipeline uses for EXR-sourced environment maps.
"""

import os, sys, struct, time
os.environ["PYTHONUNBUFFERED"] = "1"
sys.stdout.reconfigure(line_buffering=True)
from orkengine.core import vec2, vec3, vec4, mtx4
from orkengine import core, lev2

tokens = core.CrcStringProxy()

###############################################################################
# Half-float utilities
###############################################################################

def float_to_half(f):
    """Convert Python float to IEEE 754 half-precision uint16."""
    raw = struct.pack('<f', f)
    bits = struct.unpack('<I', raw)[0]
    sign = (bits >> 16) & 0x8000
    exp32 = ((bits >> 23) & 0xFF) - 127 + 15
    mant = bits & 0x007FFFFF
    if exp32 <= 0:
        return sign  # zero/denorm -> zero
    if exp32 >= 31:
        return sign | 0x7C00  # infinity
    return sign | (exp32 << 10) | (mant >> 13)

def half_to_float(h):
    """Convert IEEE 754 half-precision uint16 to Python float."""
    sign = (h >> 15) & 1
    exp = (h >> 10) & 0x1F
    mant = h & 0x3FF
    if exp == 0:
        if mant == 0:
            return (-1)**sign * 0.0
        return (-1)**sign * (2**-14) * (mant / 1024.0)
    elif exp == 31:
        return float('inf') if mant == 0 else float('nan')
    else:
        return (-1)**sign * (2**(exp - 15)) * (1.0 + mant / 1024.0)

###############################################################################
# Pixel reading
###############################################################################

def read_rgba8_pixel(image, x, y):
    """Read RGBA8 pixel as (R,G,B,A) floats in [0,1]."""
    w = image.width
    offset = (y * w + x) * 4
    data = bytes(image.data.bytes)
    raw = data[offset:offset+4]
    if len(raw) < 4:
        return None
    r, g, b, a = struct.unpack('BBBB', raw)
    return (r / 255.0, g / 255.0, b / 255.0, a / 255.0)

def read_rgba16f_pixel(image, x, y):
    """Read RGBA16F pixel as (R,G,B,A) floats."""
    w = image.width
    offset = (y * w + x) * 8
    data = bytes(image.data.bytes)
    raw = data[offset:offset+8]
    if len(raw) < 8:
        return None
    r, g, b, a = struct.unpack('<HHHH', raw)
    return (half_to_float(r), half_to_float(g), half_to_float(b), half_to_float(a))

###############################################################################
# Image creation
###############################################################################

def create_solid_rgba16f_image(w, h, r, g, b, a=1.0):
    """Create an RGBA16F Image filled with a solid color."""
    img = lev2.Image()
    img.initWithFormat(w, h, tokens.RGBA16F)
    # Write half-float pixel data
    rh = float_to_half(r)
    gh = float_to_half(g)
    bh = float_to_half(b)
    ah = float_to_half(a)
    pixel = struct.pack('<HHHH', rh, gh, bh, ah)
    row = pixel * w
    all_data = row * h
    mv = img.data.mutable_bytes
    # Copy data into mutable_bytes memoryview
    mv_bytes = bytes(all_data)
    for i in range(len(mv_bytes)):
        mv[i] = mv_bytes[i]
    return img

def create_solid_rgba8_image(w, h, r, g, b, a=255):
    """Create an RGBA8 Image filled with a solid color."""
    img = lev2.Image()
    img.initWithFormat(w, h, tokens.RGBA8)
    pixel = struct.pack('BBBB', r, g, b, a)
    row = pixel * w
    all_data = row * h
    mv = img.data.mutable_bytes
    mv_bytes = bytes(all_data)
    for i in range(len(mv_bytes)):
        mv[i] = mv_bytes[i]
    return img

###############################################################################
# Fullscreen quad with UVs
###############################################################################

def create_textured_quad(gbi):
    """Create a fullscreen quad with proper UV coordinates (0,0)-(1,1)."""
    vtx_t = lev2.VtxV12N12B12T8C4
    vbuf = vtx_t.staticBuffer(6)
    vw = gbi.lock(vbuf, 6)
    n = vec3(0, 0, 1)
    b = vec3(1, 0, 0)
    white = 0xFFFFFFFF
    # Two triangles: (-1,-1)-(1,1) with UV (0,0)-(1,1)
    vw.add(vtx_t(vec3(-1, -1, 0), n, b, vec2(0, 0), white))
    vw.add(vtx_t(vec3( 1,  1, 0), n, b, vec2(1, 1), white))
    vw.add(vtx_t(vec3( 1, -1, 0), n, b, vec2(1, 0), white))
    vw.add(vtx_t(vec3(-1, -1, 0), n, b, vec2(0, 0), white))
    vw.add(vtx_t(vec3(-1,  1, 0), n, b, vec2(0, 1), white))
    vw.add(vtx_t(vec3( 1,  1, 0), n, b, vec2(1, 1), white))
    gbi.unlock(vw)
    return vbuf, vw

###############################################################################
# Test runner
###############################################################################

def test_texture_sampling(ctx, ezapp, fbi, gbi, txi, mtl, permu, pipeline,
                          tex_img, tex_format_name, cap_fmt, expected_color,
                          color_name, width, height):
    """
    Upload tex_img as GPU texture, render a textured quad, capture, verify color.
    expected_color is (R,G,B) floats in [0,1].
    Returns (pass, message).
    """
    label = f"tex={tex_format_name} cap={cap_fmt} color={color_name}"

    # Upload texture
    tex = lev2.Texture(f"test_{tex_format_name}_{color_name}")
    txi.updateTexture(tex, tex_img, False)

    # Bind texture to ColorMap parameter
    pipeline.bindParam(mtl.param("ColorMap"), tex)

    # Create RTG (always RGBA32F for max precision)
    rtg = lev2.RtGroup(ctx, width, height)
    rtb = rtg.createBuffer(tokens.RGBA32F, tokens.color)
    rtb.clearColor = vec4(0, 0, 0, 1)

    # Create textured quad
    vbuf, vw = create_textured_quad(gbi)

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

    if cap_fmt == "RGBA8":
        pixel = read_rgba8_pixel(img, cx, cy)
    elif cap_fmt == "RGBA16F":
        pixel = read_rgba16f_pixel(img, cx, cy)
    else:
        return False, f"{label}: unsupported cap format"

    if pixel is None:
        return False, f"{label}: failed to read pixel"

    r, g, b, a = pixel
    fmt_info = f"R={r:.4f} G={g:.4f} B={b:.4f} A={a:.4f}"
    print(f"  {label}: {fmt_info}")

    er, eg, eb = expected_color
    tol = 0.15  # tolerance

    # Verify each channel is close to expected
    if abs(r - er) > tol:
        return False, f"{label}: R mismatch (got {r:.4f}, expected {er:.1f}) - {fmt_info}"
    if abs(g - eg) > tol:
        return False, f"{label}: G mismatch (got {g:.4f}, expected {eg:.1f}) - {fmt_info}"
    if abs(b - eb) > tol:
        return False, f"{label}: B mismatch (got {b:.4f}, expected {eb:.1f}) - {fmt_info}"

    return True, f"{label}: OK ({fmt_info})"

###############################################################################

def main():
    print("=" * 70)
    print("Test: RGBA16F Texture Sampling Verification")
    print("=" * 70)

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    fbi = ctx.FBI
    gbi = ctx.GBI
    txi = ctx.TXI
    ezapp.mainThreadBegin()

    W, H = 64, 64

    # Setup material with texcolor technique (texture sampling, UV-based)
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInit(ctx, "orkshader://solid.fxv2")
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = mtl.shader.technique("texcolor")
    pipeline = mtl.fxcache.findPipeline(permu)
    pipeline.bindParam(mtl.param("MatMVP"), mtx4())

    errors = 0

    # Test cases: (image_creator, tex_format_name, cap_format, expected_rgb, color_name)
    test_cases = [
        # === Baseline: RGBA8 texture, RGBA8 capture (known working) ===
        (lambda: create_solid_rgba8_image(W, H, 255, 0, 0),
         "RGBA8", "RGBA8", (1.0, 0.0, 0.0), "RED"),
        (lambda: create_solid_rgba8_image(W, H, 0, 255, 0),
         "RGBA8", "RGBA8", (0.0, 1.0, 0.0), "GREEN"),
        (lambda: create_solid_rgba8_image(W, H, 0, 0, 255),
         "RGBA8", "RGBA8", (0.0, 0.0, 1.0), "BLUE"),

        # === RGBA16F texture, RGBA8 capture (isolates texture sampling) ===
        (lambda: create_solid_rgba16f_image(W, H, 1.0, 0.0, 0.0),
         "RGBA16F", "RGBA8", (1.0, 0.0, 0.0), "RED"),
        (lambda: create_solid_rgba16f_image(W, H, 0.0, 1.0, 0.0),
         "RGBA16F", "RGBA8", (0.0, 1.0, 0.0), "GREEN"),
        (lambda: create_solid_rgba16f_image(W, H, 0.0, 0.0, 1.0),
         "RGBA16F", "RGBA8", (0.0, 0.0, 1.0), "BLUE"),

        # === RGBA16F texture, RGBA16F capture (full RGBA16F pipeline) ===
        (lambda: create_solid_rgba16f_image(W, H, 1.0, 0.0, 0.0),
         "RGBA16F", "RGBA16F", (1.0, 0.0, 0.0), "RED"),
        (lambda: create_solid_rgba16f_image(W, H, 0.0, 1.0, 0.0),
         "RGBA16F", "RGBA16F", (0.0, 1.0, 0.0), "GREEN"),
        (lambda: create_solid_rgba16f_image(W, H, 0.0, 0.0, 1.0),
         "RGBA16F", "RGBA16F", (0.0, 0.0, 1.0), "BLUE"),
    ]

    for create_img, tex_fmt, cap_fmt, expected, color_name in test_cases:
        img = create_img()
        ok, msg = test_texture_sampling(
            ctx, ezapp, fbi, gbi, txi, mtl, permu, pipeline,
            img, tex_fmt, cap_fmt, expected, color_name, W, H)
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {msg}")
        if not ok:
            errors += 1

    print("\n" + "=" * 70)
    if errors == 0:
        print("ALL PASSED: Texture sampling works correctly for both RGBA8 and RGBA16F")
    else:
        print(f"FAILED: {errors} test(s) failed - texture sampling may be corrupted")
    print("=" * 70)

    ezapp.mainThreadEnd()
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
