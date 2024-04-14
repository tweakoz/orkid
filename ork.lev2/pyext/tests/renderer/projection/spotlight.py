#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

lev2_pyexdir.addToSysPath()
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

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,12,15))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(2),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(10000)
    }

    createSceneGraph(app=self,rendermodel="DeferredPBR",params_dict=params_dict)

    ###################################

    model = XgmModel("data://tests/pbr_calib.glb")
    self.sgnode = model.createNode("nodea",self.layer1)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.sgnode.worldTransform.scale = 1
    self.sgnode.worldTransform.translation = vec3(0,2,0)

    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = ""
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################

    tex = Texture.load("src://effect_textures/noiseX.png")

    self.spot_light = DynamicSpotLight()
    self.spot_light.lookAt(
      vec3(1,1,1)*8, # eye
      vec3(0,0,0), # tgt 
      vec3(0,1,0)) # up
    self.spot_light.data.fovy = math.radians(45)
    self.spot_light.data.range = 10.0
    self.spot_light.cookieTexture = tex
    print(self.spot_light.shadowMatrix)
    self.lnode = self.layer1.createLightNode("spotlight",self.spot_light)

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    
    ########################################

    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    # just need a mainthread python callback
    # so python can process ctrl-c signals...
    pass 

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
