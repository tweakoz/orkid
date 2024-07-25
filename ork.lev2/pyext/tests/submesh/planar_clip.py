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
    #for v in poly.vertices:
    #  out_str += "%d " % v.poolindex
    #out_str += "]\n"
  print(out_str)

################################################################################

class Fragments:
  def __init__(self, #
               context=None, #
               layer=None, #
               pipeline=None, #
               flip_orientation = False,
               origin = vec3(0,0,0),
               slicing_plane=None,
               model_asset_path=None, #
               ): #

    self.origin = origin
    self.speed = 0 #random.uniform(.05,.1)
    self.target_plane_axis = dvec3(0,1,0)
    self.current_plane_axis = dvec3(0,1,0)
    self.counter = 0.0

    if slicing_plane==None:
      nx = random.uniform(-1,1)
      ny = random.uniform(-1,1)
      nz = random.uniform(-1,1)
      self.normal = dvec3(nx,ny,nz).normalized
      slicing_plane = dplane(self.normal, random.uniform(-1,1))
    else:
      self.normal = slicing_plane.normal

    self.inv_slicing_plane = dplane(self.normal*-1, slicing_plane.d*-1)

    ##################################
    # load model 
    ##################################

    self.mesh = meshutil.Mesh()
    self.mesh.readFromWavefrontObj(model_asset_path)

    ##################################
    # extract submesh
    ##################################

    self.ori_submesh = self.mesh.submesh_list[0]
    #printSubMesh("srcmesh", self.ori_submesh)

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

    #printSubMesh("stripped", self.stripped)

    self.pipeline = pipeline
    self.context = context
    self.layer = layer
    self.flip_orientation = flip_orientation

    self.clip(slicing_plane,initial=True)

    ##################################

  def clip(self,slicing_plane,initial=None):

    self.slicing_plane = slicing_plane

    ##################################
    # clip
    ##################################

    print(self.slicing_plane)
    self.clipped_front = self.stripped.clippedWithPlane( plane=self.slicing_plane,
                                                         flip_orientation = self.flip_orientation,
                                                         close_mesh = True ).prune()

    self.clipped_back = self.stripped.clippedWithPlane( plane=self.inv_slicing_plane,
                                                        flip_orientation = self.flip_orientation,
                                                        close_mesh = True ).prune()

    self.front = self.clipped_front.withBarycentricUVs()
    self.back = self.clipped_back.withBarycentricUVs()

    #printSubMesh("front", self.front)

    #print(self.front.vertexpool.orderedVertices[2])
    #print(self.front.vertexpool.orderedVertices[4])

    ##################################
    # if initial, build primitive
    ##################################

    if initial:
      self.prim_front = RigidPrimitive(self.front,self.context)
      self.prim_node_front = self.prim_front.createNode("front",self.layer,self.pipeline)
      self.prim_node_front.enabled = True
      self.prim_back = RigidPrimitive(self.back ,self.context)
      self.prim_node_back = self.prim_back.createNode("back",self.layer,self.pipeline)
      self.prim_node_back.enabled = True
    else: # TODO - update existing primitive
      self.prim_front.fromSubMesh(self.front,self.context)
      self.prim_back.fromSubMesh(self.back ,self.context)

    ##################################

  def update(self,abstime):
    θ = abstime * math.pi * 2.0 * self.speed 
    self.prim_node_front.worldTransform.translation = self.origin+vec3(0,0.25,0)
    self.prim_node_front.worldTransform.orientation = quat(vec3(0,1,0),θ)
    self.prim_node_back.worldTransform.translation = self.origin-vec3(0,0.25,0)
    self.prim_node_back.worldTransform.orientation = quat(vec3(0,1,0),-θ)

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

    self.fragments = []

    createSceneGraph(app=self,rendermodel="ForwardPBR")
    self.cam_overlay = self.layer1.createDrawableNode("camoverlay",self.uicam.createDrawable())

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
    # clip with plane
    ##################################

    f = Fragments(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  flip_orientation=False,
                  origin = vec3(2,0,-2),
                  slicing_plane=dplane(dvec3(0,1,0).normalized,-.4),
                  model_asset_path = "data://tests/simple_obj/tetra.obj" )

    self.fragments += [f]

    f = Fragments(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  flip_orientation=False,
                  origin = vec3(2,0,2),
                  slicing_plane=dplane(dvec3(0,1,1).normalized,+.5),
                  model_asset_path = "data://tests/simple_obj/cone.obj" )


    self.fragments += [f]

    f = Fragments(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  flip_orientation=False,
                  origin = vec3(0,0,0),
                  slicing_plane=dplane(dvec3(1,1,1).normalized,.5),
                  model_asset_path = "data://tests/simple_obj/box.obj" )


    self.fragments += [f]

    f = Fragments(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  flip_orientation=False,
                  origin = vec3(-2,0,-2),
                  slicing_plane=dplane(dvec3(1,0,0).normalized,0),
                  model_asset_path = "data://tests/simple_obj/torus.obj" )

    self.fragments += [f]

    f = Fragments(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  flip_orientation=False,
                  origin = vec3(-2,0,2),
                  slicing_plane=dplane(dvec3(1,0,0).normalized,0),
                  model_asset_path = "data://tests/simple_obj/uvsphere.obj" )

    self.fragments += [f]

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
  
  ################################################

  def onUpdate(self,updinfo):
    #########################################
    for f in self.fragments:
      f.update(updinfo.absolutetime)
    #########################################
    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()

