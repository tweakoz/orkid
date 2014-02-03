#!/usr/bin/python

import os
import string
import sys
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

############################################

print "ROOT_DIR<%s>" % ROOT_DIR
print "STAGE_DIR<%s>" % STAGE_DIR
print "DL_DIR<%s>" % DL_DIR
print "EB_DIR<%s>" % EB_DIR

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
		self.opt_ilmbase = False
		self.opt_openexr = False
		self.opt_alembic = False
		self.opt_cortex_vfx = False
		self.opt_cortex_ocean = False
		self.opt_gaffer = False
		self.opt_ocio = False
		self.opt_oiio = False
		self.opt_osl = False
		self.opt_clean = False
		self.opt_qt5 = False
		self.nargs = len(sys.argv)
		if 1 == self.nargs:
			print "usage: (all | hdf5 | loki | blitz | ilmbase | openexr | alembic | cortex | ocean | oiio | ocio | osl | qt5 ) [clean]"
		else:
			for i in range(1,self.nargs,1):
				arg = sys.argv[i]
				if arg=="all":
					self.opt_qt5 = True
					self.opt_hdf5 = True
					self.opt_loki = True
					self.opt_blitz = True
					self.opt_ilmbase = True
					self.opt_openexr = True
					self.opt_alembic = True
					self.opt_cortex_vfx = True
					self.opt_cortex_ocean = True
					#self.opt_gaffer = True
					#self.opt_ocio = True
					self.opt_oiio = True
					self.opt_osl = True
				elif arg=="hdf5":
					self.opt_hdf5 = True
				elif arg=="loki":
					self.opt_loki = True
				elif arg=="blitz":
					self.opt_blitz = True
				elif arg=="ilmbase":
					self.opt_ilmbase = True
				elif arg=="openexr":
					self.opt_openexr = True
				elif arg=="alembic":
					self.opt_alembic = True
				elif arg=="cortex":
					self.opt_cortex_vfx = True
				elif arg=="ocean":
					self.opt_cortex_ocean = True
				elif arg=="oiio":
					self.opt_oiio = True
				elif arg=="osl":
					self.opt_osl = True
				elif arg=="qt5":
					self.opt_qt5 = True
				elif arg=="clean":
					self.opt_clean = True
		print "opt_clean<%s>" % self.opt_clean

###############################################
# copy to staging build folder
###############################################

ctx = build_context()

if ctx.opt_clean:
	chrel("/")
	myexec("rm -rf %s"%EB_DIR)
	myexec("mkdir -p %s"%EB_DIR)
	myexec("rm -rf %s/bin/*"%STAGE_DIR)
	myexec("rm -rf %s/lib/*"%STAGE_DIR)
	myexec("rm -rf %s/include/*"%STAGE_DIR)
	#####################
	chstgrel("ext_build")
	untar( "%s/%s.tar.gz"%(DL_DIR,tkc.DELIGHT))
	untar( "%s/%s.tar.bz2"%(DL_DIR,tkc.LOKI))
	untar( "%s/%s.tar.bz2"%(DL_DIR,tkc.HDF5))
	untar( "%s/%s.tar.gz"%(DL_DIR,tkc.BLITZ))
	untar( "%s/%s.tar.gz"%(DL_DIR,tkc.ILMBASE),True)
	untar( "%s/%s.tar.gz"%(DL_DIR,tkc.OPENEXR),True)
	untar( "%s/%s.tar.gz"%(DL_DIR,tkc.QT5))
	#####################
	myexec( "rm -rf stage/3dl" )
	myexec( "mv %s/%s-%s/3delight/%s %s/3dl"%(EB_DIR,tkc.DELIGHT,tkc.DELIGHTARCH,tkc.DELIGHTARCH,STAGE_DIR))
	#####################
	chstgrel("/")
	untar( "%s/shaderlink.tar.bz2"%DL_DIR)
	#####################
	copydirs = "alembic cortex_vfx cortex_ocean ocio oiio osl"
	#####################
	for item in string.split(copydirs," "):
		myexec( "cp -r %s/%s %s/%s" % (DL_DIR,item,EB_DIR,item))
	#####################
	# apply patches
	#####################
	chrel("/")
	myexec( "cp -r patches/* %s/ext_build/"%STAGE_DIR)
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

if ctx.opt_ilmbase:
	chstgrel("ext_build/IlmBase")
	myexec("./bootstrap")
	myexec("./configure --prefix=%s"%STAGE_DIR)
	myexec("make -j %s install" %num_cores )
	chrel("/")
	myexec("ln -s %s/include/OpenEXR/ImathExc.h %s/include/OpenEXR/ImathFloatExc.h"%(STAGE_DIR,STAGE_DIR))

if ctx.opt_openexr:
	chstgrel("ext_build/OpenEXR")
	myexec("./bootstrap")
	myexec("./configure --prefix=%s"%STAGE_DIR)
	myexec("make -j %s install" %num_cores)

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

if ctx.opt_oiio:
	chstgrel("ext_build/oiio")
	myexec("make -j %s"%num_cores)
	myexec("rsync -ravE ./dist/linux64/* %s/" % STAGE_DIR)

if ctx.opt_ocio:
	chstgrel("/")
	myexec("mkdir -p ocio_build")
	chrel("ocio_build")
	myexec("cmake -DCMAKE_INSTALL_PREFIX=/projects/tweakoz/tozkit/stage -DOIIO_PATH=%s ../imageworks-OpenColorIO-533f85e/"%STAGE_DIR)

if ctx.opt_osl:
	chstgrel("ext_build/osl")
	myexec("make -j %s" %num_cores )
	myexec("rsync -ravE ./dist/linux64/* %s/" % STAGE_DIR)

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
