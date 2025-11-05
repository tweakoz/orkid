################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS GLM Provider
# Copies GLM headers from host staging to iOS subspace
################################################################

import datetime
import shutil
from pathlib import Path

######################################################################
# GLM iOS Provider
######################################################################

def install_glm_for_ios(
    host_include_dir,
    ios_include_dir,
    manifest_dir=None,
    force_rebuild=False
):
    """Install GLM headers for iOS by copying from host staging

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

    print("\n=== Installing GLM for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "glm"

        # Check if already installed (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ GLM for iOS already installed (manifest: glm)")
            print(f"  Location: {ios_include_dir}/glm")
            return True

    # Check if host GLM exists
    host_glm_dir = host_include_dir / "glm"
    if not host_glm_dir.exists():
        print(f"ERROR: GLM not found in host staging at {host_glm_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Create iOS include directory
    ios_include_dir.mkdir(parents=True, exist_ok=True)

    # Copy GLM headers to iOS subspace
    ios_glm_dir = ios_include_dir / "glm"

    print(f"Copying GLM headers from {host_glm_dir} to {ios_glm_dir}...")

    # Remove existing if present
    if ios_glm_dir.exists():
        shutil.rmtree(ios_glm_dir)

    # Copy directory tree
    shutil.copytree(host_glm_dir, ios_glm_dir)

    # Create manifest file to mark successful install
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# GLM iOS install completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {host_glm_dir}\n")
            f.write(f"# Destination: {ios_glm_dir}\n")

        print(f"\n✓ GLM for iOS installed successfully")
        print(f"  Location: {ios_glm_dir}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ GLM for iOS installed successfully")
        print(f"  Location: {ios_glm_dir}")

    return True
