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
import ork.build.localopts as localopts

from SCons.Script.SConscript import SConsEnvironment

print "Using Osx Build Env"

###############################################################################
# Python Module Export Declaration

__all__ = [ "DefaultBuildEnv" ]
__version__ = "1.0"

###############################################################################
# Basic Build Environment

XcodeDir = localopts.XCODEDIR()
AqsisDir = localopts.AQSISDIR()
Arch = localopts.ARCH()

print "OSX: using arch<%s>" % Arch
print "OSX: using xcode<%s>" % XcodeDir

USE_DEBUG_CXX = False

#############################################
class ClangToolChain:
  def __init__(self,env, prj):
	bindir = "%s/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin" % XcodeDir
	c_compiler = "%s/clang"%bindir
	cpp_compiler = "%s/clang++"%bindir
	env.Replace( CXX = cpp_compiler, CC = c_compiler )
	env.Replace( LINK = cpp_compiler )
	prj.AddLibs( ' m c c++' )
	prj.CompilerType = 'gcc'
	prj.XCFLG += "-arch %s " % Arch
	prj.XCFLG += '-fno-common -fno-strict-aliasing -g -Wno-switch-enum -Wno-deprecated-declarations '
	prj.XCXXFLG += '-std=c++11 -stdlib=libc++ ' + prj.XCFLG
	prj.XCXXFLG += '-F%s/Contents/Resources/include ' % AqsisDir
	prj.XLINK = '-stdlib=libc++ -v -g -F/Library/Frameworks -arch %s '%Arch
	prj.XLINK += '-F/System/Library/Frameworks/Quartz.framework/Frameworks '
#############################################
class MacPortsToolChain:
  def __init__(self,env, prj):
	c_compiler = "gcc-mp-4.8"
	cpp_compiler = "g++-mp-4.8"
	env.Replace( CXX = cpp_compiler, CC = c_compiler )
	env.Replace( LINK = cpp_compiler )
	prj.AddLibs( ' m c ' )
	prj.CompilerType = 'gcc'
	prj.XCFLG += "-arch %s " % Arch
	prj.XCFLG += '-fno-common -fno-strict-aliasing -g -Wno-switch-enum -Wno-deprecated-declarations '
	prj.XCXXFLG += '-std=c++0x -fexceptions ' + prj.XCFLG
	prj.XCXXFLG += '-F%s/Contents/Resources/include ' % AqsisDir
	prj.XLINK = '-v -g '
	prj.XLINK += "-arch %s " % Arch
#############################################
class HpcToolChain:
  def __init__(self,env, prj):
	c_compiler = "/opt/gcc48/bin/gcc"
	cpp_compiler = "/opt/gcc48/bin/g++"
	env.Replace( CXX = cpp_compiler, CC = c_compiler )
	env.Replace( LINK = cpp_compiler )
	prj.AddLibs( ' m c ' )
	prj.CompilerType = 'gcc'
	prj.XCFLG += "-arch %s " % Arch
	prj.XCFLG += '-fno-common -fno-strict-aliasing -g -Wno-switch-enum -Wno-deprecated-declarations '
	prj.XCXXFLG += '-std=c++0x -fexceptions ' + prj.XCFLG
	prj.XCXXFLG += '-F%s/Contents/Resources/include ' % AqsisDir
	prj.XLINK = '-v -g '
	prj.XLINK += "-arch %s " % Arch
#############################################
def DefaultBuildEnv( env, prj ):
	##
	DEFS = ' IX GCC ORK_OSX _DARWIN'
	#if USE_DEBUG_CXX:
	#	DEFS += ' _GLIBCXX_DEBUG '
	#if prj.IsLinux:
	#	DEFS += "LINUX "
	CCFLG = ' '
	CXXFLG = ' '
	LIBS = "m pthread"
	LIBPATH = ' /opt/local/lib '
	#if USE_DEBUG_CXX:
	#	LIBPATH += ' /usr/lib/x86_64-linux-gnu/debug '
	LINK = ''
	##
	
	##########################
	#toolchain = MacPortsToolChain(env,prj)
	toolchain = ClangToolChain(env,prj)
	#toolchain = HpcToolChain(env,prj)
	##########################
	
	env.Replace( CPPDEFINES = string.split(DEFS) )
	env.Replace( CCFLAGS = string.split(CCFLG) )
	env.Replace( CXXFLAGS = string.split(CXXFLG) )
#	env.Replace( CPPPATH  = [ ' /opt/local/include' ] )
	env.Replace( LINKFLAGS=string.split(LINK) )
	env.Replace( LIBS=string.split(LIBS) )
	env.Replace( LIBPATH=string.split(LIBPATH) )
	env.Replace( RANLIB = 'ranlib' )	
	env.Append( FRAMEWORKS = [ 'QtGui', 'QtCore', 'OpenGL', 'CoreMIDI', 'CoreAudio', 'AudioUnit', 'AudioToolbox' ] )
	env.Append( FRAMEWORKS = [ 'Carbon', 'Foundation', 'QuartzComposer' ] )
	env.Append( FRAMEWORKS = [ 'ApplicationServices', 'AppKit' ] )
	env.Append( FRAMEWORKS = [ 'MultitouchSupport', 'Cg' ] )
	env.Replace( AR="libtool" )
	env.Replace( ARFLAGS="-static -c -v -arch_only %s" % Arch )
	env.Replace( ARCOM="$AR $ARFLAGS -o $TARGET $SOURCES" )
	env.Replace( RANLIBCOM="" )

	env.Tool('osxbundle')

	prj.PostIncludePaths += string.split('/opt/local/include')

