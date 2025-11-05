################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS libcurl Provider
# Builds libcurl from source for iOS using its existing CMakeLists.txt
################################################################

import datetime
import os
import subprocess
import obt.path
from pathlib import Path

######################################################################
# libcurl iOS Provider
######################################################################

def build_curl_for_ios(
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
    """Build libcurl for iOS from source using existing CMakeLists.txt

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

    print("\n=== Building libcurl for iOS ===")

    # Check manifest
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_file = manifest_dir / "curl"

        # Check if already built (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ libcurl for iOS already built (manifest: curl)")
            print(f"  Location: {ios_lib_dir}/libcurl.a")
            return True

    # libcurl source location
    stage_dir = obt.path.stage()
    curl_src_dir = stage_dir / "builds" / "libcurl"

    if not curl_src_dir.exists():
        print(f"ERROR: libcurl source not found at {curl_src_dir}")
        print("Please ensure host Orkid has been built first")
        return False

    # Build directory
    build_dir = ios_builds_dir / "curl"
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
    print(f"\nConfiguring libcurl for iOS...")

    cmake_cmd = [
        "cmake",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
        f"-DCMAKE_INSTALL_PREFIX={ios_subspace}",
        "-DBUILD_SHARED_LIBS=OFF",  # Static library
        "-DBUILD_STATIC_LIBS=ON",   # Explicitly enable static
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DIOS_SIMULATOR={'ON' if is_simulator else 'OFF'}",
        "-DARCHITECTURE=AARCH64",
        # Enable OpenSSL for HTTPS support
        "-DCURL_USE_OPENSSL=ON",
        f"-DOPENSSL_ROOT_DIR={ios_subspace}",
        f"-DOPENSSL_INCLUDE_DIR={ios_include_dir}",
        f"-DOPENSSL_SSL_LIBRARY={ios_lib_dir}/libssl.a",
        f"-DOPENSSL_CRYPTO_LIBRARY={ios_lib_dir}/libcrypto.a",
        # Disable other features not needed
        "-DCURL_USE_LIBSSH2=OFF",
        "-DCURL_USE_LIBPSL=OFF",
        "-DUSE_LIBIDN2=OFF",
        "-DCURL_DISABLE_LDAP=ON",
        "-DCURL_DISABLE_LDAPS=ON",
        "-DCURL_DISABLE_TELNET=ON",
        "-DCURL_DISABLE_DICT=ON",
        "-DCURL_DISABLE_FILE=ON",
        "-DCURL_DISABLE_TFTP=ON",
        "-DCURL_DISABLE_RTSP=ON",
        "-DCURL_DISABLE_POP3=ON",
        "-DCURL_DISABLE_IMAP=ON",
        "-DCURL_DISABLE_SMTP=ON",
        "-DCURL_DISABLE_GOPHER=ON",
        "-DCURL_DISABLE_MQTT=ON",
        "-DCURL_DISABLE_NTLM=ON",  # OpenSSL 3.x doesn't support DES (required for NTLM)
        "-DBUILD_CURL_EXE=OFF",  # Don't build curl executable
        "-DBUILD_TESTING=OFF",
        str(curl_src_dir)
    ]

    result = subprocess.run(cmake_cmd, cwd=build_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"✗ CMake configuration failed for libcurl (return code: {result.returncode})")
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
    print(f"\nBuilding libcurl...")

    build_cmd = ["cmake", "--build", str(build_dir), "--parallel", str(num_cores)]
    if verbose:
        build_cmd.append("--verbose")

    result = subprocess.run(build_cmd, capture_output=not verbose)
    if result.returncode != 0:
        print(f"✗ Build failed for libcurl")
        return False

    # Install
    print(f"\nInstalling libcurl...")

    install_cmd = ["cmake", "--install", str(build_dir)]
    result = subprocess.run(install_cmd, capture_output=not verbose)
    if result.returncode != 0:
        print(f"✗ Install failed for libcurl")
        return False

    # Create manifest file to mark successful build
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# libcurl iOS build completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Source: {curl_src_dir}\n")
            f.write(f"# Build: {build_dir}\n")
            f.write(f"# Install: {ios_subspace}\n")

        print(f"\n✓ libcurl for iOS built successfully")
        print(f"  Library: {ios_lib_dir}/libcurl.a")
        print(f"  Headers: {ios_include_dir}/curl/")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ libcurl for iOS built successfully")
        print(f"  Library: {ios_lib_dir}/libcurl.a")
        print(f"  Headers: {ios_include_dir}/curl/")

    return True
