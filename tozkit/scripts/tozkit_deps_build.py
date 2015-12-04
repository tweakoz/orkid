#!/usr/bin/python

import os, sys, glob, string
import tozkit_common as tkc
import ork.build.utils as obu

###########################################

num_cores = obu.num_cores
myexec = obu.myexec
untar = obu.untar

###########################################

ROOT_DIR=os.environ["TOZ_ROOT"]
STAGE_DIR=os.environ["TOZ_STAGE"]
DL_DIR = "%s/downloads"%STAGE_DIR
EB_DIR = "%s/ext_build"%STAGE_DIR

dlctx = obu.context(DL_DIR)

############################################

print "ROOT_DIR<%s>" % ROOT_DIR
print "STAGE_DIR<%s>" % STAGE_DIR
print "DL_DIR<%s>" % DL_DIR
print "EB_DIR<%s>" % EB_DIR

os.system( "mkdir -p %s" % EB_DIR )

###############################################

def chrel(s):
	os.chdir("%s/%s"%(ROOT_DIR,s))
def chstgrel(s):
	ndir = "%s/%s"%(STAGE_DIR,s)
	print( "chdir <%s>" % ndir )
	os.chdir(ndir)

class build_context:
	def __init__(self):
		self.opt_hdf5 = False
		self.opt_loki = False
		self.opt_blitz = False
		self.opt_boost = False
		self.opt_openexr = False
		self.opt_alembic = False
		self.opt_cortex_vfx = False
		self.opt_cortex_ocean = False
		self.opt_gaffer = False
		self.opt_ocio = False
		self.opt_oiio = False
		self.opt_llvm = False
		self.opt_osl = False
		self.opt_clean = False
		self.opt_qt5 = False
		self.opt_fixipaths = False;
		self.nargs = len(sys.argv)
		if 1 == self.nargs:
			print "usage: (all | hdf5 | loki | blitz | boost | oexr | alembic | cortex | ocean | oiio | ocio | llvm | osl | qt5 | fixipaths ) [clean]"
		else:
			for i in range(1,self.nargs,1):
				arg = sys.argv[i]
				if arg=="all":
					self.opt_boost = True
					self.opt_openexr = True
					self.opt_oiio = True
					#self.opt_osl = True
				elif arg=="hdf5":
					self.opt_hdf5 = True
				elif arg=="loki":
					self.opt_loki = True
				elif arg=="blitz":
					self.opt_blitz = True
				elif arg=="oexr":
					self.opt_openexr = True
				elif arg=="alembic":
					self.opt_alembic = True
				elif arg=="cortex":
					self.opt_cortex_vfx = True
				elif arg=="ocean":
					self.opt_cortex_ocean = True
				elif arg=="oiio":
					self.opt_oiio = True
				elif arg=="llvm":
					self.opt_llvm = True
				elif arg=="osl":
					self.opt_osl = True
				elif arg=="qt5":
					self.opt_qt5 = True
				elif arg=="clean":
					self.opt_clean = True
				elif arg=="boost":
					self.opt_boost = True
				elif arg=="fixipaths":
					self.opt_fixipaths = True
		print "opt_clean<%s>" % self.opt_clean

###############################################
# copy to staging build folder
###############################################

ctx = build_context()

chrel("/")

if ctx.opt_clean:
	myexec("rm -rf %s"%EB_DIR)
	myexec("mkdir -p %s"%EB_DIR)
	myexec("rm -rf %s/bin/*"%STAGE_DIR)
	myexec("rm -rf %s/lib/*"%STAGE_DIR)
	myexec("rm -rf %s/include/*"%STAGE_DIR)
	#####################
	chstgrel("ext_build")
	#untar( "%s/%s.tar.gz"%(DL_DIR,tkc.DELIGHT))
	#untar( "%s/%s.tar.bz2"%(DL_DIR,tkc.LOKI))
	#untar( "%s/%s.tar.bz2"%(DL_DIR,tkc.HDF5))
	#untar( "%s/%s.tar.gz"%(DL_DIR,tkc.BLITZ))
	#untar( "%s/%s.tar.gz"%(DL_DIR,tkc.QT5))
	#####################
	#myexec( "rm -rf stage/3dl" )
	#myexec( "mv %s/%s-%s/3delight/%s %s/3dl"%(EB_DIR,tkc.DELIGHT,tkc.DELIGHTARCH,tkc.DELIGHTARCH,STAGE_DIR))
	#####################
	chstgrel("/")
	#untar( "%s/shaderlink.tar.bz2"%DL_DIR)
	#####################
	#copydirs = "alembic cortex_vfx cortex_ocean ocio oiio osub oexr osl"
	#copydirs = "ocio oiio osub oexr osl"
	#####################
	#for item in string.split(copydirs," "):
		#myexec( "cp -r %s/%s %s/%s" % (DL_DIR,item,EB_DIR,item))
	#####################
	# apply patches
	#####################
	#chrel("/")
	#myexec( "cp -r patches/* %s/ext_build/"%STAGE_DIR)
	#####################

if ctx.opt_hdf5:
	chstgrel("ext_build/%s"%tkc.HDF5)
	myexec("./configure --prefix=%s"%STAGE_DIR)
	myexec("make -j %s install"%num_cores )

if ctx.opt_loki:
	chstgrel("ext_build/%s"%tkc.LOKI)
	myexec("make -j %s"%num_cores)
	myexec("cp -r include/loki %s/include/"%STAGE_DIR)
	myexec("cp -r lib/libloki* %s/lib/"%STAGE_DIR)

if ctx.opt_blitz:
	chstgrel("ext_build/%s"%tkc.BLITZ)
	myexec("./configure --with-tbb --with-boost --prefix=%s"%STAGE_DIR)
	myexec("make -j %s install" %num_cores)

#####################################################
# Boost
#####################################################

if ctx.opt_boost:
	#boostver = "1_55_0"
	boostver = "1_59_0"
	chstgrel("ext_build/")
	myexec("rm -rf ./boost*" )
	myexec("rm -rf %s/lib/libboost*"%STAGE_DIR)
	myexec("rm -rf %s/include/boost*"%STAGE_DIR)
	myexec( "tar xvfj %s/boost_%s.tar.bz2" % (DL_DIR,boostver) )
	chstgrel("ext_build/boost_%s"%boostver)
	myexec("./bootstrap.sh" )
	myexec("./b2 clean" )
	myexec( './b2 -j16 install toolset=clang cxxflags="-stdlib=libc++" linkflags="-stdlib=libc++" --prefix="%s" ' % STAGE_DIR )

chstgrel("lib/")
paths = glob.glob("libboost*.dylib")

for p in paths:
 if os.path.isfile(p):
  obu.osx_dylib_setid(p)
obu.osx_dylib_changeref("libboost_thread.dylib","libboost_system.dylib")
obu.osx_dylib_changeref("libboost_filesystem.dylib","libboost_system.dylib")

#####################################################
# IlmBase and OpenEXR
#####################################################

if ctx.opt_openexr:
	chstgrel("ext_build/")
	myexec("rm -rf oexr")
	myexec("rm -rf %s/lib/libIlm*"%STAGE_DIR)
	myexec("rm -rf %s/lib/libIex*"%STAGE_DIR)
	myexec("rm -rf %s/lib/libImath*"%STAGE_DIR)
	myexec("rm -rf %s/lib/libHalf*"%STAGE_DIR)
	myexec("rm -rf %s/lib/pkgconfig/OpenEXR.pc"%STAGE_DIR)
	myexec("rm -rf %s/lib/pkgconfig/IlmBase.pc"%STAGE_DIR)
	myexec("rm -rf %s/include/OpenEXR"%STAGE_DIR)
	myexec( "cp -r %s/oexr %s/oexr" % (DL_DIR,EB_DIR))
	#untar( "%s/%s.tar.gz"%(DL_DIR,tkc.OPENEXR),True)

	os.environ["CC"]="clang"
	os.environ["CXX"]="clang++ -std=c++11 -stdlib=libc++ -Dregister="

	# ilmbase
	chstgrel("ext_build/oexr/IlmBase")
	myexec("./bootstrap")
	myexec("./configure --prefix=%s"%STAGE_DIR)
	myexec("make -j %s install" %num_cores )

	# exr
	chstgrel("ext_build/oexr/OpenEXR")
	myexec("./bootstrap")
	myexec("./configure --prefix=%s"%STAGE_DIR)
	myexec("make -j %s install" %num_cores)

#####################################################
# OpenImageIO
#####################################################

if ctx.opt_oiio:
	#ctx.gitget( "https://github.com/OpenImageIO/oiio.git", "oiio", "3800e097dd2be3af8027f387d58a92b2a75c5379" )
	chstgrel("ext_build/")
	myexec("rm -rf oiio")
	myexec( "cp -r %s/oiio %s/oiio" % (DL_DIR,EB_DIR))
	myexec( "cp -r %s/patches/oiio/* %s/oiio/" % (ROOT_DIR, EB_DIR) ) 
	chstgrel("ext_build/oiio")
	os.environ["CC"]="clang"
	os.environ["CXX"]="clang++ -std=c++11 -stdlib=libc++ -Wno-unused-local-typedefs"
	os.environ["CMAKE_INCLUDE_PATH"]="%s/include"%STAGE_DIR
	os.environ["CMAKE_LIBRARY_PATH"]="%s/lib"%STAGE_DIR
	#myexec('make -j %s VERBOSE=1 CXX_FLAGS="-std=c++11 -stdlib=libc++"'%num_cores)
	myexec('make -j %s VERBOSE=1 '%num_cores)
	if obu.IsDarwin():
		oiionam = "libOpenImageIO.1.5.dylib"
		oiiounam = "libOpenImageIO_Util.1.5.dylib"
		myexec("install_name_tool -id @executable_path/../lib/%s ./dist/macosx/lib/%s" % (oiionam,oiionam) )
		myexec("install_name_tool -id @executable_path/../lib/%s ./dist/macosx/lib/%s" % (oiiounam,oiiounam) )
		myexec("rsync -ravE ./dist/macosx/* %s/" % STAGE_DIR)
	else:
		myexec("rsync -ravE ./dist/linux64/* %s/" % STAGE_DIR)

if ctx.opt_fixipaths:
 print "yo"

#####################################################
# OpenColorIO
#####################################################

if ctx.opt_ocio:
	chstgrel("/")
	myexec("mkdir -p ocio_build")
	chrel("ocio_build")
	myexec("cmake -DCMAKE_INSTALL_PREFIX=/projects/tweakoz/tozkit/stage -DOIIO_PATH=%s ../imageworks-OpenColorIO-533f85e/"%STAGE_DIR)

#####################################################
# OpenShadingLanguage
#####################################################

if ctx.opt_llvm:
	chstgrel("/")
	myexec("rm -rf %s/include/llvm*" % STAGE_DIR )
	myexec("rm -rf %s/lib/libLLVM*" % STAGE_DIR )
	myexec("rm -rf %s/bin/ll*" % STAGE_DIR )
	myexec("rm -rf %s/share/llvm" % STAGE_DIR )
	CLDIR = "%s/clang+llvm-3.5.2-x86_64-apple-darwin" % DL_DIR
	myexec("cp -rf %s/bin/* %s/bin/" % (CLDIR,STAGE_DIR))
	myexec("cp -rf %s/lib/* %s/lib/" % (CLDIR,STAGE_DIR))
	myexec("cp -rf %s/include/* %s/include/" % (CLDIR,STAGE_DIR))
	myexec("cp -rf %s/share/llvm %s/share/" % (CLDIR,STAGE_DIR))

#if ctx.opt_llvm:
#	chstgrel("/")
#	myexec("rm -rf %s/include/llvm*" % STAGE_DIR )
#	myexec("rm -rf %s/lib/libLLVM*" % STAGE_DIR )
#	myexec("rm -rf %s/bin/ll*" % STAGE_DIR )
#	chstgrel("ext_build/")
#	myexec("mkdir -p llvm")
#	chstgrel("ext_build/llvm")

#	os.environ["CMAKE_INCLUDE_PATH"]="%s/include"%STAGE_DIR
#	os.environ["CMAKE_LIBRARY_PATH"]="%s/lib"%STAGE_DIR
#	cmakevars =  '-DLLVM_TARGETS_TO_BUILD="X86" ' #;AArch64;ARM;Mips;NVPTX"
#	cmakevars += '-DCMAKE_INSTALL_PREFIX="%s"'%STAGE_DIR
#	myexec('cmake %s %s/llvm/'%(cmakevars,DL_DIR))
#	myexec("make -j %s install" %num_cores )

if ctx.opt_osl:
	chstgrel("/")
	myexec("rm -rf shaders")
	chstgrel("ext_build/")
	myexec("rm -rf osl")
	myexec( "cp -r %s/osl %s/osl" % (DL_DIR,EB_DIR))
	chstgrel("ext_build/osl")
	os.environ["CMAKE_INCLUDE_PATH"]="%s/include"%STAGE_DIR
	os.environ["CMAKE_LIBRARY_PATH"]="%s/lib"%STAGE_DIR
	os.environ["CMAKE_OSX_SYSROOT"]="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk"
	os.environ["EXTRA_OSLEXEC_LIBRARIES"]="curses"
	os.environ["OPENIMAGEIO_LIBRARY"]="%s/lib/libOpenImageIO.dylib"%STAGE_DIR
	#os.environ["BOOST_ROOT"]=
	os.environ["OPENIMAGEIOHOME"] = STAGE_DIR
	os.environ["USE_CPP11"]="1"
	os.environ["LLVM_STATIC"]="1"
	os.environ["BUILDSTATIC"]="1"
	os.environ["LINKSTATIC"]="1"
	os.environ["USE_LIBCPLUSPLUS"]="1"
	os.environ["VERBOSE"]="1"
	os.environ["OSL_SITE"] ="%s/patches/osl/oslsite_toz"%ROOT_DIR
	#os.environ["LLVM_DIRECTORY"] = STAGE_DIR
	#os.environ["LLVM_INCLUDES"="%s/include/llvm/"%STAGE_DIR
	myexec("make" )
	myexec("rm -rf %s/shaders" % STAGE_DIR )
	myexec("cp -r %s/osl/src/shaders %s/shaders" % (EB_DIR,STAGE_DIR))
	#myexec("rsync -ravE ./dist/linux64/* %s/" % STAGE_DIR)



#####################################################

if ctx.opt_alembic:
	if False==os.path.exists("%s/ext_build/alembic_build"%STAGE_DIR):
		chstgrel("ext_build/alembic")
		myexec("sed s,/alembic-\${VERSION},, CMakeLists.txt > yo.txt")
		myexec("cp yo.txt CMakeLists.txt")
		chstgrel("ext_build")
		myexec("mkdir -p alembic_build")
		chstgrel("ext_build/alembic_build")
		bootstrap_cmd = "python ../alembic/build/bootstrap/alembic_bootstrap.py "
		bootstrap_cmd += "--hdf5_include_dir=%s/include/ " % STAGE_DIR
		bootstrap_cmd += "--hdf5_hdf5_library=%s/lib/libhdf5.so " % STAGE_DIR
		bootstrap_cmd += "--boost_include_dir=/usr/include "
		bootstrap_cmd += "--boost_thread_library=/usr/lib/libboost_thread-mt.so "
		bootstrap_cmd += "--ilmbase_include_dir=%s/include/OpenEXR " % STAGE_DIR
		bootstrap_cmd += "--ilmbase_imath_library=%s/lib/libImath.so " % STAGE_DIR
		bootstrap_cmd += "--zlib_include_dir=/usr/include "
		bootstrap_cmd += "--zlib_library=/usr/lib/x86_64-linux-gnu/libz.so"
		myexec(bootstrap_cmd)
		myexec("sed s,/usr/local,%s, CMakeCache.txt > yocache"%STAGE_DIR)
		myexec("cp yocache CMakeCache.txt")
	chstgrel("ext_build/alembic_build")
	myexec("make -j %s install" %num_cores )

if ctx.opt_cortex_vfx:
	chstgrel("ext_build/cortex_vfx")
	myexec("scons --jobs=%s install"%num_cores)

if ctx.opt_cortex_ocean:
	chstgrel("ext_build/cortex_ocean")
	myexec('cmake -G "Unix Makefiles"')
	myexec("make")
	myexec("make install")



if ctx.opt_gaffer:
	chrel("/")
	chrel("gaffer_exp")
	myexec("scons --jobs=%s install"%num_cores)

if ctx.opt_qt5:
	chstgrel("ext_build/%s"%tkc.QT5)
	myexec("./configure --prefix=%s -fatal_error -opensource -confirm-license -debug -developer-build -no-gtkstyle -opengl desktop -no-openvg -qt-pcre -no-eglfs -no-directfb -no-linuxfb -no-kms"%STAGE_DIR)
	myexec("make -j %s install" % obu.num_cores )

#chrel("/rihelper")
#myexec("make")
