################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS Klein Provider
# Copies Klein headers from host staging to iOS subspace
################################################################

import datetime
import shutil
from pathlib import Path

######################################################################
# Klein iOS Provider
######################################################################

def install_klein_for_ios(
    host_include_dir,
    ios_include_dir,
    manifest_dir=None,
    force_rebuild=False
):
    """Install Klein headers for iOS by copying from host staging

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

    print("\n=== Installing Klein for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "klein"

        # Check if already installed (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ Klein for iOS already installed (manifest: klein)")
            print(f"  Location: {ios_include_dir}/klein")
            return True

    # Check if host Klein exists
    host_klein_dir = host_include_dir / "klein"
    if not host_klein_dir.exists():
        print(f"ERROR: Klein not found in host staging at {host_klein_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Create iOS include directory
    ios_include_dir.mkdir(parents=True, exist_ok=True)

    # Copy Klein headers to iOS subspace
    ios_klein_dir = ios_include_dir / "klein"

    print(f"Copying Klein headers from {host_klein_dir} to {ios_klein_dir}...")

    # Remove existing if present
    if ios_klein_dir.exists():
        shutil.rmtree(ios_klein_dir)

    # Copy directory tree
    shutil.copytree(host_klein_dir, ios_klein_dir)

    # Create manifest file to mark successful install
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# Klein iOS install completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {host_klein_dir}\n")
            f.write(f"# Destination: {ios_klein_dir}\n")

        print(f"\n✓ Klein for iOS installed successfully")
        print(f"  Location: {ios_klein_dir}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ Klein for iOS installed successfully")
        print(f"  Location: {ios_klein_dir}")

    return True
