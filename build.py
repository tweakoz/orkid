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

stage_dir = Path(os.path.abspath(str(ork.path.stage())))

ok = True

if _args["ez"]!=False:
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

    if ok:
        ok = Command(init_env+ch_ork_root+["--command","obt.dep.build.py qt5"]).exec()==0
    if ok:
        ok = Command(init_env+ch_ork_root+["--command","./build.py --debug"]).exec()==0
    if ok:
        ok = Command(init_env+ch_tuio+["--command","make install"]).exec()==0
    if ok:
        ok = Command(init_env+ch_ork_root+["--command","./build.py --debug"]).exec()==0
    if ok:
        sys.exit(0)
    else:
        sys.exit(-1)

if _args["xcode"]!=False:
    debug = True
    cmd += ["-G","Xcode"]

if debug:
    cmd += ["-DCMAKE_BUILD_TYPE=Debug"]
else:
    cmd += ["-DCMAKE_BUILD_TYPE=Release"]

cmd += ["-DCMAKE_FIND_DEBUG_MODE=OFF","--target","install"]
cmd += ["-DPYTHON_EXECUTABLE=%s/bin/python3"%(stage_dir)]
cmd += ["-DPYTHON_LIBRARY=%s/lib/libpython3.8d.so"%(stage_dir)]

cmd += [prj_root]

ork.dep.require(["bullet","openexr","oiio","openvr","fcollada","assimp","nvtt","lua","python"])

ok = Command(cmd).exec()==0


if _args["clean"]!=False:
    ok = Command(["make","clean"]).exec()==0

cmd = ["make"]
if _args["verbose"]!=False:
    cmd += ["VERBOSE=1"]

if _args["serial"]==False:
    cmd += ["-j",ork.host.NumCores]

cmd += ["install"]

ok = Command(cmd).exec()==0

if ok:
    sys.exit(0)
else:
    sys.exit(-1)
