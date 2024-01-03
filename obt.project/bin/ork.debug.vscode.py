#!/usr/bin/env python3

import sys, os, json, argparse, re, shutil
from obt import path 

this_dir = path.fileOfInvokingModule()
import _debug_helpers

def create_vscode_config(workspace_path, bin_path, env_vars, exec_args):
  if os.path.exists(workspace_path):
    shutil.rmtree(workspace_path)

  vscode_dir = os.path.join(workspace_path, ".vscode")
  os.makedirs(vscode_dir, exist_ok=True)

  if os.name == "posix" and os.uname().sysname == "Darwin":  # macOS
    config = {
        "version": "0.2.0",
        "configurations": [
            {
                "name": "Debug Executable",
                "type": "lldb",
                "request": "launch",
                "program": bin_path,
                "args": exec_args,
                "cwd": "${workspaceFolder}",
                "environment": [{"name": k, "value": v} for k, v in env_vars.items()],
                "externalConsole": False,
                "MIMode": "lldb",
                "setupCommands": [
                    {"description": "Enable pretty-printing for gdb", "text": "-enable-pretty-printing", "ignoreFailures": True}
                ],
                "preLaunchTask": "",
                "miDebuggerPath": "/usr/bin/lldb",
                "stopAtEntry": False,
                "console": "integratedTerminal"
            }
        ]
    }
  else:  # Assuming Linux for now
    extensions_py = path.orkid()/"obt.project"/"scripts"/"ork"/"ix_gdb_extensions.py"
    stdcxx_extensions_py = path.Path("/usr/share/gcc/python/libstdcxx/v6/printers.py")
    config = {
        "version": "0.2.0",
        "configurations": [
            {
                "name": "Debug Executable (GDB)",
                #"type": "cppdbg",
                "type": "sldb",
                "request": "launch",
                "program": bin_path,
                "args": exec_args,
                "stopAtEntry": False,
                "cwd": "${workspaceFolder}",
                "environment": [{"name": k, "value": v} for k, v in env_vars.items()],
                "externalConsole": False,
                "MIMode": "gdb",
                "setupCommands": [
                    {"description": "Enable pretty-printing for gdb", "text": "-enable-pretty-printing", "ignoreFailures": True},
                    {"description": "ork-extensions", "text": "--command "+str(extensions_py), "ignoreFailures": True},
                    {"description": "ork-extensions", "text": "--command "+str(stdcxx_extensions_py), "ignoreFailures": True}
                ],
                "preLaunchTask": "",
                "miDebuggerPath": "gdb",
                "console": "integratedTerminal",
                "stopAllThreads": True,
            }
        ]
    }

  launch_file = os.path.join(vscode_dir, "launch.json")
  with open(launch_file, "w") as f:
    json.dump(config, f, indent=2)

  if os.name == "posix" and os.uname().sysname == "Darwin":  # macOS
    os.system(f'/Applications/Visual\ Studio\ Code.app/Contents/Resources/app/bin/code {workspace_path}')
  else:
    os.system(f'code {workspace_path}')

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Generate Visual Studio Code debugging structure.")
  parser.add_argument("executable_name", help="Name of the executable (without path).")
  parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
  args = parser.parse_args()

  exe_path, exe_args, exec_name = _debug_helpers.get_exec_and_args(args)

  env_vars = {
      "ORKID_WORKSPACE_DIR": os.getenv("ORKID_WORKSPACE_DIR"),
      "LD_LIBRARY_PATH": os.getenv("LD_LIBRARY_PATH"),
      "PATH": os.getenv("PATH"),
      "PYTHONPATH": os.getenv("PYTHONPATH"),
      "OBT_STAGE": os.getenv("OBT_STAGE")
  }
  if "ORKID_GRAPHICS_API" in os.environ:
    env_vars["ORKID_GRAPHICS_API"] = os.getenv("ORKID_GRAPHICS_API")    

  workspace_dir = os.path.join(env_vars["OBT_STAGE"], "tempdir", exec_name)
  create_vscode_config(workspace_dir, exe_path, env_vars, exe_args)
