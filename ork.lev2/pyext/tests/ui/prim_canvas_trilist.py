#!/usr/bin/env ork.python

################################################################################
# PrimCanvas TriList Test
# Real-world use case: Flow field visualization with directional arrows
################################################################################

import signal
import math
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class FlowFieldVisualizer:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, width=800, height=800, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.08, 0.06, 0.1, 1)

    # Create PrimCanvas widget
    canvas_layout = lg_group.makeChild(uiclass=lev2.ui.PrimCanvas, args=["flowfield_canvas"])
    self.canvas = canvas_layout.widget
    self.canvas.supersample = 4

    # Anchor to fill parent
    root_layout = lg_group.layout
    canvas_layout.layout.top.anchorTo(root_layout.top)
    canvas_layout.layout.left.anchorTo(root_layout.left)
    canvas_layout.layout.bottom.anchorTo(root_layout.bottom)
    canvas_layout.layout.right.anchorTo(root_layout.right)

    self.canvas.bg_color = vec4(0.03, 0.02, 0.05, 1)
    self.canvas.draw_background = True

    # Grid parameters
    self.grid_cols = 20
    self.grid_rows = 20
    self.arrow_size = 12.0

    # Animation state
    self.time = 0.0

    # Primitives
    self.arrow_prim = None
    self.arrow_vertices = []  # 3 vertices per arrow

    # Signal handling
    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)

    # Create layer for arrows
    self.main_layer = self.canvas.createLayer("main")

    # Create triangle list for arrows
    # Each arrow is 1 triangle = 3 vertices
    arrow_count = self.grid_cols * self.grid_rows
    self.arrow_prim = lev2.ui.TriListPrimitive(pipeline=self.canvas.pipelineVtxSolid)

    for i in range(arrow_count * 3):
      vd = lev2.ui.VertexData()
      self.arrow_vertices.append(vd)
      self.arrow_prim.addVertex(vd)

    self.main_layer.addPrimitive(self.arrow_prim)
    self._updateArrows(0.0)

  def _flow_field(self, x, y, time):
    """Calculate flow direction at a point using curl noise"""
    # Normalize coordinates
    nx = x * 0.01
    ny = y * 0.01

    # Multiple octaves of rotation
    angle = (
      math.sin(nx * 2.0 + time * 0.5) * math.cos(ny * 1.5 + time * 0.3) +
      math.sin(nx * 0.5 - ny * 0.7 + time * 0.2) * 0.5 +
      math.cos(nx * 1.2 + ny * 1.8 - time * 0.4) * 0.3
    )

    # Add vortex centers
    cx1, cy1 = 0.3, 0.4
    cx2, cy2 = 0.7, 0.6
    dx1, dy1 = x / 800 - cx1, y / 800 - cy1
    dx2, dy2 = x / 800 - cx2, y / 800 - cy2
    dist1 = math.sqrt(dx1*dx1 + dy1*dy1) + 0.01
    dist2 = math.sqrt(dx2*dx2 + dy2*dy2) + 0.01

    vortex1 = math.atan2(dy1, dx1) + math.pi/2
    vortex2 = math.atan2(dy2, dx2) - math.pi/2

    angle += vortex1 * 0.3 / (dist1 * 5 + 0.5)
    angle += vortex2 * 0.2 / (dist2 * 5 + 0.5)

    # Calculate velocity magnitude
    speed = 0.5 + 0.5 * math.sin(angle * 2 + time)

    return angle, speed

  def _updateArrows(self, time):
    if not self.arrow_prim:
      return

    canvas_w = self.canvas.width
    canvas_h = self.canvas.height

    cell_w = canvas_w / self.grid_cols
    cell_h = canvas_h / self.grid_rows

    arrow_idx = 0
    for row in range(self.grid_rows):
      for col in range(self.grid_cols):
        # Center of cell
        cx = (col + 0.5) * cell_w
        cy = (row + 0.5) * cell_h

        # Get flow direction
        angle, speed = self._flow_field(cx, cy, time)

        # Arrow size based on speed
        size = self.arrow_size * (0.5 + speed * 0.8)

        # Arrow direction vectors
        dx = math.cos(angle)
        dy = math.sin(angle)

        # Perpendicular vector for arrow width
        px = -dy * size * 0.4
        py = dx * size * 0.4

        # Arrow tip
        tip_x = cx + dx * size
        tip_y = cy + dy * size

        # Arrow base corners
        base_x = cx - dx * size * 0.3
        base_y = cy - dy * size * 0.3

        # Color based on angle
        hue = (angle / (math.pi * 2) + 0.5) % 1.0
        r, g, b = self._hsv_to_rgb(hue, 0.7, 0.9)
        alpha = 0.6 + speed * 0.4

        # Triangle vertices
        v0 = self.arrow_vertices[arrow_idx * 3 + 0]  # tip
        v1 = self.arrow_vertices[arrow_idx * 3 + 1]  # base left
        v2 = self.arrow_vertices[arrow_idx * 3 + 2]  # base right

        v0.setPosition(tip_x, tip_y)
        v0.setColor(vec4(r, g, b, alpha))

        v1.setPosition(base_x + px, base_y + py)
        v1.setColor(vec4(r * 0.5, g * 0.5, b * 0.5, alpha * 0.7))

        v2.setPosition(base_x - px, base_y - py)
        v2.setColor(vec4(r * 0.5, g * 0.5, b * 0.5, alpha * 0.7))

        arrow_idx += 1

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
    self._updateArrows(self.time)

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

FlowFieldVisualizer().ezapp.mainThreadLoop()
