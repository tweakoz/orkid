#!/usr/bin/env python3
import sys, os, json, argparse, re, shutil, plistlib, datetime, uuid, subprocess
from obt import path, pathtools, command, deco, macos
from pathlib import Path

deco = deco.Deco()

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
bundle_dir = temp_dir / f"{bundle_name}.app" / "Contents"

src_icon = orkid_src_dir/"ork.tool"/"OrkidTool.app"/"Contents"/"Resources"/"orkidlogo.icns"
dst_icon = bundle_dir / "Resources" / "orkidlogo.icns"

dst_init_env = bundle_dir / 'MacOS' / 'init_env.py'
dst_bin = bundle_dir / "MacOS" / "bin"
dst_lib = bundle_dir / "MacOS" / "lib"
src_homebrew = Path("/opt/homebrew")
dst_homebrew = bundle_dir/"MacOS"/"homebrew"
src_pyvenv = path.stage() / "pyvenv"
dst_pyvenv = bundle_dir / "MacOS" / "pyvenv"
dst_python_executable = dst_bin/"python3.11"
dst_executable = dst_bin / exe_base

####################################################################################    
# Create the macOS bundle directory structure
####################################################################################    

mkdir(temp_dir, wipe=True)
mkdir(bundle_dir / "MacOS", wipe=True)
mkdir(bundle_dir / "Resources", wipe=True)
shutil.copy(str(src_icon), str(dst_icon))

deployment_id = datetime.datetime.now().strftime("%Y%m%d%H%M%S") + '_' + str(uuid.uuid4())

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

this_path = pathlib.PosixPath(__file__)
this_dir = this_path.parent
bundle_dir = this_path.parent.parent
os.chdir(this_dir)
app_support_dir = pathlib.Path.home() / 'Library' / 'Application Support' / 'Orkid'
staging_dir = app_support_dir / '{deployment_id}'
os.makedirs(staging_dir, exist_ok=True)
exe_args = {exe_args}

env_vars = {{
    "OBT_STAGE": str(staging_dir),
    "ORKID_WORKSPACE_DIR": str(bundle_dir / "Resources"),
    "PATH": str(this_dir / "bin"),
    "LD_LIBRARY_PATH": str(this_dir / "lib"),
    "DYLD_LIBRARY_PATH": str(this_dir / "lib"),
    "PYTHONHOME": str(this_dir / "pyvenv"),
    "PYTHONPATH": str(this_dir / "pyvenv" / "lib" / "python3.11" / "site-packages"),
    "PATH": "{orig_path}",
}}

os.environ.update(env_vars)
for item in env_vars.keys():
  print(item, " = ", env_vars[item])

int_executable_name = '""" + exe_base + """'
print(int_executable_name)
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
"""
  
#########
# tail
#########

init_env_script += f"""

# Determine the path of the executable
int_executable_path = bundle_dir / "MacOS" / "bin" / int_executable_name

if not int_executable_path.exists():
  raise FileNotFoundError("Executable " + int_executable_name + " not found @ " + str(int_executable_path) + "(in bundle).")

# Execute the main application with rebased arguments
cmdlist = [str(int_executable_path)] #+ exe_args
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

with open(dst_init_env, 'w') as file:
    file.write(init_env_script)
dst_init_env.chmod(0o755)

####################################################################################    
# Copy data directories to bundle directories
####################################################################################    

print( "Copying build products ...." )

copy_tree(orkid_src_dir / "ork.data", bundle_dir / "Resources" / "ork.data")
copy_tree(path.bin(), dst_bin)
copy_tree(path.libs(), dst_lib)

print( "Copying python environment ...." )
copy_tree(src_pyvenv, dst_pyvenv)

print( "Copying examples and tests ...." )

copy_tree(orkid_src_dir / "ork.core" / "examples", bundle_dir / "Resources" / "ork.core" / "examples")
copy_tree(orkid_src_dir / "ork.core" / "pyext" / "tests", bundle_dir / "Resources" /  "ork.core" / "pyext" / "tests")
copy_tree(orkid_src_dir / "ork.lev2" / "examples", bundle_dir / "Resources" / "ork.lev2" / "examples")
copy_tree(orkid_src_dir / "ork.lev2" / "pyext" / "tests", bundle_dir / "Resources" /  "ork.lev2" / "pyext" / "tests")
copy_tree(orkid_src_dir / "ork.ecs" / "examples", bundle_dir / "Resources" / "ork.ecs" / "examples")

command.run(["mv", dst_pyvenv/"bin"/"python3.11", dst_bin/"python3.11"])
command.run(["mv", dst_pyvenv/"lib"/"libpython3.11.dylib", dst_lib/"libpython3.11.dylib"])

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

####################################################################################    
# fixup RPATHS on copied binaries
####################################################################################    

def homebrew_dylib_dependencies(raw_deps):
  rval = []
  for item in raw_deps:
    if str(src_homebrew) in item:
      rval.append(item)
  return rval

def staging_dylib_dependencies(raw_deps):
  rval = []
  for item in raw_deps:
    if str(path.stage()) in item:
      rval.append(item)
  return rval

####################################################################################    


def add_rpaths(binary_path, rpaths):
  """Add the RPATHs to the binary."""
  for item in rpaths:
    cmdlist = ['install_name_tool', '-add_rpath', item, binary_path]
    command.run(cmdlist,do_log=True)

####################################################################################    

class MachoReplaceItem(object):
  def __init__(self, str_search, str_replace):
    self.str_search = str(str_search)
    self.str_replace = str(str_replace)
  def replaceOnMachoBinary(self, mach_o_path):
    macos.macho_replace_loadpaths(mach_o_path,self.str_search,self.str_replace)

####################################################################################    

print( "Collecting homebrew references" )

homebrew_refDB = macos.DylibReferenceDatabase()
homebrew_refDB.probe_in(src_homebrew,pathtools.patglob(bundle_dir/"MacOS"/"lib","*.dylib"))
homebrew_refDB.probe_in(src_homebrew,pathtools.patglob(bundle_dir/"MacOS"/"pyvenv"/"lib","*.dylib"))

print("HOMEBREW REFERENCERS")
homebrew_refDB.dump_referencers()
print("HOMEBREW REFERENCES")
homebrew_refDB.dump_references()

print( "Collecting homebrew self-references" )

homebrew_self_references = set()
homebrew_references_remapped = set()
for item in homebrew_refDB.references:
  deps = macos.macho_enumerate_dylibs(item)
  dest = bundle_dir/"MacOS"/"lib"/os.path.basename(item)
  print("item:" , item)
  print("  dest: ", dest)
  shutil.copy(item, dest)
  homebrew_references_remapped.add(dest)
  for dep in deps:
    if str(src_homebrew) in dep:
      homebrew_self_references.add(dep)
#print("HOMEBREW SELF REFERENCES")
#for item in homebrew_self_references:
#  print(item)

for item in homebrew_references_remapped:
  dylib_paths = macos.macho_enumerate_dylibs(item)
  for inpitem in dylib_paths:
    if str(src_homebrew) in str(inpitem):
      p = "@executable_path/../"+os.path.basename(inpitem)
      print(p, inpitem)
      command.run(["install_name_tool","-change",inpitem,p,item],do_log=True)
      macos.macho_dump(item)

# add homebrew self references to homebrew_references
#assert(False)
# rebase all deps to bundle relative

all_scan = macos.macho_get_all_dylib_dependencies(exe_path)

def remap_to_bundle(paths):
  staging_dest_folder = bundle_dir/"MacOS"
  rval = []
  pyvlib = path.stage()/"pyvenv"/"lib"
  for item in paths:
    if str(src_homebrew) in str(item):
      item = item.replace(str(src_homebrew), str(dst_homebrew))
    elif str(pyvlib) in item:
      item = item.replace(str(pyvlib), str(staging_dest_folder/"lib"))
    elif str(path.stage()) in item:
      item = item.replace(str(path.stage()), str(staging_dest_folder))
    rval.append(item)
  return rval


bundle_remapped = remap_to_bundle(all_scan)

hb_deps = homebrew_dylib_dependencies(bundle_remapped)
staging_deps = staging_dylib_dependencies(bundle_remapped)
print("all_scan: ", all_scan)
print("bundle_remapped: ", bundle_remapped)
print("hb_deps: ", hb_deps)
print("staging_deps: ", staging_deps)
#assert(False)

pymod_base = dst_pyvenv/"lib"/"python3.11"/"site-packages"/"orkengine"
pymods = macos.enumerateOrkPyMods(pymod_base)
print(pymod_base)
print(pymods)

macho_replacements = []
macho_replacements.append(MachoReplaceItem("@rpath","@executable_path/../lib"))
macho_replacements.append(MachoReplaceItem(path.libs(),"@executable_path/../lib"))
macho_replacements.append(MachoReplaceItem("/opt/homebrew/opt/mpfr/lib","@executable_path/../lib"))
macho_replacements.append(MachoReplaceItem("/opt/homebrew/opt/gmp/lib","@executable_path/../lib"))

for item in pymods:
  for repl in macho_replacements:
    repl.replaceOnMachoBinary(item)
  macos.macho_dump(item)

############

def change_rpaths_staging(binary_path):
  if str(src_homebrew) in str(binary_path):
    return
  output = subprocess.check_output(['otool', '-L', binary_path]).decode()
  print(deco.rgbstr(255,255,192,output))
  deco_binpath = deco.rgbstr(128,255,255,binary_path)
  print(f"RPATH: {deco_binpath}")
  splitlines = output.splitlines()
  num_lines = len(splitlines)
  check_children = []
  for line_index in range(1,num_lines):
    line = splitlines[line_index]
    is_install_name = (line_index==1)
    is_reference = (line_index>1)
    if is_install_name and (str(path.stage()) in line):
      lib_path = line.split()[0]
      lib_name = "@executable_path/../lib/"+os.path.basename(lib_path)
      deco_libpath = deco.key(lib_path)
      deco_libname = deco.orange(lib_name)
      print("CH.INST.NAME")
      print(f"     : {deco_libpath}")
      print(f"    -> {deco_libname}")
      cmdlist = ['install_name_tool', '-change', lib_path, lib_name, binary_path]
      command.run(cmdlist,do_log=True)
    elif is_reference and (str(path.stage()) in line):
      lib_path = line.split()[0]
      lib_name = "@executable_path/../lib/"+os.path.basename(lib_path)
      deco_libpath = deco.key(lib_path)
      deco_libname = deco.orange(lib_name)
      print("CH.REF.NAME<ST>")
      print(f"     : {deco_libpath}")
      print(f"    -> {deco_libname}")
      cmdlist = ['install_name_tool', '-change', lib_path, lib_name, binary_path]
      command.run(cmdlist,do_log=True)
      check_children.append(lib_path)
    elif is_reference and (str(src_homebrew) in line):
      lib_path = line.split()[0]
      lib_name = "@executable_path/../lib/"+os.path.basename(lib_path)
      deco_libpath = deco.key(lib_path)
      deco_libname = deco.orange(lib_name)
      print("CH.REF.NAME<HB>")
      print(f"     : {deco_libpath}")
      print(f"    -> {deco_libname}")
      cmdlist = ['install_name_tool', '-change', lib_path, lib_name, binary_path]
      command.run(cmdlist,do_log=True)
      check_children.append(lib_path)
  for item in check_children:
    change_rpaths_staging(item)

change_rpaths_staging(dst_executable)
for dep in hb_deps:
  change_rpath(dep, dst_homebrew)
for dep in staging_deps:
  change_rpaths_staging(dep)
#for dep in homebrew_referencers:
#  change_rpaths_staging(dep)


os.system("install_name_tool -change libpython3.11.dylib @executable_path/../lib/libpython3.11.dylib " + str(dst_python_executable))
os.system("install_name_tool -add_rpath . " + str(dst_python_executable))

print("Dependencies packaged and RPATHs modified.")

################################
# write site customizer
################################

print("Customizing Python to Bundle....")

A = """import sys
import os

# Get PYTHONHOME environment variable
python_home = os.environ.get('PYTHONHOME', '')

# Keep only paths that start with PYTHONHOME in sys.path
sys.path = [p for p in sys.path if p.startswith(python_home)]
"""
with open(dst_pyvenv/"lib"/"python3.11"/"site-packages"/"sitecustomize.py","w") as file:
  file.write(A)

os.environ["PYTHONHOME"] = str(dst_pyvenv)
os.environ["PYTHONPATH"] = str(dst_pyvenv/"lib"/"python3.11"/"site-packages")
# Path to the bundled Python executable
python_executable = str(bundle_dir / "MacOS" / "bin" / "python3.11")

# Install ork.build package into the bundled Python environment
subprocess.run([python_executable, "-m", "pip", "install", "ork.build"])