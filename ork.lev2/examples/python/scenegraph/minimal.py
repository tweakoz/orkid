#!/usr/bin/env python3
################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, sys, os
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createFrustumPrim
#########################################
tokens = CrcStringProxy()
################################################################################
class PyOrkApp(object):
  ################################################
  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################
  def onGpuInit(self,ctx):
    FBI = ctx.FBI() # framebuffer interface

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               techname = "std_mono",
                               rendermodel = "DeferredPBR" )

    ###################################
    # frustum primitive
    ###################################
    vmatrix = ctx.lookAt( vec3(0,0,-1), # eye
                          vec3(0,0,0),  # tgt
                          vec3(0,1,0) ) # up
    pmatrix = ctx.perspective(45,1,0.1,3)
    frustum_prim = createFrustumPrim(ctx=ctx,vmatrix=vmatrix,pmatrix=pmatrix,alpha=0.35)
    ###################################
    # create scenegraph and sg node
    ###################################
    sceneparams = VarMap()
    sceneparams.preset = "DeferredPBR"
    self.scene = self.ezapp.createScene(sceneparams)
    layer = self.scene.createLayer("layer1")
    self.primnode = frustum_prim.createNode("node1",layer,pipeline)
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
