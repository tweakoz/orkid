#!/usr/bin/env python3

import sys, os, shutil
import xml.etree.ElementTree as ET
from xml.dom import minidom

from obt import path, command

this_dir = path.fileOfInvokingModule()
sys.path.append(str(this_dir))

import _debug_helpers

cur_wd = os.getcwd()

###############################################################################

def prettify(elem):
    """Return a pretty-printed XML string for the Element."""
    rough_string = ET.tostring(elem, 'utf-8')
    reparsed = minidom.parseString(rough_string)
    return reparsed.toprettyxml(indent="  ")

###############################################################################

def create_pycharm_run_config(project_path, bin_path, env_vars, exec_args, working_dir=None):
    # Temporary directory for PyCharm run config
    idea_dir = os.path.join(project_path, ".idea")
    if os.path.exists(idea_dir):
        shutil.rmtree(idea_dir)

    # Create .idea directory
    os.makedirs(idea_dir, exist_ok=True)

    # Create workspace.xml with modern configuration
    workspace_content = ET.Element("project", version="4")
    workspace_component = ET.SubElement(workspace_content, "component", name="RunManager")

    # Add Python run configuration
    configuration = ET.SubElement(workspace_component, "configuration",
                                  default="false",
                                  name=os.path.basename(bin_path),
                                  type="PythonConfigurationType",
                                  factoryName="Python",
                                  nameIsGenerated="true",
                                  temporary="false")

    # Working directory
    option = ET.SubElement(configuration, "option", name="working_directory")
    option.set("value", working_dir if working_dir else "$PROJECT_DIR$")

    # Use the explicit Python executable or venv Python interpreter
    python_exec = os.path.join(env_vars["OBT_PYTHONHOME"], "bin", "python")
    if os.path.exists(python_exec):
        option = ET.SubElement(configuration, "option", name="INTERPRETER_PATH")
        option.set("value", python_exec)
    else:
        option = ET.SubElement(configuration, "option", name="INTERPRETER_PATH")
        option.set("value", os.path.join(env_vars["OBT_VENV_DIR"], "bin", "python"))

    # Script name (the main Python file to run)
    option = ET.SubElement(configuration, "option", name="scriptName")
    option.set("value", bin_path)

    # Command line arguments
    for arg in exec_args:
        ET.SubElement(configuration, "option", name="PARAMETERS", value=arg)

    # Environment variables
    envs_element = ET.SubElement(configuration, "envs")
    for key, value in env_vars.items():
        if value:
            ET.SubElement(envs_element, "env", name=key, value=value)

    workspace_file = os.path.join(idea_dir, 'workspace.xml')
    with open(workspace_file, 'w') as f:
        f.write(prettify(workspace_content))

    return project_path

###############################################################################

if __name__ == "__main__":
    ##################################
    class MyArgParse:
        def __init__(self):
            self.executable_name = sys.argv[1]
            if len(sys.argv) > 2:
                self.exec_args = sys.argv[2:]
            else:
                self.exec_args = []
    ##################################
    args = MyArgParse()
    exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)
    ##################################
    env_vars = {
        "ORKID_WORKSPACE_DIR": os.getenv("ORKID_WORKSPACE_DIR"),
        "LD_LIBRARY_PATH": os.getenv("LD_LIBRARY_PATH"),
        "PATH": os.getenv("PATH"),
        "PYTHONPATH": os.getenv("PYTHONPATH"),
        "OBT_STAGE": os.getenv("OBT_STAGE"),
        "OBT_PYTHONHOME": os.getenv("OBT_PYTHONHOME"),
        "OBT_VENV_DIR": os.getenv("OBT_VENV_DIR")
    }
    if "ORKID_GRAPHICS_API" in os.environ:
        env_vars["ORKID_GRAPHICS_API"] = os.getenv("ORKID_GRAPHICS_API")
    ##################################
    # Define the project directory to store .idea
    project_dir = os.path.join(env_vars["OBT_STAGE"], "tempdir", exe_name + "_pycharm")
    idea_dir = create_pycharm_run_config(project_dir, exe_path, env_vars, exe_args, cur_wd)
    print(f"PyCharm run configuration created at: {idea_dir}")
    ##################################
    # Launch PyCharm with the run configuration
    ##################################
    cmd_list = ["/Applications/PyCharm.app/Contents/MacOS/pycharm", project_dir]
    command.run(cmd_list, do_log=True)
