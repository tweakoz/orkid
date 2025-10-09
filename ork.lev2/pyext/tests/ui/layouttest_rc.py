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
    rc = [3,2]

    # Create cells using makeRowsColumns
    self.griditems = lg_group.makeRowsColumns(
      rccounts=rc,
      margin = 4,
      uiclass = lev2.ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )

    # Change colors after creation so we can see individual cells
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

    # Debug: dump layout structure
    print("=" * 60)
    print(f"Created layout with rc={rc}")
    print(f"Number of items: {len(self.griditems)}")

    # Print vertical guides
    vguides = lg_group.vertical_guides
    print(f"\nVertical guides ({len(vguides)}):")
    for i, g in enumerate(sorted(vguides, key=lambda x: x.proportion)):
      print(f"  Guide {i}: proportion={g.proportion:.4f}, locked={g.locked}, margin={g.margin}")

    # Print horizontal guides
    hguides = lg_group.horizontal_guides
    print(f"\nHorizontal guides ({len(hguides)}):")
    for i, g in enumerate(sorted(hguides, key=lambda x: x.proportion)):
      print(f"  Guide {i}: proportion={g.proportion:.4f}, locked={g.locked}, margin={g.margin}")

    # Print actual widget geometries
    print("\nActual widget geometries:")
    for idx, item in enumerate(self.griditems):
      w = item.widget
      print(f"  Cell {idx} ({w.name}): x={w.x}, y={w.y}, w={w.width}, h={w.height}")
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
