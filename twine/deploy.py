#!/usr/bin/env python3
"""Upload the locally-built orkid wheels (dist/*.whl) to PyPI, ONE AT A TIME with
a delay between each (eases PyPI's new-project-creation throttle).

Delay is conditional: 30s after a real upload (the rate-limited action), 5s after
a skip (already published). Build first with twine/build.py. Does NOT build and
does NOT git clean.
"""
import pathlib
import subprocess
import sys
import time

THIS = pathlib.Path(__file__).resolve().parent
REPO = THIS.parent
DIST = REPO / "dist"
DELAY = 15        # seconds after an actual upload
SKIP_DELAY = 5    # seconds after a skip (already on PyPI)


def _on_pypi(name, version):
    """True if <name>==<version> is already published on PyPI.

    Uses curl (system certs) rather than python urllib — the shell's interpreter
    here often lacks SSL certs, which would make urllib fail for EVERY wheel and
    misclassify live ones as uploads (→ wrong 30s delay)."""
    url = f"https://pypi.org/pypi/{name}/{version}/json"
    try:
        code = subprocess.run(
            ["curl", "-s", "-o", "/dev/null", "-w", "%{http_code}", url],
            capture_output=True, text=True, timeout=20).stdout.strip()
        return code == "200"
    except Exception:
        return False


def main():
    wheels = sorted(DIST.glob("orkid*.whl"))
    if not wheels:
        sys.exit("No orkid wheels in %s — run twine/build.py first." % DIST)

    r = subprocess.run(["git", "status", "--porcelain"],
                       capture_output=True, text=True, cwd=str(REPO))
    if r.stdout.strip():
        print("WARNING: git repo is not clean:\n" + r.stdout)
        if input("Upload anyway? (y/N): ").strip().lower() != "y":
            sys.exit("aborted")

    subprocess.run(["twine", "check", *map(str, wheels)], check=True)

    # Determine which are already published (decides skip vs upload + the delay).
    status = []
    for w in wheels:
        parts = w.name.split("-")                 # {name}-{version}-{tags...}.whl
        name, version = parts[0].replace("_", "-"), parts[1]
        status.append((w, _on_pypi(name, version)))

    n_up = sum(1 for _, live in status if not live)
    if n_up == 0:
        print("\nAll wheels already on PyPI — nothing to upload.")
        return

    print("\n%d to upload, %d already live (upload=%ds apart, skip=%ds):"
          % (n_up, len(status) - n_up, DELAY, SKIP_DELAY))
    for w, live in status:
        print("   %-55s %s" % (w.name, "(live, skip)" if live else "UPLOAD"))
    if input("\nProceed? (y/N): ").strip().lower() != "y":
        sys.exit("aborted")

    failed = []
    for i, (w, live) in enumerate(status):
        if live:
            print("\n[%d/%d] skip (already on PyPI): %s" % (i + 1, len(status), w.name))
        else:
            print("\n[%d/%d] uploading: %s" % (i + 1, len(status), w.name))
            rc = subprocess.run(
                ["twine", "upload", "--verbose", "--skip-existing", str(w)]).returncode
            if rc != 0:
                failed.append(w.name)
                print("  ^ failed (rc=%d) — likely the new-project throttle; retry later" % rc)
        if i < len(status) - 1:
            d = SKIP_DELAY if live else DELAY
            print("   ... waiting %ds ..." % d)
            time.sleep(d)

    if failed:
        print("\nNot uploaded (re-run later; already-done ones are skipped):")
        for f in failed:
            print("   %s" % f)
        sys.exit(1)
    print("\nAll wheels uploaded.")


if __name__ == "__main__":
    main()
