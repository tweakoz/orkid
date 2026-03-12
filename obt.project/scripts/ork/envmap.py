#!/usr/bin/env python3
"""
ork.envmap - Synchronous environment map filtering and XIR generation.

Replaces the C++ async EnvMapProcessor with a pure-Python GPU pipeline
that avoids race conditions by doing everything synchronously on one thread.
"""

import os, time
from orkengine.core import vec2, vec4, mtx4
from orkengine import core, lev2

tokens = core.CrcStringProxy()

###############################################################################
# Constants matching C++ EnvMapProcessor
###############################################################################

NUM_ROUGHNESS_LEVELS = 10
ROUGHNESS_POWER = 0.5
SPECULAR_SAMPLES = 8192
DIFFUSE_SAMPLES = 4096
TILE_SIZE = 128
TILES_PER_FRAME = 4      # limit GPU work per frame to stay responsive
SLEEP_BETWEEN_FRAMES = 0.01  # 10ms breather between frames

###############################################################################

def _make_pipeline(mtl, technique_name):
    permu = lev2.FxPipelinePermutation()
    permu.rendermodel = "CUSTOM"
    permu.technique = mtl.shader.technique(technique_name)
    assert permu.technique, f"technique {technique_name} not found"
    pipeline = mtl.fxcache.findPipeline(permu)
    return pipeline, permu

def _filter_pass(ctx, ezapp, mtl, technique_name, src_tex,
                 roughness, fw, fh, numsamples, cap_fmt_str,
                 progress_prefix=""):
    """Render a filter pass with tiled rendering spread across multiple frames."""
    fbi = ctx.FBI
    dwi = ctx.DWI
    pipeline, permu = _make_pipeline(mtl, technique_name)

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
        pipeline.bindParam(ivs_param, vec2(1.0 / fw, 1.0 / fh))
    ivs_frg = mtl.param("InvViewportSizeFrg")
    if ivs_frg:
        pipeline.bindParam(ivs_frg, vec2(1.0 / fw, 1.0 / fh))

    rtg = lev2.RtGroup(ctx, fw, fh)
    rtb = rtg.createBuffer(tokens.RGBA32F, tokens.color)
    rtb.clearColor = vec4(0, 0, 0, 1)

    num_tiles_x = (fw + TILE_SIZE - 1) // TILE_SIZE
    num_tiles_y = (fh + TILE_SIZE - 1) // TILE_SIZE
    total_tiles = num_tiles_x * num_tiles_y

    # Build flat tile list
    tiles = []
    for ty in range(num_tiles_y):
        for tx in range(num_tiles_x):
            tiles.append((tx, ty))

    # Clear the RTG in a first frame
    ctx.beginFrame()
    fbi.rtGroupPush(rtg)
    fbi.rtGroupClear(rtg)
    fbi.rtGroupPop()
    ctx.endFrame()

    # Render tiles in batches across multiple frames
    tiles_done = 0
    while tiles_done < total_tiles:
        batch_end = min(tiles_done + TILES_PER_FRAME, total_tiles)
        batch = tiles[tiles_done:batch_end]

        ctx.beginFrame()
        fbi.rtGroupPush(rtg)
        RCFD = lev2.RenderContextFrameData(ctx)
        RCID = lev2.RenderContextInstData(RCFD)
        RCID.forceTechnique(permu.technique)
        RCID.genMatrix(lambda: mtx4())

        def draw_batch(batch=batch):
            for tx, ty in batch:
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

        pipeline.wrappedDrawCall(RCID, draw_batch)
        fbi.rtGroupPop()
        ctx.endFrame()

        tiles_done = batch_end

        if progress_prefix:
            pct = tiles_done * 100 // total_tiles
            print(f"\r    {progress_prefix} tiles {tiles_done}/{total_tiles} ({pct}%)   ", end="", flush=True)

        # Let the system breathe
        time.sleep(SLEEP_BETWEEN_FRAMES)
        ezapp.mainThreadIter()

    if progress_prefix:
        print("")  # final newline

    # Capture in a separate frame
    capbuf = lev2.CaptureBuffer()
    ctx.beginFrame()
    future = fbi.captureAsFormat(rtb, capbuf, cap_fmt_str)
    ctx.endFrame()

    start = time.time()
    while not future.is_ready:
        ezapp.mainThreadIter()
        if time.time() - start > 120:
            raise TimeoutError("GPU capture timeout")

    return capbuf

###############################################################################

def process_envmap(source_path, output_path, ctx, ezapp,
                   debug_dir=None, verbose=True):
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

    tex_w, tex_h = img.width, img.height
    if verbose:
        print(f"  Source: {tex_w}x{tex_h} nc={img.numcomponents} bpc={img.bytesPerChannel}")
        num_spec_tiles = ((tex_w + TILE_SIZE - 1) // TILE_SIZE) * ((tex_h + TILE_SIZE - 1) // TILE_SIZE)
        print(f"  Tiles per pass: {num_spec_tiles} ({TILE_SIZE}px, {TILES_PER_FRAME}/frame)")

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
    roughness_values = []

    for i in range(NUM_ROUGHNESS_LEVELS):
        roughness = (i / 9.0) ** ROUGHNESS_POWER
        roughness_values.append(roughness)
        if verbose:
            print(f"  Specular [{i+1}/{NUM_ROUGHNESS_LEVELS}] roughness={roughness:.4f}")

        step_start = time.time()
        capbuf = _filter_pass(
            ctx, ezapp, spec_mtl, spec_tek, tex,
            roughness, tex_w, tex_h, SPECULAR_SAMPLES, cap_fmt_str,
            progress_prefix=f"spec[{i+1}/{NUM_ROUGHNESS_LEVELS}]")

        cap_img = capbuf.image
        specular_images.append(cap_img)
        step_elapsed = time.time() - step_start
        if verbose:
            total_elapsed = time.time() - total_start
            print(f"    {cap_img.width}x{cap_img.height} ({step_elapsed:.1f}s, total {total_elapsed:.0f}s)")

        if debug_dir:
            debug_ext = ".exr" if is_hdr else ".png"
            cap_img.writeToFile(os.path.join(debug_dir, f"specular_{i}_r{roughness:.3f}{debug_ext}"))

    # ── Diffuse filtering (mip chain) ──────────────────────────────────
    diffuse_images = []
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
        capbuf = _filter_pass(
            ctx, ezapp, diff_mtl, diff_tek, tex,
            1.0, dw, dh, DIFFUSE_SAMPLES, cap_fmt_str,
            progress_prefix=f"diff[{mip+1}/{total_diff_mips}]")

        cap_img = capbuf.image
        diffuse_images.append(cap_img)
        step_elapsed = time.time() - step_start
        if verbose:
            total_elapsed = time.time() - total_start
            print(f"    done ({step_elapsed:.1f}s, total {total_elapsed:.0f}s)")

        if debug_dir:
            debug_ext = ".exr" if is_hdr else ".png"
            cap_img.writeToFile(os.path.join(debug_dir, f"diffuse_mip_{mip}{debug_ext}"))

        dw >>= 1
        dh >>= 1
        mip += 1

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
