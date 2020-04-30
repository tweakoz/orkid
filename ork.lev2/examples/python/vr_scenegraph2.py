#!/usr/bin/env python3
################################################################################
# lev2 sample which renders a scenegraph to an OpenVR connected HMD
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, random
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
class modelinst(object):
  def __init__(self,model,layer):
    super().__init__()
    self.model = model
    self.sgnode = model.createNode("node1",layer)
    self.pos = vec3(random.uniform(-5,5),
                    random.uniform(-5,5),
                    random.uniform(-2.5,-25))
    self.rot = quat(vec3(0,1,0),0)
    incraxis = vec3(random.uniform(-1,1),
                    random.uniform(-1,1),
                    random.uniform(-1,1)).normal()
    incrmagn = random.uniform(-0.01,0.01)
    self.rotincr = quat(incraxis,incrmagn)
    self.scale = random.uniform(0.5,0.7)
  def update(self,deltatime):
    self.rot = self.rot*self.rotincr
    self.sgnode\
        .worldMatrix\
        .compose( self.pos, # pos
                  self.rot, # rot
                  self.scale) # scale
################################################################################
class VrApp(object):
  ################################################
  def __init__(self):
    super().__init__()
    self.sceneparams = VarMap()
    self.sceneparams.preset = "PBRVR"
    self.qtapp = OrkEzQtApp.create(self)
    self.qtapp.setRefreshPolicy(RefreshFastest, 0)
    self.modelinsts=[]
  ##############################################
  def onGpuInit(self,ctx):
    layer = self.scene.createLayer("layer1")
    model = Model("srcdata://tests/pbr1/pbr1")
    ###################################
    for i in range(100):
      self.modelinsts += [modelinst(model,layer)]
    ###################################
    self.camera = CameraData()
    self.camera.perspective(0.1, 100.0, 45.0)
    self.cameralut = CameraDataLut()
    self.cameralut.addCamera("spawncam",self.camera)
  ################################################
  def onUpdate(self,updinfo):
    ###################################
    for minst in self.modelinsts:
      minst.update(updinfo.deltatime)
    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
################################################
app = VrApp()
app.qtapp.exec()
