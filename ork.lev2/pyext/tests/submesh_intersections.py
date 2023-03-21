#!/usr/bin/env python3

################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.misc import *
from common.primitives import createGridData, createFrustumPrim
from common.scenegraph import createSceneGraph

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

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,1,5),tgt=vec3(0,1,0))

  ##############################################

  def onGpuInit(self,ctx):

    createSceneGraph(app=self,rendermodel="ForwardPBR")

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ##################################

    self.pseudowire_pipe = pseudowire_pipeline(app=self,ctx=ctx)

    #################################################################
    fpmtx1 = mtx4.perspective(45,1,0.1,3)
    fvmtx1 = mtx4.lookAt(vec3(0,0,1),vec3(0,0,0),vec3(0,1,0))
    frustum1 = Frustum()
    frustum1.set(fvmtx1,fpmtx1)
    frusmesh1 = meshutil.SubMesh.createFromFrustum(frustum1,True)
    self.submesh1 = procsubmesh(frusmesh1)
    self.prim1 = meshutil.RigidPrimitive(frusmesh1,ctx)
    self.sgnode1 = self.prim1.createNode("m1",self.layer1,self.pseudowire_pipe)
    self.sgnode1.enabled = True
    self.sgnode1.sortkey = 2;
    self.sgnode1.modcolor = vec4(1,0,0,1)
    #################################################################
    fpmtx2 = mtx4.perspective(45,1,0.1,3)
    fvmtx2 = mtx4.lookAt(vec3(1,0,1),vec3(1,1,0),vec3(0,1,0))
    frustum2 = Frustum()
    frustum2.set(fvmtx2,fpmtx2)
    frusmesh2 = meshutil.SubMesh.createFromFrustum(frustum2,True)
    self.submesh2 = procsubmesh(frusmesh2)
    self.prim2 = meshutil.RigidPrimitive(frusmesh2,ctx)
    self.sgnode2 = self.prim2.createNode("m2",self.layer1,self.pseudowire_pipe)
    self.sgnode2.enabled = True
    self.sgnode2.sortkey = 2;
    self.sgnode2.modcolor = vec4(0,1,0,1)
    ##################################
    # solid wire pipeline
    ##################################
    solid_wire_pipeline = createPipeline( app = self,
                                       ctx = ctx,
                                       rendermodel = "ForwardPBR",
                                       shaderfile=Path("orkshader://basic"),
                                       techname="tek_fnormal_wire" )

    material = solid_wire_pipeline.sharedMaterial
    solid_wire_pipeline.bindParam( material.param("m"), tokens.RCFD_M)
    #################################################################
    submesh_isect = proc_with_frustum(self.submesh1,frustum2)
    self.barysub_isect = submesh_isect.barycentricUVs()
    self.prim3 = meshutil.RigidPrimitive(self.barysub_isect,ctx)
    self.sgnode3 = self.prim3.createNode("m3",self.layer1,solid_wire_pipeline)
    self.sgnode3.enabled = True
    #################################################################
    self.context = ctx

  ##############################################

  def onGpuIter(self):
    θ = self.abstime * math.pi * 2.0 * 0.1

    self.fpmtx1 = mtx4.perspective(45,1,0.1,3)
    self.fvmtx1 = mtx4.lookAt(vec3(0,0,1),vec3(math.sin(θ*1.3)*0.5,0,0),vec3(0,1,0))
    self.frustum1 = Frustum()
    self.frustum1.set(self.fvmtx1,self.fpmtx1)
    frusmesh1 = meshutil.SubMesh.createFromFrustum(self.frustum1,True)
    self.submesh1 = procsubmesh(frusmesh1)
    self.prim1.fromSubMesh(frusmesh1,self.context)

    self.fpmtx2 = mtx4.perspective(45,1,0.1,3)
    self.fvmtx2 = mtx4.lookAt(vec3(1,0,1),vec3(1,0.5+math.sin(θ)*0.4,0),vec3(0,1,0))
    self.frustum2 = Frustum()
    self.frustum2.set(self.fvmtx2,self.fpmtx2)
    frusmesh2 = meshutil.SubMesh.createFromFrustum(self.frustum2,True)
    self.prim2.fromSubMesh(frusmesh2,self.context)


    submesh_isect = proc_with_frustum(self.submesh1,self.frustum2)
    self.barysub_isect = submesh_isect.barycentricUVs()
    self.prim3.fromSubMesh(self.barysub_isect,self.context)
    print("intersection convexVolume: %s" % submesh_isect.convexVolume)

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

  ################################################

  def onUpdate(self,updinfo):
    self.abstime = updinfo.absolutetime
    #########################################
    self.scene.updateScene(self.cameralut) 

###############################################################################

sgapp = SceneGraphApp()

sgapp.ezapp.mainThreadLoop(on_iter=lambda: sgapp.onGpuIter() )

