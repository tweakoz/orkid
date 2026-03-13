#!/usr/bin/env python3
"""
ork.envmap - Synchronous environment map filtering and XIR generation.

Replaces the C++ async EnvMapProcessor with a pure-Python GPU pipeline
that avoids race conditions by doing everything synchronously on one thread.
"""

import os, time, struct
import numpy as np
from orkengine.core import vec2, vec4, mtx4
from orkengine import core, lev2

tokens = core.CrcStringProxy()

###############################################################################
# Half-float helpers
###############################################################################

def _half_to_float_array(arr_u16):
    sign = ((arr_u16 >> 15) & 1).astype(np.float32)
    exp = ((arr_u16 >> 10) & 0x1F).astype(np.int32)
    mant = (arr_u16 & 0x3FF).astype(np.float32)
    result = np.zeros_like(sign)
    normal = (exp > 0) & (exp < 31)
    result[normal] = ((-1.0)**sign[normal]) * (2.0**(exp[normal] - 15)) * (1.0 + mant[normal] / 1024.0)
    subnorm = (exp == 0) & (mant > 0)
    result[subnorm] = ((-1.0)**sign[subnorm]) * (2.0**-14) * (mant[subnorm] / 1024.0)
    return result

def _float_to_half_array(arr_f32):
    raw = arr_f32.astype(np.float32).view(np.uint32)
    sign = ((raw >> 16) & 0x8000).astype(np.uint16)
    exp32 = ((raw >> 23) & 0xFF).astype(np.int32) - 127 + 15
    mant = (raw & 0x007FFFFF).astype(np.uint16)
    result = sign.copy()
    valid = (exp32 > 0) & (exp32 < 31)
    result[valid] |= (exp32[valid].astype(np.uint16) << 10) | (mant[valid] >> 13)
    overflow = exp32 >= 31
    result[overflow] = sign[overflow] | 0x7C00
    return result

def _clamp_image(img, max_val, verbose=False):
    """Clamp HDR image pixel values to max_val in-place."""
    w, h, nc, bpc = img.width, img.height, img.numcomponents, img.bytesPerChannel
    mv = img.data.mutable_bytes
    def _report(floats, pre_max, max_val):
        pre_min = float(np.min(floats[floats > 0])) if np.any(floats > 0) else 1e-6
        old_dr = 20.0 * np.log10(pre_max / pre_min) if pre_min > 0 else 0.0
        new_dr = 20.0 * np.log10(max_val / pre_min) if pre_min > 0 else 0.0
        num_clamped = int(np.sum(floats > max_val))
        print(f"  Clamp: max {pre_max:.1f} -> {max_val:.1f} ({num_clamped} values clamped)")
        print(f"  Dynamic range: {old_dr:.1f} dB -> {new_dr:.1f} dB")

    if bpc == 2:  # RGBA16F
        arr = np.frombuffer(mv, dtype=np.uint16).copy()
        floats = _half_to_float_array(arr)
        pre_max = float(np.max(floats))
        if verbose:
            _report(floats, pre_max, max_val)
        clamped = np.minimum(floats, max_val)
        arr[:] = _float_to_half_array(clamped)
        np.frombuffer(mv, dtype=np.uint16)[:] = arr
    elif bpc == 4:  # RGBA32F
        arr = np.frombuffer(mv, dtype=np.float32)
        pre_max = float(np.max(arr))
        if verbose:
            _report(arr, pre_max, max_val)
        np.minimum(arr, max_val, out=arr)

###############################################################################
# Constants matching C++ EnvMapProcessor
###############################################################################

NUM_ROUGHNESS_LEVELS = 10
ROUGHNESS_POWER = 0.5
SPECULAR_SAMPLES = 8192
DIFFUSE_SAMPLES = 4096
SAMPLES_PER_PASS = 2048   # max samples per GPU submission to avoid watchdog
TILE_SIZE = 128
TILES_PER_FRAME = 4       # tiles per GPU frame
SLEEP_BETWEEN_FRAMES = 0.01  # 10ms breather between frames

###############################################################################

def _make_pipeline(mtl, technique_name):
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = mtl.shader.technique(technique_name)
    assert permu.technique, f"technique {technique_name} not found"
    pipeline = mtl.fxcache.findPipeline(permu)
    return pipeline, permu

def _render_single_tile(ctx, ezapp, fbi, dwi, pipeline, permu, mtl,
                        fw, fh, tx, ty):
    """Render one tile into a tile-sized RTG. Returns capbuf with tile pixels."""
    tile_x = tx * TILE_SIZE
    tile_y = ty * TILE_SIZE
    tile_w = min(TILE_SIZE, fw - tile_x)
    tile_h = min(TILE_SIZE, fh - tile_y)

    # Set tile offset so shader computes correct full-image coordinates
    pipeline.bindParam(mtl.param("tileoffset"), vec2(tile_x, tile_y))

    rtg = lev2.RtGroup(ctx, tile_w, tile_h)
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

    def draw():
        # Full-screen quad fills the tile-sized RTG
        dwi.quad2D(vec4(-1, -1, 2, 2), vec4(0, 0, 1, 1))

    pipeline.wrappedDrawCall(RCID, draw)
    future = fbi.captureAsFormat(rtb, capbuf, "RGBA32F")
    fbi.rtGroupPop()
    ctx.endFrame()

    start = time.time()
    while not future.is_ready:
        ezapp.mainThreadIter()
        if time.time() - start > 120:
            raise TimeoutError("GPU capture timeout")

    return capbuf, tile_w, tile_h

def _accumulate_tile(accum_data, capbuf_img, tx, ty, tile_w, tile_h, fw):
    """Add RGBA32F tile data into accumulator using struct for speed."""
    import struct as st
    tile_x = tx * TILE_SIZE
    tile_y = ty * TILE_SIZE
    src = bytes(capbuf_img.data.bytes)
    for row in range(tile_h):
        py = tile_y + row
        src_row_off = row * tile_w * 16
        for col in range(tile_w):
            px = tile_x + col
            src_off = src_row_off + col * 16
            r, g, b, w = st.unpack_from('<ffff', src, src_off)
            idx = (py * fw + px) * 4
            accum_data[idx] += r
            accum_data[idx+1] += g
            accum_data[idx+2] += b
            accum_data[idx+3] += w

def _finalize_accum(accum_data, fw, fh, cap_fmt_str):
    """Normalize accumulated weighted sums and produce final Image."""
    import struct as st
    import array
    img = lev2.Image()
    if cap_fmt_str == "RGBA16F":
        img.initWithFormat(fw, fh, tokens.RGBA16F)
        mv = img.data.mutable_bytes
        for idx in range(fw * fh):
            ai = idx * 4
            r, g, b, w = accum_data[ai], accum_data[ai+1], accum_data[ai+2], accum_data[ai+3]
            if w > 0:
                r /= w; g /= w; b /= w
            def f2h(f):
                raw = st.pack('<f', f)
                bits = st.unpack('<I', raw)[0]
                sign = (bits >> 16) & 0x8000
                exp32 = ((bits >> 23) & 0xFF) - 127 + 15
                mant = bits & 0x007FFFFF
                if exp32 <= 0: return sign
                if exp32 >= 31: return sign | 0x7C00
                return sign | (exp32 << 10) | (mant >> 13)
            off = idx * 8
            st.pack_into('<HHHH', mv, off, f2h(r), f2h(g), f2h(b), f2h(1.0))
    else:
        img.initWithFormat(fw, fh, tokens.RGBA8)
        mv = img.data.mutable_bytes
        for idx in range(fw * fh):
            ai = idx * 4
            r, g, b, w = accum_data[ai], accum_data[ai+1], accum_data[ai+2], accum_data[ai+3]
            if w > 0:
                r /= w; g /= w; b /= w
            off = idx * 4
            st.pack_into('BBBB', mv, off,
                         min(255, int(r * 255)),
                         min(255, int(g * 255)),
                         min(255, int(b * 255)), 255)
    return img

def _filter_pass(ctx, ezapp, mtl, technique_name, src_tex,
                 roughness, fw, fh, numsamples, cap_fmt_str,
                 progress_prefix=""):
    """Render tiles one at a time with accumulation passes to avoid GPU watchdog."""
    fbi = ctx.FBI
    dwi = ctx.DWI

    # Use accumulation technique
    accum_tek = technique_name + "_accum"
    pipeline, permu = _make_pipeline(mtl, accum_tek)

    num_tiles_x = (fw + TILE_SIZE - 1) // TILE_SIZE
    num_tiles_y = (fh + TILE_SIZE - 1) // TILE_SIZE
    total_tiles = num_tiles_x * num_tiles_y

    tiles = [(tx, ty) for ty in range(num_tiles_y) for tx in range(num_tiles_x)]

    # Build list of sample passes
    passes = []
    offset = 0
    while offset < numsamples:
        count = min(SAMPLES_PER_PASS, numsamples - offset)
        passes.append((offset, count))
        offset += count
    num_passes = len(passes)

    # Accumulator: flat array [r, g, b, weight] per pixel
    import array
    accum_data = array.array('f', [0.0] * (fw * fh * 4))

    total_work = total_tiles * num_passes
    work_done = 0

    # Bind constant params
    pipeline.bindParam(mtl.param("mvp"), mtx4())
    pipeline.bindParam(mtl.param("prefiltmap"), src_tex)
    pipeline.bindParam(mtl.param("roughness"), float(roughness))
    pipeline.bindParam(mtl.param("imgdim"), vec2(fw, fh))
    pipeline.bindParam(mtl.param("totalsamples"), int(numsamples))
    vs_param = mtl.param("ViewportSize")
    if vs_param:
        pipeline.bindParam(vs_param, vec2(fw, fh))
    ivs_param = mtl.param("InvViewportSize")
    if ivs_param:
        pipeline.bindParam(ivs_param, vec2(1.0 / fw, 1.0 / fh))
    ivs_frg = mtl.param("InvViewportSizeFrg")
    if ivs_frg:
        pipeline.bindParam(ivs_frg, vec2(1.0 / fw, 1.0 / fh))

    for tx, ty in tiles:
        for sample_offset, sample_count in passes:
            # Update per-pass params
            pipeline.bindParam(mtl.param("numsamples"), int(sample_count))
            pipeline.bindParam(mtl.param("sampleoffset"), int(sample_offset))

            capbuf, tile_w, tile_h = _render_single_tile(
                ctx, ezapp, fbi, dwi, pipeline, permu, mtl, fw, fh, tx, ty)
            _accumulate_tile(accum_data, capbuf.image, tx, ty, tile_w, tile_h, fw)

            work_done += 1
            if progress_prefix:
                pct = work_done * 100 // total_work
                print(f"\r    {progress_prefix} {work_done}/{total_work} ({pct}%)   ", end="", flush=True)

            time.sleep(SLEEP_BETWEEN_FRAMES)
            ezapp.mainThreadIter()

    if progress_prefix:
        print("", flush=True)

    return _finalize_accum(accum_data, fw, fh, cap_fmt_str)

###############################################################################

def process_envmap(source_path, output_path, ctx, ezapp,
                   debug_dir=None, verbose=True,
                   num_roughness_levels=NUM_ROUGHNESS_LEVELS,
                   roughness_values=None,
                   skip_diffuse=False,
                   scale=1.0,
                   clamp=16.0,
                   specular_samples=None,
                   diffuse_samples=None):
    """
    Synchronously filter an environment map and write XIR.

    Args:
        source_path: Path to source image (.exr, .hdr, .png, .dds)
        output_path: Path for output .xir file
        ctx: Graphics context
        ezapp: EzApp instance
        debug_dir: Optional directory for debug image output
        verbose: Print progress
    Returns:
        True on success
    """
    source_path = str(source_path)
    output_path = str(output_path)
    total_start = time.time()

    spec_samples = specular_samples if specular_samples is not None else SPECULAR_SAMPLES
    diff_samples = diffuse_samples if diffuse_samples is not None else DIFFUSE_SAMPLES

    ext = os.path.splitext(source_path)[1].lower()
    is_hdr = ext in (".exr", ".hdr")
    is_equirectangular = is_hdr
    cap_fmt_str = "RGBA16F" if is_hdr else "RGBA8"

    # Load source image as texture
    txi = ctx.TXI
    img = lev2.Image.createFromFile(source_path)
    if not img or img.width == 0:
        print(f"ERROR: Could not load {source_path}")
        return False

    if scale != 1.0:
        new_w = max(1, int(img.width * scale))
        new_h = max(1, int(img.height * scale))
        if verbose:
            print(f"  Scaling {img.width}x{img.height} -> {new_w}x{new_h} (scale={scale})")
        img = img.resized(new_w, new_h)

    if clamp > 0 and is_hdr:
        _clamp_image(img, clamp, verbose)

    tex_w, tex_h = img.width, img.height
    if verbose:
        print(f"  Source: {tex_w}x{tex_h} nc={img.numcomponents} bpc={img.bytesPerChannel}")
        num_spec_tiles = ((tex_w + TILE_SIZE - 1) // TILE_SIZE) * ((tex_h + TILE_SIZE - 1) // TILE_SIZE)
        print(f"  Tiles per pass: {num_spec_tiles} ({TILE_SIZE}px)")

    tex = lev2.Texture("envmap_src")
    txi.updateTexture(tex, img, False)

    if is_equirectangular:
        tex.setAddressMode(tokens.WRAP, tokens.CLAMP, tokens.CLAMP)
        txi.applySamplingMode(tex)

    # Setup filter materials
    filter_shader = "orkshader://pbr_filterenv.fxv2"
    spec_mtl = lev2.FreestyleMaterial()
    spec_mtl.gpuInit(ctx, filter_shader)
    diff_mtl = lev2.FreestyleMaterial()
    diff_mtl.gpuInit(ctx, filter_shader)

    spec_tek = "tek_filterSpecularMapEquirectangular" if is_equirectangular else "tek_filterSpecularMapStandard"
    diff_tek = "tek_filterDiffuseMapEquirectangular" if is_equirectangular else "tek_filterDiffuseMapStandard"

    # ── Specular filtering ──────────────────────────────────────────────
    specular_images = []
    if roughness_values is None:
        roughness_values = []
        denom = max(1, num_roughness_levels - 1)
        for i in range(num_roughness_levels):
            roughness_values.append((i / denom) ** ROUGHNESS_POWER)

    num_levels = len(roughness_values)
    for i, roughness in enumerate(roughness_values):
        if verbose:
            print(f"  Specular [{i+1}/{num_levels}] roughness={roughness:.4f}")

        step_start = time.time()
        cap_img = _filter_pass(
            ctx, ezapp, spec_mtl, spec_tek, tex,
            roughness, tex_w, tex_h, spec_samples, cap_fmt_str,
            progress_prefix=f"spec[{i+1}/{num_levels}]")

        specular_images.append(cap_img)
        step_elapsed = time.time() - step_start
        if verbose:
            total_elapsed = time.time() - total_start
            print(f"    {cap_img.width}x{cap_img.height} ({step_elapsed:.1f}s, total {total_elapsed:.0f}s)")

        if debug_dir:
            debug_ext = ".exr" if is_hdr else ".png"
            cap_img.writeToFile(os.path.join(debug_dir, f"specular_{i}_r{roughness:.3f}{debug_ext}"))

        # Breathe between passes
        time.sleep(SLEEP_BETWEEN_FRAMES)
        ezapp.mainThreadIter()

    # ── Diffuse filtering (mip chain) ──────────────────────────────────
    diffuse_images = []
    if not skip_diffuse:
        dw, dh = tex_w, tex_h
        mip = 0

        # Count total diffuse mips for progress
        tw, th = tex_w, tex_h
        total_diff_mips = 0
        while tw >= 4 and th >= 4:
            total_diff_mips += 1
            tw >>= 1
            th >>= 1

        while dw >= 4 and dh >= 4:
            if verbose:
                print(f"  Diffuse mip [{mip+1}/{total_diff_mips}] {dw}x{dh}")

            step_start = time.time()
            cap_img = _filter_pass(
                ctx, ezapp, diff_mtl, diff_tek, tex,
                1.0, dw, dh, diff_samples, cap_fmt_str,
                progress_prefix=f"diff[{mip+1}/{total_diff_mips}]")

            diffuse_images.append(cap_img)
            step_elapsed = time.time() - step_start
            if verbose:
                total_elapsed = time.time() - total_start
                print(f"    done ({step_elapsed:.1f}s, total {total_elapsed:.0f}s)")

            if debug_dir:
                debug_ext = ".exr" if is_hdr else ".png"
                cap_img.writeToFile(os.path.join(debug_dir, f"diffuse_mip_{mip}{debug_ext}"))

            # Breathe between passes
            time.sleep(SLEEP_BETWEEN_FRAMES)
            ezapp.mainThreadIter()

            dw >>= 1
            dh >>= 1
            mip += 1
    elif verbose:
        print("  Skipping diffuse filtering")

    # ── Write XIR ───────────────────────────────────────────────────────
    if verbose:
        print(f"  Writing XIR: {len(specular_images)} specular, {len(diffuse_images)} diffuse mips")

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    ok = lev2.EnvMapProcessor.writeXIR(
        specular_images, roughness_values, diffuse_images, output_path)

    total_elapsed = time.time() - total_start
    if verbose:
        if ok:
            print(f"  SUCCESS: {output_path} ({total_elapsed:.1f}s)")
        else:
            print(f"  ERROR: failed to write {output_path}")

    return ok
