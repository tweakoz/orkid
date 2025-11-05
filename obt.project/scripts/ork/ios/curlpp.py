################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS curlpp Provider
# Builds curlpp from source for iOS
################################################################

import datetime
import os
import obt.path
from pathlib import Path

######################################################################
# curlpp iOS Provider
######################################################################

def build_curlpp_for_ios(
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
    """Build curlpp for iOS from source

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
    import subprocess
    ios_subspace = Path(ios_subspace)
    ios_builds_dir = Path(ios_builds_dir)
    ios_include_dir = Path(ios_include_dir)
    ios_lib_dir = Path(ios_lib_dir)

    print("\n=== Building curlpp for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "curlpp"

        # Check if already built (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ curlpp for iOS already built (manifest: curlpp)")
            print(f"  Location: {ios_lib_dir}/libcurlpp.a")
            return True

    # curlpp source location (from host build)
    stage_dir = obt.path.stage()
    curlpp_src_dir = stage_dir / "builds" / "curlpp"

    if not curlpp_src_dir.exists():
        print(f"ERROR: curlpp source not found at {curlpp_src_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Build directory
    build_dir = ios_builds_dir / "curlpp_build"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Clean if requested
    if force_rebuild and build_dir.exists():
        import shutil
        print(f"Cleaning build directory: {build_dir}")
        shutil.rmtree(build_dir)
        build_dir.mkdir(parents=True, exist_ok=True)

    # Toolchain file
    prj_root = Path(os.environ["ORKID_WORKSPACE_DIR"])
    toolchain_file = prj_root / "cmake" / "toolchains" / "ios.toolchain.cmake"

    # Configure with CMake
    print(f"\nConfiguring curlpp for iOS...")

    cmake_cmd = [
        "cmake",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
        f"-DCMAKE_INSTALL_PREFIX={ios_subspace}",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DIOS_SIMULATOR={'ON' if is_simulator else 'OFF'}",
        "-DARCHITECTURE=AARCH64",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        # curlpp-specific options
        "-DCURLPP_BUILD_SHARED_LIBS=OFF",
        f"-DCURL_INCLUDE_DIR={ios_include_dir}",
        f"-DCURL_LIBRARY={ios_lib_dir}/libcurl.a",
        str(curlpp_src_dir)
    ]

    result = subprocess.run(cmake_cmd, cwd=build_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"✗ CMake configuration failed for curlpp (return code: {result.returncode})")
        if result.stdout:
            print("STDOUT:", result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return False
    else:
        if verbose and result.stdout:
            print(result.stdout)

    # Build
    print(f"\nBuilding curlpp...")

    build_cmd = ["cmake", "--build", str(build_dir), "--parallel", str(num_cores)]
    if verbose:
        build_cmd.append("--verbose")

    result = subprocess.run(build_cmd, capture_output=not verbose)
    if result.returncode != 0:
        print(f"✗ Build failed for curlpp")
        return False

    # Install
    print(f"\nInstalling curlpp...")

    install_cmd = ["cmake", "--install", str(build_dir)]
    result = subprocess.run(install_cmd, capture_output=not verbose)
    if result.returncode != 0:
        print(f"✗ Install failed for curlpp")
        return False

    # Verify installation
    lib_file = ios_lib_dir / "libcurlpp.a"
    if not lib_file.exists():
        print(f"✗ Library not found at {lib_file}")
        return False

    # Create manifest file to mark successful build
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# curlpp iOS build completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {curlpp_src_dir}\n")
            f.write(f"# Build: {build_dir}\n")
            f.write(f"# Install: {ios_subspace}\n")

        print(f"\n✓ curlpp for iOS built successfully")
        print(f"  Library: {lib_file}")
        print(f"  Headers: {ios_include_dir}/curlpp/")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ curlpp for iOS built successfully")
        print(f"  Library: {lib_file}")
        print(f"  Headers: {ios_include_dir}/curlpp/")

    return True
