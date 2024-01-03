#!/usr/bin/env python3

import sys, os, json, argparse, re, shutil, json
from obt import path, command
this_dir = path.fileOfInvokingModule()
curwd = os.getcwd()
sys.path.append(str(this_dir))
import _debug_helpers

parser = argparse.ArgumentParser(description="apitrace wrapper with orkid customizations")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
args = parser.parse_args()

capture_path = path.temp()/"temp.rdc"

exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)

#apitrace = path.stage()/"bin"/"apitrace"
apitrace = path.builds()/"apitrace"/".build"/"apitrace"


cmd_list = [apitrace, "trace", "-v", exe_path] + exe_args

env = {}
#env["DYLD_FRAMEWORK_PATH"] = str(path.libs()/"wrappers")

print(cmd_list, env)
command.run(cmd_list,environment=env, do_log=True)
