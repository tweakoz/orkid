#!/bin/sh
""":"
# trampoline: exec the first python3 that has numpy+PIL (movie vet needs both + ffmpeg on PATH).
for _py in \
    "$OBT_STAGE/pyvenv/bin/python3" \
    "$HOME"/.staging*/pyvenv/bin/python3 \
    "$(command -v python3)"; do
    [ -x "$_py" ] && "$_py" -c 'import numpy, PIL' 2>/dev/null && exec "$_py" "$0" "$@"
done
echo "ork.vet.movie: no python3 with numpy+PIL found on this host" >&2
exit 3
":"""
"""
Movie vet (temporal): ffmpeg frame extraction + per-frame delegation to
ork.vet.image.py + temporal checks. Porcelain verdicts.

Sample points (default first,mid,last,eof) are pulled with ffmpeg; 'eof' uses
-sseof for a robust final-decodable-frame grab. Each extracted frame is vetted
by the image instrument (a black settle-race frame FAILs there and surfaces
here). Temporal checks:

  - SETTLE: the two latest frames must be stable (changed-fraction below a
    tunable floor); a late-sequence that never settles FAILs.
  - FRAME-PAIR CLASSIFICATION (--compare): per sample, classify the A-vs-B
    difference as encoder-noise vs structural. Encoder noise = isolated pixels
    with small max-channel delta; structural = coherent regions. Both the
    noise-delta ceiling and the coherent-region fraction are NAMED, tunable
    parameters, not magic numbers.
"""
import sys
import os
import argparse
import shutil
import subprocess
import tempfile

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _ork_vet_common as vet

IMAGE_TOOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ork.vet.image.py')


def _need(binary):
    path = shutil.which(binary)
    if not path:
        sys.stderr.write(
            f"ork.vet.movie: '{binary}' not found on PATH -- ffmpeg/ffprobe are a required "
            "dependency for temporal vet. Report this missing dependency.\n")
        sys.exit(3)
    return path


def probe_duration(ffprobe, movie):
    out = subprocess.run(
        [ffprobe, '-v', 'error', '-select_streams', 'v:0',
         '-show_entries', 'stream=duration,nb_frames,r_frame_rate',
         '-show_entries', 'format=duration',
         '-of', 'default=nw=1', movie],
        capture_output=True, text=True)
    dur, nframes, fps = None, None, None
    for line in out.stdout.splitlines():
        k, _, v = line.partition('=')
        if k == 'duration' and v not in ('N/A', '') and dur is None:
            try:
                dur = float(v)
            except ValueError:
                pass
        elif k == 'nb_frames' and v not in ('N/A', ''):
            try:
                nframes = int(v)
            except ValueError:
                pass
        elif k == 'r_frame_rate' and '/' in v:
            n, d = v.split('/')
            fps = float(n) / float(d) if float(d) else None
    return dur, nframes, fps


def extract(ffmpeg, movie, spec, outpath):
    """spec: ('ss', t) input-seek at timestamp, or ('sseof', t) end-relative seek."""
    mode, t = spec
    if mode == 'sseof':
        cmd = [ffmpeg, '-y', '-v', 'error', '-sseof', f"{t}", '-i', movie,
               '-frames:v', '1', outpath]
    else:
        cmd = [ffmpeg, '-y', '-v', 'error', '-i', movie, '-ss', f"{t}",
               '-frames:v', '1', outpath]
    subprocess.run(cmd, capture_output=True, text=True)
    return os.path.exists(outpath) and os.path.getsize(outpath) > 0


def sample_specs(names, dur, dt):
    out = []
    for n in names:
        if n == 'first':
            out.append(('first', ('ss', 0.0)))
        elif n == 'mid':
            out.append(('mid', ('ss', max(0.0, dur * 0.5))))
        elif n == 'last':
            out.append(('last', ('ss', max(0.0, dur - dt * 1.5))))
        elif n == 'eof':
            # -sseof seeks to (duration+offset); offset must exceed one frame
            # duration or it lands past the last frame and yields nothing
            out.append(('eof', ('sseof', -max(dt * 1.5, 0.1))))
        else:
            raise SystemExit(f"unknown sample point '{n}' (use first,mid,last,eof)")
    return out


def load_rgb(path):
    from PIL import Image
    a = np.asarray(Image.open(path).convert('RGB')).astype(np.int16)
    return a


def binary_erode(mask):
    """3x3 binary erosion (pure numpy): survives only where all 8 neighbors also set."""
    p = np.pad(mask, 1, mode='constant', constant_values=False)
    out = mask.copy()
    for dy in (0, 1, 2):
        for dx in (0, 1, 2):
            out &= p[dy:dy + mask.shape[0], dx:dx + mask.shape[1]]
    return out


def vet_frame(frame_path, max_hb, max_spike):
    """Delegate to the image instrument; return (verdict, failed_check_names)."""
    r = subprocess.run(
        [IMAGE_TOOL, frame_path, '--kind', 'render',
         '--max-highband', str(max_hb), '--max-spike', str(max_spike)],
        capture_output=True, text=True)
    verdict, failed = 'FAIL', []
    for line in r.stdout.splitlines():
        if line.startswith('# verdict:'):
            verdict = 'FAIL' if 'FAIL' in line else 'PASS'
        elif '\t' in line and line.rstrip().endswith('FAIL'):
            failed.append(line.split('\t', 1)[0])
    return verdict, failed


def classify_pair(a, b, noise_delta, struct_frac):
    """Return (max_channel_delta, noise_pixel_frac, structural_frac, label)."""
    d = np.abs(a - b).max(axis=2)  # per-pixel max channel delta, 0..255
    max_delta = int(d.max())
    noise_mask = d > noise_delta
    noise_pixel_frac = float(noise_mask.mean())
    structural = binary_erode(noise_mask)
    structural_frac = float(structural.mean())
    if max_delta <= noise_delta:
        label = 'subnoise'
    elif structural_frac < struct_frac:
        label = 'encoder-noise'
    else:
        label = 'structural'
    return max_delta, noise_pixel_frac, structural_frac, label


def main():
    p = argparse.ArgumentParser(description='Vet a movie (temporal); porcelain verdicts')
    p.add_argument('movie')
    p.add_argument('--compare', help='second movie for frame-pair structural-vs-noise classification')
    p.add_argument('--samples', default='first,mid,last,eof',
                   help='comma list of sample points from {first,mid,last,eof}')
    p.add_argument('--settle-frac', type=float, default=0.02,
                   help='max changed-fraction between the final frame and settle-k frames earlier')
    p.add_argument('--settle-k', type=int, default=3,
                   help='frames-before-end to compare the final frame against (settle window)')
    p.add_argument('--noise-delta', type=int, default=12,
                   help='encoder-noise ceiling: max-channel delta (0-255) tolerated on isolated pixels')
    p.add_argument('--struct-frac', type=float, default=0.0005,
                   help='structural-difference floor: coherent-region pixel fraction (post-erosion)')
    p.add_argument('--frame-max-highband', type=float, default=0.20,
                   help='per-frame FFT high-band ceiling passed to the image instrument')
    p.add_argument('--frame-max-spike', type=float, default=0.30,
                   help='per-frame isolated-spike ceiling passed to the image instrument')
    p.add_argument('--frames-out', help='keep extracted frames in this dir instead of a temp dir')
    args = p.parse_args()

    ffmpeg = _need('ffmpeg')
    ffprobe = _need('ffprobe')
    r = vet.Report()

    dur, nframes, fps = probe_duration(ffprobe, args.movie)
    if dur is None:
        r.gate('movie.readable', 'no duration', 'ffprobe-ok', False)
        sys.exit(1 if r.emit() else 0)
    dt = (dur / nframes) if (nframes and nframes > 1) else (1.0 / fps if fps else 1.0 / 30.0)
    r.info('movie.duration_s', f"{dur:.3f}")
    r.info('movie.frames', nframes if nframes is not None else '?')
    r.info('movie.fps', f"{fps:.3f}" if fps else '?')

    names = [n.strip() for n in args.samples.split(',') if n.strip()]
    specs = sample_specs(names, dur, dt)

    tmp = args.frames_out or tempfile.mkdtemp(prefix='vetmovie_')
    os.makedirs(tmp, exist_ok=True)

    frames = {}   # name -> path
    for name, spec in specs:
        fp = os.path.join(tmp, f"a_{name}.png")
        if not extract(ffmpeg, args.movie, spec, fp):
            r.gate(f'frame.{name}.extracted', 'missing', 'ok', False)
            continue
        frames[name] = fp
        verdict, failed = vet_frame(fp, args.frame_max_highband, args.frame_max_spike)
        detail = verdict + ((' [' + ','.join(failed) + ']') if failed else '')
        r.gate(f'frame.{name}.image_vet', detail, '==PASS', verdict == 'PASS')

    # SETTLE: the final frame vs settle_k frames earlier must be stable. A late
    # sequence that still moves (or a black settle-race final frame) FAILs.
    f_end = os.path.join(tmp, 'settle_end.png')
    f_prev = os.path.join(tmp, 'settle_prev.png')
    got_end = extract(ffmpeg, args.movie, ('sseof', -max(dt * 1.5, 0.1)), f_end)
    t_prev = max(0.0, dur - (args.settle_k + 1.5) * dt)
    got_prev = extract(ffmpeg, args.movie, ('ss', t_prev), f_prev)
    if got_end and got_prev:
        fa, fb = load_rgb(f_end), load_rgb(f_prev)
        if fa.shape == fb.shape:
            changed = float((np.abs(fa - fb).max(axis=2) > args.noise_delta).mean())
            r.gate('temporal.settle_changed_frac', f"{changed:.4f}", f"<{args.settle_frac:g}",
                   changed < args.settle_frac)
            r.info('temporal.settle_window', f"end~end-{args.settle_k}f")
        else:
            r.gate('temporal.settle_shape', f"{fa.shape} vs {fb.shape}", 'equal', False)
    else:
        r.gate('temporal.settle_extracted', 'missing', 'ok', False)

    # FRAME-PAIR CLASSIFICATION against a second movie
    if args.compare:
        for name, spec in specs:
            if name not in frames:
                continue
            fpb = os.path.join(tmp, f"b_{name}.png")
            if not extract(ffmpeg, args.compare, spec, fpb):
                r.gate(f'pair.{name}.extracted', 'missing', 'ok', False)
                continue
            a, b = load_rgb(frames[name]), load_rgb(fpb)
            if a.shape != b.shape:
                r.gate(f'pair.{name}.shape', f"{a.shape} vs {b.shape}", 'equal', False)
                continue
            md, npf, sf, label = classify_pair(a, b, args.noise_delta, args.struct_frac)
            r.info(f'pair.{name}.max_channel_delta', md)
            r.info(f'pair.{name}.noise_pixel_frac', f"{npf:.5f}")
            r.info(f'pair.{name}.class', label)
            r.gate(f'pair.{name}.structural_frac', f"{sf:.5f}", f"<{args.struct_frac:g}",
                   sf < args.struct_frac)

    if not args.frames_out:
        r.footer(f"frames: {tmp}")
    n_fail = r.emit()
    sys.exit(1 if n_fail else 0)


if __name__ == '__main__':
    main()
