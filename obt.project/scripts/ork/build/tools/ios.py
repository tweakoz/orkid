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
import slnprj

from SCons.Script.SConscript import SConsEnvironment

###############################################################################
# Python Module Export Declaration

__all__ = [ "DefaultBuildEnv" ]
__version__ = "1.0"

###############################################################################
# Basic Build Environment

def DefaultBuildEnv( env, prj ):
	##
	DEFS = '_IX _IOS _GCC ORK_CONFIG_OPENGL'
	CFLG = ' '
	LIBS = 'm objc'
	LIBPATH = '. '
	LINK = ''
	xc4base='/Developer'
	platdir = "%s/Platforms/iPhoneOS.platform" % xc4base
	sdkbase = "%s/Developer/SDKs/iPhoneOS5.0.sdk" % platdir
	fwdir = "%s/System/Library/Frameworks" % sdkbase
	clbin = "%s/Developer/usr/bin" % platdir
	clang = "%s/clang" % clbin
	clangpp = "%s/clang++" % clbin
	sdklibdir = " %s/usr/lib " % sdkbase
	DarwinArchs = string.split("-arch armv7")
	CCANDLDFLGS = '-miphoneos-version-min=5.0 -arch armv7 -isysroot "%s" ' % sdkbase
	##
	env.Replace( CXX = clangpp, CC = clang )
	env.Replace( LINK = clangpp )
	env.Replace( CPPDEFINES = string.split(DEFS) )
	env.Replace( CCFLAGS = string.split(CFLG) )
	env.Replace( CPPPATH  = [ '.' ] )
	env.Replace( LINKFLAGS=string.split(LINK) )
	env.Replace( LIBS=string.split(LIBS) )
	env.Replace( LIBPATH=string.split(LIBPATH+sdklibdir) )
	env.Replace( RANLIB = 'ranlib' )	
	env.Append( FRAMEWORKS = [ 'OpenGLES', 'CoreMIDI', 'CoreAudio' ] )
	env.Append( FRAMEWORKS = [ 'UIKIT', 'Foundation', 'QuartzCore', 'CoreGraphics' ] )
	env.Append( CCFLAGS = DarwinArchs )
	env.Append( LINK = DarwinArchs )
	env.Replace( AR="libtool" )
	env.Replace( ARFLAGS="-static -c -v " )
	env.Replace( ARCOM="$AR $ARFLAGS -o $TARGET $SOURCES" )
	env.Replace( RANLIBCOM="" )
	prj.XCFLG += CCANDLDFLGS
	prj.XCFLG += '-fno-common -fno-strict-aliasing -fexceptions -g -Wno-switch-enum -gdwarf-2 -Wno-sign-conversion -mthumb '
	prj.XCCFLG += '-std=gnu++0x -stdlib=libc++ ' + prj.XCFLG
	prj.XLINK = '-stdlib=libc++ -v -g -arch armv7 '
	prj.XLINK += CCANDLDFLGS
	for item in env['FRAMEWORKS']:
		prj.XLINK += '-framework %s ' % item

	prj.AddLibs( ' m c stdc++' ) # squish' )
	prj.CompilerType = 'gcc'
	prj.EmbeddedDevice = True
