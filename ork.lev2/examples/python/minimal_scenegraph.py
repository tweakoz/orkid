#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to a window, using the scenegraph
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
token = CrcStringProxy()

################################################
# globals
################################################

_time_base = time.time()
scene = None
camera = None
cameralut = None

################################################
# gpu data init:
#  called on main thread when graphics context is
#   made available
##############################################

def onGpuInit(ctx):
    global scene
    global camera
    global cameralut
    ###################################
    mtl = FreestyleMaterial()
    mtl.gpuInit(ctx,Path("orkshader://solid"))
    mtl_inst = MaterialInstance(mtl)
    mtl_inst.monoTek = mtl.shader.technique("vtxcolor")
    mtl.setInstanceMvpParams(mtl_inst,"MatMVP","","")
    ###################################
    frustum_pmtx = ctx.perspective(45,1,0.1,3)
    frustum_vmtx = ctx.lookAt(vec3(0,0,-1),vec3(0,0,0),vec3(0,1,0))
    frustum = Frustum()
    frustum.set(frustum_vmtx,frustum_pmtx)
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
    scene = scenegraph.Scene()
    layer = scene.createLayer("layer1")
    primnode = prim.createNode("node1",layer,mtl_inst)
    primnode.worldMatrix = mtx4()
    ###################################
    camera = CameraData()
    cameralut = CameraDataLut()
    cameralut.addCamera("spawncam",camera)

################################################
# update:
# technically this runs from the orkid update thread
#  but since the python GIL is in place,
#  it will be serialized with the main thread python code.
#  This is still useful for doing background computation.
#   (eg. the scene can be updated from python, whilst
#        concurrently c++ is rendering..)
################################################

def onUpdate():
    Δtime = time.time()-_time_base
    θ    = Δtime * math.pi * 2.0 * 0.1
    distance = 10.0
    eye = vec3(math.sin(θ), 1.0, -math.cos(θ)) * distance
    tgt = vec3(0, 0, 0)
    up = vec3(0, 1, 0)
    camera.perspective(0.1, 100.0, 45.0)
    camera.lookAt(eye, tgt, up)
    ###################################
    scene.updateScene(cameralut)
    ###################################
    time.sleep(1.0/120.0)

################################################
# render scene (from main thread)
################################################

def onDraw(drawev):
    drawev.context.FBI().autoclear = True
    drawev.context.FBI().clearcolor = vec4(.15,.15,.2,1)
    drawev.context.beginFrame()
    scene.renderOnContext(drawev.context) # this must be on rendering thread
    drawev.context.endFrame()

################################################
# event loop
##############################################

qtapp = OrkEzQtApp.create( onGpuInit, onUpdate, onDraw )
qtapp.setRefreshPolicy(RefreshFastest, 0)
qtapp.exec()
