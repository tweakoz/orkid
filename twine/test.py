#!/usr/bin/env python3
"""Install the locally-built orkid wheels into the CURRENT venv.

Assumes you are in an activated venv (Python >=3.10 — ork.build requires it).
orkid-* wheels come from the local dist/ (never pushed to PyPI); the dev-pinned
dependency ork.build==0.0.303.dev18 (+ its deps) comes from PyPI.

    twine/build.py      # build the wheels first
    twine/test.py       # <- run this from inside your test venv
    # then, from that venv:
    ork.python -c "from orkengine import lev2; print('ok')"
    ork.shell
"""
import pathlib
import subprocess
import sys

THIS = pathlib.Path(__file__).resolve().parent
REPO = THIS.parent
DIST = REPO / "dist"


def main():
    wheels = sorted(DIST.glob("orkid*.whl"))
    if not wheels:
        sys.exit("No orkid wheels in %s — run twine/build.py first." % DIST)

    if sys.version_info[:2] < (3, 10):
        sys.exit("This venv is Python %d.%d; need >=3.10 (ork.build requires it)."
                 % sys.version_info[:2])

    if sys.prefix == sys.base_prefix:
        print("WARNING: this does not look like a venv (installing into %s)." % sys.prefix)

    print("Installing orkid into: %s  (py %d.%d)\n"
          % (sys.prefix, *sys.version_info[:2]))
    subprocess.run(
        [sys.executable, "-m", "pip", "install", "--force-reinstall",
         "--find-links", str(DIST), "orkid"],
        check=True)

    print("\nInstalled. From this venv:")
    print("  ork.python -c 'from orkengine import lev2; print(\"ok\")'")
    print("  ork.shell")


if __name__ == "__main__":
    main()
