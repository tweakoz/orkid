################################################################################
# Terrain DOCUMENT models for the editor UI (JUL09 S1; hoisted onto the generic
# GraphDocument editor models, JUL13_DFLOW E2 slice 2).
#
# The three models below DERIVE from ork.editor.graphdoc_models (the family-neutral
# editor core) and keep ONLY terrain-specific behavior as overrides:
#
#  - TerrainDocOutlinerModel: adds construct display-name annotations (loop x N /
#    group / switch), the capture-visibility test, and the add-menu factories. The
#    tree index + bypass/output badges come from the generic model (badge STATE is
#    read from TerrainDoc.node_bypass_badge / node_output_badge).
#  - TerrainNodePropertyModel: adds the DocLoop.count / DocSwitch.selected /
#    L.i-read-only rows; the flat-node scalar rows, the ENUM dropdowns (labels
#    derive from C++ reflection, E1), and the whole PropertySheetModel interface
#    are generic.
#  - TerrainParamsPropertyModel: routes the generic params-table rows through the
#    TerrainRuntime's DSL ctor kwargs and adds the session dim/extent/texel rows.
#
# All writes still go through the document/runtime mutation API (owner law L2) — the
# derived GraphData is never touched.
################################################################################

from orkengine.core import VarMap
from orkengine import lev2

from ork.hypergraph.dflow.terrain.doc import (
    DocNode, DocLoop, DocGroupCall, DocSwitch, tree_paths,
    TerrainDocParamError, editor_add_ops, add_op_node, add_loop, add_into_loop)
from ork.editor.graphdoc_models import (
    GraphDocumentOutlinerModel, GraphDocumentPropertyModel,
    GraphDocumentParamsPropertyModel, _Prop, _ptype_for)

_ui = lev2.ui

# outliner key of the synthetic top-level "Terrain Parameters" row (the DSL ctor
# kwargs). A leading NUL keeps it distinct from any deterministic doc path.
TERRAIN_PARAMS_KEY = "\x00terrain_params"


################################################################################
# Outliner model
################################################################################

class TerrainDocOutlinerModel(GraphDocumentOutlinerModel):
  """Read-only outliner over the terrain document tree (v1 — structural editing is
  S3). Derives the tree index + bypass/output badges from the generic model; overrides
  the construct display-name annotations, the capture-visibility test, and the add-menu
  factories."""

  def __init__(self, document=None):
    super().__init__(document=None)
    self.allow_add = True      # Shift+Enter add flow (factories = the curated op menu)
    if document is not None:
      self.set_document(document)

  # ---- construct display-name annotations (terrain hook) --------------------

  def _display_name_for(self, obj, key):
    seg = key.split("/")[-1]
    if isinstance(obj, DocNode):
      return f"{obj.local_name}  [{obj.clazz_name}]"
    if isinstance(obj, DocLoop):
      return f"{obj.path}  (loop x{obj.count})"
    if isinstance(obj, DocGroupCall):
      return f"{obj.path}  (group {obj.func_name})"
    if isinstance(obj, DocSwitch):
      return f"{seg}  (switch -> {obj.selected})"
    return seg

  # ---- capture visibility (terrain hook) ------------------------------------

  def _is_capture_key(self, key):
    obj = self._objs.get(key)
    return isinstance(obj, DocNode) and obj.clazz_name == "CaptureModule"

  # ---- add menu (terrain hook) ----------------------------------------------

  def getFactories(self, parent_key):
    """The add menu (Shift+Enter on a row; shift/ctrl+arrows cycle the factory):
    a DocNode row inserts AFTER it (chain semantics; 'loop' wraps its output in an
    empty identity loop); a LOOP row appends INTO its body. Captures/switches and
    the synthetic top rows offer nothing."""
    obj = self._objs.get(parent_key)
    if isinstance(obj, DocNode) and obj.clazz_name != "CaptureModule":
      names = list(editor_add_ops()) + ["loop"]
    elif isinstance(obj, DocLoop):
      names = list(editor_add_ops())
    else:
      return []
    return [{"id": n, "display_name": n,
             "default_name_generator": (lambda m, nn=n: nn)} for n in names]

  def createItem(self, parent_key, name, factory_id):
    """Perform the document mutation for the add flow. A name left as the factory
    default means 'auto' (anon, uniquified). Returns the new row's key ('' on
    refusal — refusals print loudly)."""
    obj = self._objs.get(parent_key)
    custom = name if (name and name != factory_id) else None
    try:
      if isinstance(obj, DocLoop):
        node = add_into_loop(self._document, obj, factory_id, name=custom)
      elif factory_id == "loop":
        node = add_loop(self._document, obj)
      else:
        node = add_op_node(self._document, factory_id, obj, name=custom)
    except TerrainDocParamError as ex:
      print(f"[terrain] {ex}", flush=True)
      return ""
    self.set_document(self._document)     # rebuild the key index (+ model reset)
    for (_pk, k, o) in tree_paths(self._document):
      if o is node:
        return k
    return ""


################################################################################
# Property model (one selected document object)
################################################################################

class TerrainNodePropertyModel(GraphDocumentPropertyModel):
  """Property sheet over ONE selected document object. Derives the PropertySheetModel
  interface + flat-node scalar rows from the generic model; overrides the descriptor
  build to dispatch on the terrain object types (DocNode with reflection-derived ENUM
  dropdowns + L.i rows, DocLoop count, DocSwitch selected). All writes route through the
  document mutation API (L2)."""

  def __init__(self, obj=None, on_changed=None):
    super().__init__(document=None, node=None, on_changed=on_changed)
    if obj is not None:
      self.set_object(obj)

  # ---- descriptor build (terrain object-type dispatch) ----------------------

  def _build(self, obj):
    if isinstance(obj, DocNode):
      self._build_node(obj)
    elif isinstance(obj, DocLoop):
      self._props.append(_Prop(
          "count", "count", _ui.PropertyType.Int,
          get=lambda o=obj: int(o.count),
          set_=lambda v, o=obj: o.set_count(v)))
    elif isinstance(obj, DocSwitch):
      self._props.append(_Prop(
          "selected", "selected", _ui.PropertyType.Enum,
          get=lambda o=obj: str(o.selected),
          set_=lambda v, o=obj: o.select(str(v)),
          choices=lambda o=obj: [str(b) for b in o.branches.keys()]))
    # DocGroupCall: no directly-editable params in v1 (its inner nodes are edited
    # individually via the outliner) — an empty sheet is correct.

  def _build_node(self, node):
    used = set()
    # S7 expression-source fields (ExprModule.expr_source) are surfaced as CodeView detail
    # rows below, NOT as plain module lineedits — exclude them from the flat param sweep.
    expr_fields = node.doc.expr_fields(node) if getattr(node, "doc", None) is not None else []
    expr_names = {f for (f, _c) in expr_fields}
    for (kind, name, value) in node.editable_params():
      if kind == "module" and name in expr_names:
        continue
      labels = self._enum_labels(node, kind, name, value)   # reflection-derived (E1)
      if labels is not None and isinstance(value, int) and not isinstance(value, bool):
        # int-coded selector -> ENUM widget (the ecsedit idiom: get/set trade in the
        # LABEL string, choices() lists them; the doc keeps recording the int code).
        key = name if name not in used else f"{name}#{kind}"
        used.add(name)
        self._props.append(_Prop(
            key, name, _ui.PropertyType.Enum,
            get=lambda k=kind, n=name, o=node, L=labels: self._enum_label(o, k, n, L),
            set_=lambda v, k=kind, n=name, o=node, L=labels: o.set_param(
                k, n, L.index(v) if v in L else int(v)),
            choices=lambda L=labels: list(L)))
        continue
      ptype = _ptype_for(value)
      if ptype is None or ptype == _ui.PropertyType.Vec2:
        # v1 propsheet has no Vec2 editor — show read-only so nothing is silently
        # dropped (ops-self-defend). Still carry the display-only (bake-inert)
        # label so e.g. a vec2 offset_vel row reads honestly distinct (E1).
        _ann, vlabel = self._plug_annotations(node, kind, name)
        self._props.append(_Prop(
            f"{name}", vlabel, _ui.PropertyType.String,
            get=lambda v=value: str(v), set_=None, editable=False))
        continue
      key = name if name not in used else f"{name}#{kind}"
      used.add(name)
      # reflection-derived row metadata (generic seam, E1/E1-close): plug rows get
      # plugSpec ranges + bake-inert labels; 'module' rows get the property's
      # editor.range.min/max annotations (e.g. FlowErode.blend) — no hand tables.
      ann, label = self._plug_annotations(node, kind, name)
      self._props.append(_Prop(
          key, label, ptype,
          get=lambda k=kind, n=name, o=node: self._read_param(o, k, n),
          set_=lambda v, k=kind, n=name, o=node: o.set_param(k, n, v),
          annotations=ann))
    for (kind, name, expr_str, i0) in node.iter_param_view():
      self._props.append(_Prop(
          f"{name}#iter", f"{name} (L.i, read-only)", _ui.PropertyType.String,
          get=lambda s=expr_str, x=i0: f"{s}   [i0={x:g}]",
          set_=None, editable=False))
    # E0 DOCUMENT-PARAMETER-driven plugs: read-only here (edit the referenced Terrain
    # Parameter, not the node plug). editable_params() excludes them, so without this row
    # a plug carrying an expression would be silently dropped from the sheet — show it
    # instead (topology honesty; the value tracks the parameter).
    for (kind, name, expr_str, val) in node.param_expr_view():
      # doc-param-driven: GHOST it (read_only) so the C++ sheet dims the row + refuses the
      # editor. set_param refusal is BY DESIGN (edit the referenced Terrain Parameter, not
      # the node plug) — the ghost surfaces that honestly, matching the connected-plug split.
      dp_ann = VarMap()
      dp_ann.read_only = True
      self._props.append(_Prop(
          f"{name}#expr", f"{name} (= {expr_str}, doc-param driven)", _ui.PropertyType.String,
          get=lambda s=expr_str, x=val: f"{s}   [= {x:g}]",
          set_=None, editable=False, annotations=dp_ann))
    self._append_terrain_expr_rows(node, expr_fields)

  def _append_terrain_expr_rows(self, node, expr_fields):
    """Emit one detail-editor row (editor.custom == 'expr') per terrain expression-source field
    (S7 — ExprModule.expr_source). get returns the author SOURCE; set VALIDATES (compile) +
    writes through the doc edit path (loud on an invalid edit — the document is untouched; the
    host's CodeView surfaces the reason). The row carries expr.context naming the terrain source
    form. The transformer/expr sub-rows keep their [context] label form (deliverable-1 restraint)."""
    for (field, context_name) in expr_fields:
      ann = VarMap()
      ann.__setattr__("editor.custom", "expr")
      ann.__setattr__("expr.context", context_name)
      self._props.append(_Prop(
          field, f"{field} [{context_name}]", _ui.PropertyType.String,
          get=lambda o=node, f=field: o.doc.expr_field_source(o, f),
          set_=lambda v, o=node, f=field: o.doc.set_expr_field(o, f, v),
          annotations=ann))

  @staticmethod
  def _read_param(node, kind, name):
    for (k, n, v) in reversed(node.param_actions):
      if k == kind and n == name:
        return v
    return None

  @classmethod
  def _enum_label(cls, node, kind, name, labels):
    """Current int code -> its enum label (out-of-range shows the raw code, read-truth)."""
    v = cls._read_param(node, kind, name)
    i = int(v) if v is not None else 0
    return labels[i] if 0 <= i < len(labels) else str(i)


################################################################################
# Terrain Parameters property model (the DSL constructor kwargs)
################################################################################

class TerrainParamsPropertyModel(GraphDocumentParamsPropertyModel):
  """Property sheet over a TerrainRuntime's DSL constructor kwargs — the top-level
  "Terrain Parameters" (the motivating case: warp's frequency / octaves / ring_amp_m /
  ring_period_m / center are ctor kwargs, not plug-backed doc params). Derives the
  params-table row-building (incl. typed-literal unit labels + per-component tuple rows)
  from the generic model; routes _param_get/_param_set through the runtime, and prepends
  the session dim/extent/texel rows. Editable float/int/bool/str route through
  runtime.set_dsl_kwarg (RE-TRACES the class and replaces the document, L2). Empty for a
  doc-JSON session (no DSL source)."""

  _set_error_types = (TerrainDocParamError,)

  def __init__(self, runtime=None, on_changed=None):
    super().__init__(document=None, on_changed=on_changed)
    self._runtime = None
    if runtime is not None:
      self.set_runtime(runtime)

  def set_runtime(self, runtime):
    self._runtime = runtime
    self.refresh()

  def refresh(self):
    self._props = []
    if self._runtime is not None:
      # SESSION rows (present in every session, DSL or doc-JSON): the editing/
      # preview bake resolution (square W=H; runtime clamps + chunk-rounds).
      dim_ann = VarMap()             # slider range (the Int editor defaults to 0..100)
      dim_ann.min = 128
      dim_ann.max = 16384
      self._props.append(_Prop(
          "dim", "dim (bake W=H)", _ui.PropertyType.Int,
          get=lambda: int(self._runtime.preview_dim),
          set_=lambda v: self._runtime.set_preview_dim(v),
          annotations=dim_ann))
      # the DIM's physical scalar (natural units): heights are meters, so extent is
      # the ONLY world-scale knob — texel_m = extent_m / dim rides along read-only.
      ext_ann = VarMap()
      ext_ann.min = 128
      ext_ann.max = 131072
      self._props.append(_Prop(
          "extent_m", "extent (m, world XZ)", _ui.PropertyType.Float,
          get=lambda: float(self._runtime.extent_m),
          set_=lambda v: self._runtime.set_extent_m(v),
          annotations=ext_ann))
      self._props.append(_Prop(
          "texel_m", "texel (m, derived)", _ui.PropertyType.String,
          get=lambda: "%.4f" % (self._runtime.extent_m / max(1, self._runtime.preview_dim)),
          set_=None, editable=False))
    kwargs = self._runtime.editable_dsl_kwargs() if self._runtime is not None else {}
    units = (self._runtime.dsl_kwarg_units()
             if self._runtime is not None and hasattr(self._runtime, "dsl_kwarg_units")
             else {})
    for name, value in kwargs.items():
      self._add_rows(name, value, units.get(name))
    self._by_key = {p.key: p for p in self._props}
    self.notifyStructureChanged()

  # ---- param source: route through the runtime's DSL kwargs (terrain hook) --

  def _param_get(self, name):
    return self._runtime.editable_dsl_kwargs()[name]

  def _param_set(self, name, value):
    return self._runtime.set_dsl_kwarg(name, value)

  # ---- loud-refusal on a rejected edit (terrain hook) -----------------------

  def _handle_set_error(self, key, ex):
    # ops-self-defend: set_dsl_kwarg refuses an edit that would discard document edits
    # (E0 interim guard) or an uncoercible value. Surface it LOUDLY and revert the widget
    # — never let it propagate across the C++ property-sheet setValue trampoline (no
    # try/catch there; an escaping exception would crash the editor).
    print(f"[terrain] Terrain Parameter rejected: {ex}", flush=True)
    self.notifyExternalValueChanged(key)
    return True
