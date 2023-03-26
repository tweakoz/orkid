#!/usr/bin/env python3
################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import math, sys, os
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string)# add parent dir to path
from common.cameras import setupUiCamera
from common.shaders import createPipeline
from common.primitives import createCubePrim, createFrustumPrim, createGridData
from common.scenegraph import createSceneGraph

#########################################

tokens = CrcStringProxy()
constants = mathconstants()
RENDERMODEL = "ForwardPBR"

################################################################################

class UiCamera(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()

    setupUiCamera(app=self,
                  fov_deg=45,
                  constrainZ=True, 
                  eye = vec3(10,5,10),
                  tgt = vec3(0,0,0) )

  ##############################################

  def onGpuInit(self,ctx):

    createSceneGraph(app=self,rendermodel=RENDERMODEL)

    ###################################

    def createNode(name=None, prim=None, pipeline=None, sortkey=None):
      node = prim.createNode(name,self.layer1,pipeline)
      node.sortkey = sortkey
      return node

    ###################################
    # create cube
    ###################################

    cube_prim = createCubePrim(ctx=ctx,size=2.0)
    pipeline_cube = createPipeline( app = self, ctx = ctx, rendermodel=RENDERMODEL )
    self.cube_node = createNode(name="cube",prim=cube_prim,pipeline=pipeline_cube,sortkey=1)

    ###################################
    # create double sided transparent frustum
    ###################################

    vmatrix = ctx.lookAt( vec3(0,0,-1),
                          vec3(0,0,0),
                          vec3(0,1,0) )

    pmatrix = ctx.perspective(45,1,0.1,3)

    frustum_prim = createFrustumPrim(ctx=ctx,vmatrix=vmatrix,pmatrix=pmatrix,alpha=0.35)

    pipeline_frustumF = createPipeline( app = self,
                                        ctx = ctx,
                                        rendermodel=RENDERMODEL,
                                        blending = tokens.ALPHA )

    pipeline_frustumB = createPipeline( app = self,
                                        ctx = ctx,
                                        rendermodel=RENDERMODEL,
                                        blending = tokens.ALPHA,
                                        culltest = tokens.PASS_BACK)

    self.frustum_nodeB = createNode(name="frustumB",prim=frustum_prim,pipeline=pipeline_frustumB,sortkey=2)
    self.frustum_nodeF = createNode(name="frustumF",prim=frustum_prim,pipeline=pipeline_frustumF,sortkey=3)

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

  ################################################

  def onUpdate(self,updinfo):

    def nodesetxf(node=None,trans=None,orient=None,scale=None):
      node.worldTransform.translation = trans 
      node.worldTransform.orientation = orient 
      node.worldTransform.scale = scale

    ###################################

    Y = 3

    trans = vec3(0,Y,0)
    orient = quat()
    scale = 1+(1+math.sin(updinfo.absolutetime*2))

    ###################################

    nodesetxf(node=self.frustum_nodeF,trans=trans,orient=orient,scale=scale)
    nodesetxf(node=self.frustum_nodeB,trans=trans,orient=orient,scale=scale)

    ###################################

    self.cube_node.worldTransform.translation = vec3(0,Y,math.sin(updinfo.absolutetime))

    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

###############################################################################

UiCamera().ezapp.mainThreadLoop()

