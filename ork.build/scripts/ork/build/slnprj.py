###############################################################################
# Orkid SCONS Build System
# Copyright 2010, Michael T. Mayers
# email: michael@tweakoz.com
# The Orkid Build System is published under the GPL 2.0 license
# see http://www.gnu.org/licenses/gpl-2.0.html
###############################################################################
import glob, re, string
import commands, sys, os
import shutil, fnmatch, platform
from sets import Set

import ork.build.utils as utils
import ork.build.common as common

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

__all__ =	[
	"SetCompilerOptions", "SourceEnumerator", "Project"
	]
	
optset = set()

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
		#if "" in sourcefiles:
		#	print folders
		#	print pattern
		#	print excludes
			
		self.sourceobjs  += common.builddir_replace( sourcefiles, self.basefolder, self.BUILD_DIR )

	def AddFolders(self,folders, pattern):
		srclist = string.split(folders)
		exclist = string.split('')
		sourcefiles = common.globber( self.basefolder, pattern, srclist , exclist)
		#if "" in sourcefiles:
		#	print "yo1" , folders
		#	print "yo1" , pattern
		#	print "yo1" , excludes
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
    print( "building target<%s>" % join([str(x) for x in target]) )


###############################################################################

def BuildSuffix(ARGUMENTS):
	PLATFORM = ARGUMENTS['PLATFORM']
	BUILD = ARGUMENTS['BUILD']
	BUILDNAME = "%s.%s" % (PLATFORM,BUILD)
	return BUILDNAME

###############################################################################

class Project:

	############################################

	def __init__(self,ARGUMENTS,Environment,name):
		
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
		#print "\nBUILDDIR<%s>\n"%self.BUILD_DIR
		##################################
		self.BaseEnv = Environment.Clone()
		self.BaseEnv.Replace( CCCOMSTR = "\x1b[35m Compiling \x1b[33m $SOURCE \x1b[32m" ) #% (name,self.BUILD,self.PLATFORM) )
		self.BaseEnv.Replace( CXXCOMSTR = self.BaseEnv['CCCOMSTR'])
		self.BaseEnv.Replace( SHCXXCOMSTR = self.BaseEnv['CCCOMSTR'])
		self.BaseEnv.Replace( ARCOMSTR = '\x1b[36m Archiving \x1b[37m $TARGET \x1b[32m' )
		self.BaseEnv.Replace( LINKCOMSTR = '\x1b[36m Linking \x1b[37m $TARGET \x1b[32m' )
		self.BaseEnv.Replace( SHLINKCOMSTR = "\x1b[35m Linking LIB \x1b[33m $TARGET \x1b[32m" )
  		#self.BaseEnv['PRINT_CMD_LINE_FUNC'] = CommandPrinter
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
		self.PostIncludePaths = list()
		##############
		self.PreLibraryPaths = string.split("%s/lib"%stage_dir)
		self.LibraryPaths = list()
		self.PostLibraryPaths = list()
		##############

		#if os.environ.has_key("PRJ_LIBDIRS"):
		#	self.LibraryPaths += string.split(os.environ["PRJ_LIBDIRS"])			

		self.Libraries = string.split('')
		self.Frameworks = string.split('')
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

		if name in optset:
			self.XCCFLG += '-O3 '
			self.XCXXFLG += '-O3 '
		else:
			self.XCCFLG += '-O0 '
			self.XCXXFLG += '-O0 '
				
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

	def AddIncludePaths(self,paths,platform="any"):
		if self.MatchPlatform(platform):
			self.IncludePaths += string.split(paths)

	def AddLibPaths(self,paths,platform="any"):
		if self.MatchPlatform(platform):
			self.LibraryPaths += string.split(paths)

	def AddLibs(self,libs,platform="any"):
		if self.MatchPlatform(platform):
			self.Libraries += string.split(libs)

	def AddFrameworks(self,libs,platform="any"):
		if self.IsOsx:
			self.Frameworks += string.split(libs)

	def AddCustomObjs(self,objs,platform="any"):
		if self.MatchPlatform(platform):
			self.enumerator.sourceobjs += string.split(objs)

	def AddProjectDep(self,project,platform="any"):
		if self.MatchPlatform(platform):
			self.LibraryPaths += project.LibraryPaths
			self.Libraries += project.Libraries
			if project.IsLibrary:
				self.Libraries.append(project.TargetName)

	def AddDefines( self, defs, platform="any" ):
		if self.MatchPlatform(platform):
			self.CustomDefines += string.split(defs)

	############################################

	def SetSrcBase(self,base):
		self.basefolder = os.path.normpath(base)+"/"
		self.BaseEnv.Append( CPPPATH=[self.basefolder] )
		self.BaseEnv.VariantDir( self.BUILD_DIR, self.basefolder, duplicate=0 )
		self.enumerator = SourceEnumerator(self.basefolder,self.BUILD_DIR);
		
	############################################

	def GetSources(self):
		return self.enumerator.sourceobjs

	############################################

	def qt_enable(self):

	   self.BaseEnv['QTDIR'] = stage_dir
	   self.BaseEnv['QT4DIR'] = stage_dir
	   self.BaseEnv.Tool('qt4')
	   self.BaseEnv.Append(CXXFLAGS=['-fPIC',"-g"])  #  or  whatever
	   self.BaseEnv.Append(CXXFLAGS=['-DQT_NO_KEYWORDS'])  #  needed if compiling with boost
	   #self.add_header_paths(" ./stage/include/".join( "Qt QtWidgets QtGui QtCore".split()))
	   self.BaseEnv['QT4_DEBUG'] = 1
	   self.BaseEnv.Append(LINKFLAGS="-m64 -Wl,--no-undefined")
	   self.BaseEnv['QT4_AUTOSCAN_STRATEGY'] = 1

	def qt_disable_autoscan(self):
	  self.BaseEnv['QT4_AUTOSCAN'] = 0

	def qt_add_mod(self,str):
	   lis = ["Qt5" + s for s in str.split()]
	   self.BaseEnv.EnableQt4Modules( lis )
	
	def qt_add_qrc(self,str):
  	  qrccc = self.BaseEnv.Qrc4(str.split())
  	  self.objects += qrccc

	def qt_add_uic(self,str):
	   self.BaseEnv.Uic4(str.split())

	def qt_moc_explicit(self,a,b):
	   self.objects += self.BaseEnv.ExplicitMoc4(a,b)

 	def qt_moc(self,a,b):
   	   self.BaseEnv.Moc4(a,b)

   	############################################

	def Configure(self):


		libpaths = list(Set(self.PreLibraryPaths))
		libpaths += list(Set(self.LibraryPaths))
		libpaths += list(Set(self.PostLibraryPaths))
		self.LibraryPaths = libpaths

		incpaths = list(Set(self.PreIncludePaths))
		incpaths += list(Set(self.IncludePaths))
		incpaths += list(Set(self.PostIncludePaths))
		self.IncludePaths = incpaths

		self.Libraries = list(Set(self.Libraries))
		self.Frameworks = list(Set(self.Frameworks))
 
		self.SetCompilerOptions( self.XDEFS, self.XCCFLG, self.XCXXFLG, self.IncludePaths, self.LibraryPaths, self.XLINK, self.PLATFORM, self.BUILD )
		self.CompileEnv = self.BaseEnv.Clone()

		if self.LogConfig:
			print "///////////////////////////////////////////////////////"
			print "Project: OutputName<%s>" % self.OutputName
			print
			sources = self.GetSources()
			#for s in sources:
			#	if s.find("string")>=0:
			#		print s
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

	def SharedLibrary(self):
		self.IsLibrary = True
		lib_dir = '%s/lib' % stage_dir
		libname = '#stage/lib/%s.so'%self.OutputName
		self.TargetName = self.OutputName
		lib = self.CompileEnv.SharedLibrary( libname, self.GetSources() )
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
		return prg

def xflibnams(fmt,lis):
	a = ""
	for i in string.split(lis):
		a += fmt % i
	return a
