#!/usr/bin/env python3

import os
import json
import argparse
import re
import shutil

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
    config = {
        "version": "0.2.0",
        "configurations": [
            {
                "name": "Debug Executable (GDB)",
                "type": "cppdbg",
                "request": "launch",
                "program": bin_path,
                "args": exec_args,
                "stopAtEntry": False,
                "cwd": "${workspaceFolder}",
                "environment": [{"name": k, "value": v} for k, v in env_vars.items()],
                "externalConsole": False,
                "MIMode": "gdb",
                "setupCommands": [
                    {"description": "Enable pretty-printing for gdb", "text": "-enable-pretty-printing", "ignoreFailures": True}
                ],
                "preLaunchTask": "",
                "miDebuggerPath": "gdb",
                "console": "integratedTerminal"
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

  executable_path = find_executable(args.executable_name)
  if not executable_path:
    print(f"Executable '{args.executable_name}' not found in $PATH.")
    exit(1)

  exec_args = args.exec_args

  if is_python_script(executable_path):
    python_path = find_executable("python3")
    if not python_path:
      print("python3 not found.")
      exit(1)
    exec_args.insert(0, executable_path)
    executable_path = python_path

  exec_name = re.sub(r"[^a-zA-Z0-9]", "_", args.executable_name)

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
  create_vscode_config(workspace_dir, executable_path, env_vars, exec_args)
