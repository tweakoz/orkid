#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, sys, os, random, numpy
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from common.cameras import *
from common.shaders import *
from common.primitives import createPointsPrimV12C4, createGridData
from common.scenegraph import createSceneGraph

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
    # create points primitive / sgnode
    ###################################

    NUMPOINTS = 262144

    points_prim = createPointsPrimV12C4(ctx=ctx,numpoints=NUMPOINTS)

    data_ptr = numpy.array(points_prim.lock(ctx), copy=False)
    for i in range(NUMPOINTS):
      VTX = data_ptr[i]
      VTX[0] = random.uniform(-2,2)  # float x
      VTX[1] = random.uniform(0,4)  # float y 
      VTX[2] = random.uniform(-2,2)  # float z 
      VTX[3] = 0xffffffff # uint32_t color

    print(data_ptr)
    points_prim.unlock(ctx)

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               techname = "std_mono_fwd",
                               rendermodel = "ForwardPBR" )

    self.primnode = points_prim.createNode("node1",self.layer1,pipeline)

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )

###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
