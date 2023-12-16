#!/usr/bin/env python3
import sys, os, json, argparse, re, shutil, plistlib, datetime, uuid
from obt import path, command
from pathlib import Path

# Helper Functions
def copy_tree(src, dest):
    shutil.copytree(str(src), str(dest))

def mkdir(folder, wipe=False):
    if wipe and folder.exists():
        shutil.rmtree(str(folder))
    os.makedirs(folder, exist_ok=True)

# Argument Parser Setup
parser = argparse.ArgumentParser(description="python packager")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
args = parser.parse_args()

# Import _debug_helpers after parsing arguments
import _debug_helpers

# Get executable path and arguments
exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)

# Define source, temporary, and bundle directories
orkid_src_dir = path.orkid()
temp_dir = path.temp() / "pydist"  # Temporary directory for packaging
#bundle_dir = temp_dir / f"{args.executable_name}.app" / "Contents"
bundle_dir = temp_dir / "Orkid.app" / "Contents"

# Create the macOS bundle structure
mkdir(temp_dir, wipe=True)
mkdir(bundle_dir / "MacOS", wipe=True)
mkdir(bundle_dir / "Resources", wipe=True)

deployment_id = datetime.datetime.now().strftime("%Y%m%d%H%M%S") + '_' + str(uuid.uuid4())

init_env_script = """#!/usr/bin/env python3
import os, sys, subprocess, pathlib

bundle_dir = pathlib.PosixPath(__file__).parent.parent
app_support_dir = pathlib.Path.home() / 'Library' / 'Application Support' / 'Orkid'
staging_dir = app_support_dir / '""" + deployment_id + """'
os.makedirs(staging_dir, exist_ok=True)

env_vars = {
    "OBT_STAGE": str(staging_dir),
    "ORKID_WORKSPACE_DIR": str(bundle_dir / "Resources"),
    "PATH": str(bundle_dir / "MacOS" / "bin"),
    "LD_LIBRARY_PATH": str(bundle_dir / "MacOS" / "lib"),
    "PYTHONHOME": str(bundle_dir / "MacOS" / "pyvenv"),
    # Additional environment variables
}

os.environ.update(env_vars)

executable_path = bundle_dir / "MacOS" / "bin" / "ork.test.lev2.ui.layout1.exe"

subprocess.run([str(executable_path)] + sys.argv[1:])
"""


# Create Info.plist
info_plist = {
    'CFBundleDisplayName': 'Orkid',  # Human-readable application name
    'CFBundleExecutable': 'init_env.py',  # Executable script
    'CFBundleIdentifier': 'com.example.orkid',  # Unique bundle identifier
    'CFBundleName': 'Orkid',  # Application name
    'CFBundleVersion': '1.0.0',
    # Additional plist keys
}

with open(bundle_dir / "Info.plist", 'wb') as plist_file:
    plistlib.dump(info_plist, plist_file)

# Write init_env.py to the bundle MacOS directory
init_env_path = bundle_dir / 'MacOS' / 'init_env.py'
with open(init_env_path, 'w') as file:
    file.write(init_env_script)
init_env_path.chmod(0o755)

# Copy data directories to bundle directories
copy_tree(orkid_src_dir / "ork.data", bundle_dir / "Resources" / "ork.data")
copy_tree(path.bin(), bundle_dir / "MacOS" / "bin")
copy_tree(path.libs(), bundle_dir / "MacOS" / "lib")
copy_tree(path.stage() / "pyvenv", bundle_dir / "MacOS" / "pyvenv")
