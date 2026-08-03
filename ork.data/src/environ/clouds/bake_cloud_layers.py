#!/usr/bin/env python3
###############################################################################
# bake_cloud_layers.py — SKYLIGHT lane B4: procedural cloud-coverage textures
#
# Authors the layered cloud textures consumed by the FWD_SKYBOX_PROC cloud
# shells (see pbrtools.i2 "CLOUD LAYER SEAM (slice B3)") and, later, by the
# ground cloud-shadow projection.  Pure numpy — deterministic, seeded, fully
# re-tunable (this is the interim route while the hypersyn ptex2d family is
# unbuilt; the param blocks below are shaped so a future ptex2d graph can
# reproduce each layer 1:1).
#
# CHANNEL SEMANTICS — see CHANNELS.md beside this file (the binding contract).
#   R = coverage rank field, exactly histogram-equalized: a coverage-threshold
#       knob t yields sky-coverage fraction == (1 - t).  Tileable, period 1.
#   G = core-ness / optical-thickness proxy (0 = feathered edge, 1 = densest
#       core), for sun-transmittance tinting + base darkening.
#   B = high-frequency erosion detail, for threshold-edge crinkling and
#       silver-lining modulation.
#   A = 255 (reserved).
#
# Seamless tiling is BY CONSTRUCTION: all noise lattices are periodic (modulo
# lattice indexing), all domain warps are themselves periodic, all blurs are
# FFT (circular).  The bake also numerically verifies wrap-edge continuity.
#
# Usage (staging python has numpy+PIL):
#   ~/.staging-jul24/pyvenv/bin/python bake_cloud_layers.py \
#       --outdir <textures dir> --review <contact sheet dir> [--seed 7]
###############################################################################

import argparse
import os
import numpy as np

###############################################################################
# tunable parameter blocks (A8 spirit: every look decision is a named value
# here, nothing constant-folded into the synthesis code below)
###############################################################################

PARAMS = {
    "cirrus": {
        # jul25 owner-gauge revision ("long streaky shapes unnatural"): the
        # jul24 bake (smear 48/0.7, fiber_pow 1.1, band_gamma 1.3) thresholded
        # into fat regular ropes. Now: shorter+softer smear, THINNER level sets
        # (fiber_pow), much more uncinus curl so nothing stays straight, and a
        # sparser duty cycle (band/patch gammas) — delicate tufts, big gaps.
        "res": 1024,
        "seed": 710,
        # patchiness: where in the sky the streak bundles live
        "patch_fx": 3, "patch_fy": 2, "patch_oct": 3, "patch_gamma": 2.3,
        # fiber source: fine isotropic ridged noise, smeared into strands by a
        # directional (LIC-style) periodic blur along the wind axis U
        "fiber_fx": 48, "fiber_fy": 48, "fiber_oct": 2,
        "smear_sigma_u": 28.0, "smear_sigma_v": 1.1,     # px at res 1024
        "fiber_pow": 2.0,      # level-set thinness of individual fibers
        # streak-bundle banding across the wind axis
        "band_fx": 3, "band_fy": 10, "band_oct": 2, "band_gamma": 1.9,
        # post-smear wavy warp (uncinus curl): bends the strands
        "warp_fx": 3, "warp_fy": 3, "warp_oct": 3,
        "warp_u": 0.11, "warp_v": 0.19,
        # G channel blur (px at res=1024) ; B detail lattice
        "g_sigma": 18.0,
        "b_fx": 18, "b_fy": 140, "b_oct": 3,
    },
    "altocumulus": {
        "res": 1024,
        "seed": 3300,
        # cellular puffs: one cloudlet per Worley cell
        "cell_fx": 22, "cell_fy": 22, "cell_gamma": 1.6,
        # second cellular octave breaks cell-size uniformity
        "cell2_fx": 44, "cell2_fy": 44, "cell2_amt": 0.30,
        # de-grid warp
        "warp_fx": 6, "warp_fy": 6, "warp_oct": 3, "warp_amt": 0.030,
        # undulatus rows (mackerel banding), gentle
        "row_count": 7, "row_warp": 0.35, "row_amt": 0.35, "row_angle_deg": 18.0,
        # patch grouping so the deck breaks into fields of cloudlets
        "patch_fx": 3, "patch_fy": 3, "patch_oct": 3, "patch_gamma": 1.2,
        "g_sigma": 10.0,
        "b_fx": 64, "b_fy": 64, "b_oct": 2,
    },
    "cumulus": {
        "res": 1024,          # 2048 variant baked as well for owner judgment
        "seed": 9021,
        # broken-mass base field (fbm + rounded Worley blob masses)
        "base_fx": 3, "base_fy": 3, "base_oct": 2, "base_gamma": 1.35,
        "mass_fx": 4, "mass_amt": 0.50,
        # two-stage Quilez domain warp -> billowy lobes
        "warp1_fx": 2, "warp1_fy": 2, "warp1_oct": 3, "warp1_amt": 0.22,
        "warp2_fx": 6, "warp2_fy": 6, "warp2_oct": 3, "warp2_amt": 0.10,
        # cauliflower puffs: inverted-F1 Worley fBm (2D Perlin-Worley)
        "puff_fx": 7, "puff_oct": 3, "puff_lac": 2.4, "puff_amt": 0.85,
        "puff_gamma": 1.0,
        "g_sigma": 14.0,
        "b_fx": 34, "b_fy": 34, "b_oct": 3,
    },
}

# threshold-preview strip settings (contact sheet only, not shipped data)
SWEEP_THRESHOLDS = (0.2, 0.4, 0.6, 0.8)
SWEEP_SOFT = 0.035         # smoothstep width of the preview remap
SWEEP_ERODE = 0.10         # how much B erodes the silhouette in the preview

###############################################################################
# periodic noise kernels (all vectorized, all period-1 in UV by modulo lattice)
###############################################################################

def _fade(t):
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0)

def perlin(U, V, fx, fy, seed):
    """Periodic gradient noise at arbitrary UV arrays; period 1 in both axes."""
    rng = np.random.default_rng(seed)
    ang = rng.uniform(0.0, 2.0 * np.pi, (fy, fx))
    gx, gy = np.cos(ang), np.sin(ang)
    X = (U % 1.0) * fx
    Y = (V % 1.0) * fy
    xi = np.floor(X).astype(np.int64)
    yi = np.floor(Y).astype(np.int64)
    xf = X - xi
    yf = Y - yi
    x0 = xi % fx; x1 = (xi + 1) % fx
    y0 = yi % fy; y1 = (yi + 1) % fy
    n00 = gx[y0, x0] * xf       + gy[y0, x0] * yf
    n10 = gx[y0, x1] * (xf - 1) + gy[y0, x1] * yf
    n01 = gx[y1, x0] * xf       + gy[y1, x0] * (yf - 1)
    n11 = gx[y1, x1] * (xf - 1) + gy[y1, x1] * (yf - 1)
    tx, ty = _fade(xf), _fade(yf)
    a = n00 + tx * (n10 - n00)
    b = n01 + tx * (n11 - n01)
    return a + ty * (b - a)                     # approx [-0.75, 0.75]

def fbm(U, V, fx, fy, octaves, seed, gain=0.5, lac=2, ridged=False):
    """Periodic fBm; integer lacunarity keeps every octave period-1."""
    out = np.zeros_like(U)
    amp, norm = 1.0, 0.0
    ffx, ffy = fx, fy
    for o in range(octaves):
        n = perlin(U, V, max(1, int(ffx)), max(1, int(ffy)), seed + 131 * o)
        if ridged:
            n = 1.0 - np.abs(n) * 2.0
        out += amp * n
        norm += amp
        amp *= gain
        ffx *= lac
        ffy *= lac
    return out / norm

def worley_f1(U, V, fx, fy, seed, jitter=0.85):
    """Periodic Worley F1 (nearest feature point), distance in cell units."""
    rng = np.random.default_rng(seed)
    jx = rng.uniform(0.5 - jitter / 2, 0.5 + jitter / 2, (fy, fx))
    jy = rng.uniform(0.5 - jitter / 2, 0.5 + jitter / 2, (fy, fx))
    X = (U % 1.0) * fx
    Y = (V % 1.0) * fy
    cx = np.floor(X).astype(np.int64)
    cy = np.floor(Y).astype(np.int64)
    f1 = np.full(U.shape, 1e9)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            ix = (cx + dx) % fx
            iy = (cy + dy) % fy
            px = (cx + dx) + jx[iy, ix]
            py = (cy + dy) + jy[iy, ix]
            d2 = (X - px) ** 2 + (Y - py) ** 2
            f1 = np.minimum(f1, d2)
    return np.sqrt(f1)

def worley_fbm(U, V, fx, octaves, seed, gain=0.5, lac=2.0):
    """Inverted-F1 Worley fBm: puffy bumps at multiple scales, period 1."""
    out = np.zeros_like(U)
    amp, norm, f = 1.0, 0.0, fx
    for o in range(octaves):
        fi = max(1, int(round(f)))
        w = 1.0 - np.clip(worley_f1(U, V, fi, fi, seed + 977 * o), 0.0, 1.0)
        out += amp * w
        norm += amp
        amp *= gain
        f *= lac
    return out / norm

###############################################################################
# field utilities
###############################################################################

def uvgrid(res):
    u = (np.arange(res) + 0.5) / res
    return np.meshgrid(u, u)

def norm01(a):
    lo, hi = float(a.min()), float(a.max())
    return (a - lo) / max(hi - lo, 1e-9)

def equalize(a):
    """Exact histogram equalization (rank remap). Preserves every level set,
    makes the marginal distribution uniform -> a coverage threshold t covers
    exactly (1-t) of the tile. Inputs must be tie-free (continuous fields)."""
    flat = a.ravel()
    order = np.argsort(flat, kind="stable")
    ranks = np.empty(flat.size, dtype=np.float64)
    ranks[order] = np.arange(flat.size, dtype=np.float64) / (flat.size - 1)
    return ranks.reshape(a.shape)

def pblur(a, sigma_px, sigma_py=None):
    """Periodic (circular) gaussian blur via FFT — cannot break tiling.
    Anisotropic when sigma_py differs (used for LIC-style directional smear)."""
    if sigma_py is None:
        sigma_py = sigma_px
    h, w = a.shape
    ky = np.fft.fftfreq(h)[:, None]
    kx = np.fft.rfftfreq(w)[None, :]
    g = np.exp(-2.0 * (np.pi ** 2) *
               ((sigma_px ** 2) * (kx ** 2) + (sigma_py ** 2) * (ky ** 2)))
    return np.fft.irfft2(np.fft.rfft2(a) * g, s=a.shape)

def sample_periodic(img, U, V):
    """Bilinear sample of a discrete field at UV arrays, wrapping (period 1)."""
    h, w = img.shape
    X = (U % 1.0) * w - 0.5
    Y = (V % 1.0) * h - 0.5
    x0 = np.floor(X).astype(np.int64)
    y0 = np.floor(Y).astype(np.int64)
    xf = X - x0
    yf = Y - y0
    x0 %= w; y0 %= h
    x1 = (x0 + 1) % w
    y1 = (y0 + 1) % h
    a = img[y0, x0] * (1 - xf) + img[y0, x1] * xf
    b = img[y1, x0] * (1 - xf) + img[y1, x1] * xf
    return a * (1 - yf) + b * yf

def seam_metric(a):
    """Wrap-edge continuity: mean |edge-pair diff| vs mean interior-pair diff.
    Ratio ~1.0 == seamless; >>1 == visible seam."""
    ex = np.abs(a[:, 0] - a[:, -1]).mean()
    ey = np.abs(a[0, :] - a[-1, :]).mean()
    ix = np.abs(np.diff(a, axis=1)).mean()
    iy = np.abs(np.diff(a, axis=0)).mean()
    return ex / max(ix, 1e-9), ey / max(iy, 1e-9)

###############################################################################
# layer synthesizers — each returns (R, G, B) float fields in [0,1]
###############################################################################

def synth_cirrus(p):
    res = p["res"]
    U, V = uvgrid(res)
    s = p["seed"]
    scale = res / 1024.0        # keep px-unit params resolution-independent

    # fiber strands: fine isotropic ridged noise smeared along the wind axis
    # (cheap line-integral-convolution) -> long coherent filaments
    fine = fbm(U, V, p["fiber_fx"], p["fiber_fy"], p["fiber_oct"], s + 1,
               ridged=True)
    fiber = pblur(fine, p["smear_sigma_u"] * scale, p["smear_sigma_v"] * scale)
    fiber = norm01(fiber)

    # bend the strands: wavy periodic re-warp of the smeared field (uncinus)
    wu = fbm(U, V, p["warp_fx"], p["warp_fy"], p["warp_oct"], s + 2)
    wv = fbm(U, V, p["warp_fx"], p["warp_fy"], p["warp_oct"], s + 3)
    fiber = sample_periodic(fiber, U + p["warp_u"] * wu, V + p["warp_v"] * wv)

    # streak bundles across the wind axis + large-scale patch gating
    band = norm01(fbm(U + p["warp_u"] * wu, V + p["warp_v"] * wv,
                      p["band_fx"], p["band_fy"], p["band_oct"], s + 4))
    patch = norm01(fbm(U, V, p["patch_fx"], p["patch_fy"], p["patch_oct"], s + 5))
    raw = (patch ** p["patch_gamma"]) * (band ** p["band_gamma"]) \
        * (norm01(fiber) ** p.get("fiber_pow", 1.1)) + 1e-4 * norm01(fiber)

    R = equalize(raw)
    G = norm01(pblur(R, p["g_sigma"] * scale))
    B = norm01(fbm(U, V, p["b_fx"], p["b_fy"], p["b_oct"], s + 7, ridged=True))
    return R, G, B

def synth_altocumulus(p):
    res = p["res"]
    U, V = uvgrid(res)
    s = p["seed"]

    # de-grid warp
    wu = fbm(U, V, p["warp_fx"], p["warp_fy"], p["warp_oct"], s + 1)
    wv = fbm(U, V, p["warp_fx"], p["warp_fy"], p["warp_oct"], s + 2)
    Uw = U + p["warp_amt"] * wu
    Vw = V + p["warp_amt"] * wv

    # cloudlet puffs: inverted F1, two cell scales
    c1 = 1.0 - np.clip(worley_f1(Uw, Vw, p["cell_fx"], p["cell_fy"], s + 3), 0, 1)
    c2 = 1.0 - np.clip(worley_f1(Uw, Vw, p["cell2_fx"], p["cell2_fy"], s + 4), 0, 1)
    cells = norm01(c1 * (1.0 - p["cell2_amt"]) + c2 * p["cell2_amt"]) ** p["cell_gamma"]

    # undulatus rows: rotated soft banding, warped so rows waver
    ang = np.deg2rad(p["row_angle_deg"])
    # periodic rotation axis: keep integer wave counts along both axes
    ku = int(round(p["row_count"] * np.sin(ang)))
    kv = int(round(p["row_count"] * np.cos(ang)))
    rw = fbm(U, V, 3, 3, 2, s + 5)
    rows = 0.5 + 0.5 * np.sin(2.0 * np.pi * (ku * U + kv * V + p["row_warp"] * rw))
    rowmix = (1.0 - p["row_amt"]) + p["row_amt"] * rows

    # patch grouping
    patch = norm01(fbm(U, V, p["patch_fx"], p["patch_fy"], p["patch_oct"], s + 6))
    raw = cells * rowmix * (patch ** p["patch_gamma"]) + 1e-4 * cells

    R = equalize(raw)
    G = norm01(pblur(R, p["g_sigma"]))
    B = norm01(worley_fbm(U, V, p["b_fx"], p["b_oct"], s + 7))
    return R, G, B

def synth_cumulus(p):
    res = p["res"]
    U, V = uvgrid(res)
    s = p["seed"]

    # two-stage domain warp (IQ): q = fbm(p) ; r = fbm(p + a*q) ; base = fbm(p + b*r)
    q_u = fbm(U, V, p["warp1_fx"], p["warp1_fy"], p["warp1_oct"], s + 1)
    q_v = fbm(U, V, p["warp1_fx"], p["warp1_fy"], p["warp1_oct"], s + 2)
    r_u = fbm(U + p["warp1_amt"] * q_u, V + p["warp1_amt"] * q_v,
              p["warp2_fx"], p["warp2_fy"], p["warp2_oct"], s + 3)
    r_v = fbm(U + p["warp1_amt"] * q_u, V + p["warp1_amt"] * q_v,
              p["warp2_fx"], p["warp2_fy"], p["warp2_oct"], s + 4)
    Uw = U + p["warp2_amt"] * r_u
    Vw = V + p["warp2_amt"] * r_v

    bfbm = norm01(fbm(Uw, Vw, p["base_fx"], p["base_fy"], p["base_oct"], s + 5))
    # rounded blob masses so mid coverage reads as separate cumulus, not continents
    mass = 1.0 - np.clip(worley_f1(Uw, Vw, p["mass_fx"], p["mass_fy"]
                                   if "mass_fy" in p else p["mass_fx"],
                                   s + 8, jitter=1.0), 0.0, 1.0)
    base = norm01(bfbm * (1.0 - p["mass_amt"]) + norm01(mass) * p["mass_amt"])
    base = base ** p["base_gamma"]

    # cauliflower puffs riding the warped domain (2D Perlin-Worley)
    puff = norm01(worley_fbm(Uw, Vw, p["puff_fx"], p["puff_oct"], s + 6,
                             lac=p["puff_lac"])) ** p["puff_gamma"]

    # additive silhouette erosion: puffs push the coverage boundary in/out
    # (clamp-free so the equalization rank remap stays tie-free)
    raw = base + p["puff_amt"] * (puff - 0.5)

    R = equalize(raw)
    G = norm01(pblur(R, p["g_sigma"]))
    B = norm01(worley_fbm(U, V, p["b_fx"], p["b_oct"], s + 7))
    return R, G, B

SYNTHS = {
    "cirrus": synth_cirrus,
    "altocumulus": synth_altocumulus,
    "cumulus": synth_cumulus,
}

###############################################################################
# bake + contact sheet
###############################################################################

def to_rgba8(R, G, B):
    """jul25 16-BIT RANK PACKING: R carries the coverage rank's HIGH byte, A its
    LOW byte (was reserved=255). v = (256*R + A)/257 in shader — LINEAR in both
    channels, so bilinear filtering reconstructs the 16-bit field EXACTLY (the
    8-bit terracing the owner flagged came from thresholding a 1/255-stepped
    rank). Old consumers reading R alone still get the top 8 bits."""
    def q(a):
        return np.clip(a * 255.0 + 0.5, 0, 255).astype(np.uint8)
    r16 = np.clip(R * 65535.0 + 0.5, 0, 65535).astype(np.uint32)
    hi  = (r16 >> 8).astype(np.uint8)
    lo  = (r16 & 255).astype(np.uint8)
    return np.dstack([hi, q(G), q(B), lo])

def threshold_preview(R, G, B, t, soft=SWEEP_SOFT, erode=SWEEP_ERODE):
    """Approximation of the shader's coverage remap, for owner judgment only:
    alpha = smoothstep(t, t+soft, R - erode*B*(edge proximity)); shaded by G."""
    edge = 1.0 - np.clip((R - t) / max(soft * 2.0, 1e-6), 0.0, 1.0)
    field = R - erode * B * edge
    x = np.clip((field - t) / max(soft, 1e-6), 0.0, 1.0)
    alpha = x * x * (3.0 - 2.0 * x)
    # simple lit-cloud shading: cores brighter tops, G darkens toward "base"
    shade = 0.72 + 0.28 * (1.0 - G * 0.85)
    sky = np.array([0.36, 0.57, 0.85])
    cloud = shade[..., None] * np.array([1.0, 1.0, 1.0])
    img = sky * (1.0 - alpha[..., None]) + cloud * alpha[..., None]
    return np.clip(img * 255.0 + 0.5, 0, 255).astype(np.uint8)

def build_contact_sheet(name, R, G, B, outpath):
    from PIL import Image, ImageDraw

    res = R.shape[0]
    pad, label_h = 16, 26
    panel = 512

    def gray_img(a):
        im = Image.fromarray(np.clip(a * 255 + 0.5, 0, 255).astype(np.uint8), "L")
        return im.convert("RGB").resize((panel, panel), Image.LANCZOS)

    # panel 1: R coverage full view
    p1 = gray_img(R)
    # panel 2: 2x2 tiled, center-cropped so the wrap seam is the center cross
    tiled = np.tile(R, (2, 2))
    c0 = res // 2
    crop = tiled[c0:c0 + res, c0:c0 + res]
    p2 = gray_img(crop)
    # panel 3: channels composite (R=cov, G=core, B=detail)
    p3 = Image.fromarray(to_rgba8(R, G, B)[..., :3], "RGB").resize(
        (panel, panel), Image.LANCZOS)
    # threshold sweep strip
    sweeps = [Image.fromarray(threshold_preview(R, G, B, t), "RGB").resize(
        (panel // 2, panel // 2), Image.LANCZOS) for t in SWEEP_THRESHOLDS]

    W = pad + 3 * (panel + pad)
    H = pad + label_h + panel + pad + label_h + panel // 2 + pad
    sheet = Image.new("RGB", (W, H), (24, 24, 28))
    d = ImageDraw.Draw(sheet)
    labels = [f"{name}  R=coverage (equalized)",
              "2x2 tiled, seam = center cross",
              "RGB composite (R cov / G core / B detail)"]
    x = pad
    for im, lab in zip((p1, p2, p3), labels):
        d.text((x, pad), lab, fill=(230, 230, 230))
        sheet.paste(im, (x, pad + label_h))
        x += panel + pad
    y2 = pad + label_h + panel + pad
    d.text((pad, y2), "coverage-threshold sweep  t = "
           + " / ".join(str(t) for t in SWEEP_THRESHOLDS)
           + "   (preview remap: smoothstep + B-channel edge erosion, shaded by G)",
           fill=(230, 230, 230))
    x = pad
    for im in sweeps:
        sheet.paste(im, (x, y2 + label_h))
        x += panel // 2 + pad
    sheet.save(outpath)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--outdir", required=True, help="baked-texture output dir")
    ap.add_argument("--review", default=None, help="contact-sheet output dir")
    ap.add_argument("--layers", default="cirrus,altocumulus,cumulus")
    ap.add_argument("--cumulus2048", action="store_true",
                    help="also bake the cumulus layer at 2048^2")
    args = ap.parse_args()

    from PIL import Image
    os.makedirs(args.outdir, exist_ok=True)
    if args.review:
        os.makedirs(args.review, exist_ok=True)

    jobs = []
    for name in args.layers.split(","):
        jobs.append((name, dict(PARAMS[name])))
    if args.cumulus2048:
        p2k = dict(PARAMS["cumulus"])
        p2k["res"] = 2048
        p2k["g_sigma"] *= 2.0   # keep G blur constant in UV space
        jobs.append(("cumulus", p2k))

    for name, p in jobs:
        R, G, B = SYNTHS[name](p)
        sx, sy = seam_metric(R)
        res = p["res"]
        tag = f"clouds_{name}_{res}"
        png = os.path.join(args.outdir, tag + ".png")
        Image.fromarray(to_rgba8(R, G, B), "RGBA").save(png)
        hist, _ = np.histogram(R, bins=10, range=(0, 1))
        print(f"[{tag}] seam-ratio x={sx:.3f} y={sy:.3f} "
              f"(1.0 == perfectly continuous)  R-histogram(10 bins)={hist.tolist()}")
        print(f"[{tag}] wrote {png}")
        if args.review:
            sheet = os.path.join(args.review, f"contact_{tag}.png")
            build_contact_sheet(f"{name} {res}", R, G, B, sheet)
            print(f"[{tag}] contact sheet {sheet}")

if __name__ == "__main__":
    main()
