#!/usr/bin/env ork.python

################################################################################
# PrimCanvas Widget Test
# Demonstrates GPU-accelerated canvas with SSBO-based quad rendering
################################################################################

import signal
import math
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class PrimCanvasTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, width=800, height=600, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.15, 0.15, 0.18, 1)

    # Create PrimCanvas widget
    canvas_layout = lg_group.makeChild(uiclass=lev2.ui.PrimCanvas, args=["test_canvas"])
    self.canvas = canvas_layout.widget

    # Anchor to fill parent
    root_layout = lg_group.layout
    canvas_layout.layout.top.anchorTo(root_layout.top)
    canvas_layout.layout.left.anchorTo(root_layout.left)
    canvas_layout.layout.bottom.anchorTo(root_layout.bottom)
    canvas_layout.layout.right.anchorTo(root_layout.right)

    # Canvas appearance
    self.canvas.bg_color = vec4(0.1, 0.1, 0.12, 1)
    self.canvas.draw_background = True

    # Event handling
    self.canvas.onUiEvent = self._onCanvasEvent

    # Animation state
    self.time = 0.0
    self.quad_count = 200

    # Create primitives (will be populated in onGpuInit)
    self.quad_prim = None
    self.quad_data = []

    # Signal handling
    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def onGpuInit(self, ctx):
    # Initialize canvas GPU resources (creates pipelines)
    self.canvas.gpuInit(ctx)

    # Create layer for quads
    self.main_layer = self.canvas.createLayer("main")

    # Create a QuadPrimitive with explicit pipeline
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

  def _updateQuads(self, time):
    """Update quad positions with animation"""
    if not self.quad_prim:
      return

    canvas_w = self.canvas.width
    canvas_h = self.canvas.height

    for i in range(self.quad_count):
      qd = self.quad_data[i]

      # Circular motion
      angle = (i / self.quad_count) * math.pi * 2 + time
      radius = 150 + 150 * math.sin(time * 2 + i * 0.5)

      cx = canvas_w / 2 + math.cos(angle) * radius
      cy = canvas_h / 2 + math.sin(angle) * radius

      size = 30 + 10 * math.sin(time * 3 + i)

      qd.setPosition(cx - size/2, cy - size/2)
      qd.setSize(size, size)

      # Color cycling
      hue = (i / self.quad_count + time * 0.1) % 1.0
      r, g, b = self._hsv_to_rgb(hue, 0.8, 0.9)
      qd.setColor(vec4(r, g, b, 0.8))

      # Rotation
      qd.setRotation(time + i * 0.3)

    # Mark canvas dirty so SSBO gets rebuilt
    self.canvas.markDirty()

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

  def _onCanvasEvent(self, ev):
    """Handle input events on canvas"""
    if ev.code == tokens.PUSH.hashed:
      print(f"Mouse pressed at ({ev.x}, {ev.y})")
    elif ev.code == tokens.RELEASE.hashed:
      print(f"Mouse released at ({ev.x}, {ev.y})")
    elif ev.code == tokens.KEY_DOWN.hashed:
      print(f"Key down: {ev.keycode}")

    return lev2.ui.HandlerResult()

  def onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self._updateQuads(self.time)

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

PrimCanvasTest().ezapp.mainThreadLoop()
