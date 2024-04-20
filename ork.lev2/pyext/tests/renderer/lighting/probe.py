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
tokens = CrcStringProxy()

class LIGHTING_APP(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=2,msaa=1)
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
    self.grid_data.modcolor = vec3(0.1)
    self.grid_data.texturepath = "src://effect_textures/white.dds"
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################

    cookie = MyCookie("src://effect_textures/L0D.png")
    
    shadow_size = 2048
    shadow_bias = 1e-4
    self.spotlight1 = MySpotLight( index=0,
                                   app=self,
                                   model=model,
                                   frq=0.17,
                                   color=vec3(0,700,1500),
                                   cookie=cookie,
                                   fovbase=60.0,
                                   fovamp=20.0,
                                   voffset=8,
                                   vscale=6,
                                   bias=shadow_bias,
                                   dim=shadow_size,
                                   radius=6)

    ##############################################

    self.probe = LightProbe()
    self.probe.type = tokens.REFLECTION
    self.probe.imageDim = 1024
    self.probe.worldMatrix = mtx4()
    self.probe.name = "probe1"
    self.probe_node = self.layer1.createLightProbeNode("probe",self.probe)

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

###############################################################################

LIGHTING_APP().ezapp.mainThreadLoop()
