#!/usr/bin/env python3

################################################################################

import sys, math, random, numpy, obt.path, obt.deco
from orkengine.core import *
from orkengine.lev2 import *
lev2appinit()
l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.primitives import createParticleData

DECO = obt.deco.Deco()

################################################################################
# create object graph
################################################################################

tconf = GedTestObjectConfiguration()
tobj = tconf.createTestObject("test2")
c1 = tobj.createCurve("curve1")
ga = tobj.createGradient("gradA")

ptc_data = createParticleData()
tobj.setParticleSystem("psys1", ptc_data.graphdata )

################################################################################
# serialize it to json
################################################################################

as_json = tconf.serializeJson()
with open("YO.json", "w") as f:
    f.write(as_json)
#print(as_json)

################################################################################
# create clone from json
################################################################################

clone_of_tconf = Object.deserializeJson(as_json)

################################################################################
# serialize the clone to json
################################################################################

as_json2 = clone_of_tconf.serializeJson()
with open("YO2.json", "w") as f:
    f.write(as_json2)

################################################################################
# verify that the two json strings are identical
################################################################################

match_ok = (as_json == as_json2)
print( DECO.rgbstr(255,255,255,"particle system dual serdes test : "), end="" )
if match_ok:
  print(DECO.rgbstr(0,255,0,"OK"))
else:
  print(DECO.err("BAD"))
