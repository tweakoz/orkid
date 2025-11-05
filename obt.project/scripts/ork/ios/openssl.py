################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS OpenSSL Provider
# Clones OpenSSL from git and builds static library
################################################################

import datetime
import os
import obt.path
import obt.git
from pathlib import Path

######################################################################
# OpenSSL iOS Provider
######################################################################

def build_openssl_for_ios(
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
    """Build OpenSSL for iOS from git source

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

    print("\n=== Building OpenSSL for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "openssl"

        # Check if already built (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ OpenSSL for iOS already built (manifest: openssl)")
            print(f"  Location: {ios_lib_dir}/libssl.a, {ios_lib_dir}/libcrypto.a")
            return True

    # Clone OpenSSL from git
    openssl_src_dir = ios_builds_dir / "openssl"

    if not openssl_src_dir.exists() or force_rebuild:
        # Remove existing directory if force rebuild
        if force_rebuild and openssl_src_dir.exists():
            print(f"Removing existing source directory: {openssl_src_dir}")
            shutil.rmtree(openssl_src_dir)

        print(f"\nCloning OpenSSL from git...")
        success = obt.git.Clone(
            url="https://github.com/openssl/openssl.git",
            dest=str(openssl_src_dir),
            rev="master",
            cache=False,
            shallow=False
        )
        if not success:
            print(f"✗ Failed to clone OpenSSL")
            return False

        # Checkout OpenSSL 3.6.0 tag
        print(f"Checking out openssl-3.6.0 tag...")
        result = subprocess.run(
            ["git", "checkout", "openssl-3.6.0"],
            cwd=str(openssl_src_dir),
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"✗ Failed to checkout openssl-3.6.0 tag")
            print(f"STDERR: {result.stderr}")
            return False

        print(f"✓ Cloned OpenSSL and checked out openssl-3.6.0 tag")
    else:
        print(f"OpenSSL source already exists at {openssl_src_dir}")

    # Clean if requested
    if force_rebuild:
        print(f"Cleaning previous build...")
        subprocess.run(["make", "clean"], cwd=str(openssl_src_dir), capture_output=True)
        subprocess.run(["make", "distclean"], cwd=str(openssl_src_dir), capture_output=True)

    # Determine iOS configuration based on is_simulator
    if is_simulator:
        sdk_name = "iphonesimulator"
        arch = "arm64"
        min_version = "15.0"
        target_triple = "arm64-apple-ios15.0-simulator"
        # OpenSSL target for iOS simulator
        openssl_target = "iossimulator-xcrun"
    else:
        sdk_name = "iphoneos"
        arch = "arm64"
        min_version = "15.0"
        target_triple = "arm64-apple-ios15.0"
        # OpenSSL target for iOS device
        openssl_target = "ios64-xcrun"

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

    print(f"\nConfiguring OpenSSL for iOS {sdk_name} ({arch})...")
    print(f"  SDK: {sdk_path}")
    print(f"  Target: {openssl_target}")
    print(f"  Target triple: {target_triple}")
    print(f"  Min version: {min_version}")

    # Setup environment for cross-compilation with explicit target triple
    env = os.environ.copy()
    env["CC"] = f"xcrun -sdk {sdk_name} clang"
    env["CFLAGS"] = f"-target {target_triple} -arch {arch} -isysroot {sdk_path} -mios-version-min={min_version}"
    env["LDFLAGS"] = f"-target {target_triple} -arch {arch} -isysroot {sdk_path}"

    # Configure OpenSSL
    # Use OpenSSL's built-in iOS targets with explicit environment
    configure_cmd = [
        "./Configure",
        openssl_target,
        f"--prefix={ios_subspace}",
        "--openssldir=" + str(ios_subspace / "ssl"),
        "no-shared",           # Static library only
        "no-async",            # Disable async (not needed for iOS)
        "no-tests",            # Don't build tests
        "no-ui-console",       # No console UI
        "no-deprecated",       # Don't include deprecated APIs
    ]

    result = subprocess.run(
        configure_cmd,
        cwd=str(openssl_src_dir),
        env=env,
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"✗ Configure failed for OpenSSL")
        if result.stdout:
            print("STDOUT:", result.stdout)
        if result.stderr:
            print("STDERR:", result.stderr)
        return False
    else:
        if verbose and result.stdout:
            print(result.stdout)

    # Build
    print(f"\nBuilding OpenSSL...")

    build_cmd = ["make", f"-j{num_cores}"]
    if verbose:
        build_cmd.append("V=1")

    result = subprocess.run(
        build_cmd,
        cwd=str(openssl_src_dir),
        env=env,
        capture_output=not verbose
    )
    if result.returncode != 0:
        print(f"✗ Build failed for OpenSSL")
        return False

    # Install
    print(f"\nInstalling OpenSSL...")

    install_cmd = ["make", "install_sw"]  # install_sw installs software only (no docs)
    result = subprocess.run(
        install_cmd,
        cwd=str(openssl_src_dir),
        env=env,
        capture_output=not verbose
    )
    if result.returncode != 0:
        print(f"✗ Install failed for OpenSSL")
        return False

    # Verify installation
    libssl_file = ios_lib_dir / "libssl.a"
    libcrypto_file = ios_lib_dir / "libcrypto.a"
    header_file = ios_include_dir / "openssl" / "ssl.h"

    if not libssl_file.exists():
        print(f"✗ Library not found at {libssl_file}")
        return False
    if not libcrypto_file.exists():
        print(f"✗ Library not found at {libcrypto_file}")
        return False
    if not header_file.exists():
        print(f"✗ Header not found at {header_file}")
        return False

    # Create manifest file to mark successful build
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# OpenSSL iOS build completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {openssl_src_dir}\n")
            f.write(f"# SDK: {sdk_name}\n")
            f.write(f"# Target: {openssl_target}\n")
            f.write(f"# Install: {ios_subspace}\n")

        print(f"\n✓ OpenSSL for iOS built successfully")
        print(f"  Libraries: {libssl_file}, {libcrypto_file}")
        print(f"  Headers: {ios_include_dir}/openssl/")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ OpenSSL for iOS built successfully")
        print(f"  Libraries: {libssl_file}, {libcrypto_file}")
        print(f"  Headers: {ios_include_dir}/openssl/")

    return True
