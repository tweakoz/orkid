#!/usr/bin/env python3

from obt import path, pathtools, command 
import argparse, os

parser = argparse.ArgumentParser()
parser.add_argument("--clean", action="store_true")

args = parser.parse_args()

this_dir = path.directoryOfInvokingModule()
build_dir = path.builds()/"hfs_orkrop"
if build_dir.exists() and args.clean:
    os.system("rm -rf {}".format(build_dir))

OK = True
if not build_dir.exists():
  pathtools.ensureDirectoryExists(build_dir)
  cmd_list = ["cmake", this_dir]
  rval = command.run(cmd_list, working_dir=build_dir, do_log=True)
  OK = (rval == 0)

if OK:
  rval = command.run(["make", "install"], working_dir=build_dir, do_log=True)
  OK = (rval == 0)

if OK:
  print("###################################")
  print( "running rop_test.py")
  print("###################################")
  OK = command.run(["hython","rop_test.py"], working_dir=this_dir, do_log=True)

