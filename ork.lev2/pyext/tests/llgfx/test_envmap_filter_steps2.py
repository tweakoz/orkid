#!/usr/bin/env ork.python
"""
Test: EnvMapFilterSteps2 - isolate where color is lost in the EXR pipeline.

Step 0: Load source EXR as Image, write back out, upload to texture,
        render fullscreen quad, capture framebuffer, write captured image.
        This isolates: image load → texture upload → render → capture.

Remaining steps: same as test_envmap_filter_steps.py
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import struct, time, tempfile, argparse
from orkengine.core import vec2, vec3, vec4, mtx4
from orkengine import core, lev2

tokens = core.CrcStringProxy()

DEBUG_DIR = "/tmp/stepenv"

###############################################################################
# Half-float utilities
###############################################################################

def float_to_half(f):
    raw = struct.pack('<f', f)
    bits = struct.unpack('<I', raw)[0]
    sign = (bits >> 16) & 0x8000
    exp32 = ((bits >> 23) & 0xFF) - 127 + 15
    mant = bits & 0x007FFFFF
    if exp32 <= 0:
        return sign
    if exp32 >= 31:
        return sign | 0x7C00
    return sign | (exp32 << 10) | (mant >> 13)

def half_to_float(h):
    sign = (h >> 15) & 1
    exp = (h >> 10) & 0x1F
    mant = h & 0x3FF
    if exp == 0:
        return (-1)**sign * 0.0 if mant == 0 else (-1)**sign * (2**-14) * (mant / 1024.0)
    elif exp == 31:
        return float('inf') if mant == 0 else float('nan')
    else:
        return (-1)**sign * (2**(exp - 15)) * (1.0 + mant / 1024.0)

###############################################################################

def read_pixel(img, x, y):
    """Read pixel at (x,y) from an Image, return (R,G,B,A) as floats."""
    w = img.width
    nc = img.numcomponents
    bpc = img.bytesPerChannel
    data = bytes(img.data.bytes)
    stride = nc * bpc
    off = (y * w + x) * stride

    if bpc == 1 and nc == 4:  # RGBA8
        r, g, b, a = struct.unpack('BBBB', data[off:off+4])
        return (r/255.0, g/255.0, b/255.0, a/255.0)
    elif bpc == 1 and nc == 3:  # RGB8
        r, g, b = struct.unpack('BBB', data[off:off+3])
        return (r/255.0, g/255.0, b/255.0, 1.0)
    elif bpc == 2 and nc == 4:  # RGBA16F
        rh, gh, bh, ah = struct.unpack('<HHHH', data[off:off+8])
        return (half_to_float(rh), half_to_float(gh), half_to_float(bh), half_to_float(ah))
    elif bpc == 2 and nc == 3:  # RGB16F
        rh, gh, bh = struct.unpack('<HHH', data[off:off+6])
        return (half_to_float(rh), half_to_float(gh), half_to_float(bh), 1.0)
    elif bpc == 4 and nc == 4:  # RGBA32F
        r, g, b, a = struct.unpack('<ffff', data[off:off+16])
        return (r, g, b, a)
    elif bpc == 4 and nc == 3:  # RGB32F
        r, g, b = struct.unpack('<fff', data[off:off+12])
        return (r, g, b, 1.0)
    else:
        return None

def print_pixel(label, px):
    if px is None:
        print(f"  {label}: (unknown format)")
    else:
        print(f"  {label}: R={px[0]:.4f} G={px[1]:.4f} B={px[2]:.4f} A={px[3]:.4f}")

###############################################################################

def main():
    parser = argparse.ArgumentParser(description="EnvMapFilterSteps2 - roundtrip isolation test")
    parser.add_argument("-i", "--input", type=str, required=True,
                        help="Path to an EXR/PNG file to test")
    args = parser.parse_args()

    print("=" * 70)
    print("Test: EnvMapFilterSteps2 - Texture Roundtrip Isolation")
    print("=" * 70)

    os.makedirs(DEBUG_DIR, exist_ok=True)

    ###########################################################################
    # Step 0a: Load source image as Image object
    ###########################################################################
    print("\n--- Step 0a: Load source as Image ---")

    src_img = lev2.Image.createFromFile(args.input)
    if not src_img or src_img.width == 0:
        print("  [FAIL] Could not load image")
        return 1

    w, h = src_img.width, src_img.height
    nc = src_img.numcomponents
    bpc = src_img.bytesPerChannel
    fmt_code = src_img.format
    print(f"  Loaded: {w}x{h} nc={nc} bpc={bpc} format={fmt_code}")

    # Sample a few pixels from the source image
    test_points = [
        (w//2, h//4, "sky"),
        (w//2, h//2, "horizon"),
        (w//2, 3*h//4, "ground"),
        (w//4, h//2, "left"),
        (3*w//4, h//2, "right"),
    ]
    print("  Source image pixels:")
    for px, py_, label in test_points:
        pixel = read_pixel(src_img, px, py_)
        print_pixel(f"    [{label}] ({px},{py_})", pixel)

    ###########################################################################
    # Step 0b: Write source image back out
    ###########################################################################
    print("\n--- Step 0b: Write source image to disk ---")

    src_img.writeToFile(os.path.join(DEBUG_DIR, "RW.png"))
    print(f"  -> {DEBUG_DIR}/RW.png")
    src_img.writeToFile(os.path.join(DEBUG_DIR, "RW.exr"))
    print(f"  -> {DEBUG_DIR}/RW.exr")

    # Reload and verify
    reloaded = lev2.Image.createFromFile(os.path.join(DEBUG_DIR, "RW.exr"))
    if reloaded and reloaded.width > 0:
        print(f"  Reloaded EXR: {reloaded.width}x{reloaded.height} nc={reloaded.numcomponents} bpc={reloaded.bytesPerChannel}")
        print("  Reloaded image pixels:")
        for px, py_, label in test_points:
            pixel = read_pixel(reloaded, px, py_)
            print_pixel(f"    [{label}] ({px},{py_})", pixel)
    else:
        print("  [FAIL] Could not reload RW.exr")

    ###########################################################################
    # Step 0c: Upload to GPU texture, render to framebuffer, capture back
    ###########################################################################
    print("\n--- Step 0c: GPU roundtrip (texture -> render -> capture) ---")

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    fbi = ctx.FBI
    gbi = ctx.GBI
    txi = ctx.TXI
    ezapp.mainThreadBegin()

    # Create texture from source image
    tex = lev2.Texture("roundtrip_test")
    txi.updateTexture(tex, src_img, False)
    print(f"  Texture uploaded: {tex.width}x{tex.height}")

    # Setup textured quad pipeline (passthrough - no filtering)
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInit(ctx, "orkshader://solid.fxv2")
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = mtl.shader.technique("texcolor")
    pipeline = mtl.fxcache.findPipeline(permu)
    pipeline.bindParam(mtl.param("MatMVP"), mtx4())
    pipeline.bindParam(mtl.param("ColorMap"), tex)

    vtx_t = lev2.VtxV12N12B12T8C4
    n = vec3(0, 0, 1)
    b = vec3(1, 0, 0)
    white = 0xFFFFFFFF

    # Create fullscreen quad
    vbuf = vtx_t.staticBuffer(6)
    vw = gbi.lock(vbuf, 6)
    vw.add(vtx_t(vec3(-1, -1, 0), n, b, vec2(0, 0), white))
    vw.add(vtx_t(vec3( 1,  1, 0), n, b, vec2(1, 1), white))
    vw.add(vtx_t(vec3( 1, -1, 0), n, b, vec2(1, 0), white))
    vw.add(vtx_t(vec3(-1, -1, 0), n, b, vec2(0, 0), white))
    vw.add(vtx_t(vec3(-1,  1, 0), n, b, vec2(0, 1), white))
    vw.add(vtx_t(vec3( 1,  1, 0), n, b, vec2(1, 1), white))
    gbi.unlock(vw)

    # Render at source resolution with RGBA32F to preserve HDR
    rtg = lev2.RtGroup(ctx, w, h)
    rtb = rtg.createBuffer(tokens.RGBA32F, tokens.color)
    rtb.clearColor = vec4(0, 0, 0, 1)

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

    # Capture as RGBA8
    capbuf_rgba8 = lev2.CaptureBuffer()
    ctx.beginFrame()
    future8 = fbi.captureAsFormat(rtb, capbuf_rgba8, "RGBA8")
    ctx.endFrame()
    start = time.time()
    while not future8.is_ready:
        ezapp.mainThreadIter()
        if time.time() - start > 60:
            print("  [FAIL] RGBA8 capture TIMEOUT")
            ezapp.mainThreadEnd()
            return 1

    cap_img8 = capbuf_rgba8.image
    if cap_img8 and cap_img8.width > 0:
        cap_img8.writeToFile(os.path.join(DEBUG_DIR, "GPU_roundtrip_RGBA8.png"))
        print(f"  -> {DEBUG_DIR}/GPU_roundtrip_RGBA8.png ({cap_img8.width}x{cap_img8.height})")
        print("  Captured RGBA8 pixels:")
        for px, py_, label in test_points:
            # Scale test points if capture size differs
            cx = min(px, cap_img8.width - 1)
            cy = min(py_, cap_img8.height - 1)
            pixel = read_pixel(cap_img8, cx, cy)
            print_pixel(f"    [{label}] ({cx},{cy})", pixel)
    else:
        print("  [FAIL] RGBA8 capture produced no image")

    # Capture as RGBA16F
    capbuf_rgba16f = lev2.CaptureBuffer()
    ctx.beginFrame()
    future16 = fbi.captureAsFormat(rtb, capbuf_rgba16f, "RGBA16F")
    ctx.endFrame()
    start = time.time()
    while not future16.is_ready:
        ezapp.mainThreadIter()
        if time.time() - start > 60:
            print("  [FAIL] RGBA16F capture TIMEOUT")
            ezapp.mainThreadEnd()
            return 1

    cap_img16 = capbuf_rgba16f.image
    if cap_img16 and cap_img16.width > 0:
        cap_img16.writeToFile(os.path.join(DEBUG_DIR, "GPU_roundtrip_RGBA16F.png"))
        print(f"  -> {DEBUG_DIR}/GPU_roundtrip_RGBA16F.png ({cap_img16.width}x{cap_img16.height})")
        cap_img16.writeToFile(os.path.join(DEBUG_DIR, "GPU_roundtrip_RGBA16F.exr"))
        print(f"  -> {DEBUG_DIR}/GPU_roundtrip_RGBA16F.exr")
        print("  Captured RGBA16F pixels:")
        for px, py_, label in test_points:
            cx = min(px, cap_img16.width - 1)
            cy = min(py_, cap_img16.height - 1)
            pixel = read_pixel(cap_img16, cx, cy)
            print_pixel(f"    [{label}] ({cx},{cy})", pixel)
    else:
        print("  [FAIL] RGBA16F capture produced no image")

    ###########################################################################
    print("\n" + "=" * 70)
    print("Done. Compare images in /tmp/stepenv/:")
    print("  RW.png / RW.exr          — source image re-saved")
    print("  GPU_roundtrip_RGBA8.png   — after GPU texture+render+capture (8bit)")
    print("  GPU_roundtrip_RGBA16F.png — after GPU texture+render+capture (16F)")
    print("  GPU_roundtrip_RGBA16F.exr — after GPU texture+render+capture (16F)")
    print("=" * 70)

    ezapp.mainThreadEnd()
    return 0

if __name__ == "__main__":
    sys.exit(main())
