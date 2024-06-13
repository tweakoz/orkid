#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os
from pathlib import Path
from orkengine.core import *
from orkengine.lev2 import *
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from lev2utils.cameras import *
from lev2utils.scenegraph import createSceneGraph

################################################################################

class MinimalUiApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup


    griditems = lg_group.makeGrid( width = 2,
                                   height = 2,
                                   margin = 1,
                                   uiclass = ui.SceneGraphViewport,
                                   args = ["sgvp"] )

    griditems[0].widget.clearColor = vec3(1,0,1)*0.1
    griditems[1].widget.clearColor = vec3(1,0,1)*0.2
    griditems[2].widget.clearColor = vec3(1,0,1)*0.3
    griditems[3].widget.clearColor = vec3(1,0,1)*0.4
    #self.ezapp.topWidget = lg_group


  ##############################################

  def onGpuInit(self,ctx):
    pass

  ################################################

  def onUpdate(self,updinfo):
    pass

  ##############################################

  def onUiEvent(self,uievent):
    #handled = self.uicam.uiEventHandler(uievent)
    #if handled:
    #  self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()

###############################################################################

MinimalUiApp().ezapp.mainThreadLoop()
