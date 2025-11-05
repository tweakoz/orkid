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

from ork.ios import device_manager
import ork.path

parser = argparse.ArgumentParser(description='orkid iOS build')
parser.add_argument('--clean', action="store_true", help='force clean build')
parser.add_argument('--verbose', action="store_true", help='verbose build')
parser.add_argument('--simulator', action="store_true", help='force build for iOS Simulator')
parser.add_argument('--device', action="store_true", help='force build for iOS Device')
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

# Determine target from saved selection or args
stage_dir = obt.path.stage()
selection_file = stage_dir / "ios_device_selection.json"
is_device = _args["device"]
is_simulator = _args["simulator"]

if not is_device and not is_simulator:
    # No explicit target specified, use saved selection
    saved_selection = device_manager.load_device_selection(selection_file)
    if saved_selection:
        is_simulator = (saved_selection["type"] == "simulator")
        is_device = (saved_selection["type"] == "device")
        print(f"Using saved selection: {saved_selection['name']} ({saved_selection['type']})")
    else:
        # Default to simulator
        is_simulator = True
        print("No saved selection found, defaulting to simulator")

# Setup iOS subspace directory structure based on target
if is_simulator:
    ios_subspace = ork.path.iossim_subspace
    ios_builds = ork.path.iossim_builds
    ios_include = ork.path.iossim_include
    ios_lib = ork.path.iossim_lib
    print("Building for iOS Simulator")
else:
    ios_subspace = ork.path.ios_subspace
    ios_builds = ork.path.ios_builds
    ios_include = ork.path.ios_include
    ios_lib = ork.path.ios_lib
    print("Building for iOS Device")

# Create iOS subspace directories
ios_builds.mkdir(parents=True, exist_ok=True)
ios_include.mkdir(parents=True, exist_ok=True)
ios_lib.mkdir(parents=True, exist_ok=True)

build_dest = ios_builds / "orkid"
build_dest.mkdir(parents=True, exist_ok=True)
boost_ios_dir = ios_builds / "boost"

os.environ["ORKID_BUILD_DEST"] = str(build_dest)

prj_root = obt.path.Path(os.environ["ORKID_WORKSPACE_DIR"])

print(f"iOS subspace: {ios_subspace}")
print(f"Build destination: {build_dest}")
print(f"Boost destination: {boost_ios_dir}")
print(f"Include directory: {ios_include}")
print(f"Lib directory: {ios_lib}")

######################################################################
# Install iOS dependencies using provider modules
######################################################################

manifest_dir = ios_subspace / "manifests"
manifest_dir.mkdir(parents=True, exist_ok=True)

# Define dependencies to install
ios_dependencies = [
    {
        "name": "boost",
        "module": "ork.ios.boost",
        "install_func": "build_boost_for_ios",
        "params": lambda: {
            "boost_src_dir": _find_boost_source(),
            "boost_install_dir": boost_ios_dir,
            "is_simulator": is_simulator,
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": _args["rebuild_boost"],
            "verbose": _args["verbose"],
            "num_cores": obt.host.NumCores,
            "ios_deployment_target": "15.0"
        }
    },
    {
        "name": "glm",
        "module": "ork.ios.glm",
        "install_func": "install_glm_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": False
        }
    },
    {
        "name": "lz4",
        "module": "ork.ios.lz4",
        "install_func": "install_lz4_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": False
        }
    },
    {
        "name": "klein",
        "module": "ork.ios.klein",
        "install_func": "install_klein_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": False
        }
    },
    {
        "name": "openblas",
        "module": "ork.ios.openblas",
        "install_func": "install_openblas_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": False
        }
    },
    {
        "name": "easyprof",
        "module": "ork.ios.easyprof",
        "install_func": "install_easyprof_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": False
        }
    },
    {
        "name": "rapidjson",
        "module": "ork.ios.rapidjson",
        "install_func": "install_rapidjson_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": False
        }
    },
    {
        "name": "sigslot",
        "module": "ork.ios.sigslot",
        "install_func": "install_sigslot_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": False
        }
    },
    {
        "name": "curl",
        "module": "ork.ios.curl",
        "install_func": "install_curl_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": False
        }
    },
    {
        "name": "nlohmann",
        "module": "ork.ios.nlohmann",
        "install_func": "install_nlohmann_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": False
        }
    }
]

# Helper function for Boost source lookup
def _find_boost_source():
    boost_base_dir = stage_dir / "builds" / "boost"
    if not boost_base_dir.exists():
        print(f"ERROR: Boost base directory not found at {boost_base_dir}")
        print("Please ensure host Orkid has been built first: ork.build.py")
        sys.exit(-1)

    import importlib
    boost_module = importlib.import_module("ork.ios.boost")
    boost_src_dir = boost_module.find_boost_source(boost_base_dir)
    if boost_src_dir:
        print(f"Found Boost source: {boost_src_dir.name}")
    else:
        print(f"ERROR: Boost source directory not found in {boost_base_dir}")
        print("Expected boost-X.Y.Z subdirectory with bootstrap.sh")
        sys.exit(-1)
    return boost_src_dir

# Install each dependency
for dep in ios_dependencies:
    import importlib
    dep_module = importlib.import_module(dep["module"])
    install_func = getattr(dep_module, dep["install_func"])

    params = dep["params"]()
    success = install_func(**params)

    if not success:
        print(f"ERROR: Failed to install {dep['name']} for iOS")
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

# Install prefix - use iOS subspace
cmd += [f"-DCMAKE_INSTALL_PREFIX={ios_subspace}"]
cmd += [f"-DCMAKE_INSTALL_INCLUDEDIR=include"]
cmd += [f"-DCMAKE_INSTALL_LIBDIR=lib"]

# Add iOS include directory to header search path
cmd += [f"-DCMAKE_INCLUDE_PATH={ios_include}"]

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
    print(f"Install location: {ios_subspace}")
    print(f"  Headers: {ios_include}")
    print(f"  Libraries: {ios_lib}")
    print(f"\nBoost iOS libraries: {boost_ios_dir}/lib")
else:
    print("\n✗ iOS build failed")
    sys.exit(rval)

sys.exit(0)
