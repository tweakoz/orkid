#!/usr/bin/env python3

import sys
import os, argparse
import ork.host
import ork.dep
from ork.path import Path
from ork.command import Command, run

parser = argparse.ArgumentParser(description='orkid build')
parser.add_argument('--clean', action="store_true", help='force clean build' )
parser.add_argument('--verbose', action="store_true", help='verbose build' )
parser.add_argument('--serial',action="store_true", help="non-parallel-build")
parser.add_argument('--debug',action="store_true", help=" debug build")
parser.add_argument('--profiler',action="store_true", help=" profiled build")
parser.add_argument('--trace',action="store_true", help=" cmake trace")
parser.add_argument('--xcode',action="store_true", help=" xcode debug build")
parser.add_argument('--ez',action="store_true", help=" ez build (use workarounds)")

_args = vars(parser.parse_args())

this_dir = Path(os.path.dirname(os.path.realpath(__file__)))
stage_dir = Path(os.path.abspath(str(ork.path.stage())))

build_dest = ork.path.stage()/"orkid"
debug = _args["debug"]!=False
profiler = _args["profiler"]!=False

if _args["xcode"]!=False:
    build_dest = ork.path.stage()/"orkid-xcode"

build_dest.mkdir(parents=True,exist_ok=True)
build_dest.chdir()

prj_root = Path(os.environ["ORKID_WORKSPACE_DIR"])
ork_root = prj_root
ok = True

######################################################################
# ez install
######################################################################

if _args["ez"]!=False:
    this_script = ork_root/"build.py"
    init_env_script = ork_root/"ork.build"/"bin"/"init_env.py"
    init_env = [init_env_script,"--launch",stage_dir]
    ch_ork_root = ["--chdir",ork_root]
    ch_tuio = ["--chdir",stage_dir/"orkid"/"ork.tuio"]
    ################
    # workarounds
    ################

    def docmd(cmdlist):
        global ok
        if ok:
            ok = Command(cmdlist).exec()==0

    docmd(init_env+ch_ork_root+["--command","obt.dep.build.py pkgconfig"])
    docmd(init_env+ch_ork_root+["--command","obt.dep.build.py python"])
    Command(init_env+ch_ork_root+["--command","./build.py --debug"]).exec()
    docmd(init_env+ch_tuio+["--command","make install"])
    docmd(init_env+ch_ork_root+["--command","./build.py --debug"])

    if ok:
        sys.exit(0)
    else:
        sys.exit(-1)

######################################################################
# ensure deps present
######################################################################

python = ork.dep.require("python")
pybind11 = ork.dep.require("pybind11")
qt5forpython = ork.dep.require("qt5forpython")

ork.dep.require(["bullet","openexr","oiio","fcollada","assimp",
                 "nvtt","lua","glfw","ispctexc",
                 "easyprof","eigen","igl"])

if ork.host.IsOsx:
   ork.dep.require(["moltenvk"])
if ork.host.IsLinux:
   ork.dep.require(["vulkan","openvr"])

######################################################################
# regen shiboken bindings
######################################################################

run([this_dir/"ork.lev2"/"pyext"/"shiboken"/"regen.py"])

######################################################################
# prep for build
######################################################################

build_dest.chdir()

cmd = ["cmake"]

if _args["xcode"]!=False:
  debug = True
  cmd += ["-G","Xcode"]

if debug:
  cmd += ["-DCMAKE_BUILD_TYPE=Debug"]
else:
  cmd += ["-DCMAKE_BUILD_TYPE=Release"]

if profiler:
  cmd += ["-DPROFILER=ON"]
else:
  cmd += ["-DPROFILER=OFF"]

###################################################
# inject relevant state from deppers into cmake
###################################################

cmd += ["-DPYTHON_HEADER_PATH=%s"%python.include_dir()]
cmd += ["-DPYTHON_LIBRARY_PATH=%s"%python.library_file()]

cmd += ["-DSHIBOKEN_HEADER_PATH=%s"%qt5forpython.include_dir()]
cmd += ["-DSHIBOKEN_LIBRARY_FILE=%s"%qt5forpython.library_file()]

cmd += ["-DPYSIDE_HEADER_PATH=%s"%qt5forpython.pyside_include_dir()]
cmd += ["-DPYSIDE_LIBRARY_PATH=%s"%qt5forpython.pyside_library_dir()]
cmd += ["-DPYSIDE_LIBRARY_FILE=%s"%qt5forpython.pyside_library()]
cmd += ["-DPYSIDE_QTGUI_LIB=%s"%qt5forpython.pyside_qtlibrary("QtGui")]

###################################################
# inject generated shiboken binding path into cmake
###################################################

cmd += ["-DSHIBOKENBINDINGSPATH=%s"%(build_dest/"shiboken-bindings")]

###################################################
if _args["trace"]==True:
  cmd += ["--trace"]

cmd += ["--target","install"]

cmd += [prj_root]



ok = (Command(cmd).exec()==0)

if not ok:
  sys.exit(-1)


######################################################################
# build
######################################################################

build_dest.chdir()

if _args["clean"]!=False:
  ok = (Command(["make","clean"]).exec()==0)
  if not ok:
    sys.exit(-1)

cmd = ["make"]
if _args["verbose"]!=False:
  cmd += ["VERBOSE=1"]

if _args["serial"]==False:
  cmd += ["-j",ork.host.NumCores]

cmd += ["install"]

sys,exit( Command(cmd).exec() )
