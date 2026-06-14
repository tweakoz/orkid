#!/usr/bin/env python3
"""Build ALL orkid PyPI wheels from the relocatable deploy bundle.

Usage:
    twine/build.py                      # run the deploy (phases 1-6) to a temp dir, then carve
    twine/build.py --bundle <staging>   # carve from an existing .staging tree (e.g. ~/Desktop/ORKX2/.staging)
    twine/build.py --dist <dir>         # output dir for wheels (default: <repo>/dist)

Produces <repo>/dist/*.whl :
    orkid-<ver>-py3-none-any.whl                       (umbrella: shim + entry points + Requires-Dist)
    orkid_engine/_pyext/_libdeps/_pydeps/_python/_bin  (py3-none-macosx_X_Y_arch)
    orkid_data                                         (py3-none-any)

Symlinks are preserved as symlinks (relative links resolve within orkid/
after install). The macOS platform version is read from the binaries' minos.
"""
import argparse, base64, hashlib, os, platform, re, shutil, subprocess, sys, tempfile, zipfile
import pathlib

THIS = pathlib.Path(__file__).resolve().parent
REPO = THIS.parent
sys.path.insert(0, str(THIS))
import packages as P  # noqa: E402


# ---------------------------------------------------------------- wheel writing

def _b64sha(data):
    return base64.urlsafe_b64encode(hashlib.sha256(data).digest()).rstrip(b"=").decode()

def _add(z, records, arc, *, abspath=None, data=None, mode=0o644, symlink=None):
    zi = zipfile.ZipInfo(arc)
    zi.compress_type = zipfile.ZIP_DEFLATED
    if symlink is not None:
        payload = symlink.encode()
        zi.external_attr = (0o120777 | 0o100000) << 16  # S_IFLNK
    else:
        payload = data if data is not None else pathlib.Path(abspath).read_bytes()
        # OR in S_IFREG (0o100000): pip's installer runs stat.S_ISREG(mode) before
        # honoring the exec bit, so a bare 0o755 (no type bits) installs as 0o644.
        zi.external_attr = ((0o100000 | mode) << 16)
    z.writestr(zi, payload)
    records.append((arc, _b64sha(payload), len(payload)))

def _wheel_filename(name, ver, tag):
    return f"{name.replace('-', '_')}-{ver}-{tag}.whl"

def _metadata(name, ver, summary, requires):
    L = ["Metadata-Version: 2.1", f"Name: {name}", f"Version: {ver}",
         f"Summary: {summary}", "Requires-Python: >=3.10"]
    L += [f"Requires-Dist: {r}" for r in requires]
    return "\n".join(L) + "\n"

def _wheel_meta(tag, purelib):
    return ("Wheel-Version: 1.0\nGenerator: orkid-twine-build (1.0)\n"
            f"Root-Is-Purelib: {'true' if purelib else 'false'}\n"
            f"Tag: {tag}\n")

def _entry_points(ep):
    out = []
    for section, items in ep.items():
        out.append(f"[{section}]")
        out += items
        out.append("")
    return "\n".join(out) + "\n"

def _finish(z, records, distinfo, name, ver, summary, tag, purelib,
            requires=(), entry_points=None):
    _add(z, records, f"{distinfo}/METADATA", data=_metadata(name, ver, summary, requires).encode())
    _add(z, records, f"{distinfo}/WHEEL", data=_wheel_meta(tag, purelib).encode())
    if entry_points:
        _add(z, records, f"{distinfo}/entry_points.txt", data=_entry_points(entry_points).encode())
    lines = [f"{a},sha256={h},{n}" for (a, h, n) in records]
    lines.append(f"{distinfo}/RECORD,,")
    z.writestr(f"{distinfo}/RECORD", "\n".join(lines) + "\n")


# ---------------------------------------------------------------- platform tag

def _macos_minos(staging):
    """Read the deployment-target (minos) from a representative engine dylib."""
    for cand in ("lib/libork_lev2.dylib", "lib/libork_core.dylib"):
        p = os.path.join(staging, cand)
        if not os.path.exists(p):
            continue
        out = subprocess.run(["otool", "-l", p], capture_output=True, text=True).stdout
        m = re.search(r"LC_BUILD_VERSION.*?minos (\d+)\.(\d+)", out, re.S)
        if not m:
            m = re.search(r"LC_VERSION_MIN_MACOSX.*?version (\d+)\.(\d+)", out, re.S)
        if m:
            return f"{m.group(1)}_{m.group(2)}"
    return None

def platform_tag(staging):
    """Build the macOS platform tag. pip only matches `_0`-minor macOS tags
    across majors (a 26.5 machine accepts macosx_26_0 / macosx_14_0 / macosx_11_0
    but NOT macosx_14_5), so we always emit macosx_{MAJOR}_0_{arch}.

    Default MAJOR = the build host's macOS major (the minimum we target — 26 /
    Tahoe for now). Override with MACOSX_DEPLOYMENT_TARGET to support older OSes
    (only valid if the binaries were actually built that low — minos shown below)."""
    arch = platform.machine()  # arm64 / x86_64
    dep = os.environ.get("MACOSX_DEPLOYMENT_TARGET")
    if dep:
        major = dep.split(".")[0]
    else:
        major = platform.mac_ver()[0].split(".")[0] or "26"
    minos = _macos_minos(staging)
    if minos:
        print(f"  (binaries' minos = {minos.replace('_', '.')}; tagging minimum "
              f"macOS {major}.0 — set MACOSX_DEPLOYMENT_TARGET to go lower)")
    return f"py3-none-macosx_{major}_0_{arch}"


# ---------------------------------------------------------------- bundle / carve

def make_bundle(target):
    """Run deploy phases 1-6 (relocatable, sentinelized tree; no app/dmg)."""
    deploy = REPO / "obt.project" / "bin" / "ork.deploy.macos.relocatable.py"
    for ph in ("1", "2", "3", "4", "5", "5.5", "6"):
        cmd = ["ork.python", str(deploy), "--phase", ph,
               "--target", str(target), "--project", str(REPO)]
        if ph == "1":
            cmd.append("--force")
        print(f"  [deploy] phase {ph} ...")
        subprocess.run(cmd, check=True)
    return pathlib.Path(target) / ".staging"

def walk_rels(root):
    """All staging-relative paths: regular files + symlinks (links not followed)."""
    rels = []
    def rec(d, prefix):
        for name in sorted(os.listdir(d)):
            full = os.path.join(d, name)
            rel = prefix + name
            if os.path.islink(full):
                rels.append(rel)
            elif os.path.isdir(full):
                rec(full, rel + "/")
            elif os.path.isfile(full):
                rels.append(rel)
    rec(str(root), "")
    return rels

def partition(rels):
    assigned = {p["name"]: [] for p in P.PAYLOAD_PACKAGES}
    unassigned, omitted = [], 0
    for rel in rels:
        if P.is_omitted(rel):
            omitted += 1
            continue
        for p in P.PAYLOAD_PACKAGES:
            if p["match"](rel):
                assigned[p["name"]].append(rel)
                break
        else:
            unassigned.append(rel)
    return assigned, unassigned, omitted


# ---------------------------------------------------------------- wheel builders

def build_payload_wheel(spec, staging, rels, distdir, ver, plat):
    name = spec["name"]
    tag = "py3-none-any" if spec["purelib"] else plat
    distinfo = f"{name.replace('-', '_')}-{ver}.dist-info"
    out = distdir / _wheel_filename(name, ver, tag)
    records = []
    symlinks = []   # (rel, target) — recorded in a manifest; pip can't recreate symlinks
                    # (it writes the target string as a regular file), so the launcher
                    # shim recreates them from .symlinks.d/ on first run.
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for rel in rels:
            src = os.path.join(staging, rel)
            if os.path.islink(src):
                symlinks.append((rel, os.readlink(src)))
                continue
            mode = 0o755 if os.access(src, os.X_OK) else 0o644
            _add(z, records, f"{P.BUNDLE}/{rel}", abspath=src, mode=mode)
        if symlinks:
            man = "".join(f"{r}\t{t}\n" for r, t in sorted(symlinks))
            _add(z, records, f"{P.BUNDLE}/.symlinks.d/{name.replace('-', '_')}.txt",
                 data=man.encode())
        _finish(z, records, distinfo, name, ver, spec["summary"], tag, spec["purelib"])
    print("      (%d symlinks recorded for shim restore)" % len(symlinks)) if symlinks else None
    return out

def build_umbrella_wheel(distdir, ver):
    spec = P.UMBRELLA
    tag = "py3-none-any"
    distinfo = f"{spec['name']}-{ver}.dist-info"
    out = distdir / _wheel_filename(spec["name"], ver, tag)
    shim_dir = THIS / "orkid_bootstrap"
    requires = [f"{n}=={ver}" for n in spec["requires_names"]] + spec.get("extra_requires", [])
    records = []
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for f in sorted(shim_dir.rglob("*.py")):
            arc = "orkid_bootstrap/" + str(f.relative_to(shim_dir))
            _add(z, records, arc, abspath=str(f), mode=0o644)
        _finish(z, records, distinfo, spec["name"], ver, spec["summary"], tag,
                purelib=True, requires=requires, entry_points=spec["entry_points"])
    return out


# ---------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description="Build orkid PyPI wheels from the deploy bundle.")
    ap.add_argument("--bundle", help="existing .staging tree to carve (skips the deploy)")
    ap.add_argument("--dist", default=str(REPO / "dist"), help="wheel output dir")
    ap.add_argument("--version", default=P.VERSION, help="package version")
    ap.add_argument("--keep-bundle", action="store_true", help="don't delete the temp deploy bundle")
    args = ap.parse_args()

    distdir = pathlib.Path(args.dist)
    if distdir.exists():
        shutil.rmtree(distdir)
    distdir.mkdir(parents=True)

    tmp = None
    if args.bundle:
        staging = pathlib.Path(args.bundle)
        if not staging.exists():
            sys.exit(f"ERROR: bundle not found: {staging}")
    else:
        tmp = tempfile.mkdtemp(prefix="orkid_wheelbuild_")
        print(f"Building relocatable bundle in {tmp} (deploy phases 1-6, --project {REPO}) ...")
        staging = make_bundle(tmp)

    plat = platform_tag(str(staging))
    print(f"\nPlatform tag for binary wheels: {plat}")
    print(f"Carving {staging}\n")

    rels = walk_rels(staging)
    assigned, unassigned, omitted = partition(rels)

    print(f"  {len(rels)} entries  ({omitted} omitted: obt_venv/caches)")
    if unassigned:
        print(f"\n  WARNING: {len(unassigned)} UNASSIGNED entries (not in any package!):")
        for u in unassigned[:40]:
            print(f"    {u}")
        print("  ^ fix packages.py predicates before publishing.\n")

    built = []
    for spec in P.PAYLOAD_PACKAGES:
        rels_p = assigned[spec["name"]]
        if not rels_p:
            print(f"  SKIP {spec['name']} (no files matched)")
            continue
        whl = build_payload_wheel(spec, staging, rels_p, distdir, args.version, plat)
        mb = whl.stat().st_size / (1024 * 1024)
        print(f"  {spec['name']:16s} {len(rels_p):6d} files  ->  {whl.name}  ({mb:.1f} MB)")
        built.append(whl)

    whl = build_umbrella_wheel(distdir, args.version)
    print(f"  {'orkid (umbrella)':16s}              ->  {whl.name}  ({whl.stat().st_size/1024:.0f} KB)")
    built.append(whl)

    if tmp and not args.keep_bundle:
        shutil.rmtree(tmp, ignore_errors=True)

    total = sum(w.stat().st_size for w in built) / (1024 * 1024)
    print(f"\nBuilt {len(built)} wheels in {distdir}  (total {total:.0f} MB)")
    # optional validation
    if shutil.which("twine"):
        print("\ntwine check:")
        subprocess.run(["twine", "check", *[str(w) for w in built]])
    else:
        print("\n(twine not on PATH — skipping `twine check`)")


if __name__ == "__main__":
    main()
