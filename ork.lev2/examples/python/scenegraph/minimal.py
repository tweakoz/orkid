#!/usr/bin/env python3
################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, sys, os
#########################################
# Our intention is not to 'install' anything just for running the examples
#  so we will just hack the sys,path
#########################################
from pathlib import Path
this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
pyex_dir = (this_dir/"..").resolve()
sys.path.append(str(pyex_dir))
from common.shaders import Shader
#########################################
from orkengine.core import *
from orkengine.lev2 import *
tokens = CrcStringProxy()
################################################################################
class PyOrkApp(object):
  ################################################
  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################
  def onGpuInit(self,ctx):
    FBI = ctx.FBI() # framebuffer interface
    ###################################
    # material setup
    ###################################
    self.material = FreestyleMaterial()
    self.material.gpuInit(ctx,Path("orkshader://manip"))
    tek = self.material.shader.technique("std_mono")
    self.material.rasterstate.culltest = tokens.PASS_FRONT
    self.material.rasterstate.depthtest = tokens.LEQUALS
    assert(tek)
    ###################################
    # create an fxinst (a graphics pipeline)
    ###################################
    permu = FxCachePermutation()
    permu.rendering_model = "DeferredPBR"
    permu.technique = tek
    fxinst = self.material.fxcache.findFxInst(permu)
    ###################################
    # explicit shader parameters
    ###################################
    fxinst.bindParam( self.material.param("mvp"),
                      tokens.RCFD_Camera_MVP_Mono)
    ###################################
    # frustum primitive
    ###################################
    frustum = Frustum()
    vmatrix = ctx.lookAt( vec3(0,0,-1),
                          vec3(0,0,0),
                          vec3(0,1,0) )
    pmatrix = ctx.perspective(45,1,0.1,3)
    frustum.set(vmatrix,pmatrix)
    frustum_prim = primitives.FrustumPrimitive()
    frustum_prim.topColor = vec4(1.0,0.2,0.2,1)
    frustum_prim.bottomColor = vec4(0.5,0.5,0.5,1)
    frustum_prim.leftColor = vec4(0.2,1.0,0.2,1)
    frustum_prim.rightColor = vec4(0.2,0.2,1.0,1)
    frustum_prim.nearColor = vec4(0.0,0.0,0.0,1)
    frustum_prim.farColor = vec4(1.0,1.0,1.0,1)
    frustum_prim.frustum = frustum
    frustum_prim.gpuInit(ctx)
    ###################################
    # create scenegraph and sg node
    ###################################
    sceneparams = VarMap()
    sceneparams.preset = "DeferredPBR"
    self.scene = self.ezapp.createScene(sceneparams)
    layer = self.scene.createLayer("layer1")
    self.primnode = frustum_prim.createNode("node1",layer,fxinst)
    ###################################
    # create camera
    ###################################
    self.camera = CameraData()
    self.camera.perspective(0.1, 100.0, 45.0)
    self.cameralut = CameraDataLut()
    self.cameralut.addCamera("spawncam",self.camera)
  ################################################
  # update:
  # technically this runs from the orkid update thread
  #  but since mainThreadLoop() is called,
  #  the main thread will surrender the GIL completely
  #  until ezapp.mainThreadLoop() returns.
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
    xf = self.primnode.worldTransform
    xf.translation = vec3(0,0,0) 
    xf.orientation = quat() 
    xf.scale = 1+(1+math.sin(updinfo.absolutetime*2))
    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
  ############################################
app = PyOrkApp()
app.ezapp.mainThreadLoop()
