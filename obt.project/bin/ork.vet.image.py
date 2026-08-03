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


def main():
    p = argparse.ArgumentParser(description='Vet an image/heightfield; porcelain verdicts')
    p.add_argument('candidate')
    p.add_argument('--golden', help='blessed reference to compare against')
    p.add_argument('--kind', choices=['heightmap', 'render'], default='heightmap')
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
    args = p.parse_args()

    a = vet.load_gray(args.candidate)
    r = vet.Report()

    # stats: necessary NOT sufficient -> INFO only
    r.info('stats.min_max_mean', f"{a.min():.4f}/{a.max():.4f}/{a.mean():.4f}")
    clip = float(((a <= 0) | (a >= 1)).mean())
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
    if args.kind == 'render':
        chroma_worst = _render_checks(args, r)

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

    if args.bless and args.golden:
        Path(args.golden).write_bytes(Path(args.candidate).read_bytes())
        r.footer(f"blessed: {args.candidate} -> {args.golden}")

    n_fail = r.emit()
    sys.exit(1 if n_fail else 0)


if __name__ == '__main__':
    main()
