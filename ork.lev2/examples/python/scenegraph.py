#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import numpy, time, math
from orkengine.core import *
from orkengine.lev2 import *
import _shaders

class MyApp:

  ###########################

  def __init__(self):
    self._time_base = time.time()
    pass

  ###########################

  def gpuInit(self,ctx):
    FBI = ctx.FBI()
    GBI = ctx.GBI()
    self.nsh = _shaders.Shader(ctx)

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
    #self.drawable.bindMaterial()
    #self.nsh._mtl.bindTechnique(self.nsh._tek_frustum)

    self.SG = scenegraph.Scene()
    self.layer = self.SG.createLayer("layer1")
    self.primnode = self.prim.createNode("node1",self.layer,self.nsh._mtl)
    self.nsh._mtl.bindTechnique(self.nsh._tek_frustum)

  ###########################

  def draw(self,drawev):
    ctx = drawev.context
    RCFD = RenderContextFrameData(ctx)

    WIDTH = ctx.mainSurfaceWidth()
    HEIGHT = ctx.mainSurfaceHeight()

    Δtime = time.time()-self._time_base
    θ = Δtime*0.1

    x = math.sin(θ)*5
    z = -math.cos(θ)*5

    pmatrix = ctx.perspective(70,WIDTH/HEIGHT,0.01,100.0)
    vmatrix = ctx.lookAt(vec3(x,0.8,z),
                         vec3(0,0,0),
                         vec3(0,1,0))
    rotmatrix = vmatrix.toRotMatrix3()

    mvp_matrix = vmatrix*pmatrix

    FBI = ctx.FBI()
    GBI = ctx.GBI()

    FBI.autoclear = True
    FBI.clearcolor = vec4(.15,.15,.2,1)
    ctx.beginFrame()

    self.SG.enqueueToRenderer() # technically enqueueToRenderer should work from any (single) python thread

    self.SG.renderOnContext(ctx) # this must be on rendering thread

    ctx.endFrame()

##############################################

myapp = MyApp()

def onGpuInit(ctx):
  myapp.gpuInit(ctx)

def onDraw(drawev):
  myapp.draw(drawev)
  pass

qtapp = OrkEzQtApp.create( onGpuInit, onDraw)
qtapp.setRefreshPolicy(RefreshFixedFPS, 60)

qtapp.exec()
