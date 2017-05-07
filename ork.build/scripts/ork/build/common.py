###############################################################################
# Orkid SCONS Build System
# Copyright 2010, Michael T. Mayers
# email: michael@tweakoz.com
# The Orkid Build System is published under the GPL 2.0 license
# see http://www.gnu.org/licenses/gpl-2.0.html
###############################################################################

import glob, re, string, commands, sys, os
import shutil, fnmatch, platform

###############################################################################
# Python Module Export Declaration

__all__ = [
  "builddir_replace","globber", "DumpBuildEnv", "SetCompilerOptions",
  "SourceEnumerator", "RunUnitTest", "Project", "orkpath", "posixpath",
  "msplit", "recursive_glob", "deco"
]

__version__ = "1.0"

###############################################################################
is_tty = sys.stdout.isatty()
SYSTEM = platform.system()
IsOsx = (SYSTEM=="Darwin")
IsIx = (SYSTEM=="Linux") or IsOsx
TargetPlatform = "ix"
if IsOsx:
  TargetPlatform = "osx"
###############################################################################
def BuildArgs():
  rval = dict()
  rval["PLATFORM"] = TargetPlatform
  rval["BUILD"] = "release"
  if "build_debug" in os.environ and (os.environ["build_debug"]=="1"):
    rval["BUILD"] = "debug"
  return rval

###############################################################################
# INIT local options
###############################################################################

curpyname = sys.argv[0]

pydir = os.path.dirname(__file__)

###############################################################################

def builddir_replace( filelist, searchkey, replacekey ):
  a = [s.replace(searchkey,replacekey) for s in filelist]
  return a

###############################################################################

def replace( file, searchkey, replacekey ):
  regex = re.compile( searchkey )
  str_file = str(file)
  str_rep = regex.sub( replacekey, string.join( str_file, '' ) )
  return posixpath(str_rep)

###############################################################################

def recursive_glob_get_dirs(path):
    d=[]
    try:
        ld = os.listdir(path)
        for i in ld:
            if os.path.isdir(path+i):
                d.append(os.path.basename(i))
    except:pass
    return d

###############################################################################

def recursive_glob(path,pattern):
    l=[]
    if path[-1]!='/':
        path=path+'/'
    for i in recursive_glob_get_dirs(path):
        l=l+recursive_glob(path+i,pattern)
    try:
        dirlist = os.listdir(path)
        for i in dirlist:
            ii=i
            i=path+i
            if os.path.isfile(i):
                if fnmatch.fnmatch(ii,pattern):
                    l.append(i)
    except:
        pass
    
    return l

###############################################################################

def globber( folderbase, wildcard, subdirlist, excludelist=[] ):
  globs = []
  filtered_globs = []
  for s in subdirlist:
    str_glob = folderbase
    if s!=".":
       str_glob += s + '/'
    str_glob += wildcard
    these_globs = glob.glob( str_glob )
    globs += these_globs
  for s in globs:
    incfil = int(1)
    for t in excludelist:
      regex = re.compile( t )
      matchobj = re.search( t, s )
      if( matchobj ):
        incfil = int(0)
    if incfil == 1:
      filtered_globs += [ posixpath(s) ]
  return filtered_globs

###############################################################################

def rmdirforce(basepath):
  if os.path.isdir(basepath):
    for root, dirs, files in os.walk(basepath, topdown=False):
      for name in files:
        os.remove(os.path.join(root, name))
      for name in dirs:
        os.rmdir(os.path.join(root, name))
    os.rmdir(basepath)

###############################################################################

def rmtree( pathglob ):
  paths = glob.glob( os.path.normpath(pathglob) )
  for path in paths:
    npath = os.path.normpath(path)
    if os.path.isdir(npath):
      try:
        rmdirforce(npath)
      except OSError, x:
        print "cannot remove dir<%s>" % path
    elif os.path.isfile(npath):
      try:
        os.remove(npath)
      except OSError, x:
        print "cannot remove file<%s>" % path

###############################################################################

def msplit( str, sep=" " ):
  ret = []
  if sep in str:
    list = string.split(str,sep) # sep.split(str)
    for item in list:
        ret.append(item)
  else:
    ret.append(str)
  return ret

###############################################################################

def RunUnitTest(env,target,source):
  import subprocess
  app = str(source[0].abspath)
  if not subprocess.call(app):
    open(str(target[0]),'w').write("PASSED\n")

###############################################################################

def orkpath(posix_path):
  return os.sep.join(posix_path.split('/'))
    
###############################################################################

def posixpath(path):
  return '/'.join(os.path.normpath(path).split(os.sep))

###############################################################################

class deco:
  
  ###############################
  def __init__(self,bash=False):
    self.bash = bash
  ###############################
  def rgb256(self,r,g,b):
    r = int((r*5)/255)
    g = int((g*5)/255)
    b = int((b*5)/255)
    color = 16 + 36 * r + 6 * g + b
    rval = "\033[38;5;%dm" % color
    if self.bash:
      rval = "\[" + rval + "\]"
    return rval
  ###############################
  def reset(self):
    rval = "\033[m"
    if self.bash:
      rval = "\["+rval+"\]"
    return rval
  ###############################
  def magenta(self,string):
    return self.rgb256(255,0,255)+(string)+self.reset()
  def white(self,string):
    return self.rgb256(255,255,255)+(string)+self.reset()
  def orange(self,string):
    return self.rgb256(255,128,0)+(string)+self.reset()
  def yellow(self,string):
    return self.rgb256(255,255,0)+(string)+self.reset()
  def red(self,string):
    return self.rgb256(255,0,0)+(string)+self.reset()
  ###############################
  def key(self,string):
    return self.rgb256(255,255,0)+string+self.reset()
  def val(self,string):
    return self.rgb256(255,255,255)+string+self.reset()
  def path(self,string):
    return self.rgb256(255,255,128)+(string)+self.reset()
  def inf(self,string):
    return self.rgb256(128,128,255)+(string)+self.reset()
  def warn(self,string):
    return self.yellow(string)+self.reset()
  def err(self,string):
    return self.red(string)+self.reset()
  ###############################


