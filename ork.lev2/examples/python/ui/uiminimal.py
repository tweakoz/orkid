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
from common.primitives import createGridData
from common.scenegraph import createSceneGraph

################################################################################

class MinimalUiApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    print(self.ezapp.mainwin)
    print(self.ezapp.mainwin.appwin)
    print(self.ezapp.mainwin.appwin.rootWidget)
    print(self.ezapp.mainwin.appwin.rootWidget.name)
    print(self.ezapp.topLayoutGroup.name)
    print(self.ezapp.topLayoutGroup)
    print(self.ezapp.topLayoutGroup.layout)
    print(self.ezapp.topLayoutGroup.layout.top)
    print(self.ezapp.topLayoutGroup.layout.bottom)
    print(self.ezapp.topLayoutGroup.layout.left)
    print(self.ezapp.topLayoutGroup.layout.right)
    print(self.ezapp.topLayoutGroup.layout.centerH)
    print(self.ezapp.topLayoutGroup.layout.centerV)
    print(self.ezapp.uicontext)
    #assert(False)
    # make a panel/group widget that has 4 children
    # convert ezviewport to that format
    # for the single view case have the ezviewport 
    # only have 1 view dest
    # for the quad case, each child is capable of acting as a 
    # scenegraph "view" destination
    # assign the quad panel top to the rootWidget
    self.materials = set()
    setupUiCamera( app=self, 
                   eye = vec3(10,10,10), 
                   constrainZ=True, 
                   up=vec3(0,1,0))

  ##############################################

  def onGpuInit(self,ctx):
    sceneparams = VarMap()
    sceneparams.preset = "DeferredPBR"
    # assign the "views" as a part of params
    self.scene = self.ezapp.createScene(sceneparams)
    self.layer1 = self.scene.createLayer("layer1")
    self.rendernode = self.scene.compositorrendernode
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

MinimalUiApp().ezapp.mainThreadLoop()
