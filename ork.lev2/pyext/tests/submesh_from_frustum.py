#!/usr/bin/env python3
################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
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

def procsubmesh(inpsubmesh):
  triangulated = inpsubmesh.triangulated()
  stripped = triangulated.copy(preserve_normals=False,
                               preserve_colors=False,
                               preserve_texcoords=False)
  return stripped

def proc_with_plane(inpsubmesh,plane):
  submesh2 = inpsubmesh.clippedWithPlane(plane=plane,
                                         close_mesh=True, 
                                         flip_orientation=False )["front"]

  return procsubmesh(submesh2)

################################################################################
print("###############################")
print("# FRUSTUM SUBMESH")
print("###############################")
fpmtx = mtx4.perspective(45,1,0.1,3)
fvmtx = mtx4.lookAt(vec3(0,0,1),vec3(0,0,0),vec3(0,1,0))
frustum = Frustum()
frustum.set(fvmtx,fpmtx)
submesh = procsubmesh(meshutil.SubMesh.createFromFrustum(frustum))
for item in submesh.vertexpool.orderedVertices:
    print(item.position)
print("box.submesh: convexVolume: %s" % submesh.convexVolume)

submesh.writeWavefrontObj("frustumA.obj")


fvmtx2 = mtx4.lookAt(vec3(1,0,1),vec3(1,1,0),vec3(0,1,0))
frustum2 = Frustum()
frustum2.set(fvmtx2,fpmtx)
submesh2 = procsubmesh(meshutil.SubMesh.createFromFrustum(frustum2))
submesh2.writeWavefrontObj("frustumB.obj")

print(frustum2.nearPlane)
print(frustum2.farPlane)
print(frustum2.leftPlane)
print(frustum2.rightPlane)
print(frustum2.topPlane)
print(frustum2.bottomPlane)

submesh2 = proc_with_plane(submesh,frustum2.nearPlane)
submesh3 = proc_with_plane(submesh2,frustum2.farPlane)
submesh4 = proc_with_plane(submesh3,frustum2.leftPlane)
submesh5 = proc_with_plane(submesh4,frustum2.rightPlane)
submesh6 = proc_with_plane(submesh5,frustum2.topPlane)
submesh7 = proc_with_plane(submesh6,frustum2.bottomPlane)

print(submesh7)

submesh7.writeWavefrontObj("submesh7.obj")

