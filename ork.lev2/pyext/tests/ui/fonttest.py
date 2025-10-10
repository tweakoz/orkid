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
      height=3,
      margin = MARGIN,
      uiclass = lev2.ui.TextBox,
      args = ["label",vec4(.5,.5,.5,1),"Hello"],
    )
    font_i12 = lev2.FontManager.fontForId("i12")
    font_i13 = lev2.FontManager.fontForId("i13")
    font_i14 = lev2.FontManager.fontForId("i14")
    font_i16 = lev2.FontManager.fontForId("i16")
    font_i24 = lev2.FontManager.fontForId("i24")
    font_i32 = lev2.FontManager.fontForId("i32")
    font_i48 = lev2.FontManager.fontForId("i48")
    font_d24 = lev2.FontManager.fontForId("d24")
    self.griditems[0].widget.font = font_i12
    self.griditems[1].widget.font = font_i13
    self.griditems[2].widget.font = font_i14
    self.griditems[3].widget.font = font_i16
    self.griditems[4].widget.font = font_i24
    self.griditems[5].widget.font = font_i32
    self.griditems[7].widget.font = font_i48
    self.griditems[8].widget.font = font_d24
    
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
