#!/usr/bin/env python3

import sys
import os, argparse
import ork.host
import ork.dep
from ork.path import Path
from ork.command import Command

parser = argparse.ArgumentParser(description='orkid build')
parser.add_argument('--clean', action="store_true", help='force clean build' )
parser.add_argument('--verbose', action="store_true", help='verbose build' )
parser.add_argument('--serial',action="store_true", help="non-parallel-build")
parser.add_argument('--debug',action="store_true", help=" debug build")
parser.add_argument('--xcode',action="store_true", help=" xcode debug build")
parser.add_argument('--ez',action="store_true", help=" ez build (use workarounds)")

_args = vars(parser.parse_args())

build_dest = ork.path.stage()/"orkid"
debug = _args["debug"]!=False

if _args["xcode"]!=False:
    build_dest = ork.path.stage()/"orkid-xcode"

build_dest.mkdir(parents=True,exist_ok=True)
build_dest.chdir()

prj_root = Path(os.environ["ORKID_WORKSPACE_DIR"])
cmd = ["cmake"]

if _args["ez"]!=False:
    stage_dir = Path(os.path.abspath(str(ork.path.stage())))
    ork_root = stage_dir/".."
    this_script = ork_root/"build.py"
    init_env_script = ork_root/"ork.build"/"bin"/"init_env.py"
    print(this_script)
    print(init_env_script)
    init_env = [init_env_script,"--launch",stage_dir]
    ch_ork_root = ["--chdir",ork_root]
    ch_tuio = ["--chdir",stage_dir/"orkid"/"ork.tuio"]
    ################
    # workarounds
    ################
    Command(init_env+ch_ork_root+["--command","obt.dep.build.py qt5"]).exec()
    Command(init_env+ch_ork_root+["--command","./build.py --debug"]).exec()
    Command(init_env+ch_tuio+["--command","make install"]).exec()
    Command(init_env+ch_ork_root+["--command","./build.py --debug"]).exec()
    sys.exit(0)

if _args["xcode"]!=False:
    debug = True
    cmd += ["-G","Xcode"]

if debug:
    cmd += ["-DCMAKE_BUILD_TYPE=Debug"]
else:
    cmd += ["-DCMAKE_BUILD_TYPE=Release"]

cmd += ["-DCMAKE_FIND_DEBUG_MODE=ON","--target","install"]

cmd += [prj_root]

ork.dep.require(["yarl","bullet","luajit","openexr","oiio","openvr","fcollada","assimp"])

Command(cmd).exec()



if _args["clean"]!=False:
    Command(["make","clean"]).exec()

cmd = ["make"]
if _args["verbose"]!=False:
    cmd += ["VERBOSE=1"]

if _args["serial"]==False:
    cmd += ["-j",ork.host.NumCores]

cmd += ["install"]

Command(cmd).exec()
