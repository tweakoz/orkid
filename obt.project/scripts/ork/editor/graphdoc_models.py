################################################################################
# ork.editor.graphdoc_models — GENERIC editor models over a GraphDocument.
#
# One editor core, N document specializations (JUL13_DFLOW E2). These bind the
# editor UI (lev2.ui.OutlinerModel / PropertySheetModel) to ANY GraphDocument via
# its family-neutral surface — tree enumeration (tree_paths), editable_params /
# set_param, bypass/output badge STATE (queried from the base document surface),
# and the document-parameter table (E0). The canvas / outliner / property sheet /
# undo / MCP all bind these.
#
# Terrain derives its three models from the three below (TerrainDocOutlinerModel /
# TerrainNodePropertyModel / TerrainParamsPropertyModel) and keeps ONLY its
# family-specific bits as OVERRIDES — construct display-name annotations
# (loops/groups/switch) and its add-menu factory list. Everything else (the outliner
# index, badge rendering, the whole PropertySheetModel interface, the reflection-
# derived ENUM dropdowns, typed-literal unit display) lives here.
#
# This module imports NOTHING family-specific (no terrain / particles / hypermesh):
# it edits any family purely through the GraphDocument surface + lev2.ui.
################################################################################

from orkengine.core import vec2 as _vec2, vec3 as _vec3, vec4 as _vec4, VarMap as _VarMap
from orkengine import lev2

# the family-NEUTRAL base document module (NOT a family): supplies the reflection-
# derived enum-choice lookup so the property sheet's ENUM dropdowns need zero
# per-family label tables (E1).
from ork.hypergraph.dflow.document import enum_choices as _enum_choices
from ork.hypergraph.dflow.document import plug_meta as _plug_meta
from ork.hypergraph.dflow.document import prop_meta as _prop_meta

_ui = lev2.ui


################################################################################
# Shared property descriptor + type mapping (used by every property sheet below)
################################################################################

# the property-sheet Int editor round-trips through a C++ int32 codec. A reflected value
# outside that range (e.g. a uint32 bit-mask == 0xFFFFFFFF) cannot decode and would crash the
# C++ getValue boundary — the row-population path treats such a value as un-editable (read-only
# string) instead. int32 range == [-2^31, 2^31-1].
_INT32_MIN = -2147483648
_INT32_MAX = 2147483647


def _out_of_editor_range(v):
  return isinstance(v, int) and not isinstance(v, bool) and not (_INT32_MIN <= v <= _INT32_MAX)


def _ptype_for(v):
  if isinstance(v, bool):
    return _ui.PropertyType.Bool
  if isinstance(v, int):
    return _ui.PropertyType.Int
  if isinstance(v, float):
    return _ui.PropertyType.Float
  if isinstance(v, _vec2):
    return _ui.PropertyType.Vec2
  if isinstance(v, str):
    return _ui.PropertyType.String
  return None


class _Prop:
  __slots__ = ("key", "label", "ptype", "get", "set", "choices", "editable",
               "annotations")

  def __init__(self, key, label, ptype, get, set_, choices=None, editable=True,
               annotations=None):
    self.key = key
    self.label = label
    self.ptype = ptype
    self.get = get
    self.set = set_
    self.choices = choices
    self.editable = editable
    self.annotations = annotations    # a core.VarMap (e.g. min/max slider range) or None


################################################################################
# Outliner model
################################################################################

class GraphDocumentOutlinerModel(_ui.OutlinerModel):
  """An lev2.ui.OutlinerModel over ANY GraphDocument. Rows come from the document's
  tree_paths() (the SINGLE keying source shared with the canvas / session refs);
  display names, bypass ('B') and display-output ('O') badges come from the base
  document surface. getChildren/getDisplayName/hasChildren walk a cached index;
  object_for_key() hands the selected document object back to the property sheet.

  Family HOOKS (overridable, keeping the generic default for doc-less families):
    _display_name_for   — construct annotations (loops/groups/switch)
    _is_capture_key     — which rows the 'captures' visibility toggle hides
    getFactories/createItem — the add-menu (families with structural editing)."""

  def __init__(self, document=None):
    super().__init__()
    self.allow_rename = False
    self.allow_delete = False
    self.allow_add = False
    self.allow_multiselect = False
    self._document = None
    self._children = {"": []}
    self._objs = {}
    self._names = {}
    # select-as-output state lives on the RUNTIME (session); the host points this at
    # it so the display badge can render its active state. None = never active.
    self.display_key_provider = None
    # category visibility (outliner header toggle): sinks/captures are the bake's
    # OUTPUT CONTRACT — rarely edited — so a family may hide them as tree noise.
    self.show_captures = True
    # synthetic top-level rows shown ABOVE the document tree, e.g. "Terrain Parameters".
    # [(key, display_name)]. Persist across set_document.
    self._top_extras = []
    if document is not None:
      self.set_document(document)

  def set_document(self, document):
    self._document = document
    self._rebuild_index()
    self.notifyModelReset()

  def set_top_extras(self, entries):
    """Register synthetic top-level rows shown ABOVE the document tree. entries =
    [(key, display_name)]. These are NOT document objects — object_for_key() returns
    None for them; the host detects the key."""
    self._top_extras = list(entries)
    self.notifyModelReset()

  # ---- display-name hook -----------------------------------------------------

  def _display_name_for(self, obj, key):
    """The display string for `obj` at tree-path `key`. Generic default asks the
    document surface (display_name); families with structural constructs OVERRIDE to
    annotate them (loop x N / group / switch)."""
    if self._document is not None:
      nm = self._document.display_name(obj, key)
      if nm is not None:
        return nm
    return key.split("/")[-1]

  def _rebuild_index(self):
    # keying = document.tree_paths(), the SAME source the runtime uses for the display
    # key — no parallel id scheme.
    self._children = {"": []}
    self._objs = {}
    self._names = {}
    if self._document is not None:
      for (parent_key, key, obj) in self._document.tree_paths():
        self._objs[key] = obj
        self._names[key] = self._display_name_for(obj, key)
        self._children.setdefault(parent_key, []).append(key)
        self._children.setdefault(key, [])

  # ---- capture-visibility hook -----------------------------------------------

  def set_show_captures(self, visible):
    """Toggle capture-row visibility (the outliner 'Caps' header toggle)."""
    self.show_captures = bool(visible)
    self.notifyModelReset()
    return self.show_captures

  def _is_capture_key(self, key):
    """Whether the row at `key` is a hideable capture/sink. Generic default: nothing
    is a capture (the toggle is a no-op). Families override."""
    return False

  # ---- OutlinerModel interface ----------------------------------------------

  def getChildren(self, parent_key):
    if parent_key == "":
      keys = [k for (k, _n) in self._top_extras] + list(self._children.get("", []))
    else:
      keys = list(self._children.get(parent_key, []))
    if not self.show_captures:
      keys = [k for k in keys if not self._is_capture_key(k)]
    return keys

  def getDisplayName(self, key):
    for (k, n) in self._top_extras:
      if k == key:
        return n
    if key in self._names:
      return self._names[key]
    return key.split("/")[-1] if "/" in key else key

  def hasChildren(self, key):
    return len(self._children.get(key, [])) > 0

  def getValue(self, key):
    return None

  def getBadges(self, key):
    """Per-row toggle badges from the base document surface. bypass ('B', yellow) rides
    anything the document reports bypassable (leaf nodes AND structural constructs);
    display / select-as-output ('O', blue, exclusive) rides leaf nodes that can drive
    the display. Synthetic top rows and objects the document does not badge carry none."""
    obj = self._objs.get(key)
    if obj is None or self._document is None:
      return []
    badges = []
    bp = self._document.node_bypass_badge(obj)      # (enabled, active) or None
    if bp is not None:
      enabled, active = bp
      badges.append(_ui.OutlinerBadge(
          id="bypass", glyph="B",
          color=_vec4(0.85, 0.72, 0.15, 1.0),
          active=bool(active), enabled=bool(enabled)))
    ob = self._document.node_output_badge(obj)      # enabled bool, or None (no badge)
    if ob is not None:
      dk = self.display_key_provider() if self.display_key_provider is not None else None
      badges.append(_ui.OutlinerBadge(
          id="display", glyph="O",
          color=_vec4(0.25, 0.55, 0.95, 1.0),
          active=(dk == key), enabled=bool(ob)))
    return badges

  def getFactories(self, parent_key):
    """The add menu. Generic default: nothing (a doc-less family gets structural editing
    once its document implements add_node/add_loop). Families override."""
    return []

  def createItem(self, parent_key, name, factory_id):
    """Perform the document mutation for the add flow. Generic default: refuse (no add).
    Families override. '' == refusal."""
    return ""

  # ---- host helper -----------------------------------------------------------

  def object_for_key(self, key):
    """The document object a key selects, or None (top-extras / unknown)."""
    return self._objs.get(key)


################################################################################
# Property sheet base (shared PropertySheetModel plumbing)
################################################################################

class _PropSheetBase(_ui.PropertySheetModel):
  """Shared plumbing for every graph-document property sheet: an ordered [_Prop] list
  + the lev2.ui.PropertySheetModel interface over it. A subclass populates self._props
  (then rebuilds self._by_key) and calls notifyStructureChanged(). Write-error handling
  is a hook (families that re-trace / coerce catch + revert loudly)."""

  # exception types setValue() catches from a prop set(); () == catch nothing (the
  # exception propagates, exactly as an un-guarded setValue would).
  _set_error_types = ()

  def __init__(self, on_changed=None):
    super().__init__()
    self._props = []            # ordered [_Prop]
    self._by_key = {}
    self._on_changed = on_changed   # optional callback(key) after a document write

  def _handle_set_error(self, key, ex):
    """Handle an exception a prop set() raised that is in _set_error_types. Return True
    if handled (drop the write + revert the widget), False to re-raise. Default: unhandled."""
    return False

  # ---- PropertySheetModel interface -----------------------------------------

  def getChildren(self, parent_key):
    if parent_key == "":
      return [p.key for p in self._props]
    return []

  def getDisplayName(self, key):
    p = self._by_key.get(key)
    return p.label if p else key

  def hasChildren(self, key):
    return key == "" and len(self._props) > 0

  def getPropertyType(self, key):
    p = self._by_key.get(key)
    return p.ptype if p else _ui.PropertyType.Unknown

  def getValue(self, key):
    p = self._by_key.get(key)
    return p.get() if p else None

  def setValue(self, key, value):
    p = self._by_key.get(key)
    if p is None or not p.editable or p.set is None:
      # read-only (unsupported type / derived row) — revert the widget, drop the write.
      self.notifyExternalValueChanged(key)
      return
    try:
      p.set(value)
    except self._set_error_types as ex:
      if not self._handle_set_error(key, ex):
        raise
      return
    if self._on_changed is not None:
      self._on_changed(key)

  def getChoices(self, key):
    p = self._by_key.get(key)
    if p and p.choices is not None:
      return list(p.choices())
    return []

  def getAnnotations(self, key):
    p = self._by_key.get(key)
    return p.annotations if p else None


################################################################################
# Node property model (one selected node handle)
################################################################################

class GraphDocumentPropertyModel(_PropSheetBase):
  """Property sheet over ONE selected node handle of a GraphDocument. Rows come from
  document.editable_params(node); reads/writes route through document.get_param /
  set_param (owner law L2 — the derived GraphData is never touched). ENUM dropdowns are
  reflection-derived here (no per-family tables); structural-construct rows (loops /
  switches) are family OVERRIDES; the generic build handles the flat editable-param
  surface every family exposes."""

  def __init__(self, document=None, node=None, on_changed=None):
    super().__init__(on_changed=on_changed)
    self._document = document
    self._obj = None
    if node is not None:
      self.set_object(node)

  def set_document(self, document):
    self._document = document

  def set_object(self, obj):
    self._obj = obj
    self._props = []
    self._by_key = {}
    if obj is not None:
      self._build(obj)
    self._by_key = {p.key: p for p in self._props}
    self.notifyStructureChanged()

  # ---- descriptor build (family hook) ---------------------------------------

  def _build(self, node):
    """Populate self._props for `node`. Generic default: the flat editable-param surface.
    Families with structural constructs OVERRIDE to dispatch on their node types."""
    self._build_params(node)
    self._append_expr_field_rows(node)

  def _append_expr_field_rows(self, node):
    """Emit one detail-editor row per ExprIR expression field (E2.5 S7) — e.g. ExprForce's
    force_x/force_y/force_z. The row carries editor.custom == 'expr' (the propsheet shows an
    Edit button routing to the host's CodeView detail editor) + expr.context (the vocabulary
    the host validates against). get returns the author SOURCE (the stored tree pretty-
    printed); set parses+validates author SOURCE and writes the canonical tree back through
    the document (loud on an invalid/dishonest edit — the document is untouched)."""
    if self._document is None:
      return
    try:
      fields = self._document.expr_fields(node)
    except Exception:
      return
    for (field, context_name) in fields:
      ann = _VarMap()
      ann.__setattr__("editor.custom", "expr")
      ann.__setattr__("expr.context", context_name)
      self._props.append(_Prop(
          field, f"{field} [{context_name}]", _ui.PropertyType.String,
          get=lambda o=node, f=field: self._document.expr_field_source(o, f),
          set_=lambda v, o=node, f=field: self._document.set_expr_field(o, f, v),
          annotations=ann))

  def _build_params(self, node):
    used = set()
    for (kind, name, value) in self._document.editable_params(node):
      labels = self._enum_labels(node, kind, name, value)
      if labels is not None and isinstance(value, int) and not isinstance(value, bool):
        # int-coded selector -> ENUM widget (get/set trade the LABEL string; the document
        # keeps the int code).
        key = name if name not in used else f"{name}#{kind}"
        used.add(name)
        self._props.append(_Prop(
            key, name, _ui.PropertyType.Enum,
            get=lambda k=kind, n=name, o=node, L=labels: self._enum_label(o, k, n, L),
            set_=lambda v, k=kind, n=name, o=node, L=labels: self._document.set_param(
                o, k, n, L.index(v) if v in L else int(v)),
            choices=lambda L=labels: list(L)))
        continue
      # VEC3 input plug -> three per-component SLIDER rows (owner: a slider each for
      # x/y/z, "when not plugged in"). When the plug is connected all three ghost as a
      # unit (read_only) via the same machinery; each edit recomposes the vec3 and writes
      # through the document edit path. Generic for any vec3 plug — no family special-casing.
      if kind == "inputs" and isinstance(value, (list, tuple)) and len(value) == 3 \
         and all(isinstance(c, (int, float)) for c in value):
        self._append_vec3_component_rows(node, kind, name)
        continue
      ptype = _ptype_for(value)
      if ptype is None or ptype == _ui.PropertyType.Vec2 or _out_of_editor_range(value):
        # no clean scalar editor for this value -> read-only string so nothing is silently
        # dropped (ops-self-defend). Covers: an unsupported type; a vec2; AND a value the
        # property-sheet scalar codec cannot round-trip (a uint32 bit-mask like SelectData.
        # unsel_and == 0xFFFFFFFF overflows the Int editor's int32 codec and would crash the
        # C++ getValue boundary). Surfacing it read-only keeps the value HONEST + un-editable
        # rather than exploding the row-population path (loud log names the offender).
        if _out_of_editor_range(value):
          print("[propsheet] %s.%s value %r out of the int32 editor range — read-only row"
                % (kind, name, value), flush=True)
        vann, vlabel = self._plug_annotations(node, kind, name)
        self._props.append(_Prop(
            f"{name}", vlabel, _ui.PropertyType.String,
            get=lambda v=value: str(v), set_=None, editable=False,
            annotations=vann))
        continue
      key = name if name not in used else f"{name}#{kind}"
      used.add(name)
      # E1 plug metadata (reflection-derived): a clamped-slider range + a display-only
      # (bake-inert) marker, from dflow.plugSpec() — no per-family tables. A missing/
      # engine-not-ready lookup leaves the row exactly as before (plain unbounded editor).
      ann, label = self._plug_annotations(node, kind, name)
      self._props.append(_Prop(
          key, label, ptype,
          get=lambda k=kind, n=name, o=node: self._document.get_param(o, k, n),
          set_=lambda v, k=kind, n=name, o=node: self._document.set_param(o, k, n, v),
          annotations=ann))
      # A FloatXf-typed plug carries an input-transformer: emit an always-editable row per
      # transformer item field DIRECTLY BELOW the (possibly ghosted) value row (owner
      # requirement 2). These stay editable when the value is connected/ghosted.
      self._append_transformer_rows(node, kind, name)

  # default visible slider range for a vec3 component when the plug declares none — a
  # symmetric span that gives usable slider travel for typical small force/offset vectors.
  _VEC3_DEFAULT_RANGE = (-10.0, 10.0)

  def _append_vec3_component_rows(self, node, kind, name):
    """Emit three Float slider rows (name.x / name.y / name.z) for a vec3 input plug. Each
    reads/writes ONE component (recomposing the vec3 through set_param — the honest edit
    path, rebake follows); each honors the plug's declared range if present, else the
    visible default. A connected plug ghosts all three (read_only) as a unit."""
    connected = self._is_plug_connected(node, kind, name)
    rmin, rmax = self._VEC3_DEFAULT_RANGE
    cn = self._node_class_name(node)
    if cn is not None:
      meta = _plug_meta(cn, name)
      if meta and "min" in meta and "max" in meta:
        rmin, rmax = float(meta["min"]), float(meta["max"])
    for i, ax in enumerate(("x", "y", "z")):
      ann = _VarMap()
      ann.min = rmin
      ann.max = rmax
      # each vec3 component IS a plug row -> the socket glyph (filled when the vec3 plug is
      # connected — all three ghost as a unit). No '[inputs]' text decorator.
      ann.row_plug = True
      ann.plug_connected = bool(connected)
      if connected:
        ann.read_only = True
      label = f"{name}.{ax}"
      self._props.append(_Prop(
          f"{name}#v3#{ax}", label, _ui.PropertyType.Float,
          get=lambda k=kind, n=name, ix=i, o=node: float(self._document.get_param(o, k, n)[ix]),
          set_=lambda v, k=kind, n=name, ix=i, o=node: self._vec3_set_component(o, k, n, ix, v),
          annotations=ann))

  def _vec3_set_component(self, node, kind, name, ix, v):
    cur = list(self._document.get_param(node, kind, name))
    cur[ix] = float(v)
    self._document.set_param(node, kind, name, _vec3(cur[0], cur[1], cur[2]))

  def _append_transformer_rows(self, node, kind, name):
    """Emit one always-editable row per transformer item field for a FloatXf-typed input
    plug. The base sheet model is flat, so rows are keyed '<plug>#xf#<item>#<field>' and
    labelled '<plug> -> <itemtype>.<field>' — the Transform group reads as a titled
    sub-block. Rows edit the LIVE reflected item -> round-trips to the serialized GraphData
    and re-parameterizes the bake. No-op when the plug carries no transformer."""
    if kind != "inputs" or self._document is None:
      return
    try:
      xf = self._document.plug_transformer(node, name)
    except Exception:
      xf = None
    if xf is None:
      return
    try:
      items = list(xf.items())     # [(item_name, item)] — the floatxfdata len/items binding
    except Exception:
      return
    for (iname, item) in items:
      itype = type(item).__name__
      for (attr, aval) in self._xf_item_fields(item):
        ptype = _ptype_for(aval)
        if ptype is None:
          continue
        self._props.append(_Prop(
            f"{name}#xf#{iname}#{attr}", f"{name} -> {itype}.{attr}", ptype,
            get=lambda it=item, a=attr: getattr(it, a),
            set_=lambda v, it=item, a=attr: setattr(it, a, v)))

  @staticmethod
  def _xf_item_fields(item):
    """[(attr, value)] of a transformer item's editable scalar/bool fields — the pybound
    do_* enable toggles and numeric params. Introspected (no per-type vocabulary table);
    non-scalar handles (a curve's multicurve) are skipped (its detail editor is deferred)."""
    out = []
    for attr in dir(item):
      if attr.startswith("_"):
        continue
      try:
        val = getattr(item, attr)
      except Exception:
        continue
      if isinstance(val, (bool, float)):
        out.append((attr, val))
    return out

  def _plug_annotations(self, node, kind, name):
    """(annotations_varmap_or_None, label) for a scalar row: a min/max slider range,
    a '(bake-inert)' label suffix, and — for input plugs — the socket-glyph annotation
    (row_plug + plug_connected) the C++ sheet draws in the label column. 'inputs' rows
    source plugSpec metadata (plug_meta, E1); 'module' rows source the reflected PROPERTY's
    describeX annotations (prop_meta, E1-close: editor.range.min/max — e.g. FlowErode.blend).
    Lookups before the engine is up return the plain (None, bare-name label).

    LABELS carry NO '[inputs]'/'[module]' kind decorator any more — a PLUG row is marked by
    its socket glyph (hollow ring = editable/unconnected, filled disc = connected/ghosted),
    a MODULE-property row by the ABSENCE of one (that absence IS the distinction)."""
    label = name
    ann = None
    cn = self._node_class_name(node)
    if cn is not None:
      if kind == "inputs":
        meta = _plug_meta(cn, name)
      elif kind == "module":
        meta = _prop_meta(cn, name)
      else:
        meta = None
      if meta:
        if "min" in meta and "max" in meta:
          ann = _VarMap()
          ann.min = float(meta["min"])
          ann.max = float(meta["max"])
        if meta.get("display_only"):
          label = f"{name} (bake-inert)"   # honest: shown + editable, no bake effect
    # PLUG ROW: stamp the socket glyph (drawn by property_sheet.cpp in the label column) +
    # its connected state. A connected plug's value is driven upstream, so ALSO stamp
    # read_only (the C++ sheet dims the row + refuses the editor, owner requirement 1) — the
    # FILLED socket disc reads the connection; no text suffix needed. The plug's Transform
    # sub-rows stay editable — emitted separately, unstamped. Module rows get NO glyph.
    if kind == "inputs":
      if ann is None:
        ann = _VarMap()
      ann.row_plug = True
      connected = self._is_plug_connected(node, kind, name)
      ann.plug_connected = bool(connected)
      if connected:
        ann.read_only = True
    return ann, label

  def _is_plug_connected(self, node, kind, name):
    """Whether an 'inputs' plug is fed by an edge (via the document surface). A doc-less
    node model (terrain binds a bare node, no document) or a family without live plugs
    reports not-connected — the value row stays editable, unchanged."""
    if kind != "inputs" or self._document is None:
      return False
    try:
      return bool(self._document.plug_is_connected(node, name))
    except Exception:
      return False

  # ---- ENUM hooks (reflection-derived; families need no label tables) -------

  def _node_class_name(self, node):
    """The node's DSL/reflected class name — asks the document surface first, then
    falls back to the node handle's own attribute (terrain's node-property model binds
    a bare object, no document)."""
    if self._document is not None:
      cn = self._document.node_class_name(node)
      if cn is not None:
        return cn
    return getattr(node, "clazz_name", None)

  def _enum_labels(self, node, kind, name, value):
    """VALUE-ORDERED label tuple for a reflected-enum module property, or None (not an
    enum -> a normal scalar widget). Derived from the C++ EnumSerializer via reflection
    (E1) — the single source, no per-family label tables. Only 'module' scalars carry
    enums (plug 'inputs' never do)."""
    if kind != "module":
      return None
    cn = self._node_class_name(node)
    if cn is None:
      return None
    return _enum_choices(cn, name)

  def _enum_label(self, node, kind, name, labels):
    """Current int code -> its label (out-of-range shows the raw code, read-truth)."""
    v = self._document.get_param(node, kind, name)
    i = int(v) if v is not None else 0
    return labels[i] if 0 <= i < len(labels) else str(i)


################################################################################
# Testbench property model (the DISTINCT bench section)
################################################################################

class BenchPropertyModel(_PropSheetBase):
  """Property sheet over an asset's TESTBENCH — a DISTINCT bench section, visually separated
  from the DUT's node properties (no new widget class; the same PropertySheetModel idiom as
  every other graph-document sheet). Editing semantics follow the outside-in law:

    * `enabled` (standard on every bench) rides the HOST's honest re-instantiation (rebake) —
      toggling it changes the EDITING instantiation (emitter_entity "@bench" <-> static), so the
      rebuild counter bumps, exactly like any structural edit.
    * motion params (radius / period / height / ...) are LIVE — they re-parameterize the pure
      frame->transform motion program the editor evaluates per transport tick; NO rebake, NO
      graph touch. The next tick moves the emitter.

  The shell must NOT auto-rebake on THIS model's changes (the model owns its own rebake policy);
  DflowEditor._onPropsheetChanged skips the generic rebake when the bound model is a BenchPropertyModel."""

  def __init__(self, testbench, host, on_changed=None):
    super().__init__(on_changed=on_changed)
    self._bench = testbench
    self._host = host
    self._build()

  def _build(self):
    self._props = []
    tb = self._bench
    if tb is not None:
      self._props.append(_Prop(
          "enabled", "enabled [bench]", _ui.PropertyType.Bool,
          get=lambda: bool(self._host.bench_enabled),
          set_=lambda v: self._host.setBenchEnabled(bool(v))))    # -> rebake (rebuild counter++)
      prog = tb.primary_program()
      if prog is not None:
        for (pname, _pv) in prog.params():
          self._props.append(_Prop(
              f"motion.{pname}", f"{pname} [motion]", _ui.PropertyType.Float,
              get=lambda p=prog, n=pname: float(dict(p.params()).get(n, 0.0)),
              set_=lambda v, p=prog, n=pname: p.set_param(n, float(v))))  # LIVE: no rebake
    self._by_key = {p.key: p for p in self._props}
    self.notifyStructureChanged()


################################################################################
# Document-parameters property model (the E0 params table)
################################################################################

class GraphDocumentParamsPropertyModel(_PropSheetBase):
  """Property sheet over a GraphDocument's document-parameter table (E0). One row per
  param; a TYPED-LITERAL param (its default was e.g. meters(2000)) shows its unit in the
  row LABEL ('amplitude (meters)'). Scalar values edit in place; a numeric tuple splits
  into per-component Float rows. Families with extra session knobs (terrain's dim /
  extent / texel) OVERRIDE refresh() to prepend them, and override _param_get/_param_set
  to route through a runtime instead of the raw table."""

  def __init__(self, document=None, on_changed=None):
    super().__init__(on_changed=on_changed)
    self._document = None
    if document is not None:
      self.set_document(document)

  def set_document(self, document):
    self._document = document
    self.refresh()

  def refresh(self):
    self._props = []
    self._build_param_rows()
    self._by_key = {p.key: p for p in self._props}
    self.notifyStructureChanged()

  def _build_param_rows(self):
    pt = self._document.params if self._document is not None else None
    if pt is None:
      return
    for name in pt.names():
      self._add_rows(name, pt.get(name), pt.tag_of(name))

  # ---- descriptor build ------------------------------------------------------

  def _add_rows(self, name, value, tag=None):
    # E0 part 2: a TAGGED param shows its unit in the label; the value still edits as a
    # plain scalar (a full unit-aware widget is future work).
    label = f"{name} ({tag})" if tag else name
    if isinstance(value, tuple):
      # numeric tuple -> one editable Float per component (center[0], center[1]).
      for i in range(len(value)):
        self._props.append(_Prop(
            f"{name}[{i}]", f"{label}[{i}]", _ui.PropertyType.Float,
            get=lambda n=name, ix=i: float(self._param_get(n)[ix]),
            set_=lambda v, n=name, ix=i: self._set_component(n, ix, v)))
      return
    ptype = _ptype_for(value)
    if ptype is None or ptype == _ui.PropertyType.Vec2:
      # no scalar editor for this type -> read-only string (nothing silently dropped).
      self._props.append(_Prop(
          name, label, _ui.PropertyType.String,
          get=lambda n=name: str(self._param_get(n)), set_=None, editable=False))
      return
    self._props.append(_Prop(
        name, label, ptype,
        get=lambda n=name: self._param_get(n),
        set_=lambda v, n=name: self._param_set(n, v)))

  def _set_component(self, name, ix, v):
    cur = list(self._param_get(name))
    cur[ix] = float(v)
    self._param_set(name, tuple(cur))

  # ---- param source (families override to route through a runtime) ----------

  def _param_get(self, name):
    return self._document.params.get(name)

  def _param_set(self, name, value):
    self._document.params.set(name, value)
