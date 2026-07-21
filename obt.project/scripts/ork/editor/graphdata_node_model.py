################################################################################
# graphdata_node_model — GraphDataNodeGraphModel: bind a GraphDataDocument (the
# default doc-less family document — particles first, hypermesh next) to the generic
# GPU node editor (ork.ui.node_editor.NodeEditor), as the standalone dflow editor's
# canvas (JUL13_DFLOW E3, family #2 adapter).
#
# The adapter renders the live GraphData WRAPPED by the document (owner law: the graph
# IS the document for these families) via the E1 reflection introspection:
#   * nodes come from the document's flat module list;
#   * per-node plug ports come from dflow.plugSpec(class) (name + demangled type);
#   * EDGES come from GraphData.edges() (the E1 edge-introspection binding);
#   * positions ride the GRAPH-level _editor_layout map (setNodePos/nodePos — the
#     first UI consumer of the E1 layout store); auto-layout on first open (no stored
#     positions) writes back through it, so a Save round-trips them in the .orj JSON;
#   * flags: bypass maps to each module's reflected _bypassed; display maps to the
#     graph's _output_node (the exclusive display marker);
#   * can_connect delegates to the ENGINE verdict (GraphData::plugsCompatible — strict
#     flow-type + fan-out) and composes the human-readable reason here.
#
# Structural authoring is LIVE: Tab-add resolves a reflected class name through the engine's
# rtti factory (GraphData::createByClassName) and delete severs+drops via GraphData::removeModule;
# node_types() lists the family-compatible module classes from dflow.moduleClasses().
################################################################################

from orkengine.core import dataflow as _dflow

from ork.ui.node_editor import NodeGraphModel
from ork.ui import standard_icons
from ork.hypergraph.dflow.document import _short_class_name


class GraphDataNodeGraphModel(NodeGraphModel):
  """One flat GraphData (the whole document — these families have no nested loop/group
  constructs) bound to the generic NodeEditor over a GraphDataDocument. All topology,
  ports, flags and positions resolve LIVE through the document + the reflection
  substrate, so a mutation never drifts from the serialized graph."""

  def __init__(self, document):
    self._doc = document
    # elaborate() is the IDENTITY for a GraphDataDocument — the wrapped, live GraphData.
    # Held for the graph-level _output_node (display flag) read.
    self._graph = document.elaborate()
    self._cbs = []
    self.node_editor = None            # host back-ref (status strip + post-add select)
    self._port_cache = {}              # reflected-class-name -> (inputs, outputs)
    self._nodes_cache = None           # [(module_name, reflected_class_name)]
    self._edges_cache = None

  # ---- structure caches ----------------------------------------------------

  def _nodes(self):
    if self._nodes_cache is None:
      self._nodes_cache = self._doc.nodes()      # [(name, reflected_class_name)]
    return self._nodes_cache

  def _class_of(self, nid):
    return self._doc.node_class(nid)

  def _ports_for_class(self, class_name):
    """(inputs, outputs) = [(plug_name, demangled_type)] for a reflected module class,
    from the E1 dflow.plugSpec() schema (one scratch instance per class, cached in C++
    and again here)."""
    pc = self._port_cache.get(class_name)
    if pc is None:
      spec = _dflow.plugSpec(class_name) or {"inputs": [], "outputs": []}
      ins  = [(p["name"], p["type"]) for p in spec.get("inputs", [])]
      outs = [(p["name"], p["type"]) for p in spec.get("outputs", [])]
      pc = (ins, outs)
      self._port_cache[class_name] = pc
    return pc

  # ---- NodeGraphModel: topology / introspection ----------------------------

  def nodes(self):
    return [name for (name, _cls) in self._nodes()]

  def name(self, nid):
    return nid

  def type_name(self, nid):
    # short reflected name drives the deterministic node/wire color (e.g.
    # "psys::GravityModuleData" -> "GravityModule").
    cls = self._class_of(nid)
    return _short_class_name(cls) if cls else "node"

  def pos(self, nid):
    return self._doc.node_pos(nid)                # (x, y) from _editor_layout, or None

  def set_pos(self, nid, x, y):
    self._doc.set_node_pos(nid, x, y)             # -> graph _editor_layout (round-trips)

  def inputs(self, nid):
    cls = self._class_of(nid)
    return list(self._ports_for_class(cls)[0]) if cls else []

  def outputs(self, nid):
    cls = self._class_of(nid)
    return list(self._ports_for_class(cls)[1]) if cls else []

  def icon(self, nid):
    # CATEGORY ICON: the family capability object (via the document) classifies the node
    # into a category with a canvas glyph; a white-tinted standard_icons SVG string is
    # handed back with the SAME treatment as the group/loop icons (the canvas prebuilds a
    # texture per distinct SVG at gpuInit — never during render). A family without
    # capabilities returns None here -> no icon, unchanged rendering.
    fn = getattr(self._doc, "node_icon", None)
    name = fn(nid) if fn else None
    return standard_icons.svg_markup(name, "#ffffff") if name else None

  # ---- edges ---------------------------------------------------------------

  def edges(self):
    if self._edges_cache is None:
      out = []
      for e in self._doc.edges():                 # E1 GraphData.edges() introspection
        out.append((e["out_module"], e["out_plug"], e["in_module"], e["in_plug"]))
      self._edges_cache = out
    return list(self._edges_cache)

  def _plug_type(self, nid, side, plug):
    ports = self.outputs(nid) if side == "out" else self.inputs(nid)
    for (pn, pt) in ports:
      if pn == plug:
        return pt
    return None

  def can_connect(self, si, sp, di, dp):
    # The VERDICT is the engine's own connectability check (GraphData::plugsCompatible —
    # strict typeid + fan-out, via the document); the human-readable reason is composed here.
    if si == di:
      return (False, "cannot wire a node to itself")
    ot = self._plug_type(si, "out", sp)
    it = self._plug_type(di, "in", dp)
    if ot is None:
      return (False, f"{si} has no output plug {sp!r}")
    if it is None:
      return (False, f"{di} has no input plug {dp!r}")
    try:
      ok = self._doc.plugs_compatible(si, sp, di, dp)
    except Exception as ex:
      return (False, str(ex))
    if not ok:
      return (False, f"type mismatch: {ot} -> {it}")
    return (True, "")

  def connect(self, si, sp, di, dp):
    ok, reason = self.can_connect(si, sp, di, dp)
    if not ok:
      self._status(f"connect refused: {reason}")
      return
    try:
      self._doc.connect(di, dp, si, sp)           # (dst_node, dst_plug, src_node, src_plug)
    except Exception as ex:                        # loud, never a silent drop
      self._status(f"connect failed: {ex}")
      return
    self._invalidate()
    self._fire()

  def disconnect(self, si, sp, di, dp):
    try:
      self._doc.disconnect(di, dp)                 # remove the edge feeding the consumer plug
    except Exception as ex:
      self._status(f"disconnect failed: {ex}")
      return
    self._invalidate()
    self._fire()

  # ---- node-body flags -----------------------------------------------------

  def is_bypassed(self, nid):
    bp = self._doc.node_bypass_badge(nid)          # (bypassable, active) or None
    return bool(bp[1]) if bp else False

  def set_bypassed(self, nid, on):
    try:
      self._doc.set_bypassed(nid, bool(on))
    except Exception as ex:
      return ("refused", str(ex))
    self._fire()
    return None

  def has_bypass_flag(self, nid):
    bp = self._doc.node_bypass_badge(nid)
    return bool(bp and bp[0])

  def is_output(self, nid):
    return self._graph.output_node == nid          # exclusive display marker

  def set_output(self, nid, on):
    # Houdini semantics: exactly one display node (clicking sets it exclusively; the
    # canvas only ever calls this with on=True).
    try:
      self._doc.select_output(nid if on else None)
    except Exception as ex:
      return ("refused", str(ex))
    self._fire()
    return None

  def has_display_flag(self, nid):
    return bool(self._doc.node_output_badge(nid))   # any module with an output plug

  # ---- authoring -----------------------------------------------------------

  def node_types(self):
    # Tab-add palette = the module classes COMPATIBLE with this graph's family (the reflected
    # namespace prefix of the modules already present, e.g. "psys"), from dflow.moduleClasses().
    # An editor.palette.sort annotation curates order where present; absent = listed, name-sorted
    # (particles modules are not yet curated). The menu value is the reflected class name, which
    # add_node hands straight to the engine's rtti factory.
    fams = set()
    for (_name, cls) in self._nodes():
      if cls and "::" in cls:
        fams.add(cls.split("::", 1)[0])
    entries = []
    for c in _dflow.moduleClasses():
      name = c.get("name", "")
      if not name:
        continue
      if fams and c.get("family") not in fams:
        continue
      anns = c.get("annotations", {}) or {}
      sort = int(anns.get("editor.palette.sort", 1 << 30))
      entries.append((sort, name))
    entries.sort(key=lambda e: (e[0], e[1]))
    return ["/" + name for (_s, name) in entries]

  def add_node(self, type_name, pos):
    # type_name is the reflected class name the add-menu carried (leading "/" already stripped).
    try:
      new_name = self._doc.add_node_by_class_name(type_name)
    except Exception as ex:
      self._status(f"add-node failed: {ex}")
      return None
    self._nodes_cache = None
    self._edges_cache = None
    if pos is not None:
      self._doc.set_node_pos(new_name, pos[0], pos[1])
    self._fire()
    ne = self.node_editor
    if ne is not None:
      ne.sel_nodes = {new_name}
      ne.sel_edges.clear()
      ne.mark_selection_changed()
      ne._emit_selection()
    return new_name

  def delete_node(self, nid):
    try:
      self._doc.delete_node(nid)
    except Exception as ex:
      self._status(f"delete failed: {ex}")
      return
    self._nodes_cache = None
    self._edges_cache = None
    self._fire()

  # ---- host handle for the property sheet ----------------------------------

  def object_for_nid(self, nid):
    """The document node handle a canvas node-id selects — for a GraphDataDocument the
    module NAME (what editable_params / get_param / set_param take)."""
    return nid

  # ---- notification / persistence ------------------------------------------

  def on_changed(self, cb):
    if cb not in self._cbs:
      self._cbs.append(cb)

  def save_layout(self):
    # positions already live in the graph-level _editor_layout (set_node_pos wrote them
    # there); they persist to disk on Save (.orj re-serialize) — nothing to flush here.
    pass

  # ---- internals -----------------------------------------------------------

  def _invalidate(self):
    self._edges_cache = None           # connect/disconnect change edges, not the node set

  def _fire(self):
    for cb in list(self._cbs):
      cb()

  def _status(self, msg):
    print(f"[dflowedit] {msg}", flush=True)
    if self.node_editor is not None:
      self.node_editor.show_status(msg)
