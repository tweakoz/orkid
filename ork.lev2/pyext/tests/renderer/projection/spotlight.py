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
    self.spot_light.lookAt(
      vec3(0,2,1)*4, # eye
      vec3(0,0,0), # tgt 
      vec3(0,1,0)) # up
    self.spot_light.data.range = 100.0
    self.spot_light.data.shadowBias = 1e-3
    self.spot_light.data.shadowMapSize = 512
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
    y = math.sin(phase*self.frequency*2.0)
    ty = math.sin(phase*2.0)
    z = math.cos(phase)
    fovy = 25+(1.0+math.sin(phase*3.5))*20
    self.spot_light.data.fovy = math.radians(fovy)
    LPOS =       vec3(x,1+y*0.5,z)*12

    self.spot_light.lookAt(
      LPOS, # eye
      vec3(0,ty+1,0), # tgt 
      vec3(0,1,0)) # up
    
    self.modelnode.worldTransform.translation = LPOS
    self.modelnode.worldTransform.orientation = quat(vec3(1,1,1).normalized(),phase*self.frequency*16)

################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=16,msaa=1)
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
      "SkyboxIntensity": float(0.5),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(10000)
    }

    #createSceneGraph(app=self,rendermodel="DeferredPBR",params_dict=params_dict)
    createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=params_dict)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.layer1
    self.fwd_layers = [self.layer_fwd,self.layer_donly]

    ###################################
    frust = dfrustum()
    frust .set(fmtx4_to_dmtx4(mtx4()),fmtx4_to_dmtx4(mtx4()))
    frustum_prim = primitives.FrustumPrimitive()
    frustum_prim.frustum = frust
    frustum_prim.topColor = dvec4(0.2,1.0,0.2,1)
    frustum_prim.bottomColor = dvec4(0.5,0.5,0.5,1)
    frustum_prim.leftColor = dvec4(0.2,0.2,1.0,1)
    frustum_prim.rightColor = dvec4(1.0,0.2,0.2,1)
    frustum_prim.nearColor = dvec4(0.0,0.0,0.0,1)
    frustum_prim.farColor = dvec4(1.0,1.0,1.0,1)
    self.frustum_prim = frustum_prim
    self.frustum = frust
    material = PBRMaterial()
    material.texColor = Texture.load("src://effect_textures/white.dds")
    material.texNormal = Texture.load("src://effect_textures/default_normal.dds")
    material.texMtlRuf = Texture.load("src://effect_textures/white.dds")
    material.metallicFactor = 1
    material.roughnessFactor = 1
    material.gpuInit(ctx)
    self.frustum_material = material
    ###################################

    model = XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 1
    self.modelnode.worldTransform.translation = vec3(0,2,0)

    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(0.4)
    self.grid_data.texturepath = "src://effect_textures/white.dds"
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################

    cookie1 = MyCookie("src://effect_textures/L0D.png")
    cookie2 = MyCookie("data://platform_lev2/textures/transponder24.dds")
    cookie3 = MyCookie("src://effect_textures/knob2.dds")
    
    self.spotlight1 = MySpotLight(0,self,model,0.17,vec3(0,5500,0),cookie1)
    self.spotlight2 = MySpotLight(1,self,model,0.37,vec3(5000,0,0),cookie2)
    self.spotlight3 = MySpotLight(2,self,model,0.57,vec3(800),cookie3)

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    self.spotlight1.update(self.lighttime)
    self.spotlight2.update(self.lighttime)
    self.spotlight3.update(self.lighttime)
    if hasattr(self,"sgnode_frustum"):
      self.layer1.removeDrawableNode(self.sgnode_frustum )

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
