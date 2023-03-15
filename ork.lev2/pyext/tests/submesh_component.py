#!/usr/bin/env python3
################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import ork.path
from orkengine.core import *
from orkengine.lev2 import *

subm = meshutil.SubMesh()

va = subm.makeVertex(position=vec3(0,0,0))
vb = subm.makeVertex(position=vec3(1,0,0))
vc = subm.makeVertex(position=vec3(1,1,0))
vd = subm.makeVertex(position=vec3(0,1,0))
ve = subm.makeVertex(position=vec3(0,0,0))
vf = subm.makeVertex(position=vec3(0,0,0),normal=vec3(0,1,0))

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
print("plane: %s" % pa.plane)
print("center: %s" % pa.center)
print("area: %s" % pa.area)

for item in pa.edges:
  print("edge: %s hash<0x%x>" % (item, item.hash) )
