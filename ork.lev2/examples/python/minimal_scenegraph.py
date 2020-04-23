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
  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################
  def onGpuInit(self,ctx):
    frustum = Frustum()
    frustum.set(ctx.lookAt( vec3(0,0,-1),
                            vec3(0,0,0),
                            vec3(0,1,0)),
                ctx.perspective(45,1,0.1,3))
    ###################################
    prim = primitives.FrustumPrimitive()
    prim.topColor = vec4(0.5,1.0,0.5,1)
    prim.bottomColor = vec4(0.5,0.0,0.5,1)
    prim.leftColor = vec4(0.0,0.5,0.5,1)
    prim.rightColor = vec4(1.0,0.5,0.5,1)
    prim.frontColor = vec4(0.5,0.5,1.0,1)
    prim.backColor = vec4(0.5,0.5,0.0,1)
    prim.frustum = frustum
    prim.gpuInit(ctx)
    ###################################
    layer = self.scene.createLayer("layer1")
    ###################################
    material = FreestyleMaterial(ctx,Path("orkshader://manip"))
    material_inst = material.createInstance()
    material_inst.monoTek = material.shader.technique("std_mono")
    material.setInstanceMvpParams(material_inst,"mvp","","")
    self.primnode = prim.createNode("node1",layer,material_inst)
    ###################################
    self.camera = CameraData()
    self.camera.perspective(0.1, 100.0, 45.0)
    self.cameralut = CameraDataLut()
    self.cameralut.addCamera("spawncam",self.camera)
    ###################################
    ctx.FBI().autoclear = True
    ctx.FBI().clearcolor = vec4(.15,.15,.2,1)
  ################################################
  # update:
  # technically this runs from the orkid update thread
  #  but since createWithScene() was called,
  #  the main thread will surrender the GIL completely
  #  until qtapp.exec() returns.
  #  This is useful for doing background computation.
  #   (eg. the scene is updated from python, whilst
  #        concurrently c++ is rendering..)
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
    self.primnode.\
         worldMatrix.\
         compose( vec3(0,0,0), # pos
                  quat(), # rot
                  math.sin(updinfo.absolutetime*2)*3) # scale
    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
  ################################################
  def onDraw(self,drawevent):
    ctx = drawevent.context
    ctx.beginFrame()
    self.scene.renderOnContext(ctx)
    ctx.endFrame()
################################################
app = PyOrkApp()
app.qtapp.exec()
