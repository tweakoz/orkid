#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal
from orkengine.core import vec3, vec4, quat, mtx4, dfrustum, dvec4, fmtx4_to_dmtx4, CrcStringProxy, VarMap
from orkengine.core import lev2_pyexdir, Transform, thisdir
from orkengine import lev2

################################################################################

lev2_pyexdir.addToSysPath()
this_dir = thisdir()
this_dir.addToSysPath()

import _boilerplate as boilerplate

from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData, createImposter
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

tokens = CrcStringProxy()
IMP_DIM = 768
################################################################################

################################################################################

class ImposterApp(boilerplate.ImposterBaseApp):

  def __init__(self,is_stereo=False,extapp=None,envmap="pillars",statedebug=False):
    super().__init__(is_stereo=is_stereo,extapp=extapp,envmap=envmap,statedebug=statedebug)

  ##############################################

  def onGpuInit(self,ctx):

    super().onGpuInit(ctx)
      
    sceneparams = VarMap() 
    sceneparams.preset = self.RENDERMODEL
    sceneparams.SkyboxIntensity = float(0.5)
    sceneparams.SpecularIntensity = float(1)
    sceneparams.DiffuseIntensity = float(1)
    sceneparams.AmbientLight = vec3(0.0)
    sceneparams.DepthFogDistance = float(1e5)
    sceneparams.SkyboxTexPathStr = self.envmap

    ###################################
    # post fx node
    ###################################

    postNode = lev2.PostFxNodeHSVG()
    postNode.hue = 0.0
    postNode.saturation = 0.7
    postNode.value = 1.0
    postNode.gamma = 0.8
    postNode.gpuInit(ctx,8,8);
    postNode.addToSceneVars(sceneparams,"PostFxChain")
    self.post_node = postNode

    self.scene = self.ezapp.createScene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True

    ###################################
    # create model
    ###################################

    model = lev2.XgmModel("data://tests/misc_gltf_samples/art_and_sculpture/lion.glb")
    model.debugRenderingModel = tokens.ALL if self.statedebug else tokens.NONE
    model.debugPassID = tokens.PRIMARY if self.statedebug else tokens.NONE
    model.debugSubPassID = tokens.ALL if self.statedebug else tokens.NONE
    self.drawable_model = model.createDrawable()
    self.modelnode = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"model-node",self.drawable_model)
    self.modelnode.worldTransform.scale = 1.5
    self.modelnode.worldTransform.translation = vec3(0,1,0)

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData(extent=1000.0)
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(1.0)
    self.grid_data.intensityA = 1.0
    self.grid_data.intensityB = 0.97
    self.grid_data.intensityC = 0
    self.grid_data.intensityD = 0
    self.grid_data.lineWidth = 0.025
    self.grid_node = self.layer_fwd.createDrawableNodeFromData("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################
  # create imposter
  ##############################################

    if True:
      imposter = createImposter( context=ctx,
                                 radius=1.0,
                                 filtertype=tokens.BILINEAR,
                                 filterradius=3.0, 
                                 detail=3,
                                 shaderpath=this_dir/"i5.fxv2",
                                 shadertek="tek_imp",
                                 layer=self.layer_fwd,
                                 DIM = IMP_DIM,
                                 is_stereo=self.is_stereo,
                                 use_pbr=False )
      
      imp_mtl = imposter.imp_mtl
      imp_pass = imposter.impdata.imp_pass
      imp_pass.pipeline.bindParam(imp_mtl.param("time"), lambda: self.time*3.0)

      imposter.installStandardBlit()

      self.imposter = imposter

    # debug shader state ?      
    #imposter.impdata.imp_pass.debug_shaderstate = True
    #imposter.impdata.blit_pass.debug_shaderstate = True

    ###################################
    # create spotlights
    ###################################

    spotmodel = lev2.XgmModel("data://tests/pbr_calib.glb")

    lmgr = self.scene.lightingmanager
    color_cookies = lmgr.spot_cookies_color
    depth_cookies = lmgr.spot_cookies_depth
    color_cookies.needsRadianceCache = False
    color_cookies.resize(1024,1024,4,tokens.RGB8,True)
    depth_cookies.resize(1024,1024,4,tokens.Z32F,True)

    cookie1 = color_cookies.load("src://effect_textures/L0D.png")
    cookie2 = color_cookies.load("lev2://textures/transponder24.png")
    cookie3 = color_cookies.load("src://effect_textures/knob2.png")
    cookie4 = color_cookies.load("src://effect_textures/knob2.png")
    depth1 = depth_cookies.slice(0)
    depth2 = depth_cookies.slice(1)
    depth3 = depth_cookies.slice(2)
    depth4 = depth_cookies.slice(3)
    shadow_size = 2048
    shadow_bias = 1e-4
    intens_scale = 1.0
    speed_scale = 0.5
    ctx.TXI.updateTextureArray(color_cookies)
    if hasattr(self,"modelnode"):
      self.spotlight1 = MySpotLight(index=0,app=self,model=spotmodel,frq=0.17*speed_scale,color=vec3(0,150,0)*intens_scale,cookie=cookie1,depth_cookie=depth1,fovbase=60.0,fovamp=20.0,voffset=10,vscale=5,bias=shadow_bias,dim=shadow_size,radius=1.2)
      self.spotlight2 = MySpotLight(index=1,app=self,model=spotmodel,frq=0.37*speed_scale,color=vec3(300,0,0)*intens_scale,cookie=cookie2,depth_cookie=depth2,fovbase=60.0,fovamp=20.0,voffset=10,vscale=5,bias=shadow_bias,dim=shadow_size,radius=1.5)
      self.spotlight3 = MySpotLight(index=2,app=self,model=spotmodel,frq=0.57*speed_scale,color=vec3(100)*intens_scale,cookie=cookie3,depth_cookie=depth3,fovbase=60.0,fovamp=20.0,voffset=10,vscale=5,bias=shadow_bias,dim=shadow_size,radius=2.0)
      self.spotlight4 = MySpotLight(index=3,app=self,model=spotmodel,frq=0.97*speed_scale,color=vec3(0,0,200)*intens_scale,cookie=cookie4,depth_cookie=depth4,fovbase=70.0,fovamp=20.0,voffset=3,vscale=2,bias=shadow_bias,dim=shadow_size,radius=7)

  ################################################

  def onUpdate(self,updinfo):
    super().onUpdate(updinfo)

  ################################################

  def onGpuUpdate(self,ctx):
    if hasattr(self,"imposter"):
      self.imposter.onGpuUpdate(ctx)
      z = math.sin(self.imposter.frame_index*0.003)*2.0
      self.imposter.sgnode.worldTransform.translation = vec3(0,0.1,z)
      if hasattr(self,"spotlight1"):
        self.spotlight1.update(self.lighttime)
        self.spotlight2.update(self.lighttime)
        self.spotlight3.update(self.lighttime)
        self.spotlight4.update(self.lighttime)
    if hasattr(self,"sgnode_frustum"):
      self.layer_fwd.removeDrawableNode(self.sgnode_frustum )

###############################################################################

if __name__ == "__main__":
  boilerplate.run(ImposterApp)
