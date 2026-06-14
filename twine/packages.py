#!/usr/bin/env python3
"""Multi-package split manifest for `pip install orkid`.

The relocatable deploy bundle (the `.staging` tree produced by
ork.deploy.macos.relocatable.py phases 1-6) is carved into several PyPI
projects, each < 100 MB, glued by the umbrella `orkid` package's Requires-Dist.

EVERY payload package installs into the SAME directory under site-packages —
`<site-packages>/orkid/` — mirroring the `.staging` layout 1:1. That
keeps the @rpath dylib graph intact (relative load commands resolve within
orkid/) and lets the first-import shim relocate the sentinelized text
files to wherever pip put them.

Tags: payload binaries are `py3-none-<platform>` (NOT cp314t) so they install
under ANY user python — the cp314t ABI lives entirely inside
orkid/pyvenv and pip never has to match it (the hython model). Pure data
+ the umbrella are `py3-none-any`.
"""

VERSION = "0.1.4"   # single source of truth — build.py stamps it into every wheel
                    # filename, METADATA, and the umbrella's Requires-Dist pins.
                    # (> the published 0.0.1 stub / broken 0.1.0 / 0.1.1 / 0.1.2 / 0.1.3)
BUNDLE = "orkid"      # install dir under site-packages (== reconstituted .staging)


def _base(rel):
    return rel.rsplit("/", 1)[-1]

def _is_sitepkg(rel):
    return rel.startswith("pyvenv/") and "/site-packages/" in rel

def _is_orkengine(rel):
    return _is_sitepkg(rel) and "/site-packages/orkengine/" in rel


# Order matters: first matching predicate wins.
# NOTE: collapsed from 7 -> 4 payload projects so we reuse ONLY the project names
# already registered on PyPI (orkid-engine/libdeps/bin/data + the orkid umbrella),
# avoiding PyPI's new-project-creation throttle. Merges:
#   pyext(orkengine .so)            -> orkid-engine
#   python(interp+stdlib) + pydeps  -> orkid-bin
PAYLOAD_PACKAGES = [
    dict(name="orkid-engine",
         summary="Orkid Media Engine — orkid's own compiled code (libork dylibs + orkengine).",
         purelib=False,
         match=lambda r: (r.startswith("lib/") and _base(r).startswith("libork_"))
                         or _is_orkengine(r)),

    dict(name="orkid-libdeps",
         summary="Orkid Media Engine — third-party native libraries.",
         purelib=False,
         match=lambda r: r.startswith("lib/") and not _base(r).startswith("libork_")),

    dict(name="orkid-bin",
         summary="Orkid Media Engine — runtime: executables + private CPython + python deps.",
         purelib=False,
         match=lambda r: r.startswith("bin/")
                         or (r.startswith("pyvenv/") and not _is_orkengine(r))),

    dict(name="orkid-data",
         summary="Orkid Media Engine — runtime data (ork.data, share, ICD, markers).",
         purelib=True,
         match=lambda r: (r.startswith("share/") or r.startswith("projects/")
                          or r.startswith("builds/") or r.startswith("obt_config/")
                          or r in (".relocatable_files", ".deploy_path", ".is_deploy",
                                   "obt-launch-env", "OrkidLogo.icns"))),
]

# Umbrella project: ships only the bootstrap/launcher shim + entry points, and
# depends on every payload package. install_requires pins == VERSION.
UMBRELLA = dict(
    name="orkid",
    summary="Orkid Media Engine — pip bootstrap (brings its own private Python).",
    purelib=True,
    requires_names=[p["name"] for p in PAYLOAD_PACKAGES],   # pinned == VERSION
    # obt framework (pure-python) — provides obt.env.launch.py that the launchers
    # run against orkid as --stagedir. This is the "base venv provided by
    # the pip-install venv" model; no obt_venv is shipped in the wheels.
    extra_requires=["ork.build==0.0.303.dev18"],
    entry_points={"console_scripts": [
        # ONLY these land in the user's venv/bin. The bundle's own bin/ (incl. its
        # private `ork.python` wrapper) stays private — surfaced on PATH only
        # inside the OBT shell these spawn. We deliberately do NOT expose a public
        # `ork.python` here: it would collide with the bundle's ork.python wrapper.
        "ork.shell = orkid_bootstrap.launcher:ork_shell",    # interactive OBT shell (pip LaunchShell)
        "ork.deploy = orkid_bootstrap.launcher:ork_deploy",  # build a relocatable .app/.dmg from this install
        "orkid = orkid_bootstrap.launcher:orkid_main",       # alias -> ork.shell
    ]},
)


def is_omitted(rel):
    """Files present in the DMG bundle but deliberately NOT shipped to PyPI."""
    return (rel.startswith("obt_venv/")          # homebrew base venv: dmg-only
            or rel.startswith("dblockcache/")    # runtime cache: recreated by shim
            or rel.startswith("tempdir/")
            or rel == "deploy.log")
