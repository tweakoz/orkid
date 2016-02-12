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
		self.opt_loki = False
		self.opt_cortex_vfx = False
		self.opt_cortex_ocean = False
		self.opt_gaffer = False
		self.opt_clean = False
		self.opt_fixipaths = False;
		self.nargs = len(sys.argv)
		if 1 == self.nargs:
			print "usage: (all | loki | cortex | ocean | fixipaths ) [clean]"
		else:
			for i in range(1,self.nargs,1):
				arg = sys.argv[i]
				if arg=="loki":
					self.opt_loki = True
				elif arg=="cortex":
					self.opt_cortex_vfx = True
				elif arg=="ocean":
					self.opt_cortex_ocean = True
				elif arg=="clean":
					self.opt_clean = True
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

if ctx.opt_loki:
	chstgrel("ext_build/%s"%tkc.LOKI)
	myexec("make -j %s"%num_cores)
	myexec("cp -r include/loki %s/include/"%STAGE_DIR)
	myexec("cp -r lib/libloki* %s/lib/"%STAGE_DIR)

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

#chrel("/rihelper")
#myexec("make")
