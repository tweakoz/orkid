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
# (loops/groups/switch), its int-coded ENUM label table, and its add-menu factory
# list. Everything else (the outliner index, badge rendering, the whole
# PropertySheetModel interface, typed-literal unit display) lives here.
#
# This module imports NOTHING family-specific (no terrain / particles / hypermesh):
# it edits any family purely through the GraphDocument surface + lev2.ui.
################################################################################

from orkengine.core import vec2 as _vec2, vec4 as _vec4
from orkengine import lev2

_ui = lev2.ui


################################################################################
# Shared property descriptor + type mapping (used by every property sheet below)
################################################################################

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
  set_param (owner law L2 — the derived GraphData is never touched). ENUM label tables
  and structural-construct rows (loops / switches) are family OVERRIDES; the generic
  build handles the flat editable-param surface every family exposes."""

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
            key, f"{name} [{kind}]", _ui.PropertyType.Enum,
            get=lambda k=kind, n=name, o=node, L=labels: self._enum_label(o, k, n, L),
            set_=lambda v, k=kind, n=name, o=node, L=labels: self._document.set_param(
                o, k, n, L.index(v) if v in L else int(v)),
            choices=lambda L=labels: list(L)))
        continue
      ptype = _ptype_for(value)
      if ptype is None or ptype == _ui.PropertyType.Vec2:
        # no scalar editor for this type -> read-only string so nothing is silently
        # dropped (ops-self-defend).
        self._props.append(_Prop(
            f"{name}", f"{name} [{kind}]", _ui.PropertyType.String,
            get=lambda v=value: str(v), set_=None, editable=False))
        continue
      key = name if name not in used else f"{name}#{kind}"
      used.add(name)
      self._props.append(_Prop(
          key, f"{name} [{kind}]", ptype,
          get=lambda k=kind, n=name, o=node: self._document.get_param(o, k, n),
          set_=lambda v, k=kind, n=name, o=node: self._document.set_param(o, k, n, v)))

  # ---- ENUM hooks (families provide their label tables) ---------------------

  def _enum_labels(self, node, kind, name, value):
    """Ordered label tuple for an int-coded selector param, or None (no ENUM widget).
    Generic default: None (labels derive from reflection once E1 wires it). Families
    override with their (class,param)->labels table."""
    return None

  def _enum_label(self, node, kind, name, labels):
    """Current int code -> its label (out-of-range shows the raw code, read-truth)."""
    v = self._document.get_param(node, kind, name)
    i = int(v) if v is not None else 0
    return labels[i] if 0 <= i < len(labels) else str(i)


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
