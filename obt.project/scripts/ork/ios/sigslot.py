################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS sigslot Provider
# Copies sigslot headers from host staging to iOS subspace
################################################################

import datetime
import shutil
from pathlib import Path

######################################################################
# sigslot iOS Provider
######################################################################

def install_sigslot_for_ios(
    host_include_dir,
    ios_include_dir,
    manifest_dir=None,
    force_rebuild=False
):
    """Install sigslot headers for iOS by copying from host staging

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

    print("\n=== Installing sigslot for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "sigslot"

        # Check if already installed (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ sigslot for iOS already installed (manifest: sigslot)")
            print(f"  Location: {ios_include_dir}/sigslot")
            return True

    # Check if host sigslot exists
    host_sigslot_dir = host_include_dir / "sigslot"
    if not host_sigslot_dir.exists():
        print(f"ERROR: sigslot not found in host staging at {host_sigslot_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Create iOS include directory
    ios_include_dir.mkdir(parents=True, exist_ok=True)

    # Copy sigslot headers to iOS subspace
    ios_sigslot_dir = ios_include_dir / "sigslot"

    print(f"Copying sigslot headers from {host_sigslot_dir} to {ios_sigslot_dir}...")

    # Remove existing if present
    if ios_sigslot_dir.exists():
        shutil.rmtree(ios_sigslot_dir)

    # Copy directory tree
    shutil.copytree(host_sigslot_dir, ios_sigslot_dir)

    # Create manifest file to mark successful install
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# sigslot iOS install completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {host_sigslot_dir}\n")
            f.write(f"# Destination: {ios_sigslot_dir}\n")

        print(f"\n✓ sigslot for iOS installed successfully")
        print(f"  Location: {ios_sigslot_dir}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ sigslot for iOS installed successfully")
        print(f"  Location: {ios_sigslot_dir}")

    return True
