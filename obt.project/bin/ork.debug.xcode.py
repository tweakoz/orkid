#!/usr/bin/env python3

import os
import xml.etree.ElementTree as ET
from xml.dom import minidom
import argparse
import re
import shutil

def prettify(elem):
  """Return a pretty-printed XML string for the Element."""
  rough_string = ET.tostring(elem, 'utf-8')
  reparsed = minidom.parseString(rough_string)
  return reparsed.toprettyxml(indent="  ")

def find_executable(exec_name):
  """Find the executable in PATH or use the provided path."""
  exec_name = os.path.expandvars(exec_name)
  if exec_name.startswith(("./", "../")) or os.path.isabs(exec_name):
    full_path = os.path.abspath(exec_name)
    if os.path.exists(full_path) and os.access(full_path, os.X_OK):
      return full_path
    return None
  for path in os.environ["PATH"].split(os.pathsep):
    full_path = os.path.join(path, exec_name)
    if os.path.exists(full_path) and os.access(full_path, os.X_OK):
      return full_path
  return None

def is_python_script(executable_path):
  """Determine if the given file is a Python script."""
  try:
    with open(executable_path, 'r') as f:
      first_line = f.readline().strip()
      return first_line.startswith("#!") and "python" in first_line
  except Exception as e:
    return False

def create_xcode_structure(workspace_path, bin_path, env_vars, exec_args):
  # Remove existing workspace if it exists
  if os.path.exists(workspace_path):
    shutil.rmtree(workspace_path)

  # Create directories
  shared_data_path = os.path.join(workspace_path, "xcshareddata")
  shared_schemes_path = os.path.join(shared_data_path, "xcschemes")

  for directory in [workspace_path, shared_data_path, shared_schemes_path]:
    os.makedirs(directory, exist_ok=True)

  # Create contents.xcworkspacedata
  workspace_content = ET.Element("Workspace", version="1.0")
  workspace_file = os.path.join(workspace_path, 'contents.xcworkspacedata')
  with open(workspace_file, 'w') as f:
    f.write(prettify(workspace_content))

  # Generate .xcscheme file
  scheme_name = os.path.basename(bin_path)
  scheme_file = os.path.join(shared_schemes_path, f'{scheme_name}.xcscheme')
  scheme_content = ET.Element("Scheme", LastUpgradeVersion="1500", version="1.7")

  launch_action = ET.SubElement(scheme_content, "LaunchAction",
                                buildConfiguration="Debug",
                                selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB",
                                selectedLauncherIdentifier="Xcode.DebuggerFoundation.Launcher.LLDB",
                                launchStyle="0", useCustomWorkingDirectory="NO",
                                ignoresPersistentStateOnLaunch="NO",
                                debugDocumentVersioning="YES", debugServiceExtension="internal",
                                allowLocationSimulation="YES")
  path_runnable = ET.SubElement(launch_action, "PathRunnable", runnableDebuggingMode="0", FilePath=bin_path)

  env_vars_elem = ET.SubElement(launch_action, "EnvironmentVariables")
  for key, value in env_vars.items():
    if value:
      ET.SubElement(env_vars_elem, "EnvironmentVariable", key=key, value=value, isEnabled="YES")

  # Integrate exec_args into the scheme
  ET.SubElement(launch_action, "CommandLineArguments").extend(
    [ET.Element("CommandLineArgument", argument=arg, isEnabled="YES") for arg in exec_args]
  )

  with open(scheme_file, 'w') as f:
    f.write(prettify(scheme_content))

  os.system(f"open {workspace_path}")

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Generate Xcode workspace structure with debug scheme.")
  parser.add_argument("executable_name", help="Name of the executable or path to the script.")
  parser.add_argument("--", dest="exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
  args = parser.parse_args()

  executable_path = find_executable(args.executable_name)
  if not executable_path:
    print(f"Executable '{args.executable_name}' not found.")
    exit(1)

  # Check if the executable is a Python script
  if is_python_script(executable_path):
    python_path = find_executable("python3")
    if not python_path:
      print("python3 not found.")
      exit(1)
    exec_args = [executable_path] + (args.exec_args if args.exec_args else [])
    executable_path = python_path
  else:
    exec_args = args.exec_args if args.exec_args else []

  exec_name = re.sub(r"[^a-zA-Z0-9]", "_", args.executable_name)
  env_vars = {
    "ORKID_WORKSPACE_DIR": os.getenv("ORKID_WORKSPACE_DIR"),
    "LD_LIBRARY_PATH": os.getenv("LD_LIBRARY_PATH"),
    "PATH": os.getenv("PATH"),
    "OBT_STAGE": os.getenv("OBT_STAGE")
  }
  if "ORKID_GRAPHICS_API" in os.environ:
    env_vars["ORKID_GRAPHICS_API"] = os.getenv("ORKID_GRAPHICS_API")

  workspace_dir = os.path.join(env_vars["OBT_STAGE"], "tempdir", exec_name + ".xcworkspace")
  create_xcode_structure(workspace_dir, executable_path, env_vars, exec_args)
