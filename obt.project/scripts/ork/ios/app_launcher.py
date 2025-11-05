################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS App Launcher
# Generic app deployment and launching utilities
################################################################

import subprocess
import sys
import time
from pathlib import Path
from . import device_manager

######################################################################
# Simulator App Deployment
######################################################################

def install_app(app_bundle_path, simulator_udid):
    """Install an app on a simulator

    Args:
        app_bundle_path: Path to .app bundle
        simulator_udid: Simulator UDID

    Returns:
        True if successful, False otherwise
    """
    try:
        result = subprocess.run(
            ["xcrun", "simctl", "install", simulator_udid, str(app_bundle_path)],
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"ERROR: Could not install app: {result.stderr}")
            return False
        return True
    except subprocess.CalledProcessError as e:
        print(f"ERROR: Could not install app: {e}")
        return False

def launch_app(bundle_id, simulator_udid):
    """Launch an app on a simulator

    Args:
        bundle_id: App bundle identifier (e.g., "com.tweakoz.orkid.test")
        simulator_udid: Simulator UDID

    Returns:
        Process ID if successful, None otherwise
    """
    try:
        result = subprocess.run(
            ["xcrun", "simctl", "launch", simulator_udid, bundle_id],
            capture_output=True,
            text=True
        )
        if result.returncode != 0:
            print(f"ERROR: Could not launch app: {result.stderr}")
            return None

        # Parse PID from output (format: "com.bundle.id: 12345")
        if ":" in result.stdout:
            pid = result.stdout.strip().split(":")[-1].strip()
            return int(pid)
        return None

    except (subprocess.CalledProcessError, ValueError) as e:
        print(f"ERROR: Could not launch app: {e}")
        return None

def install_and_launch_on_simulator(app_bundle_path, bundle_id, simulator_udid=None):
    """Install and launch an app on a simulator

    Args:
        app_bundle_path: Path to .app bundle
        bundle_id: App bundle identifier
        simulator_udid: Simulator UDID (if None, uses booted simulator)

    Returns:
        Process ID if successful, None otherwise
    """
    app_path = Path(app_bundle_path)

    if not app_path.exists():
        print(f"ERROR: App bundle not found: {app_path}")
        return None

    # Get or boot simulator
    if simulator_udid is None:
        simulator_udid = device_manager.get_booted_simulator()
        if simulator_udid is None:
            print("ERROR: No booted simulator found and no UDID specified")
            print("Boot a simulator or use device_manager.boot_simulator(udid)")
            return None
    else:
        # Boot the specified simulator if needed
        device_manager.boot_simulator(simulator_udid)
        time.sleep(2)  # Give simulator time to boot

    print(f"\nInstalling and launching on simulator: {simulator_udid}")

    # Install the app
    print("Installing app...")
    if not install_app(app_path, simulator_udid):
        return None

    # Launch the app
    print("Launching app...")
    pid = launch_app(bundle_id, simulator_udid)

    if pid:
        print(f"\n✓ App launched successfully! (PID: {pid})")
        print(f"  - Check the simulator for the running app")
        print(f"  - To view logs: xcrun simctl spawn {simulator_udid} log stream --predicate 'processImagePath contains \"{Path(app_path).stem}\"'")
        return pid
    else:
        return None

######################################################################
# Device App Deployment
######################################################################

def install_and_launch_on_device(app_bundle_path, bundle_id):
    """Install and launch an app on a connected device

    Args:
        app_bundle_path: Path to .app bundle
        bundle_id: App bundle identifier

    Note:
        This requires proper code signing and provisioning.
        Consider using ios-deploy or Xcode for device deployment.
    """
    print(f"\nDeploying to device...")
    print("Note: Device deployment requires proper code signing and provisioning")

    print("\nTo deploy to device, use one of these methods:")
    print("1. Use Xcode: Open the project and run on device")
    print("2. Install ios-deploy: brew install ios-deploy")
    print(f"   Then run: ios-deploy --bundle {app_bundle_path}")
    print("3. Use xcrun devicectl (iOS 17+)")

    return None
