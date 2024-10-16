#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys
from orkengine.core import *
from orkengine.lev2 import *

################################################################################

sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.misc import *
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')

################################################################################

args = vars(parser.parse_args())
numinstances = 500

################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera(app=self,eye=vec3(0,0.5,3))
    self.modelinsts=[]

  ##############################################

  def onGpuInit(self,ctx):

    createSceneGraph(app=self,rendermodel="DeferredPBR")

    model =XgmModel("data://tests/pbr_calib.glb")
    print(model)
    print(model.materials)
    print(model.meshes)
    myRay = ray3(vec3(0,0,0), vec3(1,0.01,0.01))
    isect_int = vec3(0,0,0)
    isect_out = vec3(0,0,0)
    if model.intersectBoundingBox(myRay, isect_int, isect_out):
      print(isect_int)
      print(isect_out)
    else:
      print("no intersection with", myRay)
    myRay = ray3(vec3(0,0,0), vec3(-1,0.01,0.01))
    if model.intersectBoundingBox(myRay, isect_int, isect_out):
      print(isect_int)
      print(isect_out)
    else:
      print("no intersection with", myRay)
    for mesh in model.meshes:
      print(mesh.submeshes)
      print(mesh.boundingRadius)
      print(mesh.boundingCenter)
      for submesh in mesh.submeshes:
        print(submesh)
        print(submesh.material)
        print(submesh.material.colorMapName)
        print(submesh.material.normalMapName)
        print(submesh.material.mtlRufMapName)
        print(submesh.material.amboccMapName)
        print(submesh.material.emissiveMapName)
        print(submesh.material.texColor)
        print(submesh.material.texNormal)
        print(submesh.material.texMtlRuf)
        print(submesh.material.texEmissive)

    ###################################

    for i in range(numinstances):
      self.modelinsts += [modelinst(model,self.layer1,i)]

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

    for minst in self.modelinsts:
      minst.update(updinfo.deltatime)

    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
