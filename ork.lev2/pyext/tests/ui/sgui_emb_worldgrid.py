#!/usr/bin/env ork.python

################################################################################
# lev2 sample: UI Surface embedded in 3D scene
# Demonstrates rendering a LayoutSurface as a billboard in a 3D scenegraph
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, argparse
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent

################################################################################

tokens = CrcStringProxy()

################################################################################

class UISurface3DApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 grid_variant="_V4",
                                 eye=vec3(0, 4, 12))
    self.createEzApp(name="UISurface3DTest", ssaa=1, fullscreen=False,
                      use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################

  def _onUiInit(self):
    # Create LayoutSurface (256x256 pixels)
    self.layout_surface = lev2.ui.LayoutSurface("test_surface", w=512, h=512, margin=8)
    self.layout_surface.setVirtualSize(512, 512)

    # Add a single colored test box
    lg = self.layout_surface.layoutGroup
    self.test_items = lg.makeGrid(
      width=4,
      height=4,
      margin=8,
      uiclass=lev2.ui.EvTestBox,
      args=["box", vec4(0.2, 0.5, 0.8, 1)]
    )
    lg.clearColorGuide = vec4(0.8,0.6,0.2,1)
    self.ezapp.uicontext.debug_event_routing = True
    self.layout_surface.uicontext.debug_event_routing = True
  ##############################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    SG = SGC.scenegraph

    # Create UISurfacePrimitiveData
    self.ui_prim_data = lev2.UISurfacePrimitiveData()
    self.ui_prim_data.size = 1.0  # 1 meter square
    self.ui_prim_data.blendMode = tokens.ALPHA
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
    self.ui_node.worldTransform.translation = vec3(0, 4, 0) 
    
###############################################################################

app = UISurface3DApp()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
