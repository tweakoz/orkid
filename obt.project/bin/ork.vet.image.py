#!/bin/sh
""":"
# trampoline: exec the first python3 that has numpy+PIL (image vet needs both).
for _py in \
    "$OBT_STAGE/pyvenv/bin/python3" \
    "$HOME"/.staging*/pyvenv/bin/python3 \
    "$(command -v python3)"; do
    [ -x "$_py" ] && "$_py" -c 'import numpy, PIL' 2>/dev/null && exec "$_py" "$0" "$@"
done
echo "ork.vet.image: no python3 with numpy+PIL found on this host" >&2
exit 3
":"""
"""
Image/heightfield vet: mechanical quality verdicts for agent consumption.

Porcelain, one check per line, footer that the consuming agent QUOTES (never
re-judges). House rules encoded here:
  - a degenerate frame (all-black / near-constant) FAILs LOUDLY (a black
    settle-race snapshot must never pass as 'unchanged')
  - speckle is a SPECTRAL question (FFT high-band energy), not min/max/avg
  - heightfield plausibility is a WALK question (reversal/jolt/median slope)
  - stats are necessary-not-sufficient -> INFO only
  - golden comparison (SSIM + worst-region crop) is the workhorse when a
    blessed reference exists; --bless promotes the candidate to golden
  - foliage whitening ("cotton") is an ABSOLUTE population question, never a
    contrast ratio (the ratio self-normalizes and survives the fix)
  - an impostor atlas is vetted on its TRANSPARENT texels and on its mip chain,
    not on how it looks flattened
"""
import sys
import os
import argparse
from pathlib import Path

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _ork_vet_common as vet


def _render_checks(args, r):
    """Render-only discriminating checks (GAP-1 speckle/glint, GAP-2 chroma).

    Runs on RGB (chroma-aware). Returns the magenta worst-tile (y,x,idx) when
    the cast check FAILs, else None (for the footer/crop). See _ork_vet_common
    for the discriminating feature behind each check.
    """
    rgb = vet.load_rgb(args.candidate)
    L = vet.luma(rgb)

    # GAP-1a: isolated bright impulses (hot pixel / firefly), NOT sun glints
    ff_frac, ff_cnt, ff_max = vet.firefly_frac(L)
    r.info('spike.firefly_max', f"{ff_max:.4f}")
    r.gate('spike.firefly_frac', f"{ff_frac:.6f}", f"<{args.max_firefly:g}",
           ff_frac < args.max_firefly)

    # GAP-1b: speckle vs material detail (edge-preserving residual)
    energy, anticorr = vet.speckle_residual(L)
    r.gate('speckle.residual_energy', f"{energy:.4f}", f"<{args.max_residual:g}",
           energy < args.max_residual)
    if energy > args.grain_floor:
        r.gate('speckle.grain_anticorr', f"{anticorr:+.3f}", f">{args.min_graincorr:g}",
               anticorr > args.min_graincorr)
    else:
        r.info('speckle.grain_anticorr', f"{anticorr:+.3f} (energy<{args.grain_floor:g})")

    # GAP-2: coherent off-locus magenta cast (immune to warm golden-hour tint)
    area, tint, (wy, wx), wval = vet.magenta_cast(rgb, args.strong_magenta)
    r.info('chroma.tint_mean', f"{tint:+.4f}")
    ok = area < args.max_magenta_area
    r.gate('chroma.magenta_area', f"{area:.5f}", f"<{args.max_magenta_area:g}", ok)
    return None if ok else (wy, wx, wval)


def _crop_rgb(path, cy, cx, out, size=96, zoom=3):
    """Write a size-px RGB crop centred on (cy, cx), nearest-zoomed for the eye."""
    from PIL import Image
    rgb = vet.load_rgb(path)
    y0 = max(0, min(rgb.shape[0] - size, cy - size // 2))
    x0 = max(0, min(rgb.shape[1] - size, cx - size // 2))
    c = np.clip(rgb[y0:y0 + size, x0:x0 + size], 0, 1) * 255
    Image.fromarray(c.astype(np.uint8)).resize(
        (c.shape[1] * zoom, c.shape[0] * zoom), Image.NEAREST).save(out)


def _cotton_checks(args, r):
    """Foliage whitening ('cotton') checks on a rendered still.

    Promoted from the ad-hoc canopy probe. The probe's headline contrast RATIO
    is emitted here as INFO and is deliberately NOT gated: it self-normalizes
    (bright mask vs local darks) and read ~2.5 both before and after the defect
    collapsed. The gates are absolute: a fixed-mask population, a blob-area
    fraction and a blob DENSITY (per megapixel of probed area, so the verdict
    does not move with region size or capture resolution).

    Returns the worst (largest) cotton blob (y, x, px) or None.
    """
    rgb = vet.load_rgb(args.candidate)
    if args.region:
        x0, y0, x1, y1 = [int(v) for v in args.region.split(',')]
        rgb = rgb[y0:y1, x0:x1]
        r.info('cotton.region', f"{x0},{y0},{x1},{y1} ({x1 - x0}x{y1 - y0})")
    else:
        r.info('cotton.region', f"full frame ({rgb.shape[1]}x{rgb.shape[0]})")
    m = vet.cotton_metrics(rgb, args.cotton_lum_floor, args.cotton_max_sat,
                           args.cotton_ctx_max, args.min_blob_px,
                           max_blob_frac=args.max_blob_frac,
                           field_texture=args.field_texture)

    r.info('cotton.lum_p95', f"{m['lum_p95']:.4f}")
    r.info('cotton.candidate_frac', f"{m['cand_frac']:.5f}")
    bad = vet.FAIL in (
        r.gate('cotton.fixedmask_frac', f"{m['fixedmask_frac']:.5f}",
               f"<{args.max_cotton_fixedmask:g}",
               m['fixedmask_frac'] < args.max_cotton_fixedmask),
        r.gate('cotton.area_frac', f"{m['area_frac']:.5f}", f"<{args.max_cotton_area:g}",
               m['area_frac'] < args.max_cotton_area),
        r.gate('cotton.blob_density', f"{m['blob_density']:.1f}/Mpx",
               f"<{args.max_cotton_density:g}", m['blob_density'] < args.max_cotton_density))
    r.info('cotton.blob_count', f"{m['blob_count']}")
    r.info('cotton.median_blob_px', f"{m['median_blob_px']}")
    r.info('cotton.fields', f"smooth={m['field_smooth']}/{m['field_smooth_frac']:.5f} "
                            f"textured={m['field_textured']}/{m['field_textured_frac']:.5f} "
                            '(smooth = sky through the canopy, dropped)')
    b = m['buckets']
    r.info('cotton.blob_classes',
           f"uniform={b['UNIFORM']} thin={b['THIN']} core_bright={b['CORE_BRIGHT']} "
           f"edge_fringe={b['EDGE_FRINGE']}")
    r.info('cotton.signature', m['signature']
           + (f" (rim-vs-core falloff {m['median_falloff']:+.3f})" if 'median_falloff' in m else ''))
    r.info('cotton.blob_lum_sat', f"{m['blob_lum']:.4f}/{m['blob_sat']:.4f}")
    r.info('cotton.context_lum', f"{m['ctx_lum']:.4f}")
    # SELF-NORMALIZING: survived the fix in the episode this was promoted from.
    r.info('cotton.lum_ratio', f"{m['lum_ratio']:.3f} (self-normalizing; never gated)")
    if not m['converged']:
        r.gate('cotton.labeling', 'did not converge', 'converged', False, warn_only=True)
    return m['worst'] if bad else None       # locate evidence only for a FAIL


def _atlas_checks(args, r):
    """Impostor-atlas integrity: transparent-texel white bleed + mip drift.

    Two independent defects, two independent gates. (1) Background bleed: the
    RGB under fully transparent alpha is what bilinear/mip filtering drags into
    the silhouette, so it must be dark -- white background texels become a halo
    and, at LOD range, whitened canopy. (2) Mip drift: the divergence between a
    naive box mip and an alpha-WEIGHTED box mip, i.e. how much value error the
    sampler accumulates per level. Both are absolute measures on the asset; no
    render and no golden needed.

    Returns the worst background tile (y, x, lum) or None.
    """
    rgb, alpha = vet.load_rgba(args.candidate)
    if args.alpha_from:
        _, alpha = vet.load_rgba(args.alpha_from)
        r.info('atlas.alpha_source', args.alpha_from)
        if alpha.shape != rgb.shape[:2]:
            r.gate('atlas.alpha_shape', f"{alpha.shape} vs {rgb.shape[:2]}", 'equal', False)
            return None
    f = vet.atlas_fringe(rgb, alpha)
    r.info('atlas.size', f"{rgb.shape[1]}x{rgb.shape[0]}")
    r.info('atlas.alpha_coverage', f"{f['coverage']:.4f}")
    r.info('atlas.region_fracs',
           f"bg={f['bg_frac']:.4f} edge={f['edge_frac']:.4f} fg={f['fg_frac']:.4f}")
    r.info('atlas.fg_luminance', f"{f['fg_lum']:.4f}")
    r.info('atlas.edge_luminance', f"{f['edge_lum']:.4f}")
    worst = None
    if not f['has_bg']:
        r.info('atlas.bg_excess_lum', 'n/a (no fully transparent texels)')
    else:
        r.info('atlas.bg_luminance', f"{f['bg_lum']:.4f}")
        bled = vet.FAIL in (
            r.gate('atlas.bg_excess_lum', f"{f['bg_excess']:+.4f}", f"<{args.max_bg_excess:g}",
                   f['bg_excess'] < args.max_bg_excess),
            r.gate('atlas.bg_bright_frac', f"{f['bg_bright_frac']:.5f}",
                   f"<{args.max_bg_bright:g}", f['bg_bright_frac'] < args.max_bg_bright))
        r.info('atlas.fg_bright_end', f"{f['fg_bright_end']:.4f} (bright = above this +0.10)")
        worst = f['worst'] if bled else None   # locate evidence only for a FAIL

    drift, rows = vet.mip_drift(rgb, alpha, args.mip_levels)
    for lv, d, nl, wl in rows:
        r.info(f'atlas.mip{lv}_drift', f"{d:.4f} (naive {nl:.4f} vs alpha-weighted {wl:.4f})")
    r.gate('atlas.mip_drift', f"{drift:.4f}", f"<{args.max_mip_drift:g}",
           drift < args.max_mip_drift)
    return worst


def main():
    p = argparse.ArgumentParser(description='Vet an image/heightfield; porcelain verdicts')
    p.add_argument('candidate')
    p.add_argument('--golden', help='blessed reference to compare against')
    p.add_argument('--kind', choices=['heightmap', 'render', 'foliage', 'impostor-atlas'],
                   default='heightmap',
                   help="foliage = render checks + canopy whitening ('cotton'); "
                        'impostor-atlas = transparent-texel bleed + mip drift')
    p.add_argument('--bless', action='store_true',
                   help='promote candidate to golden (after human OK)')
    p.add_argument('--crop-out', help='write worst-region golden|candidate crop PNG here')
    p.add_argument('--max-highband', type=float, default=0.02,
                   help='FFT high-band energy fraction ceiling (speckle; heightmap gate)')
    p.add_argument('--max-spike', type=float, default=0.05,
                   help='isolated-spike ceiling (heightmap gate)')
    p.add_argument('--min-range', type=float, default=0.01,
                   help='degenerate-frame floor: max-min dynamic range')
    p.add_argument('--min-stddev', type=float, default=0.005,
                   help='degenerate-frame floor: stddev')
    # render-only discriminating checks (no-op for --kind heightmap)
    p.add_argument('--max-firefly', type=float, default=0.0015,
                   help='render: max isolated-impulse (hot-pixel/firefly) density')
    p.add_argument('--max-residual', type=float, default=0.042,
                   help='render: max median-residual energy (gross speckle)')
    p.add_argument('--min-graincorr', type=float, default=0.02,
                   help='render: min residual autocorrelation (below=noise-like grain)')
    p.add_argument('--grain-floor', type=float, default=0.008,
                   help='render: residual energy below this = no grain verdict (INFO)')
    p.add_argument('--max-magenta-area', type=float, default=0.02,
                   help='render: max coherent strong-magenta area fraction (color cast)')
    p.add_argument('--strong-magenta', type=float, default=0.15,
                   help='render: per-pixel magenta_index that counts as a strong cast')
    # foliage ("cotton") -- see _cotton_checks; defaults derived from the canopy
    # whitening episode (defect vs post-fix captures), documented in the taskset.
    p.add_argument('--region', help='foliage: probe region x0,y0,x1,y1 (default: full frame). '
                                    'Cotton is a LOCAL defect -- aim this at a canopy band; '
                                    'a full frame dilutes it')
    p.add_argument('--cotton-lum-floor', type=float, default=0.25,
                   help='foliage: absolute luminance floor for a whitening candidate')
    p.add_argument('--cotton-max-sat', type=float, default=0.25,
                   help='foliage: max saturation for a whitening candidate (cotton is white)')
    p.add_argument('--cotton-ctx-max', type=float, default=0.25,
                   help='foliage: max local context luminance (rejects sky/bright fields)')
    p.add_argument('--min-blob-px', type=int, default=12,
                   help='foliage: smallest blob that counts (below = render speck)')
    p.add_argument('--max-blob-frac', type=float, default=0.005,
                   help='foliage: largest blob that counts, as a fraction of the probed '
                        'area (above = a bright FIELD, e.g. sky through a canopy gap)')
    p.add_argument('--field-texture', type=float, default=0.009,
                   help='foliage: median-residual texture that separates a whitened canopy '
                        'field (keeps leaf structure) from a smooth sky field (dropped)')
    p.add_argument('--max-cotton-fixedmask', type=float, default=0.030,
                   help='foliage: max fixed-mask bright-desaturated-in-dark-context fraction')
    p.add_argument('--max-cotton-area', type=float, default=0.008,
                   help='foliage: max area fraction in cotton-signature blobs')
    p.add_argument('--max-cotton-density', type=float, default=200.0,
                   help='foliage: max cotton blobs per megapixel of probed area')
    # impostor atlas -- see _atlas_checks
    p.add_argument('--alpha-from',
                   help='impostor-atlas: take the alpha mask from this companion atlas '
                        '(normal/mr maps that ship opaque alpha)')
    p.add_argument('--max-bg-excess', type=float, default=0.05,
                   help='impostor-atlas: max (transparent-texel luminance - silhouette '
                        'luminance); the background must not be brighter than what it borders')
    p.add_argument('--max-bg-bright', type=float, default=0.010,
                   help='impostor-atlas: max fraction of transparent texels brighter than '
                        "the sprite's own bright end")
    p.add_argument('--max-mip-drift', type=float, default=0.030,
                   help='impostor-atlas: max naive-vs-alpha-weighted mip luminance divergence')
    p.add_argument('--mip-levels', type=int, default=4,
                   help='impostor-atlas: mip levels to walk for the drift measurement')
    args = p.parse_args()

    a = vet.load_gray(args.candidate)
    r = vet.Report()

    # stats: necessary NOT sufficient -> INFO only
    r.info('stats.min_max_mean', f"{a.min():.4f}/{a.max():.4f}/{a.mean():.4f}")
    clip = float(((a <= 0) | (a >= 1)).mean())
    if args.kind == 'impostor-atlas':
        # a sprite atlas is mostly cleared background by construction -> the
        # clipped fraction is geometry, not a defect. The atlas verdict lives in
        # the atlas.* checks.
        r.info('stats.clipped_frac', f"{clip:.4f} (atlas background; not gated)")
    else:
        r.gate('stats.clipped_frac', f"{clip:.4f}", '<0.05', clip < 0.05)

    # degenerate frame: all-black / near-constant -> FAIL loudly
    rng, std, degen = vet.degenerate_frame(a, args.min_range, args.min_stddev)
    r.info('frame.stddev', f"{std:.5f}")
    r.gate('frame.dynamic_range', f"{rng:.4f}", f">{args.min_range:g}", not degen)

    hf = vet.fft_highband_ratio(a)
    if args.kind == 'heightmap':
        r.gate('spectral.highband_ratio', f"{hf:.4f}", f"<{args.max_highband:g}", hf < args.max_highband)
    else:
        # renders legitimately carry broadband material detail; the FFT highband
        # alone cannot separate detail from speckle -> INFO here. The render
        # speckle verdict is speckle.residual_energy / speckle.grain_anticorr.
        r.info('spectral.highband_ratio', f"{hf:.4f}")

    if args.kind == 'heightmap':
        slope, jolt, rev = vet.walk_metrics(a)
        r.gate('walk.median_slope', f"{slope:.5f}", '<0.02', slope < 0.02)
        r.gate('walk.jolt_p99', f"{jolt:.5f}", '<0.01', jolt < 0.01)
        r.gate('walk.reversal_rate', f"{rev:.3f}", '<0.60', rev < 0.60)

    sp = vet.spike_max(a)
    if args.kind == 'heightmap':
        r.gate('spike.max_isolated', f"{sp:.4f}", f"<{args.max_spike:g}", sp < args.max_spike)
    else:
        # a single bright content pixel (sun glint, foliage speck, specular edge)
        # maxes this out -> INFO for renders. The isolated-impulse verdict is
        # spike.firefly_frac (a density, robust to one legitimate glint).
        r.info('spike.max_isolated', f"{sp:.4f}")

    chroma_worst = None
    cotton_worst = None
    atlas_worst = None
    if args.kind in ('render', 'foliage'):
        chroma_worst = _render_checks(args, r)
    if args.kind == 'foliage':
        cotton_worst = _cotton_checks(args, r)
    if args.kind == 'impostor-atlas':
        atlas_worst = _atlas_checks(args, r)

    worst = None
    if args.golden:
        g = vet.load_gray(args.golden)
        if g.shape != a.shape:
            r.gate('golden.shape', f"{a.shape} vs {g.shape}", 'equal', False)
        else:
            smap = vet.ssim_map(g, a, win=8)
            mssim = float(smap.mean())
            r.gate('golden.ssim', f"{mssim:.4f}", '>0.98', mssim > 0.98)
            dmax = float(np.abs(a - g).max())
            r.gate('golden.max_absdiff', f"{dmax:.4f}", '<0.02', dmax < 0.02)
            changed = float((np.abs(a - g) > 0.004).mean())
            r.gate('golden.changed_frac', f"{changed:.4f}", '<0.01', changed < 0.01)
            wy, wx = np.unravel_index(np.argmin(smap), smap.shape)
            worst = (int(wy) * 8, int(wx) * 8)
            if args.crop_out and mssim <= 0.9999:
                from PIL import Image
                y0 = max(0, min(a.shape[0] - 96, worst[0] - 44))
                x0 = max(0, min(a.shape[1] - 96, worst[1] - 44))
                crop_g = g[y0:y0 + 96, x0:x0 + 96]
                crop_a = a[y0:y0 + 96, x0:x0 + 96]
                pair = np.concatenate([crop_g, np.ones((96, 4)), crop_a], axis=1)
                Image.fromarray((np.clip(pair, 0, 1) * 255).astype(np.uint8)).resize(
                    (pair.shape[1] * 3, pair.shape[0] * 3), Image.NEAREST).save(args.crop_out)

    if worst is not None:
        crop = ' -> ' + args.crop_out if args.crop_out else ''
        r.footer(f"worst-region: y={worst[0]} x={worst[1]} (96px crop{crop})")

    if chroma_worst is not None:
        wy, wx, wval = chroma_worst
        crop = ''
        if args.crop_out and not args.golden:
            from PIL import Image
            rgb = vet.load_rgb(args.candidate)
            y0 = max(0, min(rgb.shape[0] - 96, wy - 32))
            x0 = max(0, min(rgb.shape[1] - 96, wx - 32))
            Image.fromarray((np.clip(rgb[y0:y0 + 96, x0:x0 + 96], 0, 1) * 255).astype(np.uint8)).resize(
                (288, 288), Image.NEAREST).save(args.crop_out)
            crop = ' -> ' + args.crop_out
        r.footer(f"magenta-cast worst-tile: y={wy} x={wx} idx={wval:.3f}{crop}")

    if cotton_worst is not None:
        wy, wx, wpx = cotton_worst
        ox = oy = 0
        if args.region:
            rx0, ry0, _, _ = [int(v) for v in args.region.split(',')]
            ox, oy = rx0, ry0
        crop = ''
        if args.crop_out and not args.golden:
            _crop_rgb(args.candidate, oy + wy, ox + wx, args.crop_out)
            crop = ' -> ' + args.crop_out
        r.footer(f"cotton worst-blob: y={oy + wy} x={ox + wx} ({wpx}px){crop}")

    if atlas_worst is not None:
        wy, wx, wl = atlas_worst
        crop = ''
        if args.crop_out and not args.golden:
            _crop_rgb(args.candidate, wy + 48, wx + 48, args.crop_out)
            crop = ' -> ' + args.crop_out
        r.footer(f"atlas worst background tile: y={wy} x={wx} lum={wl:.4f}{crop}")

    if args.bless and args.golden:
        Path(args.golden).write_bytes(Path(args.candidate).read_bytes())
        r.footer(f"blessed: {args.candidate} -> {args.golden}")

    n_fail = r.emit()
    sys.exit(1 if n_fail else 0)


if __name__ == '__main__':
    main()
