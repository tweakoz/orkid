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

    #createSceneGraph(app=self,rendermodel="DeferredPBR",params_dict=params_dict)
    createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=params_dict)

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
    self.sgnode = model.createNode("nodea",self.layer1)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.sgnode.worldTransform.scale = 1
    self.sgnode.worldTransform.translation = vec3(0,2,0)

    self.sgnode_l = model.createNode("nodea",self.layer1)
    self.modelinst_l = self.sgnode_l.user.pyext_retain_modelinst
    self.sgnode_l.worldTransform.scale = 0.1
    self.sgnode_l.worldTransform.translation = vec3(0)

    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = ""
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################

    cookie_path = "src://effect_textures/L0D.png"

    self.spot_light = DynamicSpotLight()
    self.spot_light.data.color = vec3(200)
    self.spot_light.data.fovy = math.radians(45)
    self.spot_light.lookAt(
      vec3(0,2,1)*4, # eye
      vec3(0,0,0), # tgt 
      vec3(0,1,0)) # up
    self.spot_light.data.range = 100.0
    self.spot_light.cookieTexture = Texture.load(cookie_path)
    self.spot_light.irradianceCookie = PbrCommon.requestIrradianceMaps(cookie_path)
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
    phase = updinfo.absolutetime    *0.1
    ########################################
    x = math.sin(phase)
    z = math.cos(phase)
    fovy = 45 + math.sin(phase*3)*20
    self.spot_light.data.fovy = math.radians(fovy)
    LPOS =       vec3(x,2,z)*4

    self.spot_light.lookAt(
      LPOS, # eye
      vec3(0,0,0), # tgt 
      vec3(0,1,0)) # up
    
    self.sgnode_l.worldTransform.translation = LPOS
    
    ########################################

    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):

    if hasattr(self,"sgnode_frustum"):
      self.layer1.removeDrawableNode(self.sgnode_frustum )

    #slvp = fmtx4_to_dmtx4(self.spot_light.viewMatrix)
    #slpp = fmtx4_to_dmtx4(self.spot_light.projectionMatrix)
    #self.frustum.set(slvp,slpp)
    #self.frustum_prim.gpuInit(ctx)
    #self.sgnode_frustum = self.frustum_prim.createNodeWithMaterial("frustum",self.layer1,self.frustum_material)
    pass 

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
