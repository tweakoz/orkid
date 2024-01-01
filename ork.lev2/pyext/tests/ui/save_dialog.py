#!/usr/bin/env python3

################################################################################
# lev2 sample which opens a file dialog and prints the result to the console
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, numpy, obt.path
from orkengine.core import *
from orkengine.lev2 import *

def_path = obt.path.stage()
selected_path = ui.popupSaveDialog("Save File", str(def_path/"yo.txt"), ["*.yo"])
print("selected_path<%s>" % selected_path)
