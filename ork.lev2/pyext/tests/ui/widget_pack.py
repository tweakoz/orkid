#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, signal
from obt import path
from orkengine.core import vec2, vec3, vec4, mtx4, quat, VarMap, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class PackWidgets(object):

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

    self.griditems = lg_group.makeGrid(
      width=2,
      height=2,
      margin = 4,
      uiclass = lev2.ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )

    self.lg_group = lg_group
    lg_group.margin = 4

    pk1 = lg_group.makeChild( uiclass=lev2.ui.VerticalPack, args=["vpack1"])
    self.lg_group.replaceChild( self.griditems[0].layout, pk1 )
    self.vpack1 = pk1.widget
    self.vpack1.margin = 3
    self.vpack1.item_height = 28

    box1 = self.vpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box1  ","text",vec3(0.5,0.3,0.3)] )
    box2 = self.vpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box2  ","text",vec3(0.3,0.5,0.3)] )
    box3 = self.vpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box3  ","text",vec3(0.3,0.3,0.5)] )

    self.hpack1 = self.vpack1.makeChild( uiclass=lev2.ui.HorizontalPack, args=["hpack1"])
    self.hpack1.margin = 3
    self.hpack1.item_width = 192
    self.hpack1.fill = True
    
    box4 = self.hpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box1  ","text",vec3(0.5,0.5,0.5)] )
    box5 = self.hpack1.makeChild( uiclass=lev2.ui.LineEdit, args=["box2  ","text",vec3(0.5,0.0,0.5)] )

    self.hpack2 = self.vpack1.makeChild( uiclass=lev2.ui.HorizontalPack, args=["hpack2"])
    self.hpack2.margin = 3
    self.hpack2.item_width = 72
    self.hpack2.fill = False
    chk_col = vec3(0.25)
    self.chk1 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk1  ",chk_col] )
    self.chk2 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk2  ",chk_col] )
    self.chk3 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk3  ",chk_col] )
    self.chk4 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk4  ",chk_col] )
    self.chk5 = self.hpack2.makeChild( uiclass=lev2.ui.Checkbox, args=["chk5  ",chk_col] )
    self.chk5.onToggled = lambda x: print("chk5 toggled to ",x.toggled)

    sli_col = vec3(0.3,0.3,0.5)
    self.sli1 = self.vpack1.makeChild( uiclass=lev2.ui.IntSlider, args=["sli1  ",sli_col,0,100,50] )

    ############################################
    
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

PackWidgets().ezapp.mainThreadLoop()
