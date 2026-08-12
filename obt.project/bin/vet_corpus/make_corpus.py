#!/bin/sh
""":"
# trampoline: exec the first python3 with numpy+PIL+trimesh (corpus generation needs all).
for _py in \
    "$OBT_STAGE/pyvenv/bin/python3" \
    "$HOME"/.staging*/pyvenv/bin/python3 \
    "$(command -v python3)"; do
    [ -x "$_py" ] && "$_py" -c 'import numpy, PIL, trimesh' 2>/dev/null && exec "$_py" "$0" "$@"
done
echo "make_corpus: no python3 with numpy+PIL+trimesh found on this host" >&2
exit 3
":"""
"""
Deterministic generator for the ork.vet.* instrument REGRESSION corpus.

For every instrument it emits a CLEAN synthetic artifact and a seeded-MUTANT
twin (plus a couple of A/B siblings for the movie classifier). Synthetics
only -- no real scene bakes -- fixed seeds, tiny (<~2MB total). Re-runnable;
overwrites in place. This is what keeps future instrument evolution honest:
clean must stay PASS, mutant must stay FAIL.
"""
import os
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))
SEED = 1337


def _dir(name):
    d = os.path.join(HERE, name)
    os.makedirs(d, exist_ok=True)
    return d


def fbm(size, seed, octaves=5):
    rng = np.random.default_rng(seed)
    acc = np.zeros((size, size), dtype=np.float64)
    amp = 1.0
    for o in range(octaves):
        n = 2 ** (o + 2)
        grid = (rng.random((n, n)) * 255).astype(np.uint8)
        up = np.asarray(Image.fromarray(grid).resize((size, size), Image.BICUBIC)).astype(np.float64) / 255.0
        acc += amp * up
        amp *= 0.5
    acc -= acc.min()
    acc /= (acc.max() or 1.0)
    return acc


# ------------------------------------------------------------------- audio ---
def _wav32(path, x, sr=48000):
    """float32 WAV writer (no clamping: overs must stay visible to the meter)."""
    import struct
    x = np.asarray(x, dtype=np.float32)
    if x.ndim == 1:
        x = x[:, None]
    nfr, nch = x.shape
    body = x.reshape(-1).tobytes()
    hdr = (b'RIFF' + struct.pack('<I', 36 + len(body)) + b'WAVEfmt ' +
           struct.pack('<IHHIIHH', 16, 3, nch, sr, sr * nch * 4, nch * 4, 32) +
           b'data' + struct.pack('<I', len(body)))
    with open(path, 'wb') as f:
        f.write(hdr + body)


def make_audio():
    """CLEAN twin = a bandlimited, panning, HARD-TRANSIENT stereo program.

    The transients matter: they are what a naive click detector false-positives
    on, so the clean twin proves the click check's onset discrimination at the
    same time as the mutants prove its sensitivity.
    """
    d = _dir('audio')
    sr = 48000
    n = sr * 4
    t = np.arange(n) / sr
    env = np.ones(n)
    for on in (0.5, 1.5, 2.5, 3.5):  # instantaneous-attack musical onsets
        i = int(on * sr)
        env[i:] = np.exp(-np.arange(n - i) / (0.15 * sr))
    sig = 0.6 * env * (np.sin(2 * np.pi * 220 * t) + 0.5 * np.sin(2 * np.pi * 440 * t) +
                       0.25 * np.sin(2 * np.pi * 3000 * t)) / 1.75
    pan = 0.5 + 0.5 * np.sin(2 * np.pi * 0.25 * t)
    clean = np.stack([sig * np.cos(pan * np.pi / 2), sig * np.sin(pan * np.pi / 2)], axis=1)
    clean /= (np.max(np.abs(clean)) / 0.7)
    _wav32(os.path.join(d, 'clean.wav'), clean, sr)

    # clipped: +6.8dB into a hard rail -> flat tops, FS samples, inter-sample overs
    _wav32(os.path.join(d, 'clip.wav'), np.clip(clean * 2.2, -1.0, 1.0), sr)

    # click: ONE sample displaced at 2.0s (-6 dBFS) on L; must not be excused
    # as a transient (the clean twin's real onsets must be)
    m = clean.copy()
    m[int(2.0 * sr), 0] += 0.5
    _wav32(os.path.join(d, 'click.wav'), m, sr)

    # dropout: 30 ms of digital silence bracketed by signal
    m = clean.copy()
    i = int(1.2 * sr)
    m[i:i + int(0.03 * sr), :] = 0.0
    _wav32(os.path.join(d, 'dropout.wav'), m, sr)

    # dc: +0.02 FS offset on L only
    m = clean.copy() * 0.9
    m[:, 0] += 0.02
    _wav32(os.path.join(d, 'dc.wav'), m, sr)

    # deadch: R collapsed to -80 dB for the whole file
    m = clean.copy()
    m[:, 1] *= 1e-4
    _wav32(os.path.join(d, 'deadch.wav'), m, sr)

    # intersample: sample peaks all < 1.0, reconstructed peak > 1.0 (the defect
    # a sample-peak meter cannot see)
    s = 0.999 * np.sin(2 * np.pi * (sr / 4.0 * 0.999) * t + np.pi / 4)
    _wav32(os.path.join(d, 'intersample.wav'), np.stack([s, s], axis=1), sr)


# ------------------------------------------------------------------- image ---
def make_image():
    d = _dir('image')
    size = 256
    yy, xx = np.mgrid[0:size, 0:size] / (size - 1.0)
    base = 0.30 + 0.5 * xx + 0.15 * np.sin(6.28 * yy) * 0.2  # smooth render-like frame
    base = np.clip(base, 0, 1)
    rgb = np.stack([base, base * 0.9 + 0.05, base * 0.8 + 0.1], axis=2)
    clean = (rgb * 255).astype(np.uint8)
    Image.fromarray(clean, 'RGB').save(os.path.join(d, 'clean.png'))

    # mutant: isolated spikes (absolute spike check) + a speckle patch (golden diff)
    rng = np.random.default_rng(SEED + 1)
    mut = clean.astype(np.int16).copy()
    for _ in range(14):
        y, x = rng.integers(4, size - 4), rng.integers(4, size - 4)
        mut[y, x] = np.clip(mut[y, x].astype(int) + 170, 0, 255)
    patch = (size // 2, size // 2)
    noise = rng.normal(0, 10, (size // 2, size // 2, 3))
    mut[patch[0]:, patch[1]:] = np.clip(mut[patch[0]:, patch[1]:] + noise, 0, 255)
    Image.fromarray(mut.astype(np.uint8), 'RGB').save(os.path.join(d, 'mutant.png'))

    # black frame: the settle-race snapshot that a byte-identity gate would pass
    Image.fromarray(np.zeros((size, size, 3), np.uint8), 'RGB').save(os.path.join(d, 'black.png'))

    make_render(d, size)


def make_render(d, size):
    """Render-mode discriminating-check twins (GAP-1 speckle/glint, GAP-2 chroma).

    render_clean is a DETAILED WARM golden-hour-like frame with real sun glints
    and broadband material detail -- exactly the content that false-FAILed the
    grayscale spike/highband checks. It must PASS every render check. Each mutant
    injects ONE defect class so its target check is proven in isolation while the
    clean twin passes it.
    """
    yy, xx = np.mgrid[0:size, 0:size] / (size - 1.0)
    det = np.clip(fbm(size, SEED, octaves=6) * 0.35 + 0.35 + 0.20 * xx, 0, 1)
    R = np.clip(det * 1.05 + 0.06, 0, 1)   # warm golden grade: R > G > B
    G = np.clip(det * 0.85 + 0.02, 0, 1)
    B = np.clip(det * 0.55, 0, 1)
    for (cy, cx) in [(70, 180), (150, 90)]:   # smooth multi-pixel sun glints (NOT hot pixels)
        gy, gx = np.mgrid[0:size, 0:size]
        bump = np.exp(-(((gy - cy) ** 2 + (gx - cx) ** 2) / 8.0))
        R = np.clip(R + 0.50 * bump, 0, 1)
        G = np.clip(G + 0.45 * bump, 0, 1)
        B = np.clip(B + 0.35 * bump, 0, 1)
    clean = np.stack([R, G, B], axis=2)
    Image.fromarray((clean * 255).astype(np.uint8), 'RGB').save(os.path.join(d, 'render_clean.png'))

    rng = np.random.default_rng(SEED + 11)
    # firefly field: isolated bright impulses (hot pixels) -> spike.firefly_frac
    ff = clean.copy()
    ys, xs = rng.integers(3, size - 3, 320), rng.integers(3, size - 3, 320)
    ff[ys, xs] = np.clip(ff[ys, xs] + 0.50, 0, 1)
    Image.fromarray((ff * 255).astype(np.uint8), 'RGB').save(os.path.join(d, 'render_firefly.png'))

    # subtle flat grain: energy UNDER the residual ceiling, caught ONLY by the
    # anticorrelation of the median residual -> speckle.grain_anticorr
    sp = np.clip(clean + rng.normal(0, 0.03, clean.shape), 0, 1)
    Image.fromarray((sp * 255).astype(np.uint8), 'RGB').save(os.path.join(d, 'render_speckle.png'))

    # gross grain: high residual energy -> speckle.residual_energy
    gp = np.clip(clean + rng.normal(0, 0.13, clean.shape), 0, 1)
    Image.fromarray((gp * 255).astype(np.uint8), 'RGB').save(os.path.join(d, 'render_grossspeckle.png'))

    # coherent magenta cast on the lower ('ground') half -> chroma.magenta_area
    mg = clean.copy()
    half = mg[size // 2:]
    half[..., 0] = np.clip(half[..., 0] + 0.06, 0, 1)
    half[..., 2] = np.clip(half[..., 2] + 0.34, 0, 1)
    half[..., 1] = np.clip(half[..., 1] - 0.04, 0, 1)
    mg[size // 2:] = half
    Image.fromarray((mg * 255).astype(np.uint8), 'RGB').save(os.path.join(d, 'render_magenta.png'))


# ----------------------------------------------------------------- foliage ---
def _canopy(size, seed):
    """Dark procedural canopy + bright sky + trunk silhouettes + real glints.

    The clean twin deliberately carries the two things that false-FAIL a naive
    brightness probe: a BRIGHT SKY band, and sky seen through a GAP between dark
    trunks (bright, desaturated, ringed by dark = the shape cotton has, told
    apart only by size). It also carries specular glints (bright core, dim rim).

    The canopy carries LEAF-SCALE structure (high fbm octaves, coherent -- not
    white noise, which would trip the render speckle checks): it is what tells a
    whitened canopy apart from a smooth patch of sky.
    """
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:size, 0:size]
    det = fbm(size, seed, octaves=6)
    # leaf detail at the 2px scale: coherent (bicubic-upsampled), NOT white
    # noise, so the clean twin still passes the render speckle checks.
    fine = np.asarray(Image.fromarray(
        (np.random.default_rng(seed + 1).random((size // 2, size // 2)) * 255).astype(np.uint8)
    ).resize((size, size), Image.BICUBIC)).astype(np.float64) / 255.0
    fol = (0.03 + 0.10 * det) * (0.30 + 1.55 * fine)
    rgb = np.stack([fol * 0.55, fol * 1.00, fol * 0.45], axis=2)   # dark green
    sky = np.stack([np.full((size, size), 0.72), np.full((size, size), 0.76),
                    np.full((size, size), 0.86)], axis=2)
    horizon = int(size * 0.32)
    openness = np.clip((horizon - yy) / 12.0, 0, 1)                # sky band
    gap = ((xx > size * 0.60) & (xx < size * 0.74) &
           (yy > horizon) & (yy < size * 0.72))                    # sky through a trunk gap
    m = np.maximum(openness, gap.astype(float))[..., None]
    rgb = rgb * (1 - m) + sky * m
    for cx in (int(size * 0.58), int(size * 0.76)):                # dark trunks
        bar = (np.abs(xx - cx) < size * 0.02)
        rgb[bar] *= 0.10
    for _ in range(10):                                            # specular glints
        cy = int(rng.integers(int(size * 0.40), size - 12))
        cx = int(rng.integers(8, int(size * 0.55)))
        rad = int(rng.integers(6, 9))
        d = np.hypot(yy - cy, xx - cx)
        g = (np.clip((rad - d) / 1.5, 0, 1)
             * np.exp(-(d ** 2) / (2.0 * (rad * 0.45) ** 2)) * 0.62)[..., None]
        rgb = rgb * (1 - g) + g
    return np.clip(rgb, 0, 1)


def make_foliage():
    """Canopy-whitening ('cotton') twins. Probe region = the lower canopy band."""
    d = _dir('foliage')
    size = 512
    clean = _canopy(size, SEED + 21)
    Image.fromarray((clean * 255).astype(np.uint8), 'RGB').save(os.path.join(d, 'clean.png'))

    rng = np.random.default_rng(SEED + 22)
    yy, xx = np.mgrid[0:size, 0:size]
    # cotton: FLAT white discs -- rim luminance == core luminance (UNIFORM), the
    # signature the whitening defect has. Same footprint as the clean twin's
    # glints; only the radial profile differs.
    puffs = clean.copy()
    for _ in range(22):
        cy = int(rng.integers(int(size * 0.42), size - 12))
        cx = int(rng.integers(8, int(size * 0.55)))
        rad = int(rng.integers(6, 9))
        g = (np.clip((rad - np.hypot(yy - cy, xx - cx)) / 1.5, 0, 1) * 0.62)[..., None]
        puffs = puffs * (1 - g) + g
    Image.fromarray((np.clip(puffs, 0, 1) * 255).astype(np.uint8), 'RGB').save(
        os.path.join(d, 'cotton_puffs.png'))

    # whole-canopy whitening: no discrete blobs at all -- one merged bright field
    # that only the fixed-mask population check can see.
    whole = clean.copy()
    band = whole[int(size * 0.40):, :int(size * 0.55)]
    k = (np.clip((band @ np.array([0.299, 0.587, 0.114]) - 0.03) / 0.10, 0, 1) * 0.55)[..., None]
    whole[int(size * 0.40):, :int(size * 0.55)] = band * (1 - k) + 0.70 * k
    Image.fromarray((np.clip(whole, 0, 1) * 255).astype(np.uint8), 'RGB').save(
        os.path.join(d, 'cotton_wholecanopy.png'))


# ------------------------------------------------------------------- atlas ---
def _sprite_atlas(size, seed):
    """Impostor-style sprite sheet: leaf clusters with real alpha coverage."""
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:size, 0:size]
    alpha = np.zeros((size, size))
    rgb = np.zeros((size, size, 3))
    cell = size // 4
    for j in range(4):
        for i in range(4):
            cy, cx = j * cell + cell // 2, i * cell + cell // 2
            for _ in range(9):
                oy = int(rng.integers(-cell // 3, cell // 3))
                ox = int(rng.integers(-cell // 3, cell // 3))
                rad = int(rng.integers(cell // 8, cell // 4))
                m = np.hypot(yy - (cy + oy), xx - (cx + ox)) < rad
                alpha[m] = 1.0
                tint = 0.55 + 0.45 * rng.random()
                rgb[m] = np.array([0.10 * tint, 0.34 * tint, 0.08 * tint])
    return rgb, alpha


def _edge_extend(rgb, alpha, iters=32):
    """Author-correct background: flood the silhouette colour outward."""
    c = rgb.copy()
    known = alpha > 0.02
    for _ in range(iters):
        acc = np.zeros_like(c)
        cnt = np.zeros(c.shape[:2])
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            acc += np.roll(np.roll(c, dy, 0), dx, 1) * np.roll(np.roll(known, dy, 0), dx, 1)[..., None]
            cnt += np.roll(np.roll(known, dy, 0), dx, 1)
        fill = (cnt > 0) & ~known
        if not fill.any():
            break
        c[fill] = acc[fill] / cnt[fill][..., None]
        known = known | fill
    return c


def make_atlas():
    """Impostor-atlas twins: background bleed (2 flavours) and mip drift."""
    d = _dir('atlas')
    size = 256
    rgb, alpha = _sprite_atlas(size, SEED + 31)

    def save(name, c):
        Image.fromarray((np.clip(np.dstack([c, alpha]), 0, 1) * 255).astype(np.uint8),
                        'RGBA').save(os.path.join(d, name))

    clean = _edge_extend(rgb, alpha)
    save('clean.png', clean)                       # extended bg: no bleed, no drift

    bg = alpha < 0.02
    white = clean.copy()
    white[bg] = 1.0                                # cleared-to-white canvas
    save('whitebg.png', white)

    rng = np.random.default_rng(SEED + 32)
    speck = clean.copy()
    speck[bg & (rng.random(bg.shape) < 0.06)] = 1.0   # sparse white texels
    save('specklebg.png', speck)

    black = clean.copy()
    black[bg] = 0.0                                # unextended: mip drift ONLY
    save('blackbg.png', black)


# ------------------------------------------------------------------- hmap ----
def make_hmap():
    d = _dir('hmap')
    size = 512
    clean = fbm(size, SEED)
    Image.fromarray((clean * 65535).astype(np.uint16)).save(os.path.join(d, 'clean.png'))

    rng = np.random.default_rng(SEED + 2)
    mut = clean.copy()
    # quadrant speckle (high-frequency energy) + isolated spikes (walk jolt + spike)
    q = mut[size // 2:, size // 2:]
    mut[size // 2:, size // 2:] = np.clip(q + rng.normal(0, 0.02, q.shape), 0, 1)
    for _ in range(8):
        y, x = rng.integers(2, size - 2), rng.integers(2, size - 2)
        mut[y, x] = min(1.0, mut[y, x] + 0.4)
    Image.fromarray((mut * 65535).astype(np.uint16)).save(os.path.join(d, 'mutant.png'))


# ------------------------------------------------------------------- mesh ----
def make_mesh():
    d = _dir('mesh')
    ico = trimesh.creation.icosphere(subdivisions=3)
    V = np.asarray(ico.vertices, dtype=np.float64)
    F = np.asarray(ico.faces, dtype=np.int64)
    ico.export(os.path.join(d, 'clean.obj'))

    V2 = V.copy()
    F2 = F.copy()
    # 1) unweld ~30 faces: duplicate their vertices -> duplicate-vertex + broken watertight
    unweld = F2[:30].copy()
    newverts = []
    for fi in range(30):
        for k in range(3):
            newverts.append(V2[unweld[fi, k]])
            unweld[fi, k] = len(V2) + len(newverts) - 1
    V2 = np.vstack([V2, np.array(newverts)])
    F2 = np.vstack([F2, unweld])
    F2 = np.delete(F2, np.arange(30), axis=0)
    # 2) collapse 3 unwelded faces to zero-area slivers (degenerate-triangle check)
    last = len(F2) - 1
    for k in range(3):
        fc = F2[last - k]
        V2[fc[1]] = V2[fc[0]]
    # 3) intersecting flap: a flat sheet through the sphere interior (self-intersection)
    b = len(V2)
    flap_v = np.array([[-1.5, -0.2, 0.1], [1.5, -0.2, 0.1],
                       [1.5, 0.2, 0.1], [-1.5, 0.2, 0.1]])
    V2 = np.vstack([V2, flap_v])
    F2 = np.vstack([F2, [[b, b + 1, b + 2], [b, b + 2, b + 3]]])
    mut = trimesh.Trimesh(vertices=V2, faces=F2, process=False)
    mut.export(os.path.join(d, 'mutant.obj'))


# ------------------------------------------------------------------ movie ----
def _encode(frames, path, fps=12):
    tmp = tempfile.mkdtemp(prefix='vetframes_')
    for i, fr in enumerate(frames):
        Image.fromarray(fr, 'RGB').save(os.path.join(tmp, f"{i:04d}.png"))
    # ffv1 (lossless) + planar RGB -> decoded frames are byte-exact, so the A/B
    # classifier sees ONLY the injected differences, never codec block noise.
    subprocess.run(
        ['ffmpeg', '-y', '-v', 'error', '-framerate', str(fps),
         '-i', os.path.join(tmp, '%04d.png'),
         '-c:v', 'ffv1', '-level', '3', '-pix_fmt', 'gbrp', path],
        check=True)


def make_movie():
    d = _dir('movie')
    size, nframes = 96, 24
    yy, xx = np.mgrid[0:size, 0:size] / (size - 1.0)
    base = []
    for i in range(nframes):
        phase = min(i, 16) / 16.0  # animates for 16 frames then holds (settles)
        g = 0.25 + 0.5 * ((xx + phase * 0.4) % 1.0)
        rgb = np.clip(np.stack([g, g, g], axis=2) * 255, 0, 255).astype(np.uint8)
        base.append(rgb)
    _encode(base, os.path.join(d, 'clean.mkv'))

    # mutant: final frame is BLACK (a settle-race snapshot) -> per-frame + settle FAIL
    mut = [f.copy() for f in base]
    mut[-1] = np.zeros((size, size, 3), np.uint8)
    _encode(mut, os.path.join(d, 'mutant.mkv'))

    # A/B PASS twin: sparse ISOLATED noise ABOVE the noise-delta ceiling. Exercises
    # the erosion-based isolation test: pixels differ by >12 but are scattered, so
    # they classify as encoder-noise (PASS), not structural.
    rng = np.random.default_rng(SEED + 3)
    noisy = []
    for f in base:
        fn = f.astype(np.int16).copy()
        m = rng.random((size, size)) < 0.01
        n = int(m.sum())
        delta = rng.integers(15, 26, size=(n, 1)) * rng.choice([-1, 1], size=(n, 1))
        fn[m] = np.clip(fn[m].astype(int) + delta, 0, 255)
        noisy.append(fn.astype(np.uint8))
    _encode(noisy, os.path.join(d, 'clean_noise.mkv'))

    # A/B FAIL twin: a coherent bright block (structural difference)
    struct = []
    for f in base:
        fs = f.copy()
        fs[30:54, 30:54] = 255
        struct.append(fs)
    _encode(struct, os.path.join(d, 'structural.mkv'))


def main():
    # optional selector: `make_corpus.py audio` regenerates ONE type, so adding
    # a new instrument does not churn the other types' committed binaries.
    makers = {'audio': make_audio, 'image': make_image, 'hmap': make_hmap,
              'foliage': make_foliage, 'atlas': make_atlas,
              'mesh': make_mesh, 'movie': make_movie}
    want = [a for a in sys.argv[1:] if not a.startswith('-')] or list(makers)
    for name in want:
        if name not in makers:
            sys.stderr.write('unknown corpus type %r (have: %s)\n'
                             % (name, ', '.join(makers)))
            sys.exit(3)
        makers[name]()
    total = 0
    for root, _, files in os.walk(HERE):
        for f in files:
            if f in ('make_corpus.py', 'run_vet_regression.py'):
                continue
            total += os.path.getsize(os.path.join(root, f))
    print(f"corpus written under {HERE} ({total / 1024:.0f} KB)")


if __name__ == '__main__':
    main()
