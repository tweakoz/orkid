#!python
###############################################################################
# Orkid SCONS Build System
# Copyright 2010, Michael T. Mayers
# email: michael@tweakoz.com
# The Orkid Build System is published under the GPL 2.0 license
# see http://www.gnu.org/licenses/gpl-2.0.html
###############################################################################
# Orkid Build Machine Local Options
# feel free to edit localopts.py, but NOT localopts.py.template
###############################################################################

import os, imp, ConfigParser
from ork.build.pathtools import path

def IsWindows():
    return os.name == "nt"

if IsWindows():
    import win32api

################################################################
__all__ = [
  "XCODEDIR",
  "OSXSDKROOT",
  "QT5DIR",
  "MAYADIR",
  "AQSISDIR",
  "ARCH",
  "ConfigFileName",
  "ConfigData",
  "dump" ]
################################################################

def GetDefault( varname, default ):
  ret = default
  if varname in os.environ:
    ret = os.environ[varname]
  if False==os.path.isdir(ret):
    print "<localopts.py> Warning: path<%s> <ret %s> does not exist" % (varname,ret)
  if os.path.isdir(ret):
    if IsWindows():
      ret = win32api.GetShortPathName(ret)
  return os.path.normpath(ret)

################################################################

def ConfigFileName():
  return "%s/../ork.build.ini"%os.environ["ORKDOTBUILD_ROOT"]

################################################################

ConfigData = ConfigParser.ConfigParser()

if os.path.isfile( ConfigFileName() ):
  #print "LOCALOPTS: Found %s" % ConfigFileName()
  ConfigData.read( ConfigFileName() )
  #print ConfigData
else:
    print "LOCALOPTS: Cannot find %s : using default options" % ConfigFileName()
    sdkroot = "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.14.sdk"
    ConfigData.add_section( "PATHS" )
    ConfigData.set( "PATHS", "XCODEDIR", GetDefault("XCODEDIR", "/Applications/Xcode.app"))
    ConfigData.set( "PATHS", "QT5DIR", GetDefault("QT5DIR", "/usr/local/opt/qt5"))
    ConfigData.set( "PATHS", "OSXSDKROOT", GetDefault("OSXSDKROOT", sdkroot))
    ConfigData.add_section( "CONFIG" )
    ConfigData.set( "CONFIG", "ARCH", GetDefault("ARCH", "x86_64") )
    cfgfile = open(ConfigFileName(),'w')
    ConfigData.write(cfgfile)
    cfgfile.close()

#print ConfigData.sections()

################################################################

def GetEnv( sect, varname ):
  ret = ""
  if ConfigData.has_option( sect, varname ):
    ret = ConfigData.get( sect, varname )
  #print ret
  if os.path.isdir(ret):
    if IsWindows():
      ret = win32api.GetShortPathName(ret)
    else:
      ret = ret
  return os.path.normpath(ret)

################################################################

def XCODEDIR(): # qt base dir
  return GetEnv( "PATHS", "XCODEDIR" )
def QT5DIR(): # qt base dir
  return GetEnv( "PATHS", "QT5DIR" )
def OSXSDKROOT(): # qt base dir
  return GetEnv( "PATHS", "OSXSDKROOT" )
def AQSISDIR():
  return GetEnv( "PATHS", "AQSISDIR" )
def MAYADIR(): # qt base dir
  return GetEnv( "PATHS", "MAYADIR" )
def ARCH():
  return GetEnv( "CONFIG", "ARCH" )

################################################################

def dump():
  print "XCODEDIR<%s>" % XCODEDIR()
