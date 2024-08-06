#!/usr/bin/env python3

import sys, os, json, argparse, re, shutil
import xml.etree.ElementTree as ET
from xml.dom import minidom

from obt import path 

this_dir = path.fileOfInvokingModule()
sys.path.append(str(this_dir))

import _debug_helpers

cur_wd = os.getcwd()

def prettify(elem):
  """Return a pretty-printed XML string for the Element."""
  rough_string = ET.tostring(elem, 'utf-8')
  reparsed = minidom.parseString(rough_string)
  return reparsed.toprettyxml(indent="  ")

def create_xcode_structure(workspace_path, bin_path, env_vars, exec_args, working_dir=None):
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
                                launchStyle="0", 
                                useCustomWorkingDirectory="YES" if (working_dir!=None) else "NO",
                                ignoresPersistentStateOnLaunch="NO",
                                debugDocumentVersioning="YES", debugServiceExtension="internal",
                                allowLocationSimulation="YES")

  if working_dir!=None:
    launch_action.set('customWorkingDirectory', working_dir)

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
  #parser = argparse.ArgumentParser(description="Generate Xcode workspace structure with debug scheme.")
  #parser.add_argument("executable_name", help="Name of the executable or path to the script.")
  #parser.add_argument("rest", nargs=argparse.REMAINDER, help="Arguments for the executable.")
  class MyArgParse:
    def __init__(self):
      self.executable_name = sys.argv[1]
      if len(sys.argv) > 2:
        self.exec_args = sys.argv[2:]
      else:
        self.exec_args = []
  args = MyArgParse()
  print(args)
  exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)

  env_vars = {
    "ORKID_WORKSPACE_DIR": os.getenv("ORKID_WORKSPACE_DIR"),
    "LD_LIBRARY_PATH": os.getenv("LD_LIBRARY_PATH"),
    "PATH": os.getenv("PATH"),
    "PYTHONPATH": os.getenv("PYTHONPATH"),
    "OBT_STAGE": os.getenv("OBT_STAGE")
  }
  if "ORKID_GRAPHICS_API" in os.environ:
    env_vars["ORKID_GRAPHICS_API"] = os.getenv("ORKID_GRAPHICS_API")

  workspace_dir = os.path.join(env_vars["OBT_STAGE"], "tempdir", exe_name + ".xcworkspace")
  create_xcode_structure(workspace_dir, exe_path, env_vars, exe_args, cur_wd)
