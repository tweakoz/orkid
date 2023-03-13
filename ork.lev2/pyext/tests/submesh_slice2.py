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
numinstances = 500

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

    createSceneGraph(app=self,rendermodel="ForwardPBR")

    mesh = meshutil.Mesh()
    mesh.readFromWavefrontObj("data://tests/simple_obj/monkey.obj")
    submesh = mesh.submesh_list[0]
    slicing_plane = plane(vec3(0,1,0),0)
    clipped = submesh.clipWithPlane(slicing_plane)
    clipped_top = clipped["front"].triangulate()
    clipped_bot = clipped["back"].triangulate()

    self.prim_top = meshutil.RigidPrimitive(clipped_top,ctx)
    self.prim_bot = meshutil.RigidPrimitive(clipped_bot,ctx)

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               rendermodel = "ForwardPBR",
                               shaderfile=Path("orkshader://basic"),
                               techname="tek_wnormal" )

    material = pipeline.sharedMaterial
    pipeline.bindParam( material.param("m"), tokens.RCFD_M)

    self.prim_node_top = self.prim_top.createNode("top",self.layer1,pipeline)
    self.prim_node_bot = self.prim_bot.createNode("bot",self.layer1,pipeline)

    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

  ################################################

  def onUpdate(self,updinfo):
    θ = updinfo.absolutetime * math.pi * 2.0 * 0.1
    y = 0.5+math.sin(θ*1.7)*0.5
    self.prim_node_top.worldTransform.translation = vec3(0,1+y,1)
    self.prim_node_bot.worldTransform.translation = vec3(0,1-y,1)

    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()

