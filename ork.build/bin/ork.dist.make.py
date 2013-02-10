#!/usr/bin/python

import datetime
import subprocess
import sys
import os
import string

if False == ("ORKDOTBUILD_STAGE_DIR" in os.environ):
	print "ORKDOTBUILD_STAGE_DIR env not set, exiting..."
	sys.exit(-1)

stg_dir = os.environ["ORKDOTBUILD_STAGE_DIR"]
obuild_dir = "%s/../ork.build" % stg_dir

print "stage_dir<%s>" % stg_dir

#########################################
def call(cmd):
	return subprocess.call( cmd , shell=True, stderr=sys.stdout, stdout=sys.stdout)
#########################################

now_object = datetime.datetime.now();
todays_date_string = "%02d%02d%04d" % (now_object.month,now_object.day,now_object.year)
current_time_string = "%02d%02d%02d" % (now_object.hour,now_object.minute,now_object.second)
dtstring = "%s_%s" % (todays_date_string,current_time_string)

bas = "dist/ork.%s"%dtstring

#########################################

cont  = " bin include lib doc share "
cont += " LICENSE CHANGES INSTALL README "
cont += " 3dl glsl ops procedurals python  "
cont += " rmanDisplays rmanProcedurals rsl shaderlink shaders  "
#########################################


call( "mkdir -p %s" % bas )
for item in string.split(cont):
	call( "cp -r %s/%s %s/" % (stg_dir, item, bas) )
call( "cp -r %s %s/" % (obuild_dir,bas) )
call( "cp -r %s/../tozkit %s/" % (stg_dir,bas) )
call( "cp %s/template/ork.build.ext_env.py %s/ork.build.ext_env.py" % (obuild_dir,bas) )


