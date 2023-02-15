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
RENDERMODEL = "ForwardPBR"
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
    self.material_frustum = FreestyleMaterial()
    self.material_frustum.gpuInit(ctx,Path("orkshader://manip"))
    self.material_frustum.rasterstate.culltest = tokens.PASS_FRONT
    self.material_frustum.rasterstate.depthtest = tokens.LEQUALS
    self.material_frustum.rasterstate.blending = tokens.ALPHA
    ###################################
    self.material_cube = FreestyleMaterial()
    self.material_cube.gpuInit(ctx,Path("orkshader://manip"))
    self.material_cube.rasterstate.culltest = tokens.PASS_FRONT
    self.material_cube.rasterstate.depthtest = tokens.LEQUALS
    ###################################
    # create an fxinst (a graphics pipeline)
    ###################################
    permu = FxCachePermutation()
    permu.rendering_model = RENDERMODEL
    permu.technique = self.material_frustum.shader.technique("std_mono_fwd")
    fxinst_frustum = self.material_frustum.fxcache.findFxInst(permu)
    permu.technique = self.material_cube.shader.technique("std_mono_fwd")
    fxinst_cube = self.material_cube.fxcache.findFxInst(permu)
    ###################################
    # explicit shader parameters
    ###################################
    fxinst_frustum.bindParam( self.material_frustum.param("mvp"),
                              tokens.RCFD_Camera_MVP_Mono)
    fxinst_cube.bindParam(    self.material_cube.param("mvp"),
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
    alpha = 0.25
    frustum_prim.topColor = vec4(0.2,1.0,0.2,alpha)
    frustum_prim.bottomColor = vec4(0.5,0.5,0.5,alpha)
    frustum_prim.leftColor = vec4(0.2,0.2,1.0,alpha)
    frustum_prim.rightColor = vec4(1.0,0.2,0.2,alpha)
    frustum_prim.nearColor = vec4(0.0,0.0,0.0,alpha)
    frustum_prim.farColor = vec4(1.0,1.0,1.0,alpha)
    frustum_prim.frustum = frustum
    frustum_prim.gpuInit(ctx)
    ###################################

    cube_prim = primitives.CubePrimitive()
    cube_prim.size = 1
    cube_prim.topColor = vec4(0.5,1.0,0.5,1)
    cube_prim.bottomColor = vec4(0.5,0.0,0.5,1)
    cube_prim.leftColor = vec4(0.0,0.5,0.5,1)
    cube_prim.rightColor = vec4(1.0,0.5,0.5,1)
    cube_prim.frontColor = vec4(0.5,0.5,1.0,1)
    cube_prim.backColor = vec4(0.5,0.5,0.0,1)
    cube_prim.gpuInit(ctx)
    ###################################
    # create scenegraph and sg node
    ###################################
    sceneparams = VarMap()
    sceneparams.preset = RENDERMODEL
    self.scene = self.ezapp.createScene(sceneparams)
    layer1 = self.scene.createLayer("layer1")
    self.frustum_node = frustum_prim.createNode("frustum",layer1,fxinst_frustum)
    self.cube_node = cube_prim.createNode("cube",layer1,fxinst_cube)
    self.cube_node.sortkey = 1
    self.frustum_node.sortkey = 2
    ###################################
    # compositor setup
    ###################################
    cimpl = self.scene.compositorimpl
    cdata = self.scene.compositordata
    onode = self.scene.compositoroutputnode
    rnode = self.scene.compositorrendernode
    rnode.layers = "layer1,layer2"
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
    xf = self.frustum_node.worldTransform
    xf.translation = vec3(0,0,0) 
    xf.orientation = quat() 
    xf.scale = 1+(1+math.sin(updinfo.absolutetime*2))
    ###################################
    self.cube_node.worldTransform.translation = vec3(0,0,math.sin(updinfo.absolutetime))
    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
  ############################################
app = PyOrkApp()
app.ezapp.mainThreadLoop()
