#!/usr/bin/env python

import os
from ork.build.pathtools import path 

rootpath = path(os.environ["ORKDOTBUILD_ROOT"]);
mbpath = rootpath/"data"/"src"/"uvtest.mb"

modulepath = path(os.environ["MAYA_MODULE_PATH"])
scriptpath = modulepath/"OrkUtils"/"scripts"/"test_uvrail.mel"

cmd = "maya -batch -file %s -script %s" % (mbpath,scriptpath)

os.system(cmd)
