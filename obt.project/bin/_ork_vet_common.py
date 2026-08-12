# ork.vet.* shared library (imported by the instruments; not a tool itself).
# Owns the ONE porcelain contract so every instrument speaks it identically:
#     <check>\t<value>\t<threshold>\t<PASS|FAIL|WARN|INFO>
#     # verdict: PASS|FAIL (<n> checks, <k> failed)
#     # <extra footer lines...>
# exit code follows the verdict (0 clean, 1 any FAIL). Stats are INFO only
# (necessary-not-sufficient); WARN is advisory and does NOT gate the verdict.
import os
import numpy as np

INFO, PASS, FAIL, WARN = 'INFO', 'PASS', 'FAIL', 'WARN'


class Report:
    """Accumulates checks + footer lines, emits the shared porcelain, returns fail count."""

    def __init__(self):
        self.checks = []   # (name, value_str, threshold_str, status)
        self.footers = []  # trailing '# ...' lines printed after the verdict

    def info(self, name, value):
        self.checks.append((name, str(value), '-', INFO))

    def gate(self, name, value_str, threshold_str, ok, warn_only=False):
        """Record a judged check. ok True -> PASS; False -> FAIL (or WARN if warn_only)."""
        status = PASS if ok else (WARN if warn_only else FAIL)
        self.checks.append((name, str(value_str), str(threshold_str), status))
        return status

    def footer(self, line):
        self.footers.append(line if line.startswith('#') else '# ' + line)

    def emit(self):
        for name, val, thr, st in self.checks:
            print(f"{name}\t{val}\t{thr}\t{st}")
        n_fail = sum(1 for c in self.checks if c[3] == FAIL)
        n_judged = sum(1 for c in self.checks if c[3] in (PASS, FAIL, WARN))
        print(f"# verdict: {'FAIL' if n_fail else 'PASS'} ({n_judged} checks, {n_fail} failed)")
        for f in self.footers:
            print(f)
        return n_fail


# ---------------------------------------------------------------- image IO ---
def _load_exr(path):
    """EXR reader -- optional dep; fails LOUDLY with the exact missing module."""
    try:
        import imageio.v3 as iio
        return np.asarray(iio.imread(path)).astype(np.float64)
    except ModuleNotFoundError:
        pass
    try:
        import OpenEXR
        import Imath
        f = OpenEXR.InputFile(str(path))
        dw = f.header()['dataWindow']
        w = dw.max.x - dw.min.x + 1
        h = dw.max.y - dw.min.y + 1
        pt = Imath.PixelType(Imath.PixelType.FLOAT)
        chans = f.header()['channels'].keys()
        pick = [c for c in ('R', 'G', 'B') if c in chans] or list(chans)[:1]
        planes = [np.frombuffer(f.channel(c, pt), dtype=np.float32).reshape(h, w) for c in pick]
        return np.mean(planes, axis=0).astype(np.float64)
    except ModuleNotFoundError:
        pass
    raise RuntimeError(
        f"cannot read EXR '{path}': no EXR backend installed "
        "(need 'imageio' with the freeimage/openexr plugin, or 'OpenEXR'+'Imath'). "
        "Report this missing dependency; do not install into a shared staging.")


def load_gray(path):
    """Load PNG/EXR as normalized float64 grayscale ~0..1 (16-bit and EXR aware)."""
    ext = os.path.splitext(str(path))[1].lower()
    if ext == '.exr':
        a = _load_exr(path)
        if a.ndim == 3:
            a = a[..., :3].mean(axis=2)
        peak = float(a.max())
        return a / peak if peak > 1.0 else a
    from PIL import Image
    a = np.asarray(Image.open(path)).astype(np.float64)
    if a.ndim == 3:
        a = a[..., :3].mean(axis=2)
    peak = 65535.0 if a.max() > 255 else 255.0
    return a / peak


def load_rgb(path):
    """Load PNG/EXR as HxWx3 float64 ~0..1 (16-bit and EXR aware; grayscale expands).

    Renders need the chroma channels (load_gray discards them -> the instrument
    was blind to a color cast). EXR HDR is peak-normalized like load_gray; the
    render chroma/speckle thresholds are calibrated on LDR captures.
    """
    ext = os.path.splitext(str(path))[1].lower()
    if ext == '.exr':
        try:
            import imageio.v3 as iio
            a = np.asarray(iio.imread(path)).astype(np.float64)
        except ModuleNotFoundError:
            a = _load_exr(path)  # gray fallback -> expanded below
        if a.ndim == 2:
            a = np.stack([a] * 3, axis=-1)
        a = a[..., :3]
        peak = float(a.max())
        return a / peak if peak > 1.0 else a
    from PIL import Image
    a = np.asarray(Image.open(path).convert('RGB')).astype(np.float64)
    peak = 65535.0 if a.max() > 255 else 255.0
    return a / peak


def luma(rgb):
    """Rec.601 luminance of an RGB float image."""
    return rgb @ np.array([0.299, 0.587, 0.114])


# ------------------------------------------------------------ image metrics --
def fft_highband_ratio(a):
    """Fraction of AC spectral energy above 0.25 Nyquist -- the speckle detector.

    Speckle is a SPECTRAL question (feedback_speckle_spectral), NOT min/max/avg.
    """
    f = np.fft.fftshift(np.abs(np.fft.fft2(a - a.mean())) ** 2)
    h, w = f.shape
    yy, xx = np.mgrid[0:h, 0:w]
    r = np.hypot((yy - h / 2) / (h / 2), (xx - w / 2) / (w / 2))
    total = f.sum()
    return float(f[r > 0.25].sum() / total) if total > 0 else 0.0


def walk_metrics(a, n_walks=64):
    """Straight-line walks -> (median_slope, jolt_p99, reversal_rate).

    Heightfield plausibility is a WALK question (feedback_terrain_spikiness_walk):
    per-step reversal rate + jolt (|2nd deriv| p99) + median slope, NOT global stats.
    """
    h, w = a.shape
    slopes, jolts, revs = [], [], []
    rows = np.linspace(0, h - 1, n_walks).astype(int)
    cols = np.linspace(0, w - 1, n_walks).astype(int)
    for line in [a[r, :] for r in rows] + [a[:, c] for c in cols]:
        d1 = np.diff(line)
        d2 = np.diff(d1)
        slopes.append(np.median(np.abs(d1)))
        jolts.append(np.percentile(np.abs(d2), 99))
        sign = np.sign(d1[np.abs(d1) > 1e-7])
        revs.append(float((np.diff(sign) != 0).mean()) if len(sign) > 1 else 0.0)
    return float(np.median(slopes)), float(np.median(jolts)), float(np.median(revs))


def spike_max(a):
    """Largest single-texel deviation from the 4-neighbor mean (isolated spikes)."""
    nb = (np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 4.0
    return float(np.abs(a - nb)[1:-1, 1:-1].max())


def ssim_map(a, b, win=8):
    """Windowed SSIM, pure numpy (block-mean variant) -> per-block map."""
    h, w = a.shape
    h2, w2 = h // win * win, w // win * win

    def blocks(x):
        return x[:h2, :w2].reshape(h2 // win, win, w2 // win, win).transpose(0, 2, 1, 3)

    A, B = blocks(a), blocks(b)
    mu_a, mu_b = A.mean((2, 3)), B.mean((2, 3))
    va, vb = A.var((2, 3)), B.var((2, 3))
    cov = (A * B).mean((2, 3)) - mu_a * mu_b
    c1, c2 = 0.01 ** 2, 0.03 ** 2
    s = ((2 * mu_a * mu_b + c1) * (2 * cov + c2)) / ((mu_a ** 2 + mu_b ** 2 + c1) * (va + vb + c2))
    return s


def degenerate_frame(a, min_range, min_stddev):
    """(dynamic_range, stddev, is_degenerate) -- all-black / near-constant detector.

    Motivated by a real incident: an all-black settle-race snapshot passed a
    byte-identity gate. A frame with no dynamic range or ~zero variance is a
    degenerate capture and must FAIL loudly, never slip through as 'unchanged'.
    """
    rng = float(a.max() - a.min())
    std = float(a.std())
    return rng, std, (rng <= min_range or std <= min_stddev)


# ---------------------------------------------------------- render metrics ---
# These separate legitimate render content (sun glints, baked material detail,
# warm golden-hour grading) from genuine defects (hot pixels / firefly fields,
# render/compute speckle, off-locus color casts). Grayscale-blind checks
# (spike.max_isolated, spectral.highband_ratio) false-FAIL renders because a
# single bright content pixel maxes the spike, broadband material detail reads
# as speckle, and the chroma axis is discarded entirely. Each metric below
# keys on a DISCRIMINATING feature, not a re-tuned threshold.

def _nbmax8(x):
    """Per-pixel max of the 8 neighbours (wrap-padded; interior is masked off)."""
    return np.maximum.reduce([np.roll(np.roll(x, dy, 0), dx, 1)
                              for dy in (-1, 0, 1) for dx in (-1, 0, 1)
                              if not (dy == 0 and dx == 0)])


def _median_win(x, k):
    """(2k+1)² median via stacked rolls (numpy-only; no scipy dependency)."""
    return np.median(np.stack([np.roll(np.roll(x, dy, 0), dx, 1)
                               for dy in range(-k, k + 1)
                               for dx in range(-k, k + 1)], 0), 0)


def firefly_frac(L, delta_nb=0.10, delta_med=0.18):
    """Density of isolated bright IMPULSES (hot pixels / render fireflies).

    A pixel counts iff it (1) is a strict local maximum, (2) exceeds the MAX of
    its 8 neighbours by delta_nb -- an impulse, NOT a smooth anti-aliased glint
    or a bright edge (where the peak≈its brightest neighbour), AND (3) exceeds
    its own 5x5 median by delta_med -- isolated against its wider context, not a
    peak sitting ON bright sunlit geometry. Sun glints & foliage edges score ~0;
    corruption fields (RADV sparkle, salt-and-pepper) score high. This is a
    DENSITY, not the single worst texel -- one glint must never FAIL a frame.
    Returns (frac, count, max_impulse_excess).
    """
    exc = L - _nbmax8(L)                 # >0 only at strict local maxima
    lm = exc > 0
    out5 = L - _median_win(L, 2)         # excess over wider context
    V = np.zeros(L.shape, bool)
    V[3:-3, 3:-3] = True
    m = (exc > delta_nb) & (out5 > delta_med) & lm & V
    ffmax = float(np.where(V, np.maximum(exc, 0.0), 0.0).max())
    return float(m.mean()), int(m.sum()), ffmax


def speckle_residual(L):
    """Edge-preserving speckle metrics via 3x3 median residual.

    Returns (energy, anticorr):
      energy   = std of |L - median3x3| over the interior. Material EDGES survive
                 the median (small residual); pixel-scale noise does not (large).
                 Separates gross speckle from smooth shading, but NOT from dense
                 material micro-detail on its own -> pair with anticorr.
      anticorr = mean lag-1 spatial autocorrelation of the residual. Additive
                 white / pixel-scale noise high-passes to NEGATIVE adjacent
                 correlation; spatially-coherent texture stays POSITIVE. This
                 distinguishes flat render grain from genuine material detail
                 even when the two carry similar residual energy.
    """
    r = L - _median_win(L, 1)
    ri = r[2:-2, 2:-2]
    energy = float(np.abs(ri).std())
    rc = ri - ri.mean()
    v = float((rc * rc).mean())
    if v <= 0:
        return energy, 0.0
    a = ((rc[:, :-1] * rc[:, 1:]).mean() + (rc[:-1, :] * rc[1:, :]).mean()) / (2 * v)
    return energy, float(a)


def _mean3(x):
    return sum(np.roll(np.roll(x, dy, 0), dx, 1)
               for dy in (-1, 0, 1) for dx in (-1, 0, 1)) / 9.0


def magenta_cast(rgb, strong=0.15, tile=32):
    """Coherent off-locus MAGENTA cast detector (the RADV neutral-surface bug).

    magenta_index = (R+B)/2 - G, measured on a 3x3-smoothed image so a COHERENT
    cast survives while random per-pixel chroma noise averages toward neutral.
    Magenta requires HIGH blue + LOW green; warm/pink golden-hour grading raises
    R and drops B, so magenta_index stays low -> the check is immune to a global
    warm tint (a physical illuminant lives on the blue<->orange locus, never the
    green<->magenta axis). area_frac is the coherent-strong-magenta fraction --
    a cast paints a real AREA; content rarely does. Green casts are deliberately
    NOT gated (foliage makes green ambiguous). Returns
    (area_frac, mean_tint, (worst_tile_y, worst_tile_x), worst_tile_val).
    """
    R = _mean3(rgb[..., 0]); G = _mean3(rgb[..., 1]); B = _mean3(rgb[..., 2])
    mi = (R + B) * 0.5 - G
    area = float((mi > strong).mean())
    tint = float(mi.mean())
    H, W = mi.shape
    best = -1e9; wy = wx = 0
    for y in range(0, max(1, H - tile), tile):
        for x in range(0, max(1, W - tile), tile):
            v = float(mi[y:y + tile, x:x + tile].mean())
            if v > best:
                best = v; wy, wx = y, x
    return area, tint, (wy, wx), best


# ------------------------------------------------- foliage "cotton" family ---
# Promoted from the ad-hoc cotton probe used in the tree-canopy whitening
# episode. THE LESSON THAT SHAPES THIS CODE: the probe's headline number was a
# self-normalizing contrast RATIO (bright mask taken at the region p95, compared
# against the local darks) -- it read ~2.5 both while the canopy was blown out
# and after the defect collapsed, because BOTH sides of the ratio moved with it.
# Everything gated here is therefore an ABSOLUTE measure: a fixed luminance
# floor, a population fraction, a blob density. Ratios stay INFO forever.

def saturation(rgb):
    """HSV-style saturation (max-min)/max of an RGB float image."""
    mx = rgb.max(-1)
    mn = rgb.min(-1)
    return np.where(mx > 1e-6, (mx - mn) / np.maximum(mx, 1e-6), 0.0)


def _label4(mask, max_iter=1024):
    """4-connected component labels (numpy-only; no scipy dependency).

    Max-index propagation to a fixed point. Returns (labels, converged).
    """
    lab = np.where(mask, np.arange(mask.size).reshape(mask.shape) + 1, 0)
    for _ in range(max_iter):
        n = lab.copy()
        n[:-1, :] = np.maximum(n[:-1, :], lab[1:, :])
        n[1:, :] = np.maximum(n[1:, :], lab[:-1, :])
        n[:, :-1] = np.maximum(n[:, :-1], lab[:, 1:])
        n[:, 1:] = np.maximum(n[:, 1:], lab[:, :-1])
        n[~mask] = 0
        if np.array_equal(n, lab):
            return lab, True
        lab = n
    return lab, False


def _depth4(mask, maxd=8):
    """City-block distance to the mask boundary (1 = rim), capped at maxd."""
    d = np.zeros(mask.shape, np.int32)
    cur = mask.copy()
    for _ in range(maxd):
        d[cur] += 1
        e = cur.copy()
        e[:-1, :] &= cur[1:, :]
        e[1:, :] &= cur[:-1, :]
        e[:, :-1] &= cur[:, 1:]
        e[:, 1:] &= cur[:, :-1]
        e[0, :] = False; e[-1, :] = False; e[:, 0] = False; e[:, -1] = False
        cur = e
        if not cur.any():
            break
    return d


def _dark_context(L, cand, block=32):
    """Per-pixel local context luminance: block mean of the NON-candidate pixels.

    This is what keeps a bright sky (or any large bright field) from reading as
    cotton: a canopy puff sits in dark foliage, sky sits in more sky. Excluding
    the candidate pixels from their own context is what makes it work on a puff
    that fills most of its block.
    """
    h, w = L.shape
    by = (h + block - 1) // block
    bx = (w + block - 1) // block
    ctx = np.zeros(L.shape, np.float64)
    for j in range(by):
        y0, y1 = j * block, min(h, (j + 1) * block)
        for i in range(bx):
            x0, x1 = i * block, min(w, (i + 1) * block)
            tile_l = L[y0:y1, x0:x1]
            m = ~cand[y0:y1, x0:x1]
            ctx[y0:y1, x0:x1] = tile_l[m].mean() if m.any() else tile_l.mean()
    return ctx


def cotton_metrics(rgb, lum_floor=0.25, max_sat=0.25, ctx_max=0.25,
                   min_blob_px=12, uniform_tol=0.15, rim_depth=2, core_depth=4,
                   max_blob_frac=0.005, field_texture=0.009):
    """Uniform canopy-whitening ("cotton") measurement on a rendered still.

    Pipeline (each stage is a DISCRIMINATOR, not a re-tuned brightness knob):
      1. fixed-mask candidates: absolute luminance floor AND low saturation --
         cotton is white, sunlit foliage is not.
      2. dark local context: candidates whose surroundings are dark canopy.
         Bright sky / bright ground / HDR blowout live in bright context and
         drop out here -- this is the anti-false-FAIL stage.
      3. size band: below min_blob_px is a render speck; above max_blob_frac of
         the probed area it is a FIELD, not a puff. A field is then judged on
         its internal TEXTURE: sky through a canopy gap is bright, desaturated,
         ringed by dark trunks and SMOOTH, while whitened canopy keeps its leaf
         structure. Smooth fields are dropped from every measure (they are sky);
         textured fields stay in fixedmask_frac, which is the gate for merged
         whole-canopy whitening that never resolves into discrete puffs.
      4. blob structure + rim-vs-core profile: a blob whose CORE is much
         brighter than its rim is a specular glint (CORE_BRIGHT); a blob whose
         rim is much brighter is an alpha/edge fringe (EDGE_FRINGE). Neither is
         cotton. Cotton is flat across the blob (UNIFORM) or too thin to profile
         (THIN -- the shape the real defect took on leaf strands).

    Returns a dict of absolute measures + INFO-only descriptives + the worst
    (largest) cotton blob's centroid for the crop.
    """
    L = luma(rgb)
    S = saturation(rgb)
    npx = float(L.size)
    cand = (L >= lum_floor) & (S <= max_sat)
    ctx = _dark_context(L, cand)
    fixed = cand & (ctx < ctx_max)

    out = {
        'cand_frac': float(cand.mean()),
        'fixedmask_frac': float(fixed.mean()),
        'lum_p95': float(np.percentile(L, 95)),
        'area_frac': 0.0, 'blob_count': 0, 'blob_density': 0.0,
        'median_blob_px': 0, 'signature': 'none',
        'blob_lum': 0.0, 'blob_sat': 0.0, 'ctx_lum': float(L[~cand].mean() if (~cand).any() else 0.0),
        'lum_ratio': 0.0, 'worst': None, 'converged': True,
        'field_smooth': 0, 'field_smooth_frac': 0.0,
        'field_textured': 0, 'field_textured_frac': 0.0,
        'buckets': {'UNIFORM': 0, 'THIN': 0, 'CORE_BRIGHT': 0, 'EDGE_FRINGE': 0},
    }
    if not fixed.any():
        return out

    lab, conv = _label4(fixed)
    out['converged'] = conv
    ids, cnt = np.unique(lab[lab > 0], return_counts=True)
    cap = max_blob_frac * npx

    # FIELDS (oversize blobs): smooth = sky seen through the canopy, drop it;
    # textured = whitened canopy that merged into one region, keep it.
    resid = np.abs(L - _median_win(L, 1))
    smooth = np.zeros(fixed.shape, bool)
    for i in ids[cnt > cap]:
        m = lab == i
        tex = float(resid[m].mean())
        if tex < field_texture:
            smooth |= m
            out['field_smooth'] += 1
            out['field_smooth_frac'] += float(m.mean())
        else:
            out['field_textured'] += 1
            out['field_textured_frac'] += float(m.mean())
    if smooth.any():
        fixed = fixed & ~smooth
        out['fixedmask_frac'] = float(fixed.mean())

    keep = ids[(cnt >= min_blob_px) & (cnt <= cap)]
    if keep.size == 0:
        return out
    depth = _depth4(fixed)

    cot = np.zeros(fixed.shape, bool)
    sizes = []
    falloffs = []
    best_px, best_id = 0, None
    for i in keep:
        m = lab == i
        # rim is sampled ONE pixel inside the boundary: the depth-1 ring is the
        # antialiased/partial-coverage edge, and reading it makes every blob with
        # a soft edge look core-bright.
        rim = m & (depth == rim_depth)
        core = m & (depth >= core_depth)
        if rim.sum() < 6 or core.sum() < 6:
            kind = 'THIN'          # unprofilable strand: counts as cotton
        else:
            el = float(L[rim].mean())
            cl = float(L[core].mean())
            drop = (el - cl) / max(el, 1e-6)
            falloffs.append(drop)
            kind = ('EDGE_FRINGE' if drop > uniform_tol else
                    ('UNIFORM' if drop >= -uniform_tol else 'CORE_BRIGHT'))
        out['buckets'][kind] += 1
        if kind in ('UNIFORM', 'THIN'):
            cot |= m
            n = int(m.sum())
            sizes.append(n)
            if n > best_px:
                best_px, best_id = n, i

    out['area_frac'] = float(cot.mean())
    out['blob_count'] = len(sizes)
    out['blob_density'] = float(len(sizes) / (npx / 1e6))
    if sizes:
        out['median_blob_px'] = int(np.median(sizes))
        out['blob_lum'] = float(L[cot].mean())
        out['blob_sat'] = float(S[cot].mean())
        bg = ~cand
        if bg.any():
            # SELF-NORMALIZING -- INFO ONLY. Never gate on this (see header).
            out['lum_ratio'] = float(L[cot].mean() / max(L[bg].mean(), 1e-6))
        ys, xs = np.nonzero(lab == best_id)
        out['worst'] = (int(ys.mean()), int(xs.mean()), best_px)
    if falloffs:
        mf = float(np.median(falloffs))
        out['signature'] = ('EDGE_FRINGE' if mf > uniform_tol else
                            ('UNIFORM' if mf >= -uniform_tol else 'CORE_BRIGHT'))
        out['median_falloff'] = mf
    elif sizes:
        out['signature'] = 'THIN'
    return out


# --------------------------------------------- impostor-atlas check family ---
def load_rgba(path):
    """Load a PNG as (rgb, alpha) float64 0..1; opaque alpha if the file has none."""
    from PIL import Image
    im = Image.open(path)
    a = np.asarray(im.convert('RGBA')).astype(np.float64)
    peak = 65535.0 if a.max() > 255 else 255.0
    a = a / peak
    return a[..., :3], a[..., 3]


def atlas_fringe(rgb, alpha, bg_hi=0.02, fg_lo=0.98, bright_margin=0.10):
    """White-bleed measurement on an impostor atlas.

    Fully-TRANSPARENT texels still get filtered into the silhouette by mip
    generation and bilinear taps, so their RGB must be no brighter than the
    silhouette they border. Bright background texels are the authoring defect
    that shows up as a white halo (and as whitened canopy at LOD range).

    Measured AGAINST THE SPRITE'S OWN FOREGROUND, not an absolute dark
    assumption: a correctly authored atlas edge-extends the silhouette colour
    outward, so a legitimately light sprite has a legitimately light background
    and an absolute luminance ceiling would false-FAIL it. This is NOT the
    self-normalizing trap that sank the cotton contrast ratio -- painting the
    background white does not move the foreground, so bg-minus-fg is a true
    differential. Returns a dict.
    """
    bg = alpha < bg_hi
    fg = alpha > fg_lo
    edge = ~bg & ~fg
    L = luma(rgb)
    out = {'coverage': float((alpha > 0.5).mean()),
           'bg_frac': float(bg.mean()), 'edge_frac': float(edge.mean()),
           'fg_frac': float(fg.mean()), 'has_bg': bool(bg.any()),
           'bg_lum': 0.0, 'bg_bright_frac': 0.0, 'edge_lum': 0.0,
           'fg_lum': float(L[fg].mean()) if fg.any() else 0.0,
           'fg_bright_end': float(np.percentile(L[fg], 95)) if fg.any() else 1.0,
           'bg_excess': 0.0, 'worst': None}
    if bg.any():
        out['bg_lum'] = float(L[bg].mean())
        # sparse hot texels a MEAN would hide: brighter than anything the sprite
        # itself is (its own p95 plus a margin).
        out['bg_bright_frac'] = float((L[bg] > out['fg_bright_end'] + bright_margin).mean())
        out['bg_excess'] = out['bg_lum'] - out['fg_lum']
        Lb = np.where(bg, L, 0.0)
        tile = max(16, min(Lb.shape) // 32)
        h, w = Lb.shape
        best = -1.0
        for y in range(0, h - tile + 1, tile):
            for x in range(0, w - tile + 1, tile):
                v = float(Lb[y:y + tile, x:x + tile].mean())
                if v > best:
                    best, wy, wx = v, y, x
        if best >= 0:
            out['worst'] = (wy, wx, best)
    if edge.any():
        out['edge_lum'] = float(L[edge].mean())
    return out


def mip_drift(rgb, alpha, levels=4, min_weight=0.02):
    """Box-filter mip chain: naive vs alpha-weighted downsample divergence.

    A naive (unweighted) box mip averages transparent background texels INTO the
    silhouette; the alpha-weighted mip averages only what is actually there. The
    gap between them is the value error the sampler will show at LOD range -- it
    is what turns a green canopy white a few mips down. Measured over texels
    that still carry coverage (weight > min_weight), per channel, as the maximum
    absolute luminance divergence over the chain.
    Returns (max_drift, per_level list of (level, drift, naive_lum, weighted_lum)).
    """
    n_rgb = rgb.copy()
    w_rgb = rgb * alpha[..., None]
    w_a = alpha.copy()
    n_a = alpha.copy()
    rows = []
    worst = 0.0
    for lv in range(1, levels + 1):
        def box(x):
            h = (x.shape[0] // 2) * 2
            w = (x.shape[1] // 2) * 2
            x = x[:h, :w]
            return (x[0::2, 0::2] + x[1::2, 0::2] + x[0::2, 1::2] + x[1::2, 1::2]) * 0.25
        if min(n_rgb.shape[0], n_rgb.shape[1]) < 2:
            break
        n_rgb = box(n_rgb)
        w_rgb = box(w_rgb)
        w_a = box(w_a)
        n_a = box(n_a)
        m = w_a > min_weight
        if not m.any():
            break
        naive = luma(n_rgb)[m]
        weighted = luma(w_rgb / np.maximum(w_a, 1e-6)[..., None])[m]
        d = float(np.abs(naive - weighted).mean())
        worst = max(worst, d)
        rows.append((lv, d, float(naive.mean()), float(weighted.mean())))
    return worst, rows
