################################################################################
# AnalogClock - Canvas-based analog clock widget
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math
import datetime
from orkengine.core import vec2, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class AnalogClock:
  """Analog clock widget using PrimCanvas for rendering."""

  def __init__(self, canvas):
    self.canvas = canvas
    self.time_offset = 0  # For testing: add seconds to current time

    # Color schemes
    self.color_schemes = [
      {  # Classic dark
        'face': vec4(0.15, 0.15, 0.18, 1),
        'hour_tick': vec4(0.9, 0.9, 0.95, 1),
        'minute_tick': vec4(0.5, 0.5, 0.55, 1),
        'hour_hand': vec4(0.9, 0.9, 0.95, 1),
        'minute_hand': vec4(0.8, 0.8, 0.85, 1),
        'second_hand': vec4(0.9, 0.3, 0.2, 1),
        'center': vec4(0.7, 0.7, 0.75, 1),
      },
      {  # Ocean blue
        'face': vec4(0.05, 0.1, 0.2, 1),
        'hour_tick': vec4(0.4, 0.8, 1.0, 1),
        'minute_tick': vec4(0.2, 0.5, 0.7, 1),
        'hour_hand': vec4(0.3, 0.7, 0.9, 1),
        'minute_hand': vec4(0.5, 0.8, 1.0, 1),
        'second_hand': vec4(0.2, 1.0, 0.8, 1),
        'center': vec4(0.4, 0.9, 1.0, 1),
      },
      {  # Warm sunset
        'face': vec4(0.2, 0.1, 0.1, 1),
        'hour_tick': vec4(1.0, 0.8, 0.4, 1),
        'minute_tick': vec4(0.7, 0.5, 0.3, 1),
        'hour_hand': vec4(1.0, 0.7, 0.3, 1),
        'minute_hand': vec4(1.0, 0.6, 0.4, 1),
        'second_hand': vec4(1.0, 0.4, 0.2, 1),
        'center': vec4(1.0, 0.9, 0.5, 1),
      },
      {  # Mint green
        'face': vec4(0.1, 0.15, 0.1, 1),
        'hour_tick': vec4(0.6, 1.0, 0.7, 1),
        'minute_tick': vec4(0.3, 0.6, 0.4, 1),
        'hour_hand': vec4(0.5, 0.9, 0.6, 1),
        'minute_hand': vec4(0.6, 1.0, 0.7, 1),
        'second_hand': vec4(1.0, 0.5, 0.5, 1),
        'center': vec4(0.7, 1.0, 0.8, 1),
      },
    ]
    self.current_scheme = 0
    self._applyColorScheme()

    # Second hand oscillation state
    self.last_second = -1
    self.tick_time = 0.0  # Time since last tick
    self.oscillation_amplitude = 0.02  # Radians overshoot
    self.oscillation_frequency = 12.0  # Hz
    self.oscillation_decay = 6.0  # Decay rate

    # Primitive storage (initialized in gpuInit)
    self.tick_quads = []
    self.hand_vertices = []
    self.center_quad = None
    self._last_render_time = None
    self._gpu_initialized = False

    # Set up callbacks
    self.canvas.onPreRender = self._onPreRender
    self.canvas.onUiEvent = self._onUiEvent

  def _applyColorScheme(self):
    """Apply the current color scheme."""
    scheme = self.color_schemes[self.current_scheme]
    self.face_color = scheme['face']
    self.hour_tick_color = scheme['hour_tick']
    self.minute_tick_color = scheme['minute_tick']
    self.hour_hand_color = scheme['hour_hand']
    self.minute_hand_color = scheme['minute_hand']
    self.second_hand_color = scheme['second_hand']
    self.center_color = scheme['center']

  def _onUiEvent(self, ev):
    """Handle input events."""
    if ev.code == tokens.PUSH.hashed:
      # Click to cycle color schemes
      self.current_scheme = (self.current_scheme + 1) % len(self.color_schemes)
      self._applyColorScheme()
    return lev2.ui.HandlerResult()

  def _onPreRender(self):
    """Called by C++ before each render."""
    import time
    now = time.time()
    if self._last_render_time is not None:
      dt = now - self._last_render_time
      self.tick_time += dt
    self._last_render_time = now
    self._render()

  def _gpuInit(self):
    """Initialize GPU resources - called automatically on first render."""
    # Create layers (rendered in order)
    self.face_layer = self.canvas.createLayer("face")
    self.hands_layer = self.canvas.createLayer("hands")
    self.center_layer = self.canvas.createLayer("center")

    # Create tick marks (12 hour + 60 minute = 72 total)
    self.tick_prim = lev2.ui.QuadPrimitive(pipeline=self.canvas.pipelineSolid)
    for i in range(72):
      qd = lev2.ui.QuadData()
      self.tick_quads.append(qd)
      self.tick_prim.addQuad(qd)
    self.face_layer.addPrimitive(self.tick_prim)

    # Create hands using TriList (3 hands * 1 triangle each = 3 triangles = 9 vertices)
    hands_pipeline = self.canvas.pipelineVtxSolid
    hands_pipeline.rasterstate.culltest = tokens.OFF
    self.hands_prim = lev2.ui.TriListPrimitive(pipeline=hands_pipeline)
    for i in range(9):
      vd = lev2.ui.VertexData()
      self.hand_vertices.append(vd)
      self.hands_prim.addVertex(vd)
    self.hands_layer.addPrimitive(self.hands_prim)

    # Create center dot
    self.center_prim = lev2.ui.QuadPrimitive(pipeline=self.canvas.pipelineSolid)
    self.center_quad = lev2.ui.QuadData()
    self.center_prim.addQuad(self.center_quad)
    self.center_layer.addPrimitive(self.center_prim)
    self._gpu_initialized = True

  def _render(self):
    """Update all primitives based on current time."""
    # Lazy GPU init on first render
    if not self._gpu_initialized:
      self._gpuInit()
    w, h = self.canvas.width, self.canvas.height
    if w < 1 or h < 1:
      return

    # Clock geometry
    cx, cy = w / 2, h / 2
    radius = min(w, h) * 0.42

    # Get current time
    now = datetime.datetime.now()
    current_second = now.second

    # Detect tick and reset oscillation
    if current_second != self.last_second:
      self.last_second = current_second
      self.tick_time = 0.0

    # Calculate oscillation offset (damped spring)
    osc_offset = 0.0
    if self.tick_time < 0.5:  # Only oscillate for 0.5 seconds
      decay = math.exp(-self.oscillation_decay * self.tick_time)
      osc_offset = self.oscillation_amplitude * decay * math.sin(self.oscillation_frequency * math.pi * 2 * self.tick_time)

    seconds = current_second + self.time_offset  # Discrete ticking
    minutes = now.minute + seconds / 60.0
    hours = (now.hour % 12) + minutes / 60.0

    # Render tick marks
    self._render_ticks(cx, cy, radius)

    # Render hands
    self._render_hands(cx, cy, radius, hours, minutes, seconds, osc_offset)

    # Render center dot
    center_size = radius * 0.06
    self.center_quad.setPosition(cx - center_size / 2, cy - center_size / 2)
    self.center_quad.setSize(center_size, center_size)
    self.center_quad.setColor(self.center_color)

    self.canvas.markDirty()

  def _render_ticks(self, cx, cy, radius):
    """Render hour and minute tick marks."""
    qi = 0

    # Hour ticks (12)
    for i in range(12):
      angle = (i / 12.0) * math.pi * 2 - math.pi / 2
      inner_r = radius * 0.85
      outer_r = radius * 0.95
      tick_width = radius * 0.03

      # Calculate tick center position (negate Y for screen coords)
      mid_r = (inner_r + outer_r) / 2
      tick_x = cx + math.cos(angle) * mid_r
      tick_y = cy - math.sin(angle) * mid_r
      tick_len = outer_r - inner_r

      qd = self.tick_quads[qi]
      qd.setPosition(tick_x - tick_width / 2, tick_y - tick_len / 2)
      qd.setSize(tick_width, tick_len)
      qd.setRotation(-angle - math.pi / 2)
      qd.setColor(self.hour_tick_color)
      qi += 1

    # Minute ticks (60, but skip hour positions)
    for i in range(60):
      if i % 5 == 0:  # Skip hour positions
        qd = self.tick_quads[qi]
        qd.setSize(0, 0)  # Hide
        qi += 1
        continue

      angle = (i / 60.0) * math.pi * 2 - math.pi / 2
      inner_r = radius * 0.90
      outer_r = radius * 0.95
      tick_width = radius * 0.015

      mid_r = (inner_r + outer_r) / 2
      tick_x = cx + math.cos(angle) * mid_r
      tick_y = cy - math.sin(angle) * mid_r
      tick_len = outer_r - inner_r

      qd = self.tick_quads[qi]
      qd.setPosition(tick_x - tick_width / 2, tick_y - tick_len / 2)
      qd.setSize(tick_width, tick_len)
      qd.setRotation(-angle - math.pi / 2)
      qd.setColor(self.minute_tick_color)
      qi += 1

  def _render_hands(self, cx, cy, radius, hours, minutes, seconds, osc_offset):
    """Render clock hands as tapered triangles."""
    # Hand angles (0 = 12 o'clock, clockwise)
    hour_angle = (hours / 12.0) * math.pi * 2 - math.pi / 2
    minute_angle = (minutes / 60.0) * math.pi * 2 - math.pi / 2
    second_angle = (seconds / 60.0) * math.pi * 2 - math.pi / 2 + osc_offset

    # Hour hand (shortest, widest)
    self._set_hand_triangle(
      0, cx, cy, hour_angle,
      length=radius * 0.5,
      width=radius * 0.06,
      color=self.hour_hand_color
    )

    # Minute hand (longer, thinner)
    self._set_hand_triangle(
      1, cx, cy, minute_angle,
      length=radius * 0.75,
      width=radius * 0.04,
      color=self.minute_hand_color
    )

    # Second hand (longest, thin but visible)
    self._set_hand_triangle(
      2, cx, cy, second_angle,
      length=radius * 0.85,
      width=radius * 0.035,
      color=self.second_hand_color
    )

  def _set_hand_triangle(self, hand_idx, cx, cy, angle, length, width, color):
    """Set vertices for a tapered hand triangle."""
    # Triangle: tip at length, base at center with width
    # Negate Y for screen coordinates (Y=0 at top)
    cos_a = math.cos(angle)
    sin_a = -math.sin(angle)  # Flip Y

    # Perpendicular to (cos_a, sin_a) is (-sin_a, cos_a)
    perp_x = -sin_a * width / 2
    perp_y = cos_a * width / 2

    # Tip vertex
    tip_x = cx + cos_a * length
    tip_y = cy + sin_a * length

    # Base vertices (slightly behind center for better look)
    base_offset = -length * 0.15
    base_x = cx + cos_a * base_offset
    base_y = cy + sin_a * base_offset

    # Set vertices (CCW winding)
    vi = hand_idx * 3
    self.hand_vertices[vi + 0].setPosition(tip_x, tip_y)
    self.hand_vertices[vi + 0].setColor(color)

    self.hand_vertices[vi + 1].setPosition(base_x - perp_x, base_y - perp_y)
    self.hand_vertices[vi + 1].setColor(color)

    self.hand_vertices[vi + 2].setPosition(base_x + perp_x, base_y + perp_y)
    self.hand_vertices[vi + 2].setColor(color)

  ###########################################################################
  # Factory methods
  ###########################################################################

  @staticmethod
  def uifactory(parent_layoutgroup, args):
    """
    UI factory for use with layoutgroup.makeChild

    Args:
      parent_layoutgroup: Parent LayoutGroup
      args: [name] or [name, bg_color]

    Returns:
      uilayoutitem_ptr_t (the canvas's layout item)
    """
    name = args[0]
    bg_color = args[1] if len(args) > 1 else vec4(0.08, 0.08, 0.1, 1)

    # Create PrimCanvas via layoutgroup
    canvas_item = parent_layoutgroup.makeChild(uiclass=lev2.ui.PrimCanvas, args=[name])
    canvas = canvas_item.widget
    canvas.bg_color = bg_color
    canvas.draw_background = True

    # Create clock and store in canvas uservars
    clock = AnalogClock(canvas)
    canvas.uservars.analog_clock = clock

    return canvas_item

  @staticmethod
  def wfactory(args):
    """
    Widget factory for use with widget.makeChild (e.g., vpack.makeChild)

    Args:
      args: [name] or [name, bg_color]

    Returns:
      PrimCanvas widget containing the AnalogClock
    """
    name = args[0]
    bg_color = args[1] if len(args) > 1 else vec4(0.08, 0.08, 0.1, 1)

    canvas = lev2.ui.PrimCanvas.wfactory([name])
    canvas.bg_color = bg_color
    canvas.draw_background = True

    # Create clock and store in canvas uservars
    clock = AnalogClock(canvas)
    canvas.uservars.analog_clock = clock

    return canvas
