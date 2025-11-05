################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS OpenBLAS Provider
# Copies OpenBLAS headers from host staging to iOS subspace
################################################################

import datetime
import shutil
from pathlib import Path

######################################################################
# OpenBLAS iOS Provider
######################################################################

def install_openblas_for_ios(
    host_include_dir,
    ios_include_dir,
    manifest_dir=None,
    force_rebuild=False
):
    """Install OpenBLAS headers for iOS by copying from host staging

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

    print("\n=== Installing OpenBLAS for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "openblas"

        # Check if already installed (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ OpenBLAS for iOS already installed (manifest: openblas)")
            print(f"  Location: {ios_include_dir}/openblas")
            return True

    # Check if host OpenBLAS exists
    host_openblas_dir = host_include_dir / "openblas"
    if not host_openblas_dir.exists():
        print(f"ERROR: OpenBLAS not found in host staging at {host_openblas_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Create iOS include directory
    ios_include_dir.mkdir(parents=True, exist_ok=True)

    # Copy OpenBLAS headers to iOS subspace
    ios_openblas_dir = ios_include_dir / "openblas"

    print(f"Copying OpenBLAS headers from {host_openblas_dir} to {ios_openblas_dir}...")

    # Remove existing if present
    if ios_openblas_dir.exists():
        shutil.rmtree(ios_openblas_dir)

    # Copy directory tree
    shutil.copytree(host_openblas_dir, ios_openblas_dir)

    # Create manifest file to mark successful install
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# OpenBLAS iOS install completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {host_openblas_dir}\n")
            f.write(f"# Destination: {ios_openblas_dir}\n")

        print(f"\n✓ OpenBLAS for iOS installed successfully")
        print(f"  Location: {ios_openblas_dir}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ OpenBLAS for iOS installed successfully")
        print(f"  Location: {ios_openblas_dir}")

    return True
