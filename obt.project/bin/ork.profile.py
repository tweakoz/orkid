#!/usr/bin/env python3

import argparse
from obt import command, path

this_dir = path.fileOfInvokingModule()
import _debug_helpers

parser = argparse.ArgumentParser(description="Generate Visual Studio Code debugging structure.")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
args = parser.parse_args()

exe_path, exe_args, exec_name = _debug_helpers.get_exec_and_args(args)

print(exe_path)
print(exe_args)
print(exec_name)

cmdlist = [
  "samply",
  "record",
]

cmdlist += [exe_path]
cmdlist += exe_args

print(cmdlist)

command.run(cmdlist)