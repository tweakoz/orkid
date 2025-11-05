################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS LZ4 Provider
# Copies LZ4 headers from host staging to iOS subspace
################################################################

import datetime
import shutil
from pathlib import Path

######################################################################
# LZ4 iOS Provider
######################################################################

def install_lz4_for_ios(
    host_include_dir,
    ios_include_dir,
    manifest_dir=None,
    force_rebuild=False
):
    """Install LZ4 headers for iOS by copying from host staging

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

    print("\n=== Installing LZ4 for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "lz4"

        # Check if already installed (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ LZ4 for iOS already installed (manifest: lz4)")
            print(f"  Location: {ios_include_dir}/lz4.h")
            return True

    # Check if host LZ4 exists
    host_lz4_header = host_include_dir / "lz4.h"
    if not host_lz4_header.exists():
        print(f"ERROR: LZ4 not found in host staging at {host_lz4_header}")
        print("Please ensure host Orkid has been built first")
        return False

    # Create iOS include directory
    ios_include_dir.mkdir(parents=True, exist_ok=True)

    # Copy all LZ4 headers to iOS subspace
    import glob
    lz4_headers = glob.glob(str(host_include_dir / "lz4*.h"))

    if not lz4_headers:
        print(f"ERROR: No LZ4 headers found in {host_include_dir}")
        return False

    print(f"Copying {len(lz4_headers)} LZ4 headers to {ios_include_dir}...")

    for header in lz4_headers:
        header_path = Path(header)
        dest_path = ios_include_dir / header_path.name
        print(f"  {header_path.name}")
        shutil.copy2(header_path, dest_path)

    # Create manifest file to mark successful install
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# LZ4 iOS install completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {host_include_dir}/lz4*.h\n")
            f.write(f"# Destination: {ios_include_dir}/lz4*.h\n")

        print(f"\n✓ LZ4 for iOS installed successfully")
        print(f"  Location: {ios_include_dir}/lz4*.h")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ LZ4 for iOS installed successfully")
        print(f"  Location: {ios_include_dir}/lz4*.h")

    return True
