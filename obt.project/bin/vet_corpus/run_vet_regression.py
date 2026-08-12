#!/bin/sh
""":"
# trampoline: any python3 works (this runner is stdlib-only; it subprocesses the instruments).
for _py in \
    "$OBT_STAGE/pyvenv/bin/python3" \
    "$HOME"/.staging*/pyvenv/bin/python3 \
    "$(command -v python3)"; do
    [ -x "$_py" ] && exec "$_py" "$0" "$@"
done
echo "run_vet_regression: no python3 found on this host" >&2
exit 3
":"""
"""
ork.vet.* instrument regression suite.

Runs every instrument over the synthetic corpus (make_corpus.py) and asserts:
CLEAN artifacts PASS, seeded MUTANT twins FAIL. Emits the same porcelain
contract as the instruments so a gate/agent quotes ONE verdict for the whole
toolset. Exit 0 iff every case matched its expectation.
"""
import os
import sys
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.dirname(HERE)
MAKE = os.path.join(HERE, 'make_corpus.py')
IMAGE = os.path.join(BIN, 'ork.vet.image.py')
HMAP = os.path.join(BIN, 'ork.vet.hmap.py')
MESH = os.path.join(BIN, 'ork.vet.mesh.py')
MOVIE = os.path.join(BIN, 'ork.vet.movie.py')
AUDIO = os.path.join(BIN, 'ork.vet.audio.py')

# corpus types whose artifacts are NEVER tracked in git: regenerated here before
# their cases run, so a fresh checkout needs no manual make_corpus.py step. The
# tracked types (mesh/movie) are deliberately absent -- their committed binaries
# must stay byte-stable, so this suite never regenerates them.
GENERATED = ('audio', 'foliage', 'atlas')


def c(*parts):
    return os.path.join(HERE, *parts)


# (label, argv, expected_verdict)
CASES = [
    ('image.clean',        [IMAGE, c('image', 'clean.png'), '--kind', 'render'], 'PASS'),
    ('image.clean.golden', [IMAGE, c('image', 'clean.png'), '--kind', 'render',
                            '--golden', c('image', 'clean.png')], 'PASS'),
    ('image.mutant',       [IMAGE, c('image', 'mutant.png'), '--kind', 'render'], 'FAIL'),
    ('image.mutant.golden', [IMAGE, c('image', 'mutant.png'), '--kind', 'render',
                             '--golden', c('image', 'clean.png')], 'FAIL'),
    ('image.black',        [IMAGE, c('image', 'black.png'), '--kind', 'render'], 'FAIL'),

    # render discriminating checks: a DETAILED WARM render (glints + material
    # detail + golden-hour tint) must PASS; each defect twin must FAIL its check.
    ('render.clean',       [IMAGE, c('image', 'render_clean.png'), '--kind', 'render'], 'PASS'),
    ('render.firefly',     [IMAGE, c('image', 'render_firefly.png'), '--kind', 'render'], 'FAIL'),
    ('render.speckle',     [IMAGE, c('image', 'render_speckle.png'), '--kind', 'render'], 'FAIL'),
    ('render.grossspeckle', [IMAGE, c('image', 'render_grossspeckle.png'), '--kind', 'render'], 'FAIL'),
    ('render.magenta',     [IMAGE, c('image', 'render_magenta.png'), '--kind', 'render'], 'FAIL'),

    # foliage: the clean twin carries a bright SKY BAND, sky seen through a
    # trunk GAP, and real specular GLINTS -- all bright, all desaturated, none
    # of them cotton. It must PASS while each whitening twin FAILs. The glint
    # and puff stamps share placement/footprint/peak: only the radial profile
    # differs, so the separation is the signature, not the brightness.
    ('foliage.clean',      [IMAGE, c('foliage', 'clean.png'), '--kind', 'foliage'], 'PASS'),
    ('foliage.clean.region', [IMAGE, c('foliage', 'clean.png'), '--kind', 'foliage',
                              '--region', '0,205,282,512'], 'PASS'),
    ('foliage.puffs',      [IMAGE, c('foliage', 'cotton_puffs.png'), '--kind', 'foliage'], 'FAIL'),
    ('foliage.puffs.region', [IMAGE, c('foliage', 'cotton_puffs.png'), '--kind', 'foliage',
                              '--region', '0,205,282,512'], 'FAIL'),
    ('foliage.wholecanopy', [IMAGE, c('foliage', 'cotton_wholecanopy.png'),
                             '--kind', 'foliage'], 'FAIL'),

    # impostor atlas: one mutant per check. whitebg -> bg_excess_lum,
    # specklebg -> bg_bright_frac (a MEAN cannot see it), blackbg -> mip_drift
    # (its background is dark, so both bleed checks pass and only the mip chain
    # shows the defect).
    ('atlas.clean',        [IMAGE, c('atlas', 'clean.png'), '--kind', 'impostor-atlas'], 'PASS'),
    ('atlas.whitebg',      [IMAGE, c('atlas', 'whitebg.png'), '--kind', 'impostor-atlas'], 'FAIL'),
    ('atlas.specklebg',    [IMAGE, c('atlas', 'specklebg.png'), '--kind', 'impostor-atlas'], 'FAIL'),
    ('atlas.blackbg',      [IMAGE, c('atlas', 'blackbg.png'), '--kind', 'impostor-atlas'], 'FAIL'),

    ('hmap.clean',         [HMAP, c('hmap', 'clean.png')], 'PASS'),
    ('hmap.clean.golden',  [HMAP, c('hmap', 'clean.png'), '--golden', c('hmap', 'clean.png')], 'PASS'),
    ('hmap.mutant',        [HMAP, c('hmap', 'mutant.png')], 'FAIL'),

    ('mesh.clean',         [MESH, c('mesh', 'clean.obj')], 'PASS'),
    ('mesh.mutant',        [MESH, c('mesh', 'mutant.obj')], 'FAIL'),

    ('movie.clean',        [MOVIE, c('movie', 'clean.mkv')], 'PASS'),
    ('movie.mutant',       [MOVIE, c('movie', 'mutant.mkv')], 'FAIL'),
    ('movie.ab_noise',     [MOVIE, c('movie', 'clean.mkv'),
                            '--compare', c('movie', 'clean_noise.mkv')], 'PASS'),
    ('movie.ab_structural', [MOVIE, c('movie', 'clean.mkv'),
                             '--compare', c('movie', 'structural.mkv')], 'FAIL'),

    # audio: the clean twin carries HARD MUSICAL ONSETS on purpose -- it proves
    # the click check's transient discrimination while the mutants prove its
    # sensitivity. deadch tolerance is tightened to 2s for a 4s corpus file.
    ('audio.clean',        [AUDIO, c('audio', 'clean.wav'), '--max-dead-ch-secs', '2'], 'PASS'),
    ('audio.clip',         [AUDIO, c('audio', 'clip.wav'), '--max-dead-ch-secs', '2'], 'FAIL'),
    ('audio.click',        [AUDIO, c('audio', 'click.wav'), '--max-dead-ch-secs', '2'], 'FAIL'),
    ('audio.dropout',      [AUDIO, c('audio', 'dropout.wav'), '--max-dead-ch-secs', '2'], 'FAIL'),
    ('audio.dc',           [AUDIO, c('audio', 'dc.wav'), '--max-dead-ch-secs', '2'], 'FAIL'),
    ('audio.deadch',       [AUDIO, c('audio', 'deadch.wav'), '--max-dead-ch-secs', '2'], 'FAIL'),
    ('audio.intersample',  [AUDIO, c('audio', 'intersample.wav'), '--max-dead-ch-secs', '2'], 'FAIL'),
]


def run_case(argv):
    r = subprocess.run(argv, capture_output=True, text=True)
    verdict = None
    for line in r.stdout.splitlines():
        if line.startswith('# verdict:'):
            verdict = 'FAIL' if 'FAIL' in line else 'PASS'
    observed = 'FAIL' if r.returncode == 1 else ('PASS' if r.returncode == 0 else f'ERR{r.returncode}')
    # rc is authoritative; note any disagreement with the printed footer
    if verdict and verdict != observed and observed in ('PASS', 'FAIL'):
        observed = f'{observed}!={verdict}'
    return observed, r


def main():
    # optional label-prefix filter: `run_vet_regression.py audio` runs one
    # instrument's cases (delta gate) without needing the other corpora present.
    want = [a for a in sys.argv[1:] if not a.startswith('-')]
    cases = [c_ for c_ in CASES
             if not want or any(c_[0].startswith(w) for w in want)]
    if not cases:
        sys.stderr.write("no cases match %r\n" % (want,))
        sys.exit(3)

    missing = [t for t in {c_[1][0] for c_ in cases} if not os.path.exists(t)]
    if missing:
        sys.stderr.write("missing instrument(s): " + ", ".join(missing) + "\n")
        sys.exit(3)
    regen = sorted({c_[0].split('.')[0] for c_ in cases} & set(GENERATED))
    if regen:
        r = subprocess.run([MAKE] + regen, capture_output=True, text=True)
        if r.returncode != 0:
            last = (r.stderr.strip() or r.stdout.strip()).splitlines()
            sys.stderr.write("corpus generation failed (%s): %s\n"
                             % (" ".join(regen), last[-1] if last else 'rc %d' % r.returncode))
            sys.exit(3)

    absent = sorted({os.path.dirname(a) for c_ in cases for a in c_[1][1:]
                     if a.endswith(('.png', '.obj', '.mkv', '.wav')) and not os.path.exists(a)})
    if absent:
        sys.stderr.write("corpus missing under %s; run make_corpus.py %s first\n"
                         % (HERE, " ".join(os.path.basename(p) for p in absent)))
        sys.exit(3)

    n_fail = 0
    for label, argv, expect in cases:
        observed, r = run_case(argv)
        ok = observed == expect
        if not ok:
            n_fail += 1
        print(f"{label}\t{observed}\t{expect}\t{'PASS' if ok else 'FAIL'}")
        if not ok:
            tail = "\n".join(r.stdout.strip().splitlines()[-4:])
            for ln in tail.splitlines():
                print(f"#   {label}: {ln}")
            if r.stderr.strip():
                print(f"#   {label}: stderr {r.stderr.strip().splitlines()[-1]}")
    print(f"# verdict: {'FAIL' if n_fail else 'PASS'} ({len(cases)} checks, {n_fail} failed)")
    sys.exit(1 if n_fail else 0)


if __name__ == '__main__':
    main()
