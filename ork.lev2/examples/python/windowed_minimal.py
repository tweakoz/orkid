#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import time, math, os, sys
from orkengine.core import *
from orkengine.lev2 import *
from pathlib import Path
this_dir = Path(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(str(this_dir))
from common.shaders import Shader

class MyApp(object):
  ###########################
  def __init__(self):
    super().__init__()
    self.qtapp = OrkEzApp.create(self)
    self.qtapp.setRefreshPolicy(RefreshFixedFPS, 60)
    self._time_base = time.time()
  ###########################
  def onGpuInit(self,ctx):
    FBI = ctx.FBI()
    GBI = ctx.GBI()
    self.nsh = Shader(ctx)
    self.volumetexture = Texture.load("lev2://textures/voltex_pn3")
    ###################################
    fpmtx = ctx.perspective(45,1,0.1,3)
    fvmtx = ctx.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0))
    frust = Frustum()
    frust.set(fvmtx,fpmtx)
    self.prim = primitives.FrustumPrimitive()
    self.prim.topColor = vec4(0.5,1.0,0.5,1)
    self.prim.bottomColor = vec4(0.5,0.0,0.5,1)
    self.prim.leftColor = vec4(0.0,0.5,0.5,1)
    self.prim.rightColor = vec4(1.0,0.5,0.5,1)
    self.prim.frontColor = vec4(0.5,0.5,1.0,1)
    self.prim.backColor = vec4(0.5,0.5,0.0,1)
    self.prim.frustum = frust
    self.prim.gpuInit(ctx)
  ###########################
  def onDraw(self,drawevent):
    ctx = drawevent.context
    WIDTH = ctx.mainSurfaceWidth()
    HEIGHT = ctx.mainSurfaceHeight()
  ###########################
    Δtime = time.time()-self._time_base
    θ = Δtime*0.1
    x = math.sin(θ)*5
    z = -math.cos(θ)*5
  ###########################
    pmatrix = ctx.perspective(70,WIDTH/HEIGHT,0.01,100.0)
    vmatrix = ctx.lookAt(vec3(x,0.8,z),
                         vec3(0,0,0),
                         vec3(0,1,0))
    rotmatrix = vmatrix.toRotMatrix3()
    mvp_matrix = vmatrix*pmatrix
  ###########################
    ctx.FBI().autoclear = True
    ctx.FBI().clearcolor = vec4(.15,.15,.2,1)
    RCFD = RenderContextFrameData(ctx)
    ctx.beginFrame()
    self.nsh.beginNoise(RCFD,Δtime)
    self.nsh.bindMvpMatrix(mvp_matrix)
    self.nsh.bindRotMatrix(rotmatrix)
    self.nsh.bindVolumeTex(self.volumetexture)
    self.prim.renderEML(ctx)
    self.nsh.end(RCFD)
    ctx.endFrame()
##############################################
myapp = MyApp()
myapp.qtapp.mainThreadLoop()
