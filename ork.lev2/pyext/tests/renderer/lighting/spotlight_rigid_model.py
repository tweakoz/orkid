#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, dfrustum, dvec4, fmtx4_to_dmtx4 
from orkengine.core import lev2_pyexdir, Transform
from orkengine import lev2

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
################################################################################
args = vars(parser.parse_args())
################################################################################

class StereoApp1(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=0,msaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,12,15))

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(1.5),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.1),
      "DepthFogDistance": float(10000),
      "supersample": "1",
    }

    #createSceneGraph(app=self,rendermodel="DeferredPBR",params_dict=params_dict)
    createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=params_dict)

    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useDepthPrepass = True

    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.layer1
    self.fwd_layers = [self.layer_fwd,self.layer_donly]

    ###################################
    frust = dfrustum()
    frust .set(fmtx4_to_dmtx4(mtx4()),fmtx4_to_dmtx4(mtx4()))
    frustum_prim = lev2.primitives.FrustumPrimitive()
    frustum_prim.frustum = frust
    frustum_prim.topColor = dvec4(0.2,1.0,0.2,1)
    frustum_prim.bottomColor = dvec4(0.5,0.5,0.5,1)
    frustum_prim.leftColor = dvec4(0.2,0.2,1.0,1)
    frustum_prim.rightColor = dvec4(1.0,0.2,0.2,1)
    frustum_prim.nearColor = dvec4(0.0,0.0,0.0,1)
    frustum_prim.farColor = dvec4(1.0,1.0,1.0,1)
    self.frustum_prim = frustum_prim
    self.frustum = frust
    material = lev2.PBRMaterial()
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    material.assignImages(
      ctx,
      color = white,
      normal = normal,
      mtlruf = white,
      doConform=True
    )
    material.metallicFactor = 1
    material.roughnessFactor = 1
    material.gpuInit(ctx)
    self.frustum_material = material
    ###################################

    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 1
    self.modelnode.worldTransform.translation = vec3(0,2,0)

    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(.7)
    self.grid_data.intensityA = 1.0*0.3
    self.grid_data.intensityB = 0.97*0.3
    self.grid_data.intensityC = 0
    self.grid_data.intensityD = 0
    self.grid_data.lineWidth = 0.025
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################

    cookie1 = MyCookie("src://effect_textures/L0D.png")
    cookie2 = MyCookie("data://platform_lev2/textures/transponder24")
    cookie3 = MyCookie("src://effect_textures/knob2")
    cookie4 = MyCookie("src://effect_textures/knob2")
    
    shadow_size = 2048
    shadow_bias = 1e-4
    intens_scale = 0.5
    speed_scale = 0.5
    self.spotlight1 = MySpotLight(index=0,app=self,model=model,frq=0.17*speed_scale,color=vec3(0,5500,0)*intens_scale,cookie=cookie1,fovbase=60.0,fovamp=20.0,voffset=15,vscale=13,bias=shadow_bias,dim=shadow_size,radius=12)
    self.spotlight2 = MySpotLight(index=1,app=self,model=model,frq=0.37*speed_scale,color=vec3(5000,0,0)*intens_scale,cookie=cookie2,fovbase=60.0,fovamp=20.0,voffset=15,vscale=13,bias=shadow_bias,dim=shadow_size,radius=12)
    self.spotlight3 = MySpotLight(index=2,app=self,model=model,frq=0.57*speed_scale,color=vec3(800)*intens_scale,cookie=cookie3,fovbase=60.0,fovamp=20.0,voffset=15,vscale=13,bias=shadow_bias,dim=shadow_size,radius=12)
    self.spotlight4 = MySpotLight(index=3,app=self,model=model,frq=0.97*speed_scale,color=vec3(0,0,600)*intens_scale,cookie=cookie4,fovbase=70.0,fovamp=20.0,voffset=3,vscale=2,bias=shadow_bias,dim=shadow_size,radius=7)

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return lev2.ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    self.lighttime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) 

  def onGpuUpdate(self,ctx):
    self.spotlight1.update(self.lighttime)
    self.spotlight2.update(self.lighttime)
    self.spotlight3.update(self.lighttime)
    self.spotlight4.update(self.lighttime)
    if hasattr(self,"sgnode_frustum"):
      self.layer1.removeDrawableNode(self.sgnode_frustum )

###############################################################################

StereoApp1().ezapp.mainThreadLoop()
