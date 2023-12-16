#!/usr/bin/env python3
import sys, os, json, argparse, re, shutil, plistlib, datetime, uuid, subprocess
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
#bundle_dir = temp_dir / "Orkid.app" / "Contents"
bundle_dir = temp_dir / f"{bundle_name}.app" / "Contents"

icon_src = orkid_src_dir/"ork.tool"/"OrkidTool.app"/"Contents"/"Resources"/"orkidlogo.icns"
icon_dest = bundle_dir / "Resources" / "orkidlogo.icns"

####################################################################################    
# Create the macOS bundle directory structure
####################################################################################    

mkdir(temp_dir, wipe=True)
mkdir(bundle_dir / "MacOS", wipe=True)
mkdir(bundle_dir / "Resources", wipe=True)
shutil.copy(str(icon_src), str(icon_dest))

deployment_id = datetime.datetime.now().strftime("%Y%m%d%H%M%S") + '_' + str(uuid.uuid4())

executable_name = os.path.basename(exe_path)

####################################################################################    
# find homebrew dylibs and copy them to the bundle
####################################################################################    


def get_dylib_dependencies(file_path, seen=None):
  """Recursively get a list of dynamic library dependencies for the file."""
  if seen is None:
    seen = set()
  if not os.path.exists(file_path) or not os.path.isfile(file_path):
    return seen
  deps = subprocess.check_output(['otool', '-L', file_path]).decode()
  for line in deps.splitlines()[1:]:
    dylib = line.split()[0]
    if dylib not in seen:
      seen.add(dylib)
      get_dylib_dependencies(dylib, seen)
  return seen

homebrew_dir = Path("/opt/homebrew")
def homebrew_dylib_dependencies(raw_deps):
  rval = []
  for item in raw_deps:
    if str(homebrew_dir) in item:
      rval.append(item)
  return rval

staging_pyvenv = path.stage() / "pyvenv"
def staging_dylib_dependencies(raw_deps):
  rval = []
  for item in raw_deps:
    if str(staging_pyvenv) in item:
      rval.append(item)
  return rval

def copy_dylibs_to_folder(dylibs, dest_folder):
  """Copy dynamic libraries to the destination folder."""
  os.makedirs(dest_folder, exist_ok=True)
  for dylib in dylibs:
    shutil.copy(dylib, dest_folder)

def change_rpath_homebrew(file_path, dest_folder):
  """Change the RPATH of the file for each of its dependencies."""
  deps = subprocess.check_output(['otool', '-L', file_path]).decode()
  for line in deps.splitlines()[1:]:
    old_path = line.split()[0]
    new_path = ""
    if str(homebrew_dir) in line:
      new_path = os.path.join(dest_folder, os.path.basename(old_path))
      print(f"Changing RPATH from {old_path} to {new_path}")

def change_rpath_staging(binary_path):
  output = subprocess.check_output(['otool', '-L', binary_path]).decode()
  for line in output.splitlines():
    if str(staging_pyvenv) in line:
      lib_path = line.split()[0]
      lib_name = os.path.basename(lib_path)
      #subprocess.run(['install_name_tool', '-change', lib_path, lib_name, binary_path])
      print(f"Updated {lib_path} to {lib_name} in {binary_path}")

dest_folder = bundle_dir/"MacOS"/"homebrew"

# Step 1: Recursively identify all dependencies
all_dependencies = get_dylib_dependencies(exe_path)
hb_deps = homebrew_dylib_dependencies(all_dependencies)
staging_deps = staging_dylib_dependencies(all_dependencies)
print(all_dependencies)
print(hb_deps)
print(staging_deps)

# Step 2: Copy all dependencies to a folder
#copy_dylibs_to_folder(all_dependencies, dest_folder)

# Step 3: Modify the RPATH of the executable and all its dependencies
change_rpath_homebrew(exe_path, dest_folder)
change_rpath_staging(exe_path)
for dep in hb_deps:
  change_rpath(dep, dest_folder)
for dep in staging_deps:
  change_rpath_staging(dep)

print("Dependencies packaged and RPATHs modified.")

assert(False)

####################################################################################    
# Generate and write init_env.py script
####################################################################################    

#########
# head
#########

orig_path = os.environ["OBT_ORIGINAL_PATH"]
orig_pypath = os.environ["OBT_ORIGINAL_PYTHONPATH"]

init_env_script = f"""#!/usr/bin/env python3
import os, sys, subprocess, pathlib

bundle_dir = pathlib.PosixPath(__file__).parent.parent
app_support_dir = pathlib.Path.home() / 'Library' / 'Application Support' / 'Orkid'
staging_dir = app_support_dir / '{deployment_id}'
os.makedirs(staging_dir, exist_ok=True)
exe_args = {exe_args}

env_vars = {{
    "OBT_STAGE": str(staging_dir),
    "ORKID_WORKSPACE_DIR": str(bundle_dir / "Resources"),
    "PATH": str(bundle_dir / "MacOS" / "bin"),
    "LD_LIBRARY_PATH": str(bundle_dir / "MacOS" / "lib"),
    "PYTHONHOME": str(bundle_dir / "MacOS" / "pyvenv"),
    "PYTHONPATH": "{orig_pypath}",
    "PATH": "{orig_path}",
}}

os.environ.update(env_vars)

int_executable_name = '""" + exe_base + """'

"""

#########
# python script handling...
#########

if is_python_script:     
  first_arg = exe_args[0]
  init_env_script += f"""
# Rebase the path relative to bundle_dir / "Resources"
rebased_path = bundle_dir / "Resources" / pathlib.Path('{first_arg}')
exe_args[0] = str(rebased_path)
# Determine the path of the executable
int_executable_path = bundle_dir / "MacOS" / "pyvenv" / "bin" / int_executable_name
"""
else:
  init_env_script += f"""
int_executable_path = bundle_dir / "MacOS" / "bin" / int_executable_name
"""
  
#########
# tail
#########

init_env_script += f"""

if not int_executable_path.exists():
  raise FileNotFoundError("Executable " + int_executable_name + " not found @ " + str(int_executable_path) + "(in bundle).")

# Execute the main application with rebased arguments
subprocess.run([str(int_executable_path)] + exe_args)
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
print("#################################")
print("#################################")
print(init_env_script)
print("#################################")
print(info_plist)
print("#################################")

#assert(False)

with open(bundle_dir / "Info.plist", 'wb') as plist_file:
    plistlib.dump(info_plist, plist_file)

####################################################################################    
# Write init_env.py to the bundle MacOS directory
####################################################################################    

init_env_path = bundle_dir / 'MacOS' / 'init_env.py'
with open(init_env_path, 'w') as file:
    file.write(init_env_script)
init_env_path.chmod(0o755)

####################################################################################    
# Copy data directories to bundle directories
####################################################################################    

copy_tree(orkid_src_dir / "ork.data", bundle_dir / "Resources" / "ork.data")
copy_tree(path.bin(), bundle_dir / "MacOS" / "bin")
copy_tree(path.libs(), bundle_dir / "MacOS" / "lib")
copy_tree(path.stage() / "pyvenv", bundle_dir / "MacOS" / "pyvenv")
copy_tree(orkid_src_dir / "ork.core" / "examples", bundle_dir / "Resources" / "ork.core" / "examples")
copy_tree(orkid_src_dir / "ork.core" / "pyext" / "tests", bundle_dir / "Resources" /  "ork.core" / "pyext" / "tests")
copy_tree(orkid_src_dir / "ork.lev2" / "examples", bundle_dir / "Resources" / "ork.lev2" / "examples")
copy_tree(orkid_src_dir / "ork.lev2" / "pyext" / "tests", bundle_dir / "Resources" /  "ork.lev2" / "pyext" / "tests")
copy_tree(orkid_src_dir / "ork.ecs" / "examples", bundle_dir / "Resources" / "ork.ecs" / "examples")
#copy_tree(orkid_src_dir / "ork.ecs" / "pyext" / "tests", bundle_dir / "Resources" /  "ork.ecs" / "pyext" / "tests")

####################################################################################    
# Remove __pycache__ directories
# Remove test directories
# Remove .dist-info directories
####################################################################################    

os.chdir(bundle_dir / "MacOS" / "pyvenv")
os.system( "find . -type d -name '__pycache__' -exec rm -rf {} +")
os.system( "find . -type d -name 'tests' -exec rm -rf {} +")
os.system( "find . -type d -name 'test' -exec rm -rf {} +")
os.system( "find . -type d -name '*.dist-info' -exec rm -rf {} +")
