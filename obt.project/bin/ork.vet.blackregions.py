#!/bin/sh
""":"
# trampoline: exec the first python3 that has numpy+PIL+scipy (blackregions vet
# needs connected-component labeling from scipy.ndimage).
for _py in \
    "$OBT_STAGE/pyvenv/bin/python3" \
    "$HOME"/.staging*/pyvenv/bin/python3 \
    "$(command -v python3)"; do
    [ -x "$_py" ] && "$_py" -c 'import numpy, PIL, scipy' 2>/dev/null && exec "$_py" "$0" "$@"
done
echo "ork.vet.blackregions: no python3 with numpy+PIL+scipy found on this host" >&2
exit 3
":"""
"""
Black-region vet: mechanical detection of coherent black patches that are
MISSING ALBEDO (a material that failed to bind / a bake that came back empty),
NOT legitimate shadow or a dark backdrop. Porcelain verdicts for agent
consumption (one check per line, verdict-last, exit code follows verdict).

Proven in anger on the jul24 swest characterization: a stored (baked) render
came back with buildings rendering pure black where the on-the-fly procedural
twin had lit adobe/roof/timber material -- 13.85% of the frame. The eye would
call it "dark"; the instrument LOCATES it (connected-component families, area
ranked, magenta overlay) and QUANTIFIES it (coherent-black fraction).

TWO MODES:
  TWIN (--reference REF)   the trustworthy mode. missing-albedo =
      black-in-candidate INTERSECT lit-in-reference. A region only fires when
      the SAME camera's reference twin clearly has lit material there, so
      legitimate shadow / dark timber (dark in BOTH) is excluded by
      construction, and the sky (bright in both) never fires. Use this whenever
      a blessed / cross-seed / pre-change twin exists.

  GOLDENLESS (no --reference)  the no-twin fallback. coherent-black on
      NON-BACKGROUND geometry, where the background color is estimated from the
      border ring (the turntable idiom: subject centered on a solid backdrop).
      Black that matches the estimated backdrop is exempted; black that sits on
      geometry fires. HONEST LIMITS (see --help epilog): a legitimately dark
      scene false-arms, and a DARK backdrop makes missing-albedo black
      indistinguishable from the backdrop (a low-confidence WARN is emitted).
      Prefer TWIN mode whenever a twin exists.

SCOPE: LDR PNG (0..1 after 8/16-bit normalization). The near-black BLK ceiling
and lit LIT floor are LDR-tonemapped thresholds; this instrument is NOT
HDR/EXR-scoped today (an HDR frame's black point and lit range live on a
different scale -- feed it a tonemapped LDR capture).
"""
import sys
import os
import argparse
from pathlib import Path

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _ork_vet_common as vet
from scipy import ndimage


def _rank_families(mask, H, W, cand, Lcand, ref=None, Lref=None):
    """Connected-component label a coherent-black mask -> area-ranked families.

    Returns (families, total_px) where families is a list sorted by area desc,
    each a dict with area/frac/centroid/bbox and per-family context:
      - TWIN: refL (reference luma there = what SHOULD have been lit) and
        warm = mean(R-B) of the reference region (adobe/roof warmth cue).
      - GOLDENLESS: candL (candidate luma, ~0 by construction) and warm of the
        candidate's immediate context is not meaningful, so warm reports the
        reference-less 0.0 sentinel.
    """
    lab, n = ndimage.label(mask)
    fam = []
    for lb in range(1, n + 1):
        m = lab == lb
        area = int(m.sum())
        fam.append((area, lb, m))
    fam.sort(reverse=True, key=lambda t: t[0])
    out = []
    for area, lb, m in fam:
        ys, xs = np.where(m)
        cy, cx = int(ys.mean()), int(xs.mean())
        y0, y1, x0, x1 = int(ys.min()), int(ys.max()), int(xs.min()), int(xs.max())
        if ref is not None:
            rl = float(Lref[m].mean())
            rr = ref[m]
            warm = float((rr[:, 0] - rr[:, 2]).mean())
        else:
            rl = float(Lcand[m].mean())
            warm = 0.0
        out.append(dict(area=area, frac=area / (H * W), cy=cy, cx=cx,
                        bbox=(y0, y1, x0, x1), refL=rl, warm=warm))
    return out, int(mask.sum())


def _write_overlay(path, cand, mask):
    from PIL import Image
    ov = (np.clip(cand, 0, 1) * 255).astype(np.uint8).copy()
    ov[mask] = [255, 0, 255]
    Image.fromarray(ov).save(path)


def _emit_families(r, keep, mina):
    r.info('blackregions.families', f"{len(keep)} (kept area>={mina}px)")
    for i, f in enumerate(keep[:15]):
        y0, y1, x0, x1 = f['bbox']
        val = (f"area={f['area']} frac={100 * f['frac']:.3f}% "
               f"cy={f['cy']} cx={f['cx']} bbox={y0}:{y1},{x0}:{x1} "
               f"refL={f['refL']:.3f} warm={f['warm']:+.3f}")
        r.info(f"blackregions.family.F{i:02d}", val)


def _finish(r, args, cand, mask, fams, H, W):
    """Shared tail: min-area filter, gate on coherent-black fraction, families,
    overlay, verdict, and the BLACKREGIONS_RESULT porcelain token."""
    keep = [f for f in fams if f['area'] >= args.min_area]
    tot = int(mask.sum())
    frac = tot / (H * W)
    r.gate('blackregions.black_frac', f"{frac:.5f}", f"<{args.max_frac:g}",
           frac < args.max_frac)
    _emit_families(r, keep, args.min_area)

    if args.overlay_out:
        _write_overlay(args.overlay_out, cand, mask)

    n_fail = r.emit()
    if args.overlay_out:
        print(f"# overlay -> {args.overlay_out}")
    if keep:
        w = keep[0]
        worst = f"{w['area']}@{w['cy']},{w['cx']}"
    else:
        worst = "0@-,-"
    verdict = 'FAIL' if n_fail else 'PASS'
    print(f"BLACKREGIONS_RESULT={verdict} frac={frac:.5f} "
          f"families={len(keep)} worst={worst}")
    return n_fail


def _twin(args, r):
    cand = vet.load_rgb(args.candidate)
    ref = vet.load_rgb(args.reference)
    if cand.shape != ref.shape:
        r.gate('blackregions.shape', f"{cand.shape} vs {ref.shape}", 'equal', False)
        n = r.emit()
        print(f"BLACKREGIONS_RESULT=FAIL frac=nan families=0 worst=0@-,-")
        return n
    H, W, _ = cand.shape
    print(f"# ork.vet.blackregions  mode<twin>  {W}x{H}  "
          f"cand<{os.path.basename(args.candidate)}> ref<{os.path.basename(args.reference)}>  "
          f"BLK<{args.black_ceil}> LIT<{args.lit_floor}> DEL<{args.delta_floor}> "
          f"MINA<{args.min_area}> MAXFRAC<{args.max_frac}>")
    Lc, Lr = vet.luma(cand), vet.luma(ref)

    # alignment sanity: correlation on the shared bright background (sky+terrain).
    # A misaligned twin manufactures phantom missing-albedo; WARN (advisory) so
    # the operator knows the comparison basis, without gating on content that
    # legitimately differs between candidate and reference.
    bg = (Lc > 0.25) & (Lr > 0.25)
    align = float(np.corrcoef(Lc[bg], Lr[bg])[0, 1]) if bg.sum() > 1000 else float('nan')
    r.gate('blackregions.align_bgcorr', f"{align:.4f}", f">{args.min_align:g}",
           not (align == align) or align > args.min_align, warn_only=True)

    missing = (Lc < args.black_ceil) & (Lr > args.lit_floor) & \
              ((Lr - Lc) > args.delta_floor)
    missing = ndimage.binary_opening(missing, iterations=1)
    fams, _ = _rank_families(missing, H, W, cand, Lc, ref=ref, Lref=Lr)
    return _finish(r, args, cand, missing, fams, H, W)


def _goldenless(args, r):
    cand = vet.load_rgb(args.candidate)
    H, W, _ = cand.shape
    print(f"# ork.vet.blackregions  mode<goldenless>  {W}x{H}  "
          f"cand<{os.path.basename(args.candidate)}>  "
          f"BLK<{args.black_ceil}> BGTOL<{args.bg_tol}> RING<{args.bg_ring}> "
          f"MINA<{args.min_area}> MAXFRAC<{args.max_frac}>")
    L = vet.luma(cand)

    # background estimate: median color of the border ring (turntable idiom).
    ring = args.bg_ring
    bm = np.zeros((H, W), bool)
    bm[:ring, :] = bm[-ring:, :] = bm[:, :ring] = bm[:, -ring:] = True
    bg = np.median(cand[bm], axis=0)
    bg_l = float(vet.luma(bg[None, :])[0])
    # a pixel is background if all channels are within tolerance of the estimate.
    d = np.max(np.abs(cand - bg[None, None, :]), axis=2)
    bgm = d < args.bg_tol

    # DARK BACKDROP HONESTY: if the estimated backdrop is itself near-black, a
    # missing-albedo black region is indistinguishable from the backdrop (both
    # get exempted). Report a WARN so a PASS here is not read as high-confidence.
    r.gate('blackregions.bg_luma', f"{bg_l:.4f}", f">{args.dark_bg:g}",
           bg_l > args.dark_bg, warn_only=True)

    black = L < args.black_ceil
    suspect = black & ~bgm
    suspect = ndimage.binary_opening(suspect, iterations=1)
    fams, _ = _rank_families(suspect, H, W, cand, L, ref=None, Lref=None)
    return _finish(r, args, cand, suspect, fams, H, W)


def main():
    p = argparse.ArgumentParser(
        description='Vet a render for coherent missing-albedo black regions; porcelain verdicts',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""\
MODES
  twin (--reference REF): the trustworthy mode. Fires only where the candidate
    is near-black AND the same-camera reference is clearly lit (delta > floor),
    so legit shadow/dark-timber (dark in both) and sky (bright in both) are
    excluded by construction. Prefer this whenever a twin exists.
  goldenless (no --reference): fallback. coherent-black on non-background
    geometry, background estimated from the border ring.
    LIMITS -- documented honestly:
      * a legitimately dark scene (night, black-painted subject) FALSE-ARMS.
      * a DARK backdrop (bg_luma < --dark-bg) makes missing-albedo black
        indistinguishable from the backdrop -> a WARN fires and the PASS is
        LOW-CONFIDENCE. Re-run in twin mode against any available twin.

SCOPE: LDR PNG only. BLK/LIT are LDR-tonemapped thresholds; NOT HDR/EXR-scoped
today (feed a tonemapped LDR capture, not a linear HDR frame).

CALIBRATION (jul24 swest corpus, --max-frac 0.01 default):
  DIRTY  stored_spawn      twin frac 0.13624 / goldenless 0.13624  -> FAIL
  INTER  stored_spawn_f180 twin frac 0.02801 / goldenless 0.02801  -> FAIL
  CLEAN  proc_spawn (self/goldenless bright-bg)                    -> PASS (0)
  CLEAN  pueblo_v4_stored (goldenless, BLACK backdrop)  -> PASS + dark-bg WARN

PORCELAIN: standard <check>\\t<value>\\t<threshold>\\t<PASS|FAIL|WARN|INFO>
lines + '# verdict:' + a final BLACKREGIONS_RESULT=<PASS|FAIL> frac=<x>
families=<n> worst=<area>@<cy>,<cx> token. Exit code follows the verdict.
""")
    p.add_argument('candidate', help='render PNG to vet')
    p.add_argument('--reference', help='same-camera twin (blessed/cross-seed/pre-change) '
                                       '-> TWIN mode; omit for GOLDENLESS mode')
    p.add_argument('--overlay-out', help='write a magenta black-region overlay PNG here')
    p.add_argument('--max-frac', type=float, default=0.01,
                   help='gate: max coherent-black fraction (default 0.01; the jul24 '
                        'swest dirty 13.6%% and 3.12%% mutants both FAIL, clean PASSes)')
    p.add_argument('--min-area', type=int, default=400,
                   help='min connected-component area in px to count as a family (default 400)')
    p.add_argument('--black-ceil', type=float, default=0.06,
                   help='near-black luma ceiling (candidate) (default 0.06)')
    p.add_argument('--lit-floor', type=float, default=0.18,
                   help='twin: reference "has lit material" luma floor (default 0.18)')
    p.add_argument('--delta-floor', type=float, default=0.12,
                   help='twin: reference-minus-candidate luma delta floor (default 0.12)')
    p.add_argument('--min-align', type=float, default=0.90,
                   help='twin: background-correlation WARN floor (misalignment guard, default 0.90)')
    p.add_argument('--bg-tol', type=float, default=0.10,
                   help='goldenless: per-channel tolerance for background membership (default 0.10)')
    p.add_argument('--bg-ring', type=int, default=8,
                   help='goldenless: border ring width in px for background estimation (default 8)')
    p.add_argument('--dark-bg', type=float, default=0.12,
                   help='goldenless: bg_luma below this -> dark-backdrop WARN (low-confidence, default 0.12)')
    args = p.parse_args()

    r = vet.Report()
    if args.reference:
        n_fail = _twin(args, r)
    else:
        n_fail = _goldenless(args, r)
    sys.exit(1 if n_fail else 0)


if __name__ == '__main__':
    main()
