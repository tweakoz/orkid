#!/usr/bin/env python3

################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS Dependency Builder
# Build individual iOS dependencies with optional clean
################################################################

import sys
import os
import argparse
import importlib
import obt.host
import obt.path
from pathlib import Path

import ork.path
from ork.ios import device_manager

parser = argparse.ArgumentParser(description='Build individual iOS dependency')
parser.add_argument('dependency', help='Dependency name (boost, lz4, curl, glm, etc.)')
parser.add_argument('--clean', action="store_true", help='force clean build')
parser.add_argument('--verbose', action="store_true", help='verbose build')
parser.add_argument('--simulator', action="store_true", help='force build for iOS Simulator')
parser.add_argument('--device', action="store_true", help='force build for iOS Device')

_args = vars(parser.parse_args())

# Get paths
this_path = os.path.realpath(__file__)
this_dir = os.path.dirname(this_path)
this_dir = os.path.dirname(this_dir)
this_dir = os.path.dirname(this_dir)

print(f"iOS Dependency Builder")
print(f"Project root: {this_dir}")
print(f"Building: {_args['dependency']}")

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

manifest_dir = ios_subspace / "manifests"
manifest_dir.mkdir(parents=True, exist_ok=True)

prj_root = obt.path.Path(os.environ["ORKID_WORKSPACE_DIR"])

print(f"iOS subspace: {ios_subspace}")
print(f"Include directory: {ios_include}")
print(f"Lib directory: {ios_lib}")

######################################################################
# Dependency Helper Functions
######################################################################

def header_only_dependency(name):
    """Create dependency config for header-only libraries"""
    return {
        "name": name,
        "module": f"ork.ios.{name}",
        "install_func": f"install_{name}_for_ios",
        "params": lambda: {
            "host_include_dir": stage_dir / "include",
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": _args["clean"]
        }
    }

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

######################################################################
# Dependency Configurations
######################################################################

# Header-only libraries
header_only_libs = ["glm", "klein", "openblas", "easyprof", "rapidjson", "sigslot", "nlohmann"]

boost_ios_dir = ios_builds / "boost"

# All dependency configurations
all_dependencies = {
    "boost": {
        "name": "boost",
        "module": "ork.ios.boost",
        "install_func": "build_boost_for_ios",
        "params": lambda: {
            "boost_src_dir": _find_boost_source(),
            "boost_install_dir": boost_ios_dir,
            "is_simulator": is_simulator,
            "ios_include_dir": ios_include,
            "manifest_dir": manifest_dir,
            "force_rebuild": _args["clean"],
            "verbose": _args["verbose"],
            "num_cores": obt.host.NumCores,
            "ios_deployment_target": "15.0"
        }
    },
    "lz4": {
        "name": "lz4",
        "module": "ork.ios.lz4",
        "install_func": "build_lz4_for_ios",
        "params": lambda: {
            "ios_subspace": ios_subspace,
            "ios_builds_dir": ios_builds,
            "ios_include_dir": ios_include,
            "ios_lib_dir": ios_lib,
            "is_simulator": is_simulator,
            "manifest_dir": manifest_dir,
            "force_rebuild": _args["clean"],
            "verbose": _args["verbose"],
            "num_cores": obt.host.NumCores
        }
    },
    "curl": {
        "name": "curl",
        "module": "ork.ios.curl",
        "install_func": "build_curl_for_ios",
        "params": lambda: {
            "ios_subspace": ios_subspace,
            "ios_builds_dir": ios_builds,
            "ios_include_dir": ios_include,
            "ios_lib_dir": ios_lib,
            "is_simulator": is_simulator,
            "manifest_dir": manifest_dir,
            "force_rebuild": _args["clean"],
            "verbose": _args["verbose"],
            "num_cores": obt.host.NumCores
        }
    }
}

# Add header-only libraries
for name in header_only_libs:
    all_dependencies[name] = header_only_dependency(name)

######################################################################
# Build requested dependency
######################################################################

dep_name = _args["dependency"]

if dep_name not in all_dependencies:
    print(f"\nERROR: Unknown dependency '{dep_name}'")
    print(f"\nAvailable dependencies:")
    for name in sorted(all_dependencies.keys()):
        print(f"  - {name}")
    sys.exit(-1)

dep = all_dependencies[dep_name]

print(f"\n{'='*60}")
print(f"Building {dep_name} for iOS")
print(f"{'='*60}\n")

# Import and call the dependency installer
dep_module = importlib.import_module(dep["module"])
install_func = getattr(dep_module, dep["install_func"])

params = dep["params"]()
success = install_func(**params)

if not success:
    print(f"\n✗ Failed to build {dep_name} for iOS")
    sys.exit(-1)

print(f"\n{'='*60}")
print(f"✓ {dep_name} built successfully for iOS")
print(f"{'='*60}")

sys.exit(0)
