#!/usr/bin/env python

import os, string

#print "yo"
deploy = False

bundle_name = "OrkidTool.app"

stage_dir = os.environ["ORKDOTBUILD_STAGE_DIR"]
odb_dir = os.environ["ORKDOTBUILD_ROOT"]
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
#print "cmd_rm<%s>" % cmd_rm
os.system(cmd_rm)

cmd_cp = "cp -r %s/%s %s/" % (tool_dir,bundle_name,bundle_dir)
#print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

cmd_cp = "cp -r %s/lib %s" % (stage_dir,cont_dir)
#print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

cmd_cp = "ln -s %s/data %s" % (root_dir,cont_dir)
#cmd_cp = "cp -r %s/data %s" % (root_dir,cont_dir)
#print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

os.system("cp %s/bin/ork.tool.test.osx.release %s/MacOS/" % (stage_dir,cont_dir))
os.system("cp %s/bin/ork.assetassistant.py %s/MacOS/" % (odb_dir,cont_dir))
os.system("cp %s/bin/orkassasshl.py %s/MacOS/" % (odb_dir,cont_dir))

os.system( "rm %s/*.cst" % cont_dir )
os.system( "rm %s/*.ork" % cont_dir )

###################################

if deploy:
   os.system( "macdeployqt stage/bundle/OrkidTool.app -verbose=2 -dmg")
else:
   os.system( "macdeployqt stage/bundle/OrkidTool.app -verbose=2")

if 0:
   os.system( "bzip2  stage/bundle/OrkidTool.app.tar")

#print os.environ
#cmd_cp = "cp -r %s/%s" % (bundle_dir,bundle_name)
