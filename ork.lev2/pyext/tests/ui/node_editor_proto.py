#!/usr/bin/env ork.python
################################################################################
# node_editor_proto - windowed harness for the generic node editor (C0).
#
# A hand-built toy node graph (plain dataclasses implementing the NodeEditor
# MODEL PROTOCOL) driving ork.ui.node_editor.NodeEditor. This is the de-risk
# vehicle for the real dataflow node editor: nothing here is terrain/dflow
# specific. Houdini-style vertical flow — plugs on top (inputs) and bottom
# (outputs); plug labels appear on hover / when about to connect.
#
#   Interactions:
#     middle-drag / shift+scroll ... pan          scroll ............ zoom-to-cursor
#     drag node .................. move (group)    drag empty ........ marquee select
#     click / shift-click ........ select/toggle   drag out-plug ..... wire (live valid)
#     click wire ................. select wire     Del / Backspace ... delete selection
#     double-click group ......... dive in         U ................. up a level
#     Tab ........................ add-node menu    F=frame sel / A=fit all
#     click a path-bar crumb ..... jump to level    click flag ........ display / bypass
################################################################################

import os
from dataclasses import dataclass
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.ui.node_editor import NodeEditor, COL_BG

tokens = CrcStringProxy()

# opaque, fill-only glyphs on transparent BG (no border); the editor tints them
# monochrome. Different group subtypes (subnet/loop/...) supply different icons
# — this is the seam the real dataflow schema will drive per node-type.
ICON_SUBNET = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <rect x="3" y="3" width="7.5" height="7.5" rx="1.6" fill="#ffffff"/>
  <rect x="13.5" y="3" width="7.5" height="7.5" rx="1.6" fill="#ffffff"/>
  <rect x="3" y="13.5" width="7.5" height="7.5" rx="1.6" fill="#ffffff"/>
  <rect x="13.5" y="13.5" width="7.5" height="7.5" rx="1.6" fill="#ffffff"/>
</svg>'''
ICON_FOLDER = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M2.5 6 a2 2 0 0 1 2 -2 h4.5 l2 2 h8.5 a2 2 0 0 1 2 2 v8 a2 2 0 0 1 -2 2 h-15 a2 2 0 0 1 -2 -2 Z" fill="#ffffff"/>
</svg>'''

################################################################################
# Toy model — plain dataclasses implementing the NodeEditor protocol
################################################################################

@dataclass
class ToyNode:
  nid: str
  name: str
  type_name: str
  inputs: list          # [(plug_name, type_name), ...]
  outputs: list
  pos: object = None     # (x,y) graph-space, or None -> auto-layout
  is_group: bool = False
  child: object = None   # ToyGraphModel for the interior
  color: object = None
  render_kind: str = "box"
  icon: object = None    # optional SVG string (centered above label)
  bypassed: bool = False
  output: bool = False


# add-node library: type path -> (inputs, outputs)
NODE_LIBRARY = {
  "Source/Noise":    ([], [("out", "heightmap")]),
  "Source/Constant": ([], [("out", "heightmap")]),
  "Filter/Warp":     ([("in", "heightmap")], [("out", "heightmap")]),
  "Filter/Blur":     ([("in", "heightmap")], [("out", "heightmap")]),
  "Combine/Add":     ([("a", "heightmap"), ("b", "heightmap")], [("out", "heightmap")]),
  "Combine/Mix":     ([("a", "heightmap"), ("b", "heightmap")], [("out", "heightmap")]),
  "Mesh/Meshify":    ([("in", "heightmap")], [("out", "mesh")]),
  "Output/Sink":     ([("in", "mesh")], []),
}


class ToyGraphModel:
  """A single node-graph level. Implements the NodeEditor model protocol."""

  def __init__(self, name):
    self.level_name = name    # persistence key (NOT the protocol node accessor)
    self._nodes = {}
    self._edges = []
    self._cbs = []
    self._counter = 0

  # -- construction helpers --
  def add(self, node):
    self._nodes[node.nid] = node
    return node.nid

  def wire(self, si, sp, di, dp):
    self._edges.append((si, sp, di, dp))

  # -- protocol: introspection --
  def nodes(self):        return list(self._nodes.keys())
  def name(self, nid):    return self._nodes[nid].name
  def type_name(self, nid): return self._nodes[nid].type_name
  def color(self, nid):   return self._nodes[nid].color
  def pos(self, nid):     return self._nodes[nid].pos
  def set_pos(self, nid, x, y): self._nodes[nid].pos = (x, y)
  def inputs(self, nid):  return list(self._nodes[nid].inputs)
  def outputs(self, nid): return list(self._nodes[nid].outputs)
  def is_group(self, nid): return self._nodes[nid].is_group
  def child_model(self, nid): return self._nodes[nid].child
  def render_kind(self, nid): return self._nodes[nid].render_kind
  def icon(self, nid):     return self._nodes[nid].icon

  # -- node-body flags (output=display is exclusive; bypass is per-node) --
  def is_output(self, nid):   return self._nodes[nid].output
  def is_bypassed(self, nid): return self._nodes[nid].bypassed

  def set_output(self, nid, on):
    if on:
      for n in self._nodes.values():
        n.output = False
    self._nodes[nid].output = bool(on)
    self._fire()

  def set_bypassed(self, nid, on):
    self._nodes[nid].bypassed = bool(on)
    self._fire()

  # -- protocol: edges --
  def edges(self): return list(self._edges)

  def can_connect(self, si, sp, di, dp):
    if si == di:
      return (False, "cannot wire a node to itself")
    stype = self._plug_type(si, "out", sp)
    dtype = self._plug_type(di, "in", dp)
    if stype is None or dtype is None:
      return (False, "no such plug")
    if stype != dtype:
      return (False, f"type mismatch: {stype} != {dtype}")
    return (True, "")

  def connect(self, si, sp, di, dp):
    self._edges = [e for e in self._edges if not (e[2] == di and e[3] == dp)]
    self._edges.append((si, sp, di, dp))
    self._fire()

  def disconnect(self, si, sp, di, dp):
    self._edges = [e for e in self._edges if e != (si, sp, di, dp)]
    self._fire()

  # -- protocol: authoring --
  def node_types(self):
    return ["/" + k for k in NODE_LIBRARY.keys()]

  def add_node(self, type_name, pos):
    spec = NODE_LIBRARY.get(type_name)
    if spec is None:
      return None
    self._counter += 1
    short = type_name.split("/")[-1]
    nid = f"{short.lower()}_{self._counter}"
    ins, outs = spec
    self.add(ToyNode(nid=nid, name=nid, type_name=short,
                     inputs=list(ins), outputs=list(outs), pos=pos))
    self._fire()
    return nid

  def delete_node(self, nid):
    if nid not in self._nodes:
      return
    del self._nodes[nid]
    self._edges = [e for e in self._edges if e[0] != nid and e[2] != nid]
    self._fire()

  # -- protocol: notification --
  def on_changed(self, cb):
    self._cbs.append(cb)

  # -- persistence (optional hooks used by the editor) --
  def save_layout(self):
    path = _layout_path(self.level_name)
    if path is None:
      return
    import json
    data = {nid: list(n.pos) for nid, n in self._nodes.items() if n.pos is not None}
    try:
      with open(path, "w") as f:
        json.dump(data, f, indent=2)
    except OSError:
      pass

  def load_layout(self):
    path = _layout_path(self.level_name)
    if path is None or not os.path.exists(path):
      return
    import json
    try:
      with open(path) as f:
        data = json.load(f)
    except (OSError, ValueError):
      return
    for nid, p in data.items():
      if nid in self._nodes:
        self._nodes[nid].pos = tuple(p)

  # -- internals --
  def _fire(self):
    for cb in list(self._cbs):
      cb()

  def _plug_type(self, nid, side, plug_name):
    if nid not in self._nodes:
      return None
    ports = self._nodes[nid].outputs if side == "out" else self._nodes[nid].inputs
    for pn, tn in ports:
      if pn == plug_name:
        return tn
    return None


def _layout_path(name):
  try:
    import obt.path
    d = os.path.join(str(obt.path.temp()), "node_editor_proto")
    os.makedirs(d, exist_ok=True)
    return os.path.join(d, f"{name}.json")
  except Exception:
    return None


################################################################################
# Starter graph: 8 root nodes, fan-out (1->3), fan-in (2-in combine), and two
# groups with one nested INSIDE the other (root / detailFX / toon).
################################################################################

def _build_group_toon():
  m = ToyGraphModel("toon")
  m.add(ToyNode("in",  "in",  "boundary", [], [("out", "heightmap")], render_kind="pill_in"))
  m.add(ToyNode("toMesh", "toMesh", "Meshify", [("in", "heightmap")], [("out", "mesh")]))
  m.add(ToyNode("out", "out", "boundary", [("in", "mesh")], [], render_kind="pill_out"))
  m.wire("in", "out", "toMesh", "in")
  m.wire("toMesh", "out", "out", "in")
  m.load_layout()
  return m


def _build_group_detailfx():
  m = ToyGraphModel("detailFX")
  m.add(ToyNode("in0", "in0", "boundary", [], [("out", "heightmap")], render_kind="pill_in"))
  m.add(ToyNode("in1", "in1", "boundary", [], [("out", "heightmap")], render_kind="pill_in"))
  m.add(ToyNode("mix", "mix", "Mix",
                [("a", "heightmap"), ("b", "heightmap")], [("out", "heightmap")]))
  m.add(ToyNode("toon", "toon", "Group", [("in", "heightmap")], [("out", "mesh")],
                is_group=True, child=_build_group_toon(), icon=ICON_FOLDER))
  m.add(ToyNode("out", "out", "boundary", [("in", "mesh")], [], render_kind="pill_out"))
  m.wire("in0", "out", "mix", "a")
  m.wire("in1", "out", "mix", "b")
  m.wire("mix", "out", "toon", "in")
  m.wire("toon", "out", "out", "in")
  m.load_layout()
  return m


def build_root_model():
  m = ToyGraphModel("root")
  m.add(ToyNode("noise", "noise", "Noise", [], [("out", "heightmap")]))
  m.add(ToyNode("warpA", "warpA", "Warp", [("in", "heightmap")], [("out", "heightmap")]))
  m.add(ToyNode("warpB", "warpB", "Warp", [("in", "heightmap")], [("out", "heightmap")]))
  m.add(ToyNode("warpC", "warpC", "Warp", [("in", "heightmap")], [("out", "heightmap")]))
  m.add(ToyNode("combine", "combine", "Add",
                [("a", "heightmap"), ("b", "heightmap")], [("out", "heightmap")]))
  m.add(ToyNode("detailFX", "detailFX", "Group",
                [("in0", "heightmap"), ("in1", "heightmap")], [("out", "mesh")],
                is_group=True, child=_build_group_detailfx(), icon=ICON_SUBNET))
  m.add(ToyNode("meshify", "meshify", "Meshify", [("in", "mesh")], [("out", "mesh")]))
  m.add(ToyNode("output", "output", "Sink", [("in", "mesh")], [], output=True))
  # fan-out: noise -> 3 warps
  m.wire("noise", "out", "warpA", "in")
  m.wire("noise", "out", "warpB", "in")
  m.wire("noise", "out", "warpC", "in")
  # fan-in: warpA + warpB -> combine
  m.wire("warpA", "out", "combine", "a")
  m.wire("warpB", "out", "combine", "b")
  # into the group and out to the mesh chain
  m.wire("combine", "out", "detailFX", "in0")
  m.wire("warpC", "out", "detailFX", "in1")
  m.wire("detailFX", "out", "meshify", "in")
  m.wire("meshify", "out", "output", "in")
  m.load_layout()
  return m


################################################################################
# Application
################################################################################

class NodeEditorProtoApp(ComponentizedApplication):

  def __init__(self, width=1200, height=980):
    super().__init__(profiler_channels=[])
    self.createEzApp(width=width, height=height)
    self._perf_t = 0.0
    self._last_rc = 0

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.clearColorStd = COL_BG
    cl = lg.makeChild(uiclass=lev2.ui.PrimCanvas, args=["nodeeditor"])
    self.canvas = cl.widget
    for edge in ["top", "left", "bottom", "right"]:
      getattr(cl.layout, edge).anchorTo(getattr(lg.layout, edge))
    self.canvas.bg_color = COL_BG
    self.canvas.draw_background = True

    self.model = build_root_model()
    self.editor = NodeEditor(self.canvas, self.model, "root", orientation="vertical")

  def _onGpuInit(self, ctx):
    self.editor.uicontext = self.ezapp.uicontext
    self.editor.gpuInit(ctx)              # build crisp SVG glyph textures
    print("[node_editor_proto] ready — Tab=add, F=frame sel, A=fit all, U=up, "
          "Del=delete, dbl-click group=dive, scroll=zoom, x-drag=pan, hover a plug for its label")

  def _onUpdate(self, updinfo):
    self._perf_t += updinfo.deltatime
    if self._perf_t >= 1.0:
      self._perf_t -= 1.0
      rc = self.editor.rebuild_count
      delta = rc - self._last_rc
      self._last_rc = rc
      print(f"[perf] rebuild_count={rc} delta/sec={delta} ssbo_rebuilds={self.canvas.ssbo_rebuild_count}")

  def _onUiEvent(self, ev):
    if ev.code == tokens.KEY_DOWN.hashed:
      self.editor.handleKeyDown(ev)
    elif ev.code == tokens.KEY_UP.hashed:
      self.editor.handleKeyUp(ev)
    return lev2.ui.HandlerResult()


###############################################################################

import sys

_w, _h = 1200, 980
for _a in sys.argv[1:]:
  if _a.startswith("--size="):
    try:
      _w, _h = (int(v) for v in _a.split("=", 1)[1].split("x"))
    except ValueError:
      pass

app = NodeEditorProtoApp(width=_w, height=_h)
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
