###############################################################################
# Orkid SCONS Build System
# Copyright 2010, Michael T. Mayers
# email: michael@tweakoz.com
# The Orkid Build System is published under the GPL 2.0 license
# see http://www.gnu.org/licenses/gpl-2.0.html
###############################################################################

import glob
import re
import string
import os
import sys
import ork.build.slnprj

from SCons.Script.SConscript import SConsEnvironment

print "Using Ix Build Env"
###############################################################################
# Python Module Export Declaration

__all__ = [ "DefaultBuildEnv" ]
__version__ = "1.0"

###############################################################################
# Basic Build Environment

USE_DEBUG_CXX = False

def DefaultBuildEnv( env, prj ):
	##
	DEFS = ' IX GCC '
	if USE_DEBUG_CXX:
		DEFS += ' _GLIBCXX_DEBUG '
	if prj.IsLinux:
		DEFS += "LINUX ORK_LINUX"
	CCFLG = ' '
	CXXFLG = ' '
	LIBS = "m rt pthread openvr_api"
	LIBPATH = ' . '
	if USE_DEBUG_CXX:
		LIBPATH += ' /usr/lib/x86_64-linux-gnu/debug '
	else:
		LIBPATH += ' /usr/lib/x86_64-linux-gnu '

	LIBPATH += " /usr/local/lib "

	LINK = ''
	##
	#clang = "gcc-4.7"
	#clangpp = "g++-4.7"
	clang = "clang"
	clangpp = "clang++"
	env.Replace( CXX = clangpp, CC = clang )
	env.Replace( LINK = clangpp )
	env.Replace( CPPDEFINES = string.split(DEFS) )
	env.Replace( CCFLAGS = string.split(CCFLG) )
	env.Replace( CXXFLAGS = string.split(CXXFLG) )
	env.Replace( CPPPATH  = [ '.', "/usr/local/include" ] )
	env.Replace( LINKFLAGS=string.split(LINK) )
	env.Replace( LIBS=string.split(LIBS) )
	env.Replace( LIBPATH=string.split(LIBPATH) )

	CxFLG = '-fPIE -fno-common -fno-strict-aliasing -g -Wno-switch-enum '
	CxFLG += '-Imkspecs/linux-g++-64 -D_REENTRANT -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_GUI_LIB -DQT_CORE_LIB '
	prj.XCCFLG += CxFLG
	prj.XCXXFLG += CxFLG + " --std=c++17 -fexceptions "

	prj.CompilerType = 'gcc'

	prj.XLINK = '-m64 -v -g '
