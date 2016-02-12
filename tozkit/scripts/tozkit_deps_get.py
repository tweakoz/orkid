#!/usr/bin/python

import os
import sys
import tozkit_common as tkc
import ork.build.utils as obu

##############################################################################3

#root_dir = os.getcwd()

is_toz_root_set = "TOZ_ROOT" in os.environ
if False==is_toz_root_set:
	print "TOZ_ROOT not set"
	sys.exit(-1)

root_dir=os.environ["TOZ_ROOT"]
stage_dir=os.environ["TOZ_STAGE"]

if False==os.path.exists("%s/scripts"%root_dir):
	print "Are you sure you ran this from TOZ_ROOT?"
	sys.exit(-1)

dl_dir = "%s/downloads" % stage_dir

##############################################################################3

os.system( "mkdir -p %s" % dl_dir)

##############################################################################3

ctx = obu.context(dl_dir)

##############################################################################3

if ctx.opt_apts:
	pkgs =  " libfftw3-dev automake python-pip wget vim vim-gtk swig python-glade2 libglib2.0-dev git mercurial subversion qtcreator python-qt4 "
	pkgs += " libjpeg-turbo8-dev libpng12-dev zlib1g-dev libudev-dev clang g++"
	pkgs += " cmake libtbb-dev libboost1.48-all-dev scons libavcodec-dev "
	pkgs += " libavformat-dev libswscale-dev libncurses5-dev libblocksruntime-dev "
	pkgs += " freeglut3-dev libglew1.6-dev nvidia-cg-toolkit flex bison libconfig++-dev uuid-dev gperf "
	pkgs += " libcurl4-openssl-dev libunittest++-dev libnvtt-dev libmagick++-dev "
	os.system( "sudo apt-get update" )
	os.system( "sudo apt-get install %s" % pkgs )

##############################################################################3

if ctx.opt_exts:
	#ctx.wget( "http://www.3delight.com/downloads/free/%s-Linux-x86_64.tar.gz"%tkc.DELIGHT, "%s.tar.gz"%tkc.DELIGHT, "57c7983e20e322dfe87003ec000d8bbd" )
	ctx.wget( "http://iweb.dl.sourceforge.net/project/blitz/blitz/Blitz++\ 0.10/%s.tar.gz"%tkc.BLITZ,"%s.tar.gz"%tkc.BLITZ,"66268b92bda923735f2e3afc87dcb58a")
	ctx.wget( "http://www.hdfgroup.org/ftp/HDF5/releases/%s/src/%s.tar.bz2"%(tkc.HDF5,tkc.HDF5), "%s.tar.bz2"%tkc.HDF5,"33e105583417eff1c57fff910a53cd6f")
	#ctx.wget( "https://github.com/openexr/openexr/tarball/OPENEXR_1_7_0", "openexr-1.7.0.tar.gz","0081081d0354c633cefeab818180f089" )
	#ctx.wget( "https://github.com/openexr/openexr/tarball/ILMBASE_1_0_2", "ilmbase-1.0.2.tar.gz","637e19f3afe7394bacc0118d25e14c56" )
	#ctx.wget( "https://github.com/downloads/openexr/openexr/pyilmbase-1.0.0-v1.7.tar.gz", "pyilmbase-1.0.0-v1.7.tar.gz","a733fe614862206bdef82d624770caa8" )
	#ctx.wget( "http://sourceforge.net/projects/loki-lib/files/Loki/Loki\ 0.1.7/loki-0.1.7.tar.bz2/download", "loki-0.1.7.tar.bz2","33a24bcbb99fa2ec8fcbbab65649f3f6" )
	#ctx.wget( "http://sourceforge.net/projects/lcms/files/lcms/2.1/lcms2-2.1.tar.gz/download", "lcms2-2.1.tar.gz","08036edb115ad74456dfa20b1998b5f4" )
	#ctx.wget( "http://tweakoz.com/shaderlink.tar.bz2", "shaderlink.tar.bz2","b90a0372c80fd991984df92bbffdc9c4" )
	#ctx.wget( "http://releases.qt-project.org/qt5/%s/single/%s.tar.gz"%(tkc.QT5VER,tkc.QT5), "%s.tar.gz"%tkc.QT5, "8e8137c23c8ff078a560d3c62b4383e7" )

	#ctx.wget( "http://sourceforge.net/projects/boost/files/boost/1.55.0/boost_1_55_0.tar.bz2/download", "boost_1_55_0.tar.bz2", "d6eef4b4cacb2183f2bf265a5a03a354" )
	ctx.wget( "http://sourceforge.net/projects/boost/files/boost/1.59.0/boost_1_59_0.tar.bz2/download", "boost_1_59_0.tar.bz2", "6aa9a5c6a4ca1016edd0ed1178e3cb87" )

	ctx.gitget( "https://github.com/openexr/openexr.git", "oexr", "ecf35bd9c5830fdabd94153f0621e0b99fbfd38b" )
	ctx.gitget( "https://github.com/imageworks/OpenColorIO.git", "ocio", "533f85e" )
	ctx.gitget( "https://github.com/OpenImageIO/oiio.git", "oiio", "50e1b60de27faf39ba8c5f69b7397dcb1097a850" )
	ctx.gitget( "https://github.com/PixarAnimationStudios/OpenSubdiv.git", "osub", "23c488b4cfb7f6a74ea4bfb882b069727cfb2d65" )
	ctx.gitget( "https://github.com/imageworks/OpenShadingLanguage.git", "osl", "be53b0759098916dba310a02f441d59b842145e6" )	
	ctx.gitget( "https://github.com/llvm-mirror/llvm", "llvm", "a4cf325e41fca33c7ce7deef39a7bcf25fb38266" )
	#ctx.gitget( "https://github.com/sambler/osl-shaders") "osl-shaders", "0" )

	#ctx.svnget( "http://cortex-vfx.googlecode.com/svn/trunk", "cortex_vfx", tkc.CORTEX_REV )
	#ctx.gitget( "https://github.com/danbethell/cortex-ocean.git" , "cortex_ocean", "ff20dba" )
	#ctx.hgget( "https://code.google.com/p/alembic/", "alembic", tkc.ALEMBIC_REV )

	os.chdir(root_dir)


