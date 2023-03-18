#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
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
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')

################################################################################

args = vars(parser.parse_args())

################################################################################

def printSubMesh(name, subm):
  print(subm)
  out_str = "submesh: %s\n" % name
  for idx, vtx in enumerate(subm.vertexpool.orderedVertices):
    out_str += " vtx> %d: %s\n"%(idx, vtx.position)
  for idx, poly in enumerate(subm.polys):
    out_str += "  poly> %d: [ "%idx
    for v in poly.vertices:
      out_str += "%d " % v.poolindex
    out_str += "]\n"
  print(out_str)

################################################################################

class Fragments:
  def __init__(self, #
               context=None, #
               layer=None, #
               pipeline=None, #
               slicing_plane=None,
               model_asset_path=None, #
               ): #

    if slicing_plane==None:
      nx = random.uniform(-1,1)
      ny = random.uniform(-1,1)
      nz = random.uniform(-1,1)
      self.normal = vec3(nx,ny,nz).normalized()
    else:
      self.normal = slicing_plane.normal

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

    self.ori_prim = meshutil.RigidPrimitive(self.ori_submesh,context)
    self.ori_sgnode = self.ori_prim.createNode("ori",layer,pipeline)
    self.ori_sgnode.enabled = False

    ##################################
    # strip UV's, normals and colors from submesh
    ##################################

    self.stripped = self.ori_submesh.copy(preserve_normals=False,
                                          preserve_colors=False,
                                          preserve_texcoords=False)

    printSubMesh("stripped", self.stripped)

    self.slicing_plane = slicing_plane

    ##################################
    # clip
    ##################################

    self.clipped = self.stripped.clipWithPlane( plane=self.slicing_plane,
                                                flip_orientation = False,
                                                close_mesh = True )

    self.front = self.clipped["front"].triangulate()
    self.back = self.clipped["back"].triangulate()
    self.prim_top = meshutil.RigidPrimitive(self.front,context)
    self.prim_bot = meshutil.RigidPrimitive(self.back ,context)
    self.prim_node_top = self.prim_top.createNode("top",layer,pipeline)
    self.prim_node_bot = self.prim_bot.createNode("bot",layer,pipeline)
    self.prim_node_top.enabled = True
    self.prim_node_bot.enabled = True

    ##################################

  def update(self,abstime):
    θ = abstime * math.pi * 2.0 * 0.03
    y = math.sin(θ*1.7)
    self.prim_node_top.worldTransform.translation = self.normal*y
    self.prim_node_bot.worldTransform.translation = self.normal*(-y)

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
                               techname="tek_fnormal" )

    material = pipeline.sharedMaterial
    pipeline.bindParam( material.param("m"), tokens.RCFD_M)



    ##################################
    # clip with plane
    ##################################

    f = Fragments(context = ctx,
                  layer=self.layer1,
                  pipeline=pipeline,
                  slicing_plane=plane(vec3(1,1,1).normalized(),-.5),
                  model_asset_path = "data://tests/simple_obj/cone.obj" )

    self.fragments += [f]

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

  ################################################

  def onUpdate(self,updinfo):
    #########################################
    for f in self.fragments:
      f.update(updinfo.absolutetime)
    #########################################
    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()

