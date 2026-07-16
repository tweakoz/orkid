#!/bin/sh
""":"
# trampoline: exec the first python3 that has numpy+PIL (heightmap vet needs both).
for _py in \
    "$OBT_STAGE/pyvenv/bin/python3" \
    "$HOME"/.staging*/pyvenv/bin/python3 \
    "$(command -v python3)"; do
    [ -x "$_py" ] && "$_py" -c 'import numpy, PIL' 2>/dev/null && exec "$_py" "$0" "$@"
done
echo "ork.vet.hmap: no python3 with numpy+PIL found on this host" >&2
exit 3
":"""
"""
Heightmap vet (PNG/EXR): the ratified anti-spikiness methodology as porcelain.

  - per-step WALK metrics: reversal rate, jolt (|2nd deriv| p99), median slope
    -- judge relief plausibility by WALKING it, not by global min/max/avg
    (feedback_terrain_spikiness_walk)
  - FFT high-frequency-energy speckle check -- speckle is SPECTRAL, never
    min/max/avg (feedback_speckle_spectral)
  - isolated-spike max (single-texel outliers)
  - degenerate-map guard (flat / constant heightfields FAIL loudly)
  - optional golden compare (SSIM + worst-region crop); --bless

Heightfield goldens want 16-bit or EXR containers -- 8-bit quantization masks
small speckle (real signal lost to the container).
"""
import sys
import os
import argparse
from pathlib import Path

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _ork_vet_common as vet


def main():
    p = argparse.ArgumentParser(description='Vet a heightmap (PNG/EXR); porcelain verdicts')
    p.add_argument('candidate')
    p.add_argument('--golden', help='blessed reference heightmap to compare against')
    p.add_argument('--bless', action='store_true',
                   help='promote candidate to golden (after human OK)')
    p.add_argument('--crop-out', help='write worst-region golden|candidate crop PNG here')
    p.add_argument('--max-highband', type=float, default=0.02,
                   help='FFT high-band energy fraction ceiling (speckle)')
    p.add_argument('--max-slope', type=float, default=0.02, help='walk median-slope ceiling')
    p.add_argument('--max-jolt', type=float, default=0.01, help='walk jolt (p99 |2nd deriv|) ceiling')
    p.add_argument('--max-reversal', type=float, default=0.60, help='walk reversal-rate ceiling')
    p.add_argument('--max-spike', type=float, default=0.05, help='isolated-spike ceiling')
    p.add_argument('--min-range', type=float, default=0.01,
                   help='degenerate-map floor: max-min dynamic range')
    p.add_argument('--min-stddev', type=float, default=0.003,
                   help='degenerate-map floor: stddev')
    args = p.parse_args()

    a = vet.load_gray(args.candidate)
    r = vet.Report()

    r.info('stats.min_max_mean', f"{a.min():.4f}/{a.max():.4f}/{a.mean():.4f}")

    rng, std, degen = vet.degenerate_frame(a, args.min_range, args.min_stddev)
    r.info('frame.stddev', f"{std:.5f}")
    r.gate('frame.dynamic_range', f"{rng:.4f}", f">{args.min_range:g}", not degen)

    hf = vet.fft_highband_ratio(a)
    r.gate('spectral.highband_ratio', f"{hf:.4f}", f"<{args.max_highband:g}", hf < args.max_highband)

    slope, jolt, rev = vet.walk_metrics(a)
    r.gate('walk.median_slope', f"{slope:.5f}", f"<{args.max_slope:g}", slope < args.max_slope)
    r.gate('walk.jolt_p99', f"{jolt:.5f}", f"<{args.max_jolt:g}", jolt < args.max_jolt)
    r.gate('walk.reversal_rate', f"{rev:.3f}", f"<{args.max_reversal:g}", rev < args.max_reversal)

    sp = vet.spike_max(a)
    r.gate('spike.max_isolated', f"{sp:.4f}", f"<{args.max_spike:g}", sp < args.max_spike)

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
                pair = np.concatenate([g[y0:y0 + 96, x0:x0 + 96],
                                       np.ones((96, 4)),
                                       a[y0:y0 + 96, x0:x0 + 96]], axis=1)
                Image.fromarray((np.clip(pair, 0, 1) * 255).astype(np.uint8)).resize(
                    (pair.shape[1] * 3, pair.shape[0] * 3), Image.NEAREST).save(args.crop_out)

    if worst is not None:
        crop = ' -> ' + args.crop_out if args.crop_out else ''
        r.footer(f"worst-region: y={worst[0]} x={worst[1]} (96px crop{crop})")

    if args.bless and args.golden:
        Path(args.golden).write_bytes(Path(args.candidate).read_bytes())
        r.footer(f"blessed: {args.candidate} -> {args.golden}")

    n_fail = r.emit()
    sys.exit(1 if n_fail else 0)


if __name__ == '__main__':
    main()
