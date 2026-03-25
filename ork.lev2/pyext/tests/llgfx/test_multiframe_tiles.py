#!/usr/bin/env ork.python
"""
Test: Accumulation-based envmap filtering with real 8K EXR.
Uses _accum shader variants that output weighted sums (RGB=weighted color, A=weight).
Multiple passes per tile, each with SAMPLES_PER_PASS samples, then normalize in CPU.
"""

import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys; sys.stdout.reconfigure(line_buffering=True)
import struct, time, tempfile, argparse
from orkengine.core import vec2, vec3, vec4, mtx4
from orkengine import core, lev2

tokens = core.CrcStringProxy()

TILE_SIZE = 128
SAMPLES_PER_PASS = 2048

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

def float_to_half(f):
    raw = struct.pack('<f', f)
    bits = struct.unpack('<I', raw)[0]
    sign = (bits >> 16) & 0x8000
    exp32 = ((bits >> 23) & 0xFF) - 127 + 15
    mant = bits & 0x007FFFFF
    if exp32 <= 0: return sign
    if exp32 >= 31: return sign | 0x7C00
    return sign | (exp32 << 10) | (mant >> 13)

def render_single_tile(ctx, ezapp, fbi, dwi, pipeline, permu, fw, fh, tx, ty):
    rtg = lev2.RtGroup(ctx, fw, fh)
    rtb = rtg.createBuffer(tokens.RGBA32F, tokens.color)
    rtb.clearColor = vec4(0, 0, 0, 1)
    capbuf = lev2.CaptureBuffer()
    ctx.beginFrame()
    fbi.rtGroupPush(rtg)
    fbi.rtGroupClear(rtg)
    RCFD = lev2.RenderContextFrameData(ctx)
    RCID = lev2.RenderContextInstData(RCFD)
    RCID.forceTechnique(permu.technique)
    RCID.genMatrix(lambda: mtx4())
    tile_x = tx * TILE_SIZE
    tile_y = ty * TILE_SIZE
    tile_w = min(TILE_SIZE, fw - tile_x)
    tile_h = min(TILE_SIZE, fh - tile_y)
    def draw():
        dwi.quad2D(
            vec4(tile_x/fw*2-1, tile_y/fh*2-1, tile_w/fw*2, tile_h/fh*2),
            vec4(tile_x/fw, tile_y/fh, tile_w/fw, tile_h/fh))
    pipeline.wrappedDrawCall(RCID, draw)
    future = fbi.captureAsFormat(rtb, capbuf, "RGBA32F")
    fbi.rtGroupPop()
    ctx.endFrame()
    start = time.time()
    while not future.is_ready:
        ezapp.mainThreadIter()
        if time.time() - start > 120:
            raise TimeoutError("capture timeout")
    return capbuf

def accumulate_tile(accum, capbuf_img, tx, ty, fw, fh):
    tile_x = tx * TILE_SIZE
    tile_y = ty * TILE_SIZE
    tile_w = min(TILE_SIZE, fw - tile_x)
    tile_h = min(TILE_SIZE, fh - tile_y)
    src = bytes(capbuf_img.data.bytes)
    for row in range(tile_h):
        for col in range(tile_w):
            px, py = tile_x + col, tile_y + row
            off = (py * fw + px) * 16
            r, g, b, w = struct.unpack_from('<ffff', src, off)
            idx = py * fw + px
            accum[idx][0] += r
            accum[idx][1] += g
            accum[idx][2] += b
            accum[idx][3] += w

def check_accum(accum, fw, fh):
    rvals, gvals, bvals = [], [], []
    step = max(1, min(fw, fh) // 16)
    for sy in range(0, fh, step):
        for sx in range(0, fw, step):
            idx = sy * fw + sx
            r, g, b, w = accum[idx]
            if w > 0:
                r /= w; g /= w; b /= w
            rvals.append(r); gvals.append(g); bvals.append(b)
    n = len(rvals)
    return {
        'r': (min(rvals), max(rvals), sum(rvals)/n),
        'g': (min(gvals), max(gvals), sum(gvals)/n),
        'b': (min(bvals), max(bvals), sum(bvals)/n),
    }

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--input", type=str, required=True)
    parser.add_argument("-w", "--width", type=int, default=512)
    parser.add_argument("-H", "--height", type=int, default=256)
    parser.add_argument("-s", "--samples", type=int, default=8192)
    args = parser.parse_args()

    print("=" * 70)
    print("Test: Accumulation shader — per-tile, multi-pass")
    print("=" * 70)

    ezapp = lev2.lev2appinit()
    gfxenv = lev2.GfxEnv.ref
    ctx = gfxenv.loadingContext()
    fbi = ctx.FBI
    dwi = ctx.DWI
    txi = ctx.TXI
    ezapp.mainThreadBegin()

    real_img = lev2.Image.createFromFile(args.input)
    real_tex = lev2.Texture("real_src")
    txi.updateTexture(real_tex, real_img, False)
    real_tex.setAddressMode(tokens.WRAP, tokens.CLAMP, tokens.CLAMP)
    txi.applySamplingMode(real_tex)
    print(f"  Source: {real_img.width}x{real_img.height}")

    FW, FH = args.width, args.height
    TOTAL_SAMPLES = args.samples
    print(f"  Output: {FW}x{FH}, samples={TOTAL_SAMPLES}, per_pass={SAMPLES_PER_PASS}")

    # Build passes
    passes = []
    offset = 0
    while offset < TOTAL_SAMPLES:
        count = min(SAMPLES_PER_PASS, TOTAL_SAMPLES - offset)
        passes.append((offset, count))
        offset += count

    tek_name = "tek_filterSpecularMapEquirectangular_accum"
    errors = 0

    for rough in [0.0, 0.5, 1.0]:
        print(f"\n--- roughness={rough:.2f} ---")

        mtl = lev2.FreestyleMaterial()
        mtl.gpuInit(ctx, "orkshader://pbr_filterenv.fxv2")
        permu = lev2.FxPipelinePermutation()
        permu.rendermodel = "CUSTOM"
        permu.technique = mtl.shader.technique(tek_name)
        assert permu.technique, f"{tek_name} not found"
        pipeline = mtl.fxcache.findPipeline(permu)

        pipeline.bindParam(mtl.param("mvp"), mtx4())
        pipeline.bindParam(mtl.param("prefiltmap"), real_tex)
        pipeline.bindParam(mtl.param("roughness"), float(rough))
        pipeline.bindParam(mtl.param("imgdim"), vec2(FW, FH))
        pipeline.bindParam(mtl.param("totalsamples"), int(TOTAL_SAMPLES))
        vs_param = mtl.param("ViewportSize")
        if vs_param:
            pipeline.bindParam(vs_param, vec2(FW, FH))
        ivs_param = mtl.param("InvViewportSize")
        if ivs_param:
            pipeline.bindParam(ivs_param, vec2(1.0/FW, 1.0/FH))
        ivs_frg = mtl.param("InvViewportSizeFrg")
        if ivs_frg:
            pipeline.bindParam(ivs_frg, vec2(1.0/FW, 1.0/FH))

        num_tiles_x = (FW + TILE_SIZE - 1) // TILE_SIZE
        num_tiles_y = (FH + TILE_SIZE - 1) // TILE_SIZE
        tiles = [(tx, ty) for ty in range(num_tiles_y) for tx in range(num_tiles_x)]
        total_work = len(tiles) * len(passes)
        work_done = 0

        accum = [[0.0, 0.0, 0.0, 0.0] for _ in range(FW * FH)]
        t0 = time.time()

        for tx, ty in tiles:
            for sample_offset, sample_count in passes:
                pipeline.bindParam(mtl.param("numsamples"), int(sample_count))
                pipeline.bindParam(mtl.param("sampleoffset"), int(sample_offset))

                capbuf = render_single_tile(ctx, ezapp, fbi, dwi, pipeline, permu,
                                             FW, FH, tx, ty)
                accumulate_tile(accum, capbuf.image, tx, ty, FW, FH)

                work_done += 1
                pct = work_done * 100 // total_work
                print(f"\r  r={rough:.2f} {work_done}/{total_work} ({pct}%)   ", end="", flush=True)
                ezapp.mainThreadIter()

        elapsed = time.time() - t0
        print("")

        stats = check_accum(accum, FW, FH)
        black = stats['r'][2] < 0.001 and stats['g'][2] < 0.001 and stats['b'][2] < 0.001
        status = "FAIL BLACK" if black else "PASS"
        print(f"  [{status}] r={rough:.2f} ({elapsed:.1f}s)")
        for ch in ('r', 'g', 'b'):
            mn, mx, avg = stats[ch]
            print(f"    {ch.upper()}: min={mn:.4f} max={mx:.4f} avg={avg:.4f}")
        if black:
            errors += 1

    print("\n" + "=" * 70)
    print("ALL PASSED" if errors == 0 else f"FAILED: {errors} test(s)")
    print("=" * 70)

    ezapp.mainThreadEnd()
    return 1 if errors else 0

if __name__ == "__main__":
    sys.exit(main())
