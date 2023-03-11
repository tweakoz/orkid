#!/usr/bin/env python3
################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import ork.path
from orkengine.core import *
from orkengine.lev2 import *

coreappinit() # setup filesystem
a = meshutil.Mesh()
a.readFromWavefrontObj("data://tests/simple_obj/box.obj")
for subname in a.polygroups.keys():
  submesh = a.polygroups[subname]
  print(subname,submesh)
  print("numtris: %s" % submesh.numPolys(3))
  print("numquads: %s" % submesh.numPolys(4))  