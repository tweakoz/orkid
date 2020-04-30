#!/usr/bin/env python3
################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, _shaders
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
class PyOrkApp(object):
  ################################################
  def __init__(self):
    super().__init__()
    self.sceneparams = VarMap()
    self.sceneparams.preset = "PBR"
    self.qtapp = OrkEzQtApp.create(self)
    self.qtapp.setRefreshPolicy(RefreshFastest, 0)
    self.scene = scenegraph.Scene(self.sceneparams)
  ##############################################
  def onGpuInit(self,ctx):
    ###################################
    layer = self.scene.createLayer("layer1")
    ###################################
    model = Model("srcdata://tests/pbr1/pbr1")
    self.sgnode = model.createNode("node1",layer)
    ###################################
    self.camera = CameraData()
    self.camera.perspective(0.1, 100.0, 45.0)
    self.cameralut = CameraDataLut()
    self.cameralut.addCamera("spawncam",self.camera)
    ###################################
    ctx.FBI().autoclear = True
    ctx.FBI().clearcolor = vec4(.15,.15,.2,1)
  ################################################
  def onUpdate(self,updinfo):
    θ    = updinfo.absolutetime * math.pi * 2.0 * 0.1
    ###################################
    distance = 10.0
    eye = vec3(math.sin(θ), 1.0, -math.cos(θ)) * distance
    self.camera.lookAt(eye, # eye
                       vec3(0, 0, 0), # tgt
                       vec3(0, 1, 0)) # up
    ###################################
    self.sgnode.\
         worldMatrix.\
         compose( vec3(0,0,0), # pos
                  quat(), # rot
                  math.sin(updinfo.absolutetime*2)*3) # scale
    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
  ############################################
app = PyOrkApp()
app.qtapp.exec()
