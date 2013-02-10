###############################################################################
# Orkid SCONS Build System
# Copyright 2010, Michael T. Mayers
# email: michael@tweakoz.com
# The Orkid Build System is published under the GPL 2.0 license
# see http://www.gnu.org/licenses/gpl-2.0.html
###############################################################################

import os, sys
import shutil, glob
import platform
import subprocess
import os
import errno
import time
import sys
import multiprocessing
import xml.etree.ElementTree as xml
import hashlib
from ork.build.manifest import *
from ork.build.pathtools import *

if subprocess.mswindows:
    from win32file import ReadFile, WriteFile
    from win32pipe import PeekNamedPipe
    import msvcrt
else:
    import select
    import fcntl

num_cores = multiprocessing.cpu_count()
stage_dir = os.environ["ORKDOTBUILD_STAGE_DIR"]

###############################################################################

#MARK: yo

SYSTEM = platform.system()

def IsCygwin():
	return SYSTEM.lower().find( "cygwin" )!=-1

def IsWindows():
	iswin = SYSTEM.lower().find( "windows" )!=-1
	ismso = SYSTEM.lower().find( "microsoft" )!=-1
	return iswin or ismso

###########################################

def set_env(key,val):
	print "Setting var<%s> to<%s>" % (key,val)
	os.environ[key]	= val

###########################################

def prepend_env(key,val):
	if False==(key in os.environ):
		set_env(key,val)
	else:
		os.environ[key]	= val + ":" + os.environ[key]
		print "Setting var<%s> to<%s>" % (key,os.environ[key])

###########################################

def append_env(key,val):
	if False==(key in os.environ):
		set_env(key,val)
	else:
		os.environ[key]	= os.environ[key]+":"+val 
		print "Setting var<%s> to<%s>" % (key,os.environ[key])

 #########################

def install_headers(dir):
	print "Installing headers from <%s> to <%s/include/>" % (dir,stage_dir)
	os.system( "cp -rf %s %s/include/" % (dir,stage_dir) )

def install_files(pth,dst):
	print "Installing files from <%s> to <%s/%s/>" % (pth,stage_dir,dst)
	os.system( "mkdir -p %s/%s" % (stage_dir,dst) )
	os.system( "cp -rf %s %s/%s" % (pth,stage_dir,dst) )

###########################################

def folder_tree(rootpath):
	ret = list()
	ret.append(rootpath)
	#print "folder_tree<%s>"%rootpath
	for root, subFolders, files in os.walk(rootpath):
		for folder in subFolders:
			f = os.path.join(root,folder)
			ret.append(f)
			#print(f)
	return ret

###########################################

def install_tree(base,folders,pattern,dest_base):
	for fol in folders:
		spl = fol.split(base)
		sub_dir = (spl[1])[1:]
		install_files("%s/%s/%s"%(base,sub_dir,pattern),"%s/%s"%(dest_base,sub_dir))

###########################################
# check for ork.build projects
###########################################

def check_for_project(path):
	rval = None
	prj_manifest = "%s/ork.build.manifest"%path
	prj_scripts = os.path.abspath("%s/scripts"%path)
	print "checking for project at<%s>" % path
	if os.path.exists(prj_manifest):
		print "/////////////////////////////////////////"
		print "// Projects Found !! <%s>" % path
		print "/////////////////////////////////////////"
		###############
		prj = manifests.add_project(path)
		manifest_tree = xml.parse(prj_manifest)
		###############
		#print manifest_tree
		rootElement = manifest_tree.getroot()
		prj.manifest_root = rootElement
		for a in rootElement.findall('depends_on'):
			#print dir(a)
			depends = a.attrib["project"]
			print "project<%s> depends<%s>" % (path,depends)

			manifests.depends(path,depends)
		###############
		prj.autoexecs = list()
		if os.path.exists(prj_scripts):
			sys.path.append(prj_scripts)
			append_env("PATH",prj_scripts)
			prj.scripts_folder = prj_scripts
			###############
			for a in rootElement.findall('autoexec'):
				scr = a.attrib["script"]
				scrpath = "%s/setenv.py" % prj_scripts
				if os.path.exists(scrpath):
					prj.autoexecs.append(scrpath)
		###############
		#prj.libdir = os.path.abspath("%s/lib " % path)
		rval = prj
	return rval
###########################################
def check_for_projects(base):
	manifests.add_project("%s/ork.build"%base)
	paths = glob.glob("%s/*"%base)
	PRJ_LIBDIRS=" "
	setenv_scrs = list()
	for p in paths:
		if p!="scripts":
			if os.path.isdir(p):
				prj = check_for_project(p)
				if prj!=None:
					if hasattr(prj,"libdir"):
						PRJ_LIBDIRS += prj.libdir
					if hasattr(prj,"autoexecs"):
						for a in prj.autoexecs:
							print "execute autoexec<%s>" % a
							setenv_scrs.append(a)
	#append_env("PRJ_LIBDIRS",PRJ_LIBDIRS)
	for i in setenv_scrs:
		print "exec child setenv <%s>" % i
		execfile(i)
	#print "PRJ_LIBDIRS<%s>" % PRJ_LIBDIRS
############################
def myexec(s):
	#print "exec<%s>" % s
	os.system(s)
############################
def untar(arc,strip=False):
	is_gz = arc.find(".gz")>0
	is_bz2 = arc.find(".bz2")>0
	opts = "--extract --verbose "
	if is_gz:
		opts += "--ungzip "
	elif is_bz2:
		opts += "--bzip2 "
	if strip:
		opts += " --strip-components=1 "
	opts += "-f %s" % arc
	os.system("tar %s"%opts)
#############################################
class context:
	############################
	def __init__(self,dl_dir):
		self.dl_dir = dl_dir
		self.opt_apts = False
		self.opt_exts = False
		self.opt_force = False

		self.nargs = len(sys.argv)
		if 1 == self.nargs:
			print "usage: (all | apts | exts | assets) [force]"
		else:
			for i in range(1,self.nargs,1):
				arg = sys.argv[i]
				if arg=="all":
					self.opt_apts = True
					self.opt_exts = True
				elif arg=="apts":
					self.opt_apts = True
				elif arg=="exts":
					self.opt_exts = True
				elif arg=="force":
					self.opt_force = True
		print "opt_force<%s>" % self.opt_force
	############################
	def md5_of_file(self, fname):
		fil = open(fname,"rb")
		data = fil.read()
		fil.close()
		md5obj = hashlib.md5()
		md5obj.update(data)
		return md5obj.hexdigest()
	############################
	def wget(self,url,outputname,desmd5):
		out_path = "%s/%s" % (self.dl_dir,outputname)
		actmd5 = False
		if os.path.exists(out_path):
			actmd5 = self.md5_of_file(out_path)
		if actmd5!=desmd5:  
			cmd = "wget %s -O %s" % (url,out_path)
			os.system(cmd)
			actmd5 = self.md5_of_file(out_path)
		print "<%s> DesiredMD5<%s>" % (outputname,desmd5)
		print "<%s> ActualMD5<%s>" % (outputname,actmd5)
	############################
	def gitget( self, url, dir, rev=None ):
		os.chdir(self.dl_dir)
		if self.opt_force:
			os.system( "rm -rf %s" % dir )
		exists = os.path.exists(dir)
		if exists:
			os.chdir(dir)
			os.system("git pull origin master")
		else:
			os.system("git clone %s %s" % (url,dir))
			os.chdir(dir)
		if rev!=None:
			os.system( "git checkout %s" % rev )
	############################
	def svnget( self, url, dir, rev="HEAD" ):
		os.chdir(self.dl_dir)
		if self.opt_force:
			os.system( "rm -rf %s" % dir )
		exists = os.path.exists(dir)
		if False==exists:
			os.system("svn checkout %s %s" % (url,dir))
		os.chdir(dir)
		os.system("svn up -r %s" % rev )
	############################
	def hgget( self, url, dir, rev="" ):
		os.chdir(self.dl_dir)
		if self.opt_force:
			os.system( "rm -rf %s" % dir )
		exists = os.path.exists(dir)
		if False==exists:
			os.system( "hg clone %s %s" % (url,dir) )
		os.chdir(dir) 	
		os.system("hg update %s" % rev)
	############################
