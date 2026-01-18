#!/usr/bin/env ork.python
################################################################################
# ScrollContainer Split Test
# Demonstrates Y, X, and XY scroll modes in a split layout
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, signal
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class ScrollContainerSplitTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, left=100, top=100, width=1000, height=700)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.clearColorGuide = vec4(0.15, 0.15, 0.2, 1)
    lg.margin = 4

    ############################################
    # LEFT: Y-scroll with vpack - start with full area
    ############################################

    left_item = lg.makeChild(uiclass=lev2.ui.ScrollContainer, args=["scroll_left"])
    left_item.layout.fill(lg.layout)
    self.scroll_left = left_item.widget
    self.scroll_left.scroll_mode = lev2.ui.ScrollMode.Y
    self.scroll_left.bg_color = vec4(0.12, 0.12, 0.15, 1.0)

    ############################################
    # TOP-RIGHT: XY-scroll with canvas - split from left (left keeps 30%)
    ############################################

    topright_item = lg.split(
      layout=left_item.layout,
      proportion=0.3,
      placement=tokens.RIGHT,
      uiclass=lev2.ui.ScrollContainer,
      args=["scroll_top_right"]
    )
    self.scroll_top_right = topright_item.widget
    self.scroll_top_right.scroll_mode = lev2.ui.ScrollMode.XY
    self.scroll_top_right.bg_color = vec4(0.1, 0.12, 0.1, 1.0)

    ############################################
    # BOTTOM-RIGHT: X-scroll with hpack - split from top-right (bottom gets 30%)
    ############################################

    bottom_item = lg.split(
      layout=topright_item.layout,
      proportion=0.7,
      placement=tokens.BOTTOM,
      uiclass=lev2.ui.ScrollContainer,
      args=["scroll_bottom"]
    )
    self.scroll_bottom = bottom_item.widget
    self.scroll_bottom.scroll_mode = lev2.ui.ScrollMode.X
    self.scroll_bottom.bg_color = vec4(0.12, 0.1, 0.12, 1.0)

    ############################################
    # LEFT: VerticalPack with many items (Y-scroll)
    ############################################

    self.vpack = lev2.ui.VerticalPack.wfactory(["vpack"])
    self.vpack.margin = 2
    self.vpack.item_height = 28
    self.vpack.fill = False
    self.vpack.bg_color = vec4(0.15, 0.15, 0.18, 1.0)
    self.vpack.draw_background = True
    self.scroll_left.setChild(self.vpack)

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
    # TOP-RIGHT: PrimCanvas with large content (XY-scroll)
    ############################################

    self.canvas = lev2.ui.PrimCanvas.wfactory(["canvas"])
    self.canvas.bg_color = vec4(0.08, 0.1, 0.08, 1.0)
    self.canvas.draw_background = True
    # Set desired size larger than container for XY scrolling
    self.canvas.desiredWidth = 1200
    self.canvas.desiredHeight = 1200
    self.scroll_top_right.setChild(self.canvas)

    # Animation state
    self.time = 0.0
    self.quad_count = 300
    self.quad_prim = None
    self.quad_data = []

    ############################################
    # BOTTOM: HorizontalPack with many items (X-scroll)
    ############################################

    self.hpack = lev2.ui.HorizontalPack.wfactory(["hpack"])
    self.hpack.margin = 2
    self.hpack.item_width = 80
    self.hpack.fill = False
    self.hpack.bg_color = vec4(0.15, 0.12, 0.15, 1.0)
    self.hpack.draw_background = True
    self.scroll_bottom.setChild(self.hpack)

    for i in range(50):
      color = vec4(colors[i % len(colors)], 1.0)
      self.hpack.makeChild(uiclass=lev2.ui.EvTestBox, args=[f"hitem{i}", color])

    ############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT to ezapp")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self, ctx):
    # Initialize canvas GPU resources
    self.canvas.gpuInit(ctx)

    # Create layer for quads
    self.main_layer = self.canvas.createLayer("main")

    # Create a QuadPrimitive with additive blending
    pipeline = self.canvas.pipelineSolid
    pipeline.rasterstate.setBlendingMacro(tokens.ADDITIVE)
    self.quad_prim = lev2.ui.QuadPrimitive(pipeline=pipeline)

    # Pre-create all QuadData objects
    for i in range(self.quad_count):
      qd = lev2.ui.QuadData()
      self.quad_data.append(qd)
      self.quad_prim.addQuad(qd)

    # Add primitive to layer
    self.main_layer.addPrimitive(self.quad_prim)

    # Initialize quad positions
    self._updateQuads(0.0)

  ##############################################

  def _updateQuads(self, time):
    """Update quad positions with animation spread across canvas desired size"""
    if not self.quad_prim:
      return

    # Use desired size (the full scrollable area)
    canvas_w = self.canvas.desiredWidth
    canvas_h = self.canvas.desiredHeight

    for i in range(self.quad_count):
      qd = self.quad_data[i]

      # Spread quads across the full canvas area with circular motion
      grid_x = (i % 15) / 15.0
      grid_y = (i // 15) / 20.0

      base_x = grid_x * canvas_w
      base_y = grid_y * canvas_h

      # Add some motion
      offset_x = 30 * math.sin(time * 2 + i * 0.3)
      offset_y = 30 * math.cos(time * 2 + i * 0.4)

      cx = base_x + offset_x
      cy = base_y + offset_y

      size = 40 + 15 * math.sin(time * 3 + i)

      qd.setPosition(cx - size/2, cy - size/2)
      qd.setSize(size, size)

      # Color cycling
      hue = (i / self.quad_count + time * 0.1) % 1.0
      r, g, b = self._hsv_to_rgb(hue, 0.8, 0.9)
      qd.setColor(vec4(r, g, b, 0.7))

      # Rotation
      qd.setRotation(time * 0.5 + i * 0.2)

    # Mark canvas dirty so SSBO gets rebuilt
    self.canvas.markDirty()

  ##############################################

  def _hsv_to_rgb(self, h, s, v):
    """Simple HSV to RGB conversion"""
    if s == 0.0:
      return v, v, v

    i = int(h * 6.0)
    f = (h * 6.0) - i
    p = v * (1.0 - s)
    q = v * (1.0 - s * f)
    t = v * (1.0 - s * (1.0 - f))
    i = i % 6

    if i == 0: return v, t, p
    if i == 1: return q, v, p
    if i == 2: return p, v, t
    if i == 3: return p, q, v
    if i == 4: return t, p, v
    if i == 5: return v, p, q

  ##############################################

  def onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self._updateQuads(self.time)

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

ScrollContainerSplitTest().ezapp.mainThreadLoop()
