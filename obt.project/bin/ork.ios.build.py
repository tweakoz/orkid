#!/usr/bin/env python3

################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS Build Script
# Builds minimal ork.core for iOS platform
# Includes automatic Boost cross-compilation for iOS
################################################################

import sys
import os
import argparse
import subprocess
import glob
import datetime
import obt.host
import obt.path
from obt.command import Command

parser = argparse.ArgumentParser(description='orkid iOS build')
parser.add_argument('--clean', action="store_true", help='force clean build')
parser.add_argument('--verbose', action="store_true", help='verbose build')
parser.add_argument('--simulator', action="store_true", help='build for iOS Simulator instead of device')
parser.add_argument('--debug', action="store_true", help='debug build')
parser.add_argument('--rebuild-boost', action="store_true", help='force rebuild of Boost for iOS')

_args = vars(parser.parse_args())

# Get paths
this_path = os.path.realpath(__file__)
this_dir = os.path.dirname(this_path)
this_dir = os.path.dirname(this_dir)
this_dir = os.path.dirname(this_dir)

print(f"Orkid iOS Build")
print(f"Project root: {this_dir}")

# Set environment
os.environ["ORKID_WORKSPACE_DIR"] = this_dir

# Setup build directory
stage_dir = obt.path.Path(os.path.abspath(str(obt.path.stage())))
is_simulator = _args["simulator"]

if is_simulator:
    build_dest = stage_dir / "orkid-ios-simulator"
    boost_ios_dir = stage_dir / "builds" / "boost-ios-simulator"
    print("Building for iOS Simulator")
else:
    build_dest = stage_dir / "orkid-ios"
    boost_ios_dir = stage_dir / "builds" / "boost-ios"
    print("Building for iOS Device")

build_dest.mkdir(parents=True, exist_ok=True)
boost_ios_dir.mkdir(parents=True, exist_ok=True)

os.environ["ORKID_BUILD_DEST"] = str(build_dest)

prj_root = obt.path.Path(os.environ["ORKID_WORKSPACE_DIR"])

print(f"Build destination: {build_dest}")
print(f"Boost iOS destination: {boost_ios_dir}")

######################################################################
# Build Boost for iOS
######################################################################

def get_ios_sdk_path(is_simulator):
    """Get the iOS SDK path using xcrun"""
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

def build_boost_for_ios(boost_src_dir, boost_install_dir, is_simulator, verbose, stage_dir):
    """Build Boost for iOS using b2"""

    print("\n=== Building Boost for iOS ===")

    # Check if Boost source exists
    if not boost_src_dir.exists():
        print(f"ERROR: Boost source not found at {boost_src_dir}")
        print("Please ensure Boost is built for the host first")
        sys.exit(-1)

    # Setup manifest file for tracking builds
    manifest_dir = stage_dir / "manifests"
    manifest_dir.mkdir(parents=True, exist_ok=True)

    manifest_name = "boost_ios_simulator" if is_simulator else "boost_ios_device"
    manifest_file = manifest_dir / manifest_name

    # Check if already built (unless rebuild requested)
    if manifest_file.exists() and not _args["rebuild_boost"]:
        print(f"✓ Boost for iOS already built (manifest: {manifest_name})")
        print(f"  Install location: {boost_install_dir}")
        print("  Use --rebuild-boost to force rebuild")
        return

    # Get iOS SDK path
    ios_sdk_path = get_ios_sdk_path(is_simulator)
    print(f"iOS SDK: {ios_sdk_path}")

    # Architecture settings
    if is_simulator:
        # Simulator: arm64 only (Apple Silicon)
        archs = ["arm64"]
        arch_flags = "-arch arm64"
    else:
        # Device is arm64 only
        archs = ["arm64"]
        arch_flags = "-arch arm64"

    # Create user-config.jam for iOS cross-compilation
    # For simulator, we need to explicitly set the target triple
    if is_simulator:
        target_flag = "-target arm64-apple-ios15.0-simulator"
    else:
        target_flag = "-target arm64-apple-ios15.0"

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
            os.chdir(boost_src_dir)
            result = Command(["./bootstrap.sh"]).exec()
            if result != 0:
                print("ERROR: Boost bootstrap failed")
                sys.exit(-1)
        else:
            print("ERROR: bootstrap.sh not found in Boost directory")
            sys.exit(-1)

    # Build Boost for iOS
    os.chdir(boost_src_dir)

    # Set environment variables for iOS build
    build_env = os.environ.copy()
    if is_simulator:
        build_env["CFLAGS"] = f"-isysroot {ios_sdk_path} -target arm64-apple-ios15.0-simulator"
        build_env["CXXFLAGS"] = f"-isysroot {ios_sdk_path} -target arm64-apple-ios15.0-simulator"
        build_env["LDFLAGS"] = f"-isysroot {ios_sdk_path} -target arm64-apple-ios15.0-simulator"
    else:
        build_env["CFLAGS"] = f"-isysroot {ios_sdk_path} -target arm64-apple-ios15.0"
        build_env["CXXFLAGS"] = f"-isysroot {ios_sdk_path} -target arm64-apple-ios15.0"
        build_env["LDFLAGS"] = f"-isysroot {ios_sdk_path} -target arm64-apple-ios15.0"

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
        f"-j{obt.host.NumCores}",
        "install"
    ]

    if verbose:
        b2_cmd.append("-d+2")  # Verbose output

    print("\nBuilding Boost libraries for iOS...")
    print(" ".join(b2_cmd))

    result = subprocess.run(b2_cmd, env=build_env)
    result = result.returncode

    if result != 0:
        print("\n✗ Boost iOS build failed")
        sys.exit(-1)

    # Create manifest file to mark successful build
    with open(manifest_file, 'w') as f:
        f.write(f"# Boost iOS build completed\n")
        f.write(f"# Date: {datetime.datetime.now()}\n")
        f.write(f"# Simulator: {is_simulator}\n")
        f.write(f"# Install: {boost_install_dir}\n")
        f.write(f"# Source: {boost_src_dir}\n")

    print(f"\n✓ Boost for iOS built successfully")
    print(f"  Install location: {boost_install_dir}")
    print(f"  Manifest: {manifest_file}")

# Determine Boost source directory
boost_base_dir = stage_dir / "builds" / "boost"

# Check if boost directory exists
if not boost_base_dir.exists():
    print(f"ERROR: Boost base directory not found at {boost_base_dir}")
    print("Please ensure host Orkid has been built first: ork.build.py")
    sys.exit(-1)

# Find the actual Boost source directory (e.g., boost-1.81.0)
boost_src_dir = None
if boost_base_dir.exists():
    # Check for direct boost source
    if (boost_base_dir / "bootstrap.sh").exists():
        boost_src_dir = boost_base_dir
    else:
        # Look for versioned subdirectory (e.g., boost-1.81.0)
        boost_versions = list(boost_base_dir.glob("boost-*"))
        if boost_versions:
            boost_src_dir = boost_versions[0]  # Use first found version
            print(f"Found Boost source: {boost_src_dir.name}")

if not boost_src_dir or not boost_src_dir.exists():
    print(f"ERROR: Boost source directory not found in {boost_base_dir}")
    print("Expected boost-X.Y.Z subdirectory with bootstrap.sh")
    sys.exit(-1)

# Build Boost for iOS
build_boost_for_ios(boost_src_dir, boost_ios_dir, is_simulator, _args["verbose"], stage_dir)

######################################################################
# Configure CMake for iOS
######################################################################

build_dest.chdir()

cmd = ["cmake"]

# Use iOS toolchain
toolchain_file = prj_root / "cmake/toolchains/ios.toolchain.cmake"
cmd += [f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}"]

# Build type
if _args["debug"]:
    cmd += ["-DCMAKE_BUILD_TYPE=Debug"]
else:
    cmd += ["-DCMAKE_BUILD_TYPE=Release"]

# iOS-specific options
cmd += ["-DBUILD_IOS_MINIMAL=ON"]
cmd += ["-DIOS_BUILD=ON"]

# Simulator mode
if is_simulator:
    cmd += ["-DIOS_SIMULATOR=ON"]
else:
    cmd += ["-DIOS_SIMULATOR=OFF"]

# Architecture
cmd += ["-DARCHITECTURE=AARCH64"]

# Install prefix
ios_install_prefix = stage_dir / "ios-sdk"
cmd += [f"-DCMAKE_INSTALL_PREFIX={ios_install_prefix}"]

# Point to iOS-specific Boost
cmd += [f"-DBOOST_ROOT={boost_ios_dir}"]
cmd += [f"-DBoost_INCLUDE_DIR={boost_ios_dir}/include"]
cmd += [f"-DBoost_LIBRARY_DIR={boost_ios_dir}/lib"]
cmd += ["-DBoost_USE_STATIC_LIBS=ON"]
cmd += ["-DBoost_NO_SYSTEM_PATHS=ON"]

# Disable components not needed for iOS
cmd += ["-DENABLE_PYTHON=OFF"]
cmd += ["-DENABLE_OPENCL=OFF"]

# Warning suppression
cmd += ["-Wno-dev"]

# Project root
cmd += [str(prj_root)]

print("\n=== CMake Configuration ===")
print(" ".join(cmd))
print("===========================\n")

ok = (Command(cmd).exec() == 0)

if not ok:
    print("ERROR: CMake configuration failed")
    sys.exit(-1)

######################################################################
# Build
######################################################################

build_dest.chdir()

if _args["clean"]:
    print("Cleaning previous build...")
    ok = (Command(["make", "clean"]).exec() == 0)
    if not ok:
        sys.exit(-1)

cmd = ["make"]

if _args["verbose"]:
    cmd += ["VERBOSE=1"]

# Parallel build
cmd += ["-j", str(obt.host.NumCores)]

# Build only ork_core_ios target
cmd += ["ork_core_ios"]

print("\n=== Building Orkid iOS ===")
print(" ".join(cmd))
print("==========================\n")

rval = Command(cmd).exec()

if rval == 0:
    print("\n✓ iOS build successful!")
    print(f"Build output: {build_dest}")
    print(f"\nTo install: cd {build_dest} && make install")
    print(f"Install location: {ios_install_prefix}")
    print(f"\nBoost iOS libraries: {boost_ios_dir}/lib")
else:
    print("\n✗ iOS build failed")
    sys.exit(rval)

sys.exit(0)
