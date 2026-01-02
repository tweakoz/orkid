#!/usr/bin/env ork.python

################################################################################
# PrimCanvas Text Test
# Animated text with different colors
################################################################################

import signal
import math
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class TextCanvasTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, width=1024, height=768, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.05, 0.05, 0.08, 1)

    # Create PrimCanvas widget
    canvas_layout = lg_group.makeChild(uiclass=lev2.ui.PrimCanvas, args=["text_canvas"])
    self.canvas = canvas_layout.widget

    # Anchor to fill parent
    root_layout = lg_group.layout
    canvas_layout.layout.top.anchorTo(root_layout.top)
    canvas_layout.layout.left.anchorTo(root_layout.left)
    canvas_layout.layout.bottom.anchorTo(root_layout.bottom)
    canvas_layout.layout.right.anchorTo(root_layout.right)

    self.canvas.bg_color = vec4(0.02, 0.02, 0.04, 1)
    self.canvas.draw_background = True

    # Animation state
    self.time = 0.0

    # Text configuration
    self.words = [
      "ORKID", "ENGINE", "CANVAS", "TEXT", "RENDER",
      "GPU", "SHADER", "SSBO", "VULKAN", "PYTHON",
      "WIDGET", "PRIM", "VECTOR", "MATRIX", "FLOAT",
      "VERTEX", "FRAGMENT", "PIPELINE", "BUFFER", "TEXTURE"
    ]
    self.text_count = 400
    self.text_prims = []
    self.text_data = []  # Store animation parameters per text

    # Signal handling
    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)

    # Get default font
    self.font = lev2.FontManager.fontForId("i14")

    # Create text primitives with random-ish parameters
    for i in range(self.text_count):
      # Animation parameters
      params = {
        'word': self.words[i % len(self.words)],
        'base_x': (i % 20) * 50 + 30,
        'base_y': (i // 20) * 40 + 20,
        'phase_x': i * 0.7,
        'phase_y': i * 1.1,
        'speed_x': 0.8 + (i % 5) * 0.3,
        'speed_y': 1.2 + (i % 7) * 0.2,
        'amplitude_x': 15 + (i % 4) * 8,
        'amplitude_y': 12 + (i % 3) * 6,
        'hue_offset': i / self.text_count,
        'hue_speed': 0.1 + (i % 6) * 0.05
      }
      self.text_data.append(params)

      # Initial color
      hue = params['hue_offset']
      r, g, b = self._hsv_to_rgb(hue, 0.8, 0.95)
      color = vec4(r, g, b, 1.0)

      # Create text primitive
      text_prim = lev2.ui.TextPrimitive(font=self.font, color=color)
      text_prim.addItem(params['word'], vec2(params['base_x'], params['base_y']))
      self.text_prims.append(text_prim)
      self.canvas.addPrimitive(text_prim)

    self._updateText(0.0)

  def _updateText(self, time):
    if not self.text_prims:
      return

    canvas_w = self.canvas.width
    canvas_h = self.canvas.height
    if canvas_w < 1 or canvas_h < 1:
      return

    # Clear and recreate all text primitives with new positions/colors
    for i, (prim, params) in enumerate(zip(self.text_prims, self.text_data)):
      # Animated position
      x = params['base_x'] + math.sin(time * params['speed_x'] + params['phase_x']) * params['amplitude_x']
      y = params['base_y'] + math.sin(time * params['speed_y'] + params['phase_y']) * params['amplitude_y']

      # Wrap positions
      x = x % canvas_w
      y = y % canvas_h

      # Animated color
      hue = (params['hue_offset'] + time * params['hue_speed']) % 1.0
      r, g, b = self._hsv_to_rgb(hue, 0.75, 0.95)

      # Pulse alpha
      alpha = 0.7 + 0.3 * math.sin(time * 2 + i * 0.5)

      # Update primitive
      prim.clearItems()
      prim.addItem(params['word'], vec2(x, y))
      # Note: color is set at construction, can't change dynamically
      # For dynamic colors, would need to recreate primitives

    self.canvas.markDirty()

  def _hsv_to_rgb(self, h, s, v):
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

  def onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self._updateText(self.time)

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

TextCanvasTest().ezapp.mainThreadLoop()
