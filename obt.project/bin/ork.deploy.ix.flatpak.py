#!/usr/bin/env python3
###############################################################################
# ork.deploy.ix.flatpak.py
#
# Build a relocatable Orkid Flatpak (.flatpak) for Linux x86_64. ("ix" = *nix.)
#
# This is the Linux analog of the macOS .app/.dmg packaging (deploy phases 7/8):
# it takes the self-contained, $ORIGIN-relocatable staging tree produced by
# deploy phases 1-6 (ork.deploy.linux.relocatable.py) and wraps it into a
# Flatpak.
#
# Pipeline:
#   1. Build (or reuse via --bundle) the relocatable .staging tree.
#   2. Generate the flatpak build inputs: a JSON manifest (buildsystem: simple),
#      a .desktop file, an AppStream metainfo.xml, per-app launcher wrappers,
#      and the app icon.
#   3. flatpak-builder copies the tree wholesale into /app/orkid and BAKES the
#      sentinel path-fixup (__OBT_DEPLOY_SENTINEL__ -> /app/orkid) at build time,
#      because /app is READ-ONLY at runtime (unlike a macOS .app the launcher can
#      write into on first run).
#   4. Export to a repo, produce a single-file <app-id>.flatpak bundle, and
#      optionally install it --user.
#
# Requires: flatpak, flatpak-builder, and org.freedesktop.{Platform,Sdk}//<ver>
# installed (--user is fine). On Ubuntu:
#   sudo apt install -y flatpak flatpak-builder
#   flatpak remote-add --if-not-exists --user flathub \
#       https://dl.flathub.org/repo/flathub.flatpakrepo
#   flatpak install --user -y flathub \
#       org.freedesktop.Platform//25.08 org.freedesktop.Sdk//25.08
###############################################################################

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

APP_ID = "com.tweakoz.Orkid"
RUNTIME = "org.freedesktop.Platform"
SDK = "org.freedesktop.Sdk"
DEFAULT_RUNTIME_VERSION = "25.08"   # matches the installed GL.default / nvidia ext here
INSTALL_SUBDIR = "orkid"            # the tree lands at /app/orkid
APP_ROOT = "/app/" + INSTALL_SUBDIR

# Sandbox permissions for a realtime GPU + audio + windowed engine.
FINISH_ARGS = [
    "--device=dri",                       # GPU / DRI render nodes (Mesa + host GL/Vulkan)
    "--socket=wayland",
    "--socket=fallback-x11",
    "--share=ipc",                        # X11 MIT-SHM / shared buffers
    "--socket=pulseaudio",                # PulseAudio + PipeWire pulse shim
    "--filesystem=xdg-run/pipewire-0",    # native PipeWire
    "--share=network",                    # asset CDN / catalog
    "--persist=.obt-global",              # writable, persisted caches across runs
    "--filesystem=home",                  # v1: broad; narrow to a projects dir later
    # git is a build-time tool absent from the runtime; GitPython hard-fails at
    # import without it. A runtime deploy doesn't run git, so silence it.
    "--env=GIT_PYTHON_REFRESH=quiet",
]

# macOS DEPLOY_CONFIG["apps"] -> /app/bin wrappers. Flatpak has ONE primary
# command; the rest are reachable via `flatpak run --command=<name>`.
APPS = [
    {"name": "orkid-testrunner",   "command": ["ork.app.testrunner.py"], "mode": "gui",      "primary": True},
    {"name": "orkid-launch-shell", "command": [],                        "mode": "terminal", "primary": False},
]
PRIMARY_COMMAND = next(a["name"] for a in APPS if a.get("primary"))

ICON_SEARCH = ["ork.data/misc/OrkidLogo.png"]


def sh(cmd, **kw):
    print("  $ " + " ".join(str(c) for c in cmd))
    return subprocess.run([str(c) for c in cmd], check=True, **kw)


def _which_or_die(name, hint):
    if shutil.which(name) is None:
        sys.exit(f"ERROR: '{name}' not found on PATH. {hint}")


def build_staging_tree(workdir, staging, obt_venv, projects):
    """Run deploy phases 1-6 to produce the relocatable .staging tree."""
    deploy = Path(__file__).resolve().parent / "ork.deploy.linux.relocatable.py"
    target = Path(workdir) / "deploy"
    cmd = ["ork.python", str(deploy), "--phase", "all",
           "--staging", str(staging), "--target", str(target), "--force"]
    if obt_venv:
        cmd += ["--obt-venv", str(obt_venv)]
    for p in projects:
        cmd += ["--project", str(p)]
    sh(cmd)
    tree = target / ".staging"
    if not tree.exists():
        sys.exit(f"ERROR: expected relocatable tree not found: {tree}")
    return tree


def prepare_icon(src, dest, size=512):
    """Produce a square `size`x`size` PNG (flatpak requires square icons that
    fit the hicolor dir). Fits the source within the box and pads transparent.
    Uses Pillow if available, else ImageMagick `convert`, else copies as-is."""
    try:
        from PIL import Image
        img = Image.open(str(src)).convert("RGBA")
        img.thumbnail((size, size))
        canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        canvas.paste(img, ((size - img.width) // 2, (size - img.height) // 2))
        canvas.save(str(dest))
        return
    except Exception as e:
        print(f"  (Pillow icon resize unavailable: {e})")
    if shutil.which("convert"):
        sh(["convert", str(src), "-resize", f"{size}x{size}",
            "-background", "none", "-gravity", "center",
            "-extent", f"{size}x{size}", str(dest)])
        return
    print("  WARNING: no Pillow/convert — copying icon unresized (flatpak may reject it)")
    shutil.copy2(str(src), str(dest))


def resolve_icon(projects):
    for rel in ICON_SEARCH:
        for proj in projects:
            cand = Path(proj) / rel
            if cand.exists():
                return cand
        # also try rel as-is
        if Path(rel).exists():
            return Path(rel)
    return None


def gen_desktop():
    return (
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Orkid\n"
        "Comment=Orkid Media Engine\n"
        f"Exec={PRIMARY_COMMAND}\n"
        f"Icon={APP_ID}\n"
        "Terminal=false\n"
        "Categories=Graphics;Development;\n"
    )


def gen_metainfo():
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<component type="desktop-application">\n'
        f"  <id>{APP_ID}</id>\n"
        "  <name>Orkid</name>\n"
        "  <summary>Realtime OpenGL/Vulkan graphics + audio media engine</summary>\n"
        "  <metadata_license>CC0-1.0</metadata_license>\n"
        "  <project_license>LicenseRef-proprietary</project_license>\n"
        "  <description><p>Orkid Media Engine with an embedded private CPython.</p></description>\n"
        f'  <launchable type="desktop-id">{APP_ID}.desktop</launchable>\n'
        "  <releases>\n"
        '    <release version="0.1.13"/>\n'
        "  </releases>\n"
        "</component>\n"
    )


def gen_wrapper(app):
    """A thin /app/bin launcher that execs the self-locating obt-launch-env."""
    cmd = " ".join(app["command"])
    tail = (cmd + ' "$@"') if cmd else '"$@"'
    return f'#!/bin/sh\nexec {APP_ROOT}/obt-launch-env {tail}\n'


def gen_manifest(ctx_files, runtime_version, scrub_maps=(), redact_home=None):
    """JSON manifest (flatpak-builder accepts .json). buildsystem: simple copies
    the prebuilt tree into /app/orkid and bakes the sentinel fixup.

    scrub_maps: list of (old, new) path pairs — build-host source roots (OBT_STAGE
    install prefix, project dirs) mapped to their deployed /app/orkid paths, so
    text configs (pkgconfig/cmake/manifest.json) leak no build-host path and stay
    valid. redact_home: the build user's home (e.g. /home/<user>) — any leftover
    occurrence (hardcoded example paths in comments) is redacted to /home/user.
    (Binaries also carry the compile-time prefix as dead strings; removing those
    needs a build-time -ffile-prefix-map and is out of scope here.)"""
    sources = [
        {"type": "dir", "path": str(ctx_files["staging"]), "dest": "staging"},
    ]
    for key in ("desktop", "metainfo", "icon", "wrap_testrunner", "wrap_shell"):
        p = ctx_files[key]
        sources.append({"type": "file", "path": str(p), "dest-filename": p.name})

    build_commands = [
        "mkdir -p /app/orkid /app/bin /app/share/applications "
        "/app/share/metainfo /app/share/icons/hicolor/512x512/apps",
        # copy the relocatable tree wholesale into /app/orkid
        "cp -a staging/. /app/orkid/",
        # BAKE the relocation fixup while /app is writable (runtime is read-only):
        # rewrite the tree's CURRENT root -> /app/orkid across every relocatable
        # file, then set the marker so the launcher's runtime fixup is a no-op.
        # The current root is the tree's .deploy_path value: the sentinel
        # (__OBT_DEPLOY_SENTINEL__) for a fresh deploy, OR the install path when
        # packaging an already-installed pip bundle (`ork.deploy` from pip).
        "_OLD=$(cat /app/orkid/.deploy_path 2>/dev/null); "
        "if [ -n \"$_OLD\" ] && [ \"$_OLD\" != /app/orkid ] && [ -f /app/orkid/.relocatable_files ]; then "
        "while IFS= read -r rel; do f=\"/app/orkid/$rel\"; "
        "if [ -f \"$f\" ] && [ ! -L \"$f\" ]; then "
        "sed -i \"s|$_OLD|/app/orkid|g\" \"$f\"; fi; "
        "done < /app/orkid/.relocatable_files; fi",
        "printf '%s' /app/orkid > /app/orkid/.deploy_path",
        # ---- Scrub build-time dev cruft from the distributable ----
        # deploy.log = the build console log (leaks build-host paths/username);
        # *.la = libtool archives, build-time only. Both are unused at runtime.
        "rm -f /app/orkid/deploy.log",
        "find /app/orkid/lib -type f -name '*.la' -delete 2>/dev/null || true",
    ] + [
        # Sanitize build-host source roots baked into TEXT configs (pkgconfig/
        # cmake install prefixes, projects/manifest.json 'source', ...) -> their
        # deployed paths, so nothing leaks the build host and paths stay valid.
        # grep -I skips binaries.
        f"grep -rlIZ '{old}' /app/orkid 2>/dev/null | "
        f"xargs -0 -r sed -i 's|{old}|{new}|g' 2>/dev/null || true"
        for (old, new) in (scrub_maps or [])
    ] + ([
        # Final pass: redact any leftover build-home username (e.g. hardcoded
        # example paths in comments) -> /home/user. Runs last so functional
        # paths were already mapped above.
        f"grep -rlIZ '{redact_home}' /app/orkid 2>/dev/null | "
        f"xargs -0 -r sed -i 's|{redact_home}|/home/user|g' 2>/dev/null || true",
    ] if redact_home else []) + [
        # install launchers + desktop integration
        f"install -Dm755 {ctx_files['wrap_testrunner'].name} /app/bin/orkid-testrunner",
        f"install -Dm755 {ctx_files['wrap_shell'].name} /app/bin/orkid-launch-shell",
        f"install -Dm644 {ctx_files['desktop'].name} /app/share/applications/{APP_ID}.desktop",
        f"install -Dm644 {ctx_files['metainfo'].name} /app/share/metainfo/{APP_ID}.metainfo.xml",
        f"install -Dm644 {ctx_files['icon'].name} "
        f"/app/share/icons/hicolor/512x512/apps/{APP_ID}.png",
    ]

    return {
        "app-id": APP_ID,
        "runtime": RUNTIME,
        "runtime-version": runtime_version,
        "sdk": SDK,
        "command": PRIMARY_COMMAND,
        "finish-args": FINISH_ARGS,
        "modules": [
            {
                "name": "orkid",
                "buildsystem": "simple",
                "build-commands": build_commands,
                "sources": sources,
            }
        ],
    }


def main():
    ap = argparse.ArgumentParser(description="Build a relocatable Orkid Flatpak.")
    ap.add_argument("--bundle", help="existing relocatable .staging tree (skip phases 1-6)")
    ap.add_argument("--staging", default=os.environ.get("OBT_STAGE"),
                    help="OBT staging dir for phases 1-6 (default $OBT_STAGE)")
    ap.add_argument("--obt-venv", default=os.environ.get("VIRTUAL_ENV"),
                    help="OBT framework venv (default $VIRTUAL_ENV)")
    ap.add_argument("--project", action="append", default=None,
                    help="project dir (repeatable; default $OBT_PROJECT_DIRS)")
    ap.add_argument("--workdir", default="/tmp/orkid_flatpak",
                    help="build workdir (context + repo) [default /tmp/orkid_flatpak]")
    ap.add_argument("--runtime-version", default=DEFAULT_RUNTIME_VERSION)
    ap.add_argument("--outfile", default=None, help="output .flatpak path")
    ap.add_argument("--install", action="store_true",
                    help="flatpak install --user the built app after bundling")
    ap.add_argument("--no-bundle-file", action="store_true",
                    help="skip producing the single-file .flatpak (just build+export/install)")
    args = ap.parse_args()

    _which_or_die("flatpak", "Install: sudo apt install -y flatpak")
    _which_or_die("flatpak-builder", "Install: sudo apt install -y flatpak-builder")

    projects = args.project or (
        [p for p in os.environ.get("OBT_PROJECT_DIRS", "").split(":") if p])
    if not projects:
        sys.exit("ERROR: no --project and $OBT_PROJECT_DIRS not set.")

    workdir = Path(args.workdir)
    ctx = workdir / "context"
    repo = workdir / "repo"
    builddir = workdir / "build"
    if ctx.exists():
        shutil.rmtree(ctx)
    ctx.mkdir(parents=True, exist_ok=True)

    # ---- 1. relocatable tree ----
    if args.bundle:
        staging = Path(args.bundle)
        if not staging.exists():
            sys.exit(f"ERROR: --bundle not found: {staging}")
        print(f"Reusing relocatable tree: {staging}")
    elif args.staging and (Path(args.staging) / ".is_deploy").exists():
        # $OBT_STAGE is already a deployed, relocatable tree — e.g. `ork.deploy`
        # run from a pip-installed orkid (the analog of building a .dmg from a
        # pip install on macOS). Package it directly; the build-time bake reads
        # its .deploy_path (the install path) and relocates it to /app/orkid.
        staging = Path(args.staging)
        print(f"Packaging the deployed bundle directly: {staging}")
    else:
        if not args.staging:
            sys.exit("ERROR: no --bundle and no --staging/$OBT_STAGE.")
        print("Building relocatable tree (deploy phases 1-6)...")
        staging = build_staging_tree(workdir, args.staging, args.obt_venv, projects)

    # ---- 2. generate build inputs ----
    icon_src = resolve_icon(projects)
    if not icon_src:
        sys.exit(f"ERROR: icon not found (searched {ICON_SEARCH}).")
    ctx_files = {
        "staging": staging,
        "desktop": ctx / f"{APP_ID}.desktop",
        "metainfo": ctx / f"{APP_ID}.metainfo.xml",
        "icon": ctx / "OrkidLogo.png",
        "wrap_testrunner": ctx / "orkid-testrunner",
        "wrap_shell": ctx / "orkid-launch-shell",
    }
    ctx_files["desktop"].write_text(gen_desktop())
    ctx_files["metainfo"].write_text(gen_metainfo())
    prepare_icon(icon_src, ctx_files["icon"])
    ctx_files["wrap_testrunner"].write_text(gen_wrapper(APPS[0]))
    ctx_files["wrap_shell"].write_text(gen_wrapper(APPS[1]))

    # Build-host source roots baked into text configs; scrubbed from the
    # distributable. OBT_STAGE install-prefix -> /app/orkid; each project dir ->
    # its deployed /app/orkid/projects/<name>. Any leftover build-home username
    # is redacted last.
    scrub_maps = []
    build_stage = os.environ.get("OBT_STAGE") or args.staging
    if build_stage:
        scrub_maps.append((str(build_stage), APP_ROOT))
    for p in projects:
        scrub_maps.append((str(p), f"{APP_ROOT}/projects/{Path(p).name}"))
    redact_home = os.path.expanduser("~")
    manifest = gen_manifest(ctx_files, args.runtime_version, scrub_maps, redact_home)
    manifest_path = ctx / f"{APP_ID}.json"
    manifest_path.write_text(json.dumps(manifest, indent=2))
    print(f"Wrote manifest: {manifest_path}")

    # ---- 3. flatpak-builder (build + export to repo, optionally install) ----
    if builddir.exists():
        shutil.rmtree(builddir)
    # Keep the builder state dir on the SAME filesystem as the build target
    # (flatpak-builder reflinks/hardlinks between them and errors otherwise —
    # e.g. when --workdir is on tmpfs but $HOME/.flatpak-builder is on disk).
    state_dir = workdir / ".flatpak-builder"
    fb = ["flatpak-builder", "--user", "--force-clean",
          f"--state-dir={state_dir}", f"--repo={repo}"]
    if args.install:
        fb.append("--install")
    fb += [str(builddir), str(manifest_path)]
    sh(fb)

    # ---- 4. single-file bundle ----
    if not args.no_bundle_file:
        outfile = Path(args.outfile) if args.outfile else (workdir / f"{APP_ID}.flatpak")
        sh(["flatpak", "build-bundle", str(repo), str(outfile), APP_ID])
        print(f"\nBundle: {outfile}  ({outfile.stat().st_size/1e6:.0f} MB)")

    print("\nDone. Run with:")
    print(f"  flatpak run {APP_ID}")
    print(f"  flatpak run --command=orkid-launch-shell {APP_ID}")


if __name__ == "__main__":
    main()
