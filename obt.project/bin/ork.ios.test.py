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
import json
import time
from pathlib import Path

parser = argparse.ArgumentParser(description='orkid iOS test app launcher')
parser.add_argument('--device', action="store_true", help='install and run on connected device')
parser.add_argument('--simulator', type=str, default=None, help='simulator UDID to use (overrides saved selection)')
parser.add_argument('--list-simulators', action="store_true", help='list available simulators')
parser.add_argument('--list-devices', action="store_true", help='list connected devices')
parser.add_argument('--select', action="store_true", help='interactively select default simulator/device')
parser.add_argument('--build', action="store_true", help='force rebuild before launching')
parser.add_argument('--debug', action="store_true", help='debug build')

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
import obt.host
import obt.path

stage_dir = Path(os.path.abspath(str(obt.path.stage())))

######################################################################
# Device Selection Persistence
######################################################################

def get_selection_file():
    """Get path to the device selection JSON file"""
    return stage_dir / "ios_device_selection.json"

def save_device_selection(udid, name, device_type):
    """Save selected device UDID to JSON file

    Args:
        udid: Device UDID
        name: Device name (e.g., "iPhone 16 Pro")
        device_type: "simulator" or "device"
    """
    selection_file = get_selection_file()
    data = {
        "udid": udid,
        "name": name,
        "type": device_type
    }

    with open(selection_file, 'w') as f:
        json.dump(data, f, indent=2)

    print(f"\n✓ Saved selection: {name} ({device_type})")
    print(f"  UDID: {udid}")
    print(f"  Selection file: {selection_file}")

def load_device_selection():
    """Load saved device selection from JSON file

    Returns:
        dict with 'udid', 'name', 'type' or None if no selection saved
    """
    selection_file = get_selection_file()

    if not selection_file.exists():
        return None

    try:
        with open(selection_file, 'r') as f:
            return json.load(f)
    except (json.JSONDecodeError, IOError):
        return None

######################################################################
# Simulator and Device Management
######################################################################

def list_simulators():
    """List available iOS simulators"""
    try:
        result = subprocess.run(
            ["xcrun", "simctl", "list", "devices", "available", "-j"],
            capture_output=True,
            text=True,
            check=True
        )
        data = json.loads(result.stdout)

        print("\n=== Available iOS Simulators ===\n")
        for runtime, devices in data.get("devices", {}).items():
            if "iOS" in runtime:
                ios_version = runtime.split(".")[-1].replace("-", ".")
                print(f"iOS {ios_version}:")
                for device in devices:
                    if device.get("isAvailable", False):
                        state = device.get("state", "Unknown")
                        name = device.get("name", "Unknown")
                        udid = device.get("udid", "Unknown")
                        print(f"  [{state:8}] {name:30} {udid}")
                print()

    except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
        print(f"ERROR: Could not list simulators: {e}")
        sys.exit(-1)

def list_devices():
    """List connected iOS devices"""
    try:
        result = subprocess.run(
            ["xcrun", "xctrace", "list", "devices"],
            capture_output=True,
            text=True,
            check=True
        )

        print("\n=== Connected iOS Devices ===\n")
        lines = result.stdout.split("\n")
        in_devices = False
        for line in lines:
            if "== Devices ==" in line:
                in_devices = True
                continue
            if in_devices and line.strip():
                print(f"  {line}")
        print()

    except subprocess.CalledProcessError as e:
        print(f"ERROR: Could not list devices: {e}")
        sys.exit(-1)

def interactive_device_selection():
    """Interactively select a simulator or device and save the selection"""
    try:
        # Get all available simulators
        result = subprocess.run(
            ["xcrun", "simctl", "list", "devices", "available", "-j"],
            capture_output=True,
            text=True,
            check=True
        )
        data = json.loads(result.stdout)

        # Build list of simulators
        simulators = []
        for runtime, devices in data.get("devices", {}).items():
            if "iOS" in runtime:
                ios_version = runtime.split(".")[-1].replace("-", ".")
                for device in devices:
                    if device.get("isAvailable", False):
                        simulators.append({
                            "name": device.get("name"),
                            "udid": device.get("udid"),
                            "state": device.get("state"),
                            "ios_version": ios_version,
                            "type": "simulator"
                        })

        if not simulators:
            print("ERROR: No available simulators found")
            sys.exit(-1)

        # Display selection menu
        print("\n=== Select iOS Simulator ===\n")
        for idx, sim in enumerate(simulators, 1):
            state_indicator = "●" if sim["state"] == "Booted" else "○"
            print(f"  {idx:2}. {state_indicator} {sim['name']:30} (iOS {sim['ios_version']})")

        print()

        # Get user selection
        while True:
            try:
                choice = input("Select simulator (number): ").strip()
                choice_idx = int(choice) - 1

                if 0 <= choice_idx < len(simulators):
                    selected = simulators[choice_idx]
                    save_device_selection(
                        udid=selected["udid"],
                        name=selected["name"],
                        device_type="simulator"
                    )
                    return
                else:
                    print(f"Invalid selection. Please enter 1-{len(simulators)}")
            except (ValueError, KeyboardInterrupt):
                print("\nSelection cancelled")
                sys.exit(0)

    except (subprocess.CalledProcessError, json.JSONDecodeError) as e:
        print(f"ERROR: Could not get simulator list: {e}")
        sys.exit(-1)

def get_booted_simulator():
    """Get the UDID of the currently booted simulator"""
    try:
        result = subprocess.run(
            ["xcrun", "simctl", "list", "devices", "-j"],
            capture_output=True,
            text=True,
            check=True
        )
        data = json.loads(result.stdout)

        for runtime, devices in data.get("devices", {}).items():
            for device in devices:
                if device.get("state") == "Booted":
                    return device.get("udid")

        return None

    except (subprocess.CalledProcessError, json.JSONDecodeError):
        return None

def boot_simulator(udid):
    """Boot a simulator if it's not already booted"""
    try:
        result = subprocess.run(
            ["xcrun", "simctl", "boot", udid],
            capture_output=True,
            text=True
        )
        if result.returncode == 0 or "Unable to boot device in current state: Booted" in result.stderr:
            print(f"Simulator {udid} is now booted")
            return True
        else:
            print(f"ERROR: Could not boot simulator: {result.stderr}")
            return False
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Could not boot simulator: {e}")
        return False

######################################################################
# Build
######################################################################

def build_ios_test_app(is_simulator=True, is_debug=False):
    """Build the iOS test app"""

    print(f"\n=== Building iOS Test App ===")

    if is_simulator:
        build_dest = stage_dir / "orkid-ios-simulator"
        print("Building for iOS Simulator")
    else:
        build_dest = stage_dir / "orkid-ios"
        print("Building for iOS Device")

    build_type = "Debug" if is_debug else "Release"

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
    cmake_dir = build_dest

    build_cmd = ["cmake", "--build", str(cmake_dir), "--target", "ork_test_ios", "--config", build_type]

    result = subprocess.run(build_cmd, cwd=this_dir)
    if result.returncode != 0:
        print("ERROR: Test app build failed")
        return None

    # Find the .app bundle
    app_path = build_dest / build_type / "ork_test_ios.app"
    if not app_path.exists():
        # Try without build type subdirectory
        app_path = build_dest / "ork_test_ios.app"

    if not app_path.exists():
        # Try in ork.core subdirectory (CMake places it there)
        app_path = build_dest / "ork.core" / "ork_test_ios.app"

    if not app_path.exists():
        print(f"ERROR: Could not find ork_test_ios.app at {app_path}")
        return None

    print(f"Build complete: {app_path}")
    return app_path

######################################################################
# Launch
######################################################################

def launch_on_simulator(app_path, simulator_udid=None):
    """Install and launch the app on a simulator"""

    # Get or boot simulator
    if simulator_udid is None:
        simulator_udid = get_booted_simulator()
        if simulator_udid is None:
            print("ERROR: No booted simulator found. Please boot a simulator first or specify --simulator <UDID>")
            print("Use --list-simulators to see available simulators")
            sys.exit(-1)
    else:
        # Boot the specified simulator if needed
        boot_simulator(simulator_udid)
        time.sleep(2)  # Give simulator time to boot

    print(f"\nLaunching on simulator: {simulator_udid}")

    # Install the app
    print("Installing app...")
    result = subprocess.run(
        ["xcrun", "simctl", "install", simulator_udid, str(app_path)],
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"ERROR: Could not install app: {result.stderr}")
        sys.exit(-1)

    # Launch the app
    print("Launching app...")
    result = subprocess.run(
        ["xcrun", "simctl", "launch", simulator_udid, "com.tweakoz.orkid.test"],
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        print(f"ERROR: Could not launch app: {result.stderr}")
        sys.exit(-1)

    print("\n✓ App launched successfully!")
    print("  - Check the simulator for the running app")
    print("  - To view logs: xcrun simctl spawn {} log stream --predicate 'processImagePath contains \"ork_test_ios\"'".format(simulator_udid))

def launch_on_device(app_path):
    """Install and launch the app on a connected device"""

    print(f"\nLaunching on device...")
    print("Note: Device deployment requires proper code signing and provisioning")

    # This requires ios-deploy or similar tools
    # For now, provide instructions
    print("\nTo deploy to device, use one of these methods:")
    print("1. Use Xcode: Open the project and run on device")
    print("2. Install ios-deploy: brew install ios-deploy")
    print("   Then run: ios-deploy --bundle {}".format(app_path))
    print("3. Use xcrun devicectl (iOS 17+)")

######################################################################
# Main
######################################################################

def main():
    # Handle list commands
    if _args["list_simulators"]:
        list_simulators()
        return

    if _args["list_devices"]:
        list_devices()
        return

    # Handle device selection
    if _args["select"]:
        interactive_device_selection()
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
        saved_selection = load_device_selection()
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
        build_type = "Debug" if is_debug else "Release"
        if is_simulator:
            build_dest = stage_dir / "orkid-ios-simulator"
        else:
            build_dest = stage_dir / "orkid-ios"

        app_path = build_dest / build_type / "ork_test_ios.app"
        if not app_path.exists():
            app_path = build_dest / "ork_test_ios.app"

        if not app_path.exists():
            print("App not found, building...")
            app_path = build_ios_test_app(is_simulator=is_simulator, is_debug=is_debug)

    if app_path is None:
        print("ERROR: Build failed")
        sys.exit(-1)

    # Launch
    if is_device:
        launch_on_device(app_path)
    else:
        launch_on_simulator(app_path, target_udid)

if __name__ == "__main__":
    main()
