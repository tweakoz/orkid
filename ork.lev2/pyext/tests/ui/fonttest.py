#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, signal
from obt import path
from orkengine.core import vec2, vec3, vec4, mtx4, quat, VarMap
from orkengine import lev2

################################################################################

class LayoutTest(object):

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, 
                                      left=100, 
                                      top=100, 
                                      width=900, 
                                      height=900)

    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorGuide = vec4(0.8,0.6,0.2,1)

    MARGIN = 4
    
    self.griditems = lg_group.makeGrid(
      width=3,
      height=4,
      margin = MARGIN,
      uiclass = lev2.ui.TextBox,
      args = ["label",vec4(.5,.5,.5,1),"Hello"],
    )
    i_sizes = [12,13,14,16,17,18,20,22,24,32,48]

    for size in i_sizes:
      font = lev2.FontManager.fontForId("i%d"%size)    
      assert(font)
      gitem_index = i_sizes.index(size)
      gitem = self.griditems[gitem_index]
      gitem.widget.font = font
      gitem.widget.setText("Font: i%d\nWhat Up Yo"%size)

        
    self.lg_group = lg_group
    lg_group.margin = MARGIN

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):
    pass

  ################################################

  def onUpdate(self,updinfo):
    pass

  ##############################################

  def onUiEvent(self,uievent):
    return lev2.ui.HandlerResult()

###############################################################################

LayoutTest().ezapp.mainThreadLoop()
