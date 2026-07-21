################################################################################
# TerrainRuntime — editor-independent terrain document runtime (JUL09 S1, ECS-hosted).
#
# Owns the structured terrain DOCUMENT (the single source of truth, owner law L2),
# its physical scale, and DISPLAY via a minimal in-code ECS scene. Every edit
# mutates the DOCUMENT (doc.set_param / DocLoop.set_count / DocSwitch.select), then
# a re-elaboration derives a fresh GraphData that is EMBEDDED into the terrain
# entity's HeightFieldGenData; the C++ terrain path (TerrainChunkDrawableData)
# bakes + displays it NATIVELY — no Python-side bake_heightfield in the display
# loop, no CPU chunk plumbing, no heights re-upload (that raw parity path is what
# rotted the runtime u_dim upload; the C++ path owns it).
#
# Two consumers share ONE runtime: ork.terrain.edit.py (live-hosted, deferred sim
# rebuild on edit) and ork.terrain.viewer2.py (thin window). A headless snapshot
# gate exports the in-code scene to JSON and renders it via ork.ecs.player.exe.
#
# rebake(dim) remains as the headless DOCUMENT ORACLE (elaborate + bake_heightfield
# -> per-channel image) — the gA determinism/cache gates run against it; it is NOT
# part of the display loop.
################################################################################

import inspect
import math
import os

from orkengine.core import vec3, Path as _Path
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.dflow.terrain.resolve import resolve_dsl_file, load_dsl_class
from ork.hypergraph.dflow.terrain.doc import (
    from_json as _doc_from_json, to_json as _doc_to_json, TerrainDocParamError)
from ork.hypergraph.dflow.terrain.base import HeightField as _HeightFieldBase
from ork.hypergraph.dflow import terrain as _T

# --- #88 display-path trace (ORKID_DISPLAY_TRACE=1). Every link of the display-switch
# chain (canvas click -> model.set_output -> set_display_key -> edit -> host route ->
# pre-cook -> swap -> rebind) prints ONE line, so a live session names the link that
# breaks — offscreen replicas of the owner's sequence pass while the live session fails,
# and only the session itself can say where the chain stops.
_DISPLAY_TRACE = bool(os.environ.get("ORKID_DISPLAY_TRACE"))


def display_trace(msg):
  if _DISPLAY_TRACE:
    print(f"[disptrace] {msg}", flush=True)
from ork.hypergraph import units as _units


class MinimalTerrain(_HeightFieldBase):
  """The `new` document: one fbm into height+normal captures — the minimal VIABLE
  terrain (bakes + displays out of the box). frequency/octaves surface as Terrain
  Parameters; grow the graph with the outliner add flow (Shift+Enter) and save as
  doc-JSON (the .py writer is S3)."""
  EXTENT_M = 4096.0

  def __init__(self, frequency=6.0, octaves=6, amplitude_m=400.0):
    super().__init__()
    # capture the fbm DIRECTLY (no remap chain): heights are TRUE METERS, so the
    # fbm amplitude IS the terrain height in meters (the hash-lattice fbm is
    # [0,1)-normalized before amplitude) — the leanest possible outliner.
    h = _T.Fbm(frequency=float(frequency), octaves=int(octaves),
               amplitude=float(amplitude_m))
    self.capture(h, "height", cache=True)
    self.capture(h, "normal", cache=True)


################################################################################
# [M] terrain material-override cycle — the SINGLE source of truth (data-driven).
#
# One list, paired (asset_name, ptex3d DSL class). The runtime declares each as a
# Ptex3d asset sharing the terrain vertex_source AND lists the names on the terrain
# drawable's reflected debug_material_assets; the C++ [M] cycle is fully data-driven,
# so ADDING A DEBUG LOOK IS PURE PYTHON: author the DSL material + append ONE entry
# here (zero C++). The player + editor derive their cycle labels from the names (the
# "_dbg_<label>" suffix). Names follow terra_mat_dbg_<label> by convention.
################################################################################

def terrain_debug_materials():
  """Ordered (asset_name, DSL class) pairs for the [M] debug-material cycle. Import is lazy
  (the DSL classes pull the ptex3d stack)."""
  from ork.hypergraph.assets.materials.terrain.debug import (
      DebugNormals, DebugSlope, DebugWhite, DebugHeadlight, DebugRimlight)
  return [
      ("terra_mat_dbg_normals",   DebugNormals),
      ("terra_mat_dbg_slope",     DebugSlope),
      ("terra_mat_dbg_white",     DebugWhite),
      ("terra_mat_dbg_headlight", DebugHeadlight),
      ("terra_mat_dbg_rimlight",  DebugRimlight),
  ]


def terrain_debug_material_labels():
  """['declared', 'normals', 'slope', ...] — the [M] cycle labels (declared + each debug
  material's "_dbg_<label>" suffix). Shared by the editor HUD; the player derives the same
  labels at load from the scene's reflected debug_material_assets."""
  labels = ["declared"]
  for name, _cls in terrain_debug_materials():
    i = name.rfind("_dbg_")
    labels.append(name[i + 5:] if i >= 0 else name)
  return labels


class TerrainRuntime:
  """Reusable terrain-document runtime: document + scale + ECS scene + bake oracle.

  Lifecycle (headless / gate)::

    rt = TerrainRuntime()
    rt.load("voronoi")
    rt.set_context(ctx)
    sd_json = rt.export_scene_json()          # in-code ECS scene -> JSON (player-ready)

  Lifecycle (windowed host — owner-eyeball)::

    rt.set_context(ctx); rt.setup_camera()
    rt.create_live_scene(ctx)                 # SceneData + controller + simulation
    rt.bind_viewport(sgv)
    # ... edit the document ... rt.schedule_rebuild()
    rt.apply_pending_rebuild()                # deferred: re-elaborate -> rebuild sim
  """

  PREVIEW_DIM = 512
  FULL_DIM = 2048
  MAX_DIM = 16384          # dim slider ceiling AND the vertex-source text cap (bake_dim)
  DEFAULT_SKYBOX = "<ork_envmaps2>/blender_courtyard.xir"

  def __init__(self, *, preview_dim=PREVIEW_DIM, full_dim=FULL_DIM, chunk=128):
    self.document = None
    self.source_label = "untitled"
    self.extent_m = float(_HeightFieldBase.EXTENT_M)
    self.material_class = None
    self.material_params = {}
    self.skybox_path = self.DEFAULT_SKYBOX

    # DSL source mode (top-level "Terrain Parameters"): the resolved HeightField
    # class + the CURRENT constructor kwargs that reproduce this document. A doc-JSON
    # session has no DSL source, so editable_dsl_kwargs() is empty (feature absent).
    self._reset_dsl_kwargs()

    # editor post-fx (E/G/T/H keys): the SAME node objects are re-spliced into every
    # fresh scenegraph (values live on the nodes, so gamma/exposure/saturation survive
    # a per-edit rebuild); radiance_maps is re-applied to each fresh pbr_common.
    self.postfx_nodes = []
    self.radiance_maps = None

    self.preview_dim = int(preview_dim)
    self.full_dim = int(full_dim)
    self.chunk = int(chunk)
    self.dim = self.preview_dim

    self._ctx = None
    self._outdir = None

    # select-as-output (SESSION state, never persisted in the document/doc-JSON):
    # tree_paths() key of the node whose 'Out' drives the display captures. The KEY
    # (not the object) is stored so it survives document replacement (re-trace);
    # it re-resolves at every build and clears loudly when the path is gone.
    self._display_key = None

    # bake-oracle products (headless doc gates)
    self.height_path = None

    # live ECS host
    self.scene_data = None
    self.scenegraph = None
    self.layer = None
    self.controller = None
    self._sys_ref = None
    self._tokens = None
    self.cameralut = lev2.CameraDataLut()
    self.camera = None
    self.uicam = None
    self._cpu_hf = None            # CPU height array (display bake) for the surface sampler
    self._cpu_dim = 0
    self.dead_controllers = []
    self.dead_scenegraphs = []
    self._needs_rebuild = False
    # two-phase rebuild handoff: Phase A (GPU thread) writes the built SceneData +
    # the ready flag; Phase B (update thread) consumes them. Single-producer/single-
    # consumer — no document access from the update thread.
    self._pending_scene_data = None
    self._pending_ready = False

    # COMPOSE (multi-document viewport): additional terrain runtimes whose payloads are
    # folded into THIS runtime's scene build (this runtime is the PRIMARY / sim owner).
    # Empty for a single-source session -> exactly one payload -> a byte-identical scene.
    # Each contributor is re-elaborated on every (re)build, so its display/bypass edits
    # reflect in the composed scene (the primary owns the ONE simulation).
    self._contributors = []

    # COMPOSE (family-neutral): external scene decorators — callables fn(scenegraph, layer)
    # re-applied after EVERY scenegraph (re)build, so a NON-terrain contributor (a hypermesh
    # mesh drawable) folded into this primary's scene survives the per-edit scene rebuild the
    # same way the postfx nodes do. Empty for a pure-terrain session (a no-op).
    self._external_decorators = []

    # MT3 (JUL13 §2.6/§E5) — SLICED re-bake. The live scene's baked dim/extent (set on
    # each completed bake): a re-bake at the SAME dim/extent is a PARAM-TWEAK (products
    # already on disk) and slices; a dim/extent change bursts (would stale-delete). The
    # handle polls the SOFT_DEADLINE microtask that pre-cooks the display products +
    # capture-currency sidecars off the GPU thread, so the subsequent blocking swap-bake
    # hits currency/warm-cache and does not hitch.
    self._live_bake_dim = None
    self._live_bake_extent = None
    # display node of the currently-STAGED surface (set at each swap). S4 progressive
    # live-accept morphs the HELD drawable's height plane in place; that is only coherent
    # when the re-bake refines the SAME surface (a param tweak). Across a DISPLAY CHANGE the
    # new node's plane is a different surface entirely (range/normals/frame all shift), so
    # morphing the held drawable through it presents a broken/stale frame until the swap —
    # arm S4 only when the display is unchanged.
    self._live_bake_display_key = None
    self._live_bake_product = None
    # #88 v2 — in-place display REVISIT fast path. `_held_field_path` is the height product
    # path the HELD drawable's S4 LiveFieldBuffer is keyed on (set ONLY at a full swap, when
    # the drawable re-materializes; NEVER moved by an in-place rebind — every in-place publish
    # targets this one buffer). `_current_products` maps each product asset name baked THIS
    # session to the document content hash it was baked at, so a REVISIT to a product whose
    # hash still matches the live document is current on disk (the files ARE the LRU) and can
    # skip the full scene swap. The hash IS the invalidation — a document edit drifts it, so a
    # stale product never matches and falls back to the full swap loudly.
    self._held_field_path = None
    self._current_products = {}
    self._sliced_handle = None
    self._frames_during_rebake = 0   # gate-6 instrument: GPU frames presented per re-bake

  ##############################################################################
  # load — build the document (never touches a GraphData; L2)
  ##############################################################################

  def load(self, source, *, dsl_class=None, extent_m=None, **dsl_kwargs):
    """Load a terrain DOCUMENT from a DSL name/.py path or a doc-JSON path. Returns
    the TerrainDoc. DSL load reads EXTENT_M + the suggested material off the class
    (heights are TRUE METERS — there is no vertical scale attr); doc-JSON load uses
    the HeightField base defaults (override via kwargs)."""
    if self._looks_like_doc_json(source):
      import json
      with open(source, "r") as f:
        self.document = _doc_from_json(json.load(f))
      self.source_label = os.path.splitext(os.path.basename(str(source)))[0]
      self.material_class = None
      self.material_params = {}
      self._reset_dsl_kwargs()        # no DSL source -> Terrain Parameters absent
    else:
      if isinstance(source, str) and source == "new":
        # `new` = the minimal viable terrain; same class path as a DSL file, so
        # Terrain Parameters / material / kwargs all work.
        dsl_path, cls = "new", MinimalTerrain
      else:
        dsl_path = resolve_dsl_file(source)
        cls = load_dsl_class(dsl_path, dsl_class or None)
      # E0: introspect the editable ctor kwargs, then trace with _ParamCapture SYMBOLS for the
      # numeric ones so their uses are captured as document parameters (edit = re-elaborate,
      # not re-trace; topology edits preserved). Non-editor callers (viewer/scenes) never take
      # this path — they instantiate cls(**plain) with concrete values and empty params.
      self._capture_dsl_kwargs(cls, dsl_kwargs)
      self.document = self._trace_param_document(self._dsl_kwargs)
      self.source_label = os.path.splitext(os.path.basename(str(dsl_path)))[0]
      self.extent_m = float(cls.EXTENT_M)
      self.material_class = getattr(cls, "MATERIAL_CLASS", None)
      self.material_params = dict(getattr(cls, "MATERIAL_PARAMS", {}) or {})
    if extent_m is not None:
      self.extent_m = float(extent_m)
    self._outdir = str(_Path.expandPathString(f"<assetcache>/terrainedit/{self.source_label}"))
    os.makedirs(self._outdir, exist_ok=True)
    return self.document

  @staticmethod
  def _looks_like_doc_json(source):
    s = str(source)
    if not s.lower().endswith(".json") or not os.path.isfile(s):
      return False
    try:
      import json
      with open(s, "r") as f:
        data = json.load(f)
      return isinstance(data, dict) and "root" in data and "version" in data
    except (OSError, ValueError):
      return False

  def set_context(self, ctx):
    self._ctx = ctx

  ##############################################################################
  # DSL constructor kwargs — the top-level "Terrain Parameters" (editable)
  #
  # The motivating case (warp.py): frequency / octaves / ring_amp_m / ring_period_m
  # / center are constructor kwargs captured into a ptex3d hfbake closure — the
  # document has no plug-backed params for them, so a plain propsheet is empty.
  # These make them editable: an edit RE-TRACES the class (a fresh document, L2) and
  # the normal rebuild path re-elaborates + rebakes it.
  ##############################################################################

  def _reset_dsl_kwargs(self):
    self._dsl_class = None
    self._dsl_kwargs = {}            # effective ctor kwargs to reproduce the document
    self._dsl_editable = []          # ordered editable kwarg names (subset of _dsl_kwargs)
    self._dsl_kwarg_notes = {}       # name -> reason a param is not surfaced (skipped)
    self._dsl_tags = {}              # name -> unit tag (E0 part 2 typed literals); a kwarg
                                     # whose default is a unit constructor (meters(2000)…)

  @staticmethod
  def _is_editable_kwarg(v):
    """A ctor kwarg the editor can surface: a scalar literal or a numeric tuple
    (vec-ish, e.g. center=(0.0,0.0)). Non-literal defaults (vec3/mtx4/callables) are
    excluded (edit the DSL source for those)."""
    if isinstance(v, bool):
      return True
    if isinstance(v, (int, float, str)):
      return True
    if isinstance(v, tuple):
      return len(v) > 0 and all(
          isinstance(x, (int, float)) and not isinstance(x, bool) for x in v)
    return False

  def _capture_dsl_kwargs(self, cls, dsl_kwargs):
    """Record the DSL source class + its CURRENT effective kwargs: defaults from
    inspect.signature(cls.__init__) overlaid with the dsl_kwargs actually passed.
    Skips *args/**kwargs and non-literal / unsupplied params (recorded in
    dsl_kwarg_notes)."""
    self._reset_dsl_kwargs()
    self._dsl_class = cls
    effective = {}
    try:
      params = inspect.signature(cls.__init__).parameters
    except (TypeError, ValueError):
      params = {}
    for idx, (pname, p) in enumerate(params.items()):
      if idx == 0:                       # self
        continue
      if p.kind is inspect.Parameter.VAR_POSITIONAL:
        self._dsl_kwarg_notes[pname] = "skipped (*args)"
        continue
      if p.kind is inspect.Parameter.VAR_KEYWORD:
        self._dsl_kwarg_notes[pname] = "skipped (**kwargs)"
        continue
      if pname in dsl_kwargs:
        val = dsl_kwargs[pname]
      elif p.default is not inspect.Parameter.empty:
        val = p.default
      else:
        self._dsl_kwarg_notes[pname] = "skipped (no default, not supplied)"
        continue
      # E0 part 2: a UNIT-TAGGED default (meters(2000), cycles(8)…) contributes its tag to
      # the params table but its VALUE stays plain numeric (the tag is doc-layer metadata,
      # never on the plug — so the elaborated graph is tag-free / byte-identical). The tag
      # comes from the signature default even when the value was overridden via dsl_kwargs.
      tag = _units.unit_of(val)
      if tag is None and p.default is not inspect.Parameter.empty:
        tag = _units.unit_of(p.default)
      if _units.is_tagged(val):
        val = float(val)                 # strip -> store plain numeric
      effective[pname] = val             # reproduce faithfully on every re-trace
      if self._is_editable_kwarg(val):
        self._dsl_editable.append(pname)
        if tag is not None:
          self._dsl_tags[pname] = tag
      else:
        self._dsl_kwarg_notes[pname] = f"skipped (non-literal default: {type(val).__name__})"
    self._dsl_kwargs = effective
    if self._dsl_kwarg_notes:
      print(f"[terrain] {self.source_label}: Terrain Parameters skips "
            f"{self._dsl_kwarg_notes}", flush=True)

  def _trace_param_document(self, effective_kwargs):
    """E0 editor trace: instantiate self._dsl_class with _ParamCapture SYMBOLS for each NUMERIC
    editable ctor kwarg, derive the document, attach the params table, and flag every param
    NOT captured as a plug/prop expr STRUCTURAL (a folded / range()-forced / shader-closure /
    unused use — editing it requires the guarded re-trace, never a silent no-op). Numeric
    captured params edit re-trace-free (document mutation + re-elaborate)."""
    from ork.hypergraph.dflow.terrain.doc import (
        _ParamTable, _ParamCapture, _collect_captured_param_names)
    table = _ParamTable()
    for name in self._dsl_editable:
      table.declare(name, effective_kwargs[name], tag=self._dsl_tags.get(name))
    param_kwargs = {}
    for name, val in effective_kwargs.items():
      if (name in self._dsl_editable and isinstance(val, (int, float))
              and not isinstance(val, bool)):
        param_kwargs[name] = _ParamCapture.param(table, name)   # numeric -> symbolic capture
      else:
        param_kwargs[name] = val                             # non-numeric / non-editable -> plain
    inst = self._dsl_class(**param_kwargs)
    inst._doc.params = table
    # close the trace ONLY — no elaborate. load() runs BEFORE the engine init (the doc
    # binds the UI models), and elaboration needs the initialized engine since LoopModule
    # step 3 (reshape); the first rebake elaborates post-init on the GPU thread. A doc
    # with a T.loop elaborated here died as "unresolved placeholder '<carry>#out'".
    inst.close_trace()
    doc = inst.document()
    captured = _collect_captured_param_names(doc)
    for name in self._dsl_editable:
      if name not in captured:
        table.mark_structural(name)
    return doc

  def editable_dsl_kwargs(self):
    """The editable DSL constructor kwargs (name -> current value), typed float/int/
    bool/str/tuple, in signature order. Empty for a doc-JSON session (no DSL source)."""
    return {name: self._dsl_kwargs[name] for name in self._dsl_editable}

  def dsl_kwarg_notes(self):
    """name -> reason each non-surfaced ctor param was skipped (*args/**kwargs /
    non-literal or unsupplied default)."""
    return dict(self._dsl_kwarg_notes)

  def dsl_kwarg_units(self):
    """name -> unit tag for each editable ctor kwarg whose default was a typed literal
    (meters(2000), cycles(8)…); names with a plain default are absent. Drives the editor's
    Terrain-Parameter unit display."""
    return {name: self._dsl_tags[name]
            for name in self._dsl_editable if name in self._dsl_tags}

  def _coerce_kwarg(self, name, value):
    # E0 part 2: a TAGGED param coerces to its unit tag, loudly (all v1 units are float-like,
    # so the coerced value is a plain float; the tag stays on the params table).
    tag = self._dsl_tags.get(name)
    if tag is not None:
      try:
        return _units.coerce_to_tag(tag, value)
      except (TypeError, ValueError) as e:
        raise TerrainDocParamError(
            f"terrain parameter {name!r} ({tag}): cannot coerce {value!r} ({e})")
    old = self._dsl_kwargs[name]
    try:
      if isinstance(old, bool):
        return bool(value)
      if isinstance(old, int):
        return int(round(value)) if isinstance(value, float) else int(value)
      if isinstance(old, float):
        return float(value)
      if isinstance(old, str):
        return str(value)
      if isinstance(old, tuple):
        seq = value if isinstance(value, (tuple, list)) else [value]
        if len(seq) != len(old):
          raise ValueError(f"expected {len(old)} components, got {len(seq)}")
        return tuple(float(x) for x in seq)
    except (TypeError, ValueError) as e:
      raise TerrainDocParamError(
          f"terrain parameter {name!r}: cannot coerce {value!r} to "
          f"{type(old).__name__} ({e})")
    return value

  def set_dsl_kwarg(self, name, value):
    """Editor mutation of a Terrain Parameter (E0). A CAPTURED param (numeric, used only in
    plug/prop VALUES) is a DOCUMENT MUTATION — update doc.params, re-evaluate every recorded
    expr into its plug/prop (re-elaborate), rebuild — NO re-trace, so added/deleted nodes,
    loops and flag edits survive by construction (the P0 fix). A STRUCTURAL param (loop count,
    octaves-via-int, raw range(), ptex3d-closure fold, or any non-numeric kwarg) still needs a
    re-trace to take effect, GUARDED so it can never silently discard editor document edits
    (ops-self-defend). Loud reject on unknown names / uncoercible values. The mutation/trace
    runs on the CALLING thread; the elaborate + bake happen in the standard Phase-A/B rebuild."""
    if self._dsl_class is None:
      raise TerrainDocParamError(
          "no DSL source for this session (doc-JSON load) — Terrain Parameters are "
          "unavailable; edit node params via the outliner instead.")
    if name not in self._dsl_editable:
      raise TerrainDocParamError(
          f"unknown terrain parameter {name!r}; editable: {sorted(self._dsl_editable)}")
    coerced = self._coerce_kwarg(name, value)
    params = getattr(self.document, "params", None)
    if params is not None and name in params and not params.is_structural(name):
      from ork.hypergraph.dflow.terrain.doc import reeval_captured_params
      params.set(name, coerced)                 # L2 document mutation — the recorded exprs win
      reeval_captured_params(self.document)      # refresh param_actions (doc-JSON / undo honesty)
      self._dsl_kwargs[name] = coerced
      self.schedule_rebuild()
      return coerced
    # STRUCTURAL (or no params table): the guarded re-trace path.
    self._assert_pristine_for_retrace(name)
    kwargs = dict(self._dsl_kwargs)
    kwargs[name] = coerced
    self.document = self._trace_param_document(kwargs)   # L2 — new document replaces the old
    self._dsl_kwargs = kwargs
    self.schedule_rebuild()
    return coerced

  def _assert_pristine_for_retrace(self, name):
    """E0 STRUCTURAL guard: refuse a structural-param edit that would discard document edits.
    Re-trace the DSL class with the CURRENT kwargs (unchanged) and compare its doc-JSON against
    the live document's; the comparison is parsed-structure equality (to_json is json-ready and
    ids are traversal-order — a pristine document and its fresh re-trace serialize identically).
    Equal -> pristine -> the re-trace is lossless, proceed. Different -> the document carries
    editor edits -> raise, naming them + the remedy. This guard now protects ONLY the structural
    + doc-JSON-load paths (captured params mutate the document directly, no guard needed). Bias
    to refuse: a false refusal only costs a save/reopen; a false pass loses work."""
    from ork.hypergraph.dflow.terrain.doc import to_json
    if self.document is None:
      return
    fresh = self._trace_param_document(dict(self._dsl_kwargs))
    # Node canvas POSITIONS (E2/E3 editor data) are pure layout — a fresh re-trace can
    # never reproduce them and they do not affect the bake, so they are NOT a document
    # edit for pristine purposes. Strip them before the equality check (the node editor
    # populates positions on first open, which would otherwise refuse every retrace).
    fresh_json = to_json(fresh)
    cur_json = to_json(self.document)
    fresh_json.pop("positions", None)
    cur_json.pop("positions", None)
    if fresh_json == cur_json:
      return
    edits = self._summarize_doc_edits(fresh, self.document)
    raise TerrainDocParamError(
        f"terrain parameter {name!r}: refusing to edit — it is STRUCTURAL (a loop count / "
        f"baked selector / raw control-flow / shader-closure use), so editing it re-traces the "
        f"DSL source and would DISCARD your document edits ({edits}). Undo your document edits, "
        f"or Save as .py and reopen to make them the new baseline.")

  @staticmethod
  def _summarize_doc_edits(baseline, current):
    """Human-readable summary of how `current` diverges from `baseline` (the fresh
    re-trace): added / removed document objects (by tree path) plus per-node param /
    bypass edits. For the interim-guard message only — the authoritative pristine
    decision is the doc-JSON compare in _assert_pristine_for_retrace()."""
    from ork.hypergraph.dflow.terrain.doc import tree_paths, DocNode

    def _sigs(doc):
      out = {}
      for (_pk, key, obj) in tree_paths(doc):
        if isinstance(obj, DocNode):
          out[key] = ("node", obj.clazz_name,
                      tuple((k, n, repr(v)) for (k, n, v) in obj.param_actions),
                      bool(obj.bypassed))
        else:
          out[key] = (type(obj).__name__, getattr(obj, "count", None),
                      bool(getattr(obj, "bypassed", False)))
      return out

    base, cur = _sigs(baseline), _sigs(current)
    added = sorted(set(cur) - set(base))
    removed = sorted(set(base) - set(cur))
    modified = sorted(k for k in (set(cur) & set(base)) if cur[k] != base[k])
    parts = []
    if added:
      parts.append(f"{len(added)} added {added}")
    if removed:
      parts.append(f"{len(removed)} removed {removed}")
    if modified:
      parts.append(f"{len(modified)} edited {modified}")
    return "; ".join(parts) if parts else "topology / connection changes"

  def to_doc_json(self):
    if self.document is None:
      raise RuntimeError("TerrainRuntime.to_doc_json(): no document loaded")
    return _doc_to_json(self.document)

  def save_doc_json(self, path):
    """Write the document to a doc-JSON file. INTERNAL format (undo checkpoints /
    gates) — the user-facing Save writes a .py DSL asset via save_dsl_py()."""
    import json
    p = str(path)
    if not p.lower().endswith(".json"):
      p += ".json"
    with open(p, "w") as f:
      json.dump(self.to_doc_json(), f, indent=2)
    return p

  def save_dsl_py(self, path):
    """Write the document as a runnable HeightField .py DSL asset — the SAVE format
    ork.scene.viewer.py (the hyperecs scenes) consume. The class name is derived from
    the filename stem, EXTENT_M from self.extent_m, and the header notes the session
    source. Regenerates the DSL that would re-record this exact document (S3 codegen);
    raises LOUDLY (naming the node) if the document holds an authored ExprModule
    (hfbake/hfdisplacement/T.pow) or a group/switch the writer cannot reconstruct."""
    from ork.hypergraph.dflow.terrain.pywriter import to_python, class_name_from_stem
    if self.document is None:
      raise RuntimeError("TerrainRuntime.save_dsl_py(): no document loaded")
    p = str(path)
    if not p.lower().endswith(".py"):
      p += ".py"
    stem = os.path.splitext(os.path.basename(p))[0]
    src = to_python(self.document, class_name=class_name_from_stem(stem),
                    extent_m=self.extent_m, source_note=self.source_label)
    with open(p, "w") as f:
      f.write(src)
    return p

  ##############################################################################
  # in-code ECS scene (from the document) — the DISPLAY source of truth
  ##############################################################################

  ##############################################################################
  # select-as-output (session display override)
  ##############################################################################

  @property
  def display_key(self):
    return self._display_key

  def set_display_key(self, key):
    """Select the node at tree_paths() `key` as the display output (None clears).
    Exclusive — one node at a time. Validates NOW against the current document; the
    key re-resolves at every rebuild. A processing DocNode (captures refused) OR a
    DocLoop (displays the loop's height-typed carry output — elaborate promotes it to
    the module's "Out") is displayable."""
    from ork.hypergraph.dflow.terrain.doc import DocNode, DocLoop, find_by_path
    if key is None:
      display_trace(f"set_display_key None (was {self._display_key!r})")
      self._display_key = None
      return None
    obj = find_by_path(self.document, key) if self.document is not None else None
    if not self._is_displayable(obj):
      display_trace(f"set_display_key {key!r} REFUSED (not displayable)")
      raise ValueError(
          f"set_display_key: {key!r} does not name a displayable document node")
    display_trace(f"set_display_key {key!r} (was {self._display_key!r})")
    self._display_key = str(key)
    return obj

  @staticmethod
  def _is_displayable(obj):
    """A processing DocNode (not a capture) or any DocLoop (displays its carry output)."""
    from ork.hypergraph.dflow.terrain.doc import DocNode, DocLoop
    if isinstance(obj, DocLoop):
      return True
    return isinstance(obj, DocNode) and obj.clazz_name != "CaptureModule"

  def _resolve_display_node(self):
    """The DocNode / DocLoop for the current display key, or None. A stale key (structural
    change removed the node) CLEARS the override loudly and falls back to the
    document's real captures. Displaying the EFFECTIVE TERMINAL is definitionally the
    default view — it maps to None so the bake reuses the canonical products (usually a
    capture-currency hit: instant) and keeps the FULL stored materials, instead of
    re-deriving an identical field under a display key (#88 v1: a terminal revisit was
    a minutes-scale cold recompute of the same image the default bake already wrote)."""
    if self._display_key is None:
      return None
    if self._display_key == self.effective_display_terminal():
      return None
    from ork.hypergraph.dflow.terrain.doc import find_by_path
    obj = find_by_path(self.document, self._display_key)
    if not self._is_displayable(obj):
      print(f"[terrain] display node {self._display_key!r} no longer resolves — "
            f"clearing select-as-output.", flush=True)
      self._display_key = None
      return None
    return obj

  def product_asset_name(self):
    """The terrain product ASSET NAME for the CURRENT display state — the single lever the
    C++ bake derives every product path from (<assetcache>/terrain/<asset>/<channel>.exr +
    manifest + sidecars). Default view (no display key, or the key IS the effective
    terminal): the canonical 'terra'. An interior display node: 'terra.display.<node>' —
    its OWN directory, so display bakes never clobber the canonical products or each
    other, and a display REVISIT is a capture-currency hit on its own files instead of a
    rebake (#88 v1)."""
    k = self._display_key
    if k is None or k == self.effective_display_terminal():
      return "terra"
    import re as _re
    return "terra.display." + _re.sub(r"[^A-Za-z0-9_.-]", "_", str(k))

  @staticmethod
  def _is_interior_product(name):
    """An INTERIOR display product ('terra.display.<node>') — its bake used the simple Solid
    display material (mid-chain fields have no stored-material channels). The canonical
    'terra' (default / effective-terminal) keeps the FULL material, so it is NOT interior.
    The #88 v2 in-place plane rebind is coherent ONLY between two interior products (shared
    Solid material graph AND shared mono relax=False SSBO layout)."""
    return bool(name) and name.startswith("terra.display.")

  def _product_height_path(self, product_name):
    """<assetcache>/terrain/<product>/height.exr — where the C++ bake writes the height
    product and the drawable materialize / S4 buffer key resolve from (one derivation)."""
    d = str(_Path.expandPathString(f"<assetcache>/terrain/{product_name}"))
    return os.path.join(d, "height.exr")

  def _doc_content_hash(self):
    """Stable content hash of the current DOCUMENT — the #88 v2 currency oracle. The display
    key is SESSION state (absent from to_json) and canvas positions are pure layout, so BOTH
    are excluded: a display switch or a node drag never invalidates a product, but any real
    document edit drifts the hash and a product baked at the old hash correctly reads stale.
    elaborate() builds a fresh GraphData and never writes back onto the document, so the hash
    is stable across the display switches between a bake and its revisit."""
    if self.document is None:
      return None
    import hashlib
    import json as _json
    from ork.hypergraph.dflow.terrain.doc import to_json
    js = to_json(self.document)
    js.pop("positions", None)
    return hashlib.sha1(_json.dumps(js, sort_keys=True).encode("utf-8")).hexdigest()

  def effective_display_terminal(self):
    """tree_paths() key of the node whose output the DEFAULT (no session override) bake +
    viewport actually shows — the display badge's IMPLICIT home so the canvas is never
    flagless on open. Deterministic (resolves the #17 implicit-terminal ambiguity for the
    editor): the doc-level select_output if the document persists one, else the producer of
    the FIRST 'height' display capture (the node the display=None height plane reads; document
    order breaks any multi-height tie). Returns None if no displayable terminal resolves.

    Memoized per document object (queried once per canvas node per redraw); a doc replacement
    (edit / retrace / undo) swaps the object and re-resolves."""
    doc = self.document
    if doc is None:
      return None
    cached = getattr(self, "_eff_terminal_cache", None)
    if cached is not None and cached[0] is doc:
      return cached[1]
    key = self._compute_effective_display_terminal(doc)
    self._eff_terminal_cache = (doc, key)
    return key

  def _compute_effective_display_terminal(self, doc):
    from ork.hypergraph.dflow.terrain.doc import tree_paths, DocLoop, DocNode, _Placeholder
    display_channels = set(getattr(doc, "_DISPLAY_CHANNELS", ("height", "normal")))

    def _producer(ref):
      # the node feeding a capture: a DocNode / DocLoop directly, or a loop carry's
      # out_placeholder -> the owning DocLoop (displaying it shows that carry, Fix 1).
      node = getattr(ref, "node", None)
      if isinstance(node, (DocNode, DocLoop)):
        return node
      if isinstance(node, _Placeholder):
        for (_pk, _k, o) in tree_paths(doc):
          if isinstance(o, DocLoop):
            for c in o.carries.values():
              if getattr(c, "out_placeholder", None) is node:
                return o
      return None

    tgt = getattr(doc, "_select_output", None)
    if tgt is None:
      # producer of the primary 'height' display capture, else any display channel's —
      # document order is the deterministic tie-break for the #17 ambiguity. A capture's
      # own node is a sink (never displayable); the terminal is the node feeding it.
      cap_node = None
      for cap in doc._captures:
        if "height" in cap.channels:
          cap_node = cap.node
          break
      if cap_node is None:
        for cap in doc._captures:
          if set(cap.channels) & display_channels:
            cap_node = cap.node
            break
      if cap_node is not None:
        conns = getattr(cap_node, "connections", None)
        if conns:
          tgt = _producer(conns[0][1])
    if tgt is None or not self._is_displayable(tgt):
      return None
    for (_pk, k, o) in tree_paths(doc):
      if o is tgt:
        return k
    return None

  ##############################################################################
  # COMPOSE — fold N terrain documents into ONE scene/simulation (multi-doc host)
  ##############################################################################

  def add_contributor(self, runtime):
    """Register another terrain runtime whose payload is folded into THIS runtime's
    scene (this runtime being the PRIMARY / sim owner). Idempotent per runtime. The
    contributor is re-elaborated on every (re)build, so its display/bypass edits reflect
    in the composed scene — but it never owns a simulation of its own (one world)."""
    if runtime is not self and runtime not in self._contributors:
      self._contributors.append(runtime)

  def add_external_decorator(self, fn):
    """Register a family-neutral scene decorator fn(scenegraph, layer) re-applied after every
    scenegraph (re)build (and immediately if a scene already exists). A NON-terrain contributor
    (a hypermesh mesh drawable) uses this to fold its drawable into THIS primary's forward layer
    and keep it across per-edit rebuilds (the postfx-node re-splice precedent)."""
    self._external_decorators.append(fn)
    if self.scenegraph is not None and self.layer is not None:
      fn(self.scenegraph, self.layer)

  def _elaborate_payload(self, dim, *, simple_material=False, index=0):
    """This runtime's terrain contribution to a (possibly composed) scene build: the
    elaborated GraphData + material + geometry params, at grid `dim`. `index` positions
    the payload — 0 == the PRIMARY (asset/entity names + placement identical to a
    single-source scene); index>0 == a composed contributor (suffixed names)."""
    from ork.hypergraph.assets.materials.terrain.solid import Solid
    if self.document is None:
      raise RuntimeError("TerrainRuntime._elaborate_payload(): no document loaded")
    display_node = self._resolve_display_node()
    # sole GraphData constructor (L2). With a display node, only the height/normal
    # captures survive (rewired to it) — nothing downstream of it computes — and the
    # look falls back to the plain Solid material (mid-chain fields have no
    # stored-material channels).
    graph, _cap = self.document.elaborate(display_node=display_node)
    simple_material = simple_material or (display_node is not None)
    # RELAX detection (the _terrain.py idiom, from the ELABORATED graph = single source
    # of truth): a relaxed bake writes the relaxed_uv channel, and the C++ drawable then
    # REQUIRES the vertex source's sif_terra_frame block — a mismatch is a hard abort
    # (blank viewport). In DISPLAY mode the graph is elaborated in full (the marker is
    # stamped, not rewired), but the C++ bake KEEPS only the height/normal captures and
    # drops relaxed_uv — so relax must be False here to match what the bake writes.
    relax = False
    if display_node is None:
      for cap in lev2.terrain.capture_modules(graph):
        if "relaxed_uv" in [c.strip() for c in (cap.channel or "").split(",")]:
          relax = True
          break
    mat_cls = Solid if simple_material else (self.material_class or Solid)
    if simple_material or self.material_class is None:
      mat_params = {"albedo": vec3(0.45, 0.42, 0.35), "roughness": 0.9}
    else:
      mat_params = dict(self.material_params)
    return {
        "graph": graph, "relax": relax, "mat_cls": mat_cls, "mat_params": mat_params,
        "extent_m": self.extent_m, "chunk": self.chunk, "index": index,
        "suffix": "" if index == 0 else f"_{index}", "label": self.source_label,
        # product asset name (#88 v1): display bakes get their OWN product dir; the
        # canonical 'terra' dir is written by the DEFAULT bake alone.
        "asset_name": self.product_asset_name() + ("" if index == 0 else f"_{index}"),
    }

  def build_scene_data(self, *, dim=None, simple_material=False):
    """Derive a fresh dflow.GraphData from the DOCUMENT (L2) and lower a minimal
    one-terrain-entity ECS scene around it into an ecs.SceneData. The embedded graph
    rides HeightFieldGenData; the C++ terrain path bakes + renders it at load. `dim`
    is the render + bake grid; simple_material forces a plain Solid look (visibility
    is material-independent — used by the headless snapshot gate).

    COMPOSE: with contributors registered (add_contributor) N terrain payloads are lowered
    into the SAME scene — one entity + asset set per payload. Placement is OVERLAP at the
    world origin (the terrain drawable renders at a fixed extent-centered origin; it honors
    no per-node world matrix, so offset placement is a follow-up); with distinct terrains
    the depth test resolves per pixel, so BOTH surfaces contribute. A single-source runtime
    has no contributors -> exactly one payload -> a byte-identical scene."""
    from ork.hypergraph.ecs.scene import Scene
    from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
    from orkengine.lev2 import HeightFieldGenData, TerrainChunkDrawableData

    dbg_materials = terrain_debug_materials()  # [M] the data-driven debug-material cycle (single source)

    if self.document is None:
      raise RuntimeError("TerrainRuntime.build_scene_data(): no document loaded")
    dim = int(dim) if dim is not None else self.dim
    payloads = [self._elaborate_payload(dim, simple_material=simple_material, index=0)]
    for i, contrib in enumerate(self._contributors, start=1):
      payloads.append(contrib._elaborate_payload(dim, simple_material=False, index=i))

    skybox = self.skybox_path
    MAX_DIM = TerrainRuntime.MAX_DIM
    dimlog = os.environ.get("ORKID_TERRAIN_DIMLOG")

    class _TerrainDocScene(Scene):
      def __init__(self):
        super().__init__()
        SG = self.scenegraph(
            preset="ForwardPBR", skybox_path=skybox,
            SkyboxIntensity=2.0, DiffuseIntensity=1.0, SpecularIntensity=1.0,
            AmbientLight=vec3(0.10),
            ssaa=2,
            msaa=2)  # scene param -> _mergedParams -> fwd node MSAA RtGroup (3=8x)
        for pl in payloads:
          suffix = pl["suffix"]                       # "" for the primary -> names unchanged
          asset_name = pl["asset_name"]               # display-keyed product dir (#88 v1)
          mat_name = "terra_mat" + suffix
          entity_name = "terrain" + str(pl["index"])
          extent_m, chunk = pl["extent_m"], pl["chunk"]
          # embed the DOCUMENT's elaborated graph directly (no DSL re-trace) — the
          # HeightFieldGenData is what serializes + defers its bake to the C++ load.
          gd = HeightFieldGenData(dimension=dim, extent_m=extent_m, graph=pl["graph"])
          gd.asset_name = asset_name
          self._asset_gens.append((asset_name, gd))
          # bake_dim = MAX_DIM: the vertex-source SHADER TEXT bakes the per-chunk array
          # caps + byte offsets from bake_dim — capping at the slider ceiling makes the
          # text CONSTANT across every editor dim, so a dim change recompiles NO
          # rendering materials (they all share this one vertex source). Cost: a fixed
          # ~200KB header region; heights stay dense at the ACTUAL dim (runtime-sized).
          vs = TerrainChunkVertexSource(dim=dim, bake_dim=MAX_DIM,
                                        extent_m=extent_m, chunk=chunk, relax=pl["relax"])
          if dimlog:
            tag = "" if pl["index"] == 0 else f"[{pl['index']}]"
            print(f"[terrain-dim] SCENE{tag} dim={dim} vs(bake_dim={vs.bake_dim} "
                  f"maxnc={vs.maxnc} HEIGHTS_OFF={vs.HEIGHTS_OFF} TOTAL={vs.TOTAL}) "
                  f"extent={extent_m} chunk={chunk}", flush=True)
          self.asset.Ptex3d(mat_name, dsl_class=pl["mat_cls"], vertex_source=vs, **pl["mat_params"])
          # [M] material-override debug looks — each its OWN FWD_SSBO_CUSTOM material, sharing the
          # SAME vertex_source (identical SSBO layout) so the C++ terrain drawable can swap to any of
          # them at runtime (SceneGraphSystem SetTerrainMaterialMode). The ORDERED name list is DATA:
          # it rides the drawable's reflected debug_material_assets, and the C++ [M] cycle resolves +
          # cycles WHATEVER it names — so this loop is the only place a new debug look is registered.
          # The [M] cycle drives the PRIMARY drawable only (one cycle target); contributors carry none.
          dbg_names = []
          if pl["index"] == 0:
            for _name, _cls in dbg_materials:
              self.asset.Ptex3d(_name, dsl_class=_cls, vertex_source=vs)
            dbg_names = [n for n, _c in dbg_materials]
          self.entity(entity_name, components=[SG.component(nodes={
              asset_name: {"drawable": TerrainChunkDrawableData(
                  hf_asset=asset_name, material_asset=mat_name, chunk=chunk,
                  layout_dim_cap=MAX_DIM,   # MUST equal the vs bake_dim above
                  debug_material_assets=dbg_names)},
          })])

    scene = _TerrainDocScene()
    sd = ecs.SceneData()
    scene.build(sd)
    return sd

  def export_scene_json(self, path=None, *, dim=None, simple_material=False):
    """Serialize the in-code ECS scene to JSON (the ork.scene.tojson flow). Returns the
    JSON string; also writes it to `path` when given."""
    sd = self.build_scene_data(dim=dim, simple_material=simple_material)
    js = sd.serializeJson()
    if path is not None:
      with open(str(path), "w") as f:
        f.write(js)
    return js

  ##############################################################################
  # live ECS host (windowed editor / viewer2) — owner-eyeball
  ##############################################################################

  def setup_camera(self, camname="spawncam", eye=None, tgt=None, up=None):
    from orkengine.core import lev2_pyexdir
    lev2_pyexdir.addToSysPath()
    from lev2utils.cameras import setupUiCameraX
    if eye is None:
      # heights are true meters with no scale constant to frame against — a fraction
      # of the extent gives a sane initial elevation; surface-orbit recenters after load.
      eye = vec3(0, self.extent_m * 0.15, self.extent_m * 0.7)
    if tgt is None:
      tgt = vec3(0, 0, 0)
    if up is None:
      up = vec3(0, 1, 0)
    self.camera, self.uicam = setupUiCameraX(cameralut=self.cameralut, camname=camname)
    # MATCH the C++ player's EzUiCam so zoom/dolly + near/far feel identical when viewing the
    # SAME exported terrain scene (ork.ecs player main.cpp:414-420). The player uses CONSTANTS
    # (not extent-scaled): near_min 0.75, far_max 100000, fov 65deg, zoom-move 0.05. setupUiCameraX
    # otherwise defaults to far=1000 (clips terrain) + fov 45 — the owner's "zoom unusable".
    self.uicam.near_min = 0.75
    self.uicam.far_max = 100000.0
    self.uicam.fov = math.radians(65.0)
    self.uicam.base_zmoveamt = 0.05
    self.uicam.lookAt(eye, tgt, up)
    self.uicam.updateMatrices()
    self.camera.copyFrom(self.uicam.cameradata)
    return self.camera, self.uicam

  def create_live_scene(self, ctx, *, dim=None):
    """Build + host the in-code scene at init (GPU thread). Uses the SAME two-phase
    internals as the deferred rebuild — _build_pending (GPU materialization) then
    _consume_pending (scenegraph + sim) — so there is ONE scene-construction path."""
    self.dim = int(dim) if dim is not None else self.dim
    self._build_pending(self.dim)     # Phase A (GPU): materialize the material(s)
    self._consume_pending()            # Phase B: fresh scenegraph + simulation
    self._refresh_surface_camera(keep_xz=False)   # initial: orbit target at surface, center XZ
    return self.scenegraph

  def _build_pending(self, dim=None):
    """PHASE A core (GPU thread): re-elaborate the DOCUMENT + build_scene_data — ALL GPU
    material materialization (the PBRMaterial assignImages that crashed off-GPU-thread)
    happens here with a valid ctx. Stashes the result in the handoff slot; no sim /
    scenegraph mutation. NOTE (jul10): the C++ wire/bake step stays in
    _start_simulation — moving it here destabilized the sim-start race (task: sim-start
    GPU phase deadlock); it also buys nothing visually, since the editor runs Phase A+B
    within ONE GPU tick (nothing presents in between)."""
    if dim is not None:
      self.dim = int(dim)
    self._pending_scene_data = self.build_scene_data(dim=self.dim)
    self._pending_ready = True

  def _consume_pending(self):
    """PHASE B core (update thread): swap in the prepared scene — destroy the old sim
    (retention funnels), a FRESH scenegraph, start a new sim. No document access."""
    self.scene_data = self._pending_scene_data
    self._pending_scene_data = None
    self._pending_ready = False
    self._rebuild_scenegraph()
    self._start_simulation()
    # the display bake just ran (in _start_simulation) — record its scale so the next
    # edit at this SAME dim/extent qualifies for the MT3 sliced pre-cook (products exist).
    self._live_bake_dim = self.dim
    self._live_bake_extent = self.extent_m
    self._live_bake_display_key = self._display_key
    self._live_bake_product = self.product_asset_name()
    # #88 v2: the fresh drawable materialized against THIS product's height.exr — record it as
    # the S4 buffer key every in-place rebind publishes to (only a full swap moves it), and
    # mark the product current at the live document's content hash (the revisit currency key).
    self._held_field_path = self._display_heights_path()
    self._current_products[self._live_bake_product] = self._doc_content_hash()
    display_trace(f"swap APPLIED display={self._display_key!r} "
                  f"product={self._live_bake_product!r} dim={self.dim} "
                  f"(fresh scenegraph + sim live)")

  def _rebuild_scenegraph(self):
    """Fresh scenegraph + ForwardPBR layers for a (re)built simulation. Retains the old
    scenegraph (dead_scenegraphs) so an in-flight render frame never dereferences a freed
    scene — the ecsedit fresh-scenegraph-per-rebuild precedent (a reused scenegraph would
    keep the dead simulation's nodes)."""
    if self.scenegraph is not None:
      self.dead_scenegraphs.append(self.scenegraph)
    sg_params = self.scene_data.generateSceneGraphParams()
    # editor post-fx (ACES/HSVG) — the SAME node objects re-spliced into every fresh
    # scenegraph so gamma/exposure/saturation state SURVIVES the per-edit rebuild (values
    # live on the held nodes). The merge is additive + dedup-by-pointer (scenegraph.cpp
    # Scene::applyRuntimeParams), so re-adding the same node is safe.
    for node in self.postfx_nodes:
      node.addToSceneVars(sg_params, "PostFxChain")
    self.scenegraph = lev2.scenegraph.Scene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")
    self.scenegraph.createLayer("std_transparent")
    self.scenegraph.createLayer("depth_prepass")
    # re-apply the live-selected envmap to the fresh pbr_common (E-key cycle survives rebuild).
    if self.radiance_maps is not None:
      self.scenegraph.pbr_common.RadianceMaps = self.radiance_maps
    # COMPOSE: re-fold every external (non-terrain) contributor's drawable into the fresh scene.
    for fn in self._external_decorators:
      try:
        fn(self.scenegraph, self.layer)
      except Exception as ex:
        print(f"[terrain-runtime] external decorator failed: {ex}", flush=True)
    return self.scenegraph

  def set_radiance_maps(self, skybox):
    """Set the live environment radiance maps (editor E-key envmap cycle): apply to the
    current scenegraph immediately AND stash so each fresh scenegraph (rebuild) re-applies
    it — one path, so the envmap survives the per-edit scene rebuild."""
    self.radiance_maps = skybox
    if self.scenegraph is not None:
      self.scenegraph.pbr_common.RadianceMaps = skybox

  def _start_simulation(self):
    from orkengine.core import CrcStringProxy
    self._destroy_simulation()
    self._tokens = CrcStringProxy()
    # C++ WIRE STEP (D.5) — resolve C++-only drawables (the terrain chunk drawable's
    # manifest + material, BY NAME) + bake the heightfield. MUST run BEFORE bindScene —
    # the same call-site contract the C++ player uses — on the GPU thread with a live ctx.
    # Without it a TerrainChunkDrawableData never resolves (renders blank).
    if self._ctx is not None:
      ecs.materializeAndWireScene(self.scene_data, self._ctx)
    self.controller = ecs.Controller()
    self.controller.bindScene(self.scene_data)
    self.controller.createSimulation(scenegraph=self.scenegraph)
    self.controller.startSimulation()
    self._sys_ref = self.controller.findSystem("SceneGraphSystem")

  def _destroy_simulation(self):
    if self.controller:
      try:
        self.controller.stopSimulation()
      except Exception:
        pass
      self.dead_controllers.append(self.controller)
      self.controller = None
      self._sys_ref = None

  def schedule_rebuild(self):
    """Mark a deferred sim rebuild (edit landed). Consumed on the update thread by
    apply_pending_rebuild() — the cook cache absorbs the re-derivation cost."""
    self._needs_rebuild = True

  ##############################################################################
  # MT3 — sliced (anti-hitch) re-bake pre-cook
  ##############################################################################

  def try_inplace_display_rebind(self, ctx):
    """#88 v2 FAST PATH: an interior->interior display REVISIT whose target product is already
    current on disk skips the FULL scene swap — the held terrain drawable's height plane is
    morphed IN PLACE (C++ s4LiveAccept) from the on-disk product, so the live scenegraph,
    simulation and camera all stay put. `_display_key` is already the target (set_output ran
    before this route). Returns True iff the in-place rebind was applied; every False path is
    LOUD (display_trace) and means the caller MUST take the full-swap path — never a
    wrong-plane bind (ops self-defend)."""
    if os.environ.get("ORKID_DISPV2_DISABLE"):
      display_trace("inplace DECLINE (dispv2 disabled) -> full swap")
      return False
    if self.scenegraph is None or self._held_field_path is None:
      display_trace("inplace DECLINE (no held drawable yet) -> full swap")
      return False
    tgt_product = self.product_asset_name()
    cur_product = self._live_bake_product
    # must be a real display SWITCH — a same-product re-fire is a param tweak the normal
    # (S4 same-surface) path already handles in place.
    if tgt_product == cur_product:
      display_trace(f"inplace DECLINE (not a switch: product={tgt_product!r}) -> normal path")
      return False
    # COHERENCE BOUNDARY (hard): both source AND target must be interior (shared Solid material
    # + mono relax=False SSBO layout). A default/terminal <-> interior switch crosses the
    # material graph and needs the full swap (fresh material + possibly relaxed SSBO).
    if not (self._is_interior_product(cur_product) and self._is_interior_product(tgt_product)):
      display_trace(f"inplace DECLINE (material boundary {cur_product!r}->{tgt_product!r}) "
                    f"-> full swap")
      return False
    # a re-grid (dim/extent change) needs a fresh SSBO layout — never an in-place plane push.
    if self._live_bake_dim != self.dim or self._live_bake_extent != self.extent_m:
      display_trace(f"inplace DECLINE (dim {self._live_bake_dim}->{self.dim} / extent "
                    f"{self._live_bake_extent}->{self.extent_m}) -> full swap")
      return False
    # CURRENCY: the target product must have been baked THIS session against the CURRENT
    # document (content hash) — a document edit since would have drifted the hash, so the
    # on-disk product is stale and we must rebake (full swap).
    if self._current_products.get(tgt_product) != self._doc_content_hash():
      display_trace(f"inplace DECLINE (target {tgt_product!r} not current: not session-baked "
                    f"or document edited) -> full swap")
      return False
    # and the product file must still be on disk (the files ARE the LRU — an eviction falls
    # back cleanly to the full swap, which re-cooks / capture-currency-hits).
    tgt_height = self._product_height_path(tgt_product)
    if not os.path.exists(tgt_height):
      display_trace(f"inplace DECLINE (product file gone {tgt_height!r}) -> full swap")
      return False
    # push the on-disk target plane into the buffer the HELD drawable consumes (its
    # materialize-time key = _held_field_path; the buffer KEY never moves on an in-place
    # rebind). The C++ loads channel-0 FLOAT meters exactly as materialize does, so the
    # pushed plane is byte-identical to a fresh full-swap of the same product.
    plane_dim = lev2.terrain.publish_height_plane_from_exr(self._held_field_path, tgt_height)
    if plane_dim <= 0:
      display_trace("inplace DECLINE (publish declined: buffer unarmed / unreadable) -> full swap")
      return False
    # the held drawable now presents the target surface -> it IS the displayed product now.
    # _held_field_path stays put (the drawable's buffer key is unchanged until a full swap).
    self._live_bake_display_key = self._display_key
    self._live_bake_product = tgt_product
    # camera: re-evaluate the orbit target Y on the NEW surface at the current XZ — IDENTICAL
    # to the full-swap path's _refresh_surface_camera(keep_xz=True), so a revisit frames the
    # same way the earlier visit did (byte-identity holds when the camera was not panned).
    self._refresh_surface_camera(keep_xz=True)
    display_trace(f"plane REBIND (in-place) display={self._display_key!r} product={tgt_product!r} "
                  f"plane_dim={plane_dim} key={self._held_field_path!r} "
                  f"(held scenegraph + sim + camera kept)")
    return True

  def begin_sliced_rebuild(self, ctx, *, dim=None):
    """MT3: enqueue a SOFT_DEADLINE microtask that pre-cooks the display products +
    capture-currency sidecars for asset 'terra' as budgeted slices across frames (GPU
    thread stays live). The subsequent prepare_rebuild/apply_pending_rebuild swap then
    hits capture-currency / warm cook-cache and does NOT hitch.

    Returns True if a sliced pre-cook was enqueued (poll sliced_rebuild_ready()); False
    means the caller should BURST (the pre-MT3 blocking prepare+apply): MT3 disabled, no
    document, or a dim/extent change (a cold full cook would exceed hold-last-frame — the
    'param-tweak-only interactive v1' boundary, JUL13 §E5)."""
    if os.environ.get("ORKID_MT3_DISABLE"):
      display_trace("begin_sliced DECLINE (mt3 disabled) -> burst")
      return False
    if self.document is None:
      display_trace("begin_sliced DECLINE (no document) -> burst")
      return False
    dim = int(dim) if dim is not None else self.dim
    self.dim = dim
    # slice ONLY when the live scene is already baked at THIS dim/extent (products exist;
    # the swap-bake will currency-skip rather than stale-delete + cold-recompute).
    if (self._live_bake_dim != dim
            or self._live_bake_extent != self.extent_m):
      display_trace(f"begin_sliced DECLINE (dim {self._live_bake_dim}->{dim} or extent "
                    f"{self._live_bake_extent}->{self.extent_m}) -> burst")
      return False
    # elaborate exactly as build_scene_data does (display override honored), then point
    # the captures at the display product paths materialize expects (<assetcache>/terrain/
    # terra/<channel>.exr) so the pre-cook's flush writes the products the swap consumes.
    display_node = self._resolve_display_node()
    graph, _cap = self.document.elaborate(display_node=display_node)
    outdir = str(_Path.expandPathString(f"<assetcache>/terrain/{self.product_asset_name()}"))
    os.makedirs(outdir, exist_ok=True)
    # S4 live-accept morphs the HELD drawable's height plane IN PLACE — coherent only when
    # the re-bake refines the SAME surface. A display change stages a different surface, so
    # arm S4 only for a same-display re-bake; a display switch falls back to hold-last-frame
    # + swap (the fresh drawable loads the on-disk product and reframes the camera).
    # same PRODUCT = same surface (product_asset_name folds the terminal==default
    # equivalence in, so a badge-home click after a default bake still counts as same).
    # ALSO require the drawable's S4 buffer key (_held_field_path) to name the product being
    # baked: after a #88 v2 in-place rebind the displayed product moved but the held buffer
    # key did not, so S4 must NOT arm on a buffer nobody consumes (it would publish into the
    # void). In the no-in-place case these two conditions are identical.
    s4_same_surface = (self.product_asset_name() == self._live_bake_product
                       and self._display_heights_path() == self._held_field_path)
    for cap in lev2.terrain.capture_modules(graph):
      chans = [c.strip() for c in (cap.channel or "height").split(",") if c.strip()] or ["height"]
      cap.path = (os.path.join(outdir, chans[0] + ".exr") if len(chans) == 1
                  else os.path.join(outdir, "{channel}.exr"))
      # S4 progressive display (JUL13 §E5/S4): the sliced pre-cook publishes the height
      # plane at each viewable-node checkpoint; the OLD (held-last-frame) scene's terrain
      # drawable live-accepts it, so the terrain MORPHS during the re-bake instead of
      # popping at the end. Session-scoped (set on the freshly elaborated graph only);
      # ORKID_S4_DISABLE=1 reverts to on_complete (the C++ sites guard too).
      if "height" in chans and s4_same_surface and not os.environ.get("ORKID_S4_DISABLE"):
        cap.visual_update_mode = "on_checkpoint"
    display_trace(f"begin_sliced ENQUEUE display={self._display_key!r} "
                  f"live_bake={self._live_bake_display_key!r} s4_same={s4_same_surface} "
                  f"dim={dim}")
    self._sliced_handle = lev2.terrain.begin_sliced_bake(graph, ctx, dim, self.extent_m)
    self._frames_during_rebake = 0
    return True

  def sliced_rebuild_ready(self):
    """True once the sliced pre-cook has written its products (or no pre-cook is active).
    The caller counts its GPU frames while this is False (gate-6 responsiveness metric)."""
    h = self._sliced_handle
    if h is None:
      return True
    self._frames_during_rebake += 1
    if h.done:
      display_trace(f"sliced pre-cook DONE ({self._frames_during_rebake} frames)")
      self._sliced_handle = None
      return True
    return False

  def frames_during_last_rebake(self):
    """GPU-thread frames presented while the last sliced pre-cook was in progress —
    ~0 for the blocking path, many for the sliced path (JUL13 §E5 gate-6 evidence)."""
    return self._frames_during_rebake

  def prepare_rebuild(self, ctx, *, dim=None):
    """PHASE A (GPU thread): if a rebuild was requested (schedule_rebuild), re-elaborate +
    build_scene_data into the handoff slot — ALL GPU material materialization is here.
    No sim/scenegraph mutation. Returns True if a scene was prepared."""
    if not self._needs_rebuild:
      return False
    self._needs_rebuild = False
    self._build_pending(dim)
    return True

  def apply_pending_rebuild(self):
    """PHASE B: if a scene is prepared (prepare_rebuild ran), swap the simulation — destroy
    old (retention funnels), fresh scenegraph, start new sim. Returns True when a swap ran —
    the caller MUST then rebind its viewport to self.scenegraph (a new object).

    THREAD AFFINITY (terrain-specific): unlike ecsedit (whose createSimulation does no GPU
    work, so its sim swap runs on the update thread), the terrain's createSimulation triggers
    the DEFERRED heightfield bake — GPU work — so this must run on the GPU thread, and the
    fresh-scenegraph rebind must be SEQUENTIAL with SceneGraphViewport::DoRePaintSurface
    (which reads _scenegraph across acquire/release). The editor therefore calls both
    prepare_rebuild + apply_pending_rebuild from _onGpuUpdate and pauses the update-thread sim
    tick during the swap. Single-threaded callers (the gates) invoke them in sequence."""
    if not self._pending_ready:
      return False
    self._consume_pending()
    # re-evaluate the orbit target's Y on the NEW surface at its CURRENT XZ (a user who panned
    # keeps their spot but stays on the rebaked surface — do NOT snap XZ back to center).
    self._refresh_surface_camera(keep_xz=True)
    return True

  ##############################################################################
  # surface-height orbit target + CPU sampler (camera; the C++ display bake heights)
  ##############################################################################

  def _display_heights_path(self):
    # the C++ display bake writes <assetcache>/terrain/<asset>/height.exr; the in-code scene
    # (build_scene_data) derives the asset from product_asset_name(), which keys display
    # bakes to their own dir (#88 v1). One helper keeps camera sampler and bake in sync.
    return self._product_height_path(self.product_asset_name())

  def _load_display_heights(self):
    """Load the C++ display bake's height image into a CPU array (format-normalized to the
    stored [0,1] convention) for the camera surface sampler. Guarded — absent/unreadable =>
    leaves the previous array, returns False (the caller falls back, no crash)."""
    hpath = self._display_heights_path()
    if not os.path.exists(hpath):
      return False
    try:
      import numpy
      from orkengine.lev2 import Image
      img = Image.createFromFile(hpath)
      arr = numpy.array(img.numpy, dtype=numpy.float32)
      if arr.ndim == 3:
        arr = arr[..., 0]
      match img.format_name:
        case "R16" | "RG16" | "RGBA16":
          arr = arr / 65535.0
        case "R8" | "RG8" | "RGBA8":
          arr = arr / 255.0
        case _:
          pass                                             # R32F/RGBA32F: raw float field
      self._cpu_hf = arr
      self._cpu_dim = arr.shape[0]
      return True
    except Exception as e:
      print(f"TerrainRuntime: surface-height load failed ({e}) — camera falls back", flush=True)
      return False

  def terrain_height(self, x, z):
    """Bilinear sample of the baked height (world meters) — SAME mapping as the GPU chunk shader
    (uv = xz/extent + 0.5, texel-center convention; the EXR already carries meters)."""
    if self._cpu_hf is None:
      return 0.0
    d = self._cpu_dim
    u = x / self.extent_m + 0.5
    v = z / self.extent_m + 0.5
    fx = u * d - 0.5
    fy = v * d - 0.5
    x0 = math.floor(fx); y0 = math.floor(fy)
    x1 = min(max(x0 + 1, 0), d - 1); y1 = min(max(y0 + 1, 0), d - 1)
    tx = fx - x0; x0 = min(max(x0, 0), d - 1)
    ty = fy - y0; y0 = min(max(y0, 0), d - 1)
    hf = self._cpu_hf
    h0 = hf[y0, x0] * (1.0 - tx) + hf[y0, x1] * tx
    h1 = hf[y1, x0] * (1.0 - tx) + hf[y1, x1] * tx
    return float(h0 * (1.0 - ty) + h1 * ty)

  def _center_orbit_on_surface(self, keep_xz):
    """Put the orbit target (uicam.center = mvCenter) on the terrain SURFACE. keep_xz=False:
    center it at the terrain-center XZ (initial load). keep_xz=True: keep the current (possibly
    panned) XZ and only re-evaluate Y on the current surface (post-rebake). Guarded."""
    if self._cpu_hf is None or self.uicam is None:
      return
    if keep_xz:
      c = self.uicam.center
      x, z = float(c.x), float(c.z)
    else:
      x, z = 0.0, 0.0
    y = self.terrain_height(x, z)
    self.uicam.center = vec3(x, y, z)
    self.uicam.updateMatrices()
    if self.camera is not None:
      self.camera.copyFrom(self.uicam.cameradata)

  def _refresh_surface_camera(self, keep_xz):
    if self._load_display_heights():
      self._center_orbit_on_surface(keep_xz=keep_xz)

  def bind_viewport(self, sgv, *, camname="spawncam"):
    sgv.scenegraph = self.scenegraph
    sgv.cameraName = camname

  def handle_camera_event(self, uievent):
    if self.uicam is None:
      return lev2.ui.HandlerResult()
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  def gpuUpdate(self, ctx):
    if self.controller:
      self.controller.gpuUpdate(ctx)

  ##############################################################################
  # undo checkpoints — the SPEC'd checkpoint format is doc-JSON; kwargs + the
  # select-as-output key ride along so a restore is a COHERENT editor state.
  # One implementation shared by the editor's UndoStack, the gates, and (S5)
  # the MCP tool surface.
  ##############################################################################

  def set_preview_dim(self, n):
    """Set the editing/preview bake resolution (square W=H). Session state — rides
    the Terrain Parameters sheet. Clamped to [chunk, 16384] and rounded to a chunk
    multiple (the chunked vertex source's grid contract). Rebake to apply; dim is
    part of the cook context hash, so each dim caches independently."""
    n = int(n)
    n = max(self.chunk, min(self.MAX_DIM, n))
    n = ((n + self.chunk // 2) // self.chunk) * self.chunk
    self.preview_dim = n
    print(f"[terrain] preview dim -> {n}", flush=True)
    return n

  def set_extent_m(self, e):
    """Set the world XZ extent (meters across the field) — the DIM's physical scalar:
    texel_m == extent_m / dim. Session state on the Terrain Parameters sheet; heights
    are TRUE METERS so this rescales ONLY the horizontal domain (meter-parameterized
    ops — lpf cutoff (meters), slope radius_m, erosion cell size — re-derive their texel
    footprints from it). Rebake to apply; extent is in the cook context hash."""
    e = float(e)
    if not (e > 0.0):
      raise ValueError(f"extent_m must be > 0 (got {e})")
    self.extent_m = e
    print(f"[terrain] extent -> {e} m ({e / max(1, self.preview_dim):.3f} m/texel at dim {self.preview_dim})",
          flush=True)
    return e

  def capture_undo_state(self):
    """Opaque editor-state snapshot: (doc-JSON, DSL ctor kwargs, display key, dim)."""
    from ork.hypergraph.dflow.terrain.doc import to_json
    return {
        "doc": to_json(self.document) if self.document is not None else None,
        "kwargs": dict(self._dsl_kwargs),
        "display": self._display_key,
        "dim": self.preview_dim,
        "extent_m": self.extent_m,
    }

  def restore_undo_state(self, state):
    """Apply a capture_undo_state() snapshot (L2: the document is REPLACED from its
    doc-JSON checkpoint; kwargs, display key and dim restored to match). The caller
    refreshes its models and schedules the rebake."""
    from ork.hypergraph.dflow.terrain.doc import from_json
    if state.get("doc") is not None:
      self.document = from_json(state["doc"])
    self._dsl_kwargs = dict(state.get("kwargs") or {})
    self._display_key = state.get("display")
    if state.get("dim"):
      self.preview_dim = int(state["dim"])
    if state.get("extent_m"):
      self.extent_m = float(state["extent_m"])

  def display_ready(self):
    """True once the CURRENT scenegraph holds the terrain drawable node (the fresh
    sim staged its content) — the editor's hold-last-frame rebind gate."""
    if self.scenegraph is None or self._tokens is None:
      return False
    try:
      return len(self.scenegraph.drawableNodesWithType(self._tokens.terrain)) > 0
    except Exception:
      return False

  def _push_camera(self):
    if self.controller and self._sys_ref and self.uicam is not None:
      UIC = self.uicam.cameradata
      t = self._tokens
      self.controller.systemNotify(self._sys_ref, t.UpdateCamera, {
          t.eye: UIC.eye, t.tgt: UIC.target, t.up: UIC.up,
          t.near: UIC.near, t.far: UIC.far, t.fovy: UIC.fovy})

  def update(self):
    """Per-frame update (two-thread host): push the camera + tick the simulation.
    The paired GPU work runs in gpuUpdate() on the distinct GPU/render thread."""
    self._push_camera()
    if self.controller:
      self.controller.updateSimulation()

  def update_with_gpu(self, ctx):
    """Single-threaded (headless / test) pump: run the sim update AND its GPU phase on
    the calling thread (which must hold the gfx context). Any GPU rendezvous the update
    queues runs INLINE instead of blocking for a separate gpuUpdate() a single thread
    can never deliver. The two-thread editor host uses update()+gpuUpdate() instead."""
    self._push_camera()
    if self.controller:
      self.controller.updateWithGpu(ctx)

  ##############################################################################
  # headless DOCUMENT bake oracle (gA gates) — NOT part of the display loop
  ##############################################################################

  def rebake(self, dim=None):
    """RE-ELABORATE the document (sole GraphData constructor, L2) and bake it at `dim`
    via lev2.terrain.bake_heightfield — the headless determinism/cache oracle. `dim` is
    part of the cook context hash (preview + full cache independently). Returns a dict;
    self.height_path points at the baked height image."""
    if self.document is None:
      raise RuntimeError("TerrainRuntime.rebake(): no document loaded")
    if self._ctx is None:
      raise RuntimeError("TerrainRuntime.rebake(): no GPU context bound (set_context)")
    dim = int(dim) if dim is not None else self.preview_dim
    g, _cap = self.document.elaborate()
    channels = []
    height_path = None
    for cap in lev2.terrain.capture_modules(g):
      ch_list = [c.strip() for c in (cap.channel or "height").split(",") if c.strip()] or ["height"]
      cap.path = (os.path.join(self._outdir, f"{ch_list[0]}.exr") if len(ch_list) == 1
                  else os.path.join(self._outdir, "{channel}.exr"))
      for c in ch_list:
        channels.append(c)
        if c == "height":
          height_path = os.path.join(self._outdir, "height.exr")
    if height_path is None and channels:
      height_path = os.path.join(self._outdir, f"{channels[0]}.exr")
    lev2.terrain.bake_heightfield(g, self._ctx, dim, extent_m=self.extent_m)
    self.dim = dim
    self.height_path = height_path
    return {"height": height_path, "dim": dim, "channels": channels,
            "extent_m": self.extent_m}
