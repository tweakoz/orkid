#!/usr/bin/env python3
##########################################################################
# run doxygen on unioned filetree
##########################################################################

import os, string, pathlib

from pathlib import Path

base = Path(os.environ["ORKDOTBUILD_WORKSPACE_DIR"])
stage = Path(os.environ["ORKDOTBUILD_STAGE_DIR"])
doxyoutput = stage/"doxygen"
doxyfile = base/"ork.dox"/"Doxyfile"

os.system("ork.createunionedsource.py") # first we definitely need unioned src (for ease...)

os.chdir(base)
os.system("rm -rf %s"%doxyoutput)
os.system("doxygen %s"%doxyfile)
