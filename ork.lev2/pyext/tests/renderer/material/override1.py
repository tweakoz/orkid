#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.misc import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

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
    setupUiCamera(app=self,eye=vec3(0,0.5,3))
    self.modelinsts=[]

  ##############################################

  def onGpuInit(self,ctx):

    createSceneGraph(app=self,rendermodel="DeferredPBR")

    model = XgmModel("data://tests/pbr_calib.glb")
    for mesh in model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.texColor = Texture.load("src://effect_textures/white.dds")
        copy.texNormal = Texture.load("src://effect_textures/default_normal.dds")
        copy.texMtlRuf = Texture.load("src://effect_textures/green.dds")
        submesh.material = copy

    ###################################

    for i in range(numinstances):
      self.modelinsts += [modelinst(model,self.layer1,i)]

    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):

    for minst in self.modelinsts:
      minst.update(updinfo.deltatime)

    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
