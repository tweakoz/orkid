#!/usr/bin/python

import os
import sys
import multiprocessing

num_cores = multiprocessing.cpu_count()

###########################################
ROOT_DIR=os.environ["TOZ_ROOT"]
STAGE_DIR="%s/stage"%ROOT_DIR
############################################

print "ROOT_DIR<%s>" % ROOT_DIR
print "STAGE_DIR<%s>" % STAGE_DIR

#############################################
def myexec(s):
 print "exec<%s>" % s
 os.system(s)

def chrel(s):
	os.chdir("%s/%s"%(ROOT_DIR,s))
###############################################
chrel("/")

#################################
# PATCHES
###################################

myexec("cp patches/ImathMatrix.h ilmbase-1.0.2/Imath/")
myexec("cp patches/blurImage.cpp openexr-1.7.0/exrenvmap/")
myexec("cp patches/cortex.sconstruct cortex_exp/SConstruct")
myexec("cp patches/gaffer.sconstruct gaffer_exp/SConstruct")
myexec("cp -r patches/cortex/* ./cortex_exp/")

