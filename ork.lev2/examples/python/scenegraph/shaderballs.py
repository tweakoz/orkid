#!/usr/bin/env python3

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

tokens = CrcStringProxy()

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

SSAO_NUM_SAMPLES = 64


################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')


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
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=0)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,12,15),near=0.1,far=100)
    self.nodes=[]
    self.ssaamode = False

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(1.5),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0.0),
      "DepthFogDistance": float(10000),
      "SSAONumSamples": SSAO_NUM_SAMPLES,
      "SSAONumSteps": 4,
      "SSAOBias": 0.001,
      "SSAORadius": 1.0*25.4/1000.0, # 2 inches
      "SSAOWeight": 0.5,
      "SSAOPower": 0.5,
    }

    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap
    else:
      params_dict["SkyboxTexPathStr"] = "src://envmaps/blender_night"

    createSceneGraph(app=self,
                     rendermodel="ForwardPBR",
                     params_dict=params_dict)

    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.layer1
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True
    self.pbr_common.useDepthPrepass = True

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

    cookie3 = MyCookie("src://effect_textures/knob2.png")
    
    #self.spotlight1 = MySpotLight(0,self,model,0.17,vec3(0,500,0),cookie1)
    #self.spotlight2 = MySpotLight(1,self,model,0.37,vec3(500,0,0),cookie2)
    self.spotlight3 = MySpotLight( index=2,
                                  app=self,
                                  model=model,
                                  frq=0.27,
                                  color=vec3(500,500,400),
                                  cookie=cookie3,
                                  radius=16,
                                  bias=1e-3,
                                  dim=2048,
                                  fovamp=0,
                                  range=200.0,
                                  fovbase=65,
                                  voffset=16,
                                  vscale=14)

    ###################################

    self.grid_data = createGridData()
    self.grid_data.shader_suffix = "_V4"
    self.grid_data.modcolor = vec3(1.5)
    self.grid_draw = self.grid_data.createDrawable()
    self.grid_node = self.scene.createDrawableNodeOnLayers(self.fwd_layers,"grid",self.grid_draw)
    #self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ################################################

  def onUiEvent(self,uievent):
    res = lev2.ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == ord("A"):
        if self.ssaamode == True:
          self.ssaamode = False
        else:
          self.ssaamode = True
        print("SSAO MODE",self.ssaamode)
        return res
      if uievent.keycode == ord("-"):
        self.pbr_common.roughnessPower *= 0.95
        print("ROUGHNESS POWER",self.pbr_common.roughnessPower)
      if uievent.keycode == ord("="):
        self.pbr_common.roughnessPower *= 1.05
        print("ROUGHNESS POWER",self.pbr_common.roughnessPower)
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    else:
      handled = lev2.ui.HandlerResult()
    return res

  ################################################

  def onGpuUpdate(self,ctx):
    self.spotlight3.update(self.lighttime)

  ################################################

  def onUpdate(self,updinfo):
    if self.ssaamode == True:
      self.pbr_common.ssaoNumSamples = SSAO_NUM_SAMPLES
    else:
      self.pbr_common.ssaoNumSamples = 0
    self.lighttime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
