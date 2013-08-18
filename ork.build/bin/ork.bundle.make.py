#!/usr/bin/python

import os

print "yo"

bundle_name = "OrkidTool.app"

stage_dir = os.environ["ORKDOTBUILD_STAGE_DIR"]
bundle_dir = "%s/bundle" % stage_dir
root_dir = "%s/../" % stage_dir
tool_dir = "%s/ork.tool" % root_dir
print "root_dir<%s>" % root_dir
print "stage_dir<%s>" % stage_dir
print "bundle_dir<%s>" % bundle_dir
print "tool_dir<%s>" % tool_dir

cmd_rm = "rm -rf %s/%s" % (bundle_dir,bundle_name)
print "cmd_rm<%s>" % cmd_rm
os.system(cmd_rm)

cmd_cp = "cp -r %s/%s %s/" % (tool_dir,bundle_name,bundle_dir)
print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

cmd_cp = "cp -r %s/lib %s/%s/Contents" % (stage_dir,bundle_dir,bundle_name)
print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

cmd_cp = "cp -r %s/data %s/%s/Contents" % (root_dir,bundle_dir,bundle_name)
print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)

cmd_cp = "cp -r %s/bin/ork.tool.test.osx.release %s/%s/Contents/MacOS" % (stage_dir,bundle_dir,bundle_name)
print "cmd_cp<%s>" % cmd_cp
os.system(cmd_cp)
#print os.environ
#cmd_cp = "cp -r %s/%s" % (bundle_dir,bundle_name)
