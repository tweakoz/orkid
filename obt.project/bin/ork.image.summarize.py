#!/usr/bin/env ork.python
"""
ork.image.summarize - Summarize image content for testing and debugging.

Usage:
  ork.image.summarize.py -i <image_file> [--json] [--grid] [--compare <other>]

Modes:
  (default)   Full summary: channels, color, quadrants, scanlines
  --grid      4x4 spatial color grid (compact view of image content)
  --compare   Side-by-side comparison with another image
  --json      Machine-readable JSON output
"""

import sys, os, argparse, json
from orkengine import core
from orkengine import lev2
lev2.lev2appinit()

# Add orkid scripts to path
_script_dir = os.path.dirname(os.path.abspath(__file__))
_scripts_dir = os.path.join(os.path.dirname(_script_dir), "scripts")
if _scripts_dir not in sys.path:
    sys.path.insert(0, _scripts_dir)

from ork.imgtools import (summarize, print_summary, load_image, channel_stats,
                          grayscale_fraction, grid_matrix, print_grid_compact,
                          load_xir, firefly_stats)

def summarize_xir(args):
    """Summarize all images in an XIR file."""
    images = load_xir(args.input)
    basename = os.path.basename(args.input)

    # Parse grid size
    rows, cols = 4, 4
    if "x" in args.grid_size:
        parts = args.grid_size.split("x")
        rows, cols = int(parts[0]), int(parts[1])

    print(f"=== XIR Summary: {basename} ({len(images)} images) ===")
    all_summaries = {}
    for name, (arr, mode, size) in images.items():
        w, h = size
        stats = channel_stats(arr)
        ff = firefly_stats(arr)
        bf = float((arr[:,:,:3].max(axis=2) < 0.001).mean()) if arr.shape[2] >= 3 else 0.0
        is_black = bf > 0.99

        if args.json:
            entry = {"size": {"width": w, "height": h}, "mode": mode,
                     "channels": stats, "black_frac": round(bf, 4),
                     "fireflies": ff}
            if args.grid:
                entry["grid"] = grid_matrix(arr, rows, cols)
            all_summaries[name] = entry
        else:
            # Determine status
            has_fireflies = ff.get("spatial_outliers", 0) > 0
            if is_black:
                status = "BLACK"
            elif has_fireflies:
                status = "FIREFLIES"
            else:
                status = "ok"

            print(f"\n  {name} [{status}] {w}x{h} {mode}")
            for ch in ["R", "G", "B"]:
                s = stats.get(ch, {})
                fs = ff.get(ch, {})
                ff_10x = fs.get(">10x_median", 0)
                ff_100x = fs.get(">100x_median", 0)
                max_p999 = fs.get("max_over_p999", 0)
                ff_str = ""
                if ff_10x > 0:
                    ff_str = f"  fireflies: {ff_10x}(>10x) {ff_100x}(>100x) max/p999={max_p999:.1f}x"
                print(f"    {ch}: min={s.get('min',0):.4f} p50={s.get('p50',0):.4f} "
                      f"p99={s.get('p99',0):.4f} p99.9={s.get('p99.9',0):.4f} "
                      f"max={s.get('max',0):.4f}{ff_str}")
            so = ff.get("spatial_outliers", 0)
            so_frac = ff.get("spatial_outlier_frac", 0)
            print(f"    black_frac={bf:.4f}  spatial_outliers={so} ({so_frac*100:.4f}%)")
            if args.grid:
                g = grid_matrix(arr, rows, cols)
                print_grid_compact(g)

    if args.json:
        print(json.dumps(all_summaries, indent=2))

    return 0

def compare_images(path_a, path_b):
    """Compare two images and report differences."""
    import numpy as np

    arr_a, mode_a, size_a = load_image(path_a)
    arr_b, mode_b, size_b = load_image(path_b)

    print(f"\n--- Comparison: {os.path.basename(path_a)} vs {os.path.basename(path_b)} ---")
    print(f"  A: {size_a[0]}x{size_a[1]} {mode_a}")
    print(f"  B: {size_b[0]}x{size_b[1]} {mode_b}")

    if size_a != size_b:
        print(f"  [!] Size mismatch — cannot do pixel comparison")
        return

    nc = min(arr_a.shape[2], arr_b.shape[2], 3)
    diff = np.abs(arr_a[:, :, :nc] - arr_b[:, :, :nc])

    for i, ch in enumerate(["R", "G", "B"][:nc]):
        d = diff[:, :, i]
        print(f"  {ch} diff: mean={np.mean(d):.6f} max={np.max(d):.6f} "
              f"std={np.std(d):.6f} >0.01={np.mean(d > 0.01)*100:.1f}%")

    rmse = float(np.sqrt(np.mean(diff ** 2)))
    print(f"  RMSE: {rmse:.6f}")

    gf_a = grayscale_fraction(arr_a)
    gf_b = grayscale_fraction(arr_b)
    print(f"  gray_frac: A={gf_a:.4f} B={gf_b:.4f} delta={abs(gf_a-gf_b):.4f}")

    # Grid comparison
    print(f"\n  Grid A:")
    ga = grid_matrix(arr_a, 4, 4)
    print_grid_compact(ga)
    print(f"  Grid B:")
    gb = grid_matrix(arr_b, 4, 4)
    print_grid_compact(gb)

def main():
    parser = argparse.ArgumentParser(
        description="Summarize image content for testing and debugging")
    parser.add_argument("-i", "--input", type=str, required=True,
                        help="Image file to analyze")
    parser.add_argument("--json", action="store_true",
                        help="Output as JSON instead of human-readable")
    parser.add_argument("--grid", action="store_true",
                        help="Show 4x4 spatial color grid")
    parser.add_argument("--grid-size", type=str, default="4x4",
                        help="Grid dimensions, e.g. 4x4, 8x8 (default: 4x4)")
    parser.add_argument("--compare", type=str, default=None,
                        help="Compare against another image")
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: {args.input} not found", file=sys.stderr)
        return 1

    # XIR files get special handling
    if args.input.lower().endswith('.xir'):
        return summarize_xir(args)

    # Parse grid size
    rows, cols = 4, 4
    if "x" in args.grid_size:
        parts = args.grid_size.split("x")
        rows, cols = int(parts[0]), int(parts[1])

    if args.grid:
        arr, mode, size = load_image(args.input)
        print(f"=== Grid: {os.path.basename(args.input)} ({size[0]}x{size[1]} {mode}) ===")
        g = grid_matrix(arr, rows, cols)
        if args.json:
            print(json.dumps(g, indent=2))
        else:
            print_grid_compact(g)
        gf = grayscale_fraction(arr)
        print(f"  gray_frac={gf:.4f}  black_frac={float((arr[:,:,:3].max(axis=2) < 0.001).mean()):.4f}")
    elif args.compare:
        if not os.path.exists(args.compare):
            print(f"Error: {args.compare} not found", file=sys.stderr)
            return 1
        compare_images(args.input, args.compare)
    else:
        summary = summarize(args.input)
        if args.json:
            print(json.dumps(summary, indent=2))
        else:
            print(f"=== Image Summary: {os.path.basename(args.input)} ===")
            print_summary(summary)

    return 0

if __name__ == "__main__":
    sys.exit(main())
