#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import math, random, argparse, sys, time
from threading import Lock
from orkengine.core import *
from orkengine.lev2 import *
from _boilerplate import *
################################################################################

def stripSubMesh(inpsubmesh):
  stripped = inpsubmesh.copy(preserve_normals=False,
                             preserve_colors=False,
                             preserve_texcoords=False)
  return stripped

################################################################################

class SceneGraphApp(BasicUiCamSgApp):

  ##############################################
  def __init__(self):
    super().__init__()
    self.mutex = Lock()
  ##############################################
  def onGpuInit(self,ctx):
    super().onGpuInit(ctx,add_grid=True)
    ##############################
    self.pseudowire_pipe = self.createPseudoWirePipeline()
    solid_wire_pipeline = self.createBaryWirePipeline()
    material = solid_wire_pipeline.sharedMaterial
    solid_wire_pipeline.bindParam( material.param("m"), tokens.RCFD_M)
    ##############################
    self.fpmtx1 = mtx4.perspective(45,1,0.3,5)
    self.fvmtx1 = mtx4.lookAt(vec3(0,0,1),vec3(0,0,0),vec3(0,1,0))
    self.frustum1 = Frustum()
    self.frustum1.set(self.fvmtx1,self.fpmtx1)
    self.frusmesh1 = meshutil.SubMesh.createFromFrustum(self.frustum1,True)
    self.submesh1 = stripSubMesh(self.frusmesh1)
    self.prim1 = meshutil.RigidPrimitive(self.frusmesh1,ctx)
    self.sgnode1 = self.prim1.createNode("m1",self.layer1,self.pseudowire_pipe)
    self.sgnode1.enabled = True
    self.sgnode1.sortkey = 2;
    self.sgnode1.modcolor = vec4(1,0,0,1)
    ##############################
    self.submesh_dynamic = stripSubMesh(self.submesh1)
    self.barysub_isect = self.submesh_dynamic.withBarycentricUVs()
    self.prim3 = meshutil.RigidPrimitive(self.barysub_isect,ctx)
    self.sgnode3 = self.prim3.createNode("m3",self.layer1,solid_wire_pipeline)
    self.sgnode3.enabled = True
  ##############################################
  def onUpdate(self,updevent):
    super().onUpdate(updevent)
    θ = self.abstime * math.pi * 2.0 * 0.1
    #
    self.fvmtx1 = mtx4.lookAt(vec3(0,0,1),vec3(math.sin(θ*1.3)*0.5,0,0),vec3(0,1,0))
    #
    self.frustum1.set(self.fvmtx1,self.fpmtx1)
    #
    submesh1 = meshutil.SubMesh.createFromFrustum(self.frustum1,True)
    self.submesh_wire_frustum = submesh1
    #
    pl = plane(vec3(0,0,-1),-1)
    submesh2 = submesh1.clippedWithPlane(plane=pl,
                                         close_mesh=True, 
                                         flip_orientation=False )["front"]
    #
    self.submesh_dynamic = stripSubMesh(submesh2)#.repaired()
    #time.sleep(0.25)
  ##############################################
  def onGpuIter(self):
    super().onGpuIter()

    #self.mutex.acquire()

    # two wireframe frustums
    self.prim1.fromSubMesh(self.submesh_wire_frustum,self.context)
    # intersection mesh
    self.barysub_isect = self.submesh_dynamic.withBarycentricUVs()
    self.prim3.fromSubMesh(self.barysub_isect,self.context)
    #print("intersection convexVolume: %s" % submesh_dynamic.convexVolume)
    #self.mutex.release()

###############################################################################

sgapp = SceneGraphApp()

sgapp.ezapp.mainThreadLoop(on_iter=lambda: sgapp.onGpuIter() )

