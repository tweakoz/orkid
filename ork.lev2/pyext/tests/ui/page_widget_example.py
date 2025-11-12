#!/usr/bin/env ork.python

################################################################################
# Example demonstrating TabWidget in "page mode" (tabs hidden)
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import signal
from orkengine.core import vec3, vec4
from orkengine import lev2

################################################################################

class PageWidgetExample(object):

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    lg_group.clearColorGuide = vec4(0.1, 0.1, 0.15, 1)

    ############################################
    # Create main layout
    ############################################

    self.griditems = lg_group.makeGrid(
      width=1,
      height=1,
      margin = 4,
      uiclass = lev2.ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )

    ############################################
    # Create pager (tabs widget in page mode)
    ############################################

    tabs = lg_group.makeChild(
        uiclass=lev2.ui.TabsWidget,
        args=["tabs",vec3(1,0,0)]
    )
    self.tabsw = tabs.widget
    lg_group.replaceChild( self.griditems[0].layout, tabs )
    
    ############################################
    # Top section: horizontal pack with buttons
    ############################################

    self.box1 = self.tabsw.makeChild(
        uiclass=lev2.ui.TextBox,
        args=["box1",vec4(0.5,0.3,0.3,1),"B1"]
    )

    self.box2 = self.tabsw.makeChild(
        uiclass=lev2.ui.TextBox,
        args=["box2",vec4(0.3,0.5,0.3,1),"B2"]
    )

    self.box3 = self.tabsw.makeChild(
        uiclass=lev2.ui.TextBox,
        args=["box3",vec4(0.3,0.3,0.5,1),"B3"]
    )

    ############################################
    # Set initial page
    ############################################
    self.tabsw.draw_tabs = False
    self.tabsw.setActiveTab(0)

    ############################################
    # Signal handling
    ############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onUpdate(self,updinfo):
    abstime = updinfo.absolutetime
    iabstime = int(abstime)%3
    self.tabsw.setActiveTab(iabstime)


  def onUiEvent(self,uievent):
    return lev2.ui.HandlerResult()

###############################################################################

PageWidgetExample().ezapp.mainThreadLoop()
