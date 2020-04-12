#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to an offscreen buffer
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import time, math
from orkengine.core import *
from orkengine.lev2 import *
import _shaders
from ork.deco import Deco
from ork.log import log
deco = Deco()
crcstr = CrcStringProxy()

################################################
_time_base = time.time()
global SG
################################################

def onGpuInit(ctx):
    global SG
    nsh = _shaders.Shader(ctx)
    volumetexture = Texture.load("lev2://textures/voltex_pn3")
    ###################################
    fpmtx = ctx.perspective(45,1,0.1,3)
    fvmtx = ctx.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0))
    frust = Frustum()
    frust.set(fvmtx,fpmtx)
    ###################################
    prim = primitives.FrustumPrimitive()
    prim.topColor = vec4(0.5,1.0,0.5,1)
    prim.bottomColor = vec4(0.5,0.0,0.5,1)
    prim.leftColor = vec4(0.0,0.5,0.5,1)
    prim.rightColor = vec4(1.0,0.5,0.5,1)
    prim.frontColor = vec4(0.5,0.5,1.0,1)
    prim.backColor = vec4(0.5,0.5,0.0,1)
    prim.frustum = frust
    prim.gpuInit(ctx)
    ###################################
    #nsh._mtl.bindTechnique(nsh._tek_frustum)
    mtl_inst = MaterialInstance(nsh._mtl)
    mtl_inst.monoTek = nsh._tek_frustum
    mtl_inst.param[nsh._par_mvp] = crcstr.RCFD
    mtl_inst.param[nsh._par_mnormal] = mtx3()
    log(deco.white("monotek: "+str(mtl_inst.monoTek)))
    log(deco.yellow("param: "+str(mtl_inst.param)))
    log(deco.orange("param.MatMVP: "+str(mtl_inst.param[nsh._par_mvp])))
    log(deco.orange("param.MatNormal: "+str(mtl_inst.param[nsh._par_mnormal])))
    ###################################
    SG = scenegraph.Scene()
    layer = SG.createLayer("layer1")
    primnode = prim.createNode("node1",layer,mtl_inst)

################################################
# update
# technically this runs from the orkid update thread
#  but since the GIL is still present
#  it will be serialized with the main thread
#  still useful for doing background computation
#  while c++ is rendering
################################################

updcount = 0
def onUpdate():
  global updcount
  updcount += 1
  if(updcount%100==0):
    print(updcount)

################################################

def onDraw(drawev):
    global SG
    ctx = drawev.context
    FBI = ctx.FBI()
    GBI = ctx.GBI()
    RCFD = RenderContextFrameData(ctx)
    WIDTH = ctx.mainSurfaceWidth()
    HEIGHT = ctx.mainSurfaceHeight()
    ###################################
    Δtime = time.time()-_time_base
    θ = Δtime*0.1
    x = math.sin(θ)*5
    z = -math.cos(θ)*5
    ###################################
    pmatrix = ctx.perspective(70,WIDTH/HEIGHT,0.01,100.0)
    vmatrix = ctx.lookAt(vec3(x,0.8,z),
                         vec3(0,0,0),
                         vec3(0,1,0))
    rotmatrix = vmatrix.toRotMatrix3()
    mvp_matrix = vmatrix*pmatrix
    ###################################
    # update scene
    ###################################
    SG.enqueueToRenderer() # technically enqueueToRenderer should work from any (single) python thread
    ###################################
    # render scene
    ###################################
    FBI.autoclear = True
    FBI.clearcolor = vec4(.15,.15,.2,1)
    ctx.beginFrame()
    SG.renderOnContext(ctx) # this must be on rendering thread
    ctx.endFrame()

##############################################
qtapp = OrkEzQtApp.create( onGpuInit, onUpdate, onDraw )
qtapp.setRefreshPolicy(RefreshFastest, 0)
qtapp.exec()
