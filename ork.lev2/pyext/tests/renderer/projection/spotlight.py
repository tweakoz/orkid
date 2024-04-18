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
from common.lighting import MySpotLight, MyCookie

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
################################################################################
args = vars(parser.parse_args())
################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=0,msaa=1)
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
      "DepthFogDistance": float(10000),
      "supersample": "1",
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
