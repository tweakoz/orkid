#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import obt.path
from orkengine.core import *
from orkengine.lev2 import *

subm = meshutil.SubMesh()

va = subm.makeVertex(position=dvec3(0,0,0))
vb = subm.makeVertex(position=dvec3(1,0,0))
vc = subm.makeVertex(position=dvec3(1,1,0))
vd = subm.makeVertex(position=dvec3(0,1,0))
ve = subm.makeVertex(position=dvec3(0,0,0))
vf = subm.makeVertex(position=dvec3(0,0,0),normal=dvec3(0,1,0))

assert(va.poolindex==0)
assert(vb.poolindex==1)
assert(vc.poolindex==2)
assert(vd.poolindex==3)
assert(ve.poolindex==0)
assert(vf.poolindex==4)

assert(subm.numVertices() == 5)

pa = subm.makeQuad(va,vb,vc,vd)

print("poly: %s" % pa)
print("numsides: %s" % pa.numSides)
print("normal: %s" % pa.normal)
#print("plane: %s" % pa.plane)
print("center: %s" % pa.center)
print("area: %s" % pa.area)

for item in pa.edges:
  print("edge: %s hash<0x%x>" % (item, item.hash) )
