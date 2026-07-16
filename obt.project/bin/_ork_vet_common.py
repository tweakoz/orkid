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
