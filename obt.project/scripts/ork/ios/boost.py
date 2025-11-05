################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS Boost Builder
# Cross-compile Boost for iOS (simulator and device)
################################################################

import subprocess
import sys
import os
import datetime
from pathlib import Path

######################################################################
# iOS SDK Utilities
######################################################################

def get_ios_sdk_path(is_simulator):
    """Get the iOS SDK path using xcrun

    Args:
        is_simulator: True for simulator SDK, False for device SDK

    Returns:
        Path to iOS SDK
    """
    sdk_type = "iphonesimulator" if is_simulator else "iphoneos"
    try:
        result = subprocess.run(
            ["xcrun", "--sdk", sdk_type, "--show-sdk-path"],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        print(f"ERROR: Could not find {sdk_type} SDK")
        sys.exit(-1)

######################################################################
# Boost Source Directory Detection
######################################################################

def find_boost_source(boost_base_dir):
    """Find Boost source directory

    Args:
        boost_base_dir: Base directory containing Boost (e.g., stage/builds/boost)

    Returns:
        Path to Boost source directory or None if not found
    """
    boost_base_dir = Path(boost_base_dir)

    if not boost_base_dir.exists():
        return None

    # Check for direct boost source
    if (boost_base_dir / "bootstrap.sh").exists():
        return boost_base_dir

    # Look for versioned subdirectory (e.g., boost-1.81.0)
    boost_versions = list(boost_base_dir.glob("boost-*"))
    if boost_versions:
        return boost_versions[0]  # Use first found version

    return None

######################################################################
# Boost iOS Build
######################################################################

def build_boost_for_ios(
    boost_src_dir,
    boost_install_dir,
    is_simulator,
    manifest_dir=None,
    force_rebuild=False,
    verbose=False,
    num_cores=8,
    ios_deployment_target="15.0"
):
    """Build Boost for iOS using b2

    Args:
        boost_src_dir: Path to Boost source directory
        boost_install_dir: Path where Boost will be installed
        is_simulator: True to build for simulator, False for device
        manifest_dir: Directory for storing build manifests (optional)
        force_rebuild: Force rebuild even if manifest exists
        verbose: Enable verbose build output
        num_cores: Number of parallel build jobs
        ios_deployment_target: iOS deployment target version (e.g., "15.0")

    Returns:
        True if successful, False otherwise
    """
    boost_src_dir = Path(boost_src_dir)
    boost_install_dir = Path(boost_install_dir)

    print("\n=== Building Boost for iOS ===")

    # Check if Boost source exists
    if not boost_src_dir.exists():
        print(f"ERROR: Boost source not found at {boost_src_dir}")
        return False

    # Setup manifest file for tracking builds
    if manifest_dir:
        manifest_dir = Path(manifest_dir)
        manifest_dir.mkdir(parents=True, exist_ok=True)

        manifest_name = "boost_ios_simulator" if is_simulator else "boost_ios_device"
        manifest_file = manifest_dir / manifest_name

        # Check if already built (unless rebuild requested)
        if manifest_file.exists() and not force_rebuild:
            print(f"✓ Boost for iOS already built (manifest: {manifest_name})")
            print(f"  Install location: {boost_install_dir}")
            print("  Use force_rebuild=True to rebuild")
            return True

    # Get iOS SDK path
    ios_sdk_path = get_ios_sdk_path(is_simulator)
    print(f"iOS SDK: {ios_sdk_path}")

    # Create target triple
    if is_simulator:
        target_flag = f"-target arm64-apple-ios{ios_deployment_target}-simulator"
    else:
        target_flag = f"-target arm64-apple-ios{ios_deployment_target}"

    # Create user-config.jam for iOS cross-compilation
    user_config_content = f"""
using clang : ios
:
/usr/bin/clang++
:
<compileflags>"-isysroot {ios_sdk_path} {target_flag} -fPIC"
<linkflags>"-isysroot {ios_sdk_path} {target_flag}"
;
"""

    user_config_path = boost_src_dir / "user-config-ios.jam"
    with open(user_config_path, 'w') as f:
        f.write(user_config_content)

    print(f"Created user-config.jam at {user_config_path}")

    # Bootstrap if needed
    b2_path = boost_src_dir / "b2"
    if not b2_path.exists():
        print("Bootstrapping Boost build system...")
        bootstrap_script = boost_src_dir / "bootstrap.sh"
        if bootstrap_script.exists():
            original_dir = os.getcwd()
            os.chdir(boost_src_dir)
            result = subprocess.run(["./bootstrap.sh"])
            os.chdir(original_dir)
            if result.returncode != 0:
                print("ERROR: Boost bootstrap failed")
                return False
        else:
            print("ERROR: bootstrap.sh not found in Boost directory")
            return False

    # Build Boost for iOS
    original_dir = os.getcwd()
    os.chdir(boost_src_dir)

    # Set environment variables for iOS build
    build_env = os.environ.copy()
    build_env["CFLAGS"] = f"-isysroot {ios_sdk_path} {target_flag}"
    build_env["CXXFLAGS"] = f"-isysroot {ios_sdk_path} {target_flag}"
    build_env["LDFLAGS"] = f"-isysroot {ios_sdk_path} {target_flag}"

    # Ensure install directory exists
    boost_install_dir.mkdir(parents=True, exist_ok=True)

    b2_cmd = [
        str(b2_path),
        f"--user-config={user_config_path}",
        "--with-system",
        "--with-filesystem",
        "--prefix=" + str(boost_install_dir),
        "toolset=clang-ios",
        "target-os=iphone",
        "link=static",          # iOS prefers static libs for Boost
        "variant=release",
        "threading=multi",
        f"-j{num_cores}",
        "install"
    ]

    if verbose:
        b2_cmd.append("-d+2")  # Verbose output

    print("\nBuilding Boost libraries for iOS...")
    print(" ".join(b2_cmd))

    result = subprocess.run(b2_cmd, env=build_env)
    os.chdir(original_dir)

    if result.returncode != 0:
        print("\n✗ Boost iOS build failed")
        return False

    # Create manifest file to mark successful build
    if manifest_dir:
        with open(manifest_file, 'w') as f:
            f.write(f"# Boost iOS build completed\n")
            f.write(f"# Date: {datetime.datetime.now()}\n")
            f.write(f"# Simulator: {is_simulator}\n")
            f.write(f"# Install: {boost_install_dir}\n")
            f.write(f"# Source: {boost_src_dir}\n")

        print(f"\n✓ Boost for iOS built successfully")
        print(f"  Install location: {boost_install_dir}")
        print(f"  Manifest: {manifest_file}")
    else:
        print(f"\n✓ Boost for iOS built successfully")
        print(f"  Install location: {boost_install_dir}")

    return True
