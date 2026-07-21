################################################################################
# terrain_node_model — TerrainNodeGraphModel: bind a TerrainDoc to the generic
# GPU node editor (ork.ui.node_editor.NodeEditor) as the terrain editor's canvas
# (JUL13_DFLOW E3, slice C1 — REPLACES the terrain editor's Outliner).
#
# The adapter renders the DOCUMENT (owner law L1) — DocNode / DocLoop / DocGroupCall
# / DocSwitch, keyed by tree_paths() keys (the SAME keying the runtime uses for the
# display node + node positions) — never the elaborated GraphData. One model instance
# per navigable LEVEL: the root binds the top-level doc children; child_model() binds a
# loop/group interior PLUS its boundary carry pills (pill_in = carry entering, pill_out =
# body output leaving). Every canvas mutation routes through the EXISTING doc API
# (add_op_node / add_loop / add_into_loop / delete_node / set_bypassed / set_display_key)
# then the host's _recordEdit + _requestRebake (owner law L2); a doc-API refusal is
# surfaced LOUDLY, never bypassed.
################################################################################

from ork.ui.node_editor import NodeGraphModel
from ork.editor.terrain_runtime import display_trace as _dtrace
from ork.hypergraph.dflow.terrain.doc import (
    DocNode, DocLoop, DocGroupCall, DocSwitch, _DocOutPlug,
    tree_paths, find_by_path,
    add_op_node, add_loop, add_into_loop, delete_node,
    declared_output_plugs, scatter_weight_captures, loop_external_refs,
    editor_add_ops, TerrainDocParamError)

# port/wire type tag: every terrain plug carries a heightfield/field ref, so a single
# type drives one deterministic wire color (the doc has no scalar-plug connections —
# scalar params are recorded params, not graph edges).
_FIELD = "field"

# pill node-id separators (must not collide with tree_paths() "/" keys)
_PSEP = "\x01"
# synthetic scatter-sink node-id separator (distinct from tree_paths "/" and pill "\x01")
_SSEP = "\x02"


def _pill_key(container_key, side, name):
  return f"{container_key}{_PSEP}{side}{_PSEP}{name}"


def _scatter_key(name):
  return f"{_SSEP}scatter{_SSEP}{name}"


################################################################################
# icons (white-on-transparent; the editor tints them per node color). Loops read as
# a cycle, groups as a subnet, captures as an artifact EXPORT (artifacts-not-graphs
# law: capture/scatter sinks are visually distinct from processing nodes).
################################################################################

_ICON_LOOP = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M5.5 12 a6.5 6.5 0 1 1 2.1 4.8" fill="none" stroke="#ffffff" stroke-width="2.1"
        stroke-linecap="round"/>
  <path d="M3.2 8.2 L5.6 12.2 L9.4 10.0 Z" fill="#ffffff"/>
</svg>'''

_ICON_SUBNET = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <rect x="3" y="3" width="7.5" height="7.5" rx="1.6" fill="#ffffff"/>
  <rect x="13.5" y="3" width="7.5" height="7.5" rx="1.6" fill="#ffffff"/>
  <rect x="3" y="13.5" width="7.5" height="7.5" rx="1.6" fill="#ffffff"/>
  <rect x="13.5" y="13.5" width="7.5" height="7.5" rx="1.6" fill="#ffffff"/>
</svg>'''

_ICON_CAPTURE = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M12 3 L12 13" stroke="#ffffff" stroke-width="2.2" stroke-linecap="round"/>
  <path d="M7.5 9.5 L12 14 L16.5 9.5" fill="none" stroke="#ffffff" stroke-width="2.2"
        stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M4 16 L4 20 L20 20 L20 16" fill="none" stroke="#ffffff" stroke-width="2.2"
        stroke-linecap="round" stroke-linejoin="round"/>
</svg>'''

# scatter sink: a mask-driven PLACEMENT set (scattered instances), read as a spray of dots.
_ICON_SCATTER = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <circle cx="6" cy="7" r="1.6" fill="#ffffff"/>
  <circle cx="12" cy="5" r="1.6" fill="#ffffff"/>
  <circle cx="18" cy="8" r="1.6" fill="#ffffff"/>
  <circle cx="8" cy="13" r="1.6" fill="#ffffff"/>
  <circle cx="15" cy="14" r="1.6" fill="#ffffff"/>
  <circle cx="6" cy="18" r="1.6" fill="#ffffff"/>
  <circle cx="12" cy="19" r="1.6" fill="#ffffff"/>
  <circle cx="18" cy="18" r="1.6" fill="#ffffff"/>
</svg>'''


################################################################################
# Shared session glue (one per editor; every level model references it)
################################################################################

class _Glue:
  """State shared across every level model of ONE editor session: the runtime (live
  document + display key), the host (undo / rebake), the canvas rebuild callbacks, a
  structure-version counter (bumped on every mutation to invalidate the per-level
  caches), the child-model cache (keyed by container tree-path), and the NodeEditor
  back-reference (for post-add selection)."""

  def __init__(self, host, runtime):
    self.host = host
    self.runtime = runtime
    self.version = 0
    self._cbs = []                    # canvas rebuild callbacks (deduped)
    self.node_editor = None           # set by the host after the NodeEditor exists
    self._child_models = {}           # container_key -> TerrainNodeGraphModel

  @property
  def doc(self):
    return self.runtime.document

  def add_cb(self, cb):
    # NodeEditor registers the SAME bound method for every level model; bound-method
    # equality dedups so a mutation fires the canvas rebuild exactly once.
    if cb not in self._cbs:
      self._cbs.append(cb)

  def fire_changed(self):
    self.version += 1
    for cb in list(self._cbs):
      cb()

  def edit(self, label, coalesce_key=None, rebake=True):
    """Record the already-applied mutation as an undoable step (host undo stack) and,
    unless it is a pure layout move, schedule a rebake — mirrors what the outliner
    handlers did per-operation."""
    self.host._recordEdit(label, coalesce_key=coalesce_key)
    _dtrace(f"edit {label!r} rebake={rebake}")
    if rebake:
      self.host._requestRebake()

  def child_model(self, container_key):
    m = self._child_models.get(container_key)
    if m is None:
      m = TerrainNodeGraphModel(self.host, self.runtime,
                                container_key=container_key, glue=self)
      self._child_models[container_key] = m
    return m

  def key_for_obj(self, obj):
    doc = self.doc
    if doc is None:
      return None
    for (_pk, k, o) in tree_paths(doc):
      if o is obj:
        return k
    return None


################################################################################
# Level model
################################################################################

class TerrainNodeGraphModel(NodeGraphModel):
  """One navigable level of a TerrainDoc bound to the generic NodeEditor. container_key
  = None for the root (top-level doc children); a loop/group tree-path key for an
  interior level (its children + boundary carry pills). All state is resolved LIVE from
  the runtime's current document, so an undo / retrace that swaps the document does not
  invalidate the model (only the structure-version cache is rebuilt)."""

  def __init__(self, host, runtime, container_key=None, glue=None):
    self._glue = glue if glue is not None else _Glue(host, runtime)
    self._container_key = container_key
    self._cache_version = -1
    self._cache = None

  # ---- doc access ----------------------------------------------------------

  @property
  def _doc(self):
    return self._glue.runtime.document

  def _container(self):
    if self._container_key is None:
      return None
    doc = self._doc
    return find_by_path(doc, self._container_key) if doc is not None else None

  # ---- per-level structure cache -------------------------------------------

  def _lvl(self):
    if self._cache is not None and self._cache_version == self._glue.version:
      return self._cache
    self._cache = self._build()
    self._cache_version = self._glue.version
    return self._cache

  def _build(self):
    doc = self._doc
    ck = self._container_key
    lvl = {
        "order": [],          # nid ordering
        "kind": {},           # nid -> node|loop|group|switch|pill_in|pill_out
        "obj": {},            # nid -> doc object (or _Carry for pills)
        "inputs": {},         # nid -> [(port, type)]
        "outputs": {},        # nid -> [(port, type)]
        "edges": [],          # [(snid, sport, dnid, dport)]
        "producer": {},       # nid -> {port: (producer_obj, plug_name)}  (connect src)
        "dst_setter": {},     # (nid, port) -> setter(new_ref)            (connect dst)
    }
    if doc is None:
      return lvl
    container = self._container()
    parent_match = "" if ck is None else ck

    src_map = {}     # id(producer_obj) -> ('node', nid) | ('fixed', nid, port)
    in_refs = {}     # nid -> [(port, ref)]

    def _add(nid, kind, obj):
      lvl["order"].append(nid)
      lvl["kind"][nid] = kind
      lvl["obj"][nid] = obj

    # -- peer document objects at this level --
    for (pk, k, obj) in tree_paths(doc):
      if pk != parent_match:
        continue
      if isinstance(obj, DocNode):
        _add(k, "node", obj)
        ins = [(inm, _FIELD) for (inm, _r) in obj.connections]
        lvl["inputs"][k] = ins
        in_refs[k] = [(inm, r) for (inm, r) in obj.connections]
        # ALL declared outputs (reflection), not only the connected ones — an unconnected
        # output was invisible + unwireable (topology honesty). CaptureModule declares none.
        outs = declared_output_plugs(obj)
        lvl["outputs"][k] = [(p, _FIELD) for p in outs]
        src_map[id(obj)] = ("node", k)
        lvl["producer"][k] = {p: (obj, p) for p in outs}
        for (inm, _r) in obj.connections:
          lvl["dst_setter"][(k, inm)] = self._node_input_setter(obj, inm)
      elif isinstance(obj, DocLoop):
        _add(k, "loop", obj)
        carries = obj.carries
        # loop-invariant EXTERNAL feeds are non-carry loop inputs: expose them as input ports
        # on the loop node (with a parent-level edge from each producer) so the loop is
        # CONNECTED to its external feeders at this level — else a loop that reads a field made
        # outside it renders as a spuriously disconnected component (owner-visible symptom).
        externals = loop_external_refs(doc, obj)
        lvl["inputs"][k] = [(cn, _FIELD) for cn in carries] + [(p, _FIELD) for (p, _r) in externals]
        lvl["outputs"][k] = [(cn, _FIELD) for cn in carries]
        in_refs[k] = ([(cn, c.initial_ref) for cn, c in carries.items()]
                      + [(p, r) for (p, r) in externals])
        prod = {}
        for cn, c in carries.items():
          src_map[id(c.out_placeholder)] = ("fixed", k, cn)
          prod[cn] = (c.out_placeholder, "Out")
          lvl["dst_setter"][(k, cn)] = self._attr_setter(c, "initial_ref")
        lvl["producer"][k] = prod
      elif isinstance(obj, DocGroupCall):
        _add(k, "group", obj)
        targs = [(an, v) for an, v in obj.args.items() if isinstance(v, _DocOutPlug)]
        lvl["inputs"][k] = [(an, _FIELD) for (an, _v) in targs]
        lvl["outputs"][k] = [("Out", _FIELD)]
        in_refs[k] = [(an, v) for (an, v) in targs]
        if obj.output_ref is not None:
          src_map[id(obj.output_ref.node)] = ("fixed", k, "Out")
          lvl["producer"][k] = {"Out": (obj.output_ref.node, obj.output_ref.plug_name)}
        for (an, _v) in targs:
          lvl["dst_setter"][(k, an)] = self._dict_setter(obj.args, an)
      elif isinstance(obj, DocSwitch):
        _add(k, "switch", obj)
        lvl["inputs"][k] = [(bn, _FIELD) for bn in obj.branches]
        lvl["outputs"][k] = [("out", _FIELD)]
        in_refs[k] = [(bn, r) for bn, r in obj.branches.items()]
        src_map[id(obj.out_placeholder)] = ("fixed", k, "out")
        lvl["producer"][k] = {"out": (obj.out_placeholder, "Out")}
        for bn in obj.branches:
          lvl["dst_setter"][(k, bn)] = self._dict_setter(obj.branches, bn)

    # -- boundary pills (loop / group interior) --
    if isinstance(container, DocLoop):
      for cn, c in container.carries.items():
        pin = _pill_key(ck, "in", cn)
        pout = _pill_key(ck, "out", cn)
        _add(pin, "pill_in", c)
        lvl["inputs"][pin] = []
        lvl["outputs"][pin] = [("out", _FIELD)]
        src_map[id(c.placeholder)] = ("fixed", pin, "out")
        lvl["producer"][pin] = {"out": (c.placeholder, "Out")}
        _add(pout, "pill_out", c)
        lvl["inputs"][pout] = [("in", _FIELD)]
        lvl["outputs"][pout] = []
        in_refs[pout] = [("in", c.body_out_ref)]
        lvl["dst_setter"][(pout, "in")] = self._attr_setter(c, "body_out_ref")
    elif isinstance(container, DocGroupCall):
      # a pill_out for the returned ref; a pill_in per terrain arg. A group re-traces against
      # PARENT-scope refs, so its body nodes wire straight to the arg's parent producer (one
      # level up) — register that producer -> this pill so the interior edge that arrives via
      # the arg renders (before, it was dropped: the pill had no producer/src_map entry, so
      # trace-built and doc-JSON-reloaded group interiors diverged from the loop's). The match
      # is by producer identity, which holds for a fresh trace AND a reload (same by_id node).
      for an, v in container.args.items():
        if isinstance(v, _DocOutPlug):
          pin = _pill_key(ck, "in", an)
          _add(pin, "pill_in", v)
          lvl["inputs"][pin] = []
          lvl["outputs"][pin] = [("out", _FIELD)]
          src_map[id(v.node)] = ("fixed", pin, "out")
          lvl["producer"][pin] = {"out": (v.node, v.plug_name)}
      if container.output_ref is not None:
        pout = _pill_key(ck, "out", "output")
        _add(pout, "pill_out", container.output_ref)
        lvl["inputs"][pout] = [("in", _FIELD)]
        lvl["outputs"][pout] = []
        in_refs[pout] = [("in", container.output_ref)]

    # -- scatter SINKS (root level only): a mask-driven placement set is a real bake sink.
    # base.scatter auto-captures each type's weight as a hidden `_scatter_<name>_w<i>` channel;
    # nothing grouped those, so the sink itself was invisible. Surface ONE sink node per scatter
    # with an input edge from each weight's producer (the same producers the weight captures read,
    # which stay visible with filters off) — the placement topology is now true + honest. A
    # v1 sink is display-only (no dst_setter -> not re-wireable, no output -> not a producer).
    if container is None:
      for sname, weights in scatter_weight_captures(doc).items():
        snid = _scatter_key(sname)
        _add(snid, "scatter", None)
        lvl["inputs"][snid] = [("w%d" % idx, _FIELD) for (idx, _cap, _p) in weights]
        lvl["outputs"][snid] = []
        in_refs[snid] = [("w%d" % idx, producer) for (idx, _cap, producer) in weights]

    # -- CONTAINER-INVARIANT EXTERNAL reads (interior levels): a body node reads a node
    # created OUTSIDE this container (not a body peer, not a carry placeholder, not a group
    # arg already piled above) — e.g. a loop body reading a loop-invariant field. elaborate
    # promotes these to non-carry inputs; the canvas must too, or the interior edge is silently
    # dropped (the loop analog of the group-arg pill). Surface each distinct external as an
    # input pill keyed by the external's STABLE tree-path (so trace-built and reloaded interiors
    # render identically), then route the interior edge from it.
    if container is not None:
      ext_keys = None
      for _nid, refs in list(in_refs.items()):
        for (_port, ref) in refs:
          if ref is None or id(ref.node) in src_map:
            continue
          if ext_keys is None:
            ext_keys = {id(o): kk for (_pk, kk, o) in tree_paths(doc)}
          base = ext_keys.get(id(ref.node)) or getattr(ref.node, "local_name", "ext")
          pin = _pill_key(ck, "extin", "%s#%s" % (base, ref.plug_name))
          if pin not in lvl["kind"]:
            _add(pin, "pill_in", ref.node)
            lvl["inputs"][pin] = []
            lvl["outputs"][pin] = [("out", _FIELD)]
            lvl["producer"][pin] = {"out": (ref.node, ref.plug_name)}
          src_map[id(ref.node)] = ("fixed", pin, "out")

    # -- resolve incoming refs into level edges --
    for nid, refs in in_refs.items():
      for (port, ref) in refs:
        if ref is None:
          continue
        sm = src_map.get(id(ref.node))
        if sm is None:
          continue                    # producer not at this level (dangling / cross-level)
        if sm[0] == "node":
          snid, sport = sm[1], ref.plug_name
        else:
          snid, sport = sm[1], sm[2]
        lvl["edges"].append((snid, sport, nid, port))

    return lvl

  # -- ref setters (connect targets) --
  @staticmethod
  def _node_input_setter(node, in_name):
    def setter(ref):
      for i, (n, _r) in enumerate(node.connections):
        if n == in_name:
          node.connections[i] = (in_name, ref)
          return
      node.connections.append((in_name, ref))
    return setter

  @staticmethod
  def _attr_setter(obj, attr):
    return lambda ref: setattr(obj, attr, ref)

  @staticmethod
  def _dict_setter(d, key):
    return lambda ref: d.__setitem__(key, ref)

  # ---- NodeGraphModel: topology / introspection ----------------------------

  def nodes(self):
    return list(self._lvl()["order"])

  def name(self, nid):
    lvl = self._lvl()
    kind = lvl["kind"].get(nid)
    obj = lvl["obj"].get(nid)
    if kind == "node":
      return obj.local_name
    if kind == "loop":
      return f"{obj.path}  x{obj.count}"
    if kind == "group":
      return obj.path
    if kind == "switch":
      return f"switch: {obj.selected}"
    if kind == "scatter":
      return f"scatter: {nid.split(_SSEP)[-1]}"
    # pill: the carry / arg name is the last id segment
    return nid.split(_PSEP)[-1]

  def type_name(self, nid):
    lvl = self._lvl()
    kind = lvl["kind"].get(nid)
    if kind == "node":
      return lvl["obj"][nid].clazz_name or "node"
    if kind in ("loop", "group", "switch", "scatter"):
      return kind
    return "boundary"

  def icon(self, nid):
    lvl = self._lvl()
    kind = lvl["kind"].get(nid)
    if kind == "loop":
      return _ICON_LOOP
    if kind == "group":
      return _ICON_SUBNET
    if kind == "scatter":
      return _ICON_SCATTER
    if kind == "node" and lvl["obj"][nid].clazz_name == "CaptureModule":
      return _ICON_CAPTURE
    return None

  def pos(self, nid):
    doc = self._doc
    return doc.node_pos(nid) if doc is not None else None

  def set_pos(self, nid, x, y):
    doc = self._doc
    if doc is not None:
      doc.set_node_pos(nid, x, y)

  def inputs(self, nid):
    return list(self._lvl()["inputs"].get(nid, []))

  def outputs(self, nid):
    return list(self._lvl()["outputs"].get(nid, []))

  def render_kind(self, nid):
    kind = self._lvl()["kind"].get(nid)
    return kind if kind in ("pill_in", "pill_out") else "box"

  def is_group(self, nid):
    return self._lvl()["kind"].get(nid) in ("loop", "group")

  def child_model(self, nid):
    if not self.is_group(nid):
      return None
    return self._glue.child_model(nid)

  def object_for_nid(self, nid):
    """The document object a node-id selects (host propsheet binding), or None for a
    boundary pill / unknown."""
    lvl = self._lvl()
    if lvl["kind"].get(nid) in ("node", "loop", "group", "switch"):
      return lvl["obj"].get(nid)
    return None

  # ---- edges ---------------------------------------------------------------

  def edges(self):
    return list(self._lvl()["edges"])

  def can_connect(self, si, sp, di, dp):
    if si == di:
      return (False, "cannot wire a node to itself")
    lvl = self._lvl()
    if lvl["producer"].get(si, {}).get(sp) is None:
      return (False, "not a connectable output")
    if (di, dp) not in lvl["dst_setter"]:
      return (False, "not a connectable input")
    return (True, "")

  def connect(self, si, sp, di, dp):
    lvl = self._lvl()
    prod = lvl["producer"].get(si, {}).get(sp)
    setter = lvl["dst_setter"].get((di, dp))
    if prod is None or setter is None:
      print(f"[terrain] connect refused: {si}.{sp} -> {di}.{dp} (invalid endpoints)",
            flush=True)
      return
    if _PSEP in si or _PSEP in di:
      # boundary PILLS (a loop/group interior's carry boundary) are canvas view geometry,
      # NOT document tree-path nodes -> write the resolved carry slot directly. The doc
      # owns every node/construct edge write (the non-pill branch below).
      obj, plug = prod
      setter(_DocOutPlug(obj, plug))
    else:
      try:
        self._doc.connect(di, dp, si, sp)   # doc owns the native-edge write (L2)
      except TerrainDocParamError as ex:
        print(f"[terrain] {ex}", flush=True)
        return ("refused", str(ex))
    self._glue.fire_changed()
    self._glue.edit(f"wire -> {di}.{dp}", rebake=True)

  def disconnect(self, si, sp, di, dp):
    # DECIDED (E2): a terrain input is ALWAYS fed — the document REFUSES a bare edge cut
    # LOUDLY (never a silent no-op). Surfaced here, not fatal: splice-on-wire still works
    # because NodeEditor immediately re-connects the dst plug (connect overwrites).
    try:
      self._doc.disconnect(si, sp, di, dp)
    except TerrainDocParamError as ex:
      print(f"[terrain] {ex}", flush=True)
      return ("refused", str(ex))

  # ---- node-body flags -----------------------------------------------------

  def is_output(self, nid):
    # DISPLAY badge rides a processing DocNode AND (Fix 1) a DocLoop (its carry output).
    if self._lvl()["kind"].get(nid) not in ("node", "loop"):
      return False
    rt = self._glue.runtime
    if rt.display_key is not None:
      return rt.display_key == nid
    # NO session override -> the badge rides the EFFECTIVE (implicit) terminal so the canvas is
    # never flagless on open: the node whose output the default bake/viewport already shows
    # (#17 de-fang). display_key stays None (the default FULL bake is unchanged).
    return rt.effective_display_terminal() == nid

  def set_output(self, nid, on):
    # Returns None on success (the badge change IS the feedback); ("noop"|"refused", msg)
    # otherwise, so the canvas can surface a visible, distinct status (Fix 3).
    rt = self._glue.runtime
    _dtrace(f"set_output nid={nid!r} on={on} (display_key={rt.display_key!r})")
    if on and rt.display_key == nid:
      _dtrace(f"set_output NOOP ({nid!r} already the display output)")
      return ("noop", f"{self.name(nid)} is already the display output")
    try:
      rt.set_display_key(nid if on else None)
    except ValueError as ex:
      print(f"[terrain] {ex}", flush=True)  # non-displayable node / capture — surface loudly
      return ("refused", str(ex))
    self._glue.fire_changed()
    self._glue.edit(f"display {nid}", rebake=True)
    return None

  def is_bypassed(self, nid):
    obj = self._lvl()["obj"].get(nid)
    return bool(getattr(obj, "bypassed", False)) if obj is not None else False

  def set_bypassed(self, nid, on):
    # Returns None on success; ("refused", msg) when the doc statically/dynamically refuses
    # (source / capture / generator group / switch) so the canvas surfaces it visibly (Fix 3).
    obj = self._lvl()["obj"].get(nid)
    if obj is None or not hasattr(obj, "set_bypassed"):
      return ("refused", f"{self.name(nid)} cannot be bypassed")
    try:
      obj.set_bypassed(bool(on))            # DocNode / DocLoop / DocGroupCall (switch refuses)
    except TerrainDocParamError as ex:
      print(f"[terrain] {ex}", flush=True)  # source / capture / generator group / switch
      return ("refused", str(ex))
    self._glue.fire_changed()
    self._glue.edit(f"bypass {nid}", rebake=True)
    return None

  # ---- flag CAPABILITY (canvas hides buttons the flag can NEVER apply to) ---

  def has_display_flag(self, nid):
    # DISPLAY button appears only where set_display_key can accept the node: a processing
    # DocNode (captures statically refused) or a DocLoop (Fix 1). Groups / switches / pills
    # have no single 'Out' to display -> no button.
    obj = self._lvl()["obj"].get(nid)
    return bool(self._glue.runtime._is_displayable(obj))

  def has_bypass_flag(self, nid):
    # BYPASS button appears only where the doc CAN bypass the node — the SAME predicate
    # set_bypassed uses to refuse: captures + source nodes (DocNode.bypassable), switches
    # (never), generator groups (no terrain-typed arg) all statically refuse -> no button.
    obj = self._lvl()["obj"].get(nid)
    return bool(obj is not None and hasattr(obj, "bypassable") and obj.bypassable())

  def fill_style(self, nid):
    # captures + scatter sinks render HOLLOW (outline + header strip, no body fill) — an
    # 'artifact export' sink, visually distinct from processing nodes (pairs with the
    # no-display-flag rule; the scatter sink is a placement artifact, not a field producer).
    lvl = self._lvl()
    kind = lvl["kind"].get(nid)
    if kind == "scatter":
      return "hollow"
    if kind == "node" and lvl["obj"][nid].clazz_name == "CaptureModule":
      return "hollow"
    return "solid"

  # ---- authoring -----------------------------------------------------------

  def node_types(self):
    container = self._container()
    if isinstance(container, DocLoop):
      ops = list(editor_add_ops())
    else:
      ops = list(editor_add_ops()) + ["loop"]
    return ["/" + o for o in ops]

  def _selected_docnode(self):
    ne = self._glue.node_editor
    if ne is None:
      return None
    lvl = self._lvl()
    for nid in ne.sel_nodes:
      if lvl["kind"].get(nid) == "node":
        return lvl["obj"][nid]
    return None

  def add_node(self, type_name, pos):
    doc = self._doc
    if doc is None:
      return None
    op = type_name
    container = self._container()
    try:
      if isinstance(container, DocLoop):
        node = add_into_loop(doc, container, op)
      else:
        after = self._selected_docnode()
        if after is None:
          print("[terrain] add: select a node to insert after (Tab adds after the "
                "selected node)", flush=True)
          return None
        if op == "loop":
          node = add_loop(doc, after)
        else:
          node = add_op_node(doc, op, after)
    except TerrainDocParamError as ex:
      print(f"[terrain] {ex}", flush=True)
      return None
    new_key = self._glue.key_for_obj(node)
    if new_key and pos is not None:
      doc.set_node_pos(new_key, pos[0], pos[1])
    self._glue.fire_changed()
    self._glue.edit(f"add {new_key or op}", rebake=True)
    # select the new node (mirror the outliner add flow: onSelect -> propsheet rebind)
    ne = self._glue.node_editor
    if new_key and ne is not None:
      ne.sel_nodes = {new_key}
      ne.sel_edges.clear()
      ne.mark_selection_changed()
      ne._emit_selection()
    return new_key

  def delete_node(self, nid):
    lvl = self._lvl()
    obj = lvl["obj"].get(nid)
    if lvl["kind"].get(nid) != "node" or not isinstance(obj, DocNode):
      print(f"[terrain] delete: {nid!r} is not a deletable node (v1: nodes only)",
            flush=True)
      return
    try:
      delete_node(self._doc, obj)           # chain-delete: reconnect consumers (L2)
    except TerrainDocParamError as ex:
      print(f"[terrain] {ex}", flush=True)  # last height/normal capture / orphan source
      return
    self._glue.fire_changed()
    self._glue.edit(f"delete {nid}", rebake=True)

  # ---- notification / persistence ------------------------------------------

  def on_changed(self, cb):
    self._glue.add_cb(cb)

  def save_layout(self):
    # positions are pure editor layout (they never change the bake) — record a COALESCED
    # undo checkpoint (so a drag rides the doc-JSON checkpoints) with NO rebake.
    self._glue.edit("move nodes", coalesce_key="ne_layout", rebake=False)
