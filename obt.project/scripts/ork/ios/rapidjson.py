################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS rapidjson Provider
# Copies rapidjson headers from host staging to iOS subspace
################################################################

import datetime
import shutil
from pathlib import Path

######################################################################
# rapidjson iOS Provider
######################################################################

def install_rapidjson_for_ios(
    host_include_dir,
    ios_include_dir,
    manifest_dir=None,
    force_rebuild=False
):
    """Install rapidjson headers for iOS by copying from host staging

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

    print("\n=== Installing rapidjson for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "rapidjson"

        # Check if already installed (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ rapidjson for iOS already installed (manifest: rapidjson)")
            print(f"  Location: {ios_include_dir}/rapidjson")
            return True

    # Check if host rapidjson exists
    host_rapidjson_dir = host_include_dir / "rapidjson"
    if not host_rapidjson_dir.exists():
        print(f"ERROR: rapidjson not found in host staging at {host_rapidjson_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Create iOS include directory
    ios_include_dir.mkdir(parents=True, exist_ok=True)

    # Copy rapidjson headers to iOS subspace
    ios_rapidjson_dir = ios_include_dir / "rapidjson"

    print(f"Copying rapidjson headers from {host_rapidjson_dir} to {ios_rapidjson_dir}...")

    # Remove existing if present
    if ios_rapidjson_dir.exists():
        shutil.rmtree(ios_rapidjson_dir)

    # Copy directory tree
    shutil.copytree(host_rapidjson_dir, ios_rapidjson_dir)

    # Create manifest file to mark successful install
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# rapidjson iOS install completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {host_rapidjson_dir}\n")
            f.write(f"# Destination: {ios_rapidjson_dir}\n")

        print(f"\n✓ rapidjson for iOS installed successfully")
        print(f"  Location: {ios_rapidjson_dir}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ rapidjson for iOS installed successfully")
        print(f"  Location: {ios_rapidjson_dir}")

    return True
