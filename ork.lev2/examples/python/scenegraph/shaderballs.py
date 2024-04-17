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

sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')

class MyCookie: 
  def __init__(self,path):
    self.path = path
    self.tex = Texture.load(path)
    self.irr = PbrCommon.requestIrradianceMaps(path)
    
class MySpotLight:
  def __init__(self,index,app,model,frq,color,cookie):
    self.cookie = cookie
    self.frequency = frq
    self.drawable_model = model.createDrawable()
    self.modelnode = app.scene.createDrawableNodeOnLayers(app.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 0.25
    self.modelnode.worldTransform.translation = vec3(0)
    self.spot_light = DynamicSpotLight()
    self.spot_light.data.color = color
    self.spot_light.data.fovy = math.radians(45)
    self.spot_light.data.shadowBias = 1e-3
    self.spot_light.data.shadowMapSize = 2048
    self.spot_light.lookAt(
      vec3(0,2,1)*4, # eye
      vec3(0,0,0), # tgt 
      vec3(0,1,0)) # up
    self.spot_light.data.range = 100.0
    self.spot_light.cookieTexture = cookie.tex
    self.spot_light.irradianceCookie = cookie.irr
    self.spot_light.shadowCaster = True
    print(self.spot_light.shadowMatrix)
    self.lnode = app.layer_fwd.createLightNode("spotlight%d"%index,self.spot_light)
    pass
  def update(self,abstime):
    phase = abstime*self.frequency
    ########################################
    x = math.sin(phase)
    y = 1+(math.sin(phase*self.frequency*12.0)+1)*10
    ty = 1.0 #math.sin(phase*2.0)
    z = math.cos(phase)
    fovy = 85#+math.sin(phase*3.5)*10
    self.spot_light.data.fovy = math.radians(fovy)
    LPOS =       vec3(x*15,y,z*15)

    self.spot_light.lookAt(
      LPOS, # eye
      vec3(0,ty,0), # tgt 
      vec3(0,1,0)) # up
    
    self.modelnode.worldTransform.translation = LPOS
    self.modelnode.worldTransform.orientation = quat(vec3(1,1,1).normalized(),phase*self.frequency*16)

################################################################################

args = vars(parser.parse_args())
envmap = args["envmap"]

################################################################################

class NODE(object):

  def __init__(self,model,app, index):

    super().__init__()
    self.model = model
    self.drawable_model = model.createDrawable()
    self.modelinst = self.drawable_model.modelinst
    self.sgnode = app.scene.createDrawableNodeOnLayers(app.fwd_layers,"model-node-%d"%index,self.drawable_model)
    self.sgnode.worldTransform.scale = 1
    self.sgnode.worldTransform.translation = vec3(0)
    #self.sgnode = model.createNode("node%d"%index,layer)

################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,12,15))
    self.nodes=[]

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(1.5),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.0),
      "DepthFogDistance": float(10000)
    }

    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap
    else:
      params_dict["SkyboxTexPathStr"] = "src://envmaps/blender_night.dds"

    createSceneGraph(app=self,
                     rendermodel="ForwardPBR",
                     params_dict=params_dict)

    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.layer1
    self.fwd_layers = [self.layer_fwd,self.layer_donly]

    ###################################

    model = XgmModel("data://tests/pbr_calib.glb")

    random.seed(12)
    for mesh in model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.texColor = Texture.load("src://effect_textures/white.dds")
        copy.texNormal = Texture.load("src://effect_textures/default_normal.dds")
        copy.texMtlRuf = Texture.load("src://effect_textures/white.dds")
        submesh.material = copy

    for i in range(81):
      node = NODE(model,self,i)

      x = (i % 9)
      z = int(i/9)

      ######################
      # set transform
      ######################

      node.sgnode.worldTransform.translation = vec3((x-4)*2,1,(z-4)*2)

      ######################
      # override material for submeshinst
      ######################

      subinst = node.modelinst.submeshinsts[0]
      mtl_cloned = subinst.material.clone()
      mtl_cloned.metallicFactor = float(x/8.0)
      mtl_cloned.roughnessFactor = float(z/8.0)
      r = random.uniform(0,1)
      g = random.uniform(0,1)
      b = random.uniform(0,1)
      mtl_cloned.baseColor = vec4(r,g,b,1)
      subinst.overrideMaterial(mtl_cloned)

      ######################


      self.nodes += [node]

    cookie3 = MyCookie("src://effect_textures/knob2.dds")
    
    #self.spotlight1 = MySpotLight(0,self,model,0.17,vec3(0,500,0),cookie1)
    #self.spotlight2 = MySpotLight(1,self,model,0.37,vec3(500,0,0),cookie2)
    self.spotlight3 = MySpotLight(2,self,model,0.27,vec3(500),cookie3)

    ###################################

    self.grid_data = createGridData()
    self.grid_data.texturepath = "src://effect_textures/white.dds"
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(0.5)
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

  ################################################
  def onGpuUpdate(self,ctx):
    self.spotlight3.update(self.lighttime)

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
