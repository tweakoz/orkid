#!/usr/bin/env ork.python
"""
Test: EnvMapFilterSteps - step-by-step environment map filtering (pure Python).

Runs specular and diffuse filtering using FreestyleMaterial + pipeline,
with capture in a separate frame to ensure proper command buffer submission.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import struct, time, tempfile, argparse
from orkengine.core import vec2, vec3, vec4, mtx4
from orkengine import core, lev2

tokens = core.CrcStringProxy()

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
# Pixel inspection
###############################################################################

def inspect_capture(capbuf, fmt_name):
    """Read center pixel from a captured image."""
    img = capbuf.image
    if not img or img.width == 0:
        return None
    w, h = img.width, img.height
    cx, cy = w // 2, h // 2
    data = bytes(img.data.bytes)

    if fmt_name == "RGBA8":
        off = (cy * w + cx) * 4
        r, g, b, a = struct.unpack('BBBB', data[off:off+4])
        return (r/255.0, g/255.0, b/255.0, a/255.0)
    elif fmt_name == "RGBA16F":
        off = (cy * w + cx) * 8
        rh, gh, bh, ah = struct.unpack('<HHHH', data[off:off+8])
        return (half_to_float(rh), half_to_float(gh), half_to_float(bh), half_to_float(ah))
    elif fmt_name == "RGBA32F":
        off = (cy * w + cx) * 16
        r, g, b, a = struct.unpack('<ffff', data[off:off+16])
        return (r, g, b, a)
    return None

def pixel_stats(capbuf, fmt_name):
    """Get min/max/avg for each channel and per-pixel grayscale fraction."""
    img = capbuf.image
    if not img or img.width == 0:
        return None
    w, h = img.width, img.height
    data = bytes(img.data.bytes)

    rvals, gvals, bvals = [], [], []
    gray_count = 0
    if fmt_name == "RGBA32F":
        stride = 16
    elif fmt_name == "RGBA16F":
        stride = 8
    else:
        stride = 4

    for sy in range(0, h, max(1, h//16)):
        for sx in range(0, w, max(1, w//16)):
            off = (sy * w + sx) * stride
            if fmt_name == "RGBA8":
                pr, pg, pb, pa = struct.unpack('BBBB', data[off:off+4])
                r, g, b = pr/255.0, pg/255.0, pb/255.0
            elif fmt_name == "RGBA16F":
                rh, gh, bh, ah = struct.unpack('<HHHH', data[off:off+8])
                r, g, b = half_to_float(rh), half_to_float(gh), half_to_float(bh)
            elif fmt_name == "RGBA32F":
                r, g, b, a = struct.unpack('<ffff', data[off:off+16])
            else:
                continue
            rvals.append(r)
            gvals.append(g)
            bvals.append(b)
            brightness = max(r, g, b)
            if brightness > 0.01:
                tol = brightness * 0.05
                if abs(r - g) < tol and abs(r - b) < tol and abs(g - b) < tol:
                    gray_count += 1
            else:
                gray_count += 1

    if not rvals:
        return None
    n = len(rvals)
    return {
        'r': (min(rvals), max(rvals), sum(rvals)/n),
        'g': (min(gvals), max(gvals), sum(gvals)/n),
        'b': (min(bvals), max(bvals), sum(bvals)/n),
        'n': n,
        'gray_frac': gray_count / n,
    }

def is_grayscale(stats, threshold=0.85):
    if stats is None:
        return True
    return stats.get('gray_frac', 1.0) > threshold

def print_stats(stats):
    if stats is None:
        print("    (no data)")
        return
    for ch in ('r', 'g', 'b'):
        mn, mx, avg = stats[ch]
        print(f"    {ch.upper()}: min={mn:.4f} max={mx:.4f} avg={avg:.4f}")
    print(f"    gray_frac={stats.get('gray_frac', -1):.3f} ({stats['n']} samples)")

###############################################################################

def create_solid_color_exr(path, w, h, r, g, b):
    """Create a solid-color EXR file using RGBA16F."""
    img = lev2.Image()
    img.initWithFormat(w, h, tokens.RGBA16F)
    rh = float_to_half(r)
    gh = float_to_half(g)
    bh = float_to_half(b)
    ah = float_to_half(1.0)
    pixel = struct.pack('<HHHH', rh, gh, bh, ah)
    all_data = pixel * (w * h)
    mv = img.data.mutable_bytes
    for i in range(len(all_data)):
        mv[i] = all_data[i]
    img.writeToFile(path)
    return path

def create_solid_color_png(path, w, h, r, g, b):
    """Create a solid-color PNG file using RGBA8."""
    img = lev2.Image()
    img.initWithFormat(w, h, tokens.RGBA8)
    ri, gi, bi = int(r*255), int(g*255), int(b*255)
    pixel = struct.pack('BBBB', ri, gi, bi, 255)
    all_data = pixel * (w * h)
    mv = img.data.mutable_bytes
    for i in range(len(all_data)):
        mv[i] = all_data[i]
    img.writeToFile(path)
    return path

###############################################################################

def capture_rtg(rtg, cap_fmt_str, ctx, fbi, ezapp):
    """Capture an RTGroup's first color buffer in a separate frame.
    cap_fmt_str is "RGBA8" or "RGBA16F"."""
    rtb = rtg.buffer(0)
    capbuf = lev2.CaptureBuffer()
    ctx.beginFrame()
    future = fbi.captureAsFormat(rtb, capbuf, cap_fmt_str)
    ctx.endFrame()
    start = time.time()
    while not future.is_ready:
        ezapp.mainThreadIter()
        if time.time() - start > 60:
            print("  TIMEOUT waiting for capture!")
            return None
    return capbuf

###############################################################################

def main():
    parser = argparse.ArgumentParser(description="EnvMapFilterSteps test (pure Python)")
    parser.add_argument("-i", "--input", type=str, default=None,
                        help="Path to an EXR file to use instead of synthetic test image")
    parser.add_argument("-w", "--width", type=int, default=None,
                        help="Filter output width")
    parser.add_argument("-H", "--height", type=int, default=None,
                        help="Filter output height (default: same as width)")
    parser.add_argument("-s", "--spec-samples", type=int, default=None,
                        help="Specular filter sample count")
    parser.add_argument("-d", "--diff-samples", type=int, default=None,
                        help="Diffuse filter sample count")
    args = parser.parse_args()

    print("=" * 70)
    print("Test: EnvMapFilterSteps - Pure Python Pipeline")
    print("=" * 70)

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    fbi = ctx.FBI
    gbi = ctx.GBI
    txi = ctx.TXI
    ezapp.mainThreadBegin()

    tmpdir = tempfile.mkdtemp(prefix="envmap_test_")
    TEX_W, TEX_H = 64, 64

    if args.input:
        exr_path = args.input
        png_path = create_solid_color_png(os.path.join(tmpdir, "red_test.png"), TEX_W, TEX_H, 1.0, 0.0, 0.0)
        print(f"\n  Using input EXR: {exr_path}")
        print(f"  PNG (synthetic): {png_path}")
    else:
        exr_path = create_solid_color_exr(os.path.join(tmpdir, "red_test.exr"), TEX_W, TEX_H, 1.0, 0.0, 0.0)
        png_path = create_solid_color_png(os.path.join(tmpdir, "red_test.png"), TEX_W, TEX_H, 1.0, 0.0, 0.0)
        print(f"\n  Created test images in {tmpdir}")

    errors = 0
    step_idx = [0]
    DEBUG_DIR = "/tmp/stepenv"
    os.makedirs(DEBUG_DIR, exist_ok=True)

    ###########################################################################
    # Load source textures via Image -> Texture
    ###########################################################################
    print("\n--- Step 1: Load Source Textures ---")

    def load_texture_from_image(path, name):
        img = lev2.Image.createFromFile(path)
        if not img or img.width == 0:
            return None
        print(f"  {name}: {img.width}x{img.height} nc={img.numcomponents} bpc={img.bytesPerChannel}")
        tex = lev2.Texture(name)
        txi.updateTexture(tex, img, False)
        print(f"  {name} texture: {tex.width}x{tex.height}")
        return tex

    tex_png = load_texture_from_image(png_path, "png_src")
    tex_exr = load_texture_from_image(exr_path, "exr_src")

    if not tex_png or not tex_exr:
        print("  [FAIL] Could not load textures")
        ezapp.mainThreadEnd()
        return 1

    # Set equirectangular wrap mode on EXR texture (S=WRAP for longitude, T/R=CLAMP for poles)
    tex_exr.setAddressMode(tokens.WRAP, tokens.CLAMP, tokens.CLAMP)
    txi.applySamplingMode(tex_exr)

    # Filter output size and sample counts
    if args.input:
        FILTER_W = args.width if args.width else tex_exr.width
        FILTER_H = args.height if args.height else tex_exr.height
        SPEC_SAMPLES = args.spec_samples if args.spec_samples else 8192
        DIFF_SAMPLES = args.diff_samples if args.diff_samples else 4096
    else:
        FILTER_W = args.width if args.width else 64
        FILTER_H = args.height if args.height else FILTER_W
        SPEC_SAMPLES = args.spec_samples if args.spec_samples else 8192
        DIFF_SAMPLES = args.diff_samples if args.diff_samples else 4096
    print(f"  Filter size: {FILTER_W}x{FILTER_H}, spec samples: {SPEC_SAMPLES}, diff samples: {DIFF_SAMPLES}")

    ###########################################################################
    # Setup filter materials + pipelines
    ###########################################################################
    print("\n--- Setup filter pipelines ---")

    filter_shader = "orkshader://pbr_filterenv.fxv2"

    # Specular material
    spec_mtl = lev2.FreestyleMaterial()
    spec_mtl.gpuInit(ctx, filter_shader)

    # Diffuse material
    diff_mtl = lev2.FreestyleMaterial()
    diff_mtl.gpuInit(ctx, filter_shader)

    def make_pipeline(mtl, technique_name):
        permu = lev2.FxPipelinePermutation()
        permu.rendermodel = "CUSTOM"
        permu.technique = mtl.shader.technique(technique_name)
        assert permu.technique, f"technique {technique_name} not found"
        pipeline = mtl.fxcache.findPipeline(permu)
        return pipeline, permu

    print("  Pipelines ready")

    ###########################################################################
    # Filter + capture helper
    ###########################################################################

    TILE_SIZE = 32
    TILES_PER_FRAME = 16

    def filter_and_capture(label, mtl, technique_name, src_tex,
                           roughness, fw, fh, numsamples, cap_fmt_str):
        """Render filter pass with tiled rendering, capture in same frame."""
        dwi = ctx.DWI
        pipeline, permu = make_pipeline(mtl, technique_name)

        # Bind params
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

        # Create RTG
        rtg = lev2.RtGroup(ctx, fw, fh)
        rtb = rtg.createBuffer(tokens.RGBA32F, tokens.color)
        rtb.clearColor = vec4(0, 0, 0, 1)

        # Tiled rendering to avoid GPU watchdog timeout
        num_tiles_x = (fw + TILE_SIZE - 1) // TILE_SIZE
        num_tiles_y = (fh + TILE_SIZE - 1) // TILE_SIZE

        # Render + capture in same frame
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

        # Capture in same frame (before endFrame, while command buffer is active)
        future = fbi.captureAsFormat(rtb, capbuf, cap_fmt_str)
        ctx.endFrame()

        # Wait for capture completion
        start = time.time()
        while not future.is_ready:
            ezapp.mainThreadIter()
            if time.time() - start > 60:
                print("  TIMEOUT waiting for capture!")
                return None

        print(f"    tiles: {num_tiles_x}x{num_tiles_y} ({TILE_SIZE}px)")
        return capbuf

    ###########################################################################
    # Check helper
    ###########################################################################
    def save_debug_image(label, capbuf):
        img = capbuf.image
        if not img or img.width == 0:
            print(f"    (no image to save)")
            return
        safe_name = label.replace(" ", "_").replace("(", "").replace(")", "").replace("=", "")
        fname = f"{step_idx[0]:02d}_{safe_name}.png"
        path = os.path.join(DEBUG_DIR, fname)
        img.writeToFile(path)
        print(f"    -> {path}")
        step_idx[0] += 1

    def check_result(label, capbuf, fmt, expect_color=True):
        nonlocal errors
        if capbuf is None:
            print(f"  [FAIL] {label}: capture failed")
            errors += 1
            return
        save_debug_image(label, capbuf)
        stats = pixel_stats(capbuf, fmt)
        center = inspect_capture(capbuf, fmt)
        if not center:
            print(f"  [FAIL] {label}: no pixel data")
            errors += 1
            return
        print(f"  {label}: center=({center[0]:.4f},{center[1]:.4f},{center[2]:.4f})")
        print_stats(stats)
        if stats['r'][2] < 0.001 and stats['g'][2] < 0.001 and stats['b'][2] < 0.001:
            print(f"  [FAIL] {label}: output is BLACK (all zeros)")
            errors += 1
            return
        if expect_color and is_grayscale(stats):
            print(f"  [FAIL] {label}: output is GRAYSCALE")
            errors += 1
            return
        print(f"  [PASS] {label}")

    CAP_FMT = "RGBA32F"

    ###########################################################################
    # Step 2: Specular at roughness=0
    ###########################################################################
    print("\n--- Step 2: Specular Filter (roughness=0, mirror) ---")

    capbuf = filter_and_capture(
        "PNG specular r=0", spec_mtl, "tek_filterSpecularMapStandard",
        tex_png, 0.0, FILTER_W, FILTER_H, SPEC_SAMPLES, CAP_FMT)
    check_result("PNG specular r=0", capbuf, CAP_FMT, expect_color=not args.input)

    capbuf = filter_and_capture(
        "EXR specular r=0 equirect", spec_mtl, "tek_filterSpecularMapEquirectangular",
        tex_exr, 0.0, FILTER_W, FILTER_H, SPEC_SAMPLES, CAP_FMT)
    check_result("EXR specular r=0 equirect", capbuf, CAP_FMT)

    ###########################################################################
    # Step 3: Specular at various roughness levels
    ###########################################################################
    print("\n--- Step 3: Specular Filter (various roughness) ---")

    for rough in [0.0, 0.25, 0.5, 1.0]:
        expect_clr = (rough == 0.0)
        capbuf = filter_and_capture(
            f"EXR specular r={rough:.2f}", spec_mtl, "tek_filterSpecularMapEquirectangular",
            tex_exr, rough, FILTER_W, FILTER_H, SPEC_SAMPLES, CAP_FMT)
        check_result(f"EXR specular r={rough:.2f} equirect", capbuf, CAP_FMT, expect_color=expect_clr)

    ###########################################################################
    # Step 4: Diffuse filtering
    ###########################################################################
    print("\n--- Step 4: Diffuse Filter ---")

    capbuf = filter_and_capture(
        "PNG diffuse standard", diff_mtl, "tek_filterDiffuseMapStandard",
        tex_png, 1.0, FILTER_W, FILTER_H, DIFF_SAMPLES, CAP_FMT)
    check_result("PNG diffuse standard", capbuf, CAP_FMT, expect_color=not args.input)

    capbuf = filter_and_capture(
        "EXR diffuse equirect", diff_mtl, "tek_filterDiffuseMapEquirectangular",
        tex_exr, 1.0, FILTER_W, FILTER_H, DIFF_SAMPLES, CAP_FMT)
    check_result("EXR diffuse equirect", capbuf, CAP_FMT, expect_color=False)

    ###########################################################################
    # Step 5: EXR with Standard technique
    ###########################################################################
    print("\n--- Step 5: EXR with Standard technique ---")

    capbuf = filter_and_capture(
        "EXR specular r=0 standard", spec_mtl, "tek_filterSpecularMapStandard",
        tex_exr, 0.0, FILTER_W, FILTER_H, SPEC_SAMPLES, CAP_FMT)
    check_result("EXR specular r=0 standard", capbuf, CAP_FMT)

    ###########################################################################
    print("\n" + "=" * 70)
    if errors == 0:
        print("ALL PASSED - no grayscale artifacts detected")
    else:
        print(f"FAILED: {errors} issue(s) found")
    print("=" * 70)

    import shutil
    shutil.rmtree(tmpdir, ignore_errors=True)
    ezapp.mainThreadEnd()
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
