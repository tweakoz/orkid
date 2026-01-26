#!/usr/bin/env ork.python

################################################################################
# lev2 sample: Fullscreen 3D Mouse Mode Test
# Demonstrates fsmouse=True with virtual cursor rendering
# Run with: ./fullscreen3dmouse.py
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, argparse, os
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

################################################################################

tokens = CrcStringProxy()

################################################################################

class Fullscreen3DMouseApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 grid_variant="_V4",
                                 eye=vec3(0, 3, 8))
    # Run in fullscreen mode with fsmouse enabled (hide HW cursor, render virtual)
    self.createEzApp(name="Fullscreen3DMouse", ssaa=1, fullscreen=True, fsmouse=True,
                      use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################

  def _onUiInit(self):
    # Create LayoutSurface (512x512 pixels)
    self.layout_surface = lev2.ui.LayoutSurface("test_surface", w=512, h=512, margin=8)
    self.layout_surface.setVirtualSize(512, 512)

    # Add a grid of colored test boxes
    lg = self.layout_surface.layoutGroup
    self.test_items = lg.makeGrid(
      width=4,
      height=4,
      margin=8,
      uiclass=lev2.ui.EvTestBox,
      args=["box", vec4(0.2, 0.5, 0.8, 1)]
    )
    lg.clearColorGuide = vec4(0.8,0.6,0.2,1)

  ##############################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    SG = SGC.scenegraph

    # Create UISurfacePrimitiveData
    self.ui_prim_data = lev2.UISurfacePrimitiveData()
    self.ui_prim_data.size = 1.0  # 1 meter square
    self.ui_prim_data.blendMode = tokens.ADDITIVE
    self.ui_prim_data.doubleSided = True
    self.ui_prim_data.max_samples_per_axis = 8

    # Create drawable and node
    self.ui_drawable = self.ui_prim_data.createDrawable(surface=self.layout_surface)
    self.ui_node = SG.createDrawableNodeOnLayers(
      SGC.fwd_layers,
      "uisurface-node",
      self.ui_drawable
    )
    self.ui_node.sortkey = 100
    self.ui_node.worldTransform.translation = vec3(+4, 2, -8)
    self.ui_node.view_relative = True 

###############################################################################

app = Fullscreen3DMouseApp()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
