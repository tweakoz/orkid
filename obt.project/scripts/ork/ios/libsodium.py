################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS libsodium Provider
# Clones libsodium from git and builds static library
################################################################

import datetime
import os
import obt.path
import obt.git
from pathlib import Path

######################################################################
# libsodium iOS Provider
######################################################################

def build_libsodium_for_ios(
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
    """Build libsodium for iOS from git source

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
    import shutil
    ios_subspace = Path(ios_subspace)
    ios_builds_dir = Path(ios_builds_dir)
    ios_include_dir = Path(ios_include_dir)
    ios_lib_dir = Path(ios_lib_dir)

    print("\n=== Building libsodium for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "libsodium"

        # Check if already built (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ libsodium for iOS already built (manifest: libsodium)")
            print(f"  Location: {ios_lib_dir}/libsodium.a")
            return True

    # Clone libsodium from git
    libsodium_src_dir = ios_subspace / "builds" / "libsodium"

    if not libsodium_src_dir.exists() or force_rebuild:
        # Remove existing directory if force rebuild
        if force_rebuild and libsodium_src_dir.exists():
            print(f"Removing existing source directory: {libsodium_src_dir}")
            shutil.rmtree(libsodium_src_dir)

        print(f"\nCloning libsodium from git...")
        success = obt.git.Clone(
            url="https://github.com/tweakoz/libsodium.git",
            dest=str(libsodium_src_dir),
            rev="master",  # Clone master branch
            cache=False,
            shallow=False
        )
        if not success:
            print(f"✗ Failed to clone libsodium")
            return False

        # Checkout the specific tag
        print(f"Checking out tag 1.0.20-RELEASE...")
        result = subprocess.run(
            ["git", "checkout", "1.0.20-RELEASE"],
            cwd=str(libsodium_src_dir),
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"✗ Failed to checkout tag 1.0.20-RELEASE")
            print(f"STDERR: {result.stderr}")
            return False

        print(f"✓ Cloned libsodium and checked out tag 1.0.20-RELEASE")
    else:
        print(f"libsodium source already exists at {libsodium_src_dir}")

    # Run autogen.sh to generate configure script
    print(f"\nRunning autogen.sh...")
    result = subprocess.run(
        ["./autogen.sh"],
        cwd=str(libsodium_src_dir),
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"✗ autogen.sh failed")
        if result.stdout:
            print("STDOUT:", result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return False

    # Clean if requested
    if force_rebuild:
        print(f"Cleaning previous build...")
        subprocess.run(["make", "clean"], cwd=str(libsodium_src_dir), capture_output=True)
        subprocess.run(["make", "distclean"], cwd=str(libsodium_src_dir), capture_output=True)

    # Determine iOS SDK and architecture based on is_simulator
    if is_simulator:
        sdk_name = "iphonesimulator"
        arch = "arm64"  # M1/M2 simulator
        min_version = "15.0"
        target_triple = "arm64-apple-ios15.0-simulator"
    else:
        sdk_name = "iphoneos"
        arch = "arm64"
        min_version = "15.0"
        target_triple = "arm64-apple-ios15.0"

    # Get SDK path using xcrun
    result = subprocess.run(
        ["xcrun", "--sdk", sdk_name, "--show-sdk-path"],
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"✗ Failed to get SDK path for {sdk_name}")
        return False
    sdk_path = result.stdout.strip()

    print(f"\nConfiguring libsodium for iOS {sdk_name} ({arch})...")
    print(f"  SDK: {sdk_path}")
    print(f"  Target: {target_triple}")
    print(f"  Min version: {min_version}")

    # Setup environment for cross-compilation
    env = os.environ.copy()
    env["CC"] = "xcrun -sdk " + sdk_name + " clang"
    env["CFLAGS"] = f"-target {target_triple} -arch {arch} -isysroot {sdk_path} -mios-version-min={min_version}"
    env["LDFLAGS"] = f"-target {target_triple} -arch {arch} -isysroot {sdk_path}"
    env["PREFIX"] = str(ios_subspace)

    # Configure
    configure_cmd = [
        "./configure",
        f"--prefix={ios_subspace}",
        "--host=arm-apple-darwin",
        "--enable-static",
        "--disable-shared",
        "--disable-pie"
    ]

    result = subprocess.run(
        configure_cmd,
        cwd=str(libsodium_src_dir),
        env=env,
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"✗ Configure failed for libsodium")
        if result.stdout:
            print("STDOUT:", result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return False
    else:
        if verbose and result.stdout:
            print(result.stdout)

    # Build
    print(f"\nBuilding libsodium...")

    build_cmd = ["make", f"-j{num_cores}"]
    if verbose:
        build_cmd.append("V=1")

    result = subprocess.run(
        build_cmd,
        cwd=str(libsodium_src_dir),
        env=env,
        capture_output=not verbose
    )
    if result.returncode != 0:
        print(f"✗ Build failed for libsodium")
        return False

    # Install
    print(f"\nInstalling libsodium...")

    install_cmd = ["make", "install"]
    result = subprocess.run(
        install_cmd,
        cwd=str(libsodium_src_dir),
        env=env,
        capture_output=not verbose
    )
    if result.returncode != 0:
        print(f"✗ Install failed for libsodium")
        return False

    # Verify installation
    lib_file = ios_lib_dir / "libsodium.a"
    header_file = ios_include_dir / "sodium.h"

    if not lib_file.exists():
        print(f"✗ Library not found at {lib_file}")
        return False
    if not header_file.exists():
        print(f"✗ Header not found at {header_file}")
        return False

    # Create manifest file to mark successful build
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# libsodium iOS build completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {libsodium_src_dir}\n")
            f.write(f"# SDK: {sdk_name}\n")
            f.write(f"# Arch: {arch}\n")
            f.write(f"# Install: {ios_subspace}\n")

        print(f"\n✓ libsodium for iOS built successfully")
        print(f"  Library: {lib_file}")
        print(f"  Headers: {header_file}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ libsodium for iOS built successfully")
        print(f"  Library: {lib_file}")
        print(f"  Headers: {header_file}")

    return True
