#!/usr/bin/env python3

################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.misc import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')

################################################################################

args = vars(parser.parse_args())

################################################################################

def printSubMesh(name, subm):
  print(subm)
  out_str = "submesh: %s\n" % name
  for idx, vtx in enumerate(subm.vertices):
    out_str += " vtx> %d: %s\n"%(idx, vtx.position)
  for idx, poly in enumerate(subm.polys):
    out_str += "  poly> %d: [ "%idx
    for v in poly.vertices:
      out_str += "%d " % v.poolindex
    out_str += "]\n"
  print(out_str)

################################################################################

class ConvexDecomp:
  def __init__(self, #
               context=None, #
               layer=None, #
               pipeline=None, #
               origin = vec3(0,0,0),
               model_asset_path=None, #
               ): #

    self.origin = origin
    self.speed = random.uniform(.05,.1)
    self.target_plane_axis = vec3(0,1,0)
    self.current_plane_axis = vec3(0,1,0)
    self.counter = 0.0

    ##################################
    # load model 
    ##################################

    self.mesh = meshutil.Mesh()
    self.mesh.readFromWavefrontObj(model_asset_path)

    ##################################
    # extract submesh
    ##################################

    self.ori_submesh = self.mesh.submesh_list[0]
    printSubMesh("srcmesh", self.ori_submesh)

    # original submesh primitive

    self.ori_prim = RigidPrimitive(self.ori_submesh,context)
    self.ori_sgnode = self.ori_prim.createNode("ori",layer,pipeline)
    self.ori_sgnode.enabled = False

    ##################################
    # strip UV's, normals and colors from submesh
    ##################################

    self.stripped = self.ori_submesh.copy(preserve_normals=False,
                                          preserve_colors=False,
                                          preserve_texcoords=False)

    self.pipeline = pipeline
    self.context = context
    self.layer = layer

    ##################################

    self.convex_hulls = self.stripped.convexDecomposition()
    self.barys = []
    self.prims = []
    self.primnodes = []
    self.normals = []
    for item in self.convex_hulls:
      bary = item.withBarycentricUVs()
      prim = RigidPrimitive(bary,self.context)
      prim_node = prim.createNode("%s"%bary,self.layer,self.pipeline)
      self.barys += [bary]
      self.prims += [prim]
      self.primnodes += [prim_node]
      nx = random.uniform(-1,1)
      ny = random.uniform(-1,1)
      nz = random.uniform(-1,1)
      self.normals += [vec3(nx,ny,nz).normalized]

    ##################################

  def update(self,abstime):
    θ = abstime * math.pi * 2.0 * self.speed 
    size = len(self.primnodes)
    for i in range(0,size):
      node = self.primnodes[i]
      norm = self.normals[i]

      node.worldTransform.translation = self.origin+norm
      node.worldTransform.orientation = quat(vec3(0,1,0),θ)
      node.worldTransform.scale = 1

################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,1,5),tgt=vec3(0,1,0))
    self.modelinsts=[]

  ##############################################

  def onGpuInit(self,ctx):

    self.decomps = []

    createSceneGraph(app=self,rendermodel="ForwardPBR")
    self.cam_overlay = self.layer1.createDrawableNode("camoverlay",self.uicam.createDrawable())

    ##################################
    # create Grid
    ##################################

    #self.grid_data = createGridData()
    #self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    #self.grid_node.sortkey = 1

    ##################################
    # shared material
    ##################################

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               rendermodel = "ForwardPBR",
                               shaderfile=Path("orkshader://basic"),
                               techname="tek_fnormal_wire" )

    material = pipeline.sharedMaterial
    pipeline.bindParam( material.param("m"), tokens.RCFD_M)



    ##################################
    # convex decompositions
    ##################################

    d = ConvexDecomp(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  origin = vec3(2,0,2),
                  model_asset_path = "data://tests/simple_obj/torus.obj" )

    self.decomps += [d]

    d = ConvexDecomp(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  origin = vec3(-2,0,2),
                  model_asset_path = "data://tests/simple_obj/box.obj" )

    self.decomps += [d]

    d = ConvexDecomp(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  origin = vec3(2,0,-2),
                  model_asset_path = "data://tests/simple_obj/cone.obj" )

    self.decomps += [d]

    d = ConvexDecomp(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  origin = vec3(-2,0,-2),
                  model_asset_path = "data://tests/simple_obj/monkey.obj" )

    self.decomps += [d]

  ##############################################

  def onUiEvent(self,uievent):
    res = ui.HandlerResult()
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
      res = ui.HandlerResult()
      res.setHandler(self.ezapp.topWidget)
    return res

  ################################################

  def onUpdate(self,updinfo):
    #########################################
    for d in self.decomps:
      d.update(updinfo.absolutetime)
    #########################################
    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()

