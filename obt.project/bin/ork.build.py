#!/usr/bin/env python3

import sys
import os, argparse
import ork.host
import ork.env
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
parser.add_argument("--builddir")

_args = vars(parser.parse_args())

stage_dir = Path(os.path.abspath(str(ork.path.stage())))

build_dest = ork.path.stage()/"orkid"

if _args["builddir"]!=None:
    build_dest = Path(_args["builddir"])

debug = _args["debug"]!=False
profiler = _args["profiler"]!=False

if _args["xcode"]!=False:
    build_dest = ork.path.stage()/"orkid-xcode"

build_dest.mkdir(parents=True,exist_ok=True)
build_dest.chdir()

prj_root = Path(os.environ["ORKID_WORKSPACE_DIR"])
ork_root = prj_root
ok = True

PYTHON = ork.dep.instance("python")
#print(PYTHON)
ork.env.set("PYTHONHOME",PYTHON.home_dir)


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

    ##########################################
    # boostrap pkgconfig/python
    ##########################################
    docmd(init_env+ch_ork_root+["--command","obt.dep.build.py python"]) # bootstrap python

    ##########################################
    # boostrap pkgconfig/qt
    ##########################################
    docmd(init_env+ch_ork_root+["--command","obt.dep.build.py qt5"]) # bootstrap qt

    ##########################################
    # start ork build
    ##########################################

    if ok:
      ok = Command(init_env+ch_ork_root+["--command","./build.py --debug"]).exec()==0 # start ork build

    ##########################################
    # ork.build probably failed here because of the tuio issue
    #  need to move tuio to a dependency module
    ##########################################

    if False==ok:
      docmd(init_env+ch_tuio+["--command","make install"]) # hackinstall tuio

    ##########################################
    # continue ork build
    ##########################################

    docmd(init_env+ch_ork_root+["--command","./build.py --debug"]) # continue...

    ##########################################

    if ok:
        sys.exit(0)
    else:
        sys.exit(-1)

######################################################################
# ensure deps present
######################################################################

python = ork.dep.require("python")
pybind11 = ork.dep.require("pybind11")
#qt5forpython = ork.dep.require("qt5forpython")
#pyqt5 = ork.dep.require("pyqt5")

ork.dep.require(["bullet","openexr","oiio","assimp",
                 "lua","glfw","ispctexc",
                 "lexertl14","parsertl14",
                 "easyprof","eigen","embree","igl"])

#if ork.host.IsOsx: # until moltenvk fixed on big sur
#   ork.dep.require(["moltenvk"])
if ork.host.IsLinux:
   ork.dep.require(["vulkan","openvr","rtmidi"])

######################################################################
# regen shiboken bindings
######################################################################

#if ork.host.IsLinux:
#  run([ork_root/"ork.lev2"/"pyext"/"lev2qt"/"regen.py"])

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

if ork.host.IsOsx:
  cmd += ["-DCMAKE_OSX_SYSROOT=%s"%str(ork.path.osx_sdkdir())]

###################################################
# inject relevant state from deppers into cmake
###################################################

cmd += ["-DPYTHON_HEADER_PATH=%s"%python.include_dir]
cmd += ["-DPYTHON_LIBRARY_PATH=%s"%python.library_file]

#cmd += ["-DSHIBOKEN_HEADER_PATH=%s"%qt5forpython.include_dir]
#cmd += ["-DSHIBOKEN_LIBRARY_FILE=%s"%qt5forpython.library_file]

#cmd += ["-DPYSIDE_HEADER_PATH=%s"%qt5forpython.pyside_include_dir]
#cmd += ["-DPYSIDE_LIBRARY_PATH=%s"%qt5forpython.pyside_library_dir]
#cmd += ["-DPYSIDE_LIBRARY_FILE=%s"%qt5forpython.pyside_library]
#cmd += ["-DPYSIDE_QTGUI_LIB=%s"%qt5forpython.pyside_qtlibrary("QtGui")]

clangdep = ork.dep.instance("clang")


cmd += ["-DCMAKE_CXX_COMPILER=%s"%clangdep.bin_clangpp]
cmd += ["-DCMAKE_CC_COMPILER=%s"%clangdep.bin_clang]

###################################################
# inject generated shiboken binding path into cmake
###################################################

#cmd += ["-DSHIBOKEN_BINDINGS_PATH=%s"%(build_dest/"shiboken-bindings")]
cmd += ["-DPYQT5_BINDINGS_DIR=%s"%(build_dest/"sip-bindings")]

###################################################
if _args["trace"]==True:
  cmd += ["--trace"]

#cmd += ["--target","install"]

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

if _args["serial"]!=True:
  cmd += ["-j",ork.host.NumCores]

cmd += ["install"]

sys,exit( Command(cmd).exec() )
