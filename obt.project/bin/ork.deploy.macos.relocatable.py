#!/usr/bin/env python3
###############################################################################
# ork.deploy.macos.relocatable.py
#
# Creates a fully self-contained, relocatable deployment from an OBT staging
# directory. The result can be moved to any macOS arm64 machine with zero
# external dependencies (no homebrew, no user-specific paths).
#
# Layout:
#   target/                    ← user-visible directory (with custom folder icon)
#     LaunchShell.app           ← visible .app bundles
#     OrkTestRunner.app
#     .staging/                 ← hidden infrastructure
#       obt-launch-env, bin/, lib/, obt_venv/, pyvenv/, projects/, ...
#
# Phases:
#   1 — Deep copy staging + internalize homebrew dylib closure
#   2 — Rewrite all Mach-O load commands for @rpath-based relocation
#   3 — Verify every reference resolves correctly
#   4 — Include OBT venv (framework + Python interpreter)
#   5 — Include project runtime files
#   6 — Generate launch script + fix shebangs
#   7 — Generate macOS .app bundles + set folder icon
#
# All discovery is dynamic — no hardcoded versions, paths, or file lists.
###############################################################################

import argparse
import json
import os
import shutil
import subprocess
import sys

from obt import path, pathtools, macos, deco as deco_mod
from obt.command import run

deco = deco_mod.Deco()

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

  # Check if it's a Mach-O binary (could be a script)
  if not macos.is_macho_binary(str(target_binary)):
    print(deco.val(f"    {binary_name} is not a Mach-O binary — no dylib fixup needed"))
    return True

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

# Directories to skip (build intermediates, headers, etc.)
SKIP_DIRS = {"builds", "include", "buildlogs",
             "nanobind", "sdks", "subspaces", "tempdir",
             "doc", "obt-launch-env", "dblockcache"}

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

  # ---- Step 1: Walk staging to discover homebrew closure ----
  print(deco.val(f"\n  Step 1: Discovering dependencies..."))
  walker = macos.MachoDependencyWalker(staging_dir)
  walker.walk()
  homebrew_closure = walker.get_homebrew_closure()
  print(deco.val(f"  Homebrew closure: {len(homebrew_closure)} dylibs"))

  # ---- Step 2: Copy runtime directories ----
  print(deco.val(f"\n  Step 2: Copying runtime directories..."))
  for dirname in RUNTIME_DIRS:
    src = staging_dir / dirname
    dst = target_dir / dirname
    if src.exists():
      print(deco.val(f"    Copying {dirname}/..."))
      # Use cp -a to preserve symlinks and permissions
      run(["cp", "-a", str(src), str(dst)], do_log=False)
    else:
      print(deco.val(f"    Skipping {dirname}/ (not found)"))

  # ---- Step 3: Copy homebrew dylib closure into target/lib/ ----
  print(deco.val(f"\n  Step 3: Internalizing homebrew dylibs..."))
  target_lib = target_dir / "lib"
  target_lib.mkdir(parents=True, exist_ok=True)
  copied_count = 0
  skipped_frameworks = []

  for hb_path in sorted(homebrew_closure):
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
  pyvenv_lib = target_dir / "pyvenv" / "lib"
  if pyvenv_lib.exists():
    for item in pyvenv_lib.iterdir():
      if item.name.startswith("libpython") and item.name.endswith(".dylib"):
        dest = target_lib / item.name
        if not dest.exists():
          if item.is_symlink():
            shutil.copy2(str(item.resolve()), str(dest))
          else:
            shutil.copy2(str(item), str(dest))
          print(deco.val(f"    Copied {item.name} to lib/"))
        else:
          print(deco.val(f"    {item.name} already in lib/"))

  # ---- Step 5: Fix text files with hardcoded staging paths ----
  print(deco.val(f"\n  Step 5: Fixing text files with hardcoded paths..."))
  fix_text_references(target_dir, staging_dir)

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

    # Use grep to find files containing the old string
    try:
      result = subprocess.run(
        ["grep", "-rl", old_str, str(search_root)],
        capture_output=True, text=True, timeout=60)
    except:
      continue

    for fpath in result.stdout.strip().splitlines():
      if not fpath or not os.path.isfile(fpath):
        continue
      if macos.is_macho_binary(fpath):
        continue
      if fpath.endswith('.pyc'):
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
  print(deco.val("Phase 2: Mach-O Relocation"))
  print(deco.val("=" * 60))

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

  Also handles torch libs for pytorch3d by symlinking into the nearest
  resolvable directory.
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

  # Handle torch libs for pytorch3d: find torch/lib/ and symlink into
  # the nearest directory reachable by pytorch3d's rpath
  pyvenv_site = target_dir / "pyvenv"
  # Discover torch lib dir dynamically
  torch_lib = None
  for root, dirs, files in os.walk(str(pyvenv_site)):
    if os.path.basename(root) == "lib" and "torch" in os.path.dirname(root):
      # Check if this is the torch/lib/ directory
      parent = os.path.basename(os.path.dirname(root))
      if parent == "torch":
        torch_lib = root
        break

  if torch_lib:
    # pytorch3d's rpath @loader_path/../../.. resolves to site-packages/
    # Symlink torch dylibs into site-packages/ so pytorch3d can find them
    site_packages = os.path.dirname(os.path.dirname(torch_lib))
    torch_count = 0
    for fname in sorted(os.listdir(torch_lib)):
      if fname.endswith('.dylib'):
        link_path = os.path.join(site_packages, fname)
        if not os.path.exists(link_path):
          rel = os.path.relpath(os.path.join(torch_lib, fname), site_packages)
          os.symlink(rel, link_path)
          torch_count += 1
    if torch_count:
      print(deco.val(f"    Created {torch_count} symlinks for torch libs"))

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

  verifier = macos.MachoVerifier(target_dir)
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

def phase4_obt_venv(target_dir, obt_venv_dir, homebrew_dir="/opt/homebrew"):
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

  # ---- Step 3: Copy Python framework dylib + stdlib into target ----
  print(deco.val(f"\n  Step 3: Internalizing Python framework..."))
  _internalize_python_framework(target_dir, target_venv, homebrew_dir)

  # ---- Step 4: Relocate Mach-O binaries in obt_venv ----
  print(deco.val(f"\n  Step 4: Relocating Mach-O binaries..."))
  _relocate_obt_venv_machos(target_dir, target_venv, obt_venv_dir, homebrew_dir)

  # ---- Step 5: Fix text references ----
  print(deco.val(f"\n  Step 5: Fixing text references..."))
  _fix_obt_venv_text(target_dir, target_venv, obt_venv_dir, homebrew_dir)

  # ---- Step 6: Copy env.common.sh ----
  print(deco.val(f"\n  Step 6: Bundling env.common.sh..."))
  _bundle_env_common(target_dir)

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
        if homebrew_dir in link_target or "/opt/" in link_target:
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

  # Fix the convenience symlinks (python, python3) to point locally
  for alias in ["python", "python3"]:
    alias_path = target_bin / alias
    if os.path.islink(str(alias_path)):
      link_dest = os.readlink(str(alias_path))
      # If it points to the versioned name, it's already correct (relative)
      if link_dest == python_version:
        print(deco.val(f"    {alias} -> {python_version} (ok)"))

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
        if os.path.isdir(dst_stdlib):
          # Merge: copy only what's not already there (site-packages already exists)
          for sub in os.listdir(src_stdlib):
            src_sub = os.path.join(src_stdlib, sub)
            dst_sub = os.path.join(dst_stdlib, sub)
            if sub == "site-packages":
              continue  # Don't overwrite our venv's site-packages
            if not os.path.exists(dst_sub):
              if os.path.isdir(src_sub):
                run(["cp", "-a", src_sub, dst_sub], do_log=False)
              else:
                shutil.copy2(src_sub, dst_sub)
          print(deco.val(f"    Copied stdlib ({item}) into obt_venv/lib/ (merged)"))
        else:
          run(["cp", "-a", src_stdlib, dst_stdlib], do_log=False)
          print(deco.val(f"    Copied stdlib ({item}) into obt_venv/lib/"))
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

def _relocate_obt_venv_machos(target_dir, target_venv, old_venv_dir, homebrew_dir):
  """Relocate Mach-O binaries in the OBT venv."""
  target_dir = path.Path(target_dir)
  lib_dir = target_dir / "lib"

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
      if line.startswith("home = ") and homebrew_str in line:
        new_lines.append(f"home = {new_venv_str}/bin")
      elif line.startswith("executable = ") and homebrew_str in line:
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

def _bundle_env_common(target_dir):
  """Copy ~/.obt-global/env.common.sh into the deployment."""
  target_dir = path.Path(target_dir)
  obt_config_dir = target_dir / "obt_config"
  obt_config_dir.mkdir(parents=True, exist_ok=True)

  src = path.Path(os.path.expanduser("~/.obt-global/env.common.sh"))
  dst = obt_config_dir / "env.common.sh"

  if src.exists():
    shutil.copy2(str(src), str(dst))
    print(deco.val(f"    Copied env.common.sh to obt_config/"))
  else:
    # Create a minimal placeholder
    with open(str(dst), 'w') as f:
      f.write("# env.common.sh — customize for target machine\n")
      f.write("# This file is sourced by obt-launch-env before launching OBT.\n")
    print(deco.val(f"    Created placeholder obt_config/env.common.sh"))

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
    deploy_script = proj_root / "obt.project" / "bin" / "ork.deploy.project.py"
    if deploy_script.exists():
      deploy_manifest = _run_deploy_script(deploy_script, proj_root)
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

    # Fix text references: old project root → new project root
    old_proj_str = str(proj_root)
    new_proj_str = str(proj_target)
    replacements = [(old_proj_str, new_proj_str)]
    count = fix_text_in_tree(proj_target, replacements, label_root=target_dir)
    if count:
      print(deco.val(f"    Fixed {count} text files"))

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

  print(deco.val(f"\n  Phase 5 complete."))
  return True

###############################################################################

def _run_deploy_script(script_path, proj_root):
  """Run a project's deploy script and parse its JSON manifest output."""
  try:
    result = subprocess.run(
      [sys.executable, str(script_path)],
      capture_output=True, text=True, timeout=30,
      env={**os.environ, "PROJECT_ROOT": str(proj_root)})
    if result.returncode == 0 and result.stdout.strip():
      return json.loads(result.stdout.strip())
  except Exception as e:
    print(deco.val(f"    WARNING: Deploy script failed: {e}"))
  return _default_deploy_manifest(proj_root)

def _default_deploy_manifest(proj_root):
  """Generate a default deploy manifest — just obt.project/."""
  return {"dirs": ["obt.project"], "optional_dirs": []}

###############################################################################
# Phase 6: Launch Script + Shebang Fixup
###############################################################################

_LAUNCH_SCRIPT_TEMPLATE = r'''#!/usr/bin/env bash
###############################################################################
# obt-launch-env — Relocatable OBT environment launcher
# Generated by ork.deploy.macos.relocatable.py (Phase 6)
#
# This script is self-locating: it computes all paths relative to its own
# location, so the deployment can be moved anywhere.
###############################################################################

# Compute DEPLOY_ROOT from this script's location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
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
export PYTHONNOUSERSITE=1

# Bootstrap the OBT framework venv.
# The obt_venv's pyvenv.cfg already tells Python 3.14 where its stdlib is.
export VIRTUAL_ENV="$DEPLOY_ROOT/obt_venv"

# Build PATH: deploy dirs first, then system essentials.
# No homebrew — the deployment is fully self-contained.
export PATH="$DEPLOY_ROOT/obt_venv/bin:$DEPLOY_ROOT/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

# ---- Bash-level relocation fixup (runs BEFORE Python) ----
# Python cannot start if pyvenv.cfg contains stale paths from the build
# machine, so we must fix these in bash first.
_MARKER_FILE="$DEPLOY_ROOT/.deploy_path"
_NEED_FIXUP=0
if [ -f "$_MARKER_FILE" ]; then
  _OLD_ROOT="$(cat "$_MARKER_FILE")"
  if [ "$_OLD_ROOT" != "$DEPLOY_ROOT" ]; then
    _NEED_FIXUP=1
  fi
else
  # First run — write marker, no fixup needed (paths match build)
  echo "$DEPLOY_ROOT" > "$_MARKER_FILE"
fi

if [ "$_NEED_FIXUP" -eq 1 ]; then
  echo "[deploy-fixup] Relocation detected: $_OLD_ROOT -> $DEPLOY_ROOT"
  # Fix pyvenv.cfg files (critical — Python won't start without this)
  for _cfg in "$DEPLOY_ROOT/obt_venv/pyvenv.cfg" "$DEPLOY_ROOT/pyvenv/pyvenv.cfg"; do
    if [ -f "$_cfg" ]; then
      sed -i '' "s|$_OLD_ROOT|$DEPLOY_ROOT|g" "$_cfg"
    fi
  done
  # Fix shebangs in venv bin dirs
  for _bindir in "$DEPLOY_ROOT/obt_venv/bin" "$DEPLOY_ROOT/pyvenv/bin"; do
    if [ -d "$_bindir" ]; then
      for _f in "$_bindir"/*; do
        [ -f "$_f" ] || continue
        [ -L "$_f" ] && continue
        head -c 2 "$_f" | grep -q '#!' || continue
        sed -i '' "s|$_OLD_ROOT|$DEPLOY_ROOT|g" "$_f" 2>/dev/null
      done
    fi
  done
  # Fix pkg-config and cmake files
  for _pcdir in "$DEPLOY_ROOT/lib/pkgconfig" "$DEPLOY_ROOT/lib64/pkgconfig"; do
    if [ -d "$_pcdir" ]; then
      for _f in "$_pcdir"/*.pc; do
        [ -f "$_f" ] && sed -i '' "s|$_OLD_ROOT|$DEPLOY_ROOT|g" "$_f"
      done
    fi
  done
  if [ -d "$DEPLOY_ROOT/lib/cmake" ]; then
    for _f in "$DEPLOY_ROOT"/lib/cmake/*.cmake; do
      [ -f "$_f" ] && sed -i '' "s|$_OLD_ROOT|$DEPLOY_ROOT|g" "$_f"
    done
  fi
  echo "$DEPLOY_ROOT" > "$_MARKER_FILE"
  echo "[deploy-fixup] Done."
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
  --stagedir "$DEPLOY_ROOT" \
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
  with open(str(launch_script), 'w') as f:
    f.write(_LAUNCH_SCRIPT_TEMPLATE.lstrip('\n'))
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

  # ---- Step 3: Create MoltenVK ICD manifest ----
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

  # ---- Step 5: Write deploy path marker ----
  print(deco.val(f"\n  Step 5: Writing deploy path marker..."))
  marker_path = target_dir / ".deploy_path"
  with open(str(marker_path), 'w') as f:
    f.write(str(target_dir) + '\n')
  print(deco.val(f"    Wrote: {marker_path}"))

  # ---- Step 6: Write deploy mode marker ----
  print(deco.val(f"\n  Step 6: Writing deploy mode marker..."))
  deploy_marker = target_dir / ".is_deploy"
  deploy_marker.touch()
  print(deco.val(f"    Wrote: {deploy_marker}"))

  print(deco.val(f"\n  Phase 6 complete."))
  return True

###############################################################################
# Phase 7: App Bundle Generation
###############################################################################

def phase7_app_bundles(infra_dir, visible_dir):
  """Generate macOS .app bundles as GUI entry points.

  Creates thin .app wrappers at the top level of visible_dir (the user-facing
  deploy directory). Infrastructure lives in infra_dir (.staging/).
  Always generates LaunchShell.app (OBT-level, terminal mode), then checks
  each deployed project's manifest for additional app bundles.

  Args:
    infra_dir:   The .staging/ directory containing all runtime infrastructure
    visible_dir: The user-visible deploy root (parent of .staging/)
  """
  from ork.deploy import generate_app_bundle, png_to_icns, set_folder_icon

  infra_dir = path.Path(infra_dir)
  visible_dir = path.Path(visible_dir)

  print(deco.val("=" * 60))
  print(deco.val("Phase 7: App Bundle Generation"))
  print(deco.val("=" * 60))

  # .app bundles go directly in the visible directory
  apps_dir = visible_dir

  # ---- Step 1: Convert OrkidLogo.png → orkid.icns ----
  print(deco.val(f"\n  Step 1: Converting icon..."))
  # Store icns inside .staging/ (hidden from user)
  icns_path = infra_dir / "orkid.icns"
  # Look for the icon in the deployed orkid project, fall back to source
  deployed_icon = infra_dir / "projects" / "orkid" / "ork.data" / "misc" / "OrkidLogo.png"
  source_icon = None
  if deployed_icon.exists():
    source_icon = deployed_icon
  else:
    # Search OBT_PROJECT_DIRS for the icon source
    proj_env = os.environ.get("OBT_PROJECT_DIRS", "")
    candidates = []
    for p in proj_env.split(":"):
      if p:
        candidates.append(path.Path(p) / "ork.data" / "misc" / "OrkidLogo.png")
    for candidate in candidates:
      if candidate.exists():
        source_icon = candidate
        break

  if source_icon and not icns_path.exists():
    try:
      png_to_icns(source_icon, icns_path)
      print(deco.val(f"    Converted: {source_icon.name} -> orkid.icns"))
    except Exception as e:
      print(deco.val(f"    WARNING: Icon conversion failed: {e}"))
      icns_path = None
  elif icns_path.exists():
    print(deco.val(f"    orkid.icns already present"))
  else:
    print(deco.val(f"    WARNING: No source icon found — bundles will have no icon"))
    icns_path = None

  icon_str = str(icns_path) if icns_path and icns_path.exists() else None

  # ---- Step 2: Generate LaunchShell.app (OBT-level, terminal) ----
  print(deco.val(f"\n  Step 2: Generating LaunchShell.app..."))
  launch_shell_spec = {
    "name": "LaunchShell",
    "command": [],  # empty = interactive shell
    "mode": "terminal",
    "bundle_id": "com.tweakoz.obt.launchshell",
    "icon": icon_str,
  }
  app_path = generate_app_bundle(infra_dir, apps_dir, launch_shell_spec)
  print(deco.val(f"    Created: {os.path.relpath(app_path, str(visible_dir))}"))

  # ---- Step 3: Read projects manifest and generate project apps ----
  print(deco.val(f"\n  Step 3: Generating project app bundles..."))
  manifest_path = infra_dir / "projects" / "manifest.json"
  if not manifest_path.exists():
    print(deco.val(f"    WARNING: No projects/manifest.json — skipping project apps"))
    _phase7_finish(visible_dir, icon_str, 1)
    return True

  with open(str(manifest_path)) as f:
    projects_manifest = json.load(f)

  app_count = 0
  for proj_entry in projects_manifest.get("projects", []):
    proj_name = proj_entry["name"]
    proj_dir = infra_dir / "projects" / proj_name

    # Check if project has a deploy script with "apps" declared
    deploy_script = proj_dir / "obt.project" / "bin" / "ork.deploy.project.py"
    if not deploy_script.exists():
      continue

    deploy_manifest = _run_deploy_script(deploy_script, proj_dir)
    app_specs = deploy_manifest.get("apps", [])
    if not app_specs:
      continue

    print(deco.val(f"    Project '{proj_name}' declares {len(app_specs)} app(s)"))
    for spec in app_specs:
      # Provide default icon if not specified
      if "icon" not in spec and icon_str:
        spec["icon"] = icon_str
      app_path = generate_app_bundle(infra_dir, apps_dir, spec)
      print(deco.val(f"      Created: {os.path.relpath(app_path, str(visible_dir))}"))
      app_count += 1

  _phase7_finish(visible_dir, icon_str, app_count + 1)
  return True

def _phase7_finish(visible_dir, icon_str, total_apps):
  """Finish phase 7: set folder icon and print summary."""
  from ork.deploy import set_folder_icon

  # ---- Step 4: Set custom folder icon on the visible deploy directory ----
  print(deco.val(f"\n  Step 4: Setting folder icon..."))
  if icon_str:
    try:
      set_folder_icon(str(visible_dir), icon_str)
      print(deco.val(f"    Set folder icon on: {visible_dir}"))
    except Exception as e:
      print(deco.val(f"    WARNING: Could not set folder icon: {e}"))
  else:
    print(deco.val(f"    Skipped — no icon available"))

  print(deco.val(f"\n  Generated {total_apps} app bundle(s) total"))
  print(deco.val(f"\n  Phase 7 complete."))

###############################################################################
# Main
###############################################################################

def main():
  parser = argparse.ArgumentParser(
    description="Create a relocatable macOS deployment from OBT staging")

  parser.add_argument("--target", required=True,
    help="Target directory for the relocatable deployment")
  parser.add_argument("--staging", default=None,
    help="Source staging directory (default: $OBT_STAGE)")
  parser.add_argument("--phase", choices=["1", "2", "3", "4", "5", "6", "7", "all"], default="all",
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

  # Special modes
  if args.walk_only:
    walker = macos.MachoDependencyWalker(staging_dir, args.homebrew)
    walker.walk()
    walker.dump_manifest()
    return

  # Infrastructure lives inside .staging/ — the user-visible top level
  # only contains .app bundles (and the hidden .staging directory).
  infra_dir = target_dir / ".staging"

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
    ok = phase4_obt_venv(infra_dir, obt_venv_dir, args.homebrew)
    if not ok:
      sys.exit(1)

  if phase in ("5", "all"):
    if project_dirs is None:
      print("ERROR: No --project provided and $OBT_PROJECT_DIRS not set.")
      sys.exit(1)
    ok = phase5_projects(infra_dir, project_dirs)
    if not ok:
      sys.exit(1)

  if phase in ("6", "all"):
    ok = phase6_launch_script(infra_dir)
    if not ok:
      sys.exit(1)

  if phase in ("7", "all"):
    ok = phase7_app_bundles(infra_dir, target_dir)
    if not ok:
      sys.exit(1)

  if phase == "all":
    print(deco.val("\n" + "=" * 60))
    print(deco.val("Deployment complete!"))
    print(deco.val("=" * 60))
    print(deco.val(f"  Location: {target_dir}"))
    print(deco.val(f"  Infrastructure: {infra_dir}"))
    print(deco.val(f"  App bundles visible at top level of {target_dir}"))

    # Create a .tgz archive with OrkidDeploy/ as the top-level directory
    targz_path = target_dir.parent / f"{target_dir.name}.tgz"
    print(deco.val("\n" + "=" * 60))
    print(deco.val("Creating compressed tar.gz archive"))
    print(deco.val("=" * 60))
    if targz_path.exists():
      print(deco.val(f"  Removing existing {targz_path.name}..."))
      os.remove(str(targz_path))
    print(deco.val(f"  Source:  {target_dir}"))
    print(deco.val(f"  Output:  {targz_path}"))
    subprocess.run([
      "tar", "czf", str(targz_path),
      "-C", str(target_dir.parent),
      target_dir.name,
    ], check=True)
    targz_size_mb = targz_path.stat().st_size / (1024 * 1024)
    print(deco.val(f"  Archive created: {targz_path} ({targz_size_mb:.1f} MB)"))

if __name__ == "__main__":
  main()
