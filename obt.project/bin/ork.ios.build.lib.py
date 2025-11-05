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

# Add ork.ios module to path
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__)))), "obt.project", "scripts"))
from ork.ios import boost as ios_boost

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
# Build Boost for iOS (using ork.ios.boost module)
######################################################################

# Determine Boost source directory
boost_base_dir = stage_dir / "builds" / "boost"

# Check if boost directory exists
if not boost_base_dir.exists():
    print(f"ERROR: Boost base directory not found at {boost_base_dir}")
    print("Please ensure host Orkid has been built first: ork.build.py")
    sys.exit(-1)

# Find the actual Boost source directory using the module
boost_src_dir = ios_boost.find_boost_source(boost_base_dir)
if boost_src_dir:
    print(f"Found Boost source: {boost_src_dir.name}")
else:
    print(f"ERROR: Boost source directory not found in {boost_base_dir}")
    print("Expected boost-X.Y.Z subdirectory with bootstrap.sh")
    sys.exit(-1)

# Build Boost for iOS using the module
manifest_dir = stage_dir / "manifests"
success = ios_boost.build_boost_for_ios(
    boost_src_dir=boost_src_dir,
    boost_install_dir=boost_ios_dir,
    is_simulator=is_simulator,
    manifest_dir=manifest_dir,
    force_rebuild=_args["rebuild_boost"],
    verbose=_args["verbose"],
    num_cores=obt.host.NumCores,
    ios_deployment_target="15.0"
)

if not success:
    sys.exit(-1)

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
