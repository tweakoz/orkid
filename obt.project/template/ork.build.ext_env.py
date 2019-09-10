#!/usr/bin/python

import os
import sys

as_main = (__name__ == '__main__')

###########################################

file_dir = os.path.realpath(__file__)
par1_dir = os.path.dirname(file_dir)

root_dir = par1_dir
obt_dir = "%s/ork.build"%root_dir
scripts_dir = "%s/scripts" % obt_dir
bin_dir = "%s/bin" % obt_dir
print "ROOTDIR<%s>" % root_dir

stg_dir = "%s"%root_dir

if os.path.exists(stg_dir):
	print "ORKDOTBUILD_STAGE_DIR<%s>" % stg_dir
	os.environ["ORKDOTBUILD_STAGE_DIR"]=stg_dir

###########################################

def set_env(key,val):
	print "Setting var<%s> to<%s>" % (key,val)
	os.environ[key]	= val

def prepend_env(key,val):
	if False==(key in os.environ):
		set_env(key,val)
	else:
		os.environ[key]	= val + ":" + os.environ[key]
		print "Setting var<%s> to<%s>" % (key,os.environ[key])

###########################################

set_env("color_prompt","yes")
set_env("ORKDOTBUILD_ROOT","%s/ork.build"%root_dir)
prepend_env("PYTHONPATH",scripts_dir)
prepend_env("PATH",bin_dir)
prepend_env("PATH","%s/bin"%stg_dir)
prepend_env("LD_LIBRARY_PATH","%s/lib"%stg_dir)
prepend_env("SITE_SCONS","%s/site_scons"%scripts_dir)
sys.path.append(scripts_dir)
import ork.build.utils as obt

###########################################

print
print "ork.build eviron initialized ORKDOTBUILD_ROOT<%s>"%root_dir
print "scanning for projects..."
obt.check_for_projects(root_dir)
print

###########################################

if as_main:
	shell = os.environ["SHELL"] # get previous shell
	os.system(shell) # call shell with new vars (just "exit" to exit)

