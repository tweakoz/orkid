#!/usr/bin/env python3

import sys
import os, argparse
import ork.host
import ork.dep
from ork.path import Path
from ork.command import Command, run
from ork import buildtrace
import ork._globals as _glob

parser = argparse.ArgumentParser(description='orkid build')
parser.add_argument('--clean', action="store_true", help='force clean build' )
parser.add_argument('--verbose', action="store_true", help='verbose build' )
parser.add_argument('--serial',action="store_true", help="non-parallel-build")
parser.add_argument('--debug',action="store_true", help=" debug build")
parser.add_argument('--cmakeenv',action="store_true", help=" display cmake build flags / envvars and exit")
parser.add_argument('--profiler',action="store_true", help=" profiled build")
parser.add_argument('--trace',action="store_true", help=" cmake trace")
parser.add_argument('--obttrace',action="store_true",help='enable OBT buildtrace logging')
parser.add_argument('--xcode',action="store_true", help=" xcode debug build")
parser.add_argument("--builddir")

_args = vars(parser.parse_args())

stage_dir = Path(os.path.abspath(str(ork.path.stage())))

build_dest = ork.path.stage()/"orkid"

if _args["builddir"]!=None:
    build_dest = Path(_args["builddir"])

debug = _args["debug"]!=False
profiler = _args["profiler"]!=False
do_cmakeenv = _args["cmakeenv"]!=False

if _args["xcode"]!=False:
    build_dest = ork.path.stage()/"orkid-xcode"

if _args["obttrace"]==True:
  _glob.enableBuildTracing()

with buildtrace.NestedBuildTrace({ "op": "ork.build.py"}) as nested:

  build_dest.mkdir(parents=True,exist_ok=True)
  build_dest.chdir()

  prj_root = Path(os.environ["ORKID_WORKSPACE_DIR"])
  ork_root = prj_root
  ok = True


  PYTHON = ork.dep.instance("python")
  print(PYTHON)
  ork.env.set("PYTHONHOME",PYTHON.home_dir)

  ######################################################################
  # ensure deps present
  ######################################################################

  dep_list = []

  if ork.host.IsLinux:
    dep_list += ["vulkan","openvr","rtmidi"]

  dep_list += ["glm","eigen",
               "lexertl14", "parsertl14","rapidjson",
               "luajit", "pybind11", "ispctexc",
               "openexr","oiio","openvdb",
               "embree","igl",
               "glfw","assimp", "easyprof",
               "bullet"]

  #if ork.host.IsOsx: # until moltenvk fixed on big sur
  #   dep_list += ["moltenvk"]

  l = list()
  chain = ork.dep.Chain(dep_list)
  for item in chain._list:
    l += [item._name]

  l.reverse()
  print(l)
  #assert(False)

  ork.dep.require(dep_list)


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

  cmd += ["-DPYTHON_HEADER_PATH=%s"%PYTHON.include_dir]
  cmd += ["-DPYTHON_LIBRARY_PATH=%s"%PYTHON.library_file]

  clangdep = ork.dep.instance("clang")

  cmd += ["-DCMAKE_CXX_COMPILER=%s"%clangdep.bin_clangpp]
  cmd += ["-DCMAKE_CC_COMPILER=%s"%clangdep.bin_clang]

  if ork.host.IsAARCH64:
    cmd += ["-DARCHITECTURE=AARCH64"]
  else:
    cmd += ["-DARCHITECTURE=x86_64"]

  ###################################################
  if _args["trace"]==True:
    cmd += ["--trace"]

  cmd += [prj_root]

  if do_cmakeenv:
    cmd += ["-N"]


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

  sys.exit( Command(cmd).exec() )
