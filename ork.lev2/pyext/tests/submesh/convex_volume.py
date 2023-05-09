#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import time
from orkengine.core import *
from orkengine.lev2 import *
from PIL import Image
import numpy as np
from ork.deco import Deco
deco = Deco()
coreappinit() # setup filesystem
################################################################################
print("###############################")
print("UNIT CUBE VOLUME")
print("###############################")
a = meshutil.Mesh()
a.readFromWavefrontObj("data://tests/simple_obj/box.obj")
submesh = a.submesh_list[0].triangulated()
stripped = submesh.copy(preserve_normals=False,
                        preserve_colors=False,
                        preserve_texcoords=False).convexHull(0)
assert(stripped.isConvexHull)
for item in stripped.vertices:
    print(item.position)
print("box.submesh: convexVolume: %s" % stripped.convexVolume)

################################################################################
print("###############################")
print("CONE VOLUME")
print("###############################")
a = meshutil.Mesh()
a.readFromWavefrontObj("data://tests/simple_obj/cone.obj")
submesh = a.submesh_list[0].triangulated()
print(submesh)
stripped = submesh.triangulated().copy(preserve_normals=False,
                                       preserve_colors=False,
                                       preserve_texcoords=False).convexHull(0)
print(stripped)
#assert(stripped.isConvexHull)
#for item in stripped.vertices:
#    print(item.position)
print("cone.submesh: convexVolume: %s" % stripped.convexVolume)


################################################################################
print("###############################")
print("TETRA VOLUME")
print("###############################")
a = meshutil.Mesh()
a.readFromWavefrontObj("data://tests/simple_obj/tetra.obj")
submesh = a.submesh_list[0].triangulated()
print(submesh)
stripped = submesh.triangulated().copy(preserve_normals=False,
                                       preserve_colors=False,
                                       preserve_texcoords=False).convexHull(0)
print(stripped)
assert(stripped.isConvexHull)
for item in stripped.vertices:
    print(item.position)
print("tetra.submesh: convexVolume: %s" % stripped.convexVolume)

################################################################################
print("###############################")
print("UNIT SPHERE VOLUME")
print("###############################")
a = meshutil.Mesh()
a.readFromWavefrontObj("data://tests/simple_obj/uvsphere.obj")
submesh = a.submesh_list[0].triangulated()
print(submesh)
stripped = submesh.triangulated().copy(preserve_normals=False,
                                       preserve_colors=False,
                                       preserve_texcoords=False).convexHull(0)
print(stripped)
assert(stripped.isConvexHull)
for item in stripped.vertices:
    print(item.position)
print("sphere.submesh: convexVolume: %s" % stripped.convexVolume)
