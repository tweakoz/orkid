################################################################################
# node_editor_math - view-transform, node geometry, picking and auto-layout
#   for the generic node editor (ork.ui.node_editor).
#
# Deliberately plain-python (stdlib only, NO lev2 / NO GPU) so the picking and
# view-transform math can be unit-tested headless via `python3`. node_editor.py
# imports these primitives; both rendering and hit-testing therefore agree on
# the exact same node geometry.
#
# Orientation: default is VERTICAL (Houdini network-editor style) — inputs on a
# node's TOP edge, outputs on its BOTTOM edge, graph flows top->bottom, which
# suits tall/narrow viewports. "horizontal" (ports left/right) is retained as a
# flag.
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math

DEFAULT_ORIENT = "vertical"          # "vertical" | "horizontal"

################################################################################
# Geometry constants (graph-space, i.e. pre-zoom pixels)
################################################################################

NODE_W          = 150.0   # node tile width
NODE_H          = 46.0    # node tile height (vertical orientation)
NODE_HEADER_H   = 22.0    # header/title strip height
PORT_ROW_H      = 18.0    # vertical pitch between ports (horizontal orientation)
NODE_BOTTOM_PAD = 8.0     # padding below the last port row (horizontal)
PILL_W          = 110.0   # boundary pseudo-node (pill) width
PILL_H          = 24.0    # boundary pseudo-node (pill) height

PORT_HIT_PX     = 9.0     # generous port hit radius, SCREEN pixels
ZOOM_MIN        = 0.1
ZOOM_MAX        = 3.0
GRID_SPACING    = 32.0    # base grid pitch, graph-space pixels
TEXT_ZOOM_MIN   = 0.4     # below this zoom draw no text (far-zoom boxes)

# auto-layout origin (graph-space) — the layered-DAG layout engine itself lives in
# ork.editor.graph_layout (the node editor binds it in for both seed + Shift+A).
LAYOUT_X0       = 90.0
LAYOUT_Y0       = 80.0


def clamp(v, lo, hi):
  return lo if v < lo else (hi if v > hi else v)


def node_height(n_in, n_out):
  """Height of a node box in HORIZONTAL orientation given its port counts."""
  rows = max(1, n_in, n_out)
  return NODE_HEADER_H + rows * PORT_ROW_H + NODE_BOTTOM_PAD


def node_size(n_in, n_out, kind="box", orient=DEFAULT_ORIENT):
  """(w, h) tile size for a node."""
  if kind in ("pill_in", "pill_out"):
    return (PILL_W, PILL_H)
  if orient == "vertical":
    return (NODE_W, NODE_H)
  return (NODE_W, node_height(n_in, n_out))


################################################################################
# ViewTransform - uniform zoom + 2d pan. graph<->screen and zoom-to-cursor.
#
#   screen = graph * s + (ox, oy)
#   graph  = (screen - (ox, oy)) / s
#
# Kept as plain scalars (no matrix inversion for picking, per design).
################################################################################

class ViewTransform:

  def __init__(self, zoom=1.0, ox=0.0, oy=0.0):
    self.s  = float(zoom)
    self.ox = float(ox)
    self.oy = float(oy)

  def graph_to_screen(self, gx, gy):
    return (gx * self.s + self.ox, gy * self.s + self.oy)

  def screen_to_graph(self, sx, sy):
    return ((sx - self.ox) / self.s, (sy - self.oy) / self.s)

  def pan_screen(self, dx, dy):
    self.ox += dx
    self.oy += dy

  def zoom_at(self, sx, sy, factor, zmin=ZOOM_MIN, zmax=ZOOM_MAX):
    """Multiply zoom by `factor` while keeping the graph point currently under
    the screen cursor (sx,sy) anchored at that same screen position."""
    gx, gy = self.screen_to_graph(sx, sy)
    new_s  = clamp(self.s * factor, zmin, zmax)
    self.s = new_s
    self.ox = sx - gx * new_s
    self.oy = sy - gy * new_s
    return new_s

  def set(self, s, ox, oy):
    self.s, self.ox, self.oy = float(s), float(ox), float(oy)

  def frame_bounds(self, minx, miny, maxx, maxy, view_w, view_h,
                   margin=0.09, zmin=ZOOM_MIN, zmax=ZOOM_MAX):
    """Fit graph-space bounds into a view of (view_w,view_h) screen px, centered,
    with a fractional margin. Fits BOTH axes (no wide-viewport assumption)."""
    bw = maxx - minx
    bh = maxy - miny
    if bw < 1e-4 or bh < 1e-4 or view_w < 2 or view_h < 2:
      return
    m = 1.0 - 2.0 * margin
    s = min(view_w * m / bw, view_h * m / bh)
    s = clamp(s, zmin, zmax)
    self.s = s
    cx = (minx + maxx) * 0.5
    cy = (miny + maxy) * 0.5
    self.ox = view_w * 0.5 - cx * s
    self.oy = view_h * 0.5 - cy * s


################################################################################
# Node geometry + picking (all graph-space unless noted)
################################################################################

def node_rect(pos, n_in, n_out, kind="box", orient=DEFAULT_ORIENT):
  """(x,y,w,h) tile for a node at top-left `pos`."""
  x, y = pos
  w, h = node_size(n_in, n_out, kind, orient)
  return (x, y, w, h)


def rect_contains(rx, ry, rw, rh, px, py):
  return rx <= px <= rx + rw and ry <= py <= ry + rh


def hit_test_node(gx, gy, node_rects):
  """node_rects: list of (id, x, y, w, h) in painter order (back->front).
  Returns the id of the top-most (last) node containing the point, or None."""
  for nid, x, y, w, h in reversed(node_rects):
    if rect_contains(x, y, w, h, gx, gy):
      return nid
  return None


def _spread(n):
  """Even in-margin fractions for n ports along an edge: (i+1)/(n+1)."""
  return [(i + 1) / (n + 1) for i in range(n)]


def port_anchors(pos, n_in, n_out, kind="box", orient=DEFAULT_ORIENT):
  """Return (inputs, outputs) lists of (x,y) port anchors in graph space.

  vertical  : inputs on the TOP edge, outputs on the BOTTOM edge.
  horizontal: inputs on the LEFT edge, outputs on the RIGHT edge.
  Boundary pills expose a single anchor (pill_in -> one output on its downstream
  edge, pill_out -> one input on its upstream edge)."""
  x, y = pos
  w, h = node_size(n_in, n_out, kind, orient)
  if orient == "vertical":
    if kind == "pill_in":
      return ([], [(x + w * 0.5, y + h)])
    if kind == "pill_out":
      return ([(x + w * 0.5, y)], [])
    ins  = [(x + w * f, y)     for f in _spread(n_in)]
    outs = [(x + w * f, y + h) for f in _spread(n_out)]
    return (ins, outs)
  # horizontal
  if kind == "pill_in":
    return ([], [(x + w, y + h * 0.5)])
  if kind == "pill_out":
    return ([(x, y + h * 0.5)], [])
  ins  = [(x,     y + h * f) for f in _spread(n_in)]
  outs = [(x + w, y + h * f) for f in _spread(n_out)]
  return (ins, outs)


def pick_port(gx, gy, node_port_geoms, radius_graph):
  """Find the nearest port within `radius_graph` of (gx,gy).

  node_port_geoms: list of dicts {id, inputs:[(x,y)..], outputs:[(x,y)..]}
  Returns (id, side, index) with side in ('in','out'), or None."""
  best = None
  best_d2 = radius_graph * radius_graph
  for g in node_port_geoms:
    nid = g["id"]
    for side, pts in (("in", g["inputs"]), ("out", g["outputs"])):
      for idx, (px, py) in enumerate(pts):
        dx = px - gx
        dy = py - gy
        d2 = dx * dx + dy * dy
        if d2 <= best_d2:
          best_d2 = d2
          best = (nid, side, idx)
  return best


def point_seg_dist2(px, py, ax, ay, bx, by):
  dx = bx - ax
  dy = by - ay
  seg2 = dx * dx + dy * dy
  if seg2 < 1e-9:
    ex, ey = px - ax, py - ay
    return ex * ex + ey * ey
  t = clamp(((px - ax) * dx + (py - ay) * dy) / seg2, 0.0, 1.0)
  cx = ax + t * dx
  cy = ay + t * dy
  ex, ey = px - cx, py - cy
  return ex * ex + ey * ey


def polyline_dist2(px, py, pts):
  best = float("inf")
  for i in range(len(pts) - 1):
    d2 = point_seg_dist2(px, py, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])
    if d2 < best:
      best = d2
  return best


def cubic_bezier(p0, p1, p2, p3, t):
  u = 1.0 - t
  uu, tt = u * u, t * t
  uuu, ttt = uu * u, tt * t
  x = uuu * p0[0] + 3 * uu * t * p1[0] + 3 * u * tt * p2[0] + ttt * p3[0]
  y = uuu * p0[1] + 3 * uu * t * p1[1] + 3 * u * tt * p2[1] + ttt * p3[1]
  return (x, y)


def wire_points(src, dst, orient=DEFAULT_ORIENT, segments=20, tangent=0.5):
  """Cubic bezier from output(src) to input(dst) with axis-aligned tangents.
  vertical: tangents leave downward from src / arrive from above into dst."""
  if orient == "vertical":
    dist = max(30.0, abs(dst[1] - src[1]) * tangent)
    cp1 = (src[0], src[1] + dist)
    cp2 = (dst[0], dst[1] - dist)
  else:
    dist = max(40.0, abs(dst[0] - src[0]) * tangent)
    cp1 = (src[0] + dist, src[1])
    cp2 = (dst[0] - dist, dst[1])
  return [cubic_bezier(src, cp1, cp2, dst, i / segments) for i in range(segments + 1)]


################################################################################
# Deterministic port/type -> color palette (hash based, overridable upstream)
################################################################################

def _hsv_to_rgb(h, s, v):
  i = int(h * 6.0)
  f = (h * 6.0) - i
  p = v * (1.0 - s)
  q = v * (1.0 - s * f)
  t = v * (1.0 - s * (1.0 - f))
  i %= 6
  return [(v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q)][i]


def type_color(type_name, sat=0.55, val=0.95):
  """Deterministic RGB (0..1 tuple) for a port/wire type name."""
  h = 0
  for ch in str(type_name):
    h = (h * 131 + ord(ch)) & 0xFFFFFFFF
  hue = (h % 997) / 997.0
  return _hsv_to_rgb(hue, sat, val)
