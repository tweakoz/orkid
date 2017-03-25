#!/usr/bin/env python

import os, sys, shutil, platform, argparse
import ansi.color.fx as afx
#from ansi.color import fg, bg
#from ansi.color.rgb import rgb256

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

stg_dir = "%s/stage"%par3_dir
os.system( "mkdir -p %s" % stg_dir)

if os.path.exists(stg_dir):
	os.environ["ORKDOTBUILD_STAGE_DIR"]=stg_dir

###########################################

import ork.build.utils as obt
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
		print "Setting var<" + deco.key(key) + "> to<" + deco.val(os.environ[key]) + ">"

###########################################

set_env("color_prompt","yes")
set_env("ORKDOTBUILD_ROOT",root_dir)
prepend_env("PYTHONPATH",scripts_dir)
prepend_env("PATH",bin_dir)
prepend_env("PATH","%s/bin"%stg_dir)
prepend_env("DYLD_LIBRARY_PATH","%s/lib"%stg_dir)
prepend_env("LD_LIBRARY_PATH","%s/lib"%stg_dir)
prepend_env("SITE_SCONS","%s/site_scons/site_tools/"%root_dir)

###########################################

print deco.inf("ROOTDIR<%s>" % root_dir)
print deco.inf("ORKDOTBUILD_STAGE_DIR<%s>" % stg_dir)
obt.check_for_projects(par3_dir)
print

###########################################

IsCommandSet = hasattr(args,"cmd") and args.cmd

if as_main:
    BASHRC = 'parse_git_branch() { git branch 2> /dev/null | grep "*" | sed -e "s/*//";}; '
    PROMPT = "\["+deco.red(' ORK ')+"\]"
    PROMPT += "\["+deco.yellow("\w")+"\]"
    PROMPT += "\["+deco.orange("$(parse_git_branch) ")+"\]"
    PROMPT += "\["+deco.white("> ")+"\]"
    BASHRC += "export PS1='%s';" % PROMPT
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

