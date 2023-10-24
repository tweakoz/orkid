#!/usr/bin/env python3

import sys
import os, argparse
import obt.host
import obt.dep
import obt.path
from obt.command import Command, run
from obt import buildtrace
import obt._globals as _glob

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

this_path = os.path.realpath(__file__)
this_dir = os.path.dirname(this_path)
this_dir = os.path.dirname(this_dir)
this_dir = os.path.dirname(this_dir)
#print(this_dir)

############################################################################

PYTHON = obt.dep.instance("python")
BOOST = obt.dep.instance("boost")
ORKID_DEPMODULE = obt.dep.instance("orkid") # fetch from orkid depper to reduce code bloat
for item in os.environ.keys():
  if("OBT" in item):
    print(item,os.environ[item])
assert(ORKID_DEPMODULE)

############################################################################

os.environ["ORKID_WORKSPACE_DIR"] = this_dir

stage_dir = obt.path.Path(os.path.abspath(str(obt.path.stage())))

build_dest = ORKID_DEPMODULE.builddir

if _args["builddir"]!=None:
    build_dest = obt.path.Path(_args["builddir"])

debug = _args["debug"]!=False
profiler = _args["profiler"]!=False
do_cmakeenv = _args["cmakeenv"]!=False

if _args["xcode"]!=False:
    build_dest = obt.path.stage()/"orkid-xcode"

if _args["obttrace"]==True:
  _glob.enableBuildTracing()

with buildtrace.NestedBuildTrace({ "op": "obt.build.py"}) as nested:

  build_dest.mkdir(parents=True,exist_ok=True)
  build_dest.chdir()

  os.environ["ORKID_BUILD_DEST"]=str(build_dest)

  prj_root = obt.path.Path(os.environ["ORKID_WORKSPACE_DIR"])
  ork_root = prj_root
  ok = True

  print("PRJ_ROOT: %s " % prj_root)
  print("BUILD_DEST: %s" % build_dest)


  ######################################################################
  # ensure deps present
  ######################################################################


  dep_list = ORKID_DEPMODULE.deplist

  l = list()
  chain = obt.dep.Chain(dep_list)
  for item in chain._list:
    l += [item._name]

  l.reverse()

  obt.dep.require(dep_list)

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

  #cmd += ["-DPYTHON_HEADER_PATH=%s"%PYTHON.include_dir]
  #cmd += ["-DPYTHON_LIBRARY_PATH=%s"%PYTHON.library_file]

  clangdep = obt.dep.instance("clang")

  cmd += ["-DBUILDING_ORKID=ON"]

  cmd += ["-DCMAKE_CXX_COMPILER=%s"%clangdep.bin_clangpp]
  cmd += ["-DCMAKE_C_COMPILER=%s"%clangdep.bin_clang]

  if obt.host.IsAARCH64:
    cmd += ["-DARCHITECTURE=AARCH64"]
  else:
    cmd += ["-DARCHITECTURE=x86_64"]

  cmd += ["-DCMAKE_INSTALL_PREFIX=%s"%obt.path.stage()]
  cmd += ["-DCMAKE_MODULE_PATH=%s"%(obt.path.libs()/"cmake")]

  cmd += ["-Wno-dev"]
  ###################################################

  BOOST_FLAGS = BOOST.cmake_additional_flags()
  for key in BOOST_FLAGS.keys():
    val = BOOST_FLAGS[key]
    cmd += ["-D%s=%s"%(key,val)]

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
    cmd += ["-j",obt.host.NumCores]

  cmd += ["install"]

  rval = Command(cmd).exec()

  if rval==0 and obt.host.IsDarwin:
    rval = Command(["obt.osx.macho.fixup.libs.py","--orklibs", "--orkpymods"]).exec()

sys.exit(rval)
