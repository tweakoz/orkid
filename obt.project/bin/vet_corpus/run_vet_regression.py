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
IMAGE = os.path.join(BIN, 'ork.vet.image.py')
HMAP = os.path.join(BIN, 'ork.vet.hmap.py')
MESH = os.path.join(BIN, 'ork.vet.mesh.py')
MOVIE = os.path.join(BIN, 'ork.vet.movie.py')


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
    missing = [t for t in (IMAGE, HMAP, MESH, MOVIE) if not os.path.exists(t)]
    if missing:
        sys.stderr.write("missing instrument(s): " + ", ".join(missing) + "\n")
        sys.exit(3)
    if not os.path.isdir(c('image')):
        sys.stderr.write(f"corpus not found under {HERE}; run make_corpus.py first\n")
        sys.exit(3)

    n_fail = 0
    for label, argv, expect in CASES:
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
    print(f"# verdict: {'FAIL' if n_fail else 'PASS'} ({len(CASES)} checks, {n_fail} failed)")
    sys.exit(1 if n_fail else 0)


if __name__ == '__main__':
    main()
