#!/usr/bin/env python3

from obt import path, pathtools, command 
import argparse, os

parser = argparse.ArgumentParser()
parser.add_argument("--clean", action="store_true")

args = parser.parse_args()

this_dir = path.directoryOfInvokingModule()
build_dir = path.builds()/"hfs_orkshader"
if build_dir.exists() and args.clean:
    os.system("rm -rf {}".format(build_dir))


if not build_dir.exists():
  pathtools.ensureDirectoryExists(build_dir)
  cmd_list = ["cmake", this_dir]
  command.run(cmd_list, working_dir=build_dir, do_log=True)

command.run(["make", "install"], working_dir=build_dir, do_log=True)

