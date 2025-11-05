################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS ZeroMQ Provider
# Builds libzmq from source for iOS using its existing CMakeLists.txt
################################################################

import datetime
import os
import subprocess
import obt.path
from pathlib import Path

######################################################################
# ZeroMQ iOS Provider
######################################################################

def build_zmq_for_ios(
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
    """Build ZeroMQ for iOS from source using existing CMakeLists.txt

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

    print("\n=== Building ZeroMQ for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "zmq"

        # Check if already built (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ ZeroMQ for iOS already built (manifest: zmq)")
            print(f"  Location: {ios_lib_dir}/libzmq.a")
            return True

    # ZeroMQ source location
    stage_dir = obt.path.stage()
    zmq_src_dir = stage_dir / "builds" / "zmq"

    if not zmq_src_dir.exists():
        print(f"ERROR: ZeroMQ source not found at {zmq_src_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Build directory
    build_dir = ios_builds_dir / "zmq"
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
    print(f"\nConfiguring ZeroMQ for iOS...")

    cmake_cmd = [
        "cmake",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
        f"-DCMAKE_INSTALL_PREFIX={ios_subspace}",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DIOS_SIMULATOR={'ON' if is_simulator else 'OFF'}",
        "-DARCHITECTURE=AARCH64",
        # Static library only
        "-DBUILD_SHARED=OFF",
        "-DBUILD_STATIC=ON",
        # Disable tests, docs, and perf tools
        "-DBUILD_TESTS=OFF",
        "-DWITH_DOCS=OFF",
        "-DWITH_PERF_TOOL=OFF",
        # Disable drafts API (reduces build size)
        "-DENABLE_DRAFTS=OFF",
        # Disable CURVE security for now (requires libsodium)
        "-DENABLE_CURVE=OFF",
        "-DWITH_LIBSODIUM=OFF",
        # Disable TLS (requires GnuTLS)
        "-DWITH_TLS=OFF",
        # Disable NSS (use builtin sha1)
        "-DWITH_NSS=OFF",
        # Disable libbsd
        "-DWITH_LIBBSD=OFF",
        # Position-independent code
        "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
        str(zmq_src_dir)
    ]

    result = subprocess.run(cmake_cmd, cwd=build_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"✗ CMake configuration failed for ZeroMQ (return code: {result.returncode})")
        if result.stdout:
            print("STDOUT:", result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return False
    else:
        # Always print output to see what's configured
        if result.stdout:
            print(result.stdout)

    # Build
    print(f"\nBuilding ZeroMQ...")

    build_cmd = ["cmake", "--build", str(build_dir), "--parallel", str(num_cores)]
    if verbose:
        build_cmd.append("--verbose")

    result = subprocess.run(build_cmd, capture_output=not verbose)
    if result.returncode != 0:
        print(f"✗ Build failed for ZeroMQ")
        return False

    # Install
    print(f"\nInstalling ZeroMQ...")

    install_cmd = ["cmake", "--install", str(build_dir)]
    result = subprocess.run(install_cmd, capture_output=not verbose)
    if result.returncode != 0:
        print(f"✗ Install failed for ZeroMQ")
        return False

    # Create manifest file to mark successful build
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# ZeroMQ iOS build completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {zmq_src_dir}\n")
            f.write(f"# Build: {build_dir}\n")
            f.write(f"# Install: {ios_subspace}\n")

        print(f"\n✓ ZeroMQ for iOS built successfully")
        print(f"  Library: {ios_lib_dir}/libzmq.a")
        print(f"  Headers: {ios_include_dir}/zmq.h")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ ZeroMQ for iOS built successfully")
        print(f"  Library: {ios_lib_dir}/libzmq.a")
        print(f"  Headers: {ios_include_dir}/zmq.h")

    return True
