#!/usr/bin/env python3

################################################################################

import sys, math, random, numpy, ork.path
from orkengine.core import *
from orkengine.lev2 import *
lev2appinit()

################################################################################
# create object graph
################################################################################

tconf = GedTestObjectConfiguration()
tobj = tconf.createTestObject("test2")
c1 = tobj.createCurve("curve1")
ga = tobj.createGradient("gradA")
ps1 = tobj.createParticleSystem("psys1")
globs = ps1.create("GLOB",particles.Globals)
gravity = ps1.create("GRAV",particles.Gravity)
gravityplugs = gravity.inputs
globalplugs = globs.outputs
ps1.connect( gravityplugs.G, globalplugs.RelTime )

################################################################################
# serialize it to json
################################################################################

as_json = tconf.serializeJson()
with open("YO.json", "w") as f:
    f.write(as_json)
print(as_json)

################################################################################
# create clone from json
################################################################################

clone_of_tconf = Object.deserializeJson(as_json)
print(clone_of_tconf)

################################################################################
# serialize the clone to json
################################################################################

as_json2 = clone_of_tconf.serializeJson()
with open("YO2.json", "w") as f:
    f.write(as_json2)

################################################################################
# verify that the two json strings are identical
################################################################################

assert(as_json == as_json2)
