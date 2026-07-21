################################################################################
# node_editor - generic GPU node editor widget (PrimCanvas composite).
#
# A reusable, model-agnostic node editor surface (Houdini-network-editor style:
# vertical flow, plugs on top/bottom). It binds an abstract MODEL PROTOCOL (see
# NodeGraphModel below) rather than any concrete graph class, so a future
# GraphDocument adapter can drop in unchanged. Named "node editor" to avoid
# confusion with the data-plotting GraphView widget.
#
# Coordinate contract (PrimCanvas is top-left-origin / Y-down since 9359c9e20):
#   * graph-space geometry (node tiles, wires, halos, plugs) lives in world
#     layers whose per-layer _transform carries the view (scale-then-translate),
#     so they pan/zoom on the GPU with no rebuild.
#   * TextPrimitive is screen-fixed (ignores layer transforms), so ALL text is
#     positioned in screen space in Python via ViewTransform.graph_to_screen.
#   * TriList triangles are wound front-facing (negative signed area in Y-down)
#     — the vtx pipeline culls back-facing tris even with CullTest=OFF.
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math
import time
from orkengine.core import vec2, vec3, vec4, mtx4, CrcStringProxy
from orkengine import lev2
from ork.ui import node_editor_math as gm

tokens = CrcStringProxy()

################################################################################
# NodeGraphModel — the seam a GraphDocument adapter (or the toy harness) fills.
#
# Duck-typed; this base documents the contract and provides safe defaults for
# the optional hooks. Node ids are any hashable value.
################################################################################

class NodeGraphModel:
  """Abstract model consumed by NodeEditor. Implement the methods below.

  Topology / node introspection (all keyed by node id):
    nodes()                     -> iterable of node ids
    name(id)                    -> str  (display name)
    type_name(id)               -> str  (type label; drives deterministic color)
    color(id)                   -> vec4 | None  (optional fill hint)
    pos(id)                     -> (x, y) graph-space top-left, or None if unset
    set_pos(id, x, y)           -> move a node (graph space)
    inputs(id)                  -> ordered [(plug_name, type_name), ...]
    outputs(id)                 -> ordered [(plug_name, type_name), ...]

  Edges:
    edges()                     -> iterable of (src_id, src_plug, dst_id, dst_plug)
    can_connect(si, sp, di, dp) -> (bool, reason_str)   [called LIVE during drag]
    connect(si, sp, di, dp)     -> perform connection
    disconnect(si, sp, di, dp)  -> remove connection

  Authoring:
    add_node(type_name, pos)    -> new node id (positioned at pos)
    node_types()                -> [slash-path str, ...] for the Tab menu
    delete_node(id)             -> remove node (and incident edges)

  Grouping (optional):
    is_group(id)                -> bool
    child_model(id)             -> NodeGraphModel for the group interior
    render_kind(id)             -> 'box' | 'pill_in' | 'pill_out'  (boundary pills)

  Notification:
    on_changed(cb)              -> register a no-arg callback fired on mutation

  Flag capability + body style (optional; default True / 'solid'):
    has_display_flag(id)        -> bool  (False hides the display button entirely —
                                          neither drawn nor hit-tested; the flag can
                                          never apply, e.g. display-select on a capture)
    has_bypass_flag(id)         -> bool  (False hides the bypass button entirely)
    fill_style(id)              -> 'solid' | 'hollow'  (hollow = outline + header, no
                                          body fill; an artifact-export sink)

  Persistence (optional): save_layout()/load_layout() are called by the editor
  after node drags if present.
  """

  def color(self, nid):        return None
  def is_group(self, nid):     return False
  def child_model(self, nid):  return None
  def render_kind(self, nid):  return "box"
  def icon(self, nid):         return None   # optional SVG string, centered above label
  def on_changed(self, cb):    pass

  # Node-body flags (Houdini-style). Optional — the canvas draws + toggles
  # the output(display)/bypass flags only if the model implements the setters.
  # A setter returns None on success (the badge change is the feedback) or a
  # (kind, message) pair (kind in 'noop'|'refused'|'failed') the canvas surfaces
  # as a short-lived, colour-coded status so no-op / refused / failed read
  # distinctly from a change that worked.
  def is_output(self, nid):        return False
  def set_output(self, nid, on):   pass
  def is_bypassed(self, nid):      return False
  def set_bypassed(self, nid, on): pass

  # Per-node flag CAPABILITY + body style (all default True/'solid' so the toy model
  # and any existing adapter keep working unchanged): a flag whose has_*_flag is False
  # is neither drawn NOR hit-tested (the flag can never apply to this node — e.g. the
  # display flag on a capture); fill_style 'hollow' renders the body as an outline +
  # header strip (no fill) for artifact-export sinks.
  def has_display_flag(self, nid): return True
  def has_bypass_flag(self, nid):  return True
  def fill_style(self, nid):       return "solid"


################################################################################
# Palette
################################################################################

COL_BG          = vec4(0.09, 0.095, 0.11, 1.0)
COL_GRID_MINOR  = vec4(1.0, 1.0, 1.0, 0.055)
COL_GRID_MAJOR  = vec4(1.0, 1.0, 1.0, 0.11)
COL_NODE_BODY   = vec4(0.19, 0.20, 0.235, 1.0)
COL_GROUP_BODY  = vec4(0.16, 0.17, 0.22, 1.0)
COL_PILL_BODY   = vec4(0.22, 0.22, 0.28, 1.0)
COL_TEXT_NAME   = vec4(0.93, 0.94, 0.97, 1.0)
COL_TEXT_DIM    = vec4(0.52, 0.54, 0.60, 0.60)   # bypassed node name (faint)
COL_TEXT_TYPE   = vec4(0.60, 0.62, 0.68, 1.0)
COL_HALO        = vec4(0.30, 0.62, 1.00, 0.55)
COL_WIRE_SEL    = vec4(1.00, 0.85, 0.35, 1.0)
WIRE_ALPHA      = 0.82    # unselected wires: the SOURCE node's identity color, slightly dimmed
COL_PATHBAR_BG  = vec4(0.05, 0.05, 0.07, 0.94)
COL_SNAP_ON     = vec4(0.20, 0.46, 0.29, 1.0)
COL_SNAP_OFF    = vec4(0.14, 0.14, 0.17, 1.0)
COL_CRUMB       = vec4(0.80, 0.82, 0.88, 1.0)
COL_CRUMB_LAST  = vec4(0.55, 0.80, 1.00, 1.0)
COL_MARQUEE     = vec4(0.40, 0.70, 1.00, 0.9)
COL_MARQUEE_FIL = vec4(0.40, 0.70, 1.00, 0.12)
COL_OK          = vec4(0.40, 0.92, 0.48, 1.0)
COL_BAD         = vec4(0.95, 0.38, 0.32, 1.0)
# drag-connect drop targets: a NODE with >=1 qualifying input plug for the dragged output.
# every drop target draws a green outline; the hovered one adds a fill wash + brighter ring.
COL_DROP_TGT    = vec4(0.40, 0.92, 0.48, 0.55)   # drop-target outline (all qualifying nodes)
COL_DROP_HOV    = vec4(0.60, 1.00, 0.66, 0.16)   # hovered drop-target fill wash
COL_TOOLTIP_BG  = vec4(0.03, 0.03, 0.05, 0.92)
COL_TOOLTIP_TX  = vec4(0.92, 0.94, 0.98, 1.0)
# short-lived flag-mutation status strip (below the path bar): refused/failed = red,
# no-op = dim, so each reads distinctly from a change that WORKED (badge change only).
COL_STATUS_BG   = vec4(0.05, 0.05, 0.07, 0.94)
COL_STATUS_BAD  = vec4(0.97, 0.45, 0.38, 1.0)   # refused / failed
COL_STATUS_NOOP = vec4(0.72, 0.74, 0.42, 1.0)   # no-op (already in that state)
STATUS_TTL_S    = 4.0

PATHBAR_H       = 24.0
WIRE_HIT_PX     = 7.0
HOVER_PLUG_PX   = 11.0
NODE_RADIUS     = 7.0
FLAG_STRIP      = 4.0     # graph-space accent strip thickness

# node-body flags (Houdini-style): output/display + bypass toggles
FLAG_SZ         = 10.0
COL_FLAG_OUTPUT = vec4(0.33, 0.62, 1.00, 1.0)   # display flag / node ring (blue)
COL_FLAG_BYPASS = vec4(0.95, 0.80, 0.25, 1.0)   # bypass flag (yellow)
COL_BTN_INNER   = vec4(0.02, 0.02, 0.03, 1.0)   # button interior (black)
COL_OUT_RING    = vec4(1.00, 1.00, 1.00, 1.00)  # display node ring (white)
COL_OUT_GLOW    = vec4(1.00, 1.00, 1.00, 0.28)  # display node outer glow (white)
COL_BYPASS_X    = vec4(0.82, 0.84, 0.90, 0.30)  # bypass faint X across node
COL_GLYPH_OFF   = vec4(0.55, 0.57, 0.62, 0.55)  # button glyph, inactive (faint)

# font buckets by zoom (id -> approx advance width / line height, px)
FONT_ADV = {"i12": 6, "i14": 7, "i18": 9, "i24": 11, "i32": 16, "i48": 24}
FONT_H   = {"i12": 11, "i14": 13, "i18": 15, "i24": 21, "i32": 29, "i48": 40}


# white-on-transparent glyph SVGs (tinted per-state via the quad color, since
# the canvas textured shader computes texc * frg_color)
_SVG_EYE = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <ellipse cx="12" cy="12" rx="11" ry="6.6" fill="none" stroke="#ffffff" stroke-width="1.9"/>
  <circle cx="12" cy="12" r="3.3" fill="#ffffff"/>
</svg>'''

_SVG_X = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <line x1="3.5" y1="3.5" x2="20.5" y2="20.5" stroke="#ffffff" stroke-width="3.0" stroke-linecap="round"/>
  <line x1="20.5" y1="3.5" x2="3.5" y2="20.5" stroke="#ffffff" stroke-width="3.0" stroke-linecap="round"/>
</svg>'''


def font_for_zoom(s):
  if s < gm.TEXT_ZOOM_MIN:
    return None
  if s < 0.60:  return "i12"
  if s < 0.95:  return "i14"
  if s < 1.50:  return "i18"
  if s < 2.30:  return "i24"
  if s < 2.75:  return "i32"
  return "i48"


def _c(rgb, a=1.0):
  return vec4(rgb[0], rgb[1], rgb[2], a)


def _mix(base, rgb, t):
  return vec4(base.x * (1 - t) + rgb[0] * t,
             base.y * (1 - t) + rgb[1] * t,
             base.z * (1 - t) + rgb[2] * t, base.w)


def _brighten(base, t=0.42):
  return vec4(base.x + (1 - base.x) * t,
             base.y + (1 - base.y) * t,
             base.z + (1 - base.z) * t, base.w)


################################################################################
# Triangle geometry helpers (front-facing winding, Y-down).
################################################################################

def _vd(x, y, color):
  vd = lev2.ui.VertexData()
  vd.setPosition(x, y)
  vd.setColor(color)
  return vd


def _thick_polyline(prim, points, color, thickness):
  """Add a thick polyline (as triangle pairs) to a TriListPrimitive.
  Front-facing winding (negative signed area) — canvas is winding-sensitive."""
  for i in range(len(points) - 1):
    ax, ay = points[i]
    bx, by = points[i + 1]
    dx, dy = bx - ax, by - ay
    seg = math.sqrt(dx * dx + dy * dy)
    if seg < 1e-4:
      continue
    nx, ny = -dy / seg * thickness, dx / seg * thickness
    prim.addVertex(_vd(ax + nx, ay + ny, color))
    prim.addVertex(_vd(bx + nx, by + ny, color))
    prim.addVertex(_vd(ax - nx, ay - ny, color))
    prim.addVertex(_vd(bx + nx, by + ny, color))
    prim.addVertex(_vd(bx - nx, by - ny, color))
    prim.addVertex(_vd(ax - nx, ay - ny, color))


def _add_circle(prim, cx, cy, radius, color, segments=12):
  step = 2.0 * math.pi / segments
  for i in range(segments):
    a0 = i * step
    a1 = (i + 1) * step
    prim.addVertex(_vd(cx, cy, color))
    prim.addVertex(_vd(cx + math.cos(a1) * radius, cy + math.sin(a1) * radius, color))
    prim.addVertex(_vd(cx + math.cos(a0) * radius, cy + math.sin(a0) * radius, color))


def _rect_outline(prim, x, y, w, h, color, thickness):
  pts = [(x, y), (x + w, y), (x + w, y + h), (x, y + h), (x, y)]
  _thick_polyline(prim, pts, color, thickness)


def _add_quad(prim, x, y, w, h, color, radius=0.0):
  qd = lev2.ui.QuadData()
  qd.setPosition(x, y)
  qd.setSize(w, h)
  qd.setColor(color)
  if radius > 0:
    qd.setCornerRadius(radius)
  prim.addQuad(qd)


def _add_quad_tex(prim, x, y, w, h, color):
  qd = lev2.ui.QuadData()
  qd.setPosition(x, y)
  qd.setSize(w, h)
  qd.setColor(color)
  qd.setUV(0, 1, 1, 0)
  prim.addQuad(qd)


################################################################################
# NodeEditor
################################################################################

class NodeEditor:
  """Generic node editor bound to a NodeGraphModel via a PrimCanvas."""

  def __init__(self, canvas, model, title="root", orientation="vertical"):
    self.canvas = canvas
    canvas.supersample = 3
    self.uicontext = None                 # set by host for the Tab add-node menu
    self.orientation = orientation        # "vertical" (Houdini) | "horizontal"

    self.view = gm.ViewTransform(1.0, 0.0, 0.0)
    self.nav_stack = [(model, title)]     # [(model, crumb_label), ...]

    self.sel_nodes = set()
    self.sel_edges = set()

    # optional host hook: fired (model, primary_nid|None) when the single-node
    # selection identity changes — lets a host bind a property sheet to the picked
    # node (empty selection -> None). Deduped so it fires once per identity change.
    self.on_selection_changed = None
    self._sel_emitted = ("\x00unset", None)

    # optional host hook: fired (pre, post, restore) after a Shift+A auto-layout applies
    # ONE positions edit — pre/post are model-level position snapshots and `restore` is the
    # byte-exact undo callable, so a host wires it onto its OWN undo stack (the existing
    # ork.editor.undo_stack.UndoStack) without the widget owning a parallel one. Absent =
    # auto-layout still applies + persists, just not host-undoable (drags behave the same).
    self.on_layout_edit = None

    self.mode = None                      # None|'pan'|'drag_nodes'|'marquee'|'port_drag'
    self.snap = False                     # grid snap on node drag ('s' toggles)
    self._mb = {"left": False, "middle": False, "right": False}  # z/x/c emulation
    self._drag_orig = {}                  # node -> pos at drag start
    self._drag_start_g = (0.0, 0.0)
    self._drag_ref = None                 # the grabbed node (snap anchor)
    self._last_mx = self._last_my = 0.0
    self._last_gx = self._last_gy = 0.0
    self._last_root = (0, 0)
    self._pointer_inside = False
    self._marquee = None
    self._port_src = None                 # (id, out_index, (gx,gy)) — the dragged OUTPUT plug
    self._port_cursor = None
    # drag-to-node connect: the qualifying drop targets are computed ONCE at drag start
    # (the source output is fixed for the whole drag). _drop_targets maps a node id ->
    # its ordered candidate input plugs [(in_index, plug_name, existing_edge_or_None)];
    # _port_node is the currently-hovered drop-target node (or None over empty/unqualified).
    self._drop_targets = {}
    self._port_node = None
    self._port_ok = False
    # detach drag: the connected input plug (wire's dest end) is grab-able to pull the wire
    # OFF. When set, the port_drag is seeded from the wire's SOURCE output and _detach_edge
    # is the edge being lifted — release on empty disconnects it, release on a node re-routes.
    self._detach_edge = None              # the edge being detached (grab-the-plug-end), or None
    self._hover = None                    # ("plug",id,side,idx) | ("node",id) | None
    self._crumb_regions = []
    self._snap_region = None              # (x,y,w,h) pathbar snap toggle
    self._status = None                   # (text, color) short-lived flag-mutation feedback
    self._status_expiry = 0.0             # time.monotonic() deadline; cleared in _onPreRender

    self.rebuild_count = 0                 # perf instrument: Python-side rebuilds

    self._dirty = {k: True for k in ("grid", "groupfx", "wires", "nodes", "overlay")}
    self._gpu = False
    self._did_initial_frame = False
    self._last_w = self._last_h = 0
    self._bound_models = set()
    self._tex_eye = None                  # glyph textures (built in gpuInit)
    self._tex_x = None
    self._ctx = None                      # stashed for on-demand icon textures
    self._icon_cache = {}                 # svg string -> texture

    canvas.onPreRender = self._onPreRender
    canvas.onUiEvent = self._onUiEvent
    self._bind_model(model)

  # -- model access ----------------------------------------------------------

  @property
  def model(self):
    return self.nav_stack[-1][0]

  def _bind_model(self, model):
    if id(model) in self._bound_models:
      return
    self._bound_models.add(id(model))
    cb = getattr(model, "on_changed", None)
    if cb:
      cb(self._on_model_changed)

  def _on_model_changed(self):
    self._ensure_layout()
    self.mark_structure_changed()

  def _emit_selection(self):
    """Notify the host of a single-node selection identity change (or empty). No-op if
    no host hook is bound; deduped so drags / view changes never re-fire it."""
    cb = self.on_selection_changed
    if cb is None:
      return
    cur = next(iter(self.sel_nodes)) if len(self.sel_nodes) == 1 else None
    key = (id(self.model), cur)
    if key == self._sel_emitted:
      return
    self._sel_emitted = key
    cb(self.model, cur)

  def _render_kind(self, model, nid):
    fn = getattr(model, "render_kind", None)
    return fn(nid) if fn else "box"

  # -- dirty helpers ---------------------------------------------------------

  def _mark(self, *layers):
    for l in layers:
      self._dirty[l] = True

  def mark_view_changed(self):
    self._mark("grid", "nodes")           # world layers follow GPU transform

  def mark_structure_changed(self):
    self._mark("groupfx", "wires", "nodes")

  def mark_selection_changed(self):
    self._mark("groupfx", "wires", "nodes")

  def mark_overlay(self):
    self._mark("overlay")

  # -- gpu init --------------------------------------------------------------

  def _gpuInit(self):
    self.lyr_grid    = self.canvas.createLayer("grid")
    self.lyr_groupfx = self.canvas.createLayer("groupfx")
    self.lyr_wires   = self.canvas.createLayer("wires")
    self.lyr_nodes   = self.canvas.createLayer("nodes")
    self.lyr_flags   = self.canvas.createLayer("flags")   # screen-space node flags
    self.lyr_overlay = self.canvas.createLayer("overlay")
    self.pip      = self.canvas.pipelineSolid
    self.pip_vtx  = self.canvas.pipelineVtxSolid
    self.pip_tex  = self.canvas.pipelineTextured
    self._fonts = {fid: lev2.FontManager.fontForId(fid) for fid in FONT_ADV}
    self._gpu = True

  def gpuInit(self, ctx):
    """Build glyph + node-icon textures (crisp SVG). Call from the host's
    onGpuInit(ctx). Textures MUST be built here (GPU-init phase) — creating them
    lazily during the render callback aborts on the Vulkan backend."""
    self._ctx = ctx
    self._tex_eye = self._svg_texture(ctx, _SVG_EYE)
    self._tex_x = self._svg_texture(ctx, _SVG_X)
    self._prebuild_icons(self.nav_stack[0][0], set())

  def _prebuild_icons(self, model, seen):
    """Walk the model tree (root + reachable child models) and build/cache a
    texture for every distinct icon SVG, at GPU-init time."""
    if model is None or id(model) in seen:
      return
    seen.add(id(model))
    icon_fn = getattr(model, "icon", None)
    child_fn = getattr(model, "child_model", None)
    for nid in model.nodes():
      if icon_fn:
        svg = icon_fn(nid)
        if svg and svg not in self._icon_cache:
          self._icon_cache[svg] = self._svg_texture(self._ctx, svg)
      if child_fn:
        self._prebuild_icons(child_fn(nid), seen)

  def _svg_texture(self, ctx, svg, size=64):
    img = lev2.Image.fromSvgStringSquare(svg, size)
    tex = lev2.Texture("ne_glyph_%d" % (id(svg) & 0xffffff))
    ctx.TXI.updateTexture(tex, img, False)
    return tex

  def _icon_texture(self, svg):
    """Cached icon texture (prebuilt at gpuInit; never built during render)."""
    return self._icon_cache.get(svg) if svg else None

  def _node_base_color(self, nid):
    tc = gm.type_color(self.model.type_name(nid))
    base = COL_GROUP_BODY if self.model.is_group(nid) else COL_NODE_BODY
    base = _mix(base, tc, 0.16)
    chint = self.model.color(nid)
    return chint if chint is not None else base

  # -- view matrix -----------------------------------------------------------

  def _apply_view_matrix(self):
    s = self.view.s
    m = mtx4()
    m.setColumn(0, vec4(s, 0, 0, 0))
    m.setColumn(1, vec4(0, s, 0, 0))
    m.setColumn(2, vec4(0, 0, 1, 0))
    m.setColumn(3, vec4(self.view.ox, self.view.oy, 0, 1))
    self.lyr_groupfx.transform = m
    self.lyr_wires.transform = m
    self.lyr_nodes.transform = m

  # -- pre-render / dispatch -------------------------------------------------

  def _onPreRender(self):
    if not self._gpu:
      self._gpuInit()
    w, h = self.canvas.width, self.canvas.height
    if w < 2 or h < 2:
      return
    # expire the flag-mutation status strip (checked every frame, dirties overlay ONCE on
    # the active->cleared transition so an idle canvas still settles to 0 rebuilds/sec).
    if self._status is not None and time.monotonic() >= self._status_expiry:
      self._status = None
      self._dirty["overlay"] = True
    if (w, h) != (self._last_w, self._last_h):
      self._last_w, self._last_h = w, h
      if not self._did_initial_frame:
        self._ensure_layout()
        self.frame(sel_only=False)          # frame once, to the launch aspect
        self._did_initial_frame = True
      # subsequent resizes reposition screen-fixed text/grid but keep the graph
      # put (press F to re-frame) — avoids zoom jitter / churn on DPI settle
      self.mark_view_changed()
      self.mark_structure_changed()
      self.mark_overlay()

    if not any(self._dirty.values()):
      return

    self._apply_view_matrix()
    if self._dirty["grid"]:    self._rebuild_grid()
    if self._dirty["groupfx"]: self._rebuild_groupfx()
    if self._dirty["wires"]:   self._rebuild_wires()
    if self._dirty["nodes"]:   self._rebuild_nodes()
    if self._dirty["overlay"]: self._rebuild_overlay()
    for k in self._dirty:
      self._dirty[k] = False
    self.canvas.markDirty()

  # -- layout / geometry -----------------------------------------------------

  def _node_geom(self, model, nid):
    p = model.pos(nid)
    if p is None:
      return None
    kind = self._render_kind(model, nid)
    x, y, w, h = gm.node_rect(p, len(model.inputs(nid)), len(model.outputs(nid)),
                              kind, self.orientation)
    return (kind, x, y, w, h)

  def _node_rects(self):
    model = self.model
    out = []
    for nid in model.nodes():
      g = self._node_geom(model, nid)
      if g:
        _, x, y, w, h = g
        out.append((nid, x, y, w, h))
    return out

  def _port_geoms(self):
    """OUTPUT plug anchors per node, for picking (hover + drag-source detection). Input
    plugs are no longer interactive — a connection is made by dragging an output ONTO a
    node body, so inputs are neither hit-tested nor (when unconnected) drawn."""
    model = self.model
    geoms = []
    for nid in model.nodes():
      p = model.pos(nid)
      if p is None:
        continue
      _ins, outs = gm.port_anchors(p, len(model.inputs(nid)), len(model.outputs(nid)),
                                   self._render_kind(model, nid), self.orientation)
      geoms.append({"id": nid, "inputs": [], "outputs": outs})
    return geoms

  def _layout_positions(self):
    """Full layered-DAG layout of the CURRENT nesting level via the shared Sugiyama
    engine (ork.editor.graph_layout). Returns {nid: (x,y)} graph-space top-left
    positions. Node tile sizes match the canvas exactly (per render_kind), so the
    placement respects boundary pills. Imported lazily to keep ork.ui free of any
    import-time dependency on ork.editor."""
    from ork.editor import graph_layout
    model = self.model

    def _size_fn(nid, n_in, n_out):
      return gm.node_size(n_in, n_out, self._render_kind(model, nid), self.orientation)

    g = graph_layout.graph_from_model(model, orient=self.orientation, size_fn=_size_fn)
    return graph_layout.layout(g, orient=self.orientation)

  def _ensure_layout(self):
    model = self.model
    ids = list(model.nodes())
    if not any(model.pos(nid) is None for nid in ids):
      return
    pos = self._layout_positions()
    for nid in ids:
      if model.pos(nid) is None:
        x, y = pos.get(nid, (gm.LAYOUT_X0, gm.LAYOUT_Y0))
        model.set_pos(nid, x, y)

  def _capture_positions(self):
    """Model-level snapshot {nid: (x,y)} of every placed node at the CURRENT level.
    Restore (via set_pos) is byte-exact — this is the minimal position-undo primitive
    the Shift+A edit records so a single undo restores ALL prior positions."""
    model = self.model
    snap = {}
    for nid in model.nodes():
      p = model.pos(nid)
      if p is not None:
        snap[nid] = (p[0], p[1])
    return snap

  def _restore_positions(self, snap):
    model = self.model
    for nid, (x, y) in snap.items():
      model.set_pos(nid, x, y)
    self._save_layout()
    self.mark_structure_changed()

  def auto_layout(self):
    """Shift+A: re-lay-out the CURRENT nesting level with the shared layered-DAG engine
    and apply it as ONE undoable positions edit through the same persistence the drag
    path uses (set_pos + save_layout). A group interior (dived-in) lays out its own
    level. The pre/post snapshots are handed to on_layout_edit so a host records a single
    undo step; a single undo then restores every prior position byte-exact."""
    model = self.model
    ids = list(model.nodes())
    if not ids:
      return
    pre = self._capture_positions()
    pos = self._layout_positions()
    for nid in ids:
      xy = pos.get(nid)
      if xy is not None:
        model.set_pos(nid, xy[0], xy[1])
    self._save_layout()
    if self.on_layout_edit is not None:
      self.on_layout_edit(pre, self._capture_positions(), self._restore_positions)
    self.mark_structure_changed()

  def _bounds(self, ids):
    model = self.model
    minx = miny = 1e18
    maxx = maxy = -1e18
    found = False
    for nid in ids:
      g = self._node_geom(model, nid)
      if not g:
        continue
      _, x, y, w, h = g
      minx = min(minx, x); miny = min(miny, y)
      maxx = max(maxx, x + w); maxy = max(maxy, y + h)
      found = True
    return (minx, miny, maxx, maxy) if found else None

  def frame(self, sel_only):
    ids = list(self.sel_nodes) if (sel_only and self.sel_nodes) else list(self.model.nodes())
    b = self._bounds(ids)
    if b:
      self.view.frame_bounds(b[0], b[1], b[2], b[3], self._last_w, self._last_h)
      self.mark_view_changed()

  # -- layer rebuilds --------------------------------------------------------

  def _quad(self, layer, x, y, w, h, color, radius=0.0):
    qp = lev2.ui.QuadPrimitive(pipeline=self.pip)
    qd = lev2.ui.QuadData()
    qd.setPosition(x, y)
    qd.setSize(w, h)
    qd.setColor(color)
    if radius > 0:
      qd.setCornerRadius(radius)
    qp.addQuad(qd)
    layer.addPrimitive(qp)

  def _rebuild_grid(self):
    self.rebuild_count += 1
    self.lyr_grid.clear()
    w, h = self._last_w, self._last_h
    s = self.view.s
    step = gm.GRID_SPACING * s
    if step < 11.0:
      return
    qp = lev2.ui.QuadPrimitive(pipeline=self.pip)
    ix0 = int(math.floor(-self.view.ox / step))
    iy0 = int(math.floor(-self.view.oy / step))
    ny = int(h / step) + 2
    nx = int(w / step) + 2
    for jy in range(ny):
      gy = self.view.oy + (iy0 + jy) * step
      major_y = ((iy0 + jy) % 4 == 0)
      for jx in range(nx):
        gx = self.view.ox + (ix0 + jx) * step
        major = major_y and ((ix0 + jx) % 4 == 0)
        r = 1.6 if major else 1.1
        col = COL_GRID_MAJOR if major else COL_GRID_MINOR
        qd = lev2.ui.QuadData()
        qd.setPosition(gx - r, gy - r)
        qd.setSize(r * 2, r * 2)
        qd.setColor(col)
        qp.addQuad(qd)
    self.lyr_grid.addPrimitive(qp)

  def _rebuild_groupfx(self):
    self.rebuild_count += 1
    self.lyr_groupfx.clear()
    if not self.sel_nodes:
      return
    model = self.model
    pad = 4.0
    for nid in self.sel_nodes:
      g = self._node_geom(model, nid)
      if not g:
        continue
      _, x, y, w, hh = g
      self._quad(self.lyr_groupfx, x - pad, y - pad, w + pad * 2, hh + pad * 2,
                 COL_HALO, radius=NODE_RADIUS + pad)

  def _wire_endpoints(self, model, edge):
    si, sp, di, dp = edge
    if model.pos(si) is None or model.pos(di) is None:
      return None
    s_outs = model.outputs(si)
    d_ins = model.inputs(di)
    s_idx = next((k for k, (pn, _t) in enumerate(s_outs) if pn == sp), None)
    d_idx = next((k for k, (pn, _t) in enumerate(d_ins) if pn == dp), None)
    if s_idx is None or d_idx is None:
      return None
    _si, s_anchors = gm.port_anchors(model.pos(si), len(model.inputs(si)), len(s_outs),
                                     self._render_kind(model, si), self.orientation)
    d_anchors, _do = gm.port_anchors(model.pos(di), len(d_ins), len(model.outputs(di)),
                                     self._render_kind(model, di), self.orientation)
    if s_idx >= len(s_anchors) or d_idx >= len(d_anchors):
      return None
    return (s_anchors[s_idx], d_anchors[d_idx], s_outs[s_idx][1])

  def _rebuild_wires(self):
    self.rebuild_count += 1
    self.lyr_wires.clear()
    model = self.model
    prim = lev2.ui.TriListPrimitive(pipeline=self.pip_vtx)
    detach = self._detach_edge
    for edge in model.edges():
      if detach is not None and tuple(edge) == tuple(detach):
        continue                            # detach in flight: this end follows the cursor
      ep = self._wire_endpoints(model, edge)
      if not ep:
        continue
      src, dst, _stype = ep
      pts = gm.wire_points(src, dst, self.orientation, segments=22)
      selected = edge in self.sel_edges
      # a wire inherits the SOURCE node's identity color (the vivid type color its header
      # accent uses), slightly dimmed so wires read as connections without shouting over
      # the nodes. Generic for every family; selection still overrides to the highlight.
      color = COL_WIRE_SEL if selected else _c(gm.type_color(model.type_name(edge[0])), WIRE_ALPHA)
      thick = 3.2 if selected else 1.9
      _thick_polyline(prim, pts, color, thick)
    self.lyr_wires.addPrimitive(prim)

  def _rebuild_nodes(self):
    self.rebuild_count += 1
    self.lyr_nodes.clear()
    self.lyr_flags.clear()
    model = self.model
    fid = font_for_zoom(self.view.s)

    dot_prim = lev2.ui.TriListPrimitive(pipeline=self.pip_vtx)
    # screen-space flag buttons (constant, crisp) batched into the flags layer:
    # boxes as solid quads, glyphs as textured quads (SVG), tinted per-state.
    btn_prim = lev2.ui.QuadPrimitive(pipeline=self.pip)
    textured = self._tex_eye is not None and self._tex_x is not None
    if textured:
      eye_prim = lev2.ui.QuadPrimitive(pipeline=self.pip_tex, texture=self._tex_eye)
      x_prim = lev2.ui.QuadPrimitive(pipeline=self.pip_tex, texture=self._tex_x)
    glyph_fallback = lev2.ui.TriListPrimitive(pipeline=self.pip_vtx)
    dot_r = 3.4
    vert = (self.orientation == "vertical")
    show_flags = self.view.s >= 0.45      # hide the constant-size buttons far out
    # unconnected input plugs are not drawn (nor hit-tested): a connection is authored by
    # dragging an output onto a node body, so a bare input dot is dead affordance. Connected
    # inputs still draw their dot (the wire endpoint); outputs always draw (drag sources).
    connected_ins = {(e[2], e[3]) for e in model.edges()}
    for nid in model.nodes():
      g = self._node_geom(model, nid)
      if not g:
        continue
      kind, x, y, w, hh = g
      if kind in ("pill_in", "pill_out"):
        self._quad(self.lyr_nodes, x, y, w, hh, COL_PILL_BODY, radius=hh * 0.5)
      else:
        tc = gm.type_color(model.type_name(nid))
        is_group = model.is_group(nid)
        base = COL_GROUP_BODY if is_group else COL_NODE_BODY
        base = _mix(base, tc, 0.16)
        chint = model.color(nid)
        if chint is not None:
          base = chint
        bypassed = self._flag(nid, "bypass")
        display = self._flag(nid, "output")
        hollow = self._fill_style(nid) == "hollow"
        # groups: much larger corner radius + no header (differentiator)
        radius = (hh * 0.42) if is_group else NODE_RADIUS
        # display node (single, exclusive): rounded blue ring + faint glow (behind body)
        if display:
          self._quad(self.lyr_nodes, x - 6, y - 6, w + 12, hh + 12, COL_OUT_GLOW, radius=radius + 6)
          self._quad(self.lyr_nodes, x - 3, y - 3, w + 6,  hh + 6,  COL_OUT_RING, radius=radius + 3)
        # body (ghosted when bypassed = disabled)
        if bypassed:
          gb = _mix(base, (0.45, 0.46, 0.52), 0.55)
          body = vec4(gb.x, gb.y, gb.z, 0.22)
        else:
          body = base
        if hollow:
          # artifact-export sink: NO body fill (grid shows through) — just a thin outline
          # so the node still reads as a shape (hit-test rect is UNCHANGED). Keep the header.
          op = lev2.ui.TriListPrimitive(pipeline=self.pip_vtx)
          _rect_outline(op, x, y, w, hh, _brighten(base, 0.30), 1.4)
          self.lyr_nodes.addPrimitive(op)
        else:
          self._quad(self.lyr_nodes, x, y, w, hh, body, radius=radius)
        # type-color accent strip (header) — regular nodes only; groups have none
        if not is_group:
          strip = _c(tc, 0.16 if bypassed else 0.85)
          if vert:
            self._quad(self.lyr_nodes, x, y, w, FLAG_STRIP, strip, radius=NODE_RADIUS)
          else:
            self._quad(self.lyr_nodes, x, y, FLAG_STRIP, hh, strip, radius=NODE_RADIUS)
        # bypass: faint X across the node body (reads as "disabled")
        if bypassed:
          bpx = lev2.ui.TriListPrimitive(pipeline=self.pip_vtx)
          _thick_polyline(bpx, [(x + 7, y + 6), (x + w - 7, y + hh - 6)], COL_BYPASS_X, 1.3)
          _thick_polyline(bpx, [(x + w - 7, y + 6), (x + 7, y + hh - 6)], COL_BYPASS_X, 1.3)
          self.lyr_nodes.addPrimitive(bpx)
        # flag buttons: drawn in SCREEN space (constant, crisp, clamped size);
        # black interior + faint bright-body outline; glyph inside (eye=display,
        # x=bypass) faint when off, lit in its color when on. Groups have no
        # header so their buttons are centered vertically.
        if show_flags:
          outline = _brighten(base)
          orect, brect = self._flag_screen_rects(x, y, w, hh, has_header=not is_group)
          show_disp = self._flag_visible(nid, "output")
          show_byp  = self._flag_visible(nid, "bypass")
          ocol = COL_FLAG_OUTPUT if display else COL_GLYPH_OFF
          bcol = COL_FLAG_BYPASS if bypassed else COL_GLYPH_OFF
          # a flag whose capability query is False is neither drawn nor hit-tested.
          if show_disp:
            self._btn_box(btn_prim, orect, outline)
            (self._glyph_quad(eye_prim, orect, ocol) if textured
             else self._eye_glyph(glyph_fallback, orect, ocol))
          if show_byp:
            self._btn_box(btn_prim, brect, outline)
            (self._glyph_quad(x_prim, brect, bcol) if textured
             else self._x_glyph(glyph_fallback, brect, bcol))
      # plug dots (top=inputs / bottom=outputs in vertical)
      ins, outs = gm.port_anchors(model.pos(nid), len(model.inputs(nid)),
                                  len(model.outputs(nid)), kind, self.orientation)
      node_ins = model.inputs(nid)
      for k, (px, py) in enumerate(ins):
        if (nid, node_ins[k][0]) not in connected_ins:
          continue                          # unconnected input: no dot (drag-onto-body to wire)
        pt = node_ins[k][1]
        _add_circle(dot_prim, px, py, dot_r, _c(gm.type_color(pt)), 10)
      for k, (px, py) in enumerate(outs):
        pt = model.outputs(nid)[k][1]
        _add_circle(dot_prim, px, py, dot_r, _c(gm.type_color(pt)), 10)
    self.lyr_nodes.addPrimitive(dot_prim)
    self.lyr_flags.addPrimitive(btn_prim)
    if textured:
      self.lyr_flags.addPrimitive(eye_prim)
      self.lyr_flags.addPrimitive(x_prim)
    else:
      self.lyr_flags.addPrimitive(glyph_fallback)

    # screen-space labels: node NAME only (type/details are shown on hover).
    if fid is None:
      return
    adv = FONT_ADV[fid]
    fh = FONT_H[fid]
    font = self._fonts[fid]
    tp_name = lev2.ui.TextPrimitive(font=font, color=COL_TEXT_NAME)
    tp_dim = lev2.ui.TextPrimitive(font=font, color=COL_TEXT_DIM)
    icon_fn = getattr(model, "icon", None)
    show_icon = self._tex_eye is not None and self.view.s >= 0.60   # hide far out
    icon_prims = {}                        # texture -> QuadPrimitive (batched)
    for nid in model.nodes():
      g = self._node_geom(model, nid)
      if not g:
        continue
      kind, x, y, w, hh = g
      name = model.name(nid)
      pill = kind in ("pill_in", "pill_out")
      # Center in SCREEN space: the text is screen-fixed at a constant pixel
      # size, so the half-width/half-height centering offset must be applied in
      # screen pixels — NOT graph units (which get scaled by zoom and cause the
      # text to drift off-center as you zoom). Round to whole pixels for crispness.
      scx, scy = self.view.graph_to_screen(x + w * 0.5, y + hh * 0.5)
      # optional icon centered ABOVE the label (icon+label centered as a unit)
      icon_tex = None
      if show_icon and icon_fn and not pill:
        icon_tex = self._icon_texture(icon_fn(nid))
      if icon_tex is not None:
        y1s = self.view.graph_to_screen(x, y + hh)[1]
        y0s = self.view.graph_to_screen(x, y)[1]
        isz = gm.clamp((y1s - y0s) * 0.34, 12.0, 40.0)
        gap = 3.0
        top = scy - (isz + gap + fh) * 0.5
        prim = icon_prims.get(icon_tex)
        if prim is None:
          prim = lev2.ui.QuadPrimitive(pipeline=self.pip_tex, texture=icon_tex)
          icon_prims[icon_tex] = prim
        _add_quad_tex(prim, round(scx - isz * 0.5), round(top), isz, isz,
                      _brighten(self._node_base_color(nid), 0.55))
        label_y = top + isz + gap
      else:
        label_y = scy - fh * 0.5
      sx = float(round(scx - len(name) * adv * 0.5))
      sy = float(round(label_y))
      target = tp_dim if (not pill and self._flag(nid, "bypass")) else tp_name
      target.addItem(name, vec2(sx, sy))
    self.lyr_nodes.addPrimitive(tp_dim)
    self.lyr_nodes.addPrimitive(tp_name)
    for prim in icon_prims.values():
      self.lyr_flags.addPrimitive(prim)

  # -- node-body flags (optional model hooks) --------------------------------

  def _flag(self, nid, which):
    getter = getattr(self.model, "is_output" if which == "output" else "is_bypassed", None)
    return bool(getter(nid)) if getter else False

  def _flag_visible(self, nid, which):
    # a flag whose capability query is False is never drawn NOR hit-tested (the flag can
    # never apply — e.g. the display flag on a capture). Defaults True (toy/legacy models).
    fn = getattr(self.model, "has_display_flag" if which == "output" else "has_bypass_flag", None)
    return bool(fn(nid)) if fn else True

  def _fill_style(self, nid):
    fn = getattr(self.model, "fill_style", None)
    return fn(nid) if fn else "solid"

  def _flag_screen_rects(self, x, y, w, hh, has_header=True):
    """Flag-button rects in SCREEN space at the left/right edges: display (eye)
    right, bypass (x) left. Returns (display_rect, bypass_rect). Buttons are top-
    aligned below the header, or vertically CENTERED when there is no header
    (group nodes). Size grows a little with zoom then clamps, and is capped to
    the node box so the two never overlap/cross the border when minifying."""
    x0, y0 = self.view.graph_to_screen(x, y)
    x1, y1 = self.view.graph_to_screen(x + w, y + hh)
    node_w = x1 - x0
    node_h = y1 - y0
    side = max(5.0, node_w * 0.055)                # inset from left/right edge
    gap = 6.0
    base = gm.clamp(13.0 * self.view.s, 17.0, 30.0)
    fit_w = (node_w - side * 2.0 - gap) * 0.5      # two side-by-side fit
    if has_header:
      header = FLAG_STRIP * self.view.s
      top = header + max(4.0, node_h * 0.08)
      bot = max(5.0, node_h * 0.08)
      bp = max(4.0, min(base, fit_w, node_h - top - bot))
      ty = y0 + top
    else:
      margin = max(4.0, node_h * 0.14)
      bp = max(4.0, min(base, fit_w, node_h - margin * 2.0))
      ty = (y0 + y1) * 0.5 - bp * 0.5              # vertically centered
    display_rect = (x1 - side - bp, ty, bp, bp)
    bypass_rect  = (x0 + side,      ty, bp, bp)
    return (display_rect, bypass_rect)

  def _btn_box(self, qprim, r, outline):
    """A flag-button box: thin bright outline + black interior."""
    x, y, w, h = r
    inset = max(1.0, w * 0.055)
    _add_quad(qprim, x, y, w, h, outline, radius=3.0)
    _add_quad(qprim, x + inset, y + inset, w - inset * 2, h - inset * 2, COL_BTN_INNER, radius=2.5)

  def _glyph_quad(self, qprim, r, color):
    """A textured glyph quad (white SVG tinted by `color`) filling the button."""
    x, y, w, h = r
    ins = w * 0.05
    qd = lev2.ui.QuadData()
    qd.setPosition(x + ins, y + ins)
    qd.setSize(w - ins * 2, h - ins * 2)
    qd.setColor(color)
    qd.setUV(0, 1, 1, 0)
    qprim.addQuad(qd)

  def _x_glyph(self, prim, r, color):
    x, y, w, h = r
    m = w * 0.32
    t = max(1.5, w * 0.11)
    _thick_polyline(prim, [(x + m, y + m), (x + w - m, y + h - m)], color, t)
    _thick_polyline(prim, [(x + w - m, y + m), (x + m, y + h - m)], color, t)

  def _eye_glyph(self, prim, r, color):
    x, y, w, h = r
    cx, cy = x + w * 0.5, y + h * 0.5
    rw, rh = w * 0.34, h * 0.22
    t = max(1.4, w * 0.09)
    n = 18
    pts = [(cx + math.cos(2 * math.pi * i / n) * rw,
            cy + math.sin(2 * math.pi * i / n) * rh) for i in range(n + 1)]
    _thick_polyline(prim, pts, color, t)              # lens outline
    _add_circle(prim, cx, cy, w * 0.15, color, 12)    # pupil

  def _toggle_bypass(self, nid):
    setter = getattr(self.model, "set_bypassed", None)
    if setter:
      # the badge reads model state, so a REFUSED mutation (no state change) redraws the
      # unchanged badge automatically — we only add the visible status text (Fix 3).
      self._apply_flag_result(setter(nid, not self._flag(nid, "bypass")))
      self.mark_structure_changed()

  def _set_display(self, nid):
    # Houdini semantics: exactly one display node — clicking sets it exclusively.
    setter = getattr(self.model, "set_output", None)
    if setter:
      self._apply_flag_result(setter(nid, True))
      self.mark_structure_changed()

  # -- flag-mutation feedback (Fix 3) ----------------------------------------

  def _apply_flag_result(self, result):
    """A flag setter returns None on success (badge change IS the feedback) or a
    (kind, message) pair for a status the canvas surfaces in a colour-coded strip."""
    if not result:
      return
    kind, message = result
    color = COL_STATUS_NOOP if kind == "noop" else COL_STATUS_BAD
    self.show_status(message, color)

  def show_status(self, text, color=None):
    """Post a short-lived status message (host also calls this for rebuild failures)."""
    self._status = (str(text), color if color is not None else COL_STATUS_BAD)
    self._status_expiry = time.monotonic() + STATUS_TTL_S
    self.mark_overlay()

  def _plug_info(self, nid, side, idx):
    """(plug_name, type_name, (gx,gy)) for a plug, or None."""
    model = self.model
    p = model.pos(nid)
    if p is None:
      return None
    ins, outs = gm.port_anchors(p, len(model.inputs(nid)), len(model.outputs(nid)),
                                self._render_kind(model, nid), self.orientation)
    ports = model.inputs(nid) if side == "in" else model.outputs(nid)
    anchors = ins if side == "in" else outs
    if idx >= len(ports) or idx >= len(anchors):
      return None
    return (ports[idx][0], ports[idx][1], anchors[idx])

  def _tooltip(self, layer, text, gx, gy, color, above):
    """Small screen-fixed label near a graph-space anchor."""
    font = self._fonts["i14"]
    adv = FONT_ADV["i14"]
    sx, sy = self.view.graph_to_screen(gx, gy)
    tw = len(text) * adv + 10
    th = FONT_H["i14"] + 6
    bx = sx - tw * 0.5
    by = sy - th - 10 if above else sy + 10
    self._quad(layer, bx, by, tw, th, COL_TOOLTIP_BG, radius=3.0)
    tp = lev2.ui.TextPrimitive(font=font, color=color)
    tp.addItem(text, vec2(bx + 5, by + 3))
    layer.addPrimitive(tp)

  def _rebuild_overlay(self):
    self.rebuild_count += 1
    self.lyr_overlay.clear()
    w = self._last_w
    font = self._fonts["i14"]
    adv = FONT_ADV["i14"]
    vert = (self.orientation == "vertical")

    # path bar
    self._quad(self.lyr_overlay, 0, 0, w, PATHBAR_H, COL_PATHBAR_BG)
    self._crumb_regions = []
    tp = lev2.ui.TextPrimitive(font=font, color=COL_CRUMB)
    tp_last = lev2.ui.TextPrimitive(font=font, color=COL_CRUMB_LAST)
    cx = 10.0
    n = len(self.nav_stack)
    for i, (_m, label) in enumerate(self.nav_stack):
      wpx = len(label) * adv
      is_last = (i == n - 1)
      (tp_last if is_last else tp).addItem(label, vec2(cx, 5.0))
      self._crumb_regions.append((cx - 4, 0.0, wpx + 8, PATHBAR_H, i))
      cx += wpx + 6
      if not is_last:
        tp.addItem("/", vec2(cx, 5.0))
        cx += adv + 6
    self.lyr_overlay.addPrimitive(tp)
    self.lyr_overlay.addPrimitive(tp_last)

    # snap on/off indicator, top-right of the path bar (clickable)
    snap_txt = "SNAP"
    sw = len(snap_txt) * adv + 14
    sx = w - sw - 8
    sy = 3.0
    sh = PATHBAR_H - 6
    on = self.snap
    self._quad(self.lyr_overlay, sx, sy, sw, sh,
               COL_SNAP_ON if on else COL_SNAP_OFF, radius=3.0)
    self._snap_region = (sx, 0.0, sw + 8, PATHBAR_H)
    stp = lev2.ui.TextPrimitive(font=font, color=(COL_CRUMB_LAST if on else COL_TEXT_DIM))
    stp.addItem(snap_txt, vec2(sx + 7, 5.0))
    self.lyr_overlay.addPrimitive(stp)

    # flag-mutation status strip: a colour-coded banner just BELOW the path bar (Fix 3).
    if self._status is not None:
      msg, scol = self._status
      strip_h = FONT_H["i14"] + 8
      self._quad(self.lyr_overlay, 0, PATHBAR_H, w, strip_h, COL_STATUS_BG)
      self._quad(self.lyr_overlay, 0, PATHBAR_H, 4.0, strip_h, scol)   # colour tab
      sp = lev2.ui.TextPrimitive(font=font, color=scol)
      sp.addItem(msg, vec2(12.0, PATHBAR_H + 4.0))
      self.lyr_overlay.addPrimitive(sp)

    # marquee rect
    if self.mode == "marquee" and self._marquee:
      gx0, gy0, gx1, gy1 = self._marquee
      sx0, sy0 = self.view.graph_to_screen(min(gx0, gx1), min(gy0, gy1))
      sx1, sy1 = self.view.graph_to_screen(max(gx0, gx1), max(gy0, gy1))
      self._quad(self.lyr_overlay, sx0, sy0, sx1 - sx0, sy1 - sy0, COL_MARQUEE_FIL)
      mp = lev2.ui.TriListPrimitive(pipeline=self.pip_vtx)
      _rect_outline(mp, sx0, sy0, sx1 - sx0, sy1 - sy0, COL_MARQUEE, 1.4)
      self.lyr_overlay.addPrimitive(mp)

    # drag-to-node connect preview: outline every qualifying drop-target node, wash + brighten
    # the hovered one, and trail a wire from the source output to the cursor.
    if self.mode == "port_drag" and self._port_src and self._port_cursor:
      _sid, _oi, src_g = self._port_src
      s_scr = self.view.graph_to_screen(*src_g)
      c_scr = self.view.graph_to_screen(*self._port_cursor)
      halo = lev2.ui.TriListPrimitive(pipeline=self.pip_vtx)
      for tnid in self._drop_targets:
        g = self._node_geom(self.model, tnid)
        if not g:
          continue
        _k, nx, ny, nw, nh = g
        sx0, sy0 = self.view.graph_to_screen(nx, ny)
        sx1, sy1 = self.view.graph_to_screen(nx + nw, ny + nh)
        hovered = (tnid == self._port_node)
        if hovered:
          self._quad(self.lyr_overlay, sx0, sy0, sx1 - sx0, sy1 - sy0, COL_DROP_HOV,
                     radius=NODE_RADIUS)
        _rect_outline(halo, sx0, sy0, sx1 - sx0, sy1 - sy0, COL_DROP_TGT,
                      2.4 if hovered else 1.4)
      self.lyr_overlay.addPrimitive(halo)
      pts = gm.wire_points(s_scr, c_scr, self.orientation, segments=22)
      color = COL_OK if self._port_ok else COL_BAD
      wp = lev2.ui.TriListPrimitive(pipeline=self.pip_vtx)
      _thick_polyline(wp, pts, color, 2.4)
      self.lyr_overlay.addPrimitive(wp)

    # hover details (only when not dragging): plug label or node details
    elif self._hover is not None:
      if self._hover[0] == "plug":
        _, nid, side, idx = self._hover
        info = self._plug_info(nid, side, idx)
        if info:
          pn, tn, anchor = info
          above = (side == "in") if vert else False
          self._tooltip(self.lyr_overlay, f"{pn}:{tn}", anchor[0], anchor[1],
                        COL_TOOLTIP_TX, above=above)
      elif self._hover[0] == "node":
        nid = self._hover[1]
        g = self._node_geom(self.model, nid)
        if g:
          _k, x, y, w, hh = g
          detail = f"{self.model.name(nid)}  ({self.model.type_name(nid)})"
          if self._flag(nid, "bypass"):
            detail += "  [bypass]"
          if self._flag(nid, "output"):
            detail += "  [output]"
          self._tooltip(self.lyr_overlay, detail, x + w * 0.5, y, COL_TOOLTIP_TX, above=True)
      elif self._hover[0] == "edge":
        edge = self._hover[1]
        ep = self._wire_endpoints(self.model, edge)
        if ep:
          src_a, dst_a, stype = ep
          si, sp, di, dp = edge
          txt = f"{stype}   {self.model.name(si)}.{sp} -> {self.model.name(di)}.{dp}"
          mid = gm.wire_points(src_a, dst_a, self.orientation, segments=12)[6]
          self._tooltip(self.lyr_overlay, txt, mid[0], mid[1], _c(gm.type_color(stype)), above=True)

  # -- picking ---------------------------------------------------------------

  def _pick_wire(self, gx, gy):
    model = self.model
    best = None
    best_d = (WIRE_HIT_PX / self.view.s) ** 2
    for edge in model.edges():
      ep = self._wire_endpoints(model, edge)
      if not ep:
        continue
      pts = gm.wire_points(ep[0], ep[1], self.orientation, segments=22)
      d2 = gm.polyline_dist2(gx, gy, pts)
      if d2 <= best_d:
        best_d = d2
        best = edge
    return best

  def _pick_connected_input(self, gx, gy):
    """Nearest CONNECTED input plug within the plug hit radius, with its incoming edge —
    the grab point for a detach drag (pull the wire's dest end off). Unconnected inputs are
    dead affordance (drag-onto-body to wire), so only wired inputs are hit-tested here.
    Returns (nid, in_index, edge) or None."""
    model = self.model
    incoming = {(e[2], e[3]): e for e in model.edges()}
    r = gm.PORT_HIT_PX / self.view.s
    best = None
    best_d2 = r * r
    for nid in model.nodes():
      p = model.pos(nid)
      if p is None:
        continue
      ins, _outs = gm.port_anchors(p, len(model.inputs(nid)), len(model.outputs(nid)),
                                   self._render_kind(model, nid), self.orientation)
      node_ins = model.inputs(nid)
      for idx, (ax, ay) in enumerate(ins):
        edge = incoming.get((nid, node_ins[idx][0]))
        if edge is None:
          continue
        d2 = (ax - gx) ** 2 + (ay - gy) ** 2
        if d2 <= best_d2:
          best_d2 = d2
          best = (nid, idx, edge)
    return best

  # -- event handling --------------------------------------------------------

  def _onUiEvent(self, ev):
    code = ev.code
    mx, my = self.canvas.rootToLocal(ev.x, ev.y)
    self._last_root = (ev.x, ev.y)

    if code == tokens.PUSH.hashed:
      self._pointer_inside = True
      self._on_push(mx, my, ev)
    elif code == tokens.DOUBLECLICK.hashed:
      self._on_doubleclick(mx, my)
    elif code == tokens.DRAG.hashed:
      self._on_drag(mx, my)
    elif code == tokens.RELEASE.hashed:
      self._on_release(mx, my, ev)
    elif code == tokens.MOVE.hashed:
      self._pointer_inside = True
      self._on_move(mx, my)
    elif code == tokens.MOUSEWHEEL.hashed:
      self._on_wheel(mx, my, ev)
    elif code == tokens.MOUSE_LEAVE.hashed:
      self._pointer_inside = False
      if self._hover is not None:
        self._hover = None
        self.mark_overlay()
    self._emit_selection()
    return lev2.ui.HandlerResult()

  def _on_move(self, mx, my):
    # x-held (synthetic middle) OR real middle: bare MOVE pans
    if self.mode == "pan":
      self._pan_to(mx, my)
      return
    # z-held (synthetic left): bare MOVE continues the drag interaction
    if self.mode in ("drag_nodes", "marquee", "port_drag") and self._mb["left"]:
      self._on_drag(mx, my)
      return
    self._last_mx, self._last_my = mx, my
    self._update_hover(mx, my)

  def _pan_to(self, mx, my):
    self.view.pan_screen(mx - self._last_mx, my - self._last_my)
    self._last_mx, self._last_my = mx, my
    self.mark_view_changed()

  def _update_hover(self, mx, my):
    gx, gy = self.view.screen_to_graph(mx, my)
    plug = gm.pick_port(gx, gy, self._port_geoms(), HOVER_PLUG_PX / self.view.s)
    if plug is not None:
      h = ("plug",) + plug
    else:
      nid = gm.hit_test_node(gx, gy, self._node_rects())
      if nid is not None:
        h = ("node", nid)
      else:
        edge = self._pick_wire(gx, gy)
        h = ("edge", edge) if edge is not None else None
    if h != self._hover:                    # rebuild only when hover identity changes
      self._hover = h
      self.mark_overlay()

  def _on_wheel(self, mx, my, ev):
    # Wheel is zoom-ONLY, anchored at the cursor (zoom-to-cursor). Panning is
    # middle-mouse / x-held drag, never the wheel.
    if ev.wheel_y != 0:
      self.view.zoom_at(mx, my, math.pow(1.0015, ev.wheel_y))
      self.mark_view_changed()

  def _on_push(self, mx, my, ev):
    if my <= PATHBAR_H:
      if self._snap_region:
        sx, sy, sw, sh = self._snap_region
        if sx <= mx <= sx + sw and sy <= my <= sy + sh:
          self.snap = not self.snap
          self.mark_overlay()
          return
      for (rx, ry, rw, rh, level) in self._crumb_regions:
        if rx <= mx <= rx + rw and ry <= my <= ry + rh:
          self._navigate_to_level(level)
          return
    if ev.middle:
      self.mode = "pan"
      self._last_mx, self._last_my = mx, my
      return
    gx, gy = self.view.screen_to_graph(mx, my)
    self._last_gx, self._last_gy = gx, gy
    self._last_mx, self._last_my = mx, my
    # output plug -> start a drag-to-node connect. Qualifying drop targets (nodes with >=1
    # type-compatible input for THIS output) are computed once now — the source is fixed for
    # the drag; the drop then chooses the input plug (fast-path or dropdown) on release.
    port = gm.pick_port(gx, gy, self._port_geoms(), gm.PORT_HIT_PX / self.view.s)
    if port and port[1] == "out":
      sid, _side, oidx = port
      _oi, s_anchors = gm.port_anchors(self.model.pos(sid), len(self.model.inputs(sid)),
                                       len(self.model.outputs(sid)),
                                       self._render_kind(self.model, sid), self.orientation)
      sp = self.model.outputs(sid)[oidx][0]
      self.mode = "port_drag"
      self._port_src = (sid, oidx, s_anchors[oidx])
      self._port_cursor = (gx, gy)
      self._drop_targets = self._compute_drop_targets(sid, sp)
      self._port_node = None
      self._port_ok = False
      self._detach_edge = None
      self._hover = None
      self.mark_overlay()
      return
    # connected input plug -> start a DETACH drag: grab the wire's dest end and pull it off.
    # The port_drag is seeded from the wire's SOURCE output (the fixed end), so release re-uses
    # the connect flow — empty canvas disconnects, another node re-routes. Priority over the
    # node body (a precise, small plug target) but not over an output drag.
    det = self._pick_connected_input(gx, gy)
    if det is not None:
      _dnid, _didx, edge = det
      esi, esp, _edi, _edp = edge
      s_oidx = next((k for k, (pn, _t) in enumerate(self.model.outputs(esi)) if pn == esp), None)
      if s_oidx is not None:
        _oi, s_anchors = gm.port_anchors(self.model.pos(esi), len(self.model.inputs(esi)),
                                         len(self.model.outputs(esi)),
                                         self._render_kind(self.model, esi), self.orientation)
        self.mode = "port_drag"
        self._port_src = (esi, s_oidx, s_anchors[s_oidx])
        self._port_cursor = (gx, gy)
        self._drop_targets = self._compute_drop_targets(esi, esp)
        self._port_node = None
        self._port_ok = False
        self._detach_edge = edge
        self._hover = None
        self.mark_structure_changed()       # hide the grabbed wire (it now follows the cursor)
        self.mark_overlay()
        return
    # node-body flags (output/bypass) — hit-tested in SCREEN space (mx,my),
    # taking priority over select/drag. Buttons only exist when shown.
    if self.view.s >= 0.45:
      for (nid, nx, ny, nw, nh) in self._node_rects():
        if self._render_kind(self.model, nid) in ("pill_in", "pill_out"):
          continue
        orect, brect = self._flag_screen_rects(nx, ny, nw, nh,
                                               has_header=not self.model.is_group(nid))
        if (self._flag_visible(nid, "output")
            and orect[0] <= mx <= orect[0] + orect[2] and orect[1] <= my <= orect[1] + orect[3]):
          self._set_display(nid)
          return
        if (self._flag_visible(nid, "bypass")
            and brect[0] <= mx <= brect[0] + brect[2] and brect[1] <= my <= brect[1] + brect[3]):
          self._toggle_bypass(nid)
          return
    # node
    nid = gm.hit_test_node(gx, gy, self._node_rects())
    if nid is not None:
      if ev.shift:
        self.sel_nodes.discard(nid) if nid in self.sel_nodes else self.sel_nodes.add(nid)
      else:
        if nid not in self.sel_nodes:
          self.sel_nodes = {nid}
          self.sel_edges.clear()
      self.mode = "drag_nodes"
      # snapshot positions for a coherent (optionally snapped) group move
      self._drag_ref = nid
      self._drag_start_g = (gx, gy)
      self._drag_orig = {n: self.model.pos(n) for n in self.sel_nodes
                         if self.model.pos(n) is not None}
      self.mark_selection_changed()
      return
    # wire
    edge = self._pick_wire(gx, gy)
    if edge is not None:
      if ev.shift:
        self.sel_edges.discard(edge) if edge in self.sel_edges else self.sel_edges.add(edge)
      else:
        self.sel_edges = {edge}
        self.sel_nodes.clear()
      self.mark_selection_changed()
      return
    # empty -> marquee
    if not ev.shift:
      self.sel_nodes.clear()
      self.sel_edges.clear()
      self.mark_selection_changed()
    self.mode = "marquee"
    self._marquee = (gx, gy, gx, gy)
    self.mark_overlay()

  def _on_doubleclick(self, mx, my):
    gx, gy = self.view.screen_to_graph(mx, my)
    nid = gm.hit_test_node(gx, gy, self._node_rects())
    if nid is not None and self.model.is_group(nid):
      self._enter_group(nid)

  def _on_drag(self, mx, my):
    if self.mode == "pan":
      self.view.pan_screen(mx - self._last_mx, my - self._last_my)
      self._last_mx, self._last_my = mx, my
      self.mark_view_changed()
      return
    gx, gy = self.view.screen_to_graph(mx, my)
    if self.mode == "drag_nodes":
      model = self.model
      dgx = gx - self._drag_start_g[0]
      dgy = gy - self._drag_start_g[1]
      # grid snap: snap the grabbed node's CENTER to the nearest grid dot and
      # shift the whole selection by that same correction (preserves layout).
      if self.snap and self._drag_ref in self._drag_orig:
        g = gm.GRID_SPACING
        rx, ry = self._drag_orig[self._drag_ref]
        nw, nh = gm.node_size(len(model.inputs(self._drag_ref)),
                              len(model.outputs(self._drag_ref)),
                              self._render_kind(model, self._drag_ref), self.orientation)
        cx = rx + dgx + nw * 0.5
        cy = ry + dgy + nh * 0.5
        dgx += round(cx / g) * g - cx
        dgy += round(cy / g) * g - cy
      for nid, (ox, oy) in self._drag_orig.items():
        model.set_pos(nid, ox + dgx, oy + dgy)
      self.mark_structure_changed()
    elif self.mode == "marquee":
      self._marquee = (self._marquee[0], self._marquee[1], gx, gy)
      self.mark_overlay()
    elif self.mode == "port_drag":
      self._port_cursor = (gx, gy)
      self._update_port_node(gx, gy)
      self.mark_overlay()

  def _compute_drop_targets(self, sid, sp):
    """{nid: [(in_index, plug_name, existing_edge_or_None)]} for every node that has at least
    one input plug the dragged output (sid.sp) can feed. Qualifying = can_connect True (the
    engine's strict type + fan-out verdict), which INCLUDES already-connected inputs (selecting
    one replaces its edge); the source node itself is excluded by can_connect's self-wire guard."""
    model = self.model
    incoming = {}                             # (dst_id, dst_plug) -> full edge tuple
    for e in model.edges():
      incoming[(e[2], e[3])] = e
    targets = {}
    for nid in model.nodes():
      cands = []
      for didx, (dp, _t) in enumerate(model.inputs(nid)):
        ok, _reason = model.can_connect(sid, sp, nid, dp)
        if ok:
          cands.append((didx, dp, incoming.get((nid, dp))))
      if cands:
        targets[nid] = cands
    return targets

  def _update_port_node(self, gx, gy):
    """Track which drop-target node the cursor is over (a node body, not a plug). Only nodes
    in _drop_targets qualify — a node with zero compatible inputs never highlights."""
    nid = gm.hit_test_node(gx, gy, self._node_rects())
    self._port_node = nid if (nid in self._drop_targets) else None
    self._port_ok = self._port_node is not None

  def _on_release(self, mx, my, ev):
    mode = self.mode
    self.mode = None
    if mode == "drag_nodes":
      self._maybe_splice_on_wire()
      self._save_layout()
      self.mark_structure_changed()
    elif mode == "marquee":
      self._finish_marquee(ev.shift)
      self._marquee = None
      self.mark_selection_changed()
      self.mark_overlay()
    elif mode == "port_drag":
      self._finish_port_drag(mx, my)

  def _finish_port_drag(self, mx, my):
    """Release over a qualifying node body -> connect. One candidate: connect immediately
    (single-candidate fast path, owner-ratified). Several: open a DropdownMenu of the input
    plugs at the cursor (already-connected entries marked "(replaces ...)"). Over empty canvas
    or a node with zero compatible inputs: clean cancel (no menu, no edge).

    Detach variant (_detach_edge set): the grabbed wire's dest end is lifted off first —
    empty / unqualified release disconnects it (plain disconnect), a qualifying node re-routes
    the wire to that node's chosen input (one-gesture re-route)."""
    src = self._port_src
    detach = self._detach_edge
    gx, gy = self.view.screen_to_graph(mx, my)
    nid = gm.hit_test_node(gx, gy, self._node_rects())
    cands = self._drop_targets.get(nid) if nid is not None else None   # capture BEFORE teardown
    # tear down the in-flight drag preview BEFORE any connect/menu (the menu is a separate
    # ui-context overlay; the canvas preview must not linger under it).
    self._port_src = None
    self._port_cursor = None
    self._drop_targets = {}
    self._port_node = None
    self._port_ok = False
    self._detach_edge = None
    self.mark_overlay()
    if src is None:
      return
    sid, oidx, _sg = src
    sp = self.model.outputs(sid)[oidx][0]
    if detach is not None:
      # lift the grabbed wire off (grab-the-plug-end disconnect), THEN optionally re-route.
      self.model.disconnect(*detach)
      # candidates are re-derived on the POST-disconnect graph so their existing-edge refs
      # are current (the freed input is now an unconnected candidate, no stale double-remove).
      cands = self._compute_drop_targets(sid, sp).get(nid) if nid is not None else None
      if not cands:
        self.mark_structure_changed()         # pure disconnect (empty / unqualified drop)
        return
      if len(cands) == 1:
        self._commit_connect(sid, sp, nid, cands[0])
      else:
        self._show_connect_menu(sid, sp, nid, cands)
      return
    if not cands:
      return                                  # empty canvas / zero-qualifying: clean cancel
    if len(cands) == 1:
      self._commit_connect(sid, sp, nid, cands[0])
    else:
      self._show_connect_menu(sid, sp, nid, cands)

  def _commit_connect(self, sid, sp, nid, cand):
    """Wire sid.sp -> nid.<plug>. If the target input already has an incoming edge, REPLACE it
    through the document's disconnect+connect path (matches the splice-on-wire idiom)."""
    _didx, dp, existing = cand
    if existing is not None:
      esi, esp, edi, edp = existing
      self.model.disconnect(esi, esp, edi, edp)
    self.model.connect(sid, sp, nid, dp)

  def _show_connect_menu(self, sid, sp, nid, cands):
    """DropdownMenu of the qualifying input plugs at the last cursor position. A connected
    input is labelled "<plug>  (replaces <src>.<plug>)". Selecting commits the connect;
    click-outside / escape dismisses with no edit (the overlay's own cancel path)."""
    if self.uicontext is None:
      self.show_status("cannot open connect menu (no ui context)")
      return
    by_label = {}
    paths = []
    for cand in cands:
      _didx, dp, existing = cand
      label = dp
      if existing is not None:
        esi, esp, _edi, _edp = existing
        label = f"{dp}  (replaces {esi}.{esp})"
      paths.append("/" + label)
      by_label[label] = cand
    rx, ry = self._last_root

    def on_selected(val):
      cand = by_label.get(val.lstrip("/"))
      if cand is not None:
        self._commit_connect(sid, sp, nid, cand)

    lev2.ui.DropdownMenu.show(
        context=self.uicontext,
        paths=paths,
        x=rx, y=ry,
        on_selected=on_selected,
        sort_alphabetically=False)

  def _finish_marquee(self, additive):
    if not self._marquee:
      return
    gx0, gy0, gx1, gy1 = self._marquee
    xlo, xhi = min(gx0, gx1), max(gx0, gx1)
    ylo, yhi = min(gy0, gy1), max(gy0, gy1)
    if not additive:
      self.sel_nodes = set()
    for (nid, x, y, w, h) in self._node_rects():
      if x + w >= xlo and x <= xhi and y + h >= ylo and y <= yhi:
        self.sel_nodes.add(nid)

  def _maybe_splice_on_wire(self):
    if len(self.sel_nodes) != 1:
      return
    nid = next(iter(self.sel_nodes))
    model = self.model
    ins = model.inputs(nid)
    outs = model.outputs(nid)
    if not ins or not outs:
      return
    g = self._node_geom(model, nid)
    if not g:
      return
    _, x, y, w, h = g
    edge = self._pick_wire(x + w * 0.5, y + h * 0.5)
    if edge is None:
      return
    si, sp, di, dp = edge
    if si == nid or di == nid:
      return
    in_name = ins[0][0]
    out_name = outs[0][0]
    ok1, _ = model.can_connect(si, sp, nid, in_name)
    ok2, _ = model.can_connect(nid, out_name, di, dp)
    if ok1 and ok2:
      model.disconnect(si, sp, di, dp)
      model.connect(si, sp, nid, in_name)
      model.connect(nid, out_name, di, dp)

  def _save_layout(self):
    fn = getattr(self.model, "save_layout", None)
    if fn:
      fn()

  # -- navigation ------------------------------------------------------------

  def _enter_group(self, nid):
    child = self.model.child_model(nid)
    if child is None:
      return
    self.nav_stack.append((child, self.model.name(nid)))
    self._bind_model(child)
    self._enter_level_reset()

  def _navigate_to_level(self, level):
    if level < 0 or level >= len(self.nav_stack) - 1:
      return
    self.nav_stack = self.nav_stack[:level + 1]
    self._enter_level_reset()

  def up_level(self):
    if len(self.nav_stack) > 1:
      self.nav_stack.pop()
      self._enter_level_reset()

  def _enter_level_reset(self):
    self.sel_nodes.clear()
    self.sel_edges.clear()
    self.mode = None
    self._hover = None
    self._ensure_layout()
    self.frame(sel_only=False)
    self.mark_structure_changed()
    self.mark_overlay()

  # -- keyboard (host forwards KEY_DOWN and KEY_UP here) ---------------------
  #
  # orkid 3-button-mouse emulation for trackpads: z/x/c act as the left/middle/
  # right buttons. "Hold x and move" pans (no drag/click needed).

  def handleKeyDown(self, ev):
    kc = ev.keycode
    if kc == ord('X'):                      # middle button -> pan on move
      self._mb["middle"] = True
      self.mode = "pan"
    elif kc == ord('Z'):                    # left button -> select/move/wire/marquee
      self._mb["left"] = True
      self._on_push(self._last_mx, self._last_my, ev)
    elif kc == ord('C'):                    # right button (reserved)
      self._mb["right"] = True
    elif kc == ord('F'):
      if self._pointer_inside:              # canvas-focus gate (key-registry law)
        self.frame(sel_only=bool(self.sel_nodes))
    elif kc == ord('A'):
      # focus-local (key-registry law): only when the canvas owns the pointer/key focus.
      # Shift+A re-lays-out the current level; plain A fits ALL. A viewport Shift+A never
      # routes here (the ui context hit-tests key events on the cursor position), so it
      # stays wholly independent.
      if self._pointer_inside:
        if ev.shift:
          self.auto_layout()
        else:
          self.frame(sel_only=False)
    elif kc == ord('U'):
      self.up_level()
    elif kc == ord('S'):                    # toggle grid snap
      self.snap = not self.snap
      self.mark_overlay()
    elif kc in (259, 261):                  # Backspace / Delete
      self._delete_selection()
    elif kc == 258:                         # Tab
      self._show_add_menu()
    self._emit_selection()

  def handleKeyUp(self, ev):
    kc = ev.keycode
    if kc == ord('X'):
      self._mb["middle"] = False
      if self.mode == "pan":
        self.mode = None
    elif kc == ord('Z'):
      self._mb["left"] = False
      self._on_release(self._last_mx, self._last_my, ev)
    elif kc == ord('C'):
      self._mb["right"] = False
    self._emit_selection()

  def _delete_selection(self):
    model = self.model
    for edge in list(self.sel_edges):
      model.disconnect(*edge)
    for nid in list(self.sel_nodes):
      model.delete_node(nid)
    self.sel_nodes.clear()
    self.sel_edges.clear()
    self.mark_structure_changed()

  def _show_add_menu(self):
    if self.uicontext is None:
      return
    types = list(self.model.node_types())
    if not types:
      return
    rx, ry = self._last_root
    drop = self.view.screen_to_graph(*self.canvas.rootToLocal(rx, ry))

    def on_selected(val):
      self.model.add_node(val.lstrip("/"), drop)

    lev2.ui.DropdownMenu.show(
        context=self.uicontext,
        paths=types,
        x=rx, y=ry,
        on_selected=on_selected,
        sort_alphabetically=False)

  ###########################################################################
  # Factory methods
  ###########################################################################

  @staticmethod
  def uifactory(parent_layoutgroup, args):
    """args: [name, model, title?, bg_color?, orientation?]"""
    name = args[0]
    model = args[1]
    title = args[2] if len(args) > 2 else "root"
    bg = args[3] if len(args) > 3 else COL_BG
    orient = args[4] if len(args) > 4 else "vertical"
    canvas_item = parent_layoutgroup.makeChild(uiclass=lev2.ui.PrimCanvas, args=[name])
    canvas = canvas_item.widget
    canvas.bg_color = bg
    canvas.draw_background = True
    ne = NodeEditor(canvas, model, title, orient)
    canvas.uservars.node_editor = ne
    return canvas_item

  @staticmethod
  def wfactory(args):
    """args: [name, model, title?, bg_color?, orientation?]"""
    name = args[0]
    model = args[1]
    title = args[2] if len(args) > 2 else "root"
    bg = args[3] if len(args) > 3 else COL_BG
    orient = args[4] if len(args) > 4 else "vertical"
    canvas = lev2.ui.PrimCanvas.wfactory([name])
    canvas.bg_color = bg
    canvas.draw_background = True
    ne = NodeEditor(canvas, model, title, orient)
    canvas.uservars.node_editor = ne
    return canvas
