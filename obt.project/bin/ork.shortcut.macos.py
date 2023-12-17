#!/usr/bin/env python3
import sys, os, json, argparse, re, shutil, plistlib, datetime, uuid, subprocess
from obt import path, pathtools, command, deco, macos
from pathlib import Path

deco = deco.Deco()

# Helper Functions

def mkdir(folder, wipe=False):
    if wipe and folder.exists():
        shutil.rmtree(str(folder))
    os.makedirs(folder, exist_ok=True)

# Argument Parser Setup
parser = argparse.ArgumentParser(description="python packager")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
parser.add_argument("--bundlename", type=str, default = "Orkid", help="Bundle name for the app (CFBundleName).", required=True)
args = parser.parse_args()

# Import _debug_helpers after parsing arguments
import _debug_helpers

# Get executable path and arguments
exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)
exe_base = os.path.basename(exe_path)

is_python_script = "python" in exe_base and ".py" in exe_args[0]

print("exe_path", exe_path)
print("exe_base", exe_base)
print("exe_args", exe_args)
print("exe_name", exe_name)
print("is_python_script", is_python_script)

####################################################################################    
# Define source, temporary, and bundle directories
####################################################################################    

bundle_name = args.bundlename
orkid_src_dir = path.orkid()
temp_dir = path.temp() / "pydist"  # Temporary directory for packaging
bundle_dir = temp_dir / f"{bundle_name}.app" / "Contents"

src_icon = orkid_src_dir/"ork.tool"/"OrkidTool.app"/"Contents"/"Resources"/"orkidlogo.icns"
dst_icon = bundle_dir / "Resources" / "orkidlogo.icns"
dst_init_env = bundle_dir / 'MacOS' / 'init_env.py'

####################################################################################    
# Create the macOS bundle directory structure
####################################################################################    

mkdir(temp_dir, wipe=True)
mkdir(bundle_dir / "MacOS", wipe=True)
mkdir(bundle_dir / "Resources", wipe=True)
shutil.copy(str(src_icon), str(dst_icon))

####################################################################################    
# Generate and write init_env.py script
####################################################################################    

env_vars = [
    "OBT_STAGE",
    "ORKID_WORKSPACE_DIR",
    "PATH",
    "LD_LIBRARY_PATH",
    "PYTHONPATH",
    "PYTHONNOUSERSITE",
    "ORKID_LEV2_EXAMPLES_DIR",
    "LUA_PATH",
    "HOMEBREW_PREFIX",
]

env_var_set_str = ""

for item in env_vars:
  env_var_set_str += f"    \"{item}\": \"" + os.environ[item]+"\",\n"

init_env_script = f"""#!/usr/bin/env python3
import os, sys, subprocess, pathlib

this_path = pathlib.PosixPath(__file__)
this_dir = this_path.parent
bundle_dir = this_path.parent.parent
os.chdir(this_dir)
exe_args = {exe_args}

env_vars = {{
    {env_var_set_str}
}}

os.environ.update(env_vars)
print("#################################")
for item in env_vars.keys():
  print(item, " = ", env_vars[item])
  print()
print("#################################")

# Determine the path of the executable
int_executable_path = pathlib.Path("{exe_path}")

if not int_executable_path.exists():
  raise FileNotFoundError("Executable " + int_executable_name + " not found @ " + str(int_executable_path) + "(in bundle).")

# Execute the main application with rebased arguments
cmdlist = [str(int_executable_path)] + exe_args
print("EXEC: " + " ".join(cmdlist))
subprocess.run(cmdlist)
"""

####################################################################################    
# Create Info.plist
####################################################################################    

def format_bundle_identifier(bundle_name):
    # Sanitize the bundle_name to create a legal identifier
    sanitized_name = re.sub(r'[^a-zA-Z0-9.]+', '', bundle_name.replace(' ', ''))
    return f"com.tweakoz.orkid.{sanitized_name}"

bundle_id = format_bundle_identifier(bundle_name)

info_plist = {
    'CFBundleDisplayName': bundle_name,  # Human-readable application name
    'CFBundleExecutable': 'init_env.py',  # Executable script
    'CFBundleIdentifier': bundle_id,  # Unique bundle identifier
    'CFBundleName': bundle_name,  # Application name
    'CFBundleVersion': '1.0.0',
    'CFBundleIconFile': 'orkidlogo.icns',  # Name of your icon file
    # Additional plist keys
}

print("#################################")
print("#################################")
print(init_env_script)
print("#################################")
print(info_plist)
print("#################################")
print("#################################")

####################################################################################    
# Write init_env.py to the bundle MacOS directory
####################################################################################    

with open(bundle_dir / "Info.plist", 'wb') as plist_file:
    plistlib.dump(info_plist, plist_file)

with open(dst_init_env, 'w') as file:
    file.write(init_env_script)
dst_init_env.chmod(0o755)
