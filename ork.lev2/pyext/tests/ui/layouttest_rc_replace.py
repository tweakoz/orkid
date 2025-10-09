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

    rc = [3,2]

    # Create cells using makeRowsColumns
    self.griditems = lg_group.makeRowsColumns(
      rccounts=rc,
      margin = 4,
      uiclass = lev2.ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )

    # Change colors so we can see individual cells
    colors = [
      vec4(1.0, 0.2, 0.2, 1),  # Red
      vec4(0.2, 1.0, 0.2, 1),  # Green
      vec4(0.2, 0.2, 1.0, 1),  # Blue
      vec4(1.0, 1.0, 0.2, 1),  # Yellow
      vec4(1.0, 0.2, 1.0, 1),  # Magenta
    ]
    for idx, item in enumerate(self.griditems):
      item.widget.color = colors[idx % len(colors)]

    self.lg_group = lg_group

    ############################################
    # Replace some cells with different colored boxes
    ############################################

    # Replace cell 0 (top-left) with a cyan box
    replacement0 = lg_group.makeChild(uiclass=lev2.ui.Box, args=["replacement0", vec4(0.0, 1.0, 1.0, 1)])
    lg_group.replaceChild(self.griditems[0].layout, replacement0)

    # Replace cell 2 (top-right) with an orange box
    replacement2 = lg_group.makeChild(uiclass=lev2.ui.Box, args=["replacement2", vec4(1.0, 0.5, 0.0, 1)])
    lg_group.replaceChild(self.griditems[2].layout, replacement2)

    # Replace cell 4 (bottom-right) with a white box
    replacement4 = lg_group.makeChild(uiclass=lev2.ui.Box, args=["replacement4", vec4(1.0, 1.0, 1.0, 1)])
    lg_group.replaceChild(self.griditems[4].layout, replacement4)

    print("=" * 60)
    print(f"Created layout with rc={rc}")
    print(f"Number of items: {len(self.griditems)}")
    print(f"Replaced cells 0, 2, and 4 with different colored boxes")
    print("Try dragging the guides - replaced widgets should resize correctly!")
    print("=" * 60)

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
