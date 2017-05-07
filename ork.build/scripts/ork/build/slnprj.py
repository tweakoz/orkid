###############################################################################
# Orkid SCONS Build System
# Copyright 2010, Michael T. Mayers
# email: michael@tweakoz.com
# The Orkid Build System is published under the GPL 2.0 license
# see http://www.gnu.org/licenses/gpl-2.0.html
###############################################################################
import glob, re, string, StringIO
import commands, sys, os
import shutil, fnmatch, platform, pickle
from sets import Set

import ork.build.utils as utils
import ork.build.common as common
deco = common.deco()

HostIsOsx = common.IsOsx
HostIsIx = common.IsIx
SYSTEM = platform.system()
#print SYSTEM
###############################################################################
#IsOsx = (SYSTEM=="Darwin")
###############################################################################
# OSX is for the most part an IX
#IsIx = (SYSTEM=="Linux") or IsOsx
###############################################################################
if HostIsIx!=True:
  import win32pipe
  import win32api
###############################################################################
import ork.build.tools.gch as gch
###############################################################################
# Python Module Export Declaration

__all__ = [
  "SetCompilerOptions", "SourceEnumerator", "Project"
  ]
  
optprj = ""
optprj = "core lev2 bullet273 ent tool tuio tout lua luabind"
optset = set()
for item in string.split(optprj):
  optset.add("ork.%s"%item)

stage_dir = os.environ["ORKDOTBUILD_STAGE_DIR"]

###############################################################################

def LibBuilder( BuildEnv, library, sources, srcbase, dstbase ):
  BuildDir( dstbase, srcbase, duplicate=0 )
  print( "LibBuilder [dstbase: " + dstbase + "] [srcbase: " + srcbase + "]"  )
  ObjectFiles = common.builddir_replace( sources, srcbase, dstbase )
  return BuildEnv.Library( dstbase+library, ObjectFiles )

def ExeBuilder( BuildEnvironment, Executable, Sources, SourceBase, DestBase ):
    BuildDir( DestBase, SourceBase, duplicate=0 )
    ObjectFiles = common.builddir_replace( Sources, SourceBase, DestBase )
    print "Building executable : " + string.join( ObjectFiles )
    return BuildEnvironment.Program( Executable, ObjectFiles )

###############################################################################

class SourceEnumerator:
  def __init__(self,basefolder,BUILD_DIR):
    self.basefolder = basefolder
    self.BUILD_DIR = BUILD_DIR
    self.sourceobjs = string.split('')

  def AddFoldersExc(self,folders, excludes, pattern):
    srclist = string.split(folders)
    exclist = string.split(excludes)
    sourcefiles = common.globber( self.basefolder, pattern, srclist , exclist)      
    self.sourceobjs  += common.builddir_replace( sourcefiles, self.basefolder, self.BUILD_DIR )

  def AddFolders(self,folders, pattern):
    srclist = string.split(folders)
    exclist = string.split('')
    sourcefiles = common.globber( self.basefolder, pattern, srclist , exclist)
    self.sourceobjs  += common.builddir_replace( sourcefiles, self.basefolder, self.BUILD_DIR )

  def AddFoldersNoRep(self,folders, pattern):
    srclist = string.split(folders)
    exclist = string.split('')
    self.sourceobjs  += common.globber( self.basefolder, pattern, srclist , exclist)

  def dump(self):
    print "yo"

###############################################################################

def DumpBuildEnv( BuildEnv ):
  dict = BuildEnv.Dictionary()
  keys = dict.keys()
  keys.sort()
  for key in keys:
    print "construction variable = '%s', value = '%s'" % (key, dict[key])

###############################################################################

def GetProcessor(args):

  PROCESSOR = "cpu"
  PLAT = sys.platform.lower().replace( " ", "_" )
  TARGETPLAT = args['PLATFORM']
  if PLAT == 'darwin':
    PROCESSOR='universal'
    if TARGETPLAT=='ios':
      PROCESSOR='arm'
  elif PLAT == 'cygwin':
    PROCESSOR=commands.getstatusoutput('uname -m')[1]
  elif PLAT == 'win32':
    PROCESSOR='x86'
  else:
    PROCESSOR = commands.getstatusoutput('uname -p')[1]
  return PROCESSOR;

###############################################################################

def CommandPrinter(s, target, src, env):
    """s is the original command line, target and src are lists of target
    and source nodes respectively, and env is the environment."""
    #print( "yo" )
    tgt = deco("path",join([str(x) for x in target]) )
    print( "building target<%s>" % tgt )


###############################################################################

def BuildSuffix(ARGUMENTS):
  PLATFORM = ARGUMENTS['PLATFORM']
  BUILD = ARGUMENTS['BUILD']
  BUILDNAME = "%s.%s" % (PLATFORM,BUILD)
  return BUILDNAME

###############################################################################

class Project:

  ############################################

  def __init__(self,Environment,name):
    
    ARGUMENTS = common.BuildArgs()
    self.arguments = ARGUMENTS
    self.suffix = BuildSuffix(ARGUMENTS)

    self.additionalfiles = list()
    self.PrjDir=os.getcwd()
    self.LogConfig = False
    self.PLATFORM = ARGUMENTS['PLATFORM']
    self.BUILD = ARGUMENTS['BUILD']
    self.HOSTPLAT = sys.platform.lower().replace( " ", "_" )
    self.TARGETPLAT = ""
    self.PROCESSOR = GetProcessor(ARGUMENTS)
    self.BUILDNAME = BuildSuffix(ARGUMENTS)
    ##################################
    obj_dir = "obj"
    self.SUFFIX = BuildSuffix(ARGUMENTS)
    self.BUILD_DIR = '%s/%s.%s/' % (obj_dir,self.BUILDNAME,name)
    self.OutputName = '%s.%s' % (name,self.BUILDNAME)
    self.BaseName = name
    ##################################
    self.BaseEnv = Environment.Clone()
    self.BaseEnv.Replace( QT5_MOCCOMSTR = deco.magenta("Moccing ")+deco.path("$SOURCE") )
    self.BaseEnv.Replace( CCCOMSTR = deco.magenta("Compiling ")+deco.path("$SOURCE") )
    self.BaseEnv.Replace( CXXCOMSTR = self.BaseEnv['CCCOMSTR'])
    self.BaseEnv.Replace( SHCCCOMSTR = self.BaseEnv['CCCOMSTR'])
    self.BaseEnv.Replace( SHCXXCOMSTR = self.BaseEnv['CCCOMSTR'])
    self.BaseEnv.Replace( ARCOMSTR = deco.magenta("Archiving ")+deco.path("$TARGET") )
    self.BaseEnv.Replace( LINKCOMSTR = deco.magenta("Linking ")+deco.path("$TARGET") )
    self.BaseEnv.Replace( SHLINKCOMSTR = deco.magenta("Dylinking ")+deco.path("$TARGET") )
      #self.BaseEnv['PRINT_CMD_LINE_FUNC'] = CommandPrinter
    ##################################
    # QT crap

    self.BaseEnv['QT5_DEBUG'] = 0
    QT5DIR = os.path.expandvars('$QT5DIR')
    self.BaseEnv["QT5DIR"] = QT5DIR
    self.BaseEnv["ENV"]['PKG_CONFIG_PATH'] = QT5DIR+'/lib/pkgconfig'

    ##################################
    self.CustomDefines = []
    ##################################
    self.IsLinux = (SYSTEM=="Linux")
    self.IsIx = (self.PLATFORM=='ix')
    self.IsOsx = (self.PLATFORM=='osx')
    self.IsIos = (self.PLATFORM=='ios')
    self.IsMsVc = (self.PLATFORM=='msvc')
    self.IsDistVc = (self.PLATFORM=='distvc')
    self.IsCygwin = (self.HOSTPLAT=='cygwin')
    self.IsDbg = (self.BUILD=='dbg')
    self.IsOpt = (self.BUILD=='opt')
    self.IsRel = (self.BUILD=='rel')
    ##############
    self.PreIncludePaths = list(string.split("%s/include"%stage_dir))
    self.IncludePaths = list()
    self.IncludePathsQt = list()
    self.PostIncludePaths = list()
    ##############
    self.PreLibraryPaths = string.split("%s/lib"%stage_dir)
    self.LibraryPaths = list()
    self.PostLibraryPaths = list()
    ##############

    #if os.environ.has_key("PRJ_LIBDIRS"):
    # self.LibraryPaths += string.split(os.environ["PRJ_LIBDIRS"])      

    self.Libraries = list()
    self.ExplicitMocs = list()
    self.Frameworks = list()
    self.sourcebase = ''
    self.IsLibrary = False
    self.IsExe = False
    self.EmbeddedDevice = False
        
    ##############
    # common stuff
    ##############

    self.XDEFS = 'ORKPLAT_%s ORKHOSTPLAT_%s ORKPROC_%s ' %(self.PLATFORM,self.HOSTPLAT,self.PROCESSOR)
    self.XDEFS += '_PLATFORM=%s_%s_%s ' %(self.PLATFORM,self.HOSTPLAT,self.PROCESSOR)
    if os.environ.has_key("ORK_OPT_BUILD"):
      self.XDEFS += 'NDEBUG '
    else:
      self.XDEFS += '_DEBUG '
    self.XCFLG = ''
    self.XCCFLG = ''
    self.XCXXFLG = ''
    
    ############################
    # Build Tools/Env Selection
    ############################

    if self.IsOsx:
      import ork.build.tools.osx as build_tools
      build_tools.DefaultBuildEnv( self.BaseEnv, self )
    elif self.IsIos:
      import ork.build.tools.ios as build_tools
      ios.DefaultBuildEnv( self.BaseEnv, self )
    elif self.IsIx:
      import ork.build.tools.ix as build_tools
      build_tools.DefaultBuildEnv( self.BaseEnv, self )
    elif( self.IsMsVc ):
      self.BUILDNAME = "msvc%s" % (self.BUILD)
      if( self.IsOpt ):
        msvc_build.optenv( self.BaseEnv )
      else:
        msvc_build.dbgenv( self.BaseEnv )
      self.AddLibs( '' )
      self.XLINK = ''
      self.CompilerType = 'msvc'

    ############################

    if os.environ["build_debug"]=="1": #name in optset:
      self.XCCFLG += '-O0 '
      self.XCXXFLG += '-O0 '
    else:
      self.XCCFLG += '-O3' #-Ofast '
      self.XCXXFLG += '-O3' #'-Ofast '
        
  ############################################

  def MatchPlatform(self,platform):
    
    if (platform=='any'):
      return True
    else:
      for item in common.msplit(platform):
        if item==self.PLATFORM:
          return True
    return False
  
  def AddFoldersExc(self,folders, excludes, pattern,platform="any"):
    if self.MatchPlatform(platform):
      self.enumerator.AddFoldersExc(folders,excludes,pattern)

  def AddFolders(self,folders,pattern,platform="any"):
    if self.MatchPlatform(platform):
      self.enumerator.AddFolders(folders,pattern)

  def AddIncludePathsQt(self,paths,platform="any"):
    if self.MatchPlatform(platform):
      self.IncludePathsQt += string.split(paths)

  def AddIncludePaths(self,paths,platform="any"):
    if self.MatchPlatform(platform):
      self.IncludePaths += string.split(paths)

  def AddLibPaths(self,paths,platform="any"):
    if self.MatchPlatform(platform):
      self.LibraryPaths += string.split(paths)

  def AddLibsWithSuffix(self,libs,platform="any"):
    if self.MatchPlatform(platform):
      splits = string.split(libs)
      for item in splits:
        self.Libraries.append(item+"."+self.suffix)

  def AddLibs(self,libs,platform="any"):
    if self.MatchPlatform(platform):
      self.Libraries.append(string.split(libs))

  def AddFrameworks(self,libs,platform="any"):
    if self.IsOsx:
      self.Frameworks += string.split(libs)

  def AddCustomObjs(self,objs,platform="any"):
    if self.MatchPlatform(platform):
      self.enumerator.sourceobjs.append(string.split(objs))

  def AddProjectDep(self,project,platform="any"):
    if self.MatchPlatform(platform):
      self.LibraryPaths += project.LibraryPaths
      if project.IsLibrary:
        self.Libraries.append(project.TargetName)
      self.Libraries.append(project.Libraries)

  def AddDefines( self, defs, platform="any" ):
    if self.MatchPlatform(platform):
      self.CustomDefines.append(string.split(defs))

  ############################################

  def AddQt5Modules(self,mods):
      self.BaseEnv.EnableQt5Modules(string.split(mods))

  ############################################

  def AddExtraFiles(self,files):
    if type(files) == list:
      self.additionalfiles += files
    elif type(files) == str:
      self.additionalfiles += string.split(files)

  ############################################

  def SetSrcBase(self,base):
    self.basefolder = os.path.normpath(base)+"/"
    self.BaseEnv.Append( CPPPATH=[self.basefolder] )
    self.BaseEnv.VariantDir( self.BUILD_DIR, self.basefolder, duplicate=0 )
    self.enumerator = SourceEnumerator(self.basefolder,self.BUILD_DIR);
    
  ############################################

  def ComputeSources(self):
    srcs = self.enumerator.sourceobjs
    #sio = StringIO.StringIO()
    #fname = "%s.pickle"%self.OutputName
    #pickle.dump(srcs,sio)
    #print sio.getvalue()
    #sio.close()
    self.cached_sources = srcs

  ############################################

  def GetSources(self):
    return self.cached_sources

  ############################################

  def Configure(self):

    self.ComputeSources()

    libpaths = list(Set(self.PreLibraryPaths))
    libpaths += list(Set(self.LibraryPaths))
    libpaths += list(Set(self.PostLibraryPaths))
    self.LibraryPaths = libpaths

    incpaths = list(Set(self.PreIncludePaths))
    incpaths += list(Set(self.IncludePaths))
    incpaths += list(Set(self.IncludePathsQt))
    incpaths += list(Set(self.PostIncludePaths))
    self.IncludePaths = incpaths

    self.Libraries = list(self.Libraries)
    self.Frameworks = list(Set(self.Frameworks))
 
    self.SetCompilerOptions( self.XDEFS, self.XCCFLG, self.XCXXFLG, self.IncludePaths, self.LibraryPaths, self.XLINK, self.PLATFORM, self.BUILD )
    self.CompileEnv = self.BaseEnv.Clone()
    self.CompileEnv["BUILD_DIR"] = self.BUILD_DIR
    if self.LogConfig:
      print "///////////////////////////////////////////////////////"
      print "Project: OutputName<%s>" % self.OutputName
      print
      sources = self.GetSources()
      #for s in sources:
      # if s.find("string")>=0:
      #   print s
      #print "Sources<%s>" % self.GetSources()
      print "///////////////"
      print "PATH<%s>" % self.CompileEnv['ENV'][ 'PATH' ]
      print "///////////////"
      print "CC<%s>" % self.CompileEnv['CC']
      print "///////////////"
      print "XDEFS<%s>" % self.XDEFS
      print "///////////////"
      print "XCCFLG<%s>" % self.XCCFLG
      print "///////////////"
      print "XCXXFLG<%s>" % self.XCXXFLG
      print "///////////////"
      print "XLINK<%s>" % self.XLINK
      print "///////////////"
      print "CPPDEFINES<%s>" % self.BaseEnv['CPPDEFINES']
      print "///////////////"
      print "INCLPATHS<%s>" % self.IncludePaths
      print "///////////////"
      print "LINKCOM<%s>" % self.CompileEnv['LINKCOM']
      print "///////////////"
      print "BUILDDIR<%s>" % self.BUILD_DIR
      print "///////////////"
      print "BASEDIR<%s>" % self.basefolder
      #print self.CompileEnv.Dump()
      print "///////////////"
      print "SOURCES<%s>" % self.GetSources()
      print "///////////////////////////////////////////////////////"
      print

    self.MayaPlugEnv = self.CompileEnv.Clone()
    self.MayaPlugEnv.Replace(SHLIBPREFIX = '' )
    self.MayaPlugEnv.Replace(SHLIBSUFFIX = '.bundle')

  ############################################

  def SetCompilerOptions( self, XDEFS, XCCFLAGS, XCXXFLAGS, INCPATHS, XLIBPATH, XLINK, PLATFORM, BUILD ):
    Defines = string.split(XDEFS)
    CCFlags = string.split(XCCFLAGS)
    CXXFlags = string.split(XCXXFLAGS)
    self.BaseEnv.Append( CFLAGS = CCFlags )
    self.BaseEnv.Append( CXXFLAGS = CXXFlags )
    self.BaseEnv.Append( CPPDEFINES = Defines )
    self.BaseEnv.Append( CPPDEFINES = self.CustomDefines )
    self.BaseEnv.Append( CPPPATH=INCPATHS )
    self.BaseEnv.Append( LIBS=self.Libraries, LIBPATH=self.LibraryPaths )
    self.BaseEnv.Append( LINK=string.split(XLINK) )
    self.BaseEnv['FRAMEWORKS'] = self.Frameworks

  ############################################

  def Plugin(self,dest,subd,deps):
    self.IsLibrary = True
    lib_dir = '%s/lib' % stage_dir
    libname = '#stage/lib/%s'%self.OutputName
    self.TargetName = self.OutputName

    the_list = list()
    lib = self.CompileEnv.LoadableModule(libname, self.GetSources() )
    the_list += lib


    self.CompileEnv.Append( LINKFLAGS = string.split("-rpath ./" ) )

    basenam = "#stage/plugin/%s"%os.path.basename(dest)
    destdir = os.path.dirname(dest)


    if self.IsOsx:
      subd["%BUNDLE_EXECUTABLE%"] = os.path.basename(str(lib[0]))
      bun = self.CompileEnv.MakeBundle(basenam,lib,"Info.plist",subst_dict=subd)
      the_list += bun
      self.CompileEnv.Alias('install', self.CompileEnv.Install(destdir, basenam))
      for item in deps:
        self.CompileEnv.Install(destdir, item)
      #self.CompileEnv.Depends(target, env['GchSh'])  
    else:
      self.CompileEnv.Alias('install', self.CompileEnv.Install(destdir, lib))
      bun = lib

    return the_list

  ############################################

  def MayaPlugin(self,libname):
    self.IsLibrary = True
    self.TargetName = self.OutputName
    lib = self.MayaPlugEnv.SharedLibrary( libname, self.GetSources() )
    #env = MayaPlugEnv
    return lib

  ############################################

  def SharedLibrary(self):
    self.IsLibrary = True
    lib_dir = '%s/lib' % stage_dir
    libname = '#stage/lib/%s.so'%self.OutputName
    self.TargetName = self.OutputName
    self.CompileEnv.Append( SHLINKFLAGS = string.split("-install_name @executable_path/../lib/lib%s.so" % self.OutputName) )

    self.MocResults = list()

    for item in self.IncludePathsQt:
        mocsources=common.recursive_glob(item,"*.h")
        for sss in mocsources: 
           print sss
           self.MocResults += self.CompileEnv.Moc5( sss )

    lib = self.CompileEnv.SharedLibrary( libname, self.GetSources() + self.ExplicitMocs  )
    env = self.CompileEnv
    #env.Alias('install', env.Install(lib_dir, lib))
    return lib

  def StaticLibrary(self):
    self.IsLibrary = True
    lib_dir = '%s/lib' % stage_dir
    libname = '#stage/lib/%s'%self.OutputName
    self.TargetName = self.OutputName
    lib = self.CompileEnv.StaticLibrary( libname, self.GetSources() )
    env = self.CompileEnv
    #env.Alias('install', env.Install(lib_dir, lib))
    return lib

  def Program(self):

    self.IsExe = True
    bin_dir = '%s/bin' % stage_dir
    #exename = '%s/bin/%s' % (stage_dir,self.OutputName)
    exename = '#stage/bin/%s'%self.OutputName
    self.TargetName = self.OutputName
    #print "exename<%s>" % exename
    prg = self.CompileEnv.Program( exename , self.GetSources() )
    env = self.CompileEnv
    #env.Alias('install', env.Install(bin_dir, prg))
    ret = prg
    return ret

  def Bundle(self,name):
    destdir = '%s/bundle' % stage_dir
    self.IsExe = True
    bin_dir = '%s/bin' % stage_dir
    #exename = '%s/bin/%s' % (stage_dir,self.OutputName)
    exename = '#stage/bin/%s'%self.OutputName
    self.TargetName = self.OutputName
    #print "exename<%s>" % exename
    prg = self.CompileEnv.Program( exename , self.GetSources() )
    env = self.CompileEnv
    #env.Alias('install', env.Install(bin_dir, prg))

    ret = prg

    if self.IsOsx:
      basenam = os.path.basename(str(self.OutputName))
      subd = dict()
      subd["%BUNDLE_NAME%"] = "Yo"
      subd["%BUNDLE_ID%"] = "com.tweakoz.orkid.pro"
      subd["%BUNDLE_EXECUTABLE%"] = os.path.basename(str(self.OutputName))
      bun = self.CompileEnv.MakeBundle(name,prg,"Info.plist",subst_dict=subd,typecode='APPL')
      ret = bun
      self.CompileEnv.Alias('install', self.CompileEnv.Install(destdir, name))
      #for item in deps:
      # self.CompileEnv.Install(destdir, item)
      #self.CompileEnv.Depends(target, env['GchSh'])  
    else:
      self.CompileEnv.Alias('install', self.CompileEnv.Install(destdir, prg))
      ret = prg

    return ret


def xflibnams(fmt,lis):
  a = ""
  for i in string.split(lis):
    a += fmt % i
  return a
