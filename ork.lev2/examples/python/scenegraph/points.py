#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, sys, os
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createPointsPrimC4, createGridData
from common.scenegraph import createSceneGraph

################################################################################

class MinimalSceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, eye = vec3(10,10,10), constrainZ=True, up=vec3(0,1,0))

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
    # create points primitive / sgnode
    ###################################

    points_prim = createPointsPrimC4(ctx=ctx)

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               techname = "std_mono",
                               rendermodel = "DeferredPBR" )

    self.primnode = points_prim.createNode("node1",self.layer1,pipeline)

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

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

    x = math.sin(θ)*10
    z = -math.cos(θ)*10
    y = 5+math.sin(θ*1.7)*2

    m_world_to_view = mtx4.lookAt(vec3(x, y, z), # eye
                                  vec3(0, 0, 0),  # tgt
                                  vec3(0, 1, 0))  # up

    m_view_to_world =  m_world_to_view.inverse()

    self.primnode.worldTransform.directMatrix = m_view_to_world

    ###################################
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

###############################################################################

MinimalSceneGraphApp().ezapp.mainThreadLoop()
