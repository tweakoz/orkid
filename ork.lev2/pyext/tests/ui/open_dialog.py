#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, numpy, obt.path
from orkengine.core import *
from orkengine.lev2 import *

def_path = obt.path.stage()/"dblockcache"
selected_paths = ui.popupOpenDialog("Open File", str(def_path/"bin"), [],True)
print("selected_paths<%s>" % selected_paths)
