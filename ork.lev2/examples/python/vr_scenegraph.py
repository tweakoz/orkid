#!/usr/bin/env python3
################################################################################
# lev2 sample which renders a scenegraph to an OpenVR connected HMD
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
    self.sceneparams.preset = "PBRVR"
    self.qtapp = OrkEzQtApp.create(self)
    self.qtapp.setRefreshPolicy(RefreshFastest, 0)
  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################
  def onGpuInit(self,ctx):
    ###################################
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
    volumetexture = Texture.load("lev2://textures/voltex_pn3")
    ###################################
    material = FreestyleMaterial(ctx,Path("orkshader://noise"))
    material_inst = material.createInstance()
    material_inst.monoTek = material.shader.technique("std_mono")
    material_inst.stereoTek = material.shader.technique("std_stereo")
    param_volumetex = material.shader.param("VolumeMap")
    param_v4parref = material.shader.param("testvec4")
    material.setInstanceMvpParams(material_inst,"mvp","mvpL","mvpR")
    v4parref = vec4()
    material_inst.param[param_v4parref] = v4parref
    material_inst.param[param_volumetex] = volumetexture
    primnode = prim.createNode("node1",layer,material_inst)
    ###################################
    camera = CameraData()
    camera.perspective(0.1, 100.0, 45.0)
    cameralut = CameraDataLut()
    cameralut.addCamera("spawncam",camera)
    ###################################
    self.primnode1 = primnode
    self.camera1 = camera
    self.cameralut1 = cameralut
    self.v4parref = v4parref
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
    θ = updinfo.absolutetime * math.pi * 2.0
    self.v4parref.z = θ*0.01 # animate noise
    ###################################
    cam = self.camera1
    camlut = self.cameralut1
    primnode = self.primnode1
    ###################################
    primnode.worldMatrix.compose( vec3(0,0.25,-2.5), # pos
                                  quat(vec3(0,1,0),θ*0.01), # rot
                                  0.75) # scale
    ###################################
    self.scene.updateScene(camlut) # update and enqueue all scenenodes
  ################################################
  # there is no onDraw() method here
  #  so c++ will implement a (potentially faster) default
################################################
app = PyOrkApp()
app.qtapp.exec()
