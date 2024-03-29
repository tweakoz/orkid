#!/usr/bin/env python3

import sys, os, json, argparse, re, shutil
from obt import path, command
this_dir = path.fileOfInvokingModule()
sys.path.append(str(this_dir))
import _debug_helpers

parser = argparse.ArgumentParser(description="GDB wrapper with orkid customizations")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
args = parser.parse_args()

extensions_py = path.orkid()/"obt.project"/"scripts"/"ork"/"ix_gdb_extensions.py"
stdcxx_extensions_py = path.Path("/usr/share/gcc/python/libstdcxx/v6/printers.py")

exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)


cmd_list = ["gdb",
            "--command=%s"%str(extensions_py),
            "--command=%s"%str(stdcxx_extensions_py),
            "--args",
            exe_path
           ]

cmd_list += exe_args
print(cmd_list)
command.run(cmd_list,do_log=True)
