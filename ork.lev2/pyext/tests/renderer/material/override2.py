#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, colorsys
from orkengine.core import vec3, vec4, quat, mtx4, mathconstants
from orkengine.core import dfrustum, dvec4, fmtx4_to_dmtx4 
from orkengine.core import lev2_pyexdir, Transform
from orkengine.core import CrcStringProxy, thisdir, VarMap
from orkengine import lev2

constants = mathconstants()
tokens = CrcStringProxy()

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-a", "--ambient", type=float, default=0.0, help='ambient intensity')
parser.add_argument("-s", "--specular", type=float, default=1.0, help='specular intensity')
parser.add_argument("-d", "--diffuse", type=float, default=1.0, help='diffuse intensity')
parser.add_argument("-i", "--skybox", type=float, default=1.3, help='skybox envlight intensity')

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
    self.ezapp = lev2.OrkEzApp.create(self,ssaa=0,fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.materials = set()
    self.nodes=[]
    self.camera = lev2.CameraData()
    self.cameralut = lev2.CameraDataLut()
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
                     #rendermodel="DeferredPBR",
#                    rendermodel="ForwardPBR",
                     params_dict=params_dict)

    self.pbrcommon = self.scene.pbr_common

    ###################################

    model = lev2.XgmModel("data://tests/pbr_calib.glb")

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

    random.seed(self.seed)
    for i in range(81):
      node = NODE(model,self.layer1,i)

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
      mtl_cloned.metallicFactor = float(z/8.0)
      mtl_cloned.roughnessFactor = 1.0-float(x/8.0)
      subinst.overrideMaterial(mtl_cloned)

      ######################

      self.nodes += [node]

    ###################################

    self.regenColors()

    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ################################################

  def regenColors(self):
    #print("SEED: %d"%self.seed)
    random.seed(self.seed)
    self.seed = self.seed+1
    for i in range(81):
      node = self.nodes[i]
      subinst = node.modelinst.submeshinsts[0]
      mtl_cloned = subinst.material
      h = random.uniform(0,6)
      s = random.uniform(0,0.7)
      v = random.uniform(0.1,1)
      rgb = colorsys.hsv_to_rgb(h,s,v)
      r = rgb[0]
      g = rgb[1]
      b = rgb[2]
      mtl_cloned.baseColor = vec4(r,g,b,1)

  ################################################

  def onUpdate(self,updinfo):
    phase = updinfo.absolutetime * 0.2
    x =  math.sin(phase)*10    
    z = -math.cos(phase)*10    
    ###################################
    self.camera.perspective(0.1, 50.0, 35.0*constants.DTOR)
    self.camera.lookAt(vec3(x,5,z)*2.5, # eye
                       vec3(0, 0, 0), # tgt
                       vec3(0, 1, 0)) # up
    self.scene.updateScene(self.cameralut) 

  ##############################################

  def onUiEvent(self,uievent):
    if uievent.code in [tokens.KEY_DOWN.hashed, tokens.KEY_REPEAT.hashed]:
      if uievent.keycode == ord(" "): # spacebar
        self.regenColors()
      if uievent.keycode == ord("-"): # -
        self.ambient -= 0.05
        print("AMBIENT",self.ambient)
      if uievent.keycode == ord("="): # =
        self.ambient += 0.05
        print("AMBIENT",self.ambient)
      if uievent.keycode == ord("["): # [
        self.specular -= 0.05
        print("SPECULAR",self.specular)
      if uievent.keycode == ord("]"): # ]
        self.specular += 0.05
        print("SPECULAR",self.specular)
      if uievent.keycode == ord(","):
        self.pbrcommon.roughnessPower *= 0.95
        print("ROUGHNESS POWER",self.pbrcommon.roughnessPower)
      if uievent.keycode == ord("."):
        self.pbrcommon.roughnessPower *= 1.05
        print("ROUGHNESS POWER",self.pbrcommon.roughnessPower)
      ##############################
      self.pbrcommon.specularLevel = self.specular
      self.pbrcommon.ambientLevel = vec3(self.ambient)
    return lev2.ui.HandlerResult()

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
