#!/usr/bin/env ork.python

################################################################################
# Example demonstrating fixed_width and fixed_height widget properties
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import signal
from orkengine.core import vec3, vec4
from orkengine import lev2

################################################################################

class FixedSizeExample(object):

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorGuide = vec4(0.1, 0.1, 0.15, 1)

    ############################################
    # Create main layout
    ############################################

    self.griditems = lg_group.makeGrid(
      width=2,
      height=2,
      margin = 4,
      uiclass = lev2.ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )

    lg_group.margin = 4
    lg_group.clearColorGuide = vec4(0.8,0.6,0.2,1)
    ############################################
    # Top-left: HorizontalPack with fixed_width widgets
    ############################################

    hpack = lg_group.makeChild(
        uiclass=lev2.ui.HorizontalPack,
        args=["hpack"]
    )
    self.hpack = hpack.widget
    lg_group.replaceChild( self.griditems[0].layout, hpack )
    self.hpack.margin = 4
    self.hpack.item_width = 100
    self.hpack.fill = True

    # Regular width widget (will be resized to item_width)
    self.hbox1 = self.hpack.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["1",vec4(0.5,0.3,0.3,1),"ItemWidth"]
    )

    # Fixed width widget (keeps its width)
    self.hbox2 = self.hpack.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.3,0.5,0.3,1),"OvFixed"]
    )
    self.hbox2.fixed_width = 200

    # Another regular width widget
    self.hbox3 = self.hpack.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.3,0.3,0.5,1),"ItemWidth"]
    )
    self.hbox4 = self.hpack.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.3,0.3,0.5,1),"Fill"]
    )

    ############################################
    # Top-right: VerticalPack with fixed_height widgets
    ############################################

    vpack = lg_group.makeChild(
        uiclass=lev2.ui.VerticalPack,
        args=["vpack"]
    )
    self.vpack = vpack.widget
    lg_group.replaceChild( self.griditems[1].layout, vpack )
    self.vpack.margin = 4
    self.vpack.item_height = 40

    # Regular height widget (will be resized to item_height)
    self.vbox1 = self.vpack.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.5,0.3,0.3,1),"OvFixed"]
    )
    self.vbox1.fixed_height = 100

    # Fixed height widget (keeps its height)
    self.vbox2 = self.vpack.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.3,0.5,0.3,1),"ItemHeight"]
    )

    # Another regular height widget
    self.vbox3 = self.vpack.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.3,0.3,0.5,1),"ItemHeight"]
    )

    ############################################
    # Bottom-left: HorizontalPack with uniform distribution
    ############################################

    hpack2 = lg_group.makeChild(
        uiclass=lev2.ui.HorizontalPack,
        args=["hpack2"]
    )
    self.hpack2 = hpack2.widget
    lg_group.replaceChild( self.griditems[2].layout, hpack2 )
    self.hpack2.margin = 4
    self.hpack2.uniform = True
    self.hpack2.fill = True

    # Uniform distribution with one fixed width
    self.hubox1 = self.hpack2.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.5,0.5,0.3,1),"OvFixed"]
    )
    self.hubox1.fixed_width = 150

    self.hubox2 = self.hpack2.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.3,0.5,0.5,1),"Uniform"]
    )

    self.hubox3 = self.hpack2.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.5,0.3,0.5,1),"Uniform"]
    )

    self.hubox4 = self.hpack2.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.5,0.3,0.5,1),"Uniform"]
    )
    self.hubox5 = self.hpack2.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.5,0.3,0.5,1),"Uniform"]
    )

    self.hubox6 = self.hpack2.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.6,0.2,0.5,1),"Uniform"]
    )

    ############################################
    # Bottom-right: VerticalPack with fill
    ############################################

    vpack2 = lg_group.makeChild(
        uiclass=lev2.ui.VerticalPack,
        args=["vpack2"]
    )
    self.vpack2 = vpack2.widget
    lg_group.replaceChild( self.griditems[3].layout, vpack2 )
    self.vpack2.margin = 4
    self.vpack2.item_height = 40
    self.vpack2.fill = True

    self.vfbox1 = self.vpack2.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.5,0.4,0.3,1),"ItemHeight"]
    )

    self.vfbox2 = self.vpack2.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.4,0.5,0.3,1),"Fixed"]
    )
    self.vfbox2.fixed_height = 60

    # This one will fill remaining space
    self.vfbox3 = self.vpack2.makeChild(
        uiclass=lev2.ui.LabelBox,
        args=["",vec4(0.3,0.4,0.5,1),"Fill"]
    )

    ############################################
    # Signal handling
    ############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onUpdate(self,updinfo):
    pass

  def onUiEvent(self,uievent):
    return lev2.ui.HandlerResult()

###############################################################################

FixedSizeExample().ezapp.mainThreadLoop()
