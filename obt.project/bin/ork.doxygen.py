#!/usr/bin/env python3
##########################################################################
# run doxygen on unioned filetree
##########################################################################

import os, string, pathlib

from pathlib import Path
from ork import template, command

base = Path(os.environ["ORKID_WORKSPACE_DIR"])
stage = Path(os.environ["OBT_STAGE"])
doxyoutput = stage/"orkid"/"doxygen"
src_doxyfile = base/"ork.dox"/"Doxyfile"
dst_doxyfile = stage/"orkid"/"Doxyfile"

command.run(["ork.createunionedsource.py"]) # first we definitely need unioned src (for ease...)

icon_path = base/"ork.data"/"dox"/"doxylogo.png"
template.template_file(src_doxyfile,
                       {"ICONPATH":icon_path},
                       dst_doxyfile)

os.chdir(stage/"orkid")
command.run(["rm","-rf",doxyoutput])
command.run(["doxygen",dst_doxyfile])
