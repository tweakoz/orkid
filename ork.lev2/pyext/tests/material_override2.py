#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')

################################################################################

args = vars(parser.parse_args())

################################################################################

class NODE(object):

  def __init__(self,model,layer, index):

    super().__init__()
    self.model = model
    self.sgnode = model.createNode("node%d"%index,layer)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst

    x = (index % 7)-3
    z = int(index/7)-3

    self.sgnode.worldTransform.translation = vec3(x*2,1,z*2)
    self.sgnode.worldTransform.scale = 1

    self.rot = quat(vec3(0,1,0),0)
    incraxis = vec3(random.uniform(-1,1),
                    random.uniform(-1,1),
                    random.uniform(-1,1)).normalized()
    incrmagn = random.uniform(-0.01,0.01)
    self.rotincr = quat(incraxis,incrmagn)


  def update(self,deltatime):
    self.rot = self.rot*self.rotincr
    #self.sgnode.worldTransform.orientation = self.rot 

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
      "SkyboxIntensity": float(2),
      "SpecularIntensity": float(1),
    }
    createSceneGraph(app=self,
                     rendermodel="DeferredPBR",
                     params_dict=params_dict)

    ###################################

    model = XgmModel("data://tests/pbr_calib.glb")

    for mesh in model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.texColor = Texture.load("src://effect_textures/white.dds")
        copy.texNormal = Texture.load("src://effect_textures/default_normal.dds")
        copy.texMtlRuf = Texture.load("src://effect_textures/cyan.dds")
        submesh.material = copy

    for i in range(49):
      node = NODE(model,self.layer1,i)
      x = (i % 7)
      z = int(i/7)
      subinst = node.modelinst.submeshinsts[0]
      mtl_cloned = subinst.material.clone()
      mtl_cloned.metallicFactor = float(x/7.0)
      mtl_cloned.roughnessFactor = float(z/7.0)
      subinst.overrideMaterial(mtl_cloned)
      self.nodes += [node]

    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

  ################################################

  def onUpdate(self,updinfo):

    for minst in self.nodes:
      minst.update(updinfo.deltatime)

    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
