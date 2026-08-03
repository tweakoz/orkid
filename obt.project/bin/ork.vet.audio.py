#!/bin/sh
""":"
# trampoline: exec the first python3 that has numpy (scipy optional: true-peak
# oversampling falls back to a bandlimited-interp estimate without it).
for _py in \
    "$OBT_STAGE/pyvenv/bin/python3" \
    "$HOME"/.staging*/pyvenv/bin/python3 \
    "$(command -v python3)"; do
    [ -x "$_py" ] && "$_py" -c 'import numpy' 2>/dev/null && exec "$_py" "$0" "$@"
done
echo "ork.vet.audio: no python3 with numpy found on this host" >&2
exit 3
":"""
"""
Audio vet (WAV, float or PCM): render-defect screen as porcelain.

Judges a rendered mix the way clipping/glitch defects actually manifest, and
LOCATES every anomaly by TIMESTAMP so an audible tour jumps straight to it
(the audio analogue of the image instruments' worst-region crop).

  - LEVEL: per-channel sample peak + 4x-oversampled TRUE peak (inter-sample
    overs are invisible to a sample-peak meter and are what a DAC/limiter
    actually sees), count of samples at/beyond full scale, count within 1 dB
    of FS, and consecutive-clip RUNS (a run is a flat top = real clipping;
    isolated FS samples are just a loud peak).
  - DC: per-channel mean offset (asymmetric headroom + speaker excursion).
  - BALANCE: windowed L/R RMS delta over time. A spatial walk is EXPECTED to
    be asymmetric, so this is quantified (p50/p95/max dB, dead-channel window
    count), never gated on asymmetry itself; it gates only on a channel that
    dies for a long stretch while the other plays.
  - CLICK/DISCONTINUITY, bandlimit-theoretic (not a magic delta threshold):
    a signal bandlimited to sr/2 obeys |x[n+1]-x[n]| <= 2*localpeak. slew
    ratio = |d1| / (2*localpeak) > 1 is PROVABLY not bandlimited, i.e. a
    genuine discontinuity -- which is how a click separates from a loud
    musical transient (transients are still bandlimited).
  - DROPOUT: exact-zero runs bracketed by signal (digital silence gaps).
  - DEGENERATE: all-zero / near-silent / NaN-Inf guards fail loudly.

Written excerpts (--excerpt-dir) are the located evidence: a short WAV around
each worst offender, named by check + timestamp.
"""
import sys
import os
import argparse
import struct

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _ork_vet_common as vet


# ------------------------------------------------------------------ wav io ---

def load_wav(path):
    """Minimal RIFF/WAVE reader -> (float32 [nframes, nch], sr, subfmt_str).

    Handles PCM16/24/32 and IEEE float32/64; PCM is normalized to +-1.0 full
    scale so every threshold below is in FS units regardless of container.
    """
    with open(path, 'rb') as f:
        data = f.read()
    if data[0:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError('%s: not a RIFF/WAVE file' % path)
    pos, fmt, sr, nch, bits, raw = 12, None, None, None, None, None
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        csz = struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + csz]
        if cid == b'fmt ':
            fmt, nch, sr, _br, _ba, bits = struct.unpack('<HHIIHH', body[:16])
            if fmt == 0xFFFE and csz >= 40:  # WAVE_FORMAT_EXTENSIBLE
                fmt = struct.unpack('<H', body[24:26])[0]
        elif cid == b'data':
            raw = body
        pos += 8 + csz + (csz & 1)
    if fmt is None or raw is None:
        raise ValueError('%s: missing fmt or data chunk' % path)
    if fmt == 3 and bits == 32:
        a, sub = np.frombuffer(raw, dtype='<f4').astype(np.float64), 'float32'
    elif fmt == 3 and bits == 64:
        a, sub = np.frombuffer(raw, dtype='<f8'), 'float64'
    elif fmt == 1 and bits == 16:
        a, sub = np.frombuffer(raw, dtype='<i2').astype(np.float64) / 32768.0, 'pcm16'
    elif fmt == 1 and bits == 32:
        a, sub = np.frombuffer(raw, dtype='<i4').astype(np.float64) / 2147483648.0, 'pcm32'
    elif fmt == 1 and bits == 24:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = (b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16))
        v = np.where(v & 0x800000, v - 0x1000000, v)
        a, sub = v.astype(np.float64) / 8388608.0, 'pcm24'
    else:
        raise ValueError('%s: unsupported format tag=%d bits=%s' % (path, fmt, bits))
    nfr = len(a) // nch
    return a[:nfr * nch].reshape(nfr, nch), sr, sub


def write_wav(path, x, sr):
    """float32 WAV writer (excerpt evidence; no clamping -- overs stay visible)."""
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


# ----------------------------------------------------------------- metrics ---

def db(x):
    return -np.inf if x <= 0 else 20.0 * np.log10(x)


def dbstr(x):
    return '-inf' if not np.isfinite(db(x)) else '%.2f' % db(x)


def ts(n, sr):
    """sample index -> m:ss.mmm (what the owner types into a player)."""
    t = n / float(sr)
    return '%d:%06.3f' % (int(t // 60), t % 60.0)


def true_peak(x, os_factor=4):
    """Oversampled peak: what a reconstruction filter / DAC actually sees.

    scipy polyphase resample when available; otherwise a zero-stuff + windowed
    -sinc FIR of the same order (same estimate, slower).
    """
    try:
        from scipy.signal import resample_poly
        up = resample_poly(x, os_factor, 1)
        return float(np.max(np.abs(up))), 'scipy.resample_poly x%d' % os_factor
    except Exception:
        n_taps = 32 * os_factor + 1
        t = (np.arange(n_taps) - (n_taps - 1) / 2.0) / os_factor
        h = np.sinc(t) * np.hanning(n_taps)
        h /= np.sum(h) / os_factor
        z = np.zeros(len(x) * os_factor)
        z[::os_factor] = x
        return float(np.max(np.abs(np.convolve(z, h, mode='same')))), 'sinc-FIR x%d' % os_factor


def runs_of(mask):
    """contiguous True runs -> (start, length) array, longest first."""
    if not mask.any():
        return np.zeros((0, 2), dtype=np.int64)
    d = np.diff(np.concatenate(([0], mask.view(np.int8), [0])))
    st = np.flatnonzero(d == 1)
    en = np.flatnonzero(d == -1)
    r = np.stack([st, en - st], axis=1)
    return r[np.argsort(-r[:, 1])]


def local_peak(x, half):
    """max |x| in a +-half window, via a strided max over 2*half+1 shifts
    (rolling-max without scipy; half is small so this stays cheap)."""
    a = np.abs(x)
    pad = np.pad(a, half, mode='edge')
    out = np.zeros_like(a)
    step = 8  # coarse-to-fine: max of maxima of `step`-sized blocks + exact tails
    for off in range(0, 2 * half + 1, step):
        hi = min(off + step, 2 * half + 1)
        blk = np.max(np.stack([pad[o:o + len(a)] for o in range(off, hi)]), axis=0)
        np.maximum(out, blk, out=out)
    return out


def slew_ratio(x, half=64):
    """|x[n+1]-x[n]| / (2*localpeak): >1 is not bandlimited => discontinuity."""
    d1 = np.abs(np.diff(x))
    lp = local_peak(x, half)[:len(d1)]
    lp2 = np.maximum(local_peak(x[1:], half)[:len(d1)], lp)
    den = 2.0 * lp2
    return np.where(den > 1e-9, d1 / np.maximum(den, 1e-30), 0.0)


def impulse_outliers(x, sr, thresh, abs_floor=1e-3, blk=512, onset_ratio=2.0,
                     max_cand=500):
    """Isolated-impulse (click/pop) detector: the SENSITIVE companion to slew.

    slew_ratio proves a discontinuity but only saturates for Nyquist-rate
    content, so a mid-band click hides inside its bound. Here the signal's OWN
    local curvature is the reference: ratio = |d2| / median(|d2|) in the local
    block (robust, so the outlier cannot inflate its own scale).

    A loud musical ATTACK also spikes |d2|, so every candidate is discriminated
    by energy shape: a click is ISOLATED (energy after ~= energy before), an
    onset SUSTAINS (post RMS > onset_ratio * pre RMS) and is exempted. This is
    the transient/click separation, measured rather than assumed.

    returns (worst_ratio, worst_index, n_clicks) over non-exempt candidates.
    """
    d2 = np.abs(np.diff(x, n=2))  # index i corresponds to sample i+1
    n = len(d2)
    if n < 4 * blk:
        return 0.0, 0, 0
    nb = n // blk
    bm = np.median(d2[:nb * blk].reshape(nb, blk), axis=1)
    # widen the scale to the loudest of the 3 neighbouring blocks: a quiet block
    # beside loud content must not manufacture a huge ratio.
    bm = np.maximum.reduce([bm, np.roll(bm, 1), np.roll(bm, -1)])
    scale = np.maximum(np.repeat(bm, blk), 1e-12)
    ratio = np.zeros(n)
    ratio[:nb * blk] = d2[:nb * blk] / scale
    cand = np.flatnonzero((ratio > thresh) & (d2 > abs_floor))
    if len(cand) == 0:
        return float(ratio.max()) if n else 0.0, int(np.argmax(ratio)), 0
    if len(cand) > max_cand:
        cand = cand[np.argsort(-ratio[cand])[:max_cand]]
    w = int(0.005 * sr)  # 5 ms pre/post windows, 1 ms guard around the impulse
    g = int(0.001 * sr)
    keep = []
    for i in cand:
        c = i + 1
        a0, a1 = max(0, c - g - w), max(0, c - g)
        b0, b1 = min(len(x), c + g), min(len(x), c + g + w)
        if a1 - a0 < w // 2 or b1 - b0 < w // 2:
            continue
        pre = float(np.sqrt(np.mean(x[a0:a1] ** 2)))
        post = float(np.sqrt(np.mean(x[b0:b1] ** 2)))
        if post > onset_ratio * max(pre, 1e-9):
            continue  # musical onset, not a click
        keep.append(i)
    if not keep:
        return 0.0, 0, 0
    keep = np.array(keep)
    j = keep[int(np.argmax(ratio[keep]))]
    return float(ratio[j]), int(j + 1), int(len(keep))


# -------------------------------------------------------------------- main ---

def main():
    p = argparse.ArgumentParser(description='Vet a rendered WAV; porcelain verdicts')
    p.add_argument('candidate')
    p.add_argument('--excerpt-dir', help='write located-evidence WAV excerpts here')
    p.add_argument('--excerpt-secs', type=float, default=1.0)
    p.add_argument('--fs-margin-db', type=float, default=1.0,
                   help='"near full scale" band (samples within this of FS)')
    p.add_argument('--max-clip-run', type=int, default=2,
                   help='longest tolerated consecutive at-FS run (flat top = clipping)')
    p.add_argument('--max-truepeak-db', type=float, default=0.0,
                   help='true-peak ceiling in dBFS')
    p.add_argument('--max-dc', type=float, default=0.001, help='|mean| ceiling, FS units')
    p.add_argument('--max-slew', type=float, default=1.0,
                   help='bandlimit slew ratio ceiling (>1 = provable discontinuity)')
    p.add_argument('--max-impulse', type=float, default=60.0,
                   help='isolated-impulse ceiling, multiples of local median |d2| '
                        '(musical onsets are exempted by energy shape)')
    p.add_argument('--max-dropout-ms', type=float, default=10.0,
                   help='longest tolerated exact-zero gap bracketed by signal')
    p.add_argument('--balance-win-ms', type=float, default=500.0)
    p.add_argument('--max-dead-ch-secs', type=float, default=10.0,
                   help='longest tolerated stretch of one channel >40dB under the other')
    p.add_argument('--min-rms', type=float, default=1e-4, help='degenerate/silent floor')
    args = p.parse_args()

    x, sr, sub = load_wav(args.candidate)
    nfr, nch = x.shape
    r = vet.Report()
    r.info('file', os.path.basename(args.candidate))
    r.info('format', '%s sr=%d ch=%d frames=%d dur_s=%.3f' % (sub, sr, nch, nfr, nfr / float(sr)))

    ex = []  # (label, start_sample) located evidence

    # --- degenerate / non-finite -------------------------------------------
    nnf = int(np.count_nonzero(~np.isfinite(x)))
    r.gate('nonfinite_samples', nnf, '0', nnf == 0)
    xf = np.nan_to_num(x, nan=0.0, posinf=0.0, neginf=0.0)
    rms_all = float(np.sqrt(np.mean(xf ** 2)))
    r.gate('rms_overall', '%.6f (%s dBFS)' % (rms_all, dbstr(rms_all)),
           '>%.0e' % args.min_rms, rms_all > args.min_rms)

    fs_near = 10.0 ** (-args.fs_margin_db / 20.0)
    for c in range(nch):
        ch = xf[:, c]
        nm = 'L' if c == 0 else ('R' if c == 1 else 'c%d' % c)
        pk_i = int(np.argmax(np.abs(ch)))
        pk = float(np.abs(ch[pk_i]))
        tp, tp_how = true_peak(ch)
        r.info('peak_%s' % nm, '%.6f (%s dBFS) @ %s' % (pk, dbstr(pk), ts(pk_i, sr)))
        r.gate('truepeak_%s' % nm, '%.6f (%s dBFS)' % (tp, dbstr(tp)),
               '<=%.2f dBFS' % args.max_truepeak_db,
               db(tp) <= args.max_truepeak_db)
        n_over = int(np.count_nonzero(np.abs(ch) >= 1.0))
        r.gate('samples_at_or_over_FS_%s' % nm, n_over, '0', n_over == 0)
        n_near = int(np.count_nonzero(np.abs(ch) >= fs_near))
        r.info('samples_within_%.0fdB_of_FS_%s' % (args.fs_margin_db, nm),
               '%d (%.3f ppm)' % (n_near, 1e6 * n_near / max(nfr, 1)))
        cr = runs_of(np.abs(ch) >= 0.999)
        longest = int(cr[0, 1]) if len(cr) else 0
        r.gate('clip_run_%s' % nm, '%d samples (%d runs)' % (longest, len(cr)),
               '<=%d' % args.max_clip_run, longest <= args.max_clip_run)
        if longest > args.max_clip_run:
            ex.append(('clip_%s' % nm, int(cr[0, 0])))
        dc = float(np.mean(ch))
        r.gate('dc_offset_%s' % nm, '%+.6f' % dc, '|dc|<=%.4f' % args.max_dc,
               abs(dc) <= args.max_dc)
        sl = slew_ratio(ch)
        si = int(np.argmax(sl))
        r.gate('slew_ratio_max_%s' % nm, '%.4f @ %s' % (sl[si], ts(si, sr)),
               '<=%.2f' % args.max_slew, sl[si] <= args.max_slew)
        r.info('slew_ratio_p99999_%s' % nm, '%.4f' % float(np.quantile(sl, 0.99999)))
        n_disc = int(np.count_nonzero(sl > args.max_slew))
        r.info('discontinuity_count_%s' % nm, n_disc)
        if sl[si] > args.max_slew:
            ex.append(('slew_%s' % nm, si))
        ir, ii, nclk = impulse_outliers(ch, sr, args.max_impulse)
        r.gate('impulse_outlier_%s' % nm, '%.1fx local |d2| @ %s (%d non-onset)'
               % (ir, ts(ii, sr), nclk), '<=%.0fx' % args.max_impulse,
               nclk == 0)
        if nclk:
            ex.append(('click_%s' % nm, ii))

    # --- dropouts: exact-zero gaps bracketed by signal ----------------------
    silent = np.all(xf == 0.0, axis=1)
    zr = runs_of(silent)
    min_gap = int(args.max_dropout_ms * sr / 1000.0)
    interior = [(s, l) for s, l in zr if s > 0 and s + l < nfr]
    worst = interior[0] if interior else (0, 0)
    r.gate('dropout_longest_interior_zero_run',
           '%d samples (%.2f ms) @ %s' % (worst[1], 1000.0 * worst[1] / sr, ts(worst[0], sr)),
           '<=%.1f ms' % args.max_dropout_ms, worst[1] <= min_gap)
    if worst[1] > min_gap:
        ex.append(('dropout', int(worst[0])))
    lead = int(zr[0, 1]) if len(zr) and zr[0, 0] == 0 else 0
    r.info('leading_silence', '%d samples (%.3f s)' % (lead, lead / float(sr)))

    # --- L/R balance over time (spatial asymmetry is EXPECTED: quantify) ----
    if nch >= 2:
        w = max(1, int(args.balance_win_ms * sr / 1000.0))
        nw = nfr // w
        L = xf[:nw * w, 0].reshape(nw, w)
        R = xf[:nw * w, 1].reshape(nw, w)
        rl = np.sqrt(np.mean(L ** 2, axis=1))
        rr = np.sqrt(np.mean(R ** 2, axis=1))
        live = (np.maximum(rl, rr) > args.min_rms)
        eps = 1e-12
        d = 20.0 * np.log10((rl + eps) / (rr + eps))
        dl = d[live] if live.any() else np.zeros(1)
        wi = int(np.argmax(np.abs(d) * live)) if live.any() else 0
        r.info('lr_rms_overall', 'L=%.6f R=%.6f delta=%+.2f dB'
               % (float(np.sqrt(np.mean(xf[:, 0] ** 2))), float(np.sqrt(np.mean(xf[:, 1] ** 2))),
                  db(float(np.sqrt(np.mean(xf[:, 0] ** 2)))) - db(float(np.sqrt(np.mean(xf[:, 1] ** 2))))))
        r.info('lr_win_delta_db', 'p50=%+.2f p95abs=%.2f max=%+.2f @ %s (win=%.0fms, %d live)'
               % (float(np.median(dl)), float(np.quantile(np.abs(dl), 0.95)),
                  float(d[wi]), ts(wi * w, sr), args.balance_win_ms, int(live.sum())))
        dead = runs_of(live & (np.abs(d) > 40.0))
        dead_s = (int(dead[0, 1]) * w / float(sr)) if len(dead) else 0.0
        r.gate('dead_channel_longest', '%.2f s%s'
               % (dead_s, '' if not len(dead) else ' @ ' + ts(int(dead[0, 0]) * w, sr)),
               '<=%.1f s' % args.max_dead_ch_secs, dead_s <= args.max_dead_ch_secs)
        cor = float(np.corrcoef(xf[:, 0], xf[:, 1])[0, 1]) if rms_all > 0 else 0.0
        r.info('lr_correlation', '%.4f' % cor)

    # --- located evidence ---------------------------------------------------
    if args.excerpt_dir and ex:
        os.makedirs(args.excerpt_dir, exist_ok=True)
        half = int(args.excerpt_secs * sr / 2)
        for label, n in ex[:12]:
            a, b = max(0, n - half), min(nfr, n + half)
            out = os.path.join(args.excerpt_dir, '%s_%s.wav'
                               % (label, ts(n, sr).replace(':', 'm').replace('.', 's')))
            write_wav(out, xf[a:b], sr)
            r.footer('evidence %s @ %s -> %s' % (label, ts(n, sr), out))
    elif ex:
        for label, n in ex[:12]:
            r.footer('evidence %s @ %s (pass --excerpt-dir to write a WAV)' % (label, ts(n, sr)))

    sys.exit(1 if r.emit() else 0)


if __name__ == '__main__':
    main()
