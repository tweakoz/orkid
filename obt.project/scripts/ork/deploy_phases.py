"""Shared deploy phases for macOS relocatable deployments.

Extracted from ork.deploy.macos.relocatable.py so that multiple products
(orkid, uni, …) can share the same infrastructure phases (1–6, 5.5, 8)
while providing their own Phase 7 app-bundle configuration.

Usage from a thin top-level script::

    from ork.deploy_phases import run_deploy
    DEPLOY_CONFIG = { ... }
    if __name__ == "__main__":
        run_deploy(DEPLOY_CONFIG)
"""

import argparse
import json
import os
import shutil
import subprocess
import sys

from obt import path, pathtools, host, macos, deco as deco_mod
from obt.command import run

deco = deco_mod.Deco()

###############################################################################
# Platform relocation backend
#
# macOS relocates Mach-O binaries via obt.macos (otool / install_name_tool /
# codesign, @rpath/@loader_path). Linux relocates ELF binaries via obt.linux
# (patchelf / ldd, $ORIGIN, no signing). Phases dispatch on IS_LINUX / IS_OSX;
# the platform-neutral scaffolding (copy / prune / whitelist / project include /
# sentinelize / manifest / markers) is fully shared.
###############################################################################

IS_LINUX = host.IsLinux
IS_OSX = host.IsOsx
if IS_LINUX:
  from obt import linux as elf

def _is_native_binary(p):
  """True if p is a native loadable binary for the host (ELF on Linux, Mach-O on macOS)."""
  return elf.is_elf_binary(str(p)) if IS_LINUX else macos.is_macho_binary(str(p))

def _discover_binaries(root, skip_dirs=None):
  """Discover all native binaries under root (ELF on Linux, Mach-O on macOS)."""
  if IS_LINUX:
    return elf.discover_elf_files(root, skip_dirs=skip_dirs)
  return macos.discover_macho_files(root, skip_dirs=skip_dirs)

class _TeeWriter:
  """Write to both a file and the original stream."""
  def __init__(self, stream, log_file):
    self._stream = stream
    self._log = log_file
  def write(self, data):
    self._stream.write(data)
    self._log.write(data)
  def flush(self):
    self._stream.flush()
    self._log.flush()

###############################################################################
# Utility: Internalize a host binary + its full homebrew dylib closure
###############################################################################

def internalize_host_binary(binary_name, target_dir, homebrew_dir="/opt/homebrew"):
  """Copy a host binary into bin/ and internalize its entire homebrew dylib closure.

  This is the generic mechanism for severing a homebrew executable from its
  host dependencies. It:
    1. Finds the binary on the host via `which`
    2. Copies the real binary (resolving symlinks) into target_dir/bin/
    3. Walks its Mach-O dependencies to discover the full homebrew dylib closure
    4. Copies all closure dylibs into target_dir/lib/
    5. Rewrites load commands to @rpath and re-signs

  Args:
    binary_name: Name of the executable (e.g. "rsvg-convert", "pkg-config")
    target_dir: The deployment infrastructure directory (.staging/)
    homebrew_dir: Homebrew prefix (default /opt/homebrew)

  Returns:
    True if the binary was internalized, False if not found or not needed
  """
  target_dir = path.Path(target_dir)
  target_bin = target_dir / "bin"
  target_lib = target_dir / "lib"
  target_bin.mkdir(parents=True, exist_ok=True)
  target_lib.mkdir(parents=True, exist_ok=True)

  target_binary = target_bin / binary_name

  # Find on host
  host_path = shutil.which(binary_name)
  if not host_path:
    print(deco.val(f"    WARNING: {binary_name} not found on host"))
    return False

  # Copy the real binary (resolve symlinks)
  real_path = os.path.realpath(host_path)
  shutil.copy2(real_path, str(target_binary))
  os.chmod(str(target_binary), 0o755)
  print(deco.val(f"    Copied: {real_path} -> bin/{binary_name}"))

  # Check if it's a native binary (could be a script)
  if not _is_native_binary(target_binary):
    print(deco.val(f"    {binary_name} is not a native binary — no lib fixup needed"))
    return True

  # ---- Linux (ELF): internalize the binary's external .so closure into lib/,
  # then set an $ORIGIN rpath. No id-change, no codesign. ----
  if IS_LINUX:
    walker = elf.ElfDependencyWalker(target_dir)
    walker.seed_files = [target_binary]
    walker.walk()
    copied = []
    for libpath in sorted(walker.get_bundle_closure()):
      dest = elf.internalize_lib(libpath, target_lib)
      if dest:
        copied.append(dest)
    print(deco.val(f"    Internalized {len(copied)} libs for {binary_name}"))
    relocator = elf.ElfRelocator(target_dir)
    relocator.relocate_binary(target_binary)
    for dest in copied:
      if elf.is_elf_binary(dest):
        relocator.relocate_binary(dest, is_dylib=True)
    return True

  # ---- macOS (Mach-O): internalize the homebrew dylib closure ----
  # Walk its homebrew dylib closure
  walker = macos.MachoDependencyWalker(target_dir, homebrew_dir)
  walker.seed_files = [target_binary]
  walker.walk()
  closure = walker.get_homebrew_closure()

  # Copy closure dylibs into lib/
  copied = 0
  for hb_path in sorted(closure):
    basename = os.path.basename(hb_path)
    dest = target_lib / basename
    if dest.exists():
      continue
    if ".framework/" in hb_path:
      continue
    real = os.path.realpath(hb_path)
    if not os.path.isfile(real):
      print(deco.val(f"    WARNING: closure dylib not found: {hb_path}"))
      continue
    shutil.copy2(real, str(dest))
    copied += 1

  print(deco.val(f"    Internalized {copied} homebrew dylibs for {binary_name}"))

  # Relocate the binary + any new dylibs
  relocator = macos.MachoRelocator(target_dir)
  old_prefixes = [homebrew_dir,
                  os.path.join(homebrew_dir, "opt"),
                  os.path.join(homebrew_dir, "Cellar"),
                  os.path.join(homebrew_dir, "lib")]

  # Relocate the binary itself
  relocator.relocate_binary(target_binary, old_prefixes, is_dylib=False)

  # Relocate any new dylibs we just copied
  for hb_path in sorted(closure):
    basename = os.path.basename(hb_path)
    dest = target_lib / basename
    if dest.exists() and macos.is_macho_binary(str(dest)):
      relocator.relocate_binary(dest, old_prefixes, is_dylib=True)

  # Re-sign
  relocator.resign_all()

  return True

###############################################################################
# Phase 1: Deep Copy + Internalize
###############################################################################

# Directories to copy from staging (runtime-essential only)
RUNTIME_DIRS = ["bin", "lib", "pyvenv", "share"]

# For a RUNTIME_DIR that should ship only a subset, list the kept sub-entries
# (dir / glob / file, relative to the runtime dir). Dirs absent here are copied
# wholesale. This drops dev-only share/ content (singularity audio banks ~115MB,
# cmake, doc, man, …) — keep only what the runtime actually needs.
RUNTIME_DIR_WHITELISTS = {
    "share": ["ffmpeg", "fonts", "luajit-2.1"],
}

# Specific lib/ entries (frameworks / SDK blobs) to drop from the deployment —
# copied wholesale by the lib/ copy but not needed by the shipped runtime.
LIB_DROP = ["m3api.framework"]

# CPython stdlib entries (relative to lib/python3*/) to drop from BOTH venvs.
# All are build-time / GUI / test cruft never used by the shipped runtime:
#   config-*  -> static libpython.a + Makefile (extension-build only; include/
#                is already excluded, so source builds aren't possible anyway)
#   test      -> CPython's own ~36MB test suite
#   idlelib/tkinter/turtle* -> Tk GUI    ensurepip -> pip bootstrap    pydoc_data -> help text
# (_sysconfigdata_*.py lives at the stdlib top level, so sysconfig still works.)
STDLIB_PRUNE = ["test", "config-*", "idlelib", "ensurepip", "lib2to3",
                "pydoc_data", "turtledemo", "turtle.py", "tkinter"]

# Directories to skip (build intermediates, headers, etc.)
SKIP_DIRS = {"builds", "include", "buildlogs",
             "nanobind", "sdks", "subspaces", "tempdir",
             "doc", "obt-launch-env"}

###############################################################################
# Deployment slimming helpers
###############################################################################

def _strip_static_archives(lib_dir):
  """Remove .a static libraries from a lib directory tree.

  Static archives are link-time only — never dlopen'd at runtime. The staging
  lib/ carries the full LLVM/openvdb/boost/png/etc. static closure (~400MB)
  purely as a build byproduct, so it has no place in a deployment.

  Returns (count_removed, bytes_freed).
  """
  lib_dir = str(lib_dir)
  count = 0
  freed = 0
  if not os.path.isdir(lib_dir):
    return count, freed
  for dirpath, dirnames, filenames in os.walk(lib_dir):
    for fname in filenames:
      if not fname.endswith(".a"):
        continue
      fpath = os.path.join(dirpath, fname)
      try:
        if not os.path.islink(fpath):
          freed += os.path.getsize(fpath)
      except OSError:
        pass
      try:
        os.remove(fpath)
        count += 1
      except OSError as e:
        print(deco.val(f"    WARN: could not remove {fpath}: {e}"))
  return count, freed

def _strip_elf_binaries(root):
  """Strip debug symbols from ELF binaries (Linux only).

  Our compiled outputs ship with full debug_info — libork_lev2.so is 263MB
  unstripped vs ~24MB stripped, and there is ~1.9GB of debug symbols across the
  tree. Runtime needs none of it; it only bloats distributables (fatal for PyPI
  wheels, which have a 100MB/file limit). Uses `strip --strip-unneeded`, which
  preserves the dynamic symbols required to LOAD shared objects (and RPATH/
  RUNPATH), so relocation and $ORIGIN loading are unaffected. Opt out with
  ORKID_DEPLOY_NOSTRIP=1. Returns (count, bytes_freed).
  """
  if os.environ.get("ORKID_DEPLOY_NOSTRIP"):
    print(deco.val("    (ORKID_DEPLOY_NOSTRIP set — keeping debug symbols)"))
    return 0, 0
  strip = shutil.which("strip")
  if not strip:
    print(deco.val("    WARNING: 'strip' not found — shipping unstripped binaries"))
    return 0, 0
  n = 0
  freed = 0
  for dirpath, dirnames, filenames in os.walk(str(root)):
    for fname in filenames:
      fp = os.path.join(dirpath, fname)
      if os.path.islink(fp) or not os.path.isfile(fp):
        continue
      if not elf.is_elf_binary(fp):
        continue
      try:
        before = os.path.getsize(fp)
        r = subprocess.run([strip, "--strip-unneeded", fp], capture_output=True)
        if r.returncode == 0:
          freed += before - os.path.getsize(fp)
          n += 1
      except OSError:
        pass
  return n, freed

# Runtime-slimming lists for Linux distributables (pip wheels / flatpak). These
# are dev/example-only payload the engine never needs at runtime; optional deps
# can be re-added into the bundled venv on demand (`ork.python -m pip install
# <x>` — installs against the private 3.14t interpreter, the right ABI). This is
# NOT the same as removing them from ork.build's pydefaults (dev staging keeps
# them so examples still work in-tree) — we only prune the SHIPPED copy.
# Opt out entirely with ORKID_DEPLOY_FULL=1.
# Dev / notebook / viz python packages not needed by the engine runtime. NOTE:
# matplotlib IS used by two content tools (ork.hdri.studio.py / ork.hdri.xirview
# .py) — removing it means those two need `ork.python -m pip install matplotlib`.
# The jupyter/notebook/plotly/debugpy set is examples/tests-only.
_SLIM_SITEPKGS = ["playwright", "numba", "llvmlite",
                  "matplotlib", "plotly", "debugpy",
                  "jupyterlab", "notebook", "widgetsnbextension", "jupyter_builder"]
# Qt5 IS removable once its only two consumers (the dev GUI tools bin/iv and
# bin/profiler_gui, below) are pruned — nothing else in the bundle references
# it. icu is NOT removable (needed by libxml2 + libboost_locale, independent of
# Qt5), so it stays.
_SLIM_LIB_GLOBS = ["libQt5*.so*"]
# bin/: test/example executables, the two Qt5 GUI dev tools, and build-time
# tools. IMPORTANT: this removes the LLVM command-line TOOLS (bin/llvm-*) only —
# the LLVM *library* (lib/libLLVM.so) is KEPT, because OpenVDB AX JIT-compiles
# against it at runtime (libopenvdb_ax NEEDs libLLVM.so.18). Nothing at runtime
# shells out to cmake/ispc/llvm tools.
_SLIM_BIN_GLOBS = ["ork.test.*", "ork.example.*",
                   "iv", "profiler_gui",                   # Qt5 GUI dev tools (VDB viewer / profiler)
                   "cmake", "ccmake", "cpack", "ctest",    # CMake build tools
                   "ispc",                                 # SPMD compiler (build-time)
                   "llvm-*", "obj2yaml", "yaml2obj",       # LLVM CLI dev tools (lib kept)
                   "vdb_print", "FileCheck", "lli-child-target"]  # VDB/LLVM test tools
                   # (oiiotool intentionally KEPT — OpenImageIO texture/image tool)

def _slim_linux_runtime(target_dir):
  """Prune dev/example-only payload from a Linux runtime deploy. Returns
  (items_removed, bytes_freed)."""
  import glob as _glob
  if os.environ.get("ORKID_DEPLOY_FULL"):
    print(deco.val("    (ORKID_DEPLOY_FULL set — keeping dev/example payload)"))
    return 0, 0
  target_dir = path.Path(target_dir)
  removed = 0
  freed = 0
  def _rm(p):
    nonlocal removed, freed
    if os.path.islink(p) or os.path.isfile(p):
      try:
        freed += os.path.getsize(p)
      except OSError:
        pass
      try:
        os.remove(p); removed += 1
      except OSError:
        pass
    elif os.path.isdir(p):
      for r, _d, fs in os.walk(p):
        for fn in fs:
          fp = os.path.join(r, fn)
          if not os.path.islink(fp):
            try:
              freed += os.path.getsize(fp)
            except OSError:
              pass
      shutil.rmtree(p, ignore_errors=True); removed += 1
  # site-packages python deps (dir + <name>.libs + <name>-*.dist-info)
  for sp in _glob.glob(str(target_dir / "pyvenv" / "lib" / "python3*" / "site-packages")):
    for name in _SLIM_SITEPKGS:
      for m in (_glob.glob(os.path.join(sp, name))
                + _glob.glob(os.path.join(sp, name + ".libs"))
                + _glob.glob(os.path.join(sp, name + "-*.dist-info"))):
        _rm(m)
  # native libs pulled in transitively but unused at runtime
  for g in _SLIM_LIB_GLOBS:
    for m in _glob.glob(str(target_dir / "lib" / g)):
      _rm(m)
  # test/example executables
  for g in _SLIM_BIN_GLOBS:
    for m in _glob.glob(str(target_dir / "bin" / g)):
      _rm(m)
  # jupyter labextension static bundles under share/, and Windows .pdb debug
  # files (dead weight on Linux) anywhere in the venv.
  for m in _glob.glob(str(target_dir / "pyvenv" / "share" / "jupyter")):
    _rm(m)
  # Windows .pdb (dead on Linux) + cython build sources (.c/.pyx — the compiled
  # .so is what's imported at runtime). Skip include/ dirs so C headers shipped
  # for building extensions against numpy/scipy are preserved.
  pyvenv = target_dir / "pyvenv"
  if pyvenv.exists():
    for r, _d, fs in os.walk(str(pyvenv)):
      if (os.sep + "include" + os.sep) in (r + os.sep):
        continue
      for fn in fs:
        if fn.endswith((".pdb", ".c", ".pyx")):
          _rm(os.path.join(r, fn))
  return removed, freed

def _copy_tree_whitelist(proj_root, proj_target, tree_rel, entries):
  """Selectively copy only whitelisted entries of a source tree.

  Used to ship a curated subset of a large source tree (e.g. ork.data, where
  tests/sounds/misc and most of src/ + platform_lev2/textures are dev-only).
  Only the listed entries are copied — nothing else in the tree is touched, so
  this is far cheaper than copy-then-prune.

  Each entry is a path relative to tree_rel and may be:
    - a directory  → copied wholesale (cp -a, structure preserved)
    - a glob (contains * ? [) → every match copied, relative structure preserved
    - a file       → copied

  Returns the number of files copied.
  """
  import glob as _glob
  proj_root = path.Path(proj_root)
  proj_target = path.Path(proj_target)
  src_root = proj_root / tree_rel
  dst_root = proj_target / tree_rel
  n_files = 0

  if not src_root.exists():
    print(deco.val(f"    Skipping whitelist {tree_rel}/ (source not found)"))
    return 0

  for entry in entries:
    is_glob = any(c in entry for c in "*?[")
    if is_glob:
      matches = sorted(_glob.glob(str(src_root / entry), recursive=True))
      if not matches:
        print(deco.val(f"    WARNING: whitelist glob matched nothing: {tree_rel}/{entry}"))
    else:
      cand = str(src_root / entry)
      if os.path.exists(cand):
        matches = [cand]
      else:
        print(deco.val(f"    WARNING: whitelist entry not found: {tree_rel}/{entry}"))
        matches = []

    for m in matches:
      rel = os.path.relpath(m, str(src_root))
      dst = dst_root / rel
      dst.parent.mkdir(parents=True, exist_ok=True)
      if os.path.isdir(m):
        run(["cp", "-a", m, str(dst)], do_log=False)
        n_files += sum(len(fs) for _r, _d, fs in os.walk(m))
      else:
        shutil.copy2(m, str(dst))
        n_files += 1

  print(deco.val(f"    Whitelisted {tree_rel}/ ({len(entries)} rules, {n_files} files)"))
  return n_files

def _prune_pycache(root):
  """Remove all __pycache__ directories under root.

  Compiled bytecode is regenerated on first import, so shipping it only adds
  bulk and risks stale .pyc that embed dev-host paths (the sentinel text-fixup
  skips binaries, so .pyc never get relocated). Safe to drop wholesale.

  Returns (dirs_removed, bytes_freed).
  """
  root = str(root)
  count = 0
  freed = 0
  if not os.path.isdir(root):
    return count, freed
  to_remove = []
  for dirpath, dirnames, filenames in os.walk(root):
    if "__pycache__" in dirnames:
      to_remove.append(os.path.join(dirpath, "__pycache__"))
      dirnames.remove("__pycache__")  # don't descend into it
  for pc in to_remove:
    for r2, _d2, f2 in os.walk(pc):
      for fn in f2:
        try:
          fp = os.path.join(r2, fn)
          if not os.path.islink(fp):
            freed += os.path.getsize(fp)
        except OSError:
          pass
    shutil.rmtree(pc, ignore_errors=True)
    count += 1
  return count, freed

def _prune_stdlib(venv_dir):
  """Drop build-time/GUI/test stdlib (STDLIB_PRUNE) from a venv's lib/python3*/.

  Returns (entries_removed, bytes_freed).
  """
  import glob as _glob
  venv_dir = path.Path(venv_dir)
  libdir = venv_dir / "lib"
  removed = 0
  freed = 0
  if not libdir.exists():
    return removed, freed
  for pydir in sorted(_glob.glob(str(libdir / "python3*"))):
    if not os.path.isdir(pydir):
      continue
    for entry in STDLIB_PRUNE:
      for m in _glob.glob(os.path.join(pydir, entry)):
        try:
          for r2, _d2, f2 in os.walk(m):
            for fn in f2:
              fp = os.path.join(r2, fn)
              if not os.path.islink(fp):
                try:
                  freed += os.path.getsize(fp)
                except OSError:
                  pass
        except OSError:
          pass
        if os.path.isdir(m) and not os.path.islink(m):
          shutil.rmtree(m, ignore_errors=True)
        else:
          try:
            os.remove(m)
          except OSError:
            pass
        removed += 1
  return removed, freed

def phase1_copy(staging_dir, target_dir, force=False):
  """Deep copy staging to target and internalize homebrew dylib closure.

  Args:
    staging_dir: Source OBT staging directory
    target_dir: Destination directory for the relocatable deployment
    force: If True, remove existing target before copying
  """
  staging_dir = path.Path(staging_dir)
  target_dir = path.Path(target_dir)

  print(deco.val("=" * 60))
  print(deco.val("Phase 1: Deep Copy + Internalize Homebrew"))
  print(deco.val("=" * 60))
  print(deco.val(f"  Source:  {staging_dir}"))
  print(deco.val(f"  Target:  {target_dir}"))

  # Safety check
  if target_dir.exists():
    if force:
      print(deco.val(f"  Removing existing target..."))
      shutil.rmtree(str(target_dir))
    else:
      print(deco.val(f"  ERROR: Target already exists. Use --force to overwrite."))
      return False

  target_dir.mkdir(parents=True, exist_ok=True)

  # ---- Step 1: Walk staging to discover the external lib closure ----
  # (homebrew dylib closure on macOS; the third-party .so closure on Linux)
  print(deco.val(f"\n  Step 1: Discovering dependencies..."))
  if IS_LINUX:
    walker = elf.ElfDependencyWalker(staging_dir)
    walker.walk()
    external_closure = walker.get_bundle_closure()
    _missing = walker.get_missing()
    if _missing:
      print(deco.val(f"  WARNING: {len(_missing)} NEEDED libs unresolved from "
                     f"staging (will be MISSING in the bundle):"))
      for _m in sorted(_missing):
        print(deco.val(f"      {_m}"))
    print(deco.val(f"  External closure: {len(external_closure)} .so files"))
  else:
    walker = macos.MachoDependencyWalker(staging_dir)
    walker.walk()
    external_closure = walker.get_homebrew_closure()
    print(deco.val(f"  Homebrew closure: {len(external_closure)} dylibs"))

  # ---- Step 2: Copy runtime directories ----
  print(deco.val(f"\n  Step 2: Copying runtime directories..."))
  for dirname in RUNTIME_DIRS:
    src = staging_dir / dirname
    dst = target_dir / dirname
    if not src.exists():
      print(deco.val(f"    Skipping {dirname}/ (not found)"))
      continue
    whitelist = RUNTIME_DIR_WHITELISTS.get(dirname)
    if whitelist is not None:
      print(deco.val(f"    Copying {dirname}/ (whitelisted subset)..."))
      _copy_tree_whitelist(staging_dir, target_dir, dirname, whitelist)
    else:
      print(deco.val(f"    Copying {dirname}/..."))
      # Use cp -a to preserve symlinks and permissions
      run(["cp", "-a", str(src), str(dst)], do_log=False)

  # assetcache symlink is created at launch time (obt-launch-env)
  # so it points to the actual user's ~/.obt-global/assetcache

  # Create empty dblockcache directory
  (target_dir / "dblockcache").mkdir(parents=True, exist_ok=True)
  print(deco.val(f"    Created dblockcache/ (empty)"))
  # NOTE: the on-disk cook caches (dblockcache ~GBs, dflowcache) are NOT copied
  # from staging — the copy above is a whitelist (RUNTIME_DIRS = bin/lib/pyvenv/
  # share). They are build-host scratch and regenerate at runtime; dblockcache
  # is recreated empty here only because the engine expects the dir to exist.

  # ---- Step 2b: Strip static archives (.a) from lib/ ----
  print(deco.val(f"\n  Step 2b: Stripping static libraries (.a) from lib/..."))
  _a_count, _a_bytes = _strip_static_archives(target_dir / "lib")
  print(deco.val(f"    Removed {_a_count} .a files ({_a_bytes // (1024 * 1024)} MB)"))

  # ---- Step 2b2: Drop unwanted lib/ entries (frameworks / SDK blobs) ----
  print(deco.val(f"\n  Step 2b2: Dropping unwanted lib/ entries..."))
  for _name in LIB_DROP:
    _p = target_dir / "lib" / _name
    if _p.exists():
      _sz = 0
      for _r, _d, _f in os.walk(str(_p)):
        for _fn in _f:
          try:
            _fp = os.path.join(_r, _fn)
            if not os.path.islink(_fp):
              _sz += os.path.getsize(_fp)
          except OSError:
            pass
      if _p.is_dir() and not _p.is_symlink():
        shutil.rmtree(str(_p), ignore_errors=True)
      else:
        try:
          _p.unlink()
        except OSError:
          pass
      print(deco.val(f"    Dropped lib/{_name} ({_sz // (1024 * 1024)} MB)"))
    else:
      print(deco.val(f"    lib/{_name} not present (skip)"))

  # ---- Step 2c: Prune __pycache__ from the copied tree ----
  print(deco.val(f"\n  Step 2c: Pruning __pycache__ ..."))
  _pc_count, _pc_bytes = _prune_pycache(target_dir)
  print(deco.val(f"    Removed {_pc_count} __pycache__ dirs ({_pc_bytes // (1024 * 1024)} MB)"))

  # ---- Step 2d: Prune build-time/GUI/test stdlib from the private pyvenv ----
  print(deco.val(f"\n  Step 2d: Pruning stdlib cruft from pyvenv..."))
  _sl_count, _sl_bytes = _prune_stdlib(target_dir / "pyvenv")
  print(deco.val(f"    Removed {_sl_count} stdlib entries ({_sl_bytes // (1024 * 1024)} MB)"))

  # ---- Step 3: Copy the external lib closure into target/lib/ ----
  print(deco.val(f"\n  Step 3: Internalizing external libraries..."))
  target_lib = target_dir / "lib"
  target_lib.mkdir(parents=True, exist_ok=True)
  copied_count = 0
  skipped_frameworks = []

  if IS_LINUX:
    # ELF: copy the real .so and recreate its SONAME symlink chain so the
    # loader resolves it by SONAME within lib/.
    for so_path in sorted(external_closure):
      if elf.internalize_lib(so_path, target_lib):
        copied_count += 1
      else:
        print(deco.val(f"    WARNING: not found: {so_path}"))
    print(deco.val(f"    Copied {copied_count} external .so files (SONAME-linked)"))
  else:
    for hb_path in sorted(external_closure):
      basename = os.path.basename(hb_path)
      dest_path = target_lib / basename

      # Skip if already exists (from staging lib/ copy)
      if dest_path.exists():
        continue

      # Handle framework references specially
      if ".framework/" in hb_path:
        skipped_frameworks.append(hb_path)
        continue

      # Resolve symlinks to get the real file
      real_path = os.path.realpath(hb_path)
      if not os.path.isfile(real_path):
        print(deco.val(f"    WARNING: not found: {hb_path}"))
        continue

      shutil.copy2(real_path, str(dest_path))
      copied_count += 1

    print(deco.val(f"    Copied {copied_count} homebrew dylibs"))
  if skipped_frameworks:
    print(deco.val(f"    Skipped {len(skipped_frameworks)} framework references:"))
    for fw in skipped_frameworks:
      print(deco.val(f"      {fw}"))

  # ---- Step 4: Ensure libpython is in target/lib/ ----
  print(deco.val(f"\n  Step 4: Ensuring libpython is in lib/..."))
  # macOS: libpython3.X.dylib ; Linux: libpython3.X.so / .so.1.0
  _libpy_ok = (lambda n: n.startswith("libpython") and ".so" in n) if IS_LINUX \
              else (lambda n: n.startswith("libpython") and n.endswith(".dylib"))
  pyvenv_lib = target_dir / "pyvenv" / "lib"
  if pyvenv_lib.exists():
    for item in pyvenv_lib.iterdir():
      if _libpy_ok(item.name):
        dest = target_lib / item.name
        if not dest.exists():
          if item.is_symlink():
            shutil.copy2(str(item.resolve()), str(dest))
          else:
            shutil.copy2(str(item), str(dest))
          print(deco.val(f"    Copied {item.name} to lib/"))
        else:
          print(deco.val(f"    {item.name} already in lib/"))

  # ---- Step 4a: Slim dev/example payload (Linux runtime deploy) ----
  if IS_LINUX:
    print(deco.val(f"\n  Step 4a: Slimming dev/example payload..."))
    _sl_count, _sl_bytes = _slim_linux_runtime(target_dir)
    print(deco.val(f"    Removed {_sl_count} items ({_sl_bytes // (1024 * 1024)} MB freed)"))

  # ---- Step 4b: Strip ELF debug symbols (Linux) ----
  if IS_LINUX:
    print(deco.val(f"\n  Step 4b: Stripping ELF debug symbols..."))
    _st_count, _st_bytes = _strip_elf_binaries(target_dir)
    print(deco.val(f"    Stripped {_st_count} ELF binaries ({_st_bytes // (1024 * 1024)} MB freed)"))

  # ---- Step 5: Fix text files with hardcoded staging paths ----
  print(deco.val(f"\n  Step 5: Fixing text files with hardcoded paths..."))
  fix_text_references(target_dir, staging_dir)

  # ---- Step 6: Strip dev-authored .bashrc from the shipped bundle -----
  # The dev's .staging/.bashrc is generated by init_env.py:extend_bashrc
  # at env-create / launch time — it captures the dev shell's current
  # DYLD_LIBRARY_PATH and bakes it into the file literally. Carrying it
  # into the deployed bundle leaks absolute dev-host paths to anyone who
  # inspects the shipped .dmg. Harmless at runtime (nothing in the
  # deployed launch chain sources .bashrc) but ugly and unnecessary.
  # First launch on the user's machine will regenerate a correct .bashrc
  # from that user's own env anyway.
  _bashrc = target_dir / ".bashrc"
  if _bashrc.exists():
    _bashrc.unlink()
    print(deco.val(f"    Stripped dev-authored .bashrc from bundle"))

  print(deco.val(f"\n  Phase 1 complete."))
  return True

###############################################################################

def fix_text_in_tree(search_root, replacements, label_root=None):
  """Replace path strings in text files under search_root.

  Args:
    search_root: Directory tree to search
    replacements: List of (old_str, new_str) pairs to apply
    label_root: Root for relative-path display (default: search_root)

  Returns:
    Number of files fixed
  """
  search_root = path.Path(search_root)
  if label_root is None:
    label_root = search_root
  count = 0

  for old_str, new_str in replacements:
    old_str = str(old_str)
    new_str = str(new_str)

    # Use grep to find files containing the old string. -I makes grep skip
    # binary files entirely (grep -rl otherwise LISTS binaries that merely
    # contain the path bytes — e.g. an ELF interpreter with the staging path in
    # its RUNPATH — and rewriting one as text below corrupts it).
    try:
      result = subprocess.run(
        ["grep", "-rlI", old_str, str(search_root)],
        capture_output=True, text=True, timeout=60)
    except:
      continue

    for fpath in result.stdout.strip().splitlines():
      if not fpath or not os.path.isfile(fpath):
        continue
      if fpath.endswith('.pyc'):
        continue
      # Robust, format-agnostic binary guard (defense-in-depth beyond grep -I):
      # a null byte in the first 8K means binary — never rewrite it as text.
      # (macos.is_macho_binary was used here before, but it returns False for
      # ELF, so it failed to protect Linux binaries.)
      try:
        with open(fpath, 'rb') as _bf:
          if b'\x00' in _bf.read(8192):
            continue
      except (OSError, IOError):
        continue
      try:
        with open(fpath, 'r', errors='replace') as f:
          content = f.read()
        if old_str in content:
          content = content.replace(old_str, new_str)
          with open(fpath, 'w') as f:
            f.write(content)
          count += 1
          rel = os.path.relpath(fpath, str(label_root))
          print(deco.val(f"    Fixed: {rel}"))
      except:
        pass

  return count

def fix_shebangs_in_dir(bin_dir, replacements):
  """Fix shebang lines in scripts under bin_dir.

  Args:
    bin_dir: Directory containing scripts
    replacements: List of (old_str, new_str) pairs

  Returns:
    Number of files fixed
  """
  bin_dir = path.Path(bin_dir)
  if not bin_dir.exists():
    return 0
  count = 0
  for fname in sorted(os.listdir(str(bin_dir))):
    fpath = str(bin_dir / fname)
    if os.path.islink(fpath) or not os.path.isfile(fpath):
      continue
    try:
      with open(fpath, 'rb') as f:
        first_line = f.readline(512)
      if not first_line.startswith(b'#!'):
        continue
      for old_str, new_str in replacements:
        old_str = str(old_str)
        new_str = str(new_str)
        if old_str.encode() in first_line:
          with open(fpath, 'r', errors='replace') as f:
            content = f.read()
          content = content.replace(old_str, new_str)
          with open(fpath, 'w') as f:
            f.write(content)
          count += 1
          break
    except:
      pass
  return count

def fix_text_references(target_dir, old_staging_dir):
  """Replace absolute staging paths in text files within the target.

  Uses grep to find all files containing the old path, then does text
  replacement. Skips binary files. Fully generic — no hardcoded versions.
  """
  replacements = [(str(old_staging_dir), str(target_dir))]
  count = 0

  # Search pyvenv/ for files containing the old staging path
  pyvenv_dir = target_dir / "pyvenv"
  if pyvenv_dir.exists():
    count += fix_text_in_tree(pyvenv_dir, replacements, label_root=target_dir)

  # Also fix shebangs in pyvenv/bin
  count += fix_shebangs_in_dir(target_dir / "pyvenv" / "bin", replacements)

  print(deco.val(f"    Fixed {count} text files total"))

###############################################################################
# Phase 2: Mach-O Relocation
###############################################################################

def phase2_relocate(target_dir, old_staging_dir, homebrew_dir="/opt/homebrew"):
  """Rewrite all Mach-O load commands for @rpath-based relocation.

  Operates only on the target copy — never touches the original staging dir.

  Args:
    target_dir: The deployment copy to relocate
    old_staging_dir: Original staging dir path (for reference replacement)
    homebrew_dir: Homebrew directory path (for reference replacement)
  """
  target_dir = path.Path(target_dir)

  print(deco.val("=" * 60))
  print(deco.val(f"Phase 2: {'ELF' if IS_LINUX else 'Mach-O'} Relocation"))
  print(deco.val("=" * 60))

  if IS_LINUX:
    # ELF: rewrite every object's RPATH to a $ORIGIN-relative reach to lib/.
    # NEEDED entries are bare SONAMEs (resolved via RPATH) so there is no
    # per-dependency rewrite; there is no id-change and no codesign. The
    # $ORIGIN rpath (DT_RPATH, transitive) makes symlink farms unnecessary.
    relocator = elf.ElfRelocator(target_dir)
    relocator.relocate_all()
    print(deco.val(f"\n  Phase 2 complete."))
    return

  relocator = macos.MachoRelocator(target_dir)
  relocator.relocate_all(old_staging_dir, homebrew_dir)

  # ---- Post-relocation fixup: create symlink farms ----
  # Some binaries (e.g. CPython lib-dynload) lack header padding to add longer
  # rpaths. Create symlinks so their shorter rpaths can still resolve dylibs.
  print(deco.val(f"\n  Creating symlink farms for rpath reach..."))
  create_symlink_farms(target_dir)

  print(deco.val(f"\n  Re-signing modified binaries..."))
  relocator.resign_all()

  print(deco.val(f"\n  Phase 2 complete."))

###############################################################################

def create_symlink_farms(target_dir):
  """Create symlinks so shorter rpaths can resolve dylibs from lib/.

  Some Mach-O binaries (especially CPython lib-dynload extensions) don't have
  enough header padding to add long rpath entries. Their existing short rpaths
  (e.g. @loader_path/../..) resolve to pyvenv/lib/, not the top-level lib/.

  This function creates symlinks in pyvenv/lib/ pointing to dylibs in lib/,
  ensuring those short rpaths work.

  """
  target_dir = path.Path(target_dir)
  lib_dir = target_dir / "lib"
  pyvenv_lib = target_dir / "pyvenv" / "lib"
  count = 0

  if not lib_dir.exists() or not pyvenv_lib.exists():
    return

  # Create symlinks in pyvenv/lib/ for all dylibs in lib/
  for item in sorted(lib_dir.iterdir()):
    if item.name.endswith('.dylib') and item.is_file():
      link_path = pyvenv_lib / item.name
      if not link_path.exists():
        # Compute relative path from pyvenv/lib/ to lib/
        rel = os.path.relpath(str(item), str(pyvenv_lib))
        os.symlink(rel, str(link_path))
        count += 1

  print(deco.val(f"    Created {count} symlinks in pyvenv/lib/ -> lib/"))

  pyvenv_site = target_dir / "pyvenv"

###############################################################################
# Phase 3: Verification
###############################################################################

def phase3_verify(target_dir):
  """Verify all Mach-O references resolve correctly.

  Args:
    target_dir: The relocated deployment to verify

  Returns:
    True if all references pass, False otherwise
  """
  target_dir = path.Path(target_dir)

  print(deco.val("=" * 60))
  print(deco.val("Phase 3: Verification"))
  print(deco.val("=" * 60))

  verifier = elf.ElfVerifier(target_dir) if IS_LINUX else macos.MachoVerifier(target_dir)
  ok = verifier.verify()
  verifier.dump_report()

  if ok:
    print(deco.val(f"\n  Phase 3 PASSED — deployment is relocatable!"))
  else:
    print(deco.val(f"\n  Phase 3 FAILED — see report above."))

  return ok

###############################################################################
# Phase 4: Include OBT Venv
###############################################################################

def phase4_obt_venv(target_dir, obt_venv_dir, homebrew_dir="/opt/homebrew", deploy_manifests=None):
  """Copy the OBT framework venv into the deployment and make it relocatable.

  The OBT venv contains the OBT framework package (obt.*), the env launcher
  (obt.env.launch.py), and helper scripts. It has its own Python interpreter
  (separate from the staging pyvenv).

  Args:
    target_dir: The deployment root directory
    obt_venv_dir: Source OBT venv directory ($VIRTUAL_ENV)
    homebrew_dir: Homebrew directory (for resolving framework deps)
  """
  target_dir = path.Path(target_dir)
  obt_venv_dir = path.Path(obt_venv_dir)
  target_venv = target_dir / "obt_venv"

  print(deco.val("=" * 60))
  print(deco.val("Phase 4: Include OBT Venv"))
  print(deco.val("=" * 60))
  print(deco.val(f"  Source:  {obt_venv_dir}"))
  print(deco.val(f"  Target:  {target_venv}"))

  if not obt_venv_dir.exists():
    print(deco.val(f"  ERROR: OBT venv not found: {obt_venv_dir}"))
    return False

  # ---- Step 1: Deep copy the venv ----
  print(deco.val(f"\n  Step 1: Copying OBT venv..."))
  if target_venv.exists():
    print(deco.val(f"    Removing existing obt_venv/..."))
    shutil.rmtree(str(target_venv))
  run(["cp", "-a", str(obt_venv_dir), str(target_venv)], do_log=False)
  print(deco.val(f"    Copied."))

  # ---- Step 2: Resolve Python symlink to real binary ----
  print(deco.val(f"\n  Step 2: Resolving Python interpreter..."))
  _resolve_venv_python(target_venv, homebrew_dir)

  # ---- Step 3: Internalize the Python runtime (interpreter shared lib + stdlib)
  if IS_LINUX:
    print(deco.val(f"\n  Step 3: Internalizing libpython + stdlib..."))
    _internalize_python_linux(target_dir, target_venv)
  else:
    print(deco.val(f"\n  Step 3: Internalizing Python framework..."))
    _internalize_python_framework(target_dir, target_venv, homebrew_dir)

  # ---- Step 4: Relocate native binaries in obt_venv ----
  print(deco.val(f"\n  Step 4: Relocating {'ELF' if IS_LINUX else 'Mach-O'} binaries..."))
  _relocate_obt_venv_machos(target_dir, target_venv, obt_venv_dir, homebrew_dir)

  # ---- Step 5: Fix text references ----
  print(deco.val(f"\n  Step 5: Fixing text references..."))
  _fix_obt_venv_text(target_dir, target_venv, obt_venv_dir, homebrew_dir)

  # ---- Step 6: Copy env.common.sh ----
  print(deco.val(f"\n  Step 6: Bundling env.common.sh..."))
  _bundle_env_common(target_dir, deploy_manifests or [])

  # ---- Step 7: Prune __pycache__ from the copied obt_venv ----
  print(deco.val(f"\n  Step 7: Pruning __pycache__ from obt_venv..."))
  _pc_count, _pc_bytes = _prune_pycache(target_venv)
  print(deco.val(f"    Removed {_pc_count} __pycache__ dirs ({_pc_bytes // (1024 * 1024)} MB)"))

  # ---- Step 8: Prune build-time/GUI/test stdlib from obt_venv ----
  print(deco.val(f"\n  Step 8: Pruning stdlib cruft from obt_venv..."))
  _sl_count, _sl_bytes = _prune_stdlib(target_venv)
  print(deco.val(f"    Removed {_sl_count} stdlib entries ({_sl_bytes // (1024 * 1024)} MB)"))

  print(deco.val(f"\n  Phase 4 complete."))
  return True

###############################################################################

def _resolve_venv_python(target_venv, homebrew_dir):
  """Replace symlinked python binary with a copy of the real binary."""
  target_bin = target_venv / "bin"

  # Find the versioned python (e.g. python3.14) — it's the symlink to homebrew
  python_link = None
  python_version = None
  for item in sorted(os.listdir(str(target_bin))):
    full = target_bin / item
    if item.startswith("python3.") and not item.endswith("-config"):
      if os.path.islink(str(full)):
        link_target = os.readlink(str(full))
        real_target = os.path.realpath(str(full))
        # Linux: any base interpreter outside the venv (usually /usr/bin/pythonX.Y).
        # macOS: the homebrew python symlink specifically.
        outside_venv = not real_target.startswith(str(target_venv) + os.sep)
        if (outside_venv if IS_LINUX
            else (homebrew_dir in link_target or "/opt/" in link_target)):
          python_link = full
          python_version = item
          break

  if not python_link:
    print(deco.val(f"    No homebrew python symlink found — skipping."))
    return

  # Resolve to the real binary
  real_binary = os.path.realpath(str(python_link))
  if not os.path.isfile(real_binary):
    print(deco.val(f"    WARNING: Real binary not found: {real_binary}"))
    return

  # Replace the symlink with a copy of the real binary
  os.unlink(str(python_link))
  shutil.copy2(real_binary, str(python_link))
  os.chmod(str(python_link), 0o755)
  print(deco.val(f"    Replaced {python_version} symlink with real binary"))
  print(deco.val(f"    Source: {real_binary}"))

  # Fix the convenience symlinks (python, python3) to point at the bundled
  # versioned interpreter.
  for alias in ["python", "python3"]:
    alias_path = target_bin / alias
    if os.path.islink(str(alias_path)):
      link_dest = os.readlink(str(alias_path))
      if link_dest == python_version:
        print(deco.val(f"    {alias} -> {python_version} (ok)"))
      elif IS_LINUX:
        # On Linux a venv ships these as ABSOLUTE symlinks to the base
        # interpreter (e.g. /usr/bin/python3); left as-is the deployed launcher
        # would run the HOST/runtime python (wrong version, no obt) instead of
        # the bundled one. Repoint them relatively. (macOS: unchanged — the
        # original only reported, never rewrote these.)
        os.unlink(str(alias_path))
        os.symlink(python_version, str(alias_path))
        print(deco.val(f"    Repointed {alias} -> {python_version} (was {link_dest})"))

def _internalize_python_framework(target_dir, target_venv, homebrew_dir):
  """Copy the Python framework dylib into target/lib/ for the OBT venv's python."""
  target_lib = target_dir / "lib"
  target_lib.mkdir(parents=True, exist_ok=True)

  # Find the python binary and discover its framework dependency
  python_bin = None
  for item in sorted(os.listdir(str(target_venv / "bin"))):
    full = str(target_venv / "bin" / item)
    if item.startswith("python3.") and not item.endswith("-config"):
      if os.path.isfile(full) and not os.path.islink(full):
        if macos.is_macho_binary(full):
          python_bin = full
          break

  if not python_bin:
    print(deco.val(f"    No Mach-O python binary found in obt_venv/bin/."))
    return

  # Get its deps and find the framework reference
  deps = macos.macho_enumerate_dylibs(python_bin)
  framework_dep = None
  for dep in deps:
    if ".framework/" in dep and "Python" in dep:
      framework_dep = dep
      break

  if not framework_dep:
    print(deco.val(f"    No Python framework dependency found."))
    return

  # The framework dep looks like:
  #   /opt/homebrew/Cellar/python@3.14/.../Frameworks/Python.framework/Versions/3.14/Python
  # We want just the "Python" dylib basename
  fw_basename = os.path.basename(framework_dep)  # "Python"
  dest_path = target_lib / fw_basename

  if not dest_path.exists():
    # Resolve to real file and copy
    real_fw = os.path.realpath(framework_dep)
    if not os.path.isfile(real_fw):
      print(deco.val(f"    WARNING: Framework dylib not found: {framework_dep}"))
      return

    shutil.copy2(real_fw, str(dest_path))
    print(deco.val(f"    Copied {fw_basename} ({os.path.getsize(str(dest_path)) // 1024 // 1024}MB) to lib/"))
    print(deco.val(f"    Source: {real_fw}"))

    # Set the dylib's install name ID to @rpath/Python
    new_id = "@rpath/" + fw_basename
    macos.macho_change_id(str(dest_path), new_id)
    print(deco.val(f"    Set ID: {new_id}"))

    # Re-sign
    run(["codesign", "--force", "--sign", "-", str(dest_path)], do_log=False)
  else:
    print(deco.val(f"    {fw_basename} already in lib/"))

  # Create the Python.app structure that framework-built Python expects.
  # Python tries to posix_spawn through lib/Resources/Python.app/Contents/MacOS/Python
  # relative to the framework dylib location.
  _create_python_app_stub(target_lib, framework_dep)

  # Copy the Python standard library so the interpreter works without homebrew.
  # The stdlib lives in the framework at .../Versions/3.14/lib/python3.14/
  fw_version_dir = os.path.dirname(framework_dep)  # .../Versions/3.14/
  fw_stdlib_dir = os.path.join(fw_version_dir, "lib")
  # Discover the python version subdir dynamically
  if os.path.isdir(fw_stdlib_dir):
    for item in os.listdir(fw_stdlib_dir):
      if item.startswith("python3.") and os.path.isdir(os.path.join(fw_stdlib_dir, item)):
        src_stdlib = os.path.join(fw_stdlib_dir, item)
        # Copy into obt_venv/lib/pythonX.Y/ (merging with existing site-packages)
        dst_stdlib = str(target_venv / "lib" / item)
        # Homebrew's Python 3.14 framework ships a sitecustomize.py that
        # hijacks site.PREFIXES to include /opt/homebrew and rewrites
        # sys.base_prefix / sys.executable to Homebrew-specific paths.
        # Dragging it into a relocatable bundle leaks the dev host's
        # Homebrew layout into every shipped copy; on user machines it
        # injects /opt/homebrew/lib/python3.14/site-packages onto sys.path
        # (which, via _obt_config.py, used to leak into PYTHONPATH and
        # kill the 3.12 child at `import numpy`). Skip it during copy
        # and scrub any stale bytecode afterward.
        _customize_names = ("sitecustomize", "usercustomize")
        if os.path.isdir(dst_stdlib):
          # Merge: copy only what's not already there (site-packages already exists)
          for sub in os.listdir(src_stdlib):
            src_sub = os.path.join(src_stdlib, sub)
            dst_sub = os.path.join(dst_stdlib, sub)
            if sub == "site-packages":
              continue  # Don't overwrite our venv's site-packages
            if sub in (f"{n}.py" for n in _customize_names):
              continue  # Don't inherit Homebrew's sitecustomize
            if not os.path.exists(dst_sub):
              if os.path.isdir(src_sub):
                run(["cp", "-a", src_sub, dst_sub], do_log=False)
              else:
                shutil.copy2(src_sub, dst_sub)
          print(deco.val(f"    Copied stdlib ({item}) into obt_venv/lib/ (merged)"))
        else:
          run(["cp", "-a", src_stdlib, dst_stdlib], do_log=False)
          print(deco.val(f"    Copied stdlib ({item}) into obt_venv/lib/"))
          # cp -a above pulled in sitecustomize.py; delete it.
          for _n in _customize_names:
            _py = os.path.join(dst_stdlib, f"{_n}.py")
            if os.path.isfile(_py):
              os.remove(_py)
        # Scrub any sitecustomize/usercustomize bytecode from __pycache__
        # regardless of which copy branch ran. Python will still start fine
        # without a .py even if a stale .pyc is present, but the bundle is
        # cleaner without them.
        _cache = os.path.join(dst_stdlib, "__pycache__")
        if os.path.isdir(_cache):
          for _f in os.listdir(_cache):
            if any(_f.startswith(f"{_n}.") and _f.endswith(".pyc")
                   for _n in _customize_names):
              try:
                os.remove(os.path.join(_cache, _f))
              except OSError:
                pass
        break

def _create_python_app_stub(target_lib, framework_dep):
  """Create the Python.app stub that framework-built Python needs at launch.

  Framework-built Python tries to exec through Resources/Python.app/Contents/MacOS/Python
  relative to the framework dylib's location. We create this minimal structure
  with the binary rewritten to use @rpath.

  Args:
    target_lib: The deployment lib/ directory (where the Python dylib lives)
    framework_dep: Original absolute path to the framework Python dylib
  """
  # The framework dep path looks like:
  #   /opt/homebrew/.../Python.framework/Versions/3.14/Python
  # The Resources dir is at the same level:
  #   /opt/homebrew/.../Python.framework/Versions/3.14/Resources/Python.app/...
  fw_version_dir = os.path.dirname(framework_dep)  # .../Versions/3.14/
  src_app_binary = os.path.join(fw_version_dir,
    "Resources", "Python.app", "Contents", "MacOS", "Python")
  src_info_plist = os.path.join(fw_version_dir,
    "Resources", "Python.app", "Contents", "Info.plist")
  src_pkg_info = os.path.join(fw_version_dir,
    "Resources", "Python.app", "Contents", "PkgInfo")

  if not os.path.isfile(src_app_binary):
    print(deco.val(f"    No Python.app found in framework — skipping stub."))
    return

  # Create the directory structure relative to target_lib (where Python dylib is)
  app_macos_dir = target_lib / "Resources" / "Python.app" / "Contents" / "MacOS"
  app_macos_dir.mkdir(parents=True, exist_ok=True)

  # Copy the Python.app binary
  dst_app_binary = str(app_macos_dir / "Python")
  shutil.copy2(src_app_binary, dst_app_binary)
  os.chmod(dst_app_binary, 0o755)

  # Copy Info.plist and PkgInfo if they exist
  contents_dir = app_macos_dir / ".."
  for src_file in [src_info_plist, src_pkg_info]:
    if os.path.isfile(src_file):
      shutil.copy2(src_file, str(contents_dir / os.path.basename(src_file)))

  # Rewrite the app binary's framework dep to @rpath/Python
  deps = macos.macho_enumerate_dylibs(dst_app_binary)
  for dep in deps:
    if ".framework/" in dep and "Python" in dep:
      new_dep = "@rpath/" + os.path.basename(dep)
      run(["install_name_tool", "-change", dep, new_dep, dst_app_binary], do_log=False)
      break

  # Add rpath from the app binary to lib/ (where Python dylib lives)
  # Path: lib/Resources/Python.app/Contents/MacOS/ → lib/ = @loader_path/../../../../
  rp_to_lib = "@loader_path/../../../../"
  run(["install_name_tool", "-add_rpath", rp_to_lib, dst_app_binary], do_log=False)

  # Re-sign
  run(["codesign", "--force", "--sign", "-", dst_app_binary], do_log=False)

  print(deco.val(f"    Created Python.app stub in lib/Resources/"))

def _internalize_python_linux(target_dir, target_venv):
  """Internalize the OBT venv's Python runtime on Linux: copy libpython*.so into
  lib/ (if the interpreter links it dynamically), $ORIGIN-relocate the
  interpreter, and copy the base stdlib next to the venv so it runs without the
  host python. The ELF analog of _internalize_python_framework (no framework,
  no Python.app stub, no codesign)."""
  target_dir = path.Path(target_dir)
  target_lib = target_dir / "lib"
  target_lib.mkdir(parents=True, exist_ok=True)

  # Locate the real ELF interpreter (Step 2 already replaced the symlink with a copy)
  python_bin = None
  py_version = None
  for item in sorted(os.listdir(str(target_venv / "bin"))):
    full = str(target_venv / "bin" / item)
    if item.startswith("python3.") and not item.endswith("-config"):
      if os.path.isfile(full) and not os.path.islink(full) and elf.is_elf_binary(full):
        python_bin = full
        py_version = item          # e.g. "python3.12"
        break
  if not python_bin:
    print(deco.val(f"    No ELF python interpreter found in obt_venv/bin/."))
    return

  # Copy libpython*.so into lib/ if the interpreter depends on it dynamically
  libpy = None
  for soname, tgt in elf.ldd_resolve(python_bin).items():
    if soname.startswith("libpython") and tgt:
      if elf.internalize_lib(tgt, target_lib):
        libpy = soname
        print(deco.val(f"    Internalized {soname} -> lib/"))
  if not libpy:
    print(deco.val(f"    (interpreter has no dynamic libpython — statically linked)"))

  # $ORIGIN-relocate the interpreter so it finds libpython in lib/
  elf.ElfRelocator(target_dir).relocate_binary(python_bin)

  # Copy the base stdlib next to the venv (base prefix from pyvenv.cfg 'home')
  base_prefix = None
  cfg = target_venv / "pyvenv.cfg"
  if cfg.exists():
    for line in open(str(cfg)):
      if line.strip().startswith("home"):
        home = line.split("=", 1)[1].strip()
        base_prefix = os.path.dirname(home.rstrip("/"))   # /usr/bin -> /usr
        break
  if not base_prefix:
    base_prefix = os.path.dirname(os.path.dirname(os.path.realpath(python_bin)))
  src_stdlib = os.path.join(base_prefix, "lib", py_version)
  dst_stdlib = str(target_venv / "lib" / py_version)
  if not os.path.isdir(src_stdlib):
    print(deco.val(f"    WARNING: base stdlib not found: {src_stdlib}"))
    return
  if os.path.isdir(dst_stdlib):
    for sub in os.listdir(src_stdlib):
      if sub == "site-packages":
        continue                       # keep the venv's own site-packages
      src_sub = os.path.join(src_stdlib, sub)
      dst_sub = os.path.join(dst_stdlib, sub)
      if not os.path.exists(dst_sub):
        if os.path.isdir(src_sub):
          run(["cp", "-a", src_sub, dst_sub], do_log=False)
        else:
          shutil.copy2(src_sub, dst_sub)
    print(deco.val(f"    Copied stdlib ({py_version}) into obt_venv/lib/ (merged)"))
  else:
    run(["cp", "-a", src_stdlib, dst_stdlib], do_log=False)
    print(deco.val(f"    Copied stdlib ({py_version}) into obt_venv/lib/"))

  # Debian/Ubuntu's python patches site.py to look for 'dist-packages' instead
  # of 'site-packages'. Once the venv is self-hosting (home points in-bundle so
  # prefix==base_prefix), site.py treats it as a base Debian install and scans
  # dist-packages — missing the venv's actual packages (obt, ork.build, ...) in
  # site-packages. Symlink dist-packages -> site-packages so they're found under
  # either convention.
  sp = os.path.join(dst_stdlib, "site-packages")
  dp = os.path.join(dst_stdlib, "dist-packages")
  if os.path.isdir(sp) and not os.path.exists(dp):
    os.symlink("site-packages", dp)
    print(deco.val(f"    Linked dist-packages -> site-packages (Debian site.py compat)"))

def _relocate_obt_venv_machos(target_dir, target_venv, old_venv_dir, homebrew_dir):
  """Relocate native binaries in the OBT venv (ELF on Linux, Mach-O on macOS)."""
  target_dir = path.Path(target_dir)
  lib_dir = target_dir / "lib"

  # ---- Linux: $ORIGIN-relocate every ELF in obt_venv (incl. the interpreter) ----
  if IS_LINUX:
    all_elfs = elf.discover_elf_files(target_venv)
    relocator = elf.ElfRelocator(target_dir)
    for e in all_elfs:
      try:
        relocator.relocate_binary(e)
      except Exception as ex:
        print(deco.val(f"    WARNING: relocate failed for {e}: {ex}"))
    print(deco.val(f"    Relocated {len(all_elfs)} ELF binaries in obt_venv/"))
    return

  # Find all Mach-O files in the obt_venv
  all_machos = macos.discover_macho_files(target_venv)

  # discover_macho_files misses files with numeric extensions like python3.14
  # Explicitly find the venv's python binary
  for item in sorted(os.listdir(str(target_venv / "bin"))):
    if item.startswith("python3.") and not item.endswith("-config"):
      full = target_venv / "bin" / item
      if not os.path.islink(str(full)) and os.path.isfile(str(full)):
        if macos.is_macho_binary(str(full)):
          if full not in all_machos and str(full) not in [str(m) for m in all_machos]:
            all_machos.append(path.Path(str(full)))

  print(deco.val(f"    Found {len(all_machos)} Mach-O files in obt_venv/"))

  old_prefixes = [
    str(old_venv_dir),
    str(homebrew_dir),
    os.path.join(str(homebrew_dir), "opt"),
    os.path.join(str(homebrew_dir), "Cellar"),
  ]

  for macho in all_machos:
    macho_str = str(macho)
    basename = os.path.basename(macho_str)
    is_dylib = basename.endswith('.dylib')

    # --- Fix install name ID for dylibs ---
    if is_dylib:
      new_id = "@rpath/" + basename
      macos.macho_change_id(macho_str, new_id)

    # --- Fix LC_RPATH entries ---
    current_rpaths = macos.macho_enumerate_rpaths(macho_str)
    for rp in current_rpaths:
      if not rp.startswith("@"):
        run(["install_name_tool", "-delete_rpath", rp, macho_str], do_log=False)

    # Add rpath to target/lib/
    rp_to_lib = macos.MachoRelocator.compute_loader_path_to(macho, lib_dir)
    current_after = macos.macho_enumerate_rpaths(macho_str)
    if rp_to_lib not in current_after:
      run(["install_name_tool", "-add_rpath", rp_to_lib, macho_str], do_log=False)

    # --- Fix dependency references ---
    deps = macos.macho_enumerate_dylibs(macho_str)
    for dep in deps:
      new_dep = None
      if dep.startswith("@rpath/") or dep.startswith("@loader_path/"):
        continue
      if dep.startswith("/System/") or dep.startswith("/usr/lib/"):
        continue
      # Framework reference (Python.framework etc)
      if ".framework/" in dep:
        new_dep = "@rpath/" + os.path.basename(dep)
      # Homebrew or old venv absolute path
      else:
        for prefix in old_prefixes:
          if dep.startswith(prefix):
            new_dep = "@rpath/" + os.path.basename(dep)
            break

      if new_dep and new_dep != dep:
        run(["install_name_tool", "-change", dep, new_dep, macho_str], do_log=False)

    # Re-sign
    run(["codesign", "--force", "--sign", "-", macho_str], do_log=False)

  print(deco.val(f"    Relocated and signed {len(all_machos)} binaries"))

def _fix_obt_venv_text(target_dir, target_venv, old_venv_dir, homebrew_dir):
  """Fix text references in the OBT venv."""
  old_venv_str = str(old_venv_dir)
  new_venv_str = str(target_venv)
  homebrew_str = str(homebrew_dir)

  # Replacements for general text files
  replacements = [(old_venv_str, new_venv_str)]

  # Fix text files in the whole venv tree
  count = fix_text_in_tree(target_venv, replacements, label_root=target_dir)

  # Fix shebangs
  count += fix_shebangs_in_dir(target_venv / "bin", replacements)

  # Special: fix pyvenv.cfg which has homebrew paths for home/executable
  pyvenv_cfg = target_venv / "pyvenv.cfg"
  if pyvenv_cfg.exists():
    with open(str(pyvenv_cfg), 'r') as f:
      content = f.read()
    new_content = content
    # Replace homebrew python home with our venv bin
    # e.g. "home = /opt/homebrew/opt/python@3.14/bin" → "home = <target_venv>/bin"
    new_lines = []
    for line in new_content.splitlines():
      # On Linux the base python (home=/usr/bin) is NOT bundled — so point the
      # venv at ITSELF: the bundled interpreter + the stdlib copied into
      # obt_venv/lib/pythonX.Y by _internalize_python_linux. This makes the venv
      # self-contained (works even where the host base python is absent or a
      # different version, e.g. inside a Flatpak runtime). These paths get
      # sentinelized in Phase 6 so they relocate at launch.
      if line.startswith("home = ") and (IS_LINUX or homebrew_str in line):
        new_lines.append(f"home = {new_venv_str}/bin")
      elif line.startswith("executable = ") and (IS_LINUX or homebrew_str in line):
        # Find the python version from the existing executable line
        py_basename = os.path.basename(line.split(" = ", 1)[1].strip())
        new_lines.append(f"executable = {new_venv_str}/bin/{py_basename}")
      elif line.startswith("command = "):
        # Rewrite the whole command to reference our relocated paths
        py_basename = line.split()[2] if len(line.split()) > 2 else "python3"
        py_basename = os.path.basename(py_basename)
        new_lines.append(f"command = {new_venv_str}/bin/{py_basename} -m venv {new_venv_str}")
      else:
        new_lines.append(line)
    new_content = "\n".join(new_lines) + "\n"
    if new_content != content:
      with open(str(pyvenv_cfg), 'w') as f:
        f.write(new_content)
      count += 1
      print(deco.val(f"    Fixed: obt_venv/pyvenv.cfg"))

  print(deco.val(f"    Fixed {count} text files total"))

def _bundle_env_common(target_dir, deploy_manifests):
  """Generate env.common.sh from manifest-declared environment variables."""
  target_dir = path.Path(target_dir)
  obt_config_dir = target_dir / "obt_config"
  obt_config_dir.mkdir(parents=True, exist_ok=True)
  dst = obt_config_dir / "env.common.sh"

  # Collect env var names from all project manifests
  env_vars = []
  for m in deploy_manifests:
    env_vars.extend(m.get("environment", []))

  with open(str(dst), 'w') as f:
    f.write("# env.common.sh — generated by deploy from project manifests\n")
    f.write("# This file is sourced by obt-launch-env before launching OBT.\n\n")
    for var in env_vars:
      value = os.environ.get(var, "")
      f.write(f'export {var}="{value}"\n')

  print(deco.val(f"    Generated env.common.sh ({len(env_vars)} vars from {len(deploy_manifests)} project(s))"))

###############################################################################
# Phase 5: Include Project Runtime Files
###############################################################################

def phase5_projects(target_dir, project_dirs):
  """Copy runtime files from each project into the deployment.

  Projects are discovered from $OBT_PROJECT_DIRS — never hardcoded.
  Each project can provide obt.project/bin/ork.deploy.project.py that
  outputs a JSON manifest of what to deploy. If absent, obt.project/
  is copied as the default.

  Args:
    target_dir: The deployment root directory
    project_dirs: List of project root directories
  """
  target_dir = path.Path(target_dir)
  projects_dir = target_dir / "projects"

  print(deco.val("=" * 60))
  print(deco.val("Phase 5: Include Project Runtime Files"))
  print(deco.val("=" * 60))
  print(deco.val(f"  {len(project_dirs)} projects to process"))

  projects_dir.mkdir(parents=True, exist_ok=True)

  manifest_entries = []

  for proj_root in project_dirs:
    proj_root = path.Path(proj_root)
    if not proj_root.exists():
      print(deco.val(f"\n  WARNING: Project not found: {proj_root}"))
      continue

    proj_name = proj_root.name

    # Read obt.manifest for the canonical project name
    obt_manifest = proj_root / "obt.project" / "obt.manifest"
    canonical_name = proj_name
    if obt_manifest.exists():
      try:
        with open(str(obt_manifest)) as f:
          mdata = json.loads(f.read())
          canonical_name = mdata.get("name", proj_name)
      except:
        pass

    print(deco.val(f"\n  Project: {proj_name} (manifest name: {canonical_name})"))

    # Determine what to copy
    # Load deploy manifest by importing obt.project/deployment_manifest.py
    deploy_manifest_py = proj_root / "obt.project" / "deployment_manifest.py"
    deploy_mod = None
    if deploy_manifest_py.exists():
      deploy_manifest, deploy_mod = _load_deploy_manifest(deploy_manifest_py)
    else:
      deploy_manifest = _default_deploy_manifest(proj_root)

    # Copy declared content
    proj_target = projects_dir / proj_name
    if proj_target.exists():
      shutil.rmtree(str(proj_target))
    proj_target.mkdir(parents=True, exist_ok=True)

    copied_dirs = []
    for rel_dir in deploy_manifest.get("dirs", []):
      src = proj_root / rel_dir
      dst = proj_target / rel_dir
      if src.exists():
        print(deco.val(f"    Copying {rel_dir}/..."))
        dst.parent.mkdir(parents=True, exist_ok=True)
        run(["cp", "-a", str(src), str(dst)], do_log=False)
        copied_dirs.append(rel_dir)
      else:
        print(deco.val(f"    Skipping {rel_dir}/ (not found)"))

    for rel_dir in deploy_manifest.get("optional_dirs", []):
      src = proj_root / rel_dir
      dst = proj_target / rel_dir
      if src.exists():
        print(deco.val(f"    Copying {rel_dir}/ (optional)..."))
        dst.parent.mkdir(parents=True, exist_ok=True)
        run(["cp", "-a", str(src), str(dst)], do_log=False)
        copied_dirs.append(rel_dir)
      else:
        print(deco.val(f"    Skipping {rel_dir}/ (optional, not found)"))

    for rel_file in deploy_manifest.get("files", []):
      src = proj_root / rel_file
      dst = proj_target / rel_file
      if src.exists():
        print(deco.val(f"    Copying {rel_file}"))
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(str(src), str(dst))
      else:
        print(deco.val(f"    Skipping {rel_file} (not found)"))

    # Selectively copy whitelisted subsets of large source trees (e.g. ork.data)
    for tree_rel, entries in deploy_manifest.get("tree_whitelists", {}).items():
      _copy_tree_whitelist(proj_root, proj_target, tree_rel, entries)
      if tree_rel not in copied_dirs:
        copied_dirs.append(tree_rel)

    # Copy deploy_libs into .staging/lib/ and relocate them
    for lib_rel in deploy_manifest.get("deploy_libs", []):
      src = proj_root / lib_rel
      if not src.exists():
        print(deco.val(f"    WARNING: deploy_lib not found: {lib_rel}"))
        continue
      dst = target_dir / "lib" / src.name
      print(deco.val(f"    Installing lib {src.name} → lib/"))
      shutil.copy2(str(src), str(dst))
      if IS_LINUX:
        if elf.is_elf_binary(str(dst)):
          elf.ElfRelocator(target_dir).relocate_binary(str(dst), is_dylib=True)
      elif macos.is_macho_binary(str(dst)):
        relocator = macos.MachoRelocator(target_dir)
        relocator.relocate_binary(str(dst), old_prefixes=[], is_dylib=True)
        subprocess.run(
          ["codesign", "--force", "--sign", "-", str(dst)],
          capture_output=True)

    # Fix text references: old project root → new project root
    old_proj_str = str(proj_root)
    new_proj_str = str(proj_target)
    replacements = [(old_proj_str, new_proj_str)]
    count = fix_text_in_tree(proj_target, replacements, label_root=target_dir)
    if count:
      print(deco.val(f"    Fixed {count} text files"))

    # Run project-specific deploy fixups if provided
    if deploy_mod and hasattr(deploy_mod, 'deploy_fixup'):
      print(deco.val(f"    Running deploy fixups..."))
      deploy_mod.deploy_fixup(target_dir, proj_root, proj_target)

    manifest_entries.append({
      "name": proj_name,
      "canonical_name": canonical_name,
      "source": str(proj_root),
      "dirs": copied_dirs,
    })

  # Write projects manifest
  manifest_path = projects_dir / "manifest.json"
  with open(str(manifest_path), 'w') as f:
    json.dump({"projects": manifest_entries}, f, indent=2)
  print(deco.val(f"\n  Wrote {manifest_path}"))

  # Prune __pycache__ from all copied project trees
  print(deco.val(f"\n  Pruning __pycache__ from projects..."))
  _pc_count, _pc_bytes = _prune_pycache(projects_dir)
  print(deco.val(f"    Removed {_pc_count} __pycache__ dirs ({_pc_bytes // (1024 * 1024)} MB)"))

  print(deco.val(f"\n  Phase 5 complete."))
  return True

###############################################################################

def _load_deploy_manifest(manifest_path):
  """Import a project's deployment_manifest.py and return its manifest dict + module."""
  import importlib.util
  spec = importlib.util.spec_from_file_location("deployment_manifest", str(manifest_path))
  mod = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(mod)
  return mod.manifest, mod

def _default_deploy_manifest(proj_root):
  """Generate a default deploy manifest — just obt.project/."""
  return {"dirs": ["obt.project"], "optional_dirs": []}

###############################################################################
# Phase 5.5: Dependency Module Deployment Fixups
###############################################################################

def phase5_5_dep_fixups(target_dir):
  """Run deployment_fixup() on all dep modules that provide it."""
  print(deco.val("=" * 60))
  print(deco.val("Phase 5.5: Dependency Deployment Fixups"))
  print(deco.val("=" * 60))
  from obt import dep
  for name in sorted(dep.enumerate().keys()):
    inst = dep.instance(name)
    if inst and hasattr(inst, "deployment_fixup"):
      print(deco.val(f"  {name}"))
      inst.deployment_fixup(str(path.Path(target_dir)))
  return True

###############################################################################
# Phase 6: Launch Script + Shebang Fixup
###############################################################################

_LAUNCH_SCRIPT_TEMPLATE = r'''#!/usr/bin/env bash
###############################################################################
# obt-launch-env — Relocatable OBT environment launcher
# Generated by ork deploy (Phase 6)
#
# This script is self-locating: it computes all paths relative to its own
# location, so the deployment can be moved anywhere.
###############################################################################

# Compute DEPLOY_ROOT from this script's location.
# -P / pwd -P resolves symlinks to their physical target so that a symlink
# pointing at .staging (e.g. from a nested utilities folder) doesn't make
# DEPLOY_ROOT differ string-wise from .deploy_path on alternating launches.
# Without this, the relocation fixup ping-pongs ~170 files every time the
# user alternates between launching a symlink-routed app and a direct-path
# app — harmless per-launch but wasteful and confusing.
SCRIPT_DIR="$(cd -P "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
DEPLOY_ROOT="$SCRIPT_DIR"

# Sever all connections to any host OBT/Python environment.
# Without this, PYTHONPATH from the host leaks in and causes
# the deploy's Python to import obt from the host's site-packages,
# which then discovers source-tree projects instead of deploy projects.
unset PYTHONPATH
unset PYTHONHOME
unset VIRTUAL_ENV
unset OBT_STAGE
unset OBT_ROOT
unset OBT_PROJECT_DIRS
unset OBT_PROJECTS_LIST
unset OBT_SEARCH_PATH
unset OBT_BUILDS
unset OBT_SUBSPACE_DIR
unset OBT_SUBSPACE_LIB_DIR
unset OBT_SUBSPACE_BIN_DIR
unset OBT_SUBSPACE_BUILD_DIR
unset OBT_TARGET
unset ORKID_WORKSPACE_DIR
unset ORKID_ASSET_MANIFEST_DIRS
unset ORKID_IS_MAIN_PROJECT
unset LD_LIBRARY_PATH
unset DYLD_LIBRARY_PATH
unset DYLD_FALLBACK_LIBRARY_PATH
unset PKG_CONFIG
unset PKG_CONFIG_PATH
unset LUA_PATH
# Sever EVERY host OBT_*/ORKID_* var (belt-and-suspenders beyond the explicit
# unsets above). When launched from an existing OBT shell — or a sandbox like
# Flatpak that forwards the caller's environment — stale OBT_MODULES_PATH /
# OBT_BIN_PRIV_DIR / etc. would override the deploy's self-configuration and
# point back at host paths (loading host dep modules, prepending host bins).
for _v in ${!OBT_*} ${!ORKID_*}; do unset "$_v"; done
export PYTHONNOUSERSITE=1

# Bootstrap the OBT framework venv.
# The obt_venv's pyvenv.cfg already tells Python 3.14 where its stdlib is.
export VIRTUAL_ENV="$DEPLOY_ROOT/obt_venv"

# Build PATH: deploy dirs first, then system essentials.
# No homebrew — the deployment is fully self-contained.
export PATH="$DEPLOY_ROOT/obt_venv/bin:$DEPLOY_ROOT/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

# ---- Bash-level relocation fixup (runs BEFORE Python) ----
# Python cannot start if pyvenv.cfg contains stale paths, so the rewrite
# must happen in bash before we exec the interpreter.
#
# Bundle time writes every text file containing a build path with the
# sentinel __OBT_DEPLOY_SENTINEL__ in place of the real path, and emits
# .relocatable_files listing every such file. Launch time reads that
# manifest and substitutes sentinel -> DEPLOY_ROOT in each entry — no
# filesystem walk, O(N) where N = relocatable file count.
_MARKER_FILE="$DEPLOY_ROOT/.deploy_path"
_MANIFEST="$DEPLOY_ROOT/.relocatable_files"
_NEED_FIXUP=0
if [ -f "$_MARKER_FILE" ]; then
  _OLD_ROOT="$(cat "$_MARKER_FILE")"
  if [ "$_OLD_ROOT" != "$DEPLOY_ROOT" ]; then
    _NEED_FIXUP=1
  fi
else
  echo "$DEPLOY_ROOT" > "$_MARKER_FILE"
fi

if [ "$_NEED_FIXUP" -eq 1 ]; then
  echo "[deploy-fixup] Relocation detected: $_OLD_ROOT -> $DEPLOY_ROOT"
  if [ ! -f "$_MANIFEST" ]; then
    echo "[deploy-fixup] FATAL: manifest $_MANIFEST missing — cannot relocate" >&2
    exit 1
  fi
  _n=0
  while IFS= read -r _rel; do
    [ -z "$_rel" ] && continue
    _f="$DEPLOY_ROOT/$_rel"
    [ -f "$_f" ] || continue
    [ -L "$_f" ] && continue
    sed -i '' "s|$_OLD_ROOT|$DEPLOY_ROOT|g" "$_f" && _n=$((_n+1))
  done < "$_MANIFEST"
  echo "$DEPLOY_ROOT" > "$_MARKER_FILE"
  echo "[deploy-fixup] Done ($_n files updated)."
fi

# ---- Effective stage dir + writable caches ----
# OBT (Python) and the engine (C++, via $OBT_STAGE) resolve the mutable dirs as
# <stage>/{assetcache,envmaps,dblockcache,tempdir}. Those MUST be writable; the
# convention is to symlink them to the user's global ~/.obt-global/<name>.
#
#   * writable DEPLOY_ROOT  -> symlink the caches into it directly (classic).
#   * read-only DEPLOY_ROOT (a deployed bundle, e.g. a Flatpak /app) -> build a
#     writable "stage mirror" in $HOME: symlink every read-only tree entry, and
#     point the mutable dirs at ~/.obt-global. OBT_STAGE is then the mirror, so
#     every <stage>/assetcache lookup (Python and C++) lands in the writable
#     ~/.obt-global/assetcache. $ORIGIN rpaths still resolve (symlinks resolve to
#     the real /app/orkid/lib) and the sentinel-baked text paths stay readable.
_OG="$HOME/.obt-global"
mkdir -p "$_OG/assetcache" "$_OG/envmaps" "$_OG/dblockcache" "$_OG/tempdir"
_MUTABLE="assetcache envmaps dblockcache tempdir"
STAGE_DIR="$DEPLOY_ROOT"
if [ -w "$DEPLOY_ROOT" ]; then
  for _w in assetcache envmaps; do
    [ -e "$DEPLOY_ROOT/$_w" ] || ln -s "$_OG/$_w" "$DEPLOY_ROOT/$_w" 2>/dev/null || true
  done
else
  STAGE_DIR="$_OG/stage"
  mkdir -p "$STAGE_DIR"
  for _e in "$DEPLOY_ROOT"/* "$DEPLOY_ROOT"/.[!.]*; do
    [ -e "$_e" ] || continue
    _b="$(basename "$_e")"
    case " $_MUTABLE " in *" $_b "*) continue ;; esac
    ln -sfn "$_e" "$STAGE_DIR/$_b"
  done
  for _w in $_MUTABLE; do
    ln -sfn "$_OG/$_w" "$STAGE_DIR/$_w"
  done
fi

# Source user/machine-specific configuration (audio devices, CDN keys, etc.)
if [ -f "$DEPLOY_ROOT/obt_config/env.common.sh" ]; then
  source "$DEPLOY_ROOT/obt_config/env.common.sh"
fi

# Discover projects: any directory under projects/ that contains obt.project/
OBT_PROJECT_ARGS=()
for proj_dir in "$DEPLOY_ROOT/projects"/*/; do
  if [ -d "${proj_dir}obt.project" ]; then
    # Strip trailing slash for clean path
    OBT_PROJECT_ARGS+=(--project "${proj_dir%/}")
  fi
done

# Detect CPU core count
NUM_CORES=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# Launch OBT environment — invoke python explicitly to bypass hardcoded shebangs
exec "$DEPLOY_ROOT/obt_venv/bin/python3" \
  "$DEPLOY_ROOT/obt_venv/bin/obt.env.launch.py" \
  --stagedir "$STAGE_DIR" \
  --numcores "$NUM_CORES" \
  "${OBT_PROJECT_ARGS[@]}" \
  "$@"
'''

def phase6_launch_script(target_dir):
  """Generate the obt-launch-env wrapper and fix obt_venv shebangs.

  This is the final phase: it produces the entry point that makes the
  deployment usable. After this, running target/obt-launch-env should
  drop the user into a fully configured OBT shell.

  Args:
    target_dir: The deployment root directory
  """
  target_dir = path.Path(target_dir)

  print(deco.val("=" * 60))
  print(deco.val("Phase 6: Launch Script + Shebang Fixup"))
  print(deco.val("=" * 60))

  # ---- Step 1: Generate obt-launch-env ----
  print(deco.val(f"\n  Step 1: Generating obt-launch-env..."))
  launch_script = target_dir / "obt-launch-env"
  launch_content = _LAUNCH_SCRIPT_TEMPLATE.lstrip('\n')
  if IS_LINUX:
    # GNU sed uses `sed -i` (no separate backup-suffix arg); BSD/macOS sed
    # requires `sed -i ''`. The template is authored BSD-style, so fix it here.
    launch_content = launch_content.replace("sed -i '' ", "sed -i ")
  with open(str(launch_script), 'w') as f:
    f.write(launch_content)
  os.chmod(str(launch_script), 0o755)
  print(deco.val(f"    Wrote: {launch_script}"))

  # ---- Step 2: Bundle host binaries (with full dylib severance) ----
  HOST_BINARIES = ["pkg-config", "rsvg-convert"]
  for i, bin_name in enumerate(HOST_BINARIES):
    label = f"2{'abcdefgh'[i]}" if i > 0 else "2"
    print(deco.val(f"\n  Step {label}: Ensuring {bin_name} is bundled..."))
    target_binary = target_dir / "bin" / bin_name
    if not target_binary.exists():
      internalize_host_binary(bin_name, target_dir)
    else:
      print(deco.val(f"    bin/{bin_name} already present"))

  # ---- Step 3: Create MoltenVK ICD manifest (macOS only) ----
  # MoltenVK is the Vulkan-on-Metal translation layer — macOS only. On Linux the
  # native Vulkan loader (libvulkan.so.1) and the GPU driver ICDs are host-
  # provided (see obt.linux keep-on-host policy / the flatpak GL extension), so
  # there is nothing to wire up here.
  if IS_LINUX:
    print(deco.val(f"\n  Step 3: MoltenVK ICD — skipped (Linux uses host Vulkan loader)"))
  else:
    # The vulkan dep module expects the ICD JSON at builds/moltenvk/Package/Latest/
    # MoltenVK/dylib/macOS/MoltenVK_icd.json (relative to OBT_STAGE).
    # The dylib itself is already in lib/libMoltenVk.dylib.
    print(deco.val(f"\n  Step 3: Setting up MoltenVK ICD..."))
    mvk_icd_dir = target_dir / "builds" / "moltenvk" / "Package" / "Latest" / "MoltenVK" / "dylib" / "macOS"
    mvk_icd_dir.mkdir(parents=True, exist_ok=True)
    mvk_icd_json = mvk_icd_dir / "MoltenVK_icd.json"
    mvk_dylib_link = mvk_icd_dir / "libMoltenVk.dylib"
    # Write ICD JSON pointing to co-located dylib
    with open(str(mvk_icd_json), 'w') as f:
      f.write('{\n')
      f.write('    "file_format_version" : "1.0.0",\n')
      f.write('    "ICD": {\n')
      f.write('        "library_path": "./libMoltenVK.dylib",\n')
      f.write('        "api_version" : "1.4.0",\n')
      f.write('        "is_portability_driver" : true\n')
      f.write('    }\n')
      f.write('}\n')
    # Symlink the dylib from lib/
    if not mvk_dylib_link.exists():
      rel = os.path.relpath(str(target_dir / "lib" / "libMoltenVk.dylib"), str(mvk_icd_dir))
      os.symlink(rel, str(mvk_dylib_link))
    print(deco.val(f"    Created ICD JSON + dylib symlink"))

  # ---- Step 4: Fix shebangs in obt_venv/bin/*.py ----
  print(deco.val(f"\n  Step 4: Fixing shebangs in obt_venv/bin/..."))
  venv_bin = target_dir / "obt_venv" / "bin"
  if not venv_bin.exists():
    print(deco.val(f"    WARNING: {venv_bin} not found — skipping."))
    return True

  portable_shebang = "#!/usr/bin/env python3"
  fixed_count = 0
  for fname in sorted(os.listdir(str(venv_bin))):
    if not fname.endswith('.py'):
      continue
    fpath = str(venv_bin / fname)
    if os.path.islink(fpath) or not os.path.isfile(fpath):
      continue
    try:
      with open(fpath, 'r', errors='replace') as f:
        content = f.read()
      first_line_end = content.index('\n') if '\n' in content else len(content)
      first_line = content[:first_line_end]
      if first_line.startswith('#!') and first_line != portable_shebang:
        # Check it's a python shebang (not env-based already-portable ones)
        if 'python' in first_line:
          content = portable_shebang + content[first_line_end:]
          with open(fpath, 'w') as f:
            f.write(content)
          fixed_count += 1
    except Exception:
      pass

  print(deco.val(f"    Fixed {fixed_count} shebangs to: {portable_shebang}"))

  # ---- Step 5: Sentinelize build paths and emit relocation manifest ----
  # Walk the deploy tree and replace every embedded reference to the build
  # path with the sentinel __OBT_DEPLOY_SENTINEL__. The launcher's bash
  # fixup substitutes sentinel -> DEPLOY_ROOT on first launch using the
  # manifest emitted here, so launch time never has to walk the tree.
  #
  # Binaries are skipped (null-byte heuristic). Symlinks are skipped.
  # The manifest excludes itself, the marker, and the assetcache.
  print(deco.val(f"\n  Step 5: Sentinelizing build paths..."))
  SENTINEL = "__OBT_DEPLOY_SENTINEL__"
  build_path = str(target_dir)
  build_path_bytes = build_path.encode('utf-8')
  sentinel_bytes = SENTINEL.encode('utf-8')
  manifest_path = target_dir / ".relocatable_files"
  EXCLUDE_DIRS = {'assetcache', 'envmaps'}
  EXCLUDE_FILES = {'.relocatable_files', '.deploy_path'}

  rel_files = []
  for root, dirs, files in os.walk(str(target_dir)):
    dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
    for fname in files:
      if fname in EXCLUDE_FILES:
        continue
      abs_path = os.path.join(root, fname)
      if os.path.islink(abs_path):
        continue
      try:
        with open(abs_path, 'rb') as f:
          content = f.read()
      except (OSError, IOError):
        continue
      # Skip binaries (null byte in first 8K is the standard heuristic)
      if b'\x00' in content[:8192]:
        continue
      if build_path_bytes not in content:
        continue
      try:
        with open(abs_path, 'wb') as f:
          f.write(content.replace(build_path_bytes, sentinel_bytes))
      except (OSError, IOError) as e:
        print(deco.val(f"    WARN: failed to rewrite {abs_path}: {e}"))
        continue
      rel_files.append(os.path.relpath(abs_path, str(target_dir)))

  rel_files.sort()
  with open(str(manifest_path), 'w') as f:
    for rel in rel_files:
      f.write(rel + '\n')
  print(deco.val(f"    Sentinelized {len(rel_files)} files"))
  print(deco.val(f"    Manifest: {manifest_path}"))

  # ---- Step 6: Write deploy path marker ----
  # Sentinel value matches what Step 5 wrote into the relocatable files,
  # so the launcher's first run sees marker != DEPLOY_ROOT, runs the
  # manifest-driven sed pass, and updates the marker to the real path.
  print(deco.val(f"\n  Step 6: Writing deploy path marker..."))
  marker_path = target_dir / ".deploy_path"
  with open(str(marker_path), 'w') as f:
    f.write(SENTINEL + '\n')
  print(deco.val(f"    Wrote: {marker_path} (sentinel — forces first-launch fixup)"))

  # ---- Step 7: Write deploy mode marker ----
  print(deco.val(f"\n  Step 7: Writing deploy mode marker..."))
  deploy_marker = target_dir / ".is_deploy"
  deploy_marker.touch()
  print(deco.val(f"    Wrote: {deploy_marker}"))

  print(deco.val(f"\n  Phase 6 complete."))
  return True

###############################################################################
# Phase 7: App Bundle Generation (config-driven)
###############################################################################

def _resolve_icon(png_path, cache_dir):
  """Convert a PNG to .icns, caching in cache_dir. Returns icns path or None."""
  from ork.deploy import png_to_icns

  if not png_path or not os.path.isfile(png_path):
    return None

  # Use a deterministic cache name based on the PNG basename
  basename = os.path.splitext(os.path.basename(png_path))[0]
  icns_path = path.Path(cache_dir) / f"{basename}.icns"
  if icns_path.exists():
    return str(icns_path)

  try:
    png_to_icns(png_path, str(icns_path))
    print(deco.val(f"    Converted: {os.path.basename(png_path)} -> {icns_path.name}"))
    return str(icns_path)
  except Exception as e:
    print(deco.val(f"    WARNING: Icon conversion failed for {png_path}: {e}"))
    return None


def _resolve_folder_icon(deploy_config, infra_dir):
  """Resolve the folder icon PNG from deploy_config.

  Supports two config keys (checked in order):
    "folder_icon"        — absolute path to a PNG
    "folder_icon_search" — list of relative paths under project dirs to search

  Returns:
    Absolute path to the PNG, or None.
  """
  # Direct path
  folder_icon = deploy_config.get("folder_icon")
  if folder_icon and os.path.isfile(folder_icon):
    return folder_icon

  # Search paths: look in deployed projects first, then source projects
  search_paths = deploy_config.get("folder_icon_search", [])
  for rel_path in search_paths:
    # Try inside the deployed projects dir
    candidate = path.Path(infra_dir) / "projects" / rel_path
    if candidate.exists():
      return str(candidate)
    # Try relative to OBT_PROJECT_DIRS
    proj_env = os.environ.get("OBT_PROJECT_DIRS", "")
    for proj_dir in proj_env.split(":"):
      if not proj_dir:
        continue
      # rel_path is like "orkid/ork.data/misc/OrkidLogo.png" where first
      # component is the project name; but the project_dir IS the project root
      parts = rel_path.split("/", 1)
      if len(parts) == 2:
        proj_name, inner = parts
        if os.path.basename(proj_dir) == proj_name:
          candidate = path.Path(proj_dir) / inner
          if candidate.exists():
            return str(candidate)

  return None


def phase7_app_bundles(infra_dir, visible_dir, deploy_config):
  """Generate macOS .app bundles from deploy_config.

  Args:
    infra_dir:     The .staging/ directory containing all runtime infrastructure
    visible_dir:   The user-visible deploy root (parent of .staging/)
    deploy_config: Dict with "folder_icon"/"folder_icon_search" and "apps" list
  """
  from ork.deploy import generate_app_bundle, set_folder_icon

  infra_dir = path.Path(infra_dir)
  visible_dir = path.Path(visible_dir)

  print(deco.val("=" * 60))
  print(deco.val("Phase 7: App Bundle Generation"))
  print(deco.val("=" * 60))

  # .app bundles go directly in the visible directory
  apps_dir = visible_dir

  # ---- Step 1: Resolve and convert icons ----
  print(deco.val(f"\n  Step 1: Converting icons..."))
  cache_dir = str(infra_dir)  # cache .icns in .staging/

  # Resolve the folder icon PNG
  folder_icon_png = _resolve_folder_icon(deploy_config, infra_dir)
  folder_icon_icns = _resolve_icon(folder_icon_png, cache_dir) if folder_icon_png else None

  if not folder_icon_icns:
    print(deco.val(f"    WARNING: No folder icon found — bundles may have no icon"))

  # ---- Step 1b: Locate the obt-app-launcher Mach-O binary ----
  # Used as CFBundleExecutable for windowed-mode apps (mode=gui, terminal=False).
  # Built mac-only by ork.core/tools/CMakeLists.txt and installed into the OBT
  # staging bin dir, which Phase 1 already copied into infra_dir/bin/.
  launcher_binary = infra_dir / "bin" / "obt-app-launcher"
  if launcher_binary.exists():
    print(deco.val(f"    Found windowed-mode launcher: bin/obt-app-launcher"))
    launcher_binary = str(launcher_binary)
  else:
    print(deco.val(f"    WARNING: bin/obt-app-launcher not found — windowed-mode "
                   f"apps will fail to generate. Build orkid (ork.core/tools) first."))
    launcher_binary = None

  # ---- Step 2: Generate app bundles ----
  print(deco.val(f"\n  Step 2: Generating app bundles..."))
  app_specs = deploy_config.get("apps", [])
  app_count = 0

  for spec in app_specs:
    # Resolve per-app icon: explicit PNG → .icns, else inherit folder icon
    app_icon_png = spec.get("icon")
    if app_icon_png and os.path.isfile(app_icon_png):
      app_icon_icns = _resolve_icon(app_icon_png, cache_dir)
    else:
      app_icon_icns = folder_icon_icns

    # Determine target directory (subfolder if specified)
    subfolder = spec.get("subfolder")
    if subfolder:
      target_dir = visible_dir / subfolder
      os.makedirs(str(target_dir), exist_ok=True)
    else:
      target_dir = apps_dir

    # Build the spec for generate_app_bundle (expects .icns path in "icon")
    bundle_spec = {
      "name": spec["name"],
      "command": spec.get("command", []),
      "mode": spec.get("mode", "gui"),
      "terminal": spec.get("terminal", True),
      "bundle_id": spec.get("bundle_id", f"com.tweakoz.obt.{spec['name'].lower()}"),
      "icon": app_icon_icns,
      "launcher_binary": launcher_binary,
    }
    try:
      app_path = generate_app_bundle(infra_dir, target_dir, bundle_spec)
      print(deco.val(f"    Created: {os.path.relpath(app_path, str(visible_dir))}"))
      app_count += 1
    except Exception as e:
      print(deco.val(f"    ERROR generating {spec['name']}: {e}"))

  # ---- Step 3: Set custom folder icon on the visible deploy directory ----
  print(deco.val(f"\n  Step 3: Setting folder icon..."))
  if folder_icon_icns:
    try:
      set_folder_icon(str(visible_dir), folder_icon_icns)
      print(deco.val(f"    Set folder icon on: {visible_dir}"))
    except Exception as e:
      print(deco.val(f"    WARNING: Could not set folder icon: {e}"))
  else:
    print(deco.val(f"    Skipped — no icon available"))

  print(deco.val(f"\n  Generated {app_count} app bundle(s) total"))
  print(deco.val(f"\n  Phase 7 complete."))
  return True

###############################################################################
# Phase 8: Archive
###############################################################################

def phase8_archive(target_dir, figma_json=None):
  """Create a .dmg archive with drag-and-drop layout.

  If figma_json is provided, reads the DMG window layout (size, icon
  positions) from the Figma design's Drag & Drop screen via
  SwiftFigmaDesign.dmg_layout(). Otherwise uses sensible defaults.

  Creates an Applications symlink and uses AppleScript to set the
  DMG window size, icon positions, and background.
  """
  import tempfile

  target_dir = path.Path(target_dir)
  dmg_path = target_dir.parent / f"{target_dir.name}.dmg"
  vol_name = target_dir.name

  print(deco.val("=" * 60))
  print(deco.val("Phase 8: Create .dmg Archive"))
  print(deco.val("=" * 60))

  # Load DMG layout from Figma if available
  layout = None
  if figma_json and os.path.exists(figma_json):
    try:
      from ork.ui.figma.swift_design import SwiftFigmaDesign
      design = SwiftFigmaDesign(figma_json)
      layout = design.dmg_layout()
      if layout:
        print(deco.val(f"  Figma DMG layout: {layout['window_width']}x{layout['window_height']}"))
    except Exception as e:
      print(deco.val(f"  Warning: could not load Figma layout: {e}"))

  if not layout:
    layout = {
      "window_width": 540, "window_height": 380, "bg_color": "#ffffff",
      "icon_size": 130,
      "app_icon_x": 140, "app_icon_y": 160,
      "apps_folder_x": 400, "apps_folder_y": 160,
    }

  if dmg_path.exists():
    print(deco.val(f"  Removing existing {dmg_path.name}..."))
    os.remove(str(dmg_path))

  print(deco.val(f"  Source:  {target_dir}"))
  print(deco.val(f"  Output:  {dmg_path}"))

  # Compute size with 20% headroom for HFS+ overhead
  print(deco.val(f"\n  Step 1: Measuring source size..."))
  result = subprocess.run(
    ["du", "-sm", str(target_dir)],
    capture_output=True, text=True, check=True)
  size_mb = int(result.stdout.split()[0])
  size_mb = int(size_mb * 1.2) + 10  # extra room for symlink + DS_Store
  print(deco.val(f"    Source: ~{size_mb // 1024}GB (with 20% headroom)"))

  with tempfile.TemporaryDirectory() as tmpdir:
    sparse_path = os.path.join(tmpdir, f"{vol_name}_rw.sparseimage")
    mount_point = os.path.join(tmpdir, "mnt")
    os.makedirs(mount_point, exist_ok=True)

    # Create sparse read-write image
    print(deco.val(f"\n  Step 2: Creating sparse image..."))
    subprocess.run([
      "hdiutil", "create",
      "-size", f"{size_mb}m",
      "-type", "SPARSE",
      "-fs", "HFS+",
      "-volname", vol_name,
      "-o", sparse_path.replace(".sparseimage", ""),
    ], check=True, capture_output=True)

    # Mount it
    print(deco.val(f"\n  Step 3: Mounting, copying, and setting layout..."))
    subprocess.run([
      "hdiutil", "attach", sparse_path,
      "-mountpoint", mount_point,
    ], check=True, capture_output=True)

    try:
      # Copy app contents
      subprocess.run([
        "ditto", str(target_dir), os.path.join(mount_point, vol_name),
      ], check=True)

      # Create Applications symlink for drag-and-drop
      apps_link = os.path.join(mount_point, "Applications")
      if not os.path.exists(apps_link):
        os.symlink("/Applications", apps_link)
        print(deco.val(f"    Created Applications symlink"))

      # Set DMG window layout via AppleScript
      w = layout["window_width"]
      h = layout["window_height"]
      icon_sz = layout["icon_size"]
      app_x = layout.get("app_icon_x", 140)
      app_y = layout.get("app_icon_y", 160)
      apps_x = layout.get("apps_folder_x", 400)
      apps_y = layout.get("apps_folder_y", 160)

      applescript = f'''
      tell application "Finder"
        tell disk "{vol_name}"
          open
          set current view of container window to icon view
          set toolbar visible of container window to false
          set statusbar visible of container window to false
          set bounds of container window to {{100, 100, {100 + w}, {100 + h}}}
          set theViewOptions to icon view options of container window
          set arrangement of theViewOptions to not arranged
          set icon size of theViewOptions to {icon_sz}
          set position of item "{vol_name}" of container window to {{{app_x}, {app_y}}}
          set position of item "Applications" of container window to {{{apps_x}, {apps_y}}}
          close
          open
          update without registering applications
          delay 2
          close
        end tell
      end tell
      '''
      print(deco.val(f"    Setting DMG window layout ({w}x{h}, icons at ({app_x},{app_y}), ({apps_x},{apps_y}))..."))
      result = subprocess.run(
        ["osascript", "-e", applescript],
        capture_output=True, text=True, timeout=30)
      if result.returncode != 0:
        print(deco.val(f"    Warning: AppleScript layout failed: {result.stderr.strip()}"))
      else:
        print(deco.val(f"    DMG window layout set"))

    finally:
      # Always detach
      subprocess.run(
        ["hdiutil", "detach", mount_point],
        capture_output=True)

    # Convert to compressed read-only DMG
    print(deco.val(f"\n  Step 4: Compressing to UDZO..."))
    subprocess.run([
      "hdiutil", "convert", sparse_path,
      "-format", "UDZO",
      "-o", str(dmg_path),
    ], check=True, capture_output=True)

  dmg_size_mb = dmg_path.stat().st_size / (1024 * 1024)
  print(deco.val(f"  Archive created: {dmg_path} ({dmg_size_mb:.0f} MB)"))
  print(deco.val(f"\n  Phase 8 complete."))
  return True

###############################################################################
# Main entry point
###############################################################################

def run_deploy(deploy_config):
  """Main entry point. Parses CLI args, runs phases, uses deploy_config for Phase 7.

  Args:
    deploy_config: Dict with app bundle configuration for Phase 7.
      Keys:
        "folder_icon": str — absolute path to PNG for folder icon
        "folder_icon_search": list[str] — relative paths to search for folder icon
        "apps": list[dict] — app bundle specs, each with:
          "name": str, "command": list, "mode": str, "bundle_id": str,
          "icon": str (optional, absolute path to PNG)
  """
  parser = argparse.ArgumentParser(
    description="Create a relocatable macOS deployment from OBT staging")

  parser.add_argument("--target", required=True,
    help="Target directory for the relocatable deployment")
  parser.add_argument("--staging", default=None,
    help="Source staging directory (default: $OBT_STAGE)")
  parser.add_argument("--phase", choices=["1", "2", "3", "4", "5", "5.5", "6", "7", "8", "all"], default="all",
    help="Run specific phase (default: all)")
  parser.add_argument("--force", action="store_true",
    help="Remove existing target before copying (Phase 1)")
  parser.add_argument("--homebrew", default="/opt/homebrew",
    help="Homebrew directory (default: /opt/homebrew)")
  parser.add_argument("--obt-venv", default=None,
    help="OBT framework venv directory (default: $VIRTUAL_ENV)")
  parser.add_argument("--project", action="append", default=None,
    help="Project directory to include (repeatable; default: $OBT_PROJECT_DIRS)")
  parser.add_argument("--walk-only", action="store_true",
    help="Only run the dependency walker and print manifest (no copy)")
  parser.add_argument("--verify-only", action="store_true",
    help="Only verify an existing deployment (skip phases 1-2)")

  args = parser.parse_args()

  # Resolve staging directory
  if args.staging:
    staging_dir = path.Path(args.staging)
  else:
    try:
      staging_dir = path.stage()
    except:
      print("ERROR: No --staging provided and $OBT_STAGE not set.")
      sys.exit(1)

  target_dir = path.Path(os.path.abspath(args.target))

  if not staging_dir.exists():
    print(f"ERROR: Staging directory not found: {staging_dir}")
    sys.exit(1)

  # Resolve OBT venv directory
  if args.obt_venv:
    obt_venv_dir = path.Path(args.obt_venv)
  else:
    venv_env = os.environ.get("VIRTUAL_ENV", "")
    if venv_env:
      obt_venv_dir = path.Path(venv_env)
    else:
      obt_venv_dir = None

  # Resolve project directories
  if args.project:
    project_dirs = [path.Path(os.path.abspath(p)) for p in args.project]
  else:
    proj_env = os.environ.get("OBT_PROJECT_DIRS", "")
    if proj_env:
      project_dirs = [path.Path(p) for p in proj_env.split(":") if p]
    else:
      project_dirs = None

  # Load deploy manifests early — needed for env generation in Phase 4
  deploy_manifests = []
  if project_dirs:
    for proj_root in project_dirs:
      proj_root = path.Path(proj_root)
      manifest_py = proj_root / "obt.project" / "deployment_manifest.py"
      if manifest_py.exists():
        manifest_dict, _ = _load_deploy_manifest(manifest_py)
        deploy_manifests.append(manifest_dict)
      else:
        deploy_manifests.append(_default_deploy_manifest(proj_root))

  # Special modes
  if args.walk_only:
    if IS_LINUX:
      walker = elf.ElfDependencyWalker(staging_dir)
    else:
      walker = macos.MachoDependencyWalker(staging_dir, args.homebrew)
    walker.walk()
    walker.dump_manifest()
    return

  # Infrastructure lives inside .staging/ — the user-visible top level
  # only contains .app bundles (and the hidden .staging directory).
  infra_dir = target_dir / ".staging"

  _orig_stdout = sys.stdout

  # Tee all output to /tmp, move into bundle at the end
  import tempfile
  _tmp_log_path = tempfile.mktemp(prefix="deploy_", suffix=".log")
  _log_file = open(_tmp_log_path, "w")
  sys.stdout = _TeeWriter(_orig_stdout, _log_file)

  if args.verify_only:
    ok = phase3_verify(infra_dir)
    sys.exit(0 if ok else 1)

  # Run requested phases
  phase = args.phase

  if phase in ("1", "all"):
    ok = phase1_copy(staging_dir, infra_dir, force=args.force)
    if not ok:
      sys.exit(1)

  if phase in ("2", "all"):
    phase2_relocate(infra_dir, staging_dir, args.homebrew)

  if phase in ("3", "all"):
    ok = phase3_verify(infra_dir)
    if not ok:
      if phase == "3":
        # Standalone verification — respect the failure
        sys.exit(1)
      else:
        # Part of full deploy — warn but continue
        print(deco.val("  WARNING: Verification found issues (see above)."))
        print(deco.val("  Continuing deployment — most failures are non-critical."))

  if phase in ("4", "all"):
    if obt_venv_dir is None:
      print("ERROR: No --obt-venv provided and $VIRTUAL_ENV not set.")
      sys.exit(1)
    if not obt_venv_dir.exists():
      print(f"ERROR: OBT venv not found: {obt_venv_dir}")
      sys.exit(1)
    ok = phase4_obt_venv(infra_dir, obt_venv_dir, args.homebrew, deploy_manifests)
    if not ok:
      sys.exit(1)

  if phase in ("5", "all"):
    if project_dirs is None:
      print("ERROR: No --project provided and $OBT_PROJECT_DIRS not set.")
      sys.exit(1)
    ok = phase5_projects(infra_dir, project_dirs)
    if not ok:
      sys.exit(1)

  if phase in ("5.5", "all"):
    ok = phase5_5_dep_fixups(infra_dir)
    if not ok:
      sys.exit(1)

  if phase in ("6", "all"):
    ok = phase6_launch_script(infra_dir)
    if not ok:
      sys.exit(1)

  # Phases 7 (.app bundles) and 8 (.dmg) are macOS-specific packaging. On Linux
  # the equivalent packaging is produced by a separate driver (relocatable
  # tarball / AppImage / Flatpak via ork.deploy.ix.flatpak.py), so phases 1-6
  # produce the self-contained relocatable tree and packaging stops here.
  if phase in ("7", "all"):
    if IS_LINUX:
      if phase == "7":
        print(deco.val("  Phase 7 (.app bundles) is macOS-only — nothing to do on Linux."))
    else:
      ok = phase7_app_bundles(infra_dir, target_dir, deploy_config)
      if not ok:
        sys.exit(1)

  if phase in ("8", "all"):
    if IS_LINUX:
      if phase == "8":
        print(deco.val("  Phase 8 (.dmg) is macOS-only — use the Linux packaging "
                       "driver (tarball / AppImage / flatpak) instead."))
    else:
      figma_json = deploy_config.get("figma_json") if deploy_config else None
      ok = phase8_archive(target_dir, figma_json=figma_json)
      if not ok:
        sys.exit(1)

  if phase == "all":
    print(deco.val("\n" + "=" * 60))
    print(deco.val("Deployment complete!"))
    print(deco.val("=" * 60))
    print(deco.val(f"  Location: {target_dir}"))
    print(deco.val(f"  Infrastructure: {infra_dir}"))
    print(deco.val(f"  App bundles visible at top level of {target_dir}"))

  sys.stdout = _orig_stdout
  _log_file.close()
  if infra_dir.exists():
    shutil.move(_tmp_log_path, str(infra_dir / "deploy.log"))
  else:
    os.unlink(_tmp_log_path)
