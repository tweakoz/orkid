#!/usr/bin/env python3
"""
In-place macOS app bundle creator for development builds.
This is a thin wrapper around the ork.osxdeploy module.
"""

import argparse
from ork import osxdeploy

# Argument Parser Setup
parser = argparse.ArgumentParser(description="In-place macOS app bundle creator for development builds")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
parser.add_argument("--bundlename", type=str, default="OrkidDev", help="Bundle name for the app (CFBundleName).", required=True)
args = parser.parse_args()

# Create the bundle using the module
desktop_path = osxdeploy.create_bundle(
    executable_name=args.executable_name,
    exec_args=args.exec_args,
    bundle_name=args.bundlename,
    additional_env={},  # No additional environment for base Orkid
    additional_capture_vars=[]  # No additional variables to capture
)