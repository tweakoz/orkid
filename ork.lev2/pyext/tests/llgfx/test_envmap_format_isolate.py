#!/usr/bin/env ork.python
"""
Diagnostic test: isolate whether envmap filter BLACK output is caused by
RGBA32F *format* or by HDR *values*.

Test matrix:
  A) RGBA8 texture with LDR color          → baseline (should work)
  B) RGBA32F texture with LDR-range values  → tests format hypothesis
  C) RGBA32F texture with moderate HDR       → tests value range hypothesis
  D) RGBA32F texture with extreme HDR        → tests overflow hypothesis

All tested with equirectangular specular filter at roughness=0.5.
If A passes and B fails → format/pipeline issue.
If A,B pass and C,D fail → HDR value issue.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import struct, time, math
from orkengine.core import vec2, vec3, vec4, mtx4
from orkengine import core, lev2

tokens = core.CrcStringProxy()

###############################################################################
# Utilities
###############################################################################

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

def inspect_rgba32f(capbuf, x, y):
    img = capbuf.image
    if not img or img.width == 0:
        return None
    w = img.width
    data = bytes(img.data.bytes)
    off = (y * w + x) * 16
    return struct.unpack('<ffff', data[off:off+16])

def sample_grid_rgba32f(capbuf, grid=4):
    """Sample a grid of pixels, return (avg_r, avg_g, avg_b, min_r, max_r, nan_count)."""
    img = capbuf.image
    if not img or img.width == 0:
        return None
    w, h = img.width, img.height
    data = bytes(img.data.bytes)
    rs, gs, bs = [], [], []
    nan_count = 0
    inf_count = 0
    for gy in range(grid):
        for gx in range(grid):
            px = int((gx + 0.5) * w / grid)
            py = int((gy + 0.5) * h / grid)
            off = (py * w + px) * 16
            r, g, b, a = struct.unpack('<ffff', data[off:off+16])
            if math.isnan(r) or math.isnan(g) or math.isnan(b):
                nan_count += 1
                continue
            if math.isinf(r) or math.isinf(g) or math.isinf(b):
                inf_count += 1
                continue
            rs.append(r); gs.append(g); bs.append(b)
    if not rs:
        return {'avg': (0,0,0), 'min_r': 0, 'max_r': 0, 'nan': nan_count, 'inf': inf_count, 'n': 0}
    n = len(rs)
    return {
        'avg': (sum(rs)/n, sum(gs)/n, sum(bs)/n),
        'min_r': min(rs), 'max_r': max(rs),
        'nan': nan_count, 'inf': inf_count, 'n': n
    }

###############################################################################
# Image creation
###############################################################################

def create_rgba8_image(w, h, r, g, b):
    """Create RGBA8 Image with solid color (r,g,b in [0,1])."""
    img = lev2.Image()
    img.initWithFormat(w, h, tokens.RGBA8)
    ri, gi, bi = int(r * 255), int(g * 255), int(b * 255)
    pixel = struct.pack('BBBB', ri, gi, bi, 255)
    all_data = pixel * (w * h)
    mv = img.data.mutable_bytes
    for i in range(len(all_data)):
        mv[i] = all_data[i]
    return img

def create_rgba32f_image(w, h, r, g, b):
    """Create RGBA32F Image with solid color (r,g,b as float)."""
    img = lev2.Image()
    img.initWithFormat(w, h, tokens.RGBA32F)
    pixel = struct.pack('<ffff', r, g, b, 1.0)
    all_data = pixel * (w * h)
    mv = img.data.mutable_bytes
    for i in range(len(all_data)):
        mv[i] = all_data[i]
    return img

def create_rgb32f_image(w, h, r, g, b):
    """Create RGB32F (3-channel) Image with solid color."""
    img = lev2.Image()
    img.initWithFormat(w, h, tokens.RGB32F)
    pixel = struct.pack('<fff', r, g, b)
    all_data = pixel * (w * h)
    mv = img.data.mutable_bytes
    for i in range(len(all_data)):
        mv[i] = all_data[i]
    return img

###############################################################################
# Filter + capture
###############################################################################

TILE_SIZE = 32

def filter_and_capture(ctx, fbi, ezapp, mtl, technique_name, src_tex,
                       roughness, fw, fh, numsamples):
    """Run filter shader, capture as RGBA32F, return CaptureBuffer."""
    dwi = ctx.DWI
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = mtl.shader.technique(technique_name)
    assert permu.technique, f"technique {technique_name} not found"
    pipeline = mtl.fxcache.findPipeline(permu)

    pipeline.bindParam(mtl.param("mvp"), mtx4())
    pipeline.bindParam(mtl.param("prefiltmap"), src_tex)
    pipeline.bindParam(mtl.param("roughness"), float(roughness))
    pipeline.bindParam(mtl.param("imgdim"), vec2(fw, fh))
    pipeline.bindParam(mtl.param("numsamples"), int(numsamples))
    vs_param = mtl.param("ViewportSize")
    if vs_param:
        pipeline.bindParam(vs_param, vec2(fw, fh))
    ivs_param = mtl.param("InvViewportSize")
    if ivs_param:
        pipeline.bindParam(ivs_param, vec2(1.0/fw, 1.0/fh))

    rtg = lev2.RtGroup(ctx, fw, fh)
    rtb = rtg.createBuffer(tokens.RGBA32F, tokens.color)
    rtb.clearColor = vec4(0, 0, 0, 1)

    num_tiles_x = (fw + TILE_SIZE - 1) // TILE_SIZE
    num_tiles_y = (fh + TILE_SIZE - 1) // TILE_SIZE

    capbuf = lev2.CaptureBuffer()
    ctx.beginFrame()
    fbi.rtGroupPush(rtg)
    fbi.rtGroupClear(rtg)
    RCFD = lev2.RenderContextFrameData(ctx)
    RCID = lev2.RenderContextInstData(RCFD)
    RCID.forceTechnique(permu.technique)
    RCID.genMatrix(lambda: mtx4())

    def draw_tiles():
        for ty in range(num_tiles_y):
            for tx in range(num_tiles_x):
                tile_x = tx * TILE_SIZE
                tile_y = ty * TILE_SIZE
                tile_w = min(TILE_SIZE, fw - tile_x)
                tile_h = min(TILE_SIZE, fh - tile_y)
                ndc_x = tile_x / fw * 2.0 - 1.0
                ndc_y = tile_y / fh * 2.0 - 1.0
                ndc_w = tile_w / fw * 2.0
                ndc_h = tile_h / fh * 2.0
                uv_x = tile_x / fw
                uv_y = tile_y / fh
                uv_w = tile_w / fw
                uv_h = tile_h / fh
                dwi.quad2D(
                    vec4(ndc_x, ndc_y, ndc_w, ndc_h),
                    vec4(uv_x, uv_y, uv_w, uv_h))

    pipeline.wrappedDrawCall(RCID, draw_tiles)
    fbi.rtGroupPop()
    future = fbi.captureAsFormat(rtb, capbuf, "RGBA32F")
    ctx.endFrame()

    start = time.time()
    while not future.is_ready:
        ezapp.mainThreadIter()
        if time.time() - start > 60:
            print("  TIMEOUT!")
            return None
    return capbuf

###############################################################################
# Main
###############################################################################

def main():
    print("=" * 70)
    print("Diagnostic: RGBA32F format vs HDR value isolation")
    print("=" * 70)

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    fbi = ctx.FBI
    txi = ctx.TXI
    ezapp.mainThreadBegin()

    # Material
    spec_mtl = lev2.FreestyleMaterial()
    spec_mtl.gpuInit(ctx, "orkshader://pbr_filterenv.fxv2")

    TEX_W, TEX_H = 128, 64  # small equirectangular
    FILTER_W, FILTER_H = 64, 32
    ROUGHNESS = 0.5
    NUMSAMPLES = 1024  # enough to get meaningful results, fast enough to iterate
    TECHNIQUE = "tek_filterSpecularMapEquirectangular"

    # Test cases: (label, image_creator, color_rgb, expect_pass_description)
    test_cases = [
        ("A: RGBA8 LDR (0.8, 0.2, 0.1)",
         lambda: create_rgba8_image(TEX_W, TEX_H, 0.8, 0.2, 0.1),
         "baseline"),
        ("B: RGBA32F LDR (0.8, 0.2, 0.1)",
         lambda: create_rgba32f_image(TEX_W, TEX_H, 0.8, 0.2, 0.1),
         "format test"),
        ("C: RGBA32F moderate HDR (5.0, 1.5, 0.3)",
         lambda: create_rgba32f_image(TEX_W, TEX_H, 5.0, 1.5, 0.3),
         "moderate HDR"),
        ("D: RGBA32F extreme HDR (500.0, 100.0, 20.0)",
         lambda: create_rgba32f_image(TEX_W, TEX_H, 500.0, 100.0, 20.0),
         "extreme HDR"),
        ("E: RGBA32F unit white (1.0, 1.0, 1.0)",
         lambda: create_rgba32f_image(TEX_W, TEX_H, 1.0, 1.0, 1.0),
         "white reference"),
    ]

    errors = 0

    for label, img_fn, desc in test_cases:
        print(f"\n--- {label} ({desc}) ---")

        # Create image and upload to texture
        img = img_fn()
        print(f"  Image: {img.width}x{img.height} nc={img.numcomponents} bpc={img.bytesPerChannel}")

        tex = lev2.Texture(label)
        txi.updateTexture(tex, img, False)
        print(f"  Texture: {tex.width}x{tex.height}")

        # Set equirectangular wrap mode
        tex.setAddressMode(tokens.WRAP, tokens.CLAMP, tokens.CLAMP)
        txi.applySamplingMode(tex)

        # Run filter at roughness=0.5
        capbuf = filter_and_capture(
            ctx, fbi, ezapp, spec_mtl, TECHNIQUE, tex,
            ROUGHNESS, FILTER_W, FILTER_H, NUMSAMPLES)

        if capbuf is None:
            print(f"  [FAIL] {label}: capture failed")
            errors += 1
            continue

        stats = sample_grid_rgba32f(capbuf)
        if stats is None:
            print(f"  [FAIL] {label}: no data")
            errors += 1
            continue

        avg_r, avg_g, avg_b = stats['avg']
        print(f"  Output avg: R={avg_r:.6f} G={avg_g:.6f} B={avg_b:.6f}")
        print(f"  Output R range: [{stats['min_r']:.6f}, {stats['max_r']:.6f}]")
        print(f"  NaN pixels: {stats['nan']}, Inf pixels: {stats['inf']}, valid: {stats['n']}")

        center = inspect_rgba32f(capbuf, FILTER_W//2, FILTER_H//2)
        print(f"  Center pixel: R={center[0]:.6f} G={center[1]:.6f} B={center[2]:.6f} A={center[3]:.6f}")

        is_black = (avg_r < 0.001 and avg_g < 0.001 and avg_b < 0.001)
        has_nan = stats['nan'] > 0
        has_inf = stats['inf'] > 0

        if is_black:
            print(f"  [BLACK] {label}")
            errors += 1
        elif has_nan:
            print(f"  [NaN DETECTED] {label}")
            errors += 1
        elif has_inf:
            print(f"  [Inf DETECTED] {label}")
            errors += 1
        else:
            print(f"  [PASS] {label}")

    # Also test roughness=0 with RGBA32F to confirm that works
    print(f"\n--- F: RGBA32F LDR r=0 (passthrough control) ---")
    img = create_rgba32f_image(TEX_W, TEX_H, 0.8, 0.2, 0.1)
    tex = lev2.Texture("control_r0")
    txi.updateTexture(tex, img, False)
    tex.setAddressMode(tokens.WRAP, tokens.CLAMP, tokens.CLAMP)
    txi.applySamplingMode(tex)
    capbuf = filter_and_capture(
        ctx, fbi, ezapp, spec_mtl, TECHNIQUE, tex,
        0.0, FILTER_W, FILTER_H, NUMSAMPLES)
    if capbuf:
        stats = sample_grid_rgba32f(capbuf)
        avg_r, avg_g, avg_b = stats['avg']
        center = inspect_rgba32f(capbuf, FILTER_W//2, FILTER_H//2)
        print(f"  Output avg: R={avg_r:.6f} G={avg_g:.6f} B={avg_b:.6f}")
        print(f"  Center pixel: R={center[0]:.6f} G={center[1]:.6f} B={center[2]:.6f} A={center[3]:.6f}")
        print(f"  NaN: {stats['nan']}, Inf: {stats['inf']}")
        is_black = (avg_r < 0.001 and avg_g < 0.001 and avg_b < 0.001)
        print(f"  [{'BLACK' if is_black else 'PASS'}] RGBA32F r=0 control")
        if is_black:
            errors += 1

    ###########################################################################
    # G-J: Synthetic EXR file roundtrip through file loading path
    # Write RGBA32F image → EXR file → createFromFile → texture → filter
    ###########################################################################
    import tempfile
    tmpdir = tempfile.mkdtemp(prefix="envfmt_isolate_")

    exr_file_tests = [
        ("G: EXR4ch LDR (0.8, 0.2, 0.1)", 0.8, 0.2, 0.1, "4ch file loading LDR", 4),
        ("H: EXR4ch moderate HDR (5.0, 1.5, 0.3)", 5.0, 1.5, 0.3, "4ch file loading HDR", 4),
        ("I: EXR4ch extreme HDR (500.0, 100.0, 20.0)", 500.0, 100.0, 20.0, "4ch file loading extreme", 4),
        ("J: EXR3ch LDR (0.8, 0.2, 0.1)", 0.8, 0.2, 0.1, "3ch file → 3→4 expansion", 3),
        ("K: EXR3ch moderate HDR (5.0, 1.5, 0.3)", 5.0, 1.5, 0.3, "3ch file → 3→4 expansion HDR", 3),
        ("L: EXR3ch extreme HDR (500.0, 100.0, 20.0)", 500.0, 100.0, 20.0, "3ch file → 3→4 expansion extreme", 3),
    ]

    for label, cr, cg, cb, desc, nch in exr_file_tests:
        print(f"\n--- {label} ({desc}) ---")

        # Create image and write to EXR
        if nch == 3:
            src_img = create_rgb32f_image(TEX_W, TEX_H, cr, cg, cb)
        else:
            src_img = create_rgba32f_image(TEX_W, TEX_H, cr, cg, cb)
        exr_path = os.path.join(tmpdir, label.split(":")[0].strip() + ".exr")
        src_img.writeToFile(exr_path)
        print(f"  Wrote EXR: {exr_path} ({src_img.numcomponents}ch, bpc={src_img.bytesPerChannel})")

        # Load back through file loading path (exercises initFromInMemoryFile)
        loaded_img = lev2.Image.createFromFile(exr_path)
        if not loaded_img or loaded_img.width == 0:
            print(f"  [FAIL] {label}: could not load EXR back")
            errors += 1
            continue
        print(f"  Loaded: {loaded_img.width}x{loaded_img.height} nc={loaded_img.numcomponents} bpc={loaded_img.bytesPerChannel}")

        # Verify loaded pixel data before upload
        loaded_data = bytes(loaded_img.data.bytes)
        bpc = loaded_img.bytesPerChannel
        nc = loaded_img.numcomponents
        pixel_stride = nc * bpc
        mid_off = (TEX_H // 2 * TEX_W + TEX_W // 2) * pixel_stride
        if bpc == 4 and nc == 4:
            lr, lg, lb, la = struct.unpack('<ffff', loaded_data[mid_off:mid_off+16])
            print(f"  Loaded center pixel: R={lr:.6f} G={lg:.6f} B={lb:.6f} A={la:.6f}")
        elif bpc == 2 and nc == 4:
            rh, gh, bh, ah = struct.unpack('<HHHH', loaded_data[mid_off:mid_off+8])
            lr, lg, lb, la = half_to_float(rh), half_to_float(gh), half_to_float(bh), half_to_float(ah)
            print(f"  Loaded center pixel (half): R={lr:.6f} G={lg:.6f} B={lb:.6f} A={la:.6f}")
        else:
            print(f"  Loaded pixel: bpc={bpc} nc={nc} (unexpected)")

        # Upload and filter
        tex = lev2.Texture(label)
        txi.updateTexture(tex, loaded_img, False)
        print(f"  Texture: {tex.width}x{tex.height}")
        tex.setAddressMode(tokens.WRAP, tokens.CLAMP, tokens.CLAMP)
        txi.applySamplingMode(tex)

        capbuf = filter_and_capture(
            ctx, fbi, ezapp, spec_mtl, TECHNIQUE, tex,
            ROUGHNESS, FILTER_W, FILTER_H, NUMSAMPLES)

        if capbuf is None:
            print(f"  [FAIL] {label}: capture failed")
            errors += 1
            continue

        stats = sample_grid_rgba32f(capbuf)
        avg_r, avg_g, avg_b = stats['avg']
        center = inspect_rgba32f(capbuf, FILTER_W//2, FILTER_H//2)
        print(f"  Filter output avg: R={avg_r:.6f} G={avg_g:.6f} B={avg_b:.6f}")
        print(f"  Filter center: R={center[0]:.6f} G={center[1]:.6f} B={center[2]:.6f} A={center[3]:.6f}")
        print(f"  NaN: {stats['nan']}, Inf: {stats['inf']}")

        is_black = (avg_r < 0.001 and avg_g < 0.001 and avg_b < 0.001)
        if is_black:
            print(f"  [BLACK] {label}")
            errors += 1
        elif stats['nan'] > 0:
            print(f"  [NaN DETECTED] {label}")
            errors += 1
        else:
            print(f"  [PASS] {label}")

    import shutil
    shutil.rmtree(tmpdir, ignore_errors=True)

    print("\n" + "=" * 70)
    print("INTERPRETATION:")
    print("  If A-E pass, G-I pass, J-L fail → 3→4 channel expansion bug")
    print("  If A-E pass, G-L pass → fix works, bug was in 3→4 expansion")
    print("  If A-E pass, G-I fail → bug in EXR file save or load path")
    print("  If A passes, B fails → format/pipeline issue (not HDR values)")
    print("  If A,B pass, C or D fail → HDR value overflow/NaN issue")
    print("=" * 70)
    if errors:
        print(f"ISSUES: {errors}")
    else:
        print("ALL PASSED")

    ezapp.mainThreadEnd()
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
