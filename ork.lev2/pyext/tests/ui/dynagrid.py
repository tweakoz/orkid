#!/usr/bin/env ork.python

################################################################################
# DynaGrid Test - Shows 1-12 items in dynamic grid layouts
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
#
# DynaGrid is a smart grid layout widget that automatically arranges children
# in an optimal grid configuration based on:
#   - The number of child widgets
#   - The container's aspect ratio
#   - Optional aspect ratio constraints on cells
#
# Key Features:
#   - Automatically calculates optimal rows/columns for any item count
#   - Minimizes wasted cells and keeps cells reasonably square
#   - Centers incomplete rows (when last row has fewer items than columns)
#   - Supports configurable margins between cells
#   - Optional aspect ratio constraints (aspect_min, aspect_max)
#
# Usage:
#   # Create a DynaGrid
#   dg = parent.makeChild(uiclass=lev2.ui.DynaGrid, args=["MyGrid"])
#
#   # Configure optional properties
#   dg.margin = 8                # Pixels between cells (default: 4)
#   dg.aspect_min = 0.5          # Min cell aspect ratio (width/height, 0=ignore)
#   dg.aspect_max = 2.0          # Max cell aspect ratio (width/height, 0=ignore)
#
#   # Add children - DynaGrid will automatically arrange them
#   for i in range(n):
#       dg.makeChild(uiclass=lev2.ui.TextBox, args=[f"item_{i}", color, text])
#
# When to use DynaGrid:
#   - Variable number of items that need grid arrangement
#   - Items should be roughly equal size
#   - Want automatic layout optimization
#   - Don't need manual row/col control (use makeGrid for that)
#
################################################################################

import signal
from orkengine.core import vec3, vec4
from orkengine import lev2

################################################################################

class DynaGridTest(object):

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self,
                                      left=100,
                                      top=100,
                                      width=1200,
                                      height=900)

    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    lg_group.clearColorGuide = vec4(0.1, 0.1, 0.15, 1)

    ############################################
    # Create main layout with single cell
    ############################################

    self.griditems = lg_group.makeGrid(
      width=1,
      height=1,
      margin = 4,
      uiclass = lev2.ui.Box,
      args = ["label",vec4(0.1,0.1,0.3,1)],
    )

    ############################################
    # Create tab widget (tabs visible)
    ############################################

    tabs = lg_group.makeChild(
        uiclass=lev2.ui.TabsWidget,
        args=["DynaGrid Tests",vec3(0.5,0.5,1.0)]
    )
    self.tabsw = tabs.widget
    lg_group.replaceChild(self.griditems[0].layout, tabs)
    self.tabsw.showTabs = True

    ############################################
    # Create color palette for items
    ############################################

    self.colors = [
      vec4(0.8, 0.3, 0.3, 1),  # Red
      vec4(0.3, 0.8, 0.3, 1),  # Green
      vec4(0.3, 0.3, 0.8, 1),  # Blue
      vec4(0.8, 0.8, 0.3, 1),  # Yellow
      vec4(0.8, 0.3, 0.8, 1),  # Magenta
      vec4(0.3, 0.8, 0.8, 1),  # Cyan
      vec4(0.9, 0.6, 0.3, 1),  # Orange
      vec4(0.6, 0.3, 0.9, 1),  # Purple
      vec4(0.3, 0.9, 0.6, 1),  # Teal
      vec4(0.9, 0.5, 0.5, 1),  # Light Red
      vec4(0.5, 0.9, 0.5, 1),  # Light Green
      vec4(0.5, 0.5, 0.9, 1),  # Light Blue
    ]

    ############################################
    # Create 12 tabs, each with N items to demonstrate DynaGrid behavior
    #
    # This test shows how DynaGrid adapts its layout based on item count:
    #   1 item:  1x1 grid
    #   2 items: 1x2 or 2x1 based on container aspect ratio
    #   3 items: 2x2 grid with 1 empty cell, centered on last row
    #   4 items: 2x2 grid
    #   5 items: 2x3 grid with 1 empty cell
    #   6 items: 2x3 or 3x2 grid based on aspect ratio
    #   etc.
    #
    # The algorithm tries to:
    #   - Match the container's aspect ratio
    #   - Minimize wasted cells
    #   - Keep cells roughly square
    ############################################

    self.dynagrids = []

    for n in range(1, 13):
      # Create DynaGrid for this tab
      # TabsWidget.makeChild returns the widget directly (not a layout item)
      dg = self.tabsw.makeChild(
          uiclass=lev2.ui.DynaGrid,
          args=[f"{n} Items"]
      )
      self.dynagrids.append(dg)

      # Optional: Configure DynaGrid properties
      # dg.margin = 8              # Increase spacing between cells
      # dg.aspect_min = 0.5        # Prevent cells from being too tall
      # dg.aspect_max = 2.0        # Prevent cells from being too wide

      # Add N colored textboxes to the DynaGrid
      # DynaGrid automatically arranges children in optimal grid layout
      for i in range(n):
        color = self.colors[i % len(self.colors)]
        text = f"Item {i+1}"

        # DynaGrid.makeChild adds a child widget
        # Layout happens automatically when children are added
        dg.makeChild(
            uiclass=lev2.ui.TextBox,
            args=[f"item_{i}", color, text]
        )

    ############################################
    # Set initial tab
    ############################################

    self.tabsw.setActiveTab(0)

    ############################################
    # Signal handling
    ############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self,ctx):
    pass

  ##############################################

  def onUpdate(self,updinfo):
    pass

  ##############################################

  def onUiEvent(self,uievent):
    return lev2.ui.HandlerResult()

###############################################################################

DynaGridTest().ezapp.mainThreadLoop()
