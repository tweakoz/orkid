#!/usr/bin/env ork.python

################################################################################
# lev2 sample: Fullscreen 3D Mouse Mode Test with Matplotlib Chart
# Demonstrates embedded UI surface with animated matplotlib chart
# Run with: ./test_fsmouse_mplib.py
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, sys, threading, time
import numpy as np
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
import matplotlib.pyplot as plt
from matplotlib.backends.backend_agg import FigureCanvasAgg

################################################################################

tokens = CrcStringProxy()

################################################################################

class FsmouseMplibApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.abstime = 0.0
    self.latest_image = None
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 grid_variant="_V4",
                                 eye=vec3(0, 3, 8))
    # Run in fullscreen mode with fsmouse enabled (hide HW cursor, render virtual)
    self.createEzApp(name="FsmouseMplib", ssaa=1, fullscreen=True, fsmouse=True,
                      use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################

  def _onUiInit(self):
    # Create LayoutSurface (512x512 pixels)
    self.layout_surface = lev2.ui.LayoutSurface("mplib_surface", w=512, h=512, margin=8)
    self.layout_surface.setVirtualSize(512, 512)

    # Add a 1x1 grid with an ImageView for matplotlib
    lg = self.layout_surface.layoutGroup
    lg.clearColorGuide = vec4(0.8, 0.8, 0.15, 1)

    self.chart_items = lg.makeGrid(
      width=1,
      height=1,
      margin=8,
      uiclass=lev2.ui.ImageView,
      args=["mplib_chart", vec4(0.1, 0.1, 0.1, 1)]
    )
    self.chart_imgview = self.chart_items[0].widget
    self.chart_imgview.maintain_aspect_ratio = False

    # Setup matplotlib figure
    plt.style.use('dark_background')
    self.fig = plt.figure(figsize=(5.12, 5.12), dpi=100)
    self.canvas = FigureCanvasAgg(self.fig)
    self.ax = self.fig.add_subplot(111)
    self.ax.plot([1, 2, 3, 4], [1, 4, 2, 3])

  ##############################################

  def imageProviderMatPlotLib(self):
    # Animate the plot
    self.ax.clear()
    t = self.abstime
    x = np.linspace(0, 4 * np.pi, 100)
    y = np.sin(x + t + np.sin(x + t * 2.5))
    self.ax.plot(x, y, color='cyan', linewidth=2)
    self.ax.fill_between(x, y, alpha=0.3, color='cyan')
    self.ax.set_ylim(-1.5, 1.5)
    self.ax.set_title("Animated Sine Wave", fontsize=14)
    self.ax.set_xlabel("x")
    self.ax.set_ylabel("sin(x + t + sin(x+t*2.5))")
    self.ax.grid(True, alpha=0.3)

    # Render to canvas
    self.canvas.draw()

    # Grab the RGBA buffer from the figure
    rgba_buf = np.asarray(self.canvas.buffer_rgba())

    # Create alpha channel based on brightness
    rgb = rgba_buf[:, :, :3].astype(np.float32)
    magnitude = np.sqrt(np.sum(rgb**2, axis=2))
    magnitude = np.clip(magnitude / (255 * np.sqrt(3)), 0, 1)
    rgba_buf = rgba_buf.copy()  # Make writable
    rgba_buf[:, :, 3] = (magnitude * 255).astype(np.uint8)

    # Create an ork image from the RGBA buffer
    w = rgba_buf.shape[1]
    h = rgba_buf.shape[0]
    image = lev2.Image.createFromBuffer(w, h, tokens.RGBA8, rgba_buf)
    return image

  ##############################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    SG = SGC.scenegraph

    # Start matplotlib rendering thread
    def mpl_thread_func():
      while True:
        self.latest_image = self.imageProviderMatPlotLib()
        time.sleep(1.0 / 20.0)  # 30 fps for chart

    self.mpl_thread = threading.Thread(target=mpl_thread_func, daemon=True)
    self.mpl_thread.start()

    # Set up image provider for the chart widget
    provider = lev2.ImageProvider.createFromLambda(lambda: self.latest_image)
    self.chart_imgview.setImageProvider(provider)

    # Create UISurfacePrimitiveData
    self.ui_prim_data = lev2.UISurfacePrimitiveData()
    self.ui_prim_data.size = 2.0  # 1 meter square
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

  ##############################################

  def _onUpdate(self, updevent):
    self.abstime = updevent.absolutetime
    self.layout_surface.setDirty()

###############################################################################

app = FsmouseMplibApp()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
