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

def procsubmesh(inpsubmesh):
  triangulated = inpsubmesh.triangulated()
  stripped = triangulated.copy(preserve_normals=False,
                               preserve_colors=False,
                               preserve_texcoords=False)
  return stripped

################################################################################

def proc_with_plane(inpsubmesh,plane):
  submesh2 = inpsubmesh.clippedWithPlane(plane=plane,
                                         close_mesh=True, 
                                         flip_orientation=False )["front"]

  return procsubmesh(submesh2)

################################################################################

def proc_with_frustum(inpsubmesh,frustum):
  submesh2 = proc_with_plane(inpsubmesh,frustum.nearPlane)
  submesh3 = proc_with_plane(submesh2,frustum.farPlane)
  submesh4 = proc_with_plane(submesh3,frustum.leftPlane)
  submesh5 = proc_with_plane(submesh4,frustum.rightPlane)
  submesh6 = proc_with_plane(submesh5,frustum.topPlane)
  submesh7 = proc_with_plane(submesh6,frustum.bottomPlane)
  return submesh7

################################################################################

print("###############################")
print("# FRUSTUM SUBMESH")
print("###############################")
fpmtx = dmtx4.perspective(45,1,0.1,3)
fvmtx = dmtx4.lookAt(dvec3(0,0,1),dvec3(0,0,0),dvec3(0,1,0))
frustum = dfrustum()
frustum.set(fvmtx,fpmtx)
submesh = procsubmesh(meshutil.SubMesh.createFromFrustum(frustum))
for item in submesh.vertices:
    print(item.position)
print("box.submesh: convexVolume: %s" % submesh.convexVolume)

submesh.writeWavefrontObj("frustumA.obj")

#################################################

fvmtx2 = dmtx4.lookAt(dvec3(1,0,1),dvec3(1,1,0),dvec3(0,1,0))
frustum2 = dfrustum()
frustum2.set(fvmtx2,fpmtx)
submesh2 = procsubmesh(meshutil.SubMesh.createFromFrustum(frustum2))
submesh2.writeWavefrontObj("frustumB.obj")

final =proc_with_frustum(submesh,frustum2)

final.writeWavefrontObj("final.obj")

