################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS Device Manager
# Generic device and simulator management utilities
################################################################

import subprocess
import json
import sys
from pathlib import Path

######################################################################
# Device Selection Persistence
######################################################################

def save_device_selection(udid, name, device_type, selection_file_path):
    """Save selected device UDID to JSON file

    Args:
        udid: Device UDID
        name: Device name (e.g., "iPhone 16 Pro")
        device_type: "simulator" or "device"
        selection_file_path: Path to JSON file for storing selection
    """
    selection_file = Path(selection_file_path)
    selection_file.parent.mkdir(parents=True, exist_ok=True)

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

def load_device_selection(selection_file_path):
    """Load saved device selection from JSON file

    Args:
        selection_file_path: Path to JSON file containing selection

    Returns:
        dict with 'udid', 'name', 'type' or None if no selection saved
    """
    selection_file = Path(selection_file_path)

    if not selection_file.exists():
        return None

    try:
        with open(selection_file, 'r') as f:
            return json.load(f)
    except (json.JSONDecodeError, IOError):
        return None

######################################################################
# Simulator Management
######################################################################

def list_simulators():
    """List available iOS simulators

    Prints formatted list of simulators to stdout.
    """
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

def get_available_simulators():
    """Get list of available simulators

    Returns:
        list of dicts with keys: name, udid, state, ios_version, type
    """
    try:
        result = subprocess.run(
            ["xcrun", "simctl", "list", "devices", "available", "-j"],
            capture_output=True,
            text=True,
            check=True
        )
        data = json.loads(result.stdout)

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
        return simulators

    except (subprocess.CalledProcessError, json.JSONDecodeError):
        return []

def get_booted_simulator():
    """Get the UDID of the currently booted simulator

    Returns:
        UDID string or None if no simulator is booted
    """
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
    """Boot a simulator

    Args:
        udid: Simulator UDID to boot

    Returns:
        True if successful, False otherwise
    """
    try:
        subprocess.run(
            ["xcrun", "simctl", "boot", udid],
            capture_output=True,
            check=True
        )
        print(f"Simulator {udid} is now booted")
        return True
    except subprocess.CalledProcessError:
        # Simulator might already be booted
        return False

######################################################################
# Device Management
######################################################################

def list_devices():
    """List connected iOS devices

    Prints formatted list of devices to stdout.
    """
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

######################################################################
# Interactive Selection
######################################################################

def interactive_device_selection(selection_file_path):
    """Interactively select a simulator or device and save the selection

    Args:
        selection_file_path: Path to JSON file for storing selection
    """
    simulators = get_available_simulators()

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
                    device_type="simulator",
                    selection_file_path=selection_file_path
                )
                return
            else:
                print(f"Invalid selection. Please enter 1-{len(simulators)}")
        except (ValueError, KeyboardInterrupt):
            print("\nSelection cancelled")
            sys.exit(0)
