#!/usr/bin/env python3
################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math, sys, os
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
    self.materials = set()
  ##############################################
  def onGpuInit(self,ctx):
    permu = FxCachePermutation()
    permu.rendering_model = RENDERMODEL
    ###################################
    def createPipeline(blending,culltest):
      material = FreestyleMaterial()
      material.gpuInit(ctx,Path("orkshader://manip"))
      material.rasterstate.blending = blending
      material.rasterstate.culltest = culltest
      material.rasterstate.depthtest = tokens.LEQUALS
      permu.technique = material.shader.technique("std_mono_fwd")
      pipeline = material.fxcache.findFxInst(permu) 
      pipeline.bindParam( material.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
      self.materials.add(material) # retain material
      return  pipeline
    ###################################
    pipeline_cube = createPipeline(tokens.OFF,tokens.PASS_FRONT)
    pipeline_frustumF = createPipeline(tokens.ALPHA,tokens.PASS_FRONT)
    pipeline_frustumB = createPipeline(tokens.ALPHA,tokens.PASS_BACK)
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
    frustum = Frustum()
    vmatrix = ctx.lookAt( vec3(0,0,-1),
                          vec3(0,0,0),
                          vec3(0,1,0) )
    pmatrix = ctx.perspective(45,1,0.1,3)
    frustum.set(vmatrix,pmatrix)
    frustum_prim = primitives.FrustumPrimitive()
    alpha = 0.35
    frustum_prim.topColor = vec4(0.2,1.0,0.2,alpha)
    frustum_prim.bottomColor = vec4(0.5,0.5,0.5,alpha)
    frustum_prim.leftColor = vec4(0.2,0.2,1.0,alpha)
    frustum_prim.rightColor = vec4(1.0,0.2,0.2,alpha)
    frustum_prim.nearColor = vec4(0.0,0.0,0.0,alpha)
    frustum_prim.farColor = vec4(1.0,1.0,1.0,alpha)
    frustum_prim.frustum = frustum
    frustum_prim.gpuInit(ctx)
    ###################################
    sceneparams = VarMap()
    sceneparams.preset = RENDERMODEL
    self.scene = self.ezapp.createScene(sceneparams)
    layer1 = self.scene.createLayer("layer1")
    ###################################
    def createNode(name, prim, pipeline, sortkey):
      node = prim.createNode(name,layer1,pipeline)
      node.sortkey = sortkey
      return node
    ###################################
    self.cube_node = createNode("cube",cube_prim,pipeline_cube,1)
    self.frustum_nodeB = createNode("frustumB",frustum_prim,pipeline_frustumB,2)
    self.frustum_nodeF = createNode("frustumF",frustum_prim,pipeline_frustumF,3)
    ###################################
    self.camera = CameraData()
    self.camera.perspective(0.1, 100.0, 45.0)
    self.cameralut = CameraDataLut()
    self.cameralut.addCamera("spawncam",self.camera)
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
    def nodesetxf(node,trans,orient,scale):
      node.worldTransform.translation = trans 
      node.worldTransform.orientation = orient 
      node.worldTransform.scale = scale
    ###################################
    trans = vec3(0,0,0)
    orient = quat()
    scale = 1+(1+math.sin(updinfo.absolutetime*2))
    ###################################
    nodesetxf(self.frustum_nodeF,trans,orient,scale)
    nodesetxf(self.frustum_nodeB,trans,orient,scale)
    ###################################
    self.cube_node.worldTransform.translation = vec3(0,0,math.sin(updinfo.absolutetime))
    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
  ############################################
app = PyOrkApp()
app.ezapp.mainThreadLoop()
