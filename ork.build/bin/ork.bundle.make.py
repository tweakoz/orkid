#!/usr/bin/python

import os, string

#print "yo"

bundle_name = "OrkidTool.app"

stage_dir = os.environ["ORKDOTBUILD_STAGE_DIR"]
bundle_dir = "%s/bundle" % stage_dir
root_dir = "%s/../" % stage_dir
tool_dir = "%s/ork.tool" % root_dir
lib_dir = "%s/%s/lib" % (bundle_dir,bundle_name)
cont_dir = "%s/%s/Contents" % (bundle_dir,bundle_name)
fw_dir = "%s/%s/Frameworks" % (bundle_dir,bundle_name)

print "root_dir<%s>" % root_dir
print "stage_dir<%s>" % stage_dir
print "bundle_dir<%s>" % bundle_dir
print "tool_dir<%s>" % tool_dir
print "lib_dir<%s>" % lib_dir
print "cont_dir<%s>" % cont_dir
print "fw_dir<%s>" % fw_dir

###################################

cmd_rm = "rm -rf %s/*" % (bundle_dir)
print "cmd_rm<%s>" % cmd_rm
os.system(cmd_rm)

cmd_cp = "cp -r %s/%s %s/" % (tool_dir,bundle_name,bundle_dir)
print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

cmd_cp = "cp -r %s/lib %s" % (stage_dir,cont_dir)
print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

cmd_cp = "cp -r %s/data %s" % (root_dir,cont_dir)
print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

cmd_cp = "cp -R %s/bin/ork.tool.test.osx.release %s/MacOS" % (stage_dir,cont_dir)
print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

os.system( "rm %s/*.cst" % cont_dir )
os.system( "rm %s/*.ork" % cont_dir )

###################################

os.system( "mkdir -p %s/Frameworks" % cont_dir )

def copy_framework( fw_name ):
	fwpath = "%s.framework" % fw_name
	libpath = "%s/Versions/4/%s" % (fwpath,fw_name)

	cmd_cp = "cp -R /Library/Frameworks/%s %s/Frameworks/%s" % (fwpath,cont_dir,fwpath)
	print "cmd_cp<%s>" % cmd_cp
	os.system(cmd_cp)

###################################

def install_name_exe( fw_name, exe_name ):
	fwpath = "%s.framework" % fw_name
	libpath = "%s/Versions/4/%s" % (fwpath,fw_name)

	chp_exe = "install_name_tool -change %s @executable_path/../Frameworks/%s %s" % (libpath,libpath,exe_name)
	print chp_exe
	os.system(chp_exe)

def install_name_lib( fw_name ):
	fwpath = "%s.framework" % fw_name
	libpath = "%s/Versions/4/%s" % (fwpath,fw_name)

	chp_lib = "install_name_tool -id @executable_path/../Frameworks/%s stage/bundle/OrkidTool.app/Contents/Frameworks/%s" % (libpath,libpath)
	print chp_lib
	os.system(chp_lib)

###################################

def expand_lib_path(name):
	return "stage/bundle/OrkidTool.app/Contents/lib/lib%s.osx.release.so" % name

if 1:
	copy_framework( "QtCore" )
	copy_framework( "QtGui" )

	install_name_lib("QtCore")
	install_name_lib("QtGui")

	for qtnam in string.split( "QtCore QtGui" ):
		for item in string.split("ork.lev2 ork.tool tweakout"):
			install_name_exe( qtnam, expand_lib_path(item))

	install_name_exe( "QtCore", "stage/bundle/OrkidTool.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui" )

os.system( "macdeployqt stage/bundle/OrkidTool.app -verbose=2 -dmg")

if 0:
	os.system( "bzip2  stage/bundle/OrkidTool.app.tar")

#print os.environ
#cmd_cp = "cp -r %s/%s" % (bundle_dir,bundle_name)
