################################################################################
# AnalogClock - Canvas-based analog clock widget
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math
import datetime
from orkengine.core import vec2, vec4, CrcStringProxy, Rotor
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class AnalogClock:
  """Analog clock widget using PrimCanvas for rendering."""

  def __init__(self, canvas):
    self.canvas = canvas
    canvas.supersample = 4
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

    # Minute hand dragging state (using Klein rotors for accumulation)
    self.dragging_minute = False
    self.cumulative_rotor = Rotor.fromAngle2D(0.0)  # Total accumulated rotation
    self.rotor_windings = 0  # Track 2π crossings (where angle2D wraps from ±360° to ∓360°)
    self.last_rotor_angle2D = 0.0  # For detecting angle wraps
    self.last_drag_angle = 0.0  # Previous frame's mouse angle for incremental tracking

    # Cached geometry for hit testing
    self._cx = 0
    self._cy = 0
    self._radius = 0
    self._minute_angle = 0

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
      # Get click position in widget-local coordinates
      x, y = self._toLocalCoords(ev.x, ev.y)

      # Calculate distance from center
      dx = x - self._cx
      dy = y - self._cy
      dist = math.sqrt(dx * dx + dy * dy)

      if dist > self._radius:
        # Click outside circle - cycle color schemes
        self.current_scheme = (self.current_scheme + 1) % len(self.color_schemes)
        self._applyColorScheme()
      else:
        # Check if near minute hand
        # Use screen coords directly (Y increases downward)
        click_angle = math.atan2(dy, dx)
        # Match the stored minute_angle directly
        rendered_hand_angle = self._minute_angle
        angle_diff = self._normalize_angle(click_angle - rendered_hand_angle)

        # If within ~30 degrees of minute hand and not too close to center
        if abs(angle_diff) < 0.52 and dist > self._radius * 0.2:
          self.dragging_minute = True
          self.last_drag_angle = click_angle  # Initialize for incremental tracking (screen coords)

    elif ev.code == tokens.DRAG.hashed and self.dragging_minute:
      # Update rotation using rotor composition with incremental deltas
      x, y = self._toLocalCoords(ev.x, ev.y)
      dx = x - self._cx
      dy = y - self._cy
      current_angle = math.atan2(dy, dx)  # Screen coords (Y increases downward)

      # Calculate small angle delta from previous frame (not from drag start)
      # This delta will always be small since drag events are frequent
      angle_delta = self._normalize_angle(current_angle - self.last_drag_angle)
      self.last_drag_angle = current_angle  # Update for next frame

      # Create delta rotor and compose with cumulative rotor
      # In screen coords, clockwise = increasing angle = advancing time
      delta_rotor = Rotor.fromAngle2D(angle_delta)
      self.cumulative_rotor = self.cumulative_rotor * delta_rotor

      # Track winding count by detecting large jumps in angle2D
      # angle2D wraps from ±360° to ∓360° (jump of ~720° = 4π)
      # So we need to adjust by 2 windings (2 * 360° = 720°) to compensate
      curr_angle2D = self.cumulative_rotor.angle2D()
      angle_jump = curr_angle2D - self.last_rotor_angle2D

      # If angle jumped by more than 540° (3π), a wrap occurred
      if angle_jump > math.pi * 3:
        # Jumped from -360° to +360° = going negative direction
        self.rotor_windings -= 2
      elif angle_jump < -math.pi * 3:
        # Jumped from +360° to -360° = going positive direction
        self.rotor_windings += 2

      self.last_rotor_angle2D = curr_angle2D

    elif ev.code == tokens.RELEASE.hashed:
      self.dragging_minute = False

    return lev2.ui.HandlerResult()

  def _toLocalCoords(self, evx, evy):
    """Convert event coordinates to widget-local coordinates."""
    # Use the widget's rootToLocal method to properly convert coordinates
    return self.canvas.rootToLocal(evx, evy)

  def _normalize_angle(self, angle):
    """Normalize angle to [-pi, pi]."""
    while angle > math.pi:
      angle -= 2 * math.pi
    while angle < -math.pi:
      angle += 2 * math.pi
    return angle

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

    # Clock geometry - cache for hit testing
    cx, cy = w / 2, h / 2
    radius = min(w, h) * 0.42
    self._cx = cx
    self._cy = cy
    self._radius = radius

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
    # Convert cumulative rotor angle to minutes offset
    # angle2D() wraps at ±2π, so each winding = one 2π (360°) crossing = 60 minutes
    rotor_angle = self.cumulative_rotor.angle2D() + self.rotor_windings * 2 * math.pi
    minute_offset_from_rotor = rotor_angle / (2 * math.pi) * 60.0
    minutes = now.minute + seconds / 60.0 + minute_offset_from_rotor
    hours = (now.hour % 12) + minutes / 60.0

    # Cache minute angle for hit testing (before rendering)
    self._minute_angle = (minutes / 60.0) * math.pi * 2 - math.pi / 2

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
