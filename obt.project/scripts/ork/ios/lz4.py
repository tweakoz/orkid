################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS LZ4 Provider
# Builds LZ4 from source for iOS
################################################################

import datetime
import os
import obt.path
from pathlib import Path
from ork.ios import cmake_builder

######################################################################
# LZ4 iOS Provider
######################################################################

def build_lz4_for_ios(
    ios_subspace,
    ios_builds_dir,
    ios_include_dir,
    ios_lib_dir,
    is_simulator,
    manifest_dir=None,
    force_rebuild=False,
    verbose=False,
    num_cores=8
):
    """Build LZ4 for iOS from source

    Args:
        ios_subspace: iOS subspace root directory
        ios_builds_dir: iOS builds directory
        ios_include_dir: iOS include directory
        ios_lib_dir: iOS library directory
        is_simulator: True for simulator, False for device
        manifest_dir: Directory for storing build manifests (optional)
        force_rebuild: Force rebuild even if manifest exists
        verbose: Enable verbose build output
        num_cores: Number of parallel build jobs

    Returns:
        True if successful, False otherwise
    """
    ios_subspace = Path(ios_subspace)
    ios_builds_dir = Path(ios_builds_dir)
    ios_include_dir = Path(ios_include_dir)
    ios_lib_dir = Path(ios_lib_dir)

    print("\n=== Building LZ4 for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "lz4"

        # Check if already built (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ LZ4 for iOS already built (manifest: lz4)")
            print(f"  Location: {ios_lib_dir}/liblz4.a")
            return True

    # LZ4 source location
    stage_dir = obt.path.stage()
    lz4_src_dir = stage_dir / "builds" / "lz4" / "lib"

    if not lz4_src_dir.exists():
        print(f"ERROR: LZ4 source not found at {lz4_src_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # LZ4 build configuration
    lz4_config = {
        "library_name": "lz4",
        "source_dir": lz4_src_dir,
        "source_patterns": ["lz4.c", "lz4hc.c", "lz4frame.c"],
        "include_dirs": [str(lz4_src_dir)],
        "defines": [],
        "compile_options": [
            "-fPIC",
            "-fvisibility=hidden"
        ],
        "public_headers": [
            "lz4.h",
            "lz4hc.h",
            "lz4frame.h",
            "lz4frame_static.h"
        ],
        "library_type": "STATIC",
        "frameworks": []
    }

    # Generate CMakeLists.txt
    cmake_content = cmake_builder.generate_cmake_for_ios_library(**lz4_config)

    # Build directory
    build_dir = ios_builds_dir / "lz4"

    # Toolchain file
    prj_root = Path(os.environ["ORKID_WORKSPACE_DIR"])
    toolchain_file = prj_root / "cmake" / "toolchains" / "ios.toolchain.cmake"

    # Build the library
    success = cmake_builder.build_ios_library(
        library_name="lz4",
        source_dir=lz4_src_dir,
        build_dir=build_dir,
        install_prefix=ios_subspace,
        is_simulator=is_simulator,
        toolchain_file=toolchain_file,
        cmake_content=cmake_content,
        num_cores=num_cores,
        verbose=verbose,
        clean=force_rebuild
    )

    if not success:
        return False

    # Create manifest file to mark successful build
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# LZ4 iOS build completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {lz4_src_dir}\n")
            f.write(f"# Build: {build_dir}\n")
            f.write(f"# Install: {ios_subspace}\n")

        print(f"\n✓ LZ4 for iOS built successfully")
        print(f"  Library: {ios_lib_dir}/liblz4.a")
        print(f"  Headers: {ios_include_dir}/lz4*.h")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ LZ4 for iOS built successfully")
        print(f"  Library: {ios_lib_dir}/liblz4.a")
        print(f"  Headers: {ios_include_dir}/lz4*.h")

    return True
