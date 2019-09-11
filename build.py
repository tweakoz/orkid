#!/usr/bin/env python3

import os, argparse
import ork.host
from ork.path import Path
from ork.command import Command

parser = argparse.ArgumentParser(description='orkid build')
parser.add_argument('--clean', action="store_true", help='force clean build' )

_args = vars(parser.parse_args())

print(ork.host.SYSTEM)

build_dest = ork.path.stage()/"orkid"
build_dest.mkdir(parents=True,exist_ok=True)
build_dest.chdir()

prj_root = Path(os.environ["ORKID_WORKSPACE_DIR"])
cmd = ["cmake",prj_root]

Command(cmd).exec()
if _args["clean"]!=False:
    Command(["make","clean"]).exec()


Command(["make","-j",ork.host.NumCores]).exec()
