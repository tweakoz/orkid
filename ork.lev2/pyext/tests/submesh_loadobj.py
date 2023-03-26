#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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