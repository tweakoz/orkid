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

sys.path.append((thisdir()/".."/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-a", "--ambient", type=float, default=0.0, help='ambient intensity')
parser.add_argument("-s", "--specular", type=float, default=1.0, help='specular intensity')
parser.add_argument("-d", "--diffuse", type=float, default=1.0, help='diffuse intensity')
parser.add_argument("-i", "--skybox", type=float, default=2.0, help='skybox envlight intensity')

################################################################################

args = vars(parser.parse_args())
envmap = args["envmap"]
ambient = args["ambient"]
specular = args["specular"]
diffuse = args["diffuse"]
skybox = args["skybox"]

################################################################################

class NODE(object):

  def __init__(self,model,layer, index):

    super().__init__()
    self.model = model
    self.sgnode = model.createNode("node%d"%index,layer)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.sgnode.worldTransform.scale = 1

################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    self.node = None
    self.camera = CameraData()
    self.cameralut = CameraDataLut()
    self.cameralut.addCamera("spawncam",self.camera)
    self.seed = 12
    self.ambient = ambient
    self.specular = specular
    self.diffuse = diffuse

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": skybox,
      "SpecularIntensity": specular,
      "DiffuseIntensity": diffuse,
      "AmbientLight": vec3(ambient),
      "DepthFogDistance": float(10000)
    }
    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap
    else:
      params_dict["SkyboxTexPathStr"] = "src://envmaps/blender_night.dds"

    createSceneGraph(app=self,
                     rendermodel="DeferredPBR",
                     params_dict=params_dict)

    self.pbrcommon = self.rendernode.pbr_common

    ###################################

    model = XgmModel("data://tests/monkey_pbr.glb")
    comp_model = meshutil.Mesh()
    comp_model.readFromWavefrontObj("data://tests/monkey_pbr.obj")
    computeAmbientOcclusion(comp_model,ctx)
    
    self.node = NODE(model,self.layer1,0)
    subinst = self.node.modelinst.submeshinsts[0]

    assert(False)
    
  ################################################

  def onUpdate(self,updinfo):
    phase = updinfo.absolutetime * 0.2
    x =  math.sin(phase)*10    
    z = -math.cos(phase)*10    
    ###################################
    self.camera.perspective(0.1, 50.0, 35.0*constants.DTOR)
    self.camera.lookAt(vec3(x,5,z)*0.5, # eye
                       vec3(0, 0, 0), # tgt
                       vec3(0, 1, 0)) # up
    self.scene.updateScene(self.cameralut) 

  ##############################################

  def onUiEvent(self,uievent):
    if uievent.code in [tokens.KEY_DOWN.hashed, tokens.KEY_REPEAT.hashed]:
      if uievent.keycode == 32: # spacebar
        self.regenColors()
      if uievent.keycode == 45: # -
        self.ambient -= 0.05
      if uievent.keycode == 61: # =
        self.ambient += 0.05
      if uievent.keycode == 91: # [
        self.specular -= 0.05
      if uievent.keycode == 93: # ]
        self.specular += 0.05
      ##############################
      self.pbrcommon.specularLevel = self.specular
      self.pbrcommon.ambientLevel = vec3(self.ambient)
    return ui.HandlerResult()

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
