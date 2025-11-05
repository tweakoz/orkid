################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS curl Provider
# Copies curl headers from host staging to iOS subspace
################################################################

import datetime
import shutil
from pathlib import Path

######################################################################
# curl iOS Provider
######################################################################

def install_curl_for_ios(
    host_include_dir,
    ios_include_dir,
    manifest_dir=None,
    force_rebuild=False
):
    """Install curl headers for iOS by copying from host staging

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

    print("\n=== Installing curl for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "curl"

        # Check if already installed (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ curl for iOS already installed (manifest: curl)")
            print(f"  Location: {ios_include_dir}/curl")
            return True

    # Check if host curl exists
    host_curl_dir = host_include_dir / "curl"
    if not host_curl_dir.exists():
        print(f"ERROR: curl not found in host staging at {host_curl_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Create iOS include directory
    ios_include_dir.mkdir(parents=True, exist_ok=True)

    # Copy curl headers to iOS subspace
    ios_curl_dir = ios_include_dir / "curl"

    print(f"Copying curl headers from {host_curl_dir} to {ios_curl_dir}...")

    # Remove existing if present
    if ios_curl_dir.exists():
        shutil.rmtree(ios_curl_dir)

    # Copy directory tree
    shutil.copytree(host_curl_dir, ios_curl_dir)

    # Create manifest file to mark successful install
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# curl iOS install completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {host_curl_dir}\n")
            f.write(f"# Destination: {ios_curl_dir}\n")

        print(f"\n✓ curl for iOS installed successfully")
        print(f"  Location: {ios_curl_dir}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ curl for iOS installed successfully")
        print(f"  Location: {ios_curl_dir}")

    return True
