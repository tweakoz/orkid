#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, blending 6 lightmaps
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math
from orkengine.core import vec2, vec3, vec4, quat, mtx4
from orkengine import lev2 
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.app.loggerui import LoggerUIComponent

################################################################################
modelpath = "data://tests/environ/roomtest_lightmaps.glb"
################################################################################
class SceneGraphApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 eye=vec3(0,20,20),
                                 grid_variant=None)
    self.LUI = self.addComponent("loggerui", LoggerUIComponent, filter_regex=[".*"]) 
    self.createEzApp(ssaa=1)

  ##############################################

  def _onGpuInit(self,ctx):

    SGC = self.SGC
    SG = SGC.scenegraph

    # Apply custom scene parameters
    SG.pbr_common.skyboxLevel = float(0.0)
    SG.pbr_common.ambientLevel = vec3(0.0)
    SG.pbr_common.diffuseLevel = float(0.0)
    SG.pbr_common.specularLevel = float(0.0)
    SG.pbr_common.depthFogDistance = float(10000)

    self.model = lev2.XgmModel(modelpath)
    self.sgnode = self.model.createNode("node",SGC.layer1)
    self.pbr_common = SG.pbr_common
    
    ######################
    # override shader ?
    ######################

    self.modelinst = self.sgnode.user.pyext_retain_modelinst

    ###################################

    self.lmap_materials = []
    for m in self.model.meshes:
      for s in m.submeshes:
        mtl = s.material
        self.lmap_materials += [mtl]
    
  ################################################

  def _onGpuUpdate(self,ctx):

    for mtl in self.lmap_materials:
      phnx = math.pi*0.00+self.absolutetime*1.0
      phpx = math.pi*1.0+self.absolutetime*1.0
      phpz = math.pi*0.5+self.absolutetime*1.0
      phnz = math.pi*1.5+self.absolutetime*1.0
      phpy = math.pi*1.0+self.absolutetime*0.3
      phny = math.pi*0.5+self.absolutetime*0.6
      phnx = 0.5 + 0.5*math.sin(phnx)
      phnz = 0.5 + 0.5*math.sin(phnz)
      phpx = 0.5 + 0.5*math.sin(phpx)
      phpz = 0.5 + 0.5*math.sin(phpz)
      phny = 0.5 + 0.5*math.sin(phny)
      phpy = 0.5 + 0.5*math.sin(phpy)
      mtl.setActiveLightMap("nx",vec3(1,1,1)*phnx)
      mtl.setActiveLightMap("px",vec3(0,0,phpx))
      mtl.setActiveLightMap("ny",vec3(phny,phny,0))
      mtl.setActiveLightMap("py",vec3(0,phpy,phpy))
      mtl.setActiveLightMap("nz",vec3(0,0,phnz))
      mtl.setActiveLightMap("pz",vec3(0,phpz,phpz))

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
