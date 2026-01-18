#!/usr/bin/env ork.python
################################################################################
# ScrollContainer test
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, os, signal
from orkengine.core import vec2, vec3, vec4, quat, Transform
from orkengine import lev2

################################################################################

class ScrollContainerTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, left=100, top=100, width=400, height=600)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.clearColorGuide = vec4(0.2, 0.2, 0.3, 1)  # Background clear color
    lg.margin = 4

    ############################################
    # Create ScrollContainer as the main widget
    ############################################

    sc_layout = lg.makeChild(uiclass=lev2.ui.ScrollContainer, args=["scroll1"])
    sc_layout.layout.fill(lg.layout)
    self.scroll_container = sc_layout.widget
    self.scroll_container.scroll_mode = lev2.ui.ScrollMode.Y
    self.scroll_container.bg_color = vec4(0.15, 0.15, 0.2, 1.0)
    self.scroll_container.draw_background = True

    ############################################
    # Create VerticalPack as child of ScrollContainer
    ############################################

    self.vpack = lev2.ui.VerticalPack.wfactory(["vpack"])
    self.vpack.margin = 2
    self.vpack.item_height = 28
    self.vpack.fill = False  # Don't fill - let content height determine scrolling
    self.vpack.bg_color = vec4(0.2, 0.2, 0.25, 1.0)
    self.vpack.draw_background = True

    self.scroll_container.setChild(self.vpack)

    ############################################
    # Add simple LineEdit widgets to test scrolling
    ############################################

    colors = [
      vec3(0.5, 0.3, 0.3),
      vec3(0.3, 0.5, 0.3),
      vec3(0.3, 0.3, 0.5),
      vec3(0.5, 0.5, 0.3),
      vec3(0.5, 0.3, 0.5),
      vec3(0.3, 0.5, 0.5),
    ]

    for i in range(100):
      color = colors[i % len(colors)]
      self.vpack.makeChild(uiclass=lev2.ui.LineEdit, args=[f"line{i}", f"Item {i}", color])

    ############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self, ctx):
    pass

  ##############################################

  def onUpdate(self, updinfo):
    pass

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

ScrollContainerTest().ezapp.mainThreadLoop()
