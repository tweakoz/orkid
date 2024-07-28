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
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.primitives import createCubePrim, createGridData
from lev2utils.scenegraph import createSceneGraph

################################################################################

tokens = CrcStringProxy()
constants = mathconstants()
RENDERMODEL = "ForwardPBR"

################################################################################

class UiCamera(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    setupUiCamera( app=self,
                   eye = vec3(10,5,10),
                   tgt = vec3(0,0,0) )

  ##############################################

  def onGpuInit(self,ctx):

    createSceneGraph(app=self,rendermodel=RENDERMODEL)

    pipeline_cube = createPipeline(app=self,
                                   ctx=ctx,
                                   rendermodel=RENDERMODEL)
    cube_prim = createCubePrim(ctx=ctx,size=1.0)
    self.cube_node = cube_prim.createNode("cube",self.layer1,pipeline_cube)
    self.cube_node.sortkey = 1

    self.grid_data = createGridData()
    self.grid_data.intensityA = 2.1
    self.grid_data.intensityB = 2.0
    self.grid_data.intensityC = 0
    self.grid_data.intensityD = 0
    self.grid_data.lineWidth = 0.025
    self.grid_data.extent = 100.0
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):

    ###############################################
    # first allow the uicamera to process the event
    ###############################################

    handled = self.uicam.uiEventHandler(uievent)

    ## it will return true of the camera handled the event.

    if handled:

      self.camera.copyFrom( self.uicam.cameradata )

    ###############################################
    # it will return false if the event was ignored.
    ###############################################

    else:

      # print out the event type

      if False:
        if uievent.code == tokens.PUSH.hashed:
          print("PUSH")
        elif uievent.code == tokens.DRAG.hashed:
          print("DRAG")
        elif uievent.code == tokens.MOVE.hashed:
          print("MOVE")
        elif uievent.code == tokens.RELEASE.hashed:
          print("RELEASE")
        elif uievent.code == tokens.KEY_DOWN.hashed:
          print("KEY_DOWN")
        elif uievent.code == tokens.KEY_REPEAT.hashed:
          print("KEY_REPEAT")
        elif uievent.code == tokens.KEY_UP.hashed:
          print("KEY_UP")
        else:

          # unhandled type

          print(uievent.code,uievent.x,uievent.y)
        
    return ui.HandlerResult()

  ################################################

  def onUpdate(self,updinfo):
    cube_pos = vec3(0,0.5,math.sin(updinfo.absolutetime))
    self.cube_node.worldTransform.translation = cube_pos
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes

###############################################################################

UiCamera().ezapp.mainThreadLoop()

