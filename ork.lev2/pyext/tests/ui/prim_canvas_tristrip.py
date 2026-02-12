#!/usr/bin/env ork.python

################################################################################
# PrimCanvas TriStrip Test
# Real-world use case: Audio waveform / oscilloscope visualization
################################################################################

import math
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.frame_profiler import FrameProfilerComponent

tokens = CrcStringProxy()

################################################################################

class WaveformVisualizer(ComponentizedApplication):

  def __init__(self):
    super().__init__()

    self.profiler = self.addComponent("profiler", FrameProfilerComponent,
                                      gpu_filter=["*", "-fwd:total"])

    self.createEzApp(width=1024, height=400, fullscreen=False)

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.05, 0.05, 0.08, 1)

    # Create PrimCanvas widget
    canvas_layout = lg_group.makeChild(uiclass=lev2.ui.PrimCanvas, args=["waveform_canvas"])
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
    self.sample_count = 256  # Number of sample points
    self.wave_thickness = 8.0  # Ribbon thickness in pixels

    # Primitives
    self.wave_prim = None
    self.wave_vertices = []

  def _onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)

    # Create layer for waveform
    self.main_layer = self.canvas.createLayer("main")

    # Create triangle strip for waveform ribbon
    # Each sample point needs 2 vertices (top and bottom of ribbon)
    self.wave_prim = lev2.ui.TriStripPrimitive(pipeline=self.canvas.pipelineVtxSolid)

    # Pre-create vertices (2 per sample for ribbon)
    for i in range(self.sample_count * 2):
      vd = lev2.ui.VertexData()
      self.wave_vertices.append(vd)
      self.wave_prim.addVertex(vd)

    self.main_layer.addPrimitive(self.wave_prim)
    self._updateWaveform(0.0)

  def _updateWaveform(self, time):
    if not self.wave_prim:
      return

    canvas_w = self.canvas.width
    canvas_h = self.canvas.height
    center_y = canvas_h / 2
    amplitude = canvas_h * 0.35

    for i in range(self.sample_count):
      # Calculate x position
      x = (i / (self.sample_count - 1)) * canvas_w

      # Generate complex waveform (mix of frequencies)
      t = time
      phase = (i / self.sample_count) * math.pi * 8
      wave = (
        math.sin(phase + t * 3) * 0.5 +
        math.sin(phase * 2.1 + t * 5) * 0.3 +
        math.sin(phase * 0.5 + t * 1.5) * 0.4 +
        math.sin(phase * 4.7 + t * 7) * 0.15
      )
      wave = wave / 1.35  # Normalize

      y = center_y + wave * amplitude

      # Calculate ribbon thickness based on wave intensity
      thickness = self.wave_thickness * (0.5 + abs(wave) * 0.5)

      # Color based on wave value (blue to cyan to green)
      hue = 0.5 + wave * 0.15  # Range 0.35 to 0.65
      r, g, b = self._hsv_to_rgb(hue, 0.8, 0.9)
      alpha = 0.7 + abs(wave) * 0.3

      # Bottom vertex of ribbon (first for correct winding after Y flip)
      vd_bot = self.wave_vertices[i * 2]
      vd_bot.setPosition(x, y + thickness)
      vd_bot.setColor(vec4(r * 0.6, g * 0.6, b * 0.6, alpha * 0.8))

      # Top vertex of ribbon
      vd_top = self.wave_vertices[i * 2 + 1]
      vd_top.setPosition(x, y - thickness)
      vd_top.setColor(vec4(r, g, b, alpha))

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

  def _onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self._updateWaveform(self.time)

  def _onUiEvent(self, uievent):
    return None

###############################################################################

app = WaveformVisualizer()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
