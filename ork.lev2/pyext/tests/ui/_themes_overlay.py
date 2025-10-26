#!/usr/bin/env ork.python

################################################################################
# OverlayComponent - Creates Blender-style overlay panels on edges of viewports
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math
from ork.app.application import ApplicationComponent
from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class OverlayComponent(ApplicationComponent):
  """
  Creates Blender-style overlay panels on edges of a scenegraph viewport
  """

  def __init__(self, grid_index, phase_offset=0.0):
    super().__init__()
    self.grid_index = grid_index
    self.phase_offset = phase_offset
    self.overlays = []

  ###############################################

  def _onGpuInit(self, ctx):
    self.lg_group = self.app.ezapp.topLayoutGroup
    self.uicontext = self.app.ezapp.uicontext

    # Get the grid layout for this viewport
    grid = self.app.multiscene.griditems[self.grid_index]
    grid_layout = grid.layout

    # Get guide edges of the grid viewport
    g_top = grid_layout.top
    g_bot = grid_layout.bottom
    g_lft = grid_layout.left
    g_rht = grid_layout.right

    # Left toolbar overlay (using offset guides from left edge)
    left_overlay = self.lg_group.makeChild(uiclass=lev2.ui.EvTestBox, args=[f"left_{self.grid_index}", vec4(0.2, 0.3, 0.4, 0.9)])

    left_overlay.layout.setRect(
      top=grid_layout.offsetHorizontalGuide(g_top, 8, locked=True),
      bottom=grid_layout.offsetHorizontalGuide(g_bot, -8, locked=True),
      left=grid_layout.offsetVerticalGuide(g_lft, 8, locked=True),
      right=grid_layout.offsetVerticalGuide(g_lft, 96, locked=True)
    )

    left_overlay.widget.theme = tokens.sg_overlay
    self.overlays.append({"widget": left_overlay.widget, "type": "left"})

    # Right properties overlay (using offset guides from right edge)
    right_overlay = self.lg_group.makeChild(uiclass=lev2.ui.EvTestBox, args=[f"right_{self.grid_index}", vec4(0.4, 0.3, 0.2, 0.9)])

    right_overlay.layout.setRect(
      top=grid_layout.offsetHorizontalGuide(g_top, 8, locked=True),
      bottom=grid_layout.offsetHorizontalGuide(g_bot, -8, locked=True),
      left=grid_layout.offsetVerticalGuide(g_rht, -128, locked=True),
      right=grid_layout.offsetVerticalGuide(g_rht, -8, locked=True)
    )

    right_overlay.widget.theme = tokens.sg_overlay
    self.overlays.append({"widget": right_overlay.widget, "type": "right"})

    # Top header overlay (spanning across top)
    top_overlay = self.lg_group.makeChild(uiclass=lev2.ui.EvTestBox, args=[f"top_{self.grid_index}", vec4(0.3, 0.4, 0.3, 0.9)])

    top_overlay.layout.setRect(
      top=grid_layout.offsetHorizontalGuide(g_top, 8, locked=True),
      bottom=grid_layout.offsetHorizontalGuide(g_top, 56, locked=True),
      left=grid_layout.offsetVerticalGuide(g_lft, 104, locked=True),
      right=grid_layout.offsetVerticalGuide(g_rht, -136, locked=True)
    )

    top_overlay.widget.theme = tokens.sg_overlay
    self.overlays.append({"widget": top_overlay.widget, "type": "top"})

    # Bottom timeline overlay (spanning across bottom)
    bottom_overlay = self.lg_group.makeChild(uiclass=lev2.ui.EvTestBox, args=[f"bottom_{self.grid_index}", vec4(0.4, 0.4, 0.2, 0.9)])

    bottom_overlay.layout.setRect(
      top=grid_layout.offsetHorizontalGuide(g_bot, -96, locked=True),
      bottom=grid_layout.offsetHorizontalGuide(g_bot, -8, locked=True),
      left=grid_layout.offsetVerticalGuide(g_lft, 104, locked=True),
      right=grid_layout.offsetVerticalGuide(g_rht, -136, locked=True)
    )

    bottom_overlay.widget.theme = tokens.sg_overlay
    self.overlays.append({"widget": bottom_overlay.widget, "type": "bottom"})

  ###############################################

  def _onGpuUpdate(self, ctx):
    # Animation is now handled by ThemesTestApp based on mode
    pass
