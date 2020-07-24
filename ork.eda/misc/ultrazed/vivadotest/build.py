#!/usr/bin/env python3

import os
from ork import vivado, path, pathtools

#todo invoke genclock.py

this_dir = path.Path(os.path.dirname(os.path.realpath(__file__)))
pathtools.chdir(this_dir)

print(os.getcwd())

dirmaps = {
  this_dir: "/tmp/build"
}
vivado.run(dirmaps=dirmaps,
           workingdir="/tmp/build",
           args=["-mode",
            "batch",
            "-nojournal",
            "-nolog",
            "-source",
            "cleanbuild.tcl"])
