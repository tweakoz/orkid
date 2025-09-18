################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys
from ork import path as ork_path
from orkengine.core import *
from orkengine.lev2 import *

SSAO_NUM_SAMPLES = 96

from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.scenegraph import createSceneGraph

class SpinningModelInst(object):

  def __init__(self,model,layer, index):

    super().__init__()

    self.model = model
    self.sgnode = model.createNode("node%d"%index,layer)
    self.pos = vec3(random.uniform(-1.5,1.5),
                    random.uniform(1,2),
                    random.uniform(-1.5,1.5))
    self.rot = quat(vec3(0,1,0),0)
    incraxis = vec3(random.uniform(-1,1),
                    random.uniform(-1,1),
                    random.uniform(-1,1)).normalized
    incrmagn = random.uniform(-0.01,0.01)
    self.rotincr = quat(incraxis,incrmagn)
    self.scale = 1.0
    self.sgnode.worldTransform.translation = self.pos 
    self.sgnode.worldTransform.scale = self.scale

  def update(self,deltatime):
    self.rot = self.rot*self.rotincr
    self.sgnode.worldTransform.orientation = self.rot 

class TurntableModelInst(object):

  def __init__(self,model,layer, index):

    super().__init__()

    self.model = model
    self.sgnode = model.createNode("node%d"%index,layer)
    
    self.pos = vec3(random.uniform(-4.5,4.5),
                    random.uniform(-2,2),
                    random.uniform(-4.5,4.5))
    self.pos = self.pos.normalized*3
    self.rot = quat(vec3(0,1,0),0)
    incraxis = vec3(0,1,0)
    incrmagn = random.uniform(-0.01,0.01)
    self.rotincr = quat(incraxis,incrmagn)
    self.scale = random.uniform(0.5,0.7)
    self.sgnode.worldTransform.translation = self.pos 
    self.sgnode.worldTransform.scale = self.scale

  def update(self,deltatime):
    self.rot = self.rot*self.rotincr
    self.sgnode.worldTransform.orientation = self.rot 

class BoilerplateSgApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=2)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,tgt=vec3(0,0,1),eye=vec3(0,0,0))
    self.modelinsts=[]
    self.ssaamode = False
    self.skybox = "nebula"
    self.skybox_intensity = 1.0

  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    sceneparams = VarMap() 
    sceneparams.preset = "ForwardPBR"
    sceneparams.SkyboxIntensity = float(self.skybox_intensity)
    sceneparams.SpecularIntensity = float(1)
    sceneparams.DiffuseIntensity = float(1)
    sceneparams.AmbientLight = vec3(0.0)
    sceneparams.DepthFogDistance = float(1e6)
    sceneparams.UseFloatBuffer = True
    sceneparams.SkyboxTexPathStr = self.skybox
    ###################################
    # post fx node
    ###################################
    postNode = PostFxNodeHSVG()
    postNode.hue = 0.0
    postNode.saturation = 0.85
    postNode.value = 1.0
    postNode.gamma = 1.2
    postNode.gpuInit(ctx,8,8);
    postNode.addToSceneVars(sceneparams,"PostFxChain")
    self.post_node = postNode
    ###################################
    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    self.pbr_common = self.scene.pbr_common

  ##############################################

  def onUiEvent(self,uievent):
    res = ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == ord("A"):
        if self.ssaamode == True:
          self.ssaamode = False
        else:
          self.ssaamode = True
        print("SSAO MODE",self.ssaamode)
        return res
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    else:
      handled = ui.HandlerResult()
    return res
  
  ################################################

  def onUpdate(self,updinfo):

    if self.ssaamode == True:
      self.pbr_common.ssaoNumSamples = SSAO_NUM_SAMPLES
    else:
      self.pbr_common.ssaoNumSamples = 0

    self.scene.updateScene(self.cameralut) 
