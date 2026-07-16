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
    make_image()
    make_hmap()
    make_mesh()
    make_movie()
    total = 0
    for root, _, files in os.walk(HERE):
        for f in files:
            if f in ('make_corpus.py', 'run_vet_regression.py'):
                continue
            total += os.path.getsize(os.path.join(root, f))
    print(f"corpus written under {HERE} ({total / 1024:.0f} KB)")


if __name__ == '__main__':
    main()
