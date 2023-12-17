#!/usr/bin/env python3
import sys, os, json, argparse, re, shutil, plistlib, datetime, uuid, subprocess
from obt import path, pathtools, command, deco, macos
from pathlib import Path

deco = deco.Deco()

# Helper Functions
def copy_tree(src, dest):
    shutil.copytree(str(src), str(dest),symlinks=True)

def mkdir(folder, wipe=False):
    if wipe and folder.exists():
        shutil.rmtree(str(folder))
    os.makedirs(folder, exist_ok=True)

# Argument Parser Setup
parser = argparse.ArgumentParser(description="python packager")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
parser.add_argument("--bundlename", type=str, default = "Orkid", help="Bundle name for the app (CFBundleName).", required=True)
parser.add_argument("--rebase", action="store_true", help="Bundle name for the app (CFBundleName).")
args = parser.parse_args()

# Import _debug_helpers after parsing arguments
import _debug_helpers

do_rebase = args.rebase

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

dst_python_executable = dst_pyvenv/"bin"/"python3.11"

if is_python_script:
  exe_base = "../pyvenv/bin/python3.11"

if do_rebase:
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
    "PATH": "{orig_path}",
"""

if do_rebase:
  # OVERRIDE PYTHONPATH, PYTHONHOME
  init_env_script += f"""
    "PYTHONHOME": str(this_dir / "pyvenv"),
    "PYTHONPATH": str(this_dir / "pyvenv" / "lib" / "python3.11" / "site-packages"),
"""

init_env_script += f"""
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
"""

if do_rebase:
  init_env_script += f"""cmdlist = [str(int_executable_path)] #+ exe_args
print("EXEC: " + " ".join(cmdlist))
subprocess.run(cmdlist)
"""
else:
  init_env_script += f"""cmdlist = [str(int_executable_path)] + exe_args
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

if do_rebase:
  command.run(["mv", dst_pyvenv/"bin"/"python3.11", dst_bin/"python3.11"])
  command.run(["mv", dst_pyvenv/"lib"/"libpython3.11.dylib", dst_lib/"libpython3.11.dylib"])

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

  homebrew_refDB.references.add(src_homebrew/"opt"/"zstd"/"lib"/"libzstd.1.dylib")
  homebrew_refDB.references.add(src_homebrew/"opt"/"xz"/"lib"/"liblzma.5.dylib")
  homebrew_refDB.references.add(src_homebrew/"opt"/"jpeg-turbo"/"lib"/"libjpeg.8.dylib")
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

  #################
        
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

  ################################
  # fixup dylib paths in dylibs
  ################################

  def is_candidate(line):
    rval  = line.startswith('lib')
    rval = rval or "@rpath" in line 
    rval = rval or str(src_homebrew) in line 
    rval = rval or str(path.stage()) in line 
    rval = rval and '.dylib' in line
    #print(line,rval)
    return rval

  # List all dylib files in the directory
  dylibs = [f for f in os.listdir(dst_lib) if f.endswith('.dylib')]
  for dylib in dylibs:
    dylib_path = os.path.join(dst_lib, dylib)
    # change id of dylib
    new_id = f'@executable_path/../lib/{dylib}'
    subprocess.run(['install_name_tool', '-id', new_id, dylib_path])
    # Get the dependencies of the dylib
    output = subprocess.check_output(['otool', '-L', dylib_path]).decode()
    # Iterate over each line of the otool output
    for line in output.splitlines():
      line = line.strip()
      # Check if the line represents a non-rooted dylib reference
      if is_candidate(line):
        # Extract the original dylib name
        original_dylib_name = line.split(' ')[0]
        # Construct the new rooted path without stripping off version code
        X = original_dylib_name.split('/')
        new_dylib_path = f'@executable_path/../lib/{X[-1]}'
        # Use install_name_tool to change the reference
        subprocess.run(['install_name_tool', '-change', original_dylib_name, new_dylib_path, dylib_path])
    macos.macho_dump(dylib_path)

  ################################
  # fixup dylib paths in executables
  ################################

  # List all dylib files in the directory
  binaries = [f for f in os.listdir(dst_bin)]
  for binary in binaries:
    binary_path = os.path.join(dst_bin, binary)
    # Get the dependencies of the dylib
    output = subprocess.check_output(['otool', '-L', binary_path]).decode()
    # Iterate over each line of the otool output
    for line in output.splitlines():
      line = line.strip()
      # Check if the line represents a non-rooted dylib reference
      if is_candidate(line):
        # Extract the original dylib name
        original_dylib_name = line.split(' ')[0]
        # Construct the new rooted path without stripping off version code
        X = original_dylib_name.split('/')
        new_dylib_path = f'@executable_path/../lib/{X[-1]}'
        # Use install_name_tool to change the reference
        command.run(['install_name_tool', '-change', original_dylib_name, new_dylib_path, binary_path],do_log=True)
    macos.macho_dump(binary_path)

  ################################
  # fixup python extension modules
  ################################

  command.run(["install_name_tool","-change",
                src_pyvenv/"lib"/"libpython3.11.dylib",
                "@executable_path/../lib/libpython3.11.dylib",
                dst_pyvenv/"lib"/"python3.11"/"site-packages"/"orkengine"/"core"/"_core.so"],do_log=True)

  command.run(["install_name_tool","-change",
                src_pyvenv/"lib"/"libpython3.11.dylib",
                "@executable_path/../lib/libpython3.11.dylib",
                dst_pyvenv/"lib"/"python3.11"/"site-packages"/"orkengine"/"lev2"/"_lev2.so"],do_log=True)

  print("Dependencies packaged and RPATHs modified.")

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

################################
# write site customizer
################################

if do_rebase:
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