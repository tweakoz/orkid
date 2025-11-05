################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS easyprof Provider
# Copies easyprof headers from host staging to iOS subspace
################################################################

import datetime
import shutil
from pathlib import Path

######################################################################
# easyprof iOS Provider
######################################################################

def install_easyprof_for_ios(
    host_include_dir,
    ios_include_dir,
    manifest_dir=None,
    force_rebuild=False
):
    """Install easyprof headers for iOS by copying from host staging

    Args:
        host_include_dir: Path to host staging include directory
        ios_include_dir: Path to iOS subspace include directory
        manifest_dir: Directory for storing build manifests (optional)
        force_rebuild: Force reinstall even if manifest exists

    Returns:
        True if successful, False otherwise
    """
    host_include_dir = Path(host_include_dir)
    ios_include_dir = Path(ios_include_dir)

    print("\n=== Installing easyprof for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "easyprof"

        # Check if already installed (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ easyprof for iOS already installed (manifest: easyprof)")
            print(f"  Location: {ios_include_dir}/easy")
            return True

    # Check if host easyprof exists
    host_easyprof_dir = host_include_dir / "easy"
    if not host_easyprof_dir.exists():
        print(f"ERROR: easyprof not found in host staging at {host_easyprof_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Create iOS include directory
    ios_include_dir.mkdir(parents=True, exist_ok=True)

    # Copy easyprof headers to iOS subspace
    ios_easyprof_dir = ios_include_dir / "easy"

    print(f"Copying easyprof headers from {host_easyprof_dir} to {ios_easyprof_dir}...")

    # Remove existing if present
    if ios_easyprof_dir.exists():
        shutil.rmtree(ios_easyprof_dir)

    # Copy directory tree
    shutil.copytree(host_easyprof_dir, ios_easyprof_dir)

    # Create manifest file to mark successful install
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# easyprof iOS install completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {host_easyprof_dir}\n")
            f.write(f"# Destination: {ios_easyprof_dir}\n")

        print(f"\n✓ easyprof for iOS installed successfully")
        print(f"  Location: {ios_easyprof_dir}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ easyprof for iOS installed successfully")
        print(f"  Location: {ios_easyprof_dir}")

    return True
