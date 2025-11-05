#!/usr/bin/env python3

################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS Test App Quick Build
# Rebuilds only the iOS test app (no CMake reconfigure, no Boost check)
################################################################

import sys
import os
import argparse
import obt.host
import obt.path
from obt.command import Command
from pathlib import Path

parser = argparse.ArgumentParser(description='orkid iOS test app quick build')
parser.add_argument('--verbose', action="store_true", help='verbose build')
parser.add_argument('--simulator', action="store_true", help='force build for iOS Simulator')
parser.add_argument('--device', action="store_true", help='force build for iOS Device')
parser.add_argument('--debug', action="store_true", help='debug build')
parser.add_argument('--release', action="store_true", help='release build')

_args = vars(parser.parse_args())

# Get paths
this_path = os.path.realpath(__file__)
this_dir = os.path.dirname(this_path)
this_dir = os.path.dirname(this_dir)
this_dir = os.path.dirname(this_dir)

os.environ["ORKID_WORKSPACE_DIR"] = this_dir

# Import OBT modules
sys.path.insert(0, os.path.join(this_dir, "obt.project", "python"))
sys.path.insert(0, os.path.join(this_dir, "obt.project", "scripts"))

from ork.ios import device_manager

print(f"Orkid iOS Test App Quick Build")

# Setup build directory
stage_dir = Path(os.path.abspath(str(obt.path.stage())))
selection_file = stage_dir / "ios_device_selection.json"

# Determine target from saved selection or args
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

# Determine build type
if _args["release"]:
    is_debug = False
elif _args["debug"]:
    is_debug = True
else:
    # Default to debug
    is_debug = True

if is_simulator:
    build_dest = stage_dir / "orkid-ios-simulator"
    print("Building for: iOS Simulator")
else:
    build_dest = stage_dir / "orkid-ios"
    print("Building for: iOS Device")

if not build_dest.exists():
    print(f"ERROR: Build directory not found: {build_dest}")
    print("Run ork.ios.build.py first to configure and build the iOS library")
    sys.exit(-1)

build_type = "Debug" if is_debug else "Release"

cmd = ["cmake", "--build", str(build_dest), "--target", "ork_test_ios", "--config", build_type]

if _args["verbose"]:
    cmd += ["--verbose"]
else:
    # Parallel build
    cmd += ["--parallel", str(obt.host.NumCores)]

print(f"Building iOS test app ({build_type})...")
print(" ".join(cmd))

rval = Command(cmd, working_dir=build_dest).exec()

if rval == 0:
    print(f"\n✓ iOS test app build successful!")
    app_path = build_dest / "ork.core" / "ork_test_ios.app"
    print(f"App bundle: {app_path}")
else:
    print("\n✗ iOS test app build failed")
    sys.exit(rval)

sys.exit(0)
