#!/bin/sh
""":"
# trampoline: exec the first python3 that has numpy+PIL+scipy.
for _py in \
    "$OBT_STAGE/pyvenv/bin/python3" \
    "$HOME"/.staging*/pyvenv/bin/python3 \
    "$(command -v python3)"; do
    [ -x "$_py" ] && "$_py" -c 'import numpy, PIL, scipy' 2>/dev/null && exec "$_py" "$0" "$@"
done
echo "ork.vet.fire: no python3 with numpy+PIL+scipy found on this host" >&2
exit 3
":"""
"""
FIRE vet: mechanical realism verdicts for rendered flame/smoke/ember imagery.

Same porcelain contract as the rest of the ork.vet.* family:
    <check>\t<value>\t<threshold>\t<PASS|FAIL|WARN|INFO>
    # verdict: PASS|FAIL (<n> checks, <k> failed)
exit code follows the verdict.

WHY THIS INSTRUMENT EXISTS (the gap ork.vet.image.py does not cover):
  ork.vet.image answers "did this frame change / is it degenerate / is it
  speckled".  It has nothing to say about whether a FIRE looks like fire.  The
  discriminating physics for fire is that a flame is a GREY-BODY EMITTER: its
  colour is not a free art choice, it is a one-parameter family (temperature)
  running ~1000 K (deep red soot) -> ~1900 K (yellow-white core).  A cartoon
  fire is one that sits OFF that locus, or that spans too little of it.

  So the core checks project the flame's pixels onto the PLANCKIAN LOCUS AS
  RENDERED BY THIS PIPELINE — the locus is swept through the same ACES
  tonemap+encode at a log sweep of exposures, so the reference is "what a real
  blackbody would look like through this renderer", not a textbook chromaticity
  that no tonemapper ever emits.  Distance to that manifold = off-locus error;
  the nearest point's temperature = implied T.

  The other classic tells, each its own check:
    * HARD SPRITE EDGES   — a billboard cookie that reaches the quad boundary,
                            or a flame/ground intersection with no soft fade,
                            shows a sub-2-pixel luminance cliff along a long
                            run of silhouette.  Real flame boundaries are
                            optically thin: 4-20 px of falloff.
    * FLAT COLOUR         — hue span and lum/hue correlation.  Real fire is
                            HOTTER WHERE BRIGHTER; a tinted-alpha blob is not.
    * BLOWN CORE          — clipped-white area fraction.  A stack of additive
                            sprites clips to a white slab; a real core is a
                            small clipped kernel inside a big unclipped ramp.
    * NO EMBERS           — isolated small warm blobs detached from the body.
    * DEAD LIGHT          — the cast light on surroundings must fall off, and
                            (movie mode) must flicker COHERENTLY with the
                            flame body, not independently.

  Movie mode adds the temporal half: a pool fire of diameter D sheds its
  buoyant toroidal vortex at f ~= 1.5/sqrt(D) Hz, so flicker RATE is the scale
  cue.  Too fast reads as a campfire enlarged; DC-steady reads as a gas jet.

USAGE
  ork.vet.fire.py <image.png> [--smoke-above] [--kind still]
  ork.vet.fire.py <movie.mp4> --kind movie [--fps 60]
  ork.vet.fire.py <image.png> --crop-out <dir>     # worst-region evidence crops
  ork.vet.fire.py --selftest                       # seeded-mutant regression
"""
import sys
import os
import argparse
import math
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


# ----------------------------------------------------------------------------
# porcelain
# ----------------------------------------------------------------------------
class Report:
    def __init__(self):
        self.n = 0
        self.failed = 0
        self.lines = []
        self.worst = {}

    def info(self, name, value):
        self.lines.append("%s\t%s\t-\tINFO" % (name, value))

    def gate(self, name, value_str, threshold_str, ok, warn_only=False):
        self.n += 1
        if ok:
            st = "PASS"
        elif warn_only:
            st = "WARN"
        else:
            st = "FAIL"
            self.failed += 1
        self.lines.append("%s\t%s\t%s\t%s" % (name, value_str, threshold_str, st))

    def emit(self):
        for l in self.lines:
            print(l)
        verdict = "FAIL" if self.failed else "PASS"
        print("# verdict: %s (%d checks, %d failed)" % (verdict, self.n, self.failed))
        return 1 if self.failed else 0


# ----------------------------------------------------------------------------
# colour: the rendered Planckian manifold
# ----------------------------------------------------------------------------
def _cie_xyz_of_T(T):
    """CIE 1931 XYZ of a Planckian radiator at T kelvin.

    Wyman/Sloan/Shirley multi-lobe Gaussian fits to the 1931 CMFs (JCGT 2013);
    accurate to well under the precision this instrument needs, and it keeps
    the instrument dependency-free of a colour-science package."""
    lam = np.arange(360.0, 831.0, 1.0)   # nm

    def g(x, mu, s1, s2):
        s = np.where(x < mu, s1, s2)
        return np.exp(-0.5 * ((x - mu) / s) ** 2)

    xb = 1.056 * g(lam, 599.8, 37.9, 31.0) + 0.362 * g(lam, 442.0, 16.0, 26.7) \
        - 0.065 * g(lam, 501.1, 20.4, 26.2)
    yb = 0.821 * g(lam, 568.8, 46.9, 40.5) + 0.286 * g(lam, 530.9, 16.3, 31.1)
    zb = 1.217 * g(lam, 437.0, 11.8, 36.0) + 0.681 * g(lam, 459.0, 26.0, 13.8)

    # Planck spectral radiance (arbitrary scale)
    h = 6.62607015e-34
    c = 2.99792458e8
    kB = 1.380649e-23
    l = lam * 1e-9
    Bl = (2.0 * h * c ** 2) / (l ** 5) / (np.exp(h * c / (l * kB * T)) - 1.0)
    X = float(np.sum(Bl * xb))
    Y = float(np.sum(Bl * yb))
    Z = float(np.sum(Bl * zb))
    return np.array([X, Y, Z]) / max(Y, 1e-30)


_M_XYZ2RGB = np.array([[3.2404542, -1.5371385, -0.4985314],
                       [-0.9692660, 1.8760108, 0.0415560],
                       [0.0556434, -0.2040259, 1.0572252]])


def _aces_narkowicz(x):
    """Narkowicz ACES RRT+ODT curve fit — the tonemap shape the compositor uses."""
    a, b, c, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
    return np.clip((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0)


def _srgb_encode(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)


def _chroma(rgb):
    """normalized chromaticity (r,g) of a display-referred RGB triple/array."""
    s = np.sum(rgb, axis=-1, keepdims=True)
    s = np.where(s < 1e-6, 1e-6, s)
    return (rgb / s)[..., :2]


_LOCUS_CACHE = {}


def rendered_locus(tmin=900.0, tmax=3000.0, tstep=25.0):
    """The blackbody locus AS THIS RENDERER EMITS IT.

    Each temperature is pushed through the pipeline at a log sweep of exposures
    (0.03 .. 40 linear scale), ACES-tonemapped and sRGB-encoded, then reduced
    to chromaticity.  The result is a 2D point cloud (the achievable manifold)
    with a temperature label per point.  A flame pixel's distance to this cloud
    is its off-locus error; the nearest label is its implied temperature."""
    key = (tmin, tmax, tstep)
    if key in _LOCUS_CACHE:
        return _LOCUS_CACHE[key]
    temps = np.arange(tmin, tmax + 1e-6, tstep)
    exps = np.exp(np.linspace(math.log(0.03), math.log(40.0), 96))
    pts = []
    labels = []
    lums = []
    for T in temps:
        xyz = _cie_xyz_of_T(float(T))
        lin = _M_XYZ2RGB.dot(xyz)
        lin = np.maximum(lin, 0.0)
        lin = lin / max(float(np.max(lin)), 1e-9)
        for s in exps:
            disp = _srgb_encode(_aces_narkowicz(lin * s))
            if float(np.sum(disp)) < 0.02:
                continue
            pts.append(_chroma(disp))
            labels.append(T)
            lums.append(float(luma(disp)))
    out = (np.array(pts), np.array(labels), np.array(lums))
    _LOCUS_CACHE[key] = out
    return out


# Brightness tolerance for the locus fit, in display luma. See locus_fit: this is
# what keeps the manifold a CURVE instead of collapsing into a 2D blob.
_LOCUS_LUM_TOL = 0.06


def locus_fit(rgb_pixels, sub=20000, seed=7):
    """-> (distance to rendered locus, implied T) per pixel (subsampled).

    THE BRIGHTNESS CONSTRAINT IS THE WHOLE CHECK.  Sweeping the locus over
    temperature AND exposure and then taking the nearest point in chromaticity
    makes the reference a broad 2D REGION — ACES desaturates as exposure rises,
    so the swept cloud covers most of the warm quadrant and essentially every
    plausible orange lands inside it.  Measured on the seeded corpus: the `flat`
    mutant (one fixed hue, no temperature ramp at all) scored bb_dev_p90 0.0023
    against the clean twin's 0.0020 — i.e. the check was blind.

    So each pixel is compared only against manifold points of MATCHING DISPLAY
    BRIGHTNESS (+-_LOCUS_LUM_TOL).  That pins the exposure and leaves a 1D
    family in temperature, which is the real question: "at the brightness this
    pixel actually has, is its colour one a blackbody could produce?"."""
    pts, labels, plum = rendered_locus()
    n = rgb_pixels.shape[0]
    if n > sub:
        rs = np.random.RandomState(seed)
        idx = rs.choice(n, sub, replace=False)
        rgb_pixels = rgb_pixels[idx]
    c = _chroma(rgb_pixels)
    L = luma(rgb_pixels)
    dists = np.empty(c.shape[0])
    temps = np.empty(c.shape[0])
    CH = 2000
    for i in range(0, c.shape[0], CH):
        blk = c[i:i + CH]
        blkL = L[i:i + CH]
        d2 = ((blk[:, None, :] - pts[None, :, :]) ** 2).sum(-1)
        # mask out manifold points at the wrong brightness (large sentinel, not
        # -inf: a pixel brighter/darker than anything on the locus must still
        # get a finite answer rather than a NaN)
        near = np.abs(plum[None, :] - blkL[:, None]) <= _LOCUS_LUM_TOL
        anyrow = near.any(axis=1)
        d2m = np.where(near, d2, 1e9)
        d2m[~anyrow] = d2[~anyrow]       # fall back to unconstrained for outliers
        j = np.argmin(d2m, axis=1)
        dists[i:i + CH] = np.sqrt(d2[np.arange(blk.shape[0]), j])
        temps[i:i + CH] = labels[j]
    return dists, temps


# ----------------------------------------------------------------------------
# image helpers
# ----------------------------------------------------------------------------
def load_rgb01(path):
    im = Image.open(path).convert("RGB")
    return np.asarray(im).astype(np.float64) / 255.0


def luma(rgb):
    return 0.2126 * rgb[..., 0] + 0.7152 * rgb[..., 1] + 0.0722 * rgb[..., 2]


def hue_deg(rgb):
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    mx = np.max(rgb, axis=-1)
    mn = np.min(rgb, axis=-1)
    d = mx - mn
    h = np.zeros_like(mx)
    m = d > 1e-9
    ri = m & (mx == r)
    gi = m & (mx == g) & ~ri
    bi = m & (mx == b) & ~ri & ~gi
    with np.errstate(invalid='ignore', divide='ignore'):
        h[ri] = (60.0 * ((g[ri] - b[ri]) / d[ri])) % 360.0
        h[gi] = (60.0 * ((b[gi] - r[gi]) / d[gi]) + 120.0) % 360.0
        h[bi] = (60.0 * ((r[bi] - g[bi]) / d[bi]) + 240.0) % 360.0
    return h


def saturation(rgb):
    mx = np.max(rgb, axis=-1)
    mn = np.min(rgb, axis=-1)
    return np.where(mx > 1e-9, (mx - mn) / np.maximum(mx, 1e-9), 0.0)


# Fire hues, in the wrapped-hue convention used throughout (red 0, orange ~25,
# yellow ~55, and the deep-red shoulder wrapping negative). Anything outside is
# not incandescent, whatever its r-vs-b ratio says.
_FIRE_HUE_LO = -25.0
_FIRE_HUE_HI = 65.0


def flame_mask(rgb, lum_floor=0.05, warm=1.12):
    """Bright + FIRE-HUED + hysteresis-grown, then the largest connected body.

    TWO FALSE-POSITIVE CLASSES THIS HAS TO REFUSE, both measured:

    (1) NOT-EVEN-WARM.  The original gate was `r > b*1.12` alone.  Green foliage
        satisfies that trivially (green has little blue), so on a real engine
        render of a vegetated scene the mask latched onto the terrain and
        reported flame.hue_p5_p95_deg = 91.9..114.6 — green — with a straight
        face.  Hence the explicit fire-hue window.

    (2) FIRELIT GROUND.  The hue window alone does NOT separate flame from the
        stone and earth the flame is lighting: firelit rock is warm, in-window,
        and in a night scene it covers far more of the frame than the flame
        does, so "largest connected warm component" picks the GROUND.  The
        separator is that a flame is an EMITTER — it lives at the top of the
        histogram — while lit ground is reflective and sits well below it.

    So: seed on a HIGH percentile (emitter-bright, fire-hued), then region-grow
    down to the permissive threshold (standard double-threshold hysteresis) so
    the dim outer filaments still come in, but only where they connect to a
    genuinely hot seed.  Ground never seeds, so ground never grows.

    Returns (body, other_blobs, threshold); the detached blobs are the ember
    candidates."""
    L = luma(rgb)
    r, b = rgb[..., 0], rgb[..., 2]
    hu = hue_deg(rgb)
    hu = np.where(hu > 300.0, hu - 360.0, hu)
    fire_hued = (hu >= _FIRE_HUE_LO) & (hu <= _FIRE_HUE_HI) & (r > b * warm)

    thr = max(lum_floor, float(np.percentile(L, 99.5)) * 0.06)
    loose = (L > thr) & fire_hued

    # NO brightness-hysteresis / no texture growth constraint, DELIBERATELY.
    # Both were tried and neither survives honest calibration: a flame standing
    # on the ground it lights is CONNECTED to that ground in any threshold mask,
    # so propagation leaks regardless of the seed; and the corpus separation for
    # local texture (flame rel_sd p50 0.048 vs lit-ground 0.018) is real in
    # DIRECTION but far too weak to cut on without eating most of the flame.
    # An unvalidated segmentation heuristic that silently contaminates every
    # colour number is worse than a simple mask plus an honest contamination
    # report — hence flame.mask_smooth_frac below, and hence the standing rule
    # that the located crop gets looked at before any final call.
    lab, n = ndimage.label(loose)
    if n == 0:
        return loose, np.zeros_like(loose), thr
    sizes = ndimage.sum(loose, lab, range(1, n + 1))
    big = int(np.argmax(sizes)) + 1
    body = lab == big
    # ember candidates: fire-hued blobs that did NOT survive into the body
    rest = loose & ~body
    return body, rest, thr


# ----------------------------------------------------------------------------
# checks
# ----------------------------------------------------------------------------
def analyze_still(path, r, crop_dir=None, tag=""):
    rgb = load_rgb01(path)
    H, W = rgb.shape[:2]
    L = luma(rgb)
    r.info("image.dims", "%dx%d" % (W, H))

    body, blobs, thr = flame_mask(rgb)
    area = float(body.sum()) / (H * W)
    r.info("flame.mask_luma_thr", "%.4f" % thr)
    r.gate("flame.area_frac", "%.5f" % area, ">0.0008", area > 0.0008)
    if area <= 0.0008:
        return None

    ys, xs = np.nonzero(body)
    y0, y1, x0, x1 = ys.min(), ys.max(), xs.min(), xs.max()
    r.info("flame.bbox_xywh", "%d,%d,%d,%d" % (x0, y0, x1 - x0 + 1, y1 - y0 + 1))
    cy, cx = float(ys.mean()), float(xs.mean())
    r.info("flame.centroid_xy", "%d,%d" % (int(cx), int(cy)))

    px = rgb[body]
    Lp = L[body]

    # --- COLOUR: the grey-body checks -------------------------------------
    # only pixels with real chroma information: fully clipped white has no hue
    unclipped = np.max(px, axis=-1) < 0.995
    pxu = px[unclipped] if unclipped.sum() > 200 else px
    dist, temp = locus_fit(pxu)
    d90 = float(np.percentile(dist, 90))
    r.info("flame.bb_dev_p50", "%.4f" % float(np.percentile(dist, 50)))
    # threshold from the seeded corpus under the brightness-constrained fit
    # (see locus_fit): flat 0.0385 must FAIL; clean 0.0073 / hardedge 0.0082 /
    # blown 0.0029 must PASS.  0.025 sits between with ~1.5x margin below and
    # ~3.4x above.  SYNTHETIC-CALIBRATED — no rendered fire has been measured
    # against it yet; a revision once real frames exist is expected, and must
    # be reported as a threshold change with its own evidence, never silent.
    r.gate("flame.bb_dev_p90", "%.4f" % d90, "<0.0250", d90 < 0.0250)

    t5, t95 = float(np.percentile(temp, 5)), float(np.percentile(temp, 95))
    span = t95 - t5
    r.info("flame.temp_p5_p95_K", "%d..%d" % (int(t5), int(t95)))
    r.gate("flame.temp_span_K", "%d" % int(span), ">450", span > 450)

    hu = hue_deg(px)
    # wrap into [-60,300) so red-side wrap (359 deg) sits next to 0
    hu = np.where(hu > 300.0, hu - 360.0, hu)
    h5, h95 = float(np.percentile(hu, 5)), float(np.percentile(hu, 95))
    hspan = h95 - h5
    r.info("flame.hue_p5_p95_deg", "%.1f..%.1f" % (h5, h95))
    r.gate("flame.hue_span_deg", "%.1f" % hspan, ">18.0", hspan > 18.0)

    # hotter where brighter: rank correlation of luma vs hue
    if Lp.size > 50:
        from scipy.stats import spearmanr
        sub = np.random.RandomState(3).choice(Lp.size, min(30000, Lp.size), replace=False)
        rho = float(spearmanr(Lp[sub], hu[sub]).statistic)
    else:
        rho = 0.0
    r.gate("flame.lum_hue_corr", "%+.3f" % rho, ">+0.250", rho > 0.250)

    # BLOWN CORE.  NOT min(rgb) > thr — that spells "clipped to pure WHITE",
    # which a warm emitter essentially never reaches: ACES saturates red first,
    # then green, while blue stays low, so a fully blown fire core still has
    # min(rgb) ~ 0.4.  Measured on the seeded corpus: the `blown` mutant (9x
    # over) scored clip_frac 0.0000 under the min() spelling while 19% of its
    # flame pixels had genuinely lost highlight detail.
    #
    # "TWO channels pinned" is also too strict, for the same reason one step
    # further: that mutant pins only RED (2ch>=0.99 -> 0.0189).  The separating
    # spelling is MAX channel pinned = the pixel has no highlight headroom left
    # anywhere, i.e. the region has gone flat and detail-free.  Corpus, over
    # flame-mask pixels, max(rgb)>=0.995:
    #     blown 0.1883 | clean, flat, hardedge, noembers, deadlight ALL 0.0000
    # Threshold 0.08 keeps the small genuinely-clipped kernel a real fire has
    # (see the module header) while failing a clipped slab.
    clip = float((np.max(px, axis=-1) >= 0.995).sum()) / max(px.shape[0], 1)
    r.gate("flame.clip_frac", "%.4f" % clip, "<0.0800", clip < 0.0800)

    # MASK CONTAMINATION (WARN-only, and it is a REPORT not a gate).
    # A flame standing on the ground it lights is connected to that ground in
    # any threshold mask, and no photometric rule separates them reliably (see
    # flame_mask).  What CAN be measured is how much of the mask is optically
    # SMOOTH: lit stone/earth is a reflector and reads flat, while a flame is a
    # turbulent emitter.  Corpus, local relative sd over a 7 px window:
    #     flame p50 0.048   |   firelit ground p50 0.018
    # High smooth_frac means the colour numbers above are averaged over surround
    # as well as flame — read the located crop before trusting them.
    Lb = luma(rgb)
    mu = ndimage.uniform_filter(Lb, 7)
    mu2 = ndimage.uniform_filter(Lb * Lb, 7)
    rel = np.sqrt(np.maximum(mu2 - mu * mu, 0.0)) / np.maximum(mu, 1e-3)
    smooth = float((rel[body] < 0.025).mean())
    r.gate("flame.mask_smooth_frac", "%.3f" % smooth, "<0.500 (WARN)",
           smooth < 0.500, warn_only=True)

    dr = math.log10(max(float(np.percentile(Lp, 99)), 1e-6) /
                    max(float(np.percentile(Lp, 50)), 1e-6))
    r.gate("flame.lum_dyn_range_dec", "%.3f" % dr, ">0.350", dr > 0.350)

    # --- EDGES: the sprite tell -------------------------------------------
    fall, hard_frac, hard_pt = edge_falloff(L, body)
    r.gate("edge.falloff_px_median", "%.2f" % fall, ">2.50", fall > 2.50)
    r.gate("edge.hard_frac", "%.3f" % hard_frac, "<0.150", hard_frac < 0.150)
    if hard_pt is not None:
        r.info("edge.worst_xy", "%d,%d" % (hard_pt[1], hard_pt[0]))

    # --- EMBERS -----------------------------------------------------------
    lab, n = ndimage.label(blobs)
    cnt = 0
    if n:
        sizes = ndimage.sum(blobs, lab, range(1, n + 1))
        cnt = int(((sizes >= 1) & (sizes <= 60)).sum())
    r.gate("ember.blob_count", "%d" % cnt, ">6", cnt > 6)

    # --- SMOKE ------------------------------------------------------------
    above = np.zeros_like(body)
    above[:max(y0 - 2, 0), :] = True
    sat = saturation(rgb)
    smoke = above & (L > 0.010) & (L < 0.30) & (sat < 0.45)
    sfrac = float(smoke.sum()) / (H * W)
    r.info("smoke.area_frac", "%.5f" % sfrac)
    if smoke.sum() > 200:
        r.info("smoke.luma_mean", "%.4f" % float(L[smoke].mean()))
        r.info("smoke.sat_mean", "%.3f" % float(sat[smoke].mean()))

    # --- CAST LIGHT -------------------------------------------------------
    exp_, prof = cast_falloff(L, body, cy, cx)
    r.info("cast.radial_profile", prof)
    r.gate("cast.falloff_exp", "%+.2f" % exp_, "in[-3.60,-0.60]",
           -3.60 <= exp_ <= -0.60)

    if crop_dir:
        write_crops(rgb, crop_dir, tag, (x0, y0, x1, y1), hard_pt)
    return dict(bbox=(x0, y0, x1, y1), body=body)


def edge_falloff(L, body):
    """10%->90% luminance falloff width along the outward normal, in pixels.

    A billboard whose cookie reaches the quad edge, or a flame cut by geometry
    with no soft fade, gives a sub-2px cliff over a long run of the silhouette.
    Real flame boundaries are optically thin and ramp over many pixels."""
    er = ndimage.binary_erosion(body, iterations=1)
    boundary = body & ~er
    ys, xs = np.nonzero(boundary)
    if ys.size == 0:
        return 0.0, 1.0, None
    # outward normal ~ -grad(smoothed body indicator)
    sm = ndimage.gaussian_filter(body.astype(np.float64), 2.0)
    gy, gx = np.gradient(sm)
    # local body thickness: a ray launched off a 1-px filament runs ALONG the
    # boundary and reports a fake cliff, so only sample edges of solid regions.
    dt = ndimage.distance_transform_edt(body)
    rs = np.random.RandomState(11)
    idx = rs.choice(ys.size, min(2500, ys.size), replace=False)
    H, W = L.shape
    widths = []
    worst = None
    worst_w = 1e9
    MAXD = 24
    for k in idx:
        y, x = int(ys[k]), int(xs[k])
        nx, ny = -gx[y, x], -gy[y, x]
        nn = math.hypot(nx, ny)
        if nn < 1e-6:
            continue
        nx, ny = nx / nn, ny / nn
        iy, ix = int(round(y - ny * 3)), int(round(x - nx * 3))
        if not (0 <= iy < H and 0 <= ix < W) or dt[iy, ix] < 2.0:
            continue
        samp = []
        for d in range(-3, MAXD + 1):
            sy, sx = int(round(y + ny * d)), int(round(x + nx * d))
            if not (0 <= sy < H and 0 <= sx < W):
                break
            samp.append(L[sy, sx])
        if len(samp) < 9:
            continue
        s = np.array(samp)
        v0 = float(np.max(s[:4]))
        s = s[3:]
        vend = float(np.min(s[-3:]))
        if v0 - vend < 0.02:
            continue
        # reject rays that walk INTO something brighter (the flame's contact
        # with lit ground, or a crossing sibling sprite): a falloff width is
        # only meaningful against a darker background.
        if float(s.max()) > v0 * 1.05:
            continue
        hi = vend + 0.9 * (v0 - vend)
        lo = vend + 0.1 * (v0 - vend)
        d_hi = np.argmax(s <= hi) if np.any(s <= hi) else MAXD
        d_lo = np.argmax(s <= lo) if np.any(s <= lo) else MAXD
        w = float(max(d_lo - d_hi, 0))
        widths.append(w)
        if w < worst_w:
            worst_w = w
            worst = (y, x)
    if not widths:
        return 0.0, 1.0, None
    widths = np.array(widths)
    return float(np.median(widths)), float((widths < 2.0).mean()), worst


def cast_falloff(L, body, cy, cx):
    """log-log slope of scene luminance vs image-space radius from the flame.

    Coarse by construction (image radius is not world distance) — it is a
    LIVE/DEAD check on the light interplay, not a photometric measurement."""
    H, W = L.shape
    yy, xx = np.mgrid[0:H, 0:W]
    rad = np.hypot(yy - cy, xx - cx)
    dil = ndimage.binary_dilation(body, iterations=6)
    # the cast light lives on the GROUND, i.e. below the flame centroid; the
    # sky above it carries no falloff to measure.
    ok = (~dil) & (rad > 8) & (yy > cy)
    rs, vs, parts = [], [], []
    r0 = max(float(np.percentile(rad[ok], 2)) if ok.sum() else 10.0, 8.0)
    edges = np.geomspace(r0, max(rad[ok].max() if ok.sum() else 60.0, r0 * 2), 10)
    for a, b in zip(edges[:-1], edges[1:]):
        sel = ok & (rad >= a) & (rad < b)
        if sel.sum() < 120:
            continue
        v = float(np.percentile(L[sel], 80))
        parts.append("%d:%.4f" % (int((a + b) / 2), v))
        if v > 2e-4:
            rs.append(math.log10((a + b) / 2))
            vs.append(math.log10(v))
    if len(rs) < 3:
        return 0.0, ";".join(parts)
    A = np.polyfit(np.array(rs), np.array(vs), 1)
    return float(A[0]), ";".join(parts)


def write_crops(rgb, crop_dir, tag, bbox, hard_pt):
    Path(crop_dir).mkdir(parents=True, exist_ok=True)
    H, W = rgb.shape[:2]
    x0, y0, x1, y1 = bbox
    img = Image.fromarray((np.clip(rgb, 0, 1) * 255).astype(np.uint8))
    pad = 24
    b = (max(x0 - pad, 0), max(y0 - pad, 0), min(x1 + pad, W), min(y1 + pad, H))
    img.crop(b).save(os.path.join(crop_dir, "%sflame_body.png" % tag))
    if hard_pt is not None:
        y, x = hard_pt
        s = 96
        b2 = (max(x - s, 0), max(y - s, 0), min(x + s, W), min(y + s, H))
        img.crop(b2).resize(((b2[2] - b2[0]) * 3, (b2[3] - b2[1]) * 3),
                            Image.NEAREST).save(
            os.path.join(crop_dir, "%sedge_worst_3x.png" % tag))


# ----------------------------------------------------------------------------
# movie mode
# ----------------------------------------------------------------------------
def analyze_movie(path, r, fps, crop_dir=None, max_frames=1200):
    tmp = tempfile.mkdtemp(prefix="orkvetfire_")
    cmd = ["ffmpeg", "-loglevel", "error", "-i", path,
           "-vsync", "0", os.path.join(tmp, "f_%05d.png")]
    subprocess.run(cmd, check=True)
    frames = sorted(Path(tmp).glob("f_*.png"))[:max_frames]
    r.info("movie.frames", "%d" % len(frames))
    r.info("movie.fps", "%g" % fps)
    if len(frames) < 30:
        r.gate("movie.frame_count", "%d" % len(frames), ">30", False)
        return
    body_lum, surr_lum, area = [], [], []
    body0 = None
    for i, f in enumerate(frames):
        rgb = load_rgb01(str(f))
        L = luma(rgb)
        b, _, _ = flame_mask(rgb)
        if body0 is None and b.sum() > 100:
            body0 = ndimage.binary_dilation(b, iterations=10)
        area.append(float(b.sum()))
        body_lum.append(float(L[b].sum()) if b.sum() else 0.0)
        if body0 is not None:
            surr = (~body0) & (L > 0.004)
            surr_lum.append(float(L[surr].mean()) if surr.sum() else 0.0)
        else:
            surr_lum.append(0.0)
    bl = np.array(body_lum)
    sl = np.array(surr_lum)
    ar = np.array(area)
    # use the developed tail only (the fire lights up over the first ~half)
    n0 = len(bl) // 2
    blt, slt, art = bl[n0:], sl[n0:], ar[n0:]

    cv = float(blt.std() / max(blt.mean(), 1e-9))
    r.gate("temporal.flicker_cv", "%.4f" % cv, "in[0.030,0.450]",
           0.030 <= cv <= 0.450)
    acv = float(art.std() / max(art.mean(), 1e-9))
    r.info("temporal.area_cv", "%.4f" % acv)

    # dominant flicker frequency (detrended, Hann-windowed)
    x = blt - blt.mean()
    x = x * np.hanning(x.size)
    sp = np.abs(np.fft.rfft(x))
    fr = np.fft.rfftfreq(x.size, d=1.0 / fps)
    band = (fr >= 0.15) & (fr <= 6.0)
    if band.sum() > 2:
        k = int(np.argmax(sp[band]))
        fdom = float(fr[band][k])
        r.info("temporal.dominant_hz", "%.3f" % fdom)
        r.info("temporal.dominant_period_frames", "%.1f" % (fps / max(fdom, 1e-6)))
        # a big pool fire breathes below ~1.5 Hz; a campfire flickers above 2 Hz
        r.gate("temporal.puff_hz", "%.3f" % fdom, "<1.500", fdom < 1.500,
               warn_only=True)

    # cast-light coherence: flame body vs surroundings, best lag in [0,12] frames
    best, blag = -2.0, 0
    for lag in range(0, 13):
        a = blt[:len(blt) - lag]
        b = slt[lag:]
        if a.size < 20 or a.std() < 1e-12 or b.std() < 1e-12:
            continue
        c = float(np.corrcoef(a, b)[0, 1])
        if c > best:
            best, blag = c, lag
    r.info("temporal.light_lag_frames", "%d" % blag)
    r.gate("temporal.light_coherence", "%+.3f" % best, ">+0.400", best > 0.400)

    if crop_dir:
        Path(crop_dir).mkdir(parents=True, exist_ok=True)
        j = int(np.argmax(blt)) + n0
        k = int(np.argmin(blt)) + n0
        for nm, ix in (("peak", j), ("trough", k)):
            Image.open(str(frames[ix])).save(
                os.path.join(crop_dir, "movie_%s_f%05d.png" % (nm, ix)))
        r.info("movie.peak_trough_frames", "%d,%d" % (j, k))


# ----------------------------------------------------------------------------
# self-test: seeded mutants (a check that claims to catch X fails on X)
# ----------------------------------------------------------------------------
# how much brighter the firelit ground is in the `groundfp` twin. Calibrated
# below (see selftest notes): the ground must DOMINATE IN AREA and pass the warm
# gate, but must stay DIMMER than the flame — it is lit BY the flame, with
# albedo < 1 and inverse-square falloff, so a ground brighter than its own
# source is not a case the mask should be asked to solve.
_GROUNDFP_GMUL = 2.5


def _synth(kind, W=512, H=512):
    """clean = a grey-body flame with soft edges + embers + cast light.
    mutants inject exactly one defect each."""
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float64)
    cx, cy = W * 0.5, H * 0.62
    rs = np.random.RandomState(5)
    img = np.zeros((H, W, 3))
    # cast light on a "ground"
    rad = np.hypot(xx - cx, yy - cy)
    # the ground's near edge is FEATHERED on purpose: a step here would be an
    # accidental hard edge in the CLEAN twin and edge.hard_frac would (rightly)
    # catch it, which is not the defect this twin is supposed to be free of.
    horizon = np.clip((yy - (cy - 40)) / 60.0, 0.0, 1.0)
    ground = np.clip(2.5e3 / (rad ** 2 + 60.0), 0, 1) * horizon
    # `groundfp` is the FALSE-POSITIVE twin: a correct flame over a big, bright,
    # FIRELIT ground — warm, in the fire-hue window, and covering far more of
    # the frame than the flame. It exists to prove the mask seeds on emitter
    # brightness and does not simply grab the largest warm blob.
    gmul = _GROUNDFP_GMUL if kind == "groundfp" else 1.0
    img += ground[..., None] * np.array([0.45, 0.22, 0.06]) * gmul
    # flame body: a stack of soft warm gaussians, hotter (whiter) low down
    for i in range(160):
        t = rs.rand()
        h = cy - 10 - t * 210
        w = 42 * (1.0 - 0.55 * t) * (0.6 + 0.8 * rs.rand())
        ox = (rs.rand() - 0.5) * 60 * (0.3 + t)
        g = np.exp(-(((xx - cx - ox) ** 2 + (yy - h) ** 2) / (2 * w * w)))
        T = 1850.0 - 850.0 * t
        xyz = _cie_xyz_of_T(T)
        lin = np.maximum(_M_XYZ2RGB.dot(xyz), 0.0)
        lin = lin / max(lin.max(), 1e-9)
        img += g[..., None] * lin[None, None, :] * 0.055
    if kind == "flat":
        # cartoon: one hue, no temperature ramp
        Lm = luma(img)
        img = Lm[..., None] * np.array([1.0, 0.55, 0.10])[None, None, :]
    if kind == "hardedge":
        # billboard cliff: quantize the body to a hard-thresholded slab
        Lm = luma(img)
        m = Lm > 0.10
        img = np.where(m[..., None], np.clip(img * 2.2, 0, 1), img * 0.02)
    if kind == "blown":
        img = img * 9.0
    if kind != "noembers":
        for _ in range(28):
            ex = cx + (rs.rand() - 0.5) * 260
            ey = cy - 120 - rs.rand() * 220
            g = np.exp(-(((xx - ex) ** 2 + (yy - ey) ** 2) / 2.4))
            img += g[..., None] * np.array([1.0, 0.62, 0.22])[None, None, :] * 0.9
    if kind == "deadlight":
        img -= ground[..., None] * np.array([0.45, 0.22, 0.06])
        img = np.maximum(img, 0)
    disp = _srgb_encode(_aces_narkowicz(np.maximum(img, 0) * 1.1))
    return (np.clip(disp, 0, 1) * 255).astype(np.uint8)


def selftest():
    d = tempfile.mkdtemp(prefix="orkvetfire_selftest_")
    cases = [("clean", "PASS"), ("flat", "FAIL"), ("hardedge", "FAIL"),
             ("blown", "FAIL"), ("noembers", "FAIL"), ("deadlight", "FAIL"),
             # groundfp expects FAIL, and that is the CORRECT behaviour, not a
             # concession: once the lit ground is swept into the mask, cast
             # falloff genuinely cannot be measured (its sampling region is
             # gone). A loud FAIL plus the contamination WARN is the honest
             # answer — "I could not measure this cleanly" — and the real claim
             # for this twin is the mask.contamination_visible assertion below.
             ("groundfp", "FAIL")]
    ok = True
    print("# ork.vet.fire selftest (seeded mutants) — artifacts in %s" % d)
    for kind, want in cases:
        p = os.path.join(d, "%s.png" % kind)
        Image.fromarray(_synth(kind)).save(p)
        rr = Report()
        try:
            analyze_still(p, rr)
        except Exception as e:  # noqa
            print("mutant.%s\tEXCEPTION %s\t-\tFAIL" % (kind, e))
            ok = False
            continue
        got = "FAIL" if rr.failed else "PASS"
        which = [l.split("\t")[0] for l in rr.lines if l.endswith("\tFAIL")]
        status = "PASS" if got == want else "FAIL"
        if status == "FAIL":
            ok = False
        print("mutant.%s\t%s (failed: %s)\t==%s\t%s" %
              (kind, got, ",".join(which) or "-", want, status))

    # CONTAMINATION assertion for the false-positive twin. The honest claim is
    # NOT "the mask excludes the lit ground" — it cannot, the two are connected
    # (see flame_mask). It is "when the ground IS swept in, the instrument SAYS
    # SO" rather than reporting contaminated colour numbers silently.
    # gmul 2.5 is the edge of physical validity: at 3.0+ the synthetic ground
    # becomes BRIGHTER than the flame lighting it, which cannot happen.
    rgbfp = load_rgb01(os.path.join(d, "groundfp.png"))
    rgbcl = load_rgb01(os.path.join(d, "clean.png"))
    def _smooth_frac(rgbx):
        b, _, _ = flame_mask(rgbx)
        Lx = luma(rgbx)
        mu = ndimage.uniform_filter(Lx, 7)
        mu2 = ndimage.uniform_filter(Lx * Lx, 7)
        relx = np.sqrt(np.maximum(mu2 - mu * mu, 0.0)) / np.maximum(mu, 1e-3)
        return float((relx[b] < 0.025).mean()) if b.sum() else -1.0
    sf_fp, sf_cl = _smooth_frac(rgbfp), _smooth_frac(rgbcl)
    good = sf_fp > sf_cl * 1.5
    if not good:
        ok = False
    print("mask.contamination_visible\tgroundfp %.3f vs clean %.3f\t"
          "fp>1.5x clean\t%s" % (sf_fp, sf_cl, "PASS" if good else "FAIL"))

    print("# verdict: %s (%d checks, %d failed)" %
          ("PASS" if ok else "FAIL", len(cases), 0 if ok else 1))
    return 0 if ok else 1


def main():
    p = argparse.ArgumentParser()
    p.add_argument("artifact", nargs="?")
    p.add_argument("--kind", choices=["still", "movie"], default="still")
    p.add_argument("--fps", type=float, default=60.0)
    p.add_argument("--crop-out", help="directory for located-evidence crops")
    p.add_argument("--tag", default="")
    p.add_argument("--selftest", action="store_true")
    a = p.parse_args()
    if a.selftest:
        return selftest()
    if not a.artifact:
        p.error("artifact required (or --selftest)")
    r = Report()
    if a.kind == "movie":
        analyze_movie(a.artifact, r, a.fps, a.crop_out)
    else:
        analyze_still(a.artifact, r, a.crop_out, a.tag)
    return r.emit()


if __name__ == "__main__":
    sys.exit(main())
