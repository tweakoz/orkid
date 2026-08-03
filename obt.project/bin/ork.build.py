#!/usr/bin/env python3

import sys
import os, argparse
import obt.host
import obt.dep
import obt.path
import obt.pathtools
from obt.command import Command, run
from obt import buildtrace
import obt._globals as _glob

#ln -s libMoltenVk.dylib libvulkan.1.dylib

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
parser.add_argument('--sanitize', choices=['address', 'thread', 'undefined'],
                    help="enable sanitizer (address=ASan+UBSan+Leak, thread=TSan+UBSan, undefined=UBSan only)")
parser.add_argument("--builddir")

_args = vars(parser.parse_args())

this_path = os.path.realpath(__file__)
this_dir = os.path.dirname(this_path)
this_dir = os.path.dirname(this_dir)
this_dir = os.path.dirname(this_dir)
#print(this_dir)

sys.path.append(this_dir+"/obt.project/scripts")
from ork import path as ork_path

############################################################################

PYTHON = obt.dep.instance("python")
BOOST = obt.dep.instance("boost")
ORKID_DEPMODULE = obt.dep.instance("orkid") # fetch from orkid depper to reduce code bloat
if _args["verbose"]!=False:
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

############################################################################
# BUILD MUTEX (owner-adjudicated 2026-07-22): serialize concurrent builds that
# share ONE staging install. Concurrent make on a shared tree + interleaved
# `make install`s = stale-.o FALSE-GREENS and cross-contaminated gates (the
# two-engine-lane incident). flock on <stage>/.ork_build_mutex — the kernel
# releases it on process death, so there are no stale locks to clean. Separate
# stagings (private-prefix worktrees) do not contend. A second build WAITS with
# a loud notice rather than failing: serialization IS the desired semantics.
############################################################################
import fcntl, time as _time
_mutex_path = str(stage_dir/".ork_build_mutex")
_mutex_f = open(_mutex_path, "a+")
try:
  fcntl.flock(_mutex_f, fcntl.LOCK_EX | fcntl.LOCK_NB)
except OSError:
  try:
    _mutex_f.seek(0)
    _holder = _mutex_f.read().strip() or "unknown"
  except Exception:
    _holder = "unknown"
  print("[ork.build] build mutex BUSY (%s) at %s — waiting..." % (_holder, _mutex_path), flush=True)
  _t0 = _time.time()
  fcntl.flock(_mutex_f, fcntl.LOCK_EX)  # block until the holder exits/finishes
  print("[ork.build] build mutex acquired after %.0fs" % (_time.time()-_t0), flush=True)
_mutex_f.seek(0)
_mutex_f.truncate()
_mutex_f.write("pid %d since %s\n" % (os.getpid(), _time.strftime("%H:%M:%S")))
_mutex_f.flush()

with buildtrace.NestedBuildTrace({ "op": "obt.build.py"}) as nested:

  build_dest.mkdir(parents=True,exist_ok=True)
  build_dest.chdir()

  os.environ["ORKID_BUILD_DEST"]=str(build_dest)

  prj_root = obt.path.Path(os.environ["ORKID_WORKSPACE_DIR"])
  ork_root = prj_root
  ok = True

#  print("PRJ_ROOT: %s " % prj_root)
#  print("BUILD_DEST: %s" % build_dest)


  ######################################################################
  # ensure deps present
  ######################################################################


  dep_list = ORKID_DEPMODULE.deplist + ["lunasvg"] # todo move to OBT depper
  
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

  if _args["sanitize"]:
    cmd += ["-DSANITIZER=%s" % _args["sanitize"].upper()]
  else:
    cmd += ["-USANITIZER"]  # Unset cached sanitizer from previous builds

  ###################################################
  # inject relevant state from deppers into cmake
  ###################################################

  #cmd += ["-DPYTHON_HEADER_PATH=%s"%PYTHON.include_dir]
  #cmd += ["-DPYTHON_LIBRARY_PATH=%s"%PYTHON.library_file]

  clangdep = obt.dep.instance("clang")

  cmd += ["-DBUILDING_ORKID=ON"]
  cmd += ["-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]

  cmd += ["-DCMAKE_CXX_COMPILER=%s"%clangdep.bin_clangpp]
  cmd += ["-DCMAKE_C_COMPILER=%s"%clangdep.bin_clang]

  # mold linker on Linux. orkid has heavy C++ link steps (ork_core,
  # ork_lev2, the _core/_lev2/_ecs python-extension .so's) — mold cuts
  # that link phase sharply. Passed as -D cache vars because this is a direct cmake
  # invocation — the -D values land straight in the cache. clang (the
  # orkid compiler, see above) supports -fuse-ld=mold. Linux-gated:
  # mold is ELF-only; macOS uses Apple's linker.
  if obt.host.IsLinux:
    cmd += ["-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=mold"]
    cmd += ["-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=mold"]
    cmd += ["-DCMAKE_MODULE_LINKER_FLAGS=-fuse-ld=mold"]

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

  if rval==0 and obt.host.IsLinux:
    #
    # ELF fixup: the Linux analog of the Mach-O fixup below. Third-party
    # deps install RUNPATH-less libs into stage/lib whose inter-lib NEEDED
    # entries only resolve via LD_LIBRARY_PATH — which the dev env must not
    # carry (staged libLLVM on the global loader path breaks system clang).
    # Unlike macOS there is no first-run marker: the sweep only patches
    # objects with NO rpath, so it is idempotent and cheap, and any dep
    # rebuild can reintroduce bare libs at any time.
    rval = Command(["obt.ix.elf.fixup.libs.py","--alllibs","--orkpymods"]).exec()

  if rval==0 and obt.host.IsDarwin:
    #
    # Macho fixup: on the first successful orkid build into this staging,
    # walk every dylib in stage/lib (--all) so any non-orkid dep that uses
    # @executable_path/.. install_names (e.g. libpng built as a framework)
    # gets normalized to @rpath. After that one-time pass, subsequent
    # incremental builds only need to fix orkid's own libs/pymods.
    fixup_marker = obt.path.manifests()/"orkid_macho_first_fixup_done"
    if fixup_marker.exists():
      fixup_args = ["--orklibs","--orkpymods"]
    else:
      # First-build pass: walk every dylib in stage/lib (--alllibs) AND
      # every C-extension under orkengine/{core,lev2,ecs,ecssim}
      # (--orkpymods). Without --orkpymods, _core.so / _lev2.so / _ecs.so /
      # _ecssim.so retain @executable_path/.. install_names that fail when
      # the venv python imports them directly.
      fixup_args = ["--alllibs","--orkpymods"]
    rval = Command(["obt.osx.macho.fixup.libs.py"]+fixup_args).exec()
    if rval==0 and not fixup_marker.exists():
      fixup_marker.touch()

  # Asset cache symlink: assets (downloads, large data files, etc.) can take
  # a long time to fetch and we want them to survive staging recreation.
  # On a fresh user, ~/.obt-global/assetcache won't exist yet — create it
  # (mkdir -p), then link <staging>/assetcache to it so subsequent fresh
  # stagings reuse the same cache. Runs on every successful build but is
  # idempotent once both the dir and the link exist.
  if rval == 0:
    stage_assetcache  = obt.path.stage()/"assetcache"
    global_assetcache = obt.path.Path(os.path.expanduser("~/.obt-global/assetcache"))
    global_assetcache.mkdir(parents=True, exist_ok=True)
    # exists() returns False for broken symlinks; is_symlink() catches them
    # so we don't overwrite an existing (even broken) link the user placed.
    if not stage_assetcache.exists() and not stage_assetcache.is_symlink():
      os.symlink(str(global_assetcache), str(stage_assetcache))

  # ork.python is built as a custom Mach-O wrapper around the OBT-built
  # python interpreter (see ork.core/tools/ork_python_wrapper.cpp). The
  # source python3.<minor> binary lives in $OBT_STAGE/pyvenv/bin/. Use the
  # python dep's deconame so this isn't pinned to a specific minor version.
  src = ork_path.pyvenv/"bin"/PYTHON._deconame
  dst = obt.path.stage()/"bin"/"ork.python"
  if rval==0 and (not dst.exists()):
    obt.pathtools.copyfile(src,dst)

sys.exit(rval)

