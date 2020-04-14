#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to a window, using the scenegraph
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import time, math
from ork.deco import Deco
from ork.log import log
from orkengine.core import *
from orkengine.lev2 import *
import _shaders
################################################
# globals
################################################
deco = Deco()
time_base = time.time()
scene = None
camera = None
cameralut = None
primnode = None
################################################
# gpu data init:
#  called on main thread when graphics context is
#   made available
##############################################
def onGpuInit(ctx):
    global scene
    global camera
    global cameralut
    global primnode
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
    scene = scenegraph.Scene()
    layer = scene.createLayer("layer1")
    ###################################
    material = FreestyleMaterial(ctx,Path("orkshader://manip"))
    material_inst = material.createInstance()
    material_inst.monoTek = material.shader.technique("std_mono")
    material.setInstanceMvpParams(material_inst,"mvp","","")
    primnode = prim.createNode("node1",layer,material_inst)
    ###################################
    camera = CameraData()
    camera.perspective(0.1, 100.0, 45.0)
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
    Δtime = time.time()-time_base
    θ    = Δtime * math.pi * 2.0 * 0.1
    ###################################
    distance = 10.0
    eye = vec3(math.sin(θ), 1.0, -math.cos(θ)) * distance
    camera.lookAt(eye, # eye
                  vec3(0, 0, 0), # tgt
                  vec3(0, 1, 0)) # up
    ###################################
    primnode.worldMatrix.compose( vec3(0,0,0), # pos
                                  quat(), # rot
                                  math.sin(Δtime*2)*3) # scale
    ###################################
    scene.updateScene(cameralut) # update and enqueue all scenenodes
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
