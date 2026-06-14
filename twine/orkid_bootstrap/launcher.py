"""orkid console-script launchers (installed into the user's venv/bin).

Entry points (see packages.py UMBRELLA):
    ork.shell   -> interactive OBT shell with orkid's bundle paths (like LaunchShell)
    ork.python  -> run a script/args under orkid's private interpreter
    orkid       -> alias for ork.shell

All three relocate `orkid/` to wherever pip installed it (first run), then
delegate to `obt.env.launch.py` (from the `ork.build` dependency) pointed at the
bundle as its --stagedir. That reproduces the exact OBT environment the .dmg
LaunchShell sets up — no hand-rolled env contract. The cp314t engine lives
entirely inside orkid/pyvenv and is never loaded by the user's python.
"""
import os
import shlex
import shutil
import sys
import pathlib

SENTINEL = "__OBT_DEPLOY_SENTINEL__"

# Non-dev (default) `ork.shell` scans only these deps/sdks at launch and uses a
# git-free prompt — much faster shell entry. `ork.shell --dev` skips all of this
# and does the full OBT dep/sdk scan (today's behavior). Grow these lists as the
# minimal runtime surface needs more.
MINIMAL_DEPS = ["python", "orkid", "vulkan"]   # vulkan dep carries the MoltenVK render env
MINIMAL_SDKS = []                              # platform-target sdks; none needed at runtime


def _bundle_root():
    # <site-packages>/orkid_bootstrap/launcher.py -> <site-packages>/orkid
    here = pathlib.Path(__file__).resolve().parent          # orkid_bootstrap/
    bundle = here.parent / "orkid"
    if not bundle.is_dir():
        sys.exit("orkid: bundle not found at %s\n"
                 "  Install the payload packages too: `pip install orkid` pulls\n"
                 "  orkid-engine / orkid-libdeps / orkid-python / orkid-data / ..." % bundle)
    return str(bundle)


def _write(path, text):
    try:
        with open(path, "w") as f:
            f.write(text + "\n")
    except OSError:
        pass


def _relocate(bundle):
    """First-run fixup: rewrite the sentinel (or a previous root) -> this bundle
    path across every text file listed in .relocatable_files. Port of the bash
    fixup in obt-launch-env; idempotent (no-op once .deploy_path == bundle)."""
    marker = os.path.join(bundle, ".deploy_path")
    manifest = os.path.join(bundle, ".relocatable_files")
    old = None
    if os.path.exists(marker):
        try:
            old = open(marker).read().strip()
        except OSError:
            old = None
    if old == bundle:
        return                                   # already relocated to here
    if not os.path.exists(manifest):
        _write(marker, bundle)
        return
    n = 0
    with open(manifest) as mf:
        for line in mf:
            rel = line.strip()
            if not rel:
                continue
            f = os.path.join(bundle, rel)
            if (not os.path.isfile(f)) or os.path.islink(f):
                continue
            try:
                data = open(f, "r", errors="replace").read()
            except OSError:
                continue
            new = data.replace(SENTINEL, bundle)
            if old and old != SENTINEL:
                new = new.replace(old, bundle)
            if new != data:
                try:
                    open(f, "w").write(new)
                    n += 1
                except OSError:
                    pass
    _write(marker, bundle)
    if n:
        print("[orkid] relocated %d files -> %s" % (n, bundle), file=sys.stderr)


def _restore_symlinks(bundle):
    """Recreate the bundle's symlinks from .symlinks.d/ manifests.

    pip cannot install symlinks (it writes the target string as a regular file,
    which then fails to dlopen — 'slice is not valid mach-o file'). build.py
    records every symlink in orkid/.symlinks.d/<pkg>.txt as 'relpath<TAB>
    target'; we recreate them here. Idempotent: skips ones already linked, and
    replaces the stale regular-file stand-ins pip left behind."""
    d = os.path.join(bundle, ".symlinks.d")
    if not os.path.isdir(d):
        return
    n = 0
    for fn in sorted(os.listdir(d)):
        try:
            with open(os.path.join(d, fn)) as f:
                lines = f.read().splitlines()
        except OSError:
            continue
        for line in lines:
            if "\t" not in line:
                continue
            rel, target = line.split("\t", 1)
            link = os.path.join(bundle, rel)
            if os.path.islink(link):
                continue
            try:
                os.makedirs(os.path.dirname(link), exist_ok=True)
                if os.path.lexists(link):
                    os.remove(link)          # stale regular-file stand-in from pip
                os.symlink(target, link)
                n += 1
            except OSError:
                pass
    if n:
        print("[orkid] restored %d symlinks" % n, file=sys.stderr)


def _ensure_caches(bundle):
    """Point runtime caches at the user's global dir (assetcache/envmaps), and
    make sure the writable cache dirs exist — mirrors obt-launch-env."""
    gc = os.path.join(os.path.expanduser("~"), ".obt-global")
    for sub in ("assetcache", "envmaps"):
        g = os.path.join(gc, sub)
        os.makedirs(g, exist_ok=True)
        link = os.path.join(bundle, sub)
        if not os.path.lexists(link):
            try:
                os.symlink(g, link)
            except OSError:
                pass
    os.makedirs(os.path.join(bundle, "dblockcache"), exist_ok=True)


def _obt_launch_exe():
    # ork.build installs obt.env.launch.py into the SAME bin dir as this venv's
    # python (and as the ork.python console script). Look next to the interpreter
    # first so it resolves even when the venv isn't activated / PATH is minimal,
    # then fall back to PATH.
    cand = os.path.join(os.path.dirname(sys.executable), "obt.env.launch.py")
    if os.path.exists(cand):
        return cand
    exe = shutil.which("obt.env.launch.py")
    if not exe:
        sys.exit("orkid: obt.env.launch.py not found (next to %s or on PATH).\n"
                 "  `ork.build` provides it and is a dependency of `orkid` — "
                 "ensure it is installed in this environment." % sys.executable)
    return exe


def _launch(extra_args):
    bundle = _bundle_root()
    _restore_symlinks(bundle)
    _relocate(bundle)
    _ensure_caches(bundle)
    exe = _obt_launch_exe()
    project = os.path.join(bundle, "projects", "orkid")
    argv = [exe, "--stagedir", bundle, "--project", project] + list(extra_args)
    os.execv(exe, argv)


def _orkid_version():
    """Version of the installed PyPI `orkid` package (for the minimal prompt)."""
    try:
        import importlib.metadata as md
        return md.version("orkid")
    except Exception:
        return ""


def ork_shell():
    """Drop into an interactive OBT shell with orkid's paths (pip LaunchShell).

    Default is MINIMAL (fast): scans only MINIMAL_DEPS / MINIMAL_SDKS at launch
    and uses a git-free prompt. Pass `--dev` for the full OBT dep/sdk scan and
    the git-aware prompt.
    """
    argv = sys.argv[1:]
    dev = "--dev" in argv
    argv = [a for a in argv if a != "--dev"]
    if not dev:
        os.environ["OBT_NONDEV"] = "1"            # master non-dev flag: minimal prompt + quiet env logging
        os.environ["OBT_MINIMAL_DEPS"] = ":".join(MINIMAL_DEPS)
        os.environ["OBT_MINIMAL_SDKS"] = ":".join(MINIMAL_SDKS)
        os.environ["ORKID_VERSION"] = _orkid_version()
    _launch(argv)


def ork_deploy():
    """Build a relocatable .app/.dmg FROM this pip-installed orkid bundle.

    Runs the bundled ork.deploy.macos.relocatable.py against orkid (which
    is $OBT_STAGE inside the OBT env). e.g.  ork.deploy --target ~/Desktop/MyApp
    """
    cmd = shlex.join(["ork.deploy.macos.relocatable.py"] + sys.argv[1:])
    _launch(["--command", cmd])


def orkid_main():
    """`orkid` — alias for `ork.shell`."""
    ork_shell()
