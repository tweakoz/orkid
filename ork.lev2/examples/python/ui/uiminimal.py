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
    print(self.ezapp.topWidget)
    print(self.ezapp.topWidget.name)

    lg_group = self.ezapp.topLayoutGroup

    print(lg_group.name)
    print(lg_group)
    print(lg_group.layout)
    print(lg_group.layout.top)
    print(lg_group.layout.bottom)
    print(lg_group.layout.left)
    print(lg_group.layout.right)
    print(lg_group.layout.centerH)
    print(lg_group.layout.centerV)
    print(self.ezapp.uicontext)

    griditems = lg_group.makeGrid( width = 2,
                                   height = 2,
                                   margin = 1,
                                   uiclass = ui.UiBox,
                                   args = ["box",vec4(1,0,1,1)] )

    #self.ezapp.topWidget = lg_group


  ##############################################

  def onGpuInit(self,ctx):
    pass

  ################################################

  def onUpdate(self,updinfo):
    pass

  ##############################################

  def onUiEvent(self,uievent):
    pass
    #handled = self.uicam.uiEventHandler(uievent)
    #if handled:
    #  self.camera.copyFrom( self.uicam.cameradata )

###############################################################################

MinimalUiApp().ezapp.mainThreadLoop()
