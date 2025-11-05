#!/usr/bin/env python3

################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS Test App Launch Script
# Builds and launches ork.test.ios on simulator or device
################################################################

import sys
import os
import argparse
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description='orkid iOS test app launcher')
parser.add_argument('--device', action="store_true", help='install and run on connected device')
parser.add_argument('--simulator', type=str, default=None, help='simulator UDID to use (overrides saved selection)')
parser.add_argument('--list-simulators', action="store_true", help='list available simulators')
parser.add_argument('--list-devices', action="store_true", help='list connected devices')
parser.add_argument('--select', action="store_true", help='interactively select default simulator/device')
parser.add_argument('--build', action="store_true", help='force rebuild before launching')
parser.add_argument('--debug', action="store_true", help='debug build')
parser.add_argument('--xcode', action="store_true", help='open in Xcode debugger instead of direct launch')

_args = vars(parser.parse_args())

# Get paths
this_path = os.path.realpath(__file__)
this_dir = os.path.dirname(this_path)
this_dir = os.path.dirname(this_dir)
this_dir = os.path.dirname(this_dir)

print(f"Orkid iOS Test App Launcher")
print(f"Project root: {this_dir}")

os.environ["ORKID_WORKSPACE_DIR"] = this_dir

# Import OBT modules
sys.path.insert(0, os.path.join(this_dir, "obt.project", "python"))
sys.path.insert(0, os.path.join(this_dir, "obt.project", "scripts"))

import obt.path
from ork.ios import device_manager, app_launcher, xcode_debug

stage_dir = Path(os.path.abspath(str(obt.path.stage())))
selection_file = stage_dir / "ios_device_selection.json"

######################################################################
# Orkid-specific Configuration
######################################################################

BUNDLE_ID = "com.tweakoz.orkid.test"
APP_NAME = "ork_test_ios"

######################################################################
# Build Management
######################################################################

def build_ios_test_app(is_simulator, is_debug):
    """Build the iOS test app using ork.ios.build.py

    Args:
        is_simulator: True to build for simulator, False for device
        is_debug: True for debug build, False for release

    Returns:
        Path to .app bundle or None if build failed
    """
    print(f"\n=== Building iOS Test App ===")

    if is_simulator:
        build_dest = stage_dir / "orkid-ios-simulator"
        print("Building for iOS Simulator")
    else:
        build_dest = stage_dir / "orkid-ios"
        print("Building for iOS Device")

    # Run the iOS build script first
    print("Running iOS build script...")
    build_cmd = [
        "python3",
        os.path.join(this_dir, "obt.project", "bin", "ork.ios.build.py")
    ]
    if is_simulator:
        build_cmd.append("--simulator")
    if is_debug:
        build_cmd.append("--debug")

    result = subprocess.run(build_cmd)
    if result.returncode != 0:
        print("ERROR: iOS library build failed")
        return None

    # Build the test app
    print("\nBuilding test app...")
    build_type = "Debug" if is_debug else "Release"

    build_cmd = ["cmake", "--build", str(build_dest), "--target", APP_NAME, "--config", build_type]

    result = subprocess.run(build_cmd, cwd=this_dir)
    if result.returncode != 0:
        print("ERROR: Test app build failed")
        return None

    # Find the .app bundle
    app_path = build_dest / build_type / f"{APP_NAME}.app"
    if not app_path.exists():
        app_path = build_dest / f"{APP_NAME}.app"

    if not app_path.exists():
        # Try in ork.core subdirectory (CMake places it there)
        app_path = build_dest / "ork.core" / f"{APP_NAME}.app"

    if not app_path.exists():
        print(f"ERROR: Could not find {APP_NAME}.app")
        return None

    print(f"Build complete: {app_path}")
    return app_path

def find_existing_app(is_simulator, is_debug):
    """Find existing app bundle

    Args:
        is_simulator: True for simulator build, False for device
        is_debug: True for debug build, False for release

    Returns:
        Path to .app bundle or None if not found
    """
    build_type = "Debug" if is_debug else "Release"
    if is_simulator:
        build_dest = stage_dir / "orkid-ios-simulator"
    else:
        build_dest = stage_dir / "orkid-ios"

    app_path = build_dest / build_type / f"{APP_NAME}.app"
    if app_path.exists():
        return app_path

    app_path = build_dest / f"{APP_NAME}.app"
    if app_path.exists():
        return app_path

    app_path = build_dest / "ork.core" / f"{APP_NAME}.app"
    if app_path.exists():
        return app_path

    return None

######################################################################
# Main
######################################################################

def main():
    # Handle list commands
    if _args["list_simulators"]:
        device_manager.list_simulators()
        return

    if _args["list_devices"]:
        device_manager.list_devices()
        return

    # Handle device selection
    if _args["select"]:
        device_manager.interactive_device_selection(selection_file)
        return

    # Determine target
    is_device = _args["device"]
    is_simulator = not is_device
    is_debug = _args["debug"]

    # Determine which simulator/device to use
    target_udid = None

    if _args["simulator"]:
        # Explicit simulator UDID provided - use it
        target_udid = _args["simulator"]
        is_simulator = True
        is_device = False
    elif not is_device:
        # No explicit device specified, try to use saved selection
        saved_selection = device_manager.load_device_selection(selection_file)
        if saved_selection:
            target_udid = saved_selection["udid"]
            print(f"\nUsing saved selection: {saved_selection['name']}")
            print(f"  Type: {saved_selection['type']}")
            print(f"  UDID: {saved_selection['udid']}")
            print(f"  (Use --select to change)\n")

            if saved_selection["type"] == "device":
                is_device = True
                is_simulator = False
        # Otherwise, target_udid remains None and will auto-detect booted simulator

    # Build if requested or if app doesn't exist
    app_path = None
    if _args["build"]:
        app_path = build_ios_test_app(is_simulator=is_simulator, is_debug=is_debug)
    else:
        # Try to find existing build
        app_path = find_existing_app(is_simulator, is_debug)
        if not app_path:
            print("App not found, building...")
            app_path = build_ios_test_app(is_simulator=is_simulator, is_debug=is_debug)

    if app_path is None:
        print("ERROR: Build failed")
        sys.exit(-1)

    # Launch or open in Xcode
    if _args["xcode"]:
        # Open in Xcode debugger
        if is_device:
            print("ERROR: Xcode debugging for physical devices not yet supported")
            print("Use --simulator to debug on simulator")
            sys.exit(-1)

        # Ensure we have a target UDID
        if target_udid is None:
            target_udid = device_manager.get_booted_simulator()
            if target_udid is None:
                print("ERROR: No booted simulator found")
                print("Please boot a simulator or use --simulator <UDID>")
                sys.exit(-1)

        # Ensure we have a target UDID
        if target_udid is None:
            target_udid = device_manager.get_booted_simulator()
            if target_udid is None:
                print("ERROR: No booted simulator found")
                print("Please boot a simulator or use --simulator <UDID>")
                sys.exit(-1)

        # Create Xcode workspace
        workspace_name = f"{APP_NAME}_debug"
        workspace_path = stage_dir / "tempdir" / f"{workspace_name}.xcworkspace"

        # Environment variables (can be customized)
        env_vars = {
            "ORKID_WORKSPACE_DIR": os.environ.get("ORKID_WORKSPACE_DIR"),
            "OBT_STAGE": os.environ.get("OBT_STAGE"),
        }

        xcode_debug.create_ios_xcode_workspace(
            workspace_path=workspace_path,
            app_bundle_path=app_path,
            bundle_id=BUNDLE_ID,
            simulator_udid=target_udid,
            env_vars=env_vars,
            working_dir=this_dir
        )

    else:
        # Direct launch
        if is_device:
            app_launcher.install_and_launch_on_device(app_path, BUNDLE_ID)
        else:
            app_launcher.install_and_launch_on_simulator(app_path, BUNDLE_ID, target_udid)

if __name__ == "__main__":
    main()
