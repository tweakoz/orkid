#!/usr/bin/env python3
################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import obt.path
import math, random, argparse, sys
import trimesh, pickle
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
################################################################################
# apply transform to trimesh
################################################################################

################################################################################

class SceneGraphApp(BasicUiCamSgApp):
  ##############################################
  def __init__(self):
    super().__init__()
  ##############################################
  def cutWithPlane(self,inpsubmesh,n,o):
    nn = n.normalized()
    tp = dplane(nn,0)
    d = tp.distanceToPoint(o) * -1.0
    new_plane = dplane(nn,d)
    return clipMeshWithPlane(inpsubmesh,new_plane,debug=False).prune()
  ##############################################
  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    ##################################
    # solid wire pipeline
    ##################################
    pbr_pipeline = self.createBaryWirePipeline()
    material = pbr_pipeline.sharedMaterial
    pbr_pipeline.bindParam( material.param("m"), tokens.RCFD_M)
    pbr_pipeline.sharedMaterial = material
    ##############################
    #self.pts_drawabledata = LabeledPointDrawableData()
    #self.pts_drawabledata.pipeline_points = self.createPointsPipeline()
    #self.sgnode_pts = self.layer1.createDrawableNodeFromData("points",self.pts_drawabledata)

    #################################################################
    # source mesh paths
    #################################################################

    cub_path = "data://tests/simple_obj/box.obj"

      # load source meshes

    cub_orkmesh = meshutil.Mesh()
    cub_orkmesh.readFromWavefrontObj(cub_path)
    cub_submesh = stripSubmesh(cub_orkmesh.submesh_list[0])
    self.cube_submesh = cub_submesh
    self.bary_prim = RigidPrimitive(cub_submesh,ctx)
    self.bary_sgnode = self.bary_prim.createNode("bevel",self.layer1,pbr_pipeline)
    self.bary_sgnode.enabled = True

  ##############################################
  def onUpdate(self,updevent):
    super().onUpdate(updevent)
  ##############################################
  def onGpuIter(self):
    super().onGpuIter()
    phi = 0.5+math.sin(self.abstime*3.0)*0.5
    beveld = 1.5+phi*0.5
    cub_submesh = stripSubmesh(self.cube_submesh)

    ##############################################
    # YZ
    ##############################################

    numsides = 18
    rotincr = 360.0/numsides
    dm = dmtx4(dquat(dvec3(0,1,0),rotincr*constants.DTOR))
    yzdir = dvec3(0,1,1)
    for i in range(0,numsides):
      cub_submesh = self.cutWithPlane(cub_submesh,yzdir,dvec3(0,-beveld,0))
      cub_submesh = self.cutWithPlane(cub_submesh,yzdir*-1.0,dvec3(0,beveld,0))
      yzdir = dvec4(yzdir,0).transform(dm).xyz

    ##############################################
    # XZ
    ##############################################

    numsides = 9
    rotincr = 360.0/numsides
    dm = dmtx4(dquat(dvec3(0,1,0),rotincr*constants.DTOR))
    yzdir = dvec3(1,0,1)
    for i in range(0,numsides):
      cub_submesh = self.cutWithPlane(cub_submesh,yzdir,yzdir*-beveld*0.5)
      yzdir = dvec4(yzdir,0).transform(dm).xyz

    ##############################################

    #self.pts_drawabledata.pointsmesh = cub_submesh

    self.barysubmesh = cub_submesh.withBarycentricUVs()
    self.bary_prim.fromSubMesh(self.barysubmesh,self.context)

  ##############################################
  def onUiEvent(self,uievent):
    super().onUiEvent(uievent)
    return ui.HandlerResult()


###############################################################################
sgapp = SceneGraphApp()
sgapp.ezapp.mainThreadLoop(on_iter=lambda: sgapp.onGpuIter() )
