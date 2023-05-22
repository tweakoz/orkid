#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, numpy, ork.path
from orkengine.core import *
from orkengine.lev2 import *
lev2appinit()
################################################################################
#sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
#from _boilerplate import *
################################################################################

tconf = GedTestObjectConfiguration()
tobj = tconf.createTestObject("test2")
c1 = tobj.createCurve("curve1")
ga = tobj.createGradient("gradA")
ps1 = tobj.createParticleSystem("psys1")
globs = ps1.create("GLOB",particles.Globals)
gravity = ps1.create("GRAV",particles.Gravity)

as_json = tconf.serializeJson()
with open("YO.json", "w") as f:
    f.write(as_json)
print(as_json)
clone_of_tconf = Object.deserializeJson(as_json)
print(clone_of_tconf)