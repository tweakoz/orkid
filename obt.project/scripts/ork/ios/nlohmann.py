################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS nlohmann json Provider
# Copies nlohmann json headers from host staging to iOS subspace
################################################################

import datetime
import shutil
from pathlib import Path

######################################################################
# nlohmann iOS Provider
######################################################################

def install_nlohmann_for_ios(
    host_include_dir,
    ios_include_dir,
    manifest_dir=None,
    force_rebuild=False
):
    """Install nlohmann json headers for iOS by copying from host staging

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

    print("\n=== Installing nlohmann json for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "nlohmann"

        # Check if already installed (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ nlohmann json for iOS already installed (manifest: nlohmann)")
            print(f"  Location: {ios_include_dir}/nlohmann")
            return True

    # Check if host nlohmann exists
    host_nlohmann_dir = host_include_dir / "nlohmann"
    if not host_nlohmann_dir.exists():
        print(f"ERROR: nlohmann not found in host staging at {host_nlohmann_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Create iOS include directory
    ios_include_dir.mkdir(parents=True, exist_ok=True)

    # Copy nlohmann headers to iOS subspace
    ios_nlohmann_dir = ios_include_dir / "nlohmann"

    print(f"Copying nlohmann json headers from {host_nlohmann_dir} to {ios_nlohmann_dir}...")

    # Remove existing if present
    if ios_nlohmann_dir.exists():
        shutil.rmtree(ios_nlohmann_dir)

    # Copy directory tree
    shutil.copytree(host_nlohmann_dir, ios_nlohmann_dir)

    # Create manifest file to mark successful install
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# nlohmann json iOS install completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {host_nlohmann_dir}\n")
            f.write(f"# Destination: {ios_nlohmann_dir}\n")

        print(f"\n✓ nlohmann json for iOS installed successfully")
        print(f"  Location: {ios_nlohmann_dir}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ nlohmann json for iOS installed successfully")
        print(f"  Location: {ios_nlohmann_dir}")

    return True
