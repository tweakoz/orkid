"""
ork.imgtools - Image analysis utilities for testing and debugging.

Provides image summarization, comparison, and quality metrics
using PIL, numpy, and orkengine (for EXR). No OpenCV dependency.
"""

import numpy as np
from PIL import Image
from collections import OrderedDict
import json, math, struct

###############################################################################
# Loading
###############################################################################

def _load_via_orkengine(path):
    """Load image via orkengine's OIIO-backed Image class (handles EXR, HDR)."""
    from orkengine import core
    from orkengine import lev2
    img = lev2.Image.createFromFile(str(path))
    if not img or img.width == 0:
        raise RuntimeError(f"orkengine could not load {path}")
    w, h = img.width, img.height
    nc = img.numcomponents
    bpc = img.bytesPerChannel
    data = bytes(img.data.bytes)
    if bpc == 4:  # float32
        arr = np.frombuffer(data, dtype=np.float32).reshape(h, w, nc).copy()
    elif bpc == 2:  # half-float — decode to float32
        raw = np.frombuffer(data, dtype=np.uint16).reshape(h, w, nc)
        arr = np.zeros((h, w, nc), dtype=np.float32)
        for c in range(nc):
            arr[:, :, c] = _half_array_to_float(raw[:, :, c])
    elif bpc == 1:  # uint8
        arr = np.frombuffer(data, dtype=np.uint8).reshape(h, w, nc).astype(np.float32) / 255.0
    else:
        raise RuntimeError(f"Unsupported bpc={bpc}")
    # Ensure at least 3 channels
    if nc == 1:
        arr = np.stack([arr[:,:,0]]*3, axis=-1)
    mode = {1: "L", 3: "RGB", 4: "RGBA"}.get(nc, "RGBA")
    return arr, mode, (w, h)

def _half_array_to_float(arr_u16):
    """Convert array of uint16 half-float bits to float32."""
    sign = ((arr_u16 >> 15) & 1).astype(np.float32)
    exp = ((arr_u16 >> 10) & 0x1F).astype(np.int32)
    mant = (arr_u16 & 0x3FF).astype(np.float32)
    result = np.zeros_like(sign)
    # Normal numbers
    normal = (exp > 0) & (exp < 31)
    result[normal] = ((-1.0)**sign[normal]) * (2.0**(exp[normal] - 15)) * (1.0 + mant[normal] / 1024.0)
    # Subnormals
    subnorm = (exp == 0) & (mant > 0)
    result[subnorm] = ((-1.0)**sign[subnorm]) * (2.0**-14) * (mant[subnorm] / 1024.0)
    return result

def load_image(path):
    """Load image as numpy float32 array in [0,1] range (RGB or RGBA).
    Uses PIL for PNG/JPG, falls back to orkengine for EXR/HDR."""
    path = str(path)
    # Use orkengine for formats PIL can't handle
    if path.lower().endswith(('.exr', '.hdr')):
        return _load_via_orkengine(path)
    # PIL path for PNG, JPG, etc.
    img = Image.open(path)
    arr = np.array(img, dtype=np.float32)
    if img.mode in ("L", "P"):
        arr = arr / 255.0
        arr = np.stack([arr, arr, arr], axis=-1)
    if arr.dtype != np.float32:
        arr = arr.astype(np.float32)
    if img.mode in ("RGB", "RGBA", "L", "P"):
        if arr.ndim == 2:
            arr = np.stack([arr, arr, arr], axis=-1)
        maxval = 255.0 if img.mode != "I;16" else 65535.0
        if arr.max() > 1.5:
            arr = arr / maxval
    return arr, img.mode, img.size

###############################################################################
# Channel statistics
###############################################################################

def channel_stats(arr):
    """Per-channel min/max/mean/std/percentiles for an HxWxC array."""
    nc = arr.shape[2] if arr.ndim == 3 else 1
    labels = ["R", "G", "B", "A"][:nc]
    stats = OrderedDict()
    for i, label in enumerate(labels):
        ch = arr[:, :, i].ravel() if arr.ndim == 3 else arr.ravel()
        p50, p99, p999, p9999 = np.percentile(ch, [50, 99, 99.9, 99.99])
        stats[label] = {
            "min": float(np.min(ch)),
            "p50": float(p50),
            "p99": float(p99),
            "p99.9": float(p999),
            "p99.99": float(p9999),
            "max": float(np.max(ch)),
            "mean": float(np.mean(ch)),
            "std": float(np.std(ch)),
        }
    return stats

def firefly_stats(arr, spatial_threshold=10.0, global_thresholds=(10, 100)):
    """Detect firefly artifacts in an image.

    Returns dict with:
      - per-channel firefly counts at each global threshold
      - spatial outlier count (pixels > spatial_threshold × local 3x3 median)
      - max/p99.9 ratio per channel (>10 suggests fireflies)
    """
    from scipy.ndimage import median_filter

    nc = min(arr.shape[2], 3) if arr.ndim == 3 else 1
    labels = ["R", "G", "B"][:nc]

    # Brightness = max of RGB channels
    if nc >= 3:
        brightness = np.max(arr[:, :, :3], axis=2)
    else:
        brightness = arr[:, :, 0] if arr.ndim == 3 else arr

    result = OrderedDict()

    # Per-channel firefly counts at global thresholds
    for i, label in enumerate(labels):
        ch = arr[:, :, i] if arr.ndim == 3 else arr
        ch_flat = ch.ravel()
        median = float(np.median(ch_flat))
        p999 = float(np.percentile(ch_flat, 99.9))
        ch_max = float(np.max(ch_flat))
        ch_stats = {"median": round(median, 4), "max": round(ch_max, 4)}
        ch_stats["max_over_p999"] = round(ch_max / p999, 2) if p999 > 0 else 0.0
        for thresh in global_thresholds:
            if median > 0:
                count = int(np.sum(ch_flat > median * thresh))
                ch_stats[f">{thresh}x_median"] = count
            else:
                ch_stats[f">{thresh}x_median"] = 0
        result[label] = ch_stats

    # Spatial outliers: pixels much brighter than 3x3 neighborhood median
    local_med = median_filter(brightness, size=3)
    # Avoid division by zero
    safe_local = np.maximum(local_med, 1e-6)
    spatial_outliers = int(np.sum(brightness > safe_local * spatial_threshold))
    total_pixels = brightness.size
    result["spatial_outliers"] = spatial_outliers
    result["spatial_outlier_frac"] = round(spatial_outliers / total_pixels, 6)
    result["total_pixels"] = total_pixels

    return result

###############################################################################
# Color analysis
###############################################################################

def grayscale_fraction(arr, rel_tol=0.05, abs_tol=0.01):
    """Fraction of pixels where R≈G≈B (individually gray).
    Uses max(rel_tol * brightness, abs_tol) as the per-pixel tolerance."""
    if arr.ndim != 3 or arr.shape[2] < 3:
        return 1.0
    r, g, b = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    brightness = np.maximum(np.maximum(r, g), b)
    tol = np.maximum(brightness * rel_tol, abs_tol)
    gray = (np.abs(r - g) < tol) & (np.abs(r - b) < tol) & (np.abs(g - b) < tol)
    return float(np.mean(gray))

def color_histogram(arr, bins=16):
    """Simplified hue histogram (ignoring dark/gray pixels).
    Returns dict with hue bin counts and dominant hue info."""
    if arr.ndim != 3 or arr.shape[2] < 3:
        return {"dominant": "gray", "saturation_mean": 0.0}

    r, g, b = arr[:, :, 0].ravel(), arr[:, :, 1].ravel(), arr[:, :, 2].ravel()
    maxc = np.maximum(np.maximum(r, g), b)
    minc = np.minimum(np.minimum(r, g), b)
    sat = np.where(maxc > 0.01, (maxc - minc) / maxc, 0.0)

    # Only analyze pixels with meaningful saturation and brightness
    mask = (sat > 0.1) & (maxc > 0.02)
    if mask.sum() < 10:
        return {"dominant": "gray", "saturation_mean": float(np.mean(sat)),
                "colorful_frac": float(mask.mean())}

    # Compute hue for colorful pixels
    rm, gm, bm = r[mask], g[mask], b[mask]
    maxm, minm = maxc[mask], minc[mask]
    delta = maxm - minm

    hue = np.zeros_like(rm)
    r_max = rm == maxm
    g_max = (~r_max) & (gm == maxm)
    b_max = (~r_max) & (~g_max)

    hue[r_max] = 60.0 * (((gm[r_max] - bm[r_max]) / delta[r_max]) % 6)
    hue[g_max] = 60.0 * (((bm[g_max] - rm[g_max]) / delta[g_max]) + 2)
    hue[b_max] = 60.0 * (((rm[b_max] - gm[b_max]) / delta[b_max]) + 4)

    hist, edges = np.histogram(hue, bins=bins, range=(0, 360))
    peak_bin = int(np.argmax(hist))
    peak_hue = float((edges[peak_bin] + edges[peak_bin + 1]) / 2)

    # Name the dominant hue
    hue_names = ["red", "orange", "yellow", "yellow-green", "green", "cyan-green",
                 "cyan", "blue-cyan", "blue", "blue-violet", "violet", "magenta"]
    hue_idx = int(peak_hue / 30) % 12
    dominant = hue_names[hue_idx]

    return {
        "dominant_hue": dominant,
        "dominant_hue_deg": round(peak_hue, 1),
        "saturation_mean": round(float(np.mean(sat)), 4),
        "saturation_colorful": round(float(np.mean(sat[mask])), 4),
        "colorful_frac": round(float(mask.mean()), 4),
        "hue_histogram": hist.tolist(),
    }

###############################################################################
# Spatial analysis
###############################################################################

def quadrant_means(arr):
    """Mean RGB per quadrant (TL, TR, BL, BR) and center."""
    h, w = arr.shape[:2]
    ch = min(arr.shape[2], 3) if arr.ndim == 3 else 1
    regions = OrderedDict()
    slices = {
        "top_left": (slice(0, h//2), slice(0, w//2)),
        "top_right": (slice(0, h//2), slice(w//2, w)),
        "bottom_left": (slice(h//2, h), slice(0, w//2)),
        "bottom_right": (slice(h//2, h), slice(w//2, w)),
        "center": (slice(h//4, 3*h//4), slice(w//4, 3*w//4)),
    }
    for name, (ys, xs) in slices.items():
        region = arr[ys, xs]
        means = [round(float(np.mean(region[:, :, c])), 4) for c in range(ch)]
        regions[name] = {"R": means[0], "G": means[1] if ch > 1 else means[0],
                         "B": means[2] if ch > 2 else means[0]}
    return regions

def detect_scanlines(arr, axis=0):
    """Detect horizontal (axis=0) or vertical (axis=1) scanline artifacts.
    Returns a score (0 = no artifacts, higher = more artifacts) and details."""
    if arr.ndim == 3:
        lum = 0.2126 * arr[:, :, 0] + 0.7152 * arr[:, :, 1] + 0.0722 * arr[:, :, 2]
    else:
        lum = arr

    # Compute mean brightness per row (or column)
    row_means = np.mean(lum, axis=1 if axis == 0 else 0)

    # Detect alternating pattern: compute difference between adjacent rows
    diffs = np.diff(row_means)

    # Scanline artifacts create a high-frequency alternating pattern
    # Check for sign alternation in the diffs
    signs = np.sign(diffs)
    sign_changes = np.abs(np.diff(signs))
    alternation_rate = float(np.mean(sign_changes > 0))

    # Also check magnitude of row-to-row variation vs overall std
    row_variation = float(np.std(diffs))
    overall_std = float(np.std(lum))

    # Score: high alternation + high variation relative to content = artifacts
    if overall_std < 0.001:
        score = 0.0  # uniform image
    else:
        score = alternation_rate * (row_variation / overall_std)

    return {
        "score": round(score, 4),
        "alternation_rate": round(alternation_rate, 4),
        "row_variation": round(row_variation, 6),
        "overall_std": round(overall_std, 4),
        "axis": "horizontal" if axis == 0 else "vertical",
    }

###############################################################################
# Black / uniform detection
###############################################################################

def black_fraction(arr, threshold=0.001):
    """Fraction of pixels that are near-black."""
    if arr.ndim == 3:
        brightness = np.max(arr[:, :, :3], axis=2)
    else:
        brightness = arr
    return round(float(np.mean(brightness < threshold)), 4)

def is_uniform(arr, tol=0.01):
    """Check if image is uniform (all pixels same color)."""
    if arr.ndim == 3:
        for c in range(min(arr.shape[2], 3)):
            if np.ptp(arr[:, :, c]) > tol:
                return False
        return True
    return float(np.ptp(arr)) <= tol

###############################################################################
# 4x4 grid matrix — compact spatial color summary
###############################################################################

def grid_matrix(arr, rows=4, cols=4):
    """Divide image into a rows x cols grid. For each cell, compute
    avg/min/max RGB. Returns a list of rows, each a list of cell dicts."""
    h, w = arr.shape[:2]
    nc = min(arr.shape[2], 3) if arr.ndim == 3 else 1
    ch_names = ["R", "G", "B"][:nc]

    grid = []
    for gy in range(rows):
        row = []
        y0 = gy * h // rows
        y1 = (gy + 1) * h // rows
        for gx in range(cols):
            x0 = gx * w // cols
            x1 = (gx + 1) * w // cols
            cell = arr[y0:y1, x0:x1, :nc] if nc > 1 else arr[y0:y1, x0:x1]
            cell_dict = {}
            for ci, ch in enumerate(ch_names):
                c = cell[:, :, ci] if nc > 1 else cell
                cell_dict[ch] = {
                    "avg": round(float(np.mean(c)), 4),
                    "min": round(float(np.min(c)), 4),
                    "max": round(float(np.max(c)), 4),
                }
            # Compact color summary for the cell
            if nc >= 3:
                ravg = cell_dict["R"]["avg"]
                gavg = cell_dict["G"]["avg"]
                bavg = cell_dict["B"]["avg"]
                brightness = max(ravg, gavg, bavg)
                cell_dict["_brightness"] = round(brightness, 4)
                if brightness < 0.01:
                    cell_dict["_color"] = "black"
                else:
                    tol = max(brightness * 0.05, 0.01)
                    if abs(ravg - gavg) < tol and abs(ravg - bavg) < tol:
                        cell_dict["_color"] = "gray"
                    else:
                        # Simple dominant channel naming
                        if ravg >= gavg and ravg >= bavg:
                            cell_dict["_color"] = "red" if ravg > 2 * max(gavg, bavg) else "warm"
                        elif gavg >= ravg and gavg >= bavg:
                            cell_dict["_color"] = "green" if gavg > 2 * max(ravg, bavg) else "green-ish"
                        else:
                            cell_dict["_color"] = "blue" if bavg > 2 * max(ravg, gavg) else "cool"
            row.append(cell_dict)
        grid.append(row)
    return grid

def print_grid_compact(grid):
    """Print grid as a compact visual table."""
    rows = len(grid)
    cols = len(grid[0]) if rows > 0 else 0
    # Header
    hdr = "     " + "".join(f"  col{c:<2}" for c in range(cols))
    print(hdr)
    print("     " + "-" * (cols * 7))
    for gy, row in enumerate(grid):
        parts = []
        for cell in row:
            r = cell.get("R", {}).get("avg", 0)
            g = cell.get("G", {}).get("avg", 0)
            b = cell.get("B", {}).get("avg", 0)
            tag = cell.get("_color", "?")
            parts.append(f"{tag:>6s}")
        print(f"  r{gy} |" + "|".join(parts) + "|")
    print()
    # Detailed table
    for gy, row in enumerate(grid):
        for gx, cell in enumerate(row):
            r = cell.get("R", {}).get("avg", 0)
            g = cell.get("G", {}).get("avg", 0)
            b = cell.get("B", {}).get("avg", 0)
            br = cell.get("_brightness", 0)
            tag = cell.get("_color", "?")
            print(f"  [{gy},{gx}] avg=({r:.3f},{g:.3f},{b:.3f}) bri={br:.3f} {tag}")

###############################################################################
# XIR loading
###############################################################################

def load_xir(path):
    """Load an XIR file and return a dict of named image arrays.
    Returns dict: {
      "specular_0_r0.0000": (arr, mode, size),
      "specular_1_r0.5000": (arr, mode, size),
      ...
      "diffuse_0": (arr, mode, size),
      ...
    }
    Each value is a (numpy_array, mode_str, (w,h)) tuple like load_image returns.
    """
    from orkengine import core
    from orkengine import lev2
    result = lev2.EnvMapProcessor.readXIR(str(path))
    specular_images = list(result["specular_images"])
    roughness_values = list(result["roughness_values"])
    diffuse_images = list(result["diffuse_images"])

    images = OrderedDict()
    for i, img in enumerate(specular_images):
        r = roughness_values[i] if i < len(roughness_values) else 0.0
        key = f"specular_{i}_r{r:.4f}"
        images[key] = _ork_image_to_array(img)
    for i, img in enumerate(diffuse_images):
        key = f"diffuse_{i}"
        images[key] = _ork_image_to_array(img)
    return images

def _ork_image_to_array(img):
    """Convert an orkengine Image to (numpy_array, mode, (w,h)) tuple."""
    w, h = img.width, img.height
    nc = img.numcomponents
    bpc = img.bytesPerChannel
    data = bytes(img.data.bytes)
    if bpc == 4:
        arr = np.frombuffer(data, dtype=np.float32).reshape(h, w, nc).copy()
    elif bpc == 2:
        raw = np.frombuffer(data, dtype=np.uint16).reshape(h, w, nc)
        arr = np.zeros((h, w, nc), dtype=np.float32)
        for c in range(nc):
            arr[:, :, c] = _half_array_to_float(raw[:, :, c])
    elif bpc == 1:
        arr = np.frombuffer(data, dtype=np.uint8).reshape(h, w, nc).astype(np.float32) / 255.0
    else:
        raise RuntimeError(f"Unsupported bpc={bpc}")
    if nc == 1:
        arr = np.stack([arr[:,:,0]]*3, axis=-1)
    mode = {1: "L", 3: "RGB", 4: "RGBA"}.get(nc, "RGBA")
    return arr, mode, (w, h)

###############################################################################
# Full summary
###############################################################################

def summarize(path):
    """Generate a comprehensive summary dict for an image file."""
    arr, mode, size = load_image(path)
    w, h = size

    summary = OrderedDict()
    summary["path"] = str(path)
    summary["size"] = {"width": w, "height": h}
    summary["mode"] = mode
    summary["channels"] = channel_stats(arr)
    summary["black_frac"] = black_fraction(arr)
    summary["gray_frac"] = grayscale_fraction(arr)
    summary["is_uniform"] = is_uniform(arr)
    summary["color"] = color_histogram(arr)
    summary["quadrants"] = quadrant_means(arr)
    summary["fireflies"] = firefly_stats(arr)
    summary["scanlines_h"] = detect_scanlines(arr, axis=0)
    summary["scanlines_v"] = detect_scanlines(arr, axis=1)
    summary["grid_4x4"] = grid_matrix(arr, 4, 4)

    return summary

def print_summary(summary, indent=0):
    """Pretty-print a summary dict."""
    pad = "  " * indent
    for k, v in summary.items():
        if isinstance(v, dict):
            print(f"{pad}{k}:")
            print_summary(v, indent + 1)
        elif isinstance(v, list):
            print(f"{pad}{k}: {v}")
        else:
            print(f"{pad}{k}: {v}")
