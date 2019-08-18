#!/usr/bin/env python

import os, sys, shutil, platform, argparse

as_main = (__name__ == '__main__')

parser = argparse.ArgumentParser()
parser.add_argument("-c", "--cmd", help="execute in environ")

args = parser.parse_args()
SYSTEM = platform.system()

IsWin = (os.name=="nt")

###########################################

file_dir = os.path.realpath(__file__)
par1_dir = os.path.dirname(file_dir)
par2_dir = os.path.dirname(par1_dir)
par3_dir = os.path.dirname(par2_dir)
par4_dir = os.path.dirname(par3_dir)
par5_dir = os.path.dirname(par4_dir)

root_dir = par2_dir
scripts_dir = "%s/scripts" % root_dir
sys.path.append(scripts_dir)
bin_dir = "%s/bin" % root_dir

if os.path.exists(par3_dir):
  os.environ["ORKDOTBUILD_WORKSPACE_DIR"]=par3_dir



stg_dir = "%s/stage"%par3_dir
os.system( "mkdir -p %s" % stg_dir)

if os.path.exists(stg_dir):
  os.environ["ORKDOTBUILD_STAGE_DIR"]=stg_dir

###########################################

import ork.build.utils as obt
from ork.build.pathtools import path
import ork.build.common

deco = ork.build.common.deco()

###########################################

def set_env(key,val):
  print "Setting var<" + deco.key(key)+"> to <" + deco.val(val) + ">"
  os.environ[key]	= val

def prepend_env(key,val):
  if False==(key in os.environ):
    set_env(key,val)
  else:
    os.environ[key]	= val + ":" + os.environ[key]
    print "prepend var<" + deco.key(key) + "> to<" + deco.val(os.environ[key]) + ">"

def append_env(key,val):
  if False==(key in os.environ):
    set_env(key,val)
  else:
    os.environ[key] = os.environ[key] + ":" + val
    print "append var<" + deco.key(key) + "> to<" + deco.val(os.environ[key]) + ">"

###########################################

set_env("color_prompt","yes")
set_env("ORKDOTBUILD_ROOT",root_dir)
prepend_env("PYTHONPATH",scripts_dir)
prepend_env("PATH",bin_dir)
prepend_env("PATH","%s/bin"%stg_dir)
prepend_env("DYLD_LIBRARY_PATH","%s/lib"%stg_dir)
prepend_env("LD_LIBRARY_PATH","%s/lib"%stg_dir)
prepend_env("SITE_SCONS","%s/site_scons/site_tools/"%root_dir)

import ork.build.localopts as localopts

set_env("QT5DIR",localopts.QT5DIR())
prepend_env("PATH",localopts.QT5DIR()+"/bin")


###########################################

print deco.inf("ROOTDIR<%s>" % root_dir)
print deco.inf("ORKDOTBUILD_STAGE_DIR<%s>" % stg_dir)
obt.check_for_projects(par3_dir)
print

###########################################
# maya dev
###########################################

mayaver = "maya2017"
mayapath = path("/Applications")/"Autodesk"/mayaver
mayaapp = mayapath/"Maya.app"
mayacon = mayaapp/"Contents"
mayainc = mayapath/"include"
mayalib = mayacon/"MacOS"
mayabin = mayacon/"bin"
mayadkpath = mayapath/"devkit"
mayaqmake = mayadkpath/"bin"/"qmake"
mayapipath = path(stg_dir)/"mayaplugs"
mayamodulepath = path(stg_dir)/"mayamodules"

if mayaqmake.exists():
  print deco.inf("Maya found at<%s>" % mayapath)
  set_env("MAYA_DIR",str(mayapath))
  set_env("MAYA_APP",str(mayapath))
  append_env("PATH",str(mayabin))
  set_env("MAYADK_DIR",str(mayadkpath))
  set_env("MAYAINC_DIR",str(mayainc))
  set_env("MAYALIB_DIR",str(mayalib))
  set_env("MAYA_QMAKE",str(mayaqmake))
  set_env("MAYA_PLUG_IN_PATH",str(mayapipath))
  set_env("MAYA_MODULE_PATH",str(mayamodulepath))

else:
  print deco.inf("Maya notfound at<%s>" % mayapath)

###########################################

IsCommandSet = hasattr(args,"cmd") and args.cmd

if as_main:
  bdeco = ork.build.common.deco(bash=True)
  BASHRC = 'parse_git_branch() { git branch 2> /dev/null | grep "*" | sed -e "s/*//";}; '
  PROMPT = bdeco.red('[ ORK ]')
  PROMPT += bdeco.yellow("\w")
  PROMPT += bdeco.orange("[$(parse_git_branch) ]")
  PROMPT += bdeco.white("> ")
  BASHRC += "\nexport PS1='%s';" % PROMPT
  BASHRC += "alias ls='ls -G';"
  bashrc = os.path.expandvars('$ORKDOTBUILD_STAGE_DIR/.bashrc')
  f = open(bashrc, 'w')
  f.write(BASHRC)
  f.close()
  print deco.inf("System is <"+os.name+">")
  if IsWin:
    if IsCommandSet:
      print "cmd: %s" % args.cmd
      os.system(args.cmd) # call shell with new vars (just "exit" to exit)
    else:
      os.environ["PROMPT"]="$E[0;91mORK$E[0;97m $P$G$E[0;m"
      os.system("cmd.exe /f")
  else:
    if IsCommandSet:
      os.system(args.cmd) # call shell with new vars (just "exit" to exit)
    else:
      shell = os.environ["SHELL"] # get previous shell
      os.system("%s --init-file '%s'" %(shell,bashrc)) # call shell with new vars (just "exit" to exit)
