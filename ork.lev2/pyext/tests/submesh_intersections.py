#!/usr/bin/env python3
################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *
from _boilerplate import *
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
  return submesh7.convexDecomposition()[0]

################################################################################

class SceneGraphApp(BasicUiCamSgApp):

  ##############################################
  def __init__(self):
    super().__init__()
  ##############################################
  def onGpuInit(self,ctx):
    super().onGpuInit(ctx,add_grid=True)
    ##############################
    self.pseudowire_pipe = self.createPseudoWirePipeline()
    solid_wire_pipeline = self.createBaryWirePipeline()
    material = solid_wire_pipeline.sharedMaterial
    solid_wire_pipeline.bindParam( material.param("m"), tokens.RCFD_M)
    ##############################
    self.fpmtx1 = mtx4.perspective(45,1,0.1,3)
    self.fvmtx1 = mtx4.lookAt(vec3(0,0,1),vec3(0,0,0),vec3(0,1,0))
    self.frustum1 = Frustum()
    self.frustum1.set(self.fvmtx1,self.fpmtx1)
    self.frusmesh1 = meshutil.SubMesh.createFromFrustum(self.frustum1,True)
    self.submesh1 = procsubmesh(self.frusmesh1)
    self.prim1 = meshutil.RigidPrimitive(self.frusmesh1,ctx)
    self.sgnode1 = self.prim1.createNode("m1",self.layer1,self.pseudowire_pipe)
    self.sgnode1.enabled = True
    self.sgnode1.sortkey = 2;
    self.sgnode1.modcolor = vec4(1,0,0,1)
    ##############################
    self.fpmtx2 = mtx4.perspective(45,1,0.1,3)
    self.fvmtx2 = mtx4.lookAt(vec3(1,0,1),vec3(1,1,0),vec3(0,1,0))
    self.frustum2 = Frustum()
    self.frustum2.set(self.fvmtx2,self.fpmtx2)
    self.frusmesh2 = meshutil.SubMesh.createFromFrustum(self.frustum2,True)
    self.submesh2 = procsubmesh(self.frusmesh2)
    self.prim2 = meshutil.RigidPrimitive(self.frusmesh2,ctx)
    self.sgnode2 = self.prim2.createNode("m2",self.layer1,self.pseudowire_pipe)
    self.sgnode2.enabled = True
    self.sgnode2.sortkey = 2;
    self.sgnode2.modcolor = vec4(0,1,0,1)
    ##############################
    submesh_isect = proc_with_frustum(self.submesh1,self.frustum2)
    self.barysub_isect = submesh_isect.withBarycentricUVs()
    self.prim3 = meshutil.RigidPrimitive(self.barysub_isect,ctx)
    self.sgnode3 = self.prim3.createNode("m3",self.layer1,solid_wire_pipeline)
    self.sgnode3.enabled = True
  ##############################################
  def onGpuIter(self):
    super().onGpuIter()
    θ = self.abstime * math.pi * 2.0 * 0.1

    self.fvmtx1 = mtx4.lookAt(vec3(0,0,1),vec3(math.sin(θ*1.3)*0.5,0,0),vec3(0,1,0))
    self.frustum1.set(self.fvmtx1,self.fpmtx1)
    frusmesh1 = meshutil.SubMesh.createFromFrustum(self.frustum1,True)
    self.submesh1 = procsubmesh(frusmesh1)
    self.prim1.fromSubMesh(frusmesh1,self.context)

    self.fvmtx2 = mtx4.lookAt(vec3(1,0,1),vec3(1,0.5+math.sin(θ)*0.4,0),vec3(0,1,0))
    self.frustum2.set(self.fvmtx2,self.fpmtx2)
    frusmesh2 = meshutil.SubMesh.createFromFrustum(self.frustum2,True)
    self.prim2.fromSubMesh(frusmesh2,self.context)


    submesh_isect = proc_with_frustum(self.submesh1,self.frustum2)
    self.barysub_isect = submesh_isect.withBarycentricUVs()
    self.prim3.fromSubMesh(self.barysub_isect,self.context)
    print("intersection convexVolume: %s" % submesh_isect.convexVolume)

###############################################################################

sgapp = SceneGraphApp()

sgapp.ezapp.mainThreadLoop(on_iter=lambda: sgapp.onGpuIter() )

