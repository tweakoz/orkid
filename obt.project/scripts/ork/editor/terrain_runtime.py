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
      # E0: introspect the editable ctor kwargs, then trace with _ParamExpr SYMBOLS for the
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
    """E0 editor trace: instantiate self._dsl_class with _ParamExpr SYMBOLS for each NUMERIC
    editable ctor kwarg, derive the document, attach the params table, and flag every param
    NOT captured as a plug/prop expr STRUCTURAL (a folded / range()-forced / shader-closure /
    unused use — editing it requires the guarded re-trace, never a silent no-op). Numeric
    captured params edit re-trace-free (document mutation + re-elaborate)."""
    from ork.hypergraph.dflow.terrain.doc import (
        _ParamTable, _ParamExpr, _collect_captured_param_names)
    table = _ParamTable()
    for name in self._dsl_editable:
      table.declare(name, effective_kwargs[name], tag=self._dsl_tags.get(name))
    param_kwargs = {}
    for name, val in effective_kwargs.items():
      if (name in self._dsl_editable and isinstance(val, (int, float))
              and not isinstance(val, bool)):
        param_kwargs[name] = _ParamExpr.param(table, name)   # numeric -> symbolic capture
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
    if to_json(fresh) == to_json(self.document):
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
    Exclusive — one node at a time. Validates NOW against the current document
    (DocNode only, captures refused); the key re-resolves at every rebuild."""
    from ork.hypergraph.dflow.terrain.doc import DocNode, find_by_path
    if key is None:
      self._display_key = None
      return None
    obj = find_by_path(self.document, key) if self.document is not None else None
    if not isinstance(obj, DocNode) or obj.clazz_name == "CaptureModule":
      raise ValueError(
          f"set_display_key: {key!r} does not name a displayable document node")
    self._display_key = str(key)
    return obj

  def _resolve_display_node(self):
    """The DocNode for the current display key, or None. A stale key (structural
    change removed the node) CLEARS the override loudly and falls back to the
    document's real captures."""
    if self._display_key is None:
      return None
    from ork.hypergraph.dflow.terrain.doc import DocNode, find_by_path
    obj = find_by_path(self.document, self._display_key)
    if not isinstance(obj, DocNode) or obj.clazz_name == "CaptureModule":
      print(f"[terrain] display node {self._display_key!r} no longer resolves — "
            f"clearing select-as-output.", flush=True)
      self._display_key = None
      return None
    return obj

  def build_scene_data(self, *, dim=None, simple_material=False):
    """Derive a fresh dflow.GraphData from the DOCUMENT (L2) and lower a minimal
    one-terrain-entity ECS scene around it into an ecs.SceneData. The embedded graph
    rides HeightFieldGenData; the C++ terrain path bakes + renders it at load. `dim`
    is the render + bake grid; simple_material forces a plain Solid look (visibility
    is material-independent — used by the headless snapshot gate)."""
    from ork.hypergraph.ecs.scene import Scene
    from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
    from ork.hypergraph.assets.materials.terrain.solid import Solid
    from orkengine.lev2 import HeightFieldGenData, TerrainChunkDrawableData

    dbg_materials = terrain_debug_materials()  # [M] the data-driven debug-material cycle (single source)

    if self.document is None:
      raise RuntimeError("TerrainRuntime.build_scene_data(): no document loaded")
    dim = int(dim) if dim is not None else self.dim
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

    extent_m, chunk = self.extent_m, self.chunk
    skybox = self.skybox_path

    class _TerrainDocScene(Scene):
      def __init__(self):
        super().__init__()
        SG = self.scenegraph(
            preset="ForwardPBR", skybox_path=skybox,
            SkyboxIntensity=2.0, DiffuseIntensity=1.0, SpecularIntensity=1.0,
            AmbientLight=vec3(0.10),
            ssaa=2,
            msaa=2)  # scene param -> _mergedParams -> fwd node MSAA RtGroup (3=8x)
        # embed the DOCUMENT's elaborated graph directly (no DSL re-trace) — the
        # HeightFieldGenData is what serializes + defers its bake to the C++ load.
        gd = HeightFieldGenData(dimension=dim, extent_m=extent_m, graph=graph)
        gd.asset_name = "terra"
        self._asset_gens.append(("terra", gd))
        # bake_dim = MAX_DIM: the vertex-source SHADER TEXT bakes the per-chunk array
        # caps + byte offsets from bake_dim — capping at the slider ceiling makes the
        # text CONSTANT across every editor dim, so a dim change recompiles NO
        # rendering materials (they all share this one vertex source). Cost: a fixed
        # ~200KB header region; heights stay dense at the ACTUAL dim (runtime-sized).
        vs = TerrainChunkVertexSource(dim=dim, bake_dim=TerrainRuntime.MAX_DIM,
                                      extent_m=extent_m, chunk=chunk, relax=relax)
        if os.environ.get("ORKID_TERRAIN_DIMLOG"):
          print(f"[terrain-dim] SCENE dim={dim} vs(bake_dim={vs.bake_dim} maxnc={vs.maxnc} "
                f"HEIGHTS_OFF={vs.HEIGHTS_OFF} TOTAL={vs.TOTAL}) extent={extent_m} "
                f"chunk={chunk}", flush=True)
        self.asset.Ptex3d("terra_mat", dsl_class=mat_cls, vertex_source=vs, **mat_params)
        # [M] material-override debug looks — each its OWN FWD_SSBO_CUSTOM material, sharing the
        # SAME vertex_source (identical SSBO layout) so the C++ terrain drawable can swap to any of
        # them at runtime (SceneGraphSystem SetTerrainMaterialMode). The ORDERED name list is DATA:
        # it rides the drawable's reflected debug_material_assets, and the C++ [M] cycle resolves +
        # cycles WHATEVER it names — so this loop is the only place a new debug look is registered.
        for _name, _cls in dbg_materials:
          self.asset.Ptex3d(_name, dsl_class=_cls, vertex_source=vs)
        self.entity("terrain0", components=[SG.component(nodes={
            "terra": {"drawable": TerrainChunkDrawableData(
                hf_asset="terra", material_asset="terra_mat", chunk=chunk,
                layout_dim_cap=TerrainRuntime.MAX_DIM,   # MUST equal the vs bake_dim above
                debug_material_assets=[n for n, _c in dbg_materials])},
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
    # (build_scene_data) names the terrain "terra". One name keeps both in sync.
    d = str(_Path.expandPathString("<assetcache>/terrain/terra"))
    return os.path.join(d, "height.exr")

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
    ops — lpf cutoff_m, slope radius_m, erosion cell size — re-derive their texel
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
