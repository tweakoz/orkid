#!/usr/bin/env python3
################################################################################
# lev2 sample which renders to a window, using the scenegraph
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import time, math
from ork.deco import Deco
from orkengine.core import *
from orkengine.lev2 import *
import _shaders
################################################
# globals
################################################
deco = Deco()
################################################
# gpu data init:
#  called on main thread when graphics context is
#   made available
##############################################
def onGpuInitWithScene(ctx,scene):
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
    layer = scene.createLayer("layer1")
    ###################################
    material = FreestyleMaterial(ctx,Path("orkshader://manip"))
    material_inst = material.createInstance()
    material_inst.monoTek = material.shader.technique("std_mono")
    material.setInstanceMvpParams(material_inst,"mvp","","")
    scene.primnode1 = prim.createNode("node1",layer,material_inst)
    print(scene.primnode1._primitive)
    ###################################
    scene.camera1 = CameraData()
    scene.camera1.perspective(0.1, 100.0, 45.0)
    scene.cameralut1 = CameraDataLut()
    scene.cameralut1.addCamera("spawncam",scene.camera1)
    ctx.FBI().autoclear = True
    ctx.FBI().clearcolor = vec4(.15,.15,.2,1)
    print(deco.orange("YOYOYO"))
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
def onUpdateWithScene(updinfo,scene):
    θ    = updinfo.absolutetime * math.pi * 2.0 * 0.1
    ###################################
    distance = 10.0
    eye = vec3(math.sin(θ), 1.0, -math.cos(θ)) * distance
    scene.camera1.lookAt(eye, # eye
                         vec3(0, 0, 0), # tgt
                         vec3(0, 1, 0)) # up
    ###################################
    scene.primnode1.worldMatrix.compose( vec3(0,0,0), # pos
                                         quat(), # rot
                                         math.sin(updinfo.absolutetime*2)*3) # scale
    ###################################
    scene.updateScene(scene.cameralut1) # update and enqueue all scenenodes
################################################
# event loop
##############################################
qtapp = OrkEzQtApp.createWithScene( onGpuInitWithScene, onUpdateWithScene )
qtapp.setRefreshPolicy(RefreshFastest, 0)
qtapp.exec()
