#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal, colorsys
from orkengine.core import vec3, vec4, quat, mtx4
from orkengine.core import dfrustum, dvec4, fmtx4_to_dmtx4 
from orkengine.core import lev2_pyexdir, Transform
from orkengine.core import CrcStringProxy, thisdir, VarMap
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent, StdSpotLight
from ork.app.loggerui import LoggerUIComponent

tokens = CrcStringProxy()

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

SSAO_NUM_SAMPLES = 16


################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-i", "--intensity", type=float, default=1.5, help='envmap intensity')


################################################################################

args = vars(parser.parse_args())
envmap = args["envmap"]
inten = args["intensity"]

################################################################################

class NODE(object):

  def __init__(self,model,app, index):

    super().__init__()
    SGC = app.SGC
    SG = SGC.scenegraph
    self.model = model
    self.drawable_model = model.createDrawable()
    self.modelinst = self.drawable_model.modelinst
    self.sgnode = SG.createDrawableNodeOnLayers(SGC.fwd_layers,"model-node-%d"%index,self.drawable_model)
    self.sgnode.worldTransform.scale = 1
    self.sgnode.worldTransform.translation = vec3(0)
    #self.sgnode = model.createNode("node%d"%index,layer)

################################################################################

class SceneGraphApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.materials = set()
    self.nodes=[]
    self.ssaomode = False
    params_dict = {
      "SkyboxIntensity": float(inten),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0),
      "DepthFogDistance": float(10000),
      "SSAONumSamples": int(SSAO_NUM_SAMPLES),
      "SSAONumSteps": 2,
      "SSAOBias": 0.005,
      "SSAORadius": 0.05, # 2 inches
      "SSAOWeight": 0.25,
      "SSAOPower": 0.125,
      "SSAOFeedback": 1.0/16.0,
      "SkyboxTexPathStr": envmap if envmap != "" else "ork_envmaps|blender_night"
    }
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 enable_ui_camera=True,
                                 eye=vec3(0,20,20),
                                 sg_params=params_dict )
    self.createEzApp(name="ShaderBalls", ssaa=1)

  ##############################################

  def _onGpuInit(self,ctx):
    SGC = self.SGC
    SG = SGC.scenegraph

    #self.layer_donly = self.scene.createLayer("depth_prepass")
    #self.layer_fwd = self.layer1
    #self.fwd_layers = [self.layer_fwd,self.layer_donly]
    pbr_common = SG.pbr_common
    pbr_common.useFloatColorBuffer = True
    pbr_common.useDepthPrepass = True
    pbr_common.dppZBias = 1.0e-4

    SGC.rendernode.debugRenderingModel = tokens.DEPTH_PREPASS # NONE ALL FORWARD_PBR
    SGC.rendernode.debugPassID = tokens.SHADOW # PROBE MAIN
    SGC.rendernode.debugSubPassID = tokens.ALL # tokens.FORWARD_PBR

    ###################################

    model = lev2.XgmModel("data://tests/pbr_calib.glb")



    random.seed(12)
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    for mesh in model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.assignImages(
          ctx,
          color = white,
          normal = normal,
          mtlruf = white,
          doConform=True
        )
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
      h = random.uniform(0,6)
      s = random.uniform(0,0.7)
      v = random.uniform(0.1,1)
      rgb = colorsys.hsv_to_rgb(h,s,v)
      r = rgb[0]
      g = rgb[1]
      b = rgb[2]
      mtl_cloned.baseColor = vec4(r,g,b,1)
      subinst.overrideMaterial(mtl_cloned)

      ######################

    self.nodes += [node]

    if False:
      lmgr = SG.lightingmanager
      COOKIE_DIM = 2048
      color_cookies = lev2.TextureArray(w=COOKIE_DIM,h=COOKIE_DIM,slices=4,fmt=tokens.RGB8,mipmapped=True)
      depth_cookies = lev2.TextureArray(w=COOKIE_DIM,h=COOKIE_DIM,slices=4,fmt=tokens.Z32F,mipmapped=True)
      color_cookies.needsRadianceCache = False

      cookie1 = color_cookies.load("src://effect_textures/knob2.png")
      ctx.TXI.updateTextureArray(color_cookies)
      depth_cookie1 = depth_cookies.slice(0)

      self.spotlight1 = StdSpotLight( index=0,
                                      SGC=SGC,
                                      model=model,
                                      frq=0.17,
                                      color=vec3(1000,800,500),
                                      cookie=cookie1,
                                      depth_cookie=depth_cookie1, 
                                      dim=COOKIE_DIM,
                                      radius=24,
                                      voffset=10,
                                      fovbase=25)

  ################################################

  def onUiEvent(self,uievent):
    pbrc = self.SGC.pbr_common
    res = lev2.ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == ord("A"):
        if self.ssaomode == True:
          self.ssaomode = False
        else:
          self.ssaomode = True
        print("SSAO MODE",self.ssaomode)
        return res
      if uievent.keycode == ord("-"):
        pbrc.roughnessPower *= 0.95
        print("ROUGHNESS POWER",pbrc.roughnessPower)
      if uievent.keycode == ord("="):
        pbrc.roughnessPower *= 1.05
        print("ROUGHNESS POWER",pbrc.roughnessPower)
    #handled = self.uicam.uiEventHandler(uievent)
    #if handled:
      #self.camera.copyFrom( self.uicam.cameradata )
    #else:
    #  handled = lev2.ui.HandlerResult()
    return res

  ################################################

  def _onGpuUpdate(self,ctx):
    if hasattr(self,"spotlight1"):
      self.spotlight1.update(self.lighttime)

  ################################################

  def _onUpdate(self,updinfo):
    pbrc = self.SGC.pbr_common
    if self.ssaomode == True:
      pbrc.ssaoNumSamples = SSAO_NUM_SAMPLES
    else:
      pbrc.ssaoNumSamples = 0
    self.lighttime = updinfo.absolutetime

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
