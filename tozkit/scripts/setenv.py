#!/usr/bin/python

import os
import sys
import inspect

if os.environ.get("ORKDOTBUILD_ROOT")==None:
	print "Please init ork.build environment first!"
	sys.exit(0)

import ork.build.utils as obu

as_main = (__name__ == '__main__')

num_cores = obu.num_cores
prepend_env = obu.prepend_env
append_env = obu.append_env
set_env = obu.set_env

##################################################
this_script_path = inspect.getfile(inspect.currentframe())
this_script_p1 = os.path.split(this_script_path)[0]
root_dir = os.path.split(this_script_p1)[0]
build_dir = "%s/build" % root_dir

print "TOZKIT ROOTDIR<%s>" % root_dir

##################################################
stage_dir = "%s/stage" % root_dir
odbsd = os.environ.get("ORKDOTBUILD_STAGE_DIR")
if odbsd != None:
	stage_dir = odbsd

##################################################

qt_dir = "%s/ext/qt5/qtbase" % root_dir

##################################################

set_env("YO","what_up")
set_env("color_prompt","yes")
set_env("TOZ_ROOT",root_dir)
set_env("TOZ_BUILD",build_dir)
set_env("TOZ_STAGE",stage_dir)
#set_env("CONFIG_SITE","%s/scripts/config.site"%root_dir)
append_env("PYTHONPATH","%s/scripts:%s/lib/python2.7/site-packages"%(root_dir,stage_dir))

##################################################

set_env("CXXFLAGS","-I%s/include"%root_dir)
set_env("CMAKE_INSTALL_PREFIX",stage_dir)

##################################################

set_env("HFS","") # cortex not building with Houdini yet...
set_env("CORTEX_OPTIONS_FILE","%s/scripts/cortex.options"%root_dir)
set_env("GAFFER_OPTIONS_FILE","%s/scripts/gaffer.options"%root_dir)

##################################################

set_env("ILMBASE_INCLUDE_DIR","%s/include"%stage_dir)
set_env("ILMBASE_HOME",stage_dir)
set_env("OPENIMAGEIOHOME",stage_dir)
set_env("MAKEFLAGS","-j %s"%num_cores)

##################################################

DELPATH = "%s/3dl"%(stage_dir)
set_env("DELIGHT",DELPATH)
prepend_env("LD_LIBRARY_PATH","%s/lib"%DELPATH)
prepend_env("PATH","%s/bin"%DELPATH)
set_env("INFOPATH","%s/doc/info"%DELPATH)
set_env("DL_TEXTURES_PATH",".")
set_env("DL_DISPLAYS_PATH",".:%s/displays:%s/rmanDisplays"%(DELPATH,stage_dir))
set_env("DL_SHADERS_PATH",".:%s/shaders"%(DELPATH))
set_env("SHADERDL_OPTIONS", "-I%s/shaderlink/src/data/include"%stage_dir)

##################################################

set_env("LLVM_DIRECTORY","/usr/lib/llvm-3.0") ### For OSL

##################################################

prepend_env("PATH","%s/scripts:%s/bin:%s/bin"%(root_dir,stage_dir,root_dir))
prepend_env("LD_LIBRARY_PATH","%s/lib"%(stage_dir))
prepend_env("PKG_CONFIG_PATH","%s/lib/pkgconfig"%(stage_dir))

if as_main:
	shell = os.environ["SHELL"] # get previous shell
	os.system(shell) # call shell with new vars (just "exit" to exit)
