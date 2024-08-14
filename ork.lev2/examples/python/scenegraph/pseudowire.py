#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, random, numpy
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createFrustumPrim, createGridData
from lev2utils.scenegraph import createSceneGraph


################################################################################

class PointsPrimApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, eye = vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))

  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################

    createSceneGraph(app=self,rendermodel="ForwardPBR")

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ##################
    # create frustum primitive
    ##################

    vmatrix = ctx.lookAt( vec3(0,0,-1),
                          vec3(0,0,0),
                          vec3(0,1,0) )

    pmatrix = ctx.perspective(45,1,0.1,3)

    frustum_prim = createFrustumPrim(ctx=ctx,
                                     vmatrix=fmtx4_to_dmtx4(vmatrix),
                                     pmatrix=fmtx4_to_dmtx4(pmatrix))

    ##################
    # create shading pipeline
    ##################

    pipeline = pseudowire_pipeline(app=self,ctx=ctx)

    ##################
    # create sg node
    ##################

    self.primnode = frustum_prim.createNode("node1",self.layer1,pipeline)
    self.primnode.sortkey = 2;


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

    nodesetxf(node=self.primnode,trans=trans,orient=orient,scale=scale)

    r = 0.5+math.sin(updinfo.absolutetime*1.2)*0.05
    g = 0.5+math.sin(updinfo.absolutetime*1.7)*0.05
    b = 0.5+math.sin(updinfo.absolutetime*1.9)*0.05

    self.primnode.modcolor = vec4(r,g,b,1)


    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
  
###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
