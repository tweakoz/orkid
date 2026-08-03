################################################################################
# dflowedit — the standalone dataflow graph editor shell (ork.dflow.edit.py).
#
# A FAMILY-NEUTRAL desktop editor for any dflow graph family: viewport + toolbar +
# generic GPU node-editor canvas + property sheet. JUL13_DFLOW E3 — "a standalone
# ork.dflow.edit.py shell (dock + canvas + propsheet + viewport) that loads any family
# via the E2 protocol". Layout rides the ui.DockSpace substrate (viewport fill; left
# column @0.4 hosting VerticalPack[Toolbar, canvas TabsWidget]; property sheet @0.55 of
# the left column), persisted per (app-name, entry-path) via ork.ui.app_state — Shift+L
# resets it live, --reset-layout ignores the saved session. NOT terrain-coupled: the
# shell CORE binds ONLY the C0 NodeGraphModel protocol (the canvas)
# and the E2 GraphDocument protocol (the property sheet). Families plug in via the small
# loaders below, which are the ONLY place a family (terrain / particles) is imported.
#
# v1 = LOAD + VIEW + EDIT (select -> propsheet, move/persist positions, bypass/display
# flags where the family supports them). Live viewport PAYLOADS now host RUNNING scenes:
# terrain (live-bake), hypermesh (live mesh), and E7 particles (a RUNNING particle system,
# transport-gated). A doc-only family (a non-particles .orj GraphData) keeps the placeholder
# viewport; the viewport_setup / viewport_compose seam is the payload plug-in point. Saving:
# .orj re-serialize for GraphData families, ALWAYS through the house file requester (Save
# STATES where to write — DSL-backed docs default to the asset dir + <name>.orj, .orj-backed
# docs to their own path; NEVER an implicit cwd write). terrain Save (the .py pywriter) stays
# out (terrainedit owns it).
################################################################################

import os

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from orkengine import ecs
from ork.app.application import ComponentizedApplication
from ork.editor import save_target
from ork.ui import icon_library
from ork.ui.node_editor import NodeEditor, COL_BG
from ork.ui.dock_layout import save_layout, load_layout, to_json
from ork.ui.dock_editor_glue import EditorDockGlue
from ork.ui.app_state import dock_layout_path, text_input_has_focus

tokens = CrcStringProxy()

# default dock layout: viewport (fill) | left column [toolbar + canvas tabs] split
# LEFT @0.4; property sheet split BOTTOM @0.55 of the left column. Panel titles are
# BOTH the titlebar labels AND the stable persistence ids (name-keyed layout JSON).
_DOCK_VIEWPORT_TITLE = "Viewport"
_DOCK_LEFT_TITLE     = "Graph"
_DOCK_PROPS_TITLE    = "propsheet"
_DOCK_LEFT_PROP      = 0.4
_DOCK_PROPS_PROP     = 0.55
_DOCK_SPLIT_MARGIN   = 2


################################################################################
# Transport — the family-neutral start/pause/stop contract the shell surfaces as
# toolbar buttons for EVERY family. A family with a live ECS viewport host binds its
# host (which implements this contract); a doc-only family (graphdata placeholder) binds
# the NullTransport (the buttons are present but inert — nothing is running).
################################################################################

class NullTransport:
  """No-op transport for doc-only families (no ECS viewport to run)."""
  def start(self):
    return "stopped"

  def pause(self):
    return "stopped"

  def stop(self):
    return "stopped"

  @property
  def state(self):
    return "stopped"


################################################################################
# Family binding + loaders (the ONLY family-aware code; the shell core below never
# imports terrain or particles). A loader returns a _Binding: a C0 node model (canvas),
# an E2-document-backed property model (property sheet), a root-crumb title, the document
# (for Save), an optional save(path)/viewport_setup(app, ctx) seam, and — for a family
# with a live viewport — an ECS viewport host + its transport.
################################################################################

class _Binding:
  __slots__ = ("node_model", "prop_model", "title", "document", "save",
               "viewport_setup", "viewport_compose", "family", "viewport_host",
               "transport", "dsl_source", "bench_prop_model")

  def __init__(self, node_model, prop_model, title, document,
               save=None, viewport_setup=None, viewport_compose=None, family="",
               viewport_host=None, transport=None, dsl_source=None, bench_prop_model=None):
    self.node_model = node_model
    self.prop_model = prop_model
    self.title = title
    self.document = document
    self.save = save                    # callable(path) -> path, or None (unsupported)
    self.viewport_setup = viewport_setup  # callable(app, ctx) -> PRIMARY: create scene+bind, or None
    self.viewport_compose = viewport_compose  # callable(app, primary_host, ctx) -> CONTRIBUTOR:
                                          # fold this payload into the primary's scene, or None
    self.family = family
    self.viewport_host = viewport_host    # ECS viewport host (live scene), or None
    self.transport = transport if transport is not None else NullTransport()
    self.dsl_source = dsl_source          # the .py DSL path if graph came from CODE, else None
                                          # (gates Save: DSL code is never written in-place —
                                          # Save exports a fresh .orj, .py left untouched)
    self.bench_prop_model = bench_prop_model  # BenchPropertyModel (the DISTINCT bench section), or
                                          # None (no TESTBENCH, or benches disabled at the shell)


def _stem(source):
  try:
    return os.path.splitext(os.path.basename(str(source)))[0] or "graph"
  except Exception:
    return "graph"


def _envmaps_dir():
  return os.path.join(os.environ.get("OBT_STAGE", ""), "assetcache", "envmaps2")


def resolve_envmap_override(name):
  """Resolve an -e/--envmap CLI override to a full <ork_envmaps2>/<name>.xir path, VERIFYING
  existence at the SHELL level (the engine crashes on a missing envmap — issue #54; we refuse
  BEFORE the engine ever sees it). A BARE name resolves against the on-disk envmaps2 dir and
  raises FileNotFoundError (listing what IS available) when absent; an explicit <bracketed> alias
  or an absolute/relative filesystem path is verified when it names a real file, else trusted with
  a loud warning (a bracketed alias can't be stat'd before the engine resolves it). Empty -> None
  (each host keeps its default envmap). Mirrors the sibling viewers' -e value form (shortname OR
  bracketed/absolute path)."""
  if not name:
    return None
  s = str(name)
  if s.startswith("<") or os.path.isabs(s) or "/" in s:
    if os.path.isfile(s):
      return s
    if s.startswith("<"):
      print(f"[dflowedit] envmap override {s!r} is a path alias — passing through unverified "
            f"(shell existence check applies to BARE names)", flush=True)
      return s
    raise FileNotFoundError(f"envmap override path not found: {s}")
  envdir = _envmaps_dir()
  fpath = os.path.join(envdir, s + ".xir")
  if not os.path.isfile(fpath):
    import glob as _glob
    avail = sorted(os.path.splitext(os.path.basename(f))[0]
                   for f in _glob.glob(os.path.join(envdir, "*.xir")))
    raise FileNotFoundError(
        f"envmap {s!r} not found under {envdir} — available: {avail}")
  return f"<ork_envmaps2>/{s}.xir"


def _graphdata_file_family(path):
  """Family for a reflection-JSON graph FILE (.orj / .json), or None if it is not a GraphData.

  A GraphData is classified by the namespace of its MODULE classes: a graph carrying any
  hypermesh:: module (box/extrude/L-system meshes AND the roads R-family, which is hypermesh
  C++) routes to 'hypermesh' — so a saved hypermesh graph gets the mesh viewport + the
  hypermesh Tab-add palette, and a roads graph opens with the hypermesh node model (view/edit;
  it has no mesh terminal, so no viewport). Every other GraphData (particles, terrain-doc-less)
  stays the generic 'graphdata' family. An unreadable .orj falls back to 'graphdata' (the
  pre-existing byte-for-byte behavior)."""
  try:
    import json
    with open(path, "r") as f:
      root = json.load(f)
  except Exception:
    return "graphdata" if str(path).lower().endswith(".orj") else None
  obj = root.get("root", {}).get("object", {})
  # the serialized GraphData class token is "dflow/graphdata" (lowercase) — match it robustly
  # (the pre-existing "GraphData" substring probe never matched, so a GraphData .json used to
  # misroute; a .orj is a GraphData by construction).
  cls = (obj.get("class", "") or "").lower()
  if "graphdata" not in cls:
    return None
  mods = obj.get("properties", {}).get("Modules", {}) or {}
  for entry in mods.values():
    cls = entry.get("object", {}).get("class", "") or ""
    if cls.startswith("hypermesh::"):
      return "hypermesh"
  return "graphdata"


def _resolve_family(source):
  """Resolve `source` to (family, resolved) — the single family-detection authority.

  Resolution priority (documented; the order matters for a bare NAME that could exist in
  more than one family's search path):
    1. a reflection-JSON graph (.orj, or a .json whose root object is a dflow::GraphData):
       family by MODULE NAMESPACE (hypermesh:: -> 'hypermesh'; else 'graphdata').
    2. a name/.py the TERRAIN resolver resolves on ORK_TERRAIN_SEARCH_PATH -> 'terrain'.
    3. a name/.py the PARTICLES resolver resolves on ORK_PARTICLES_SEARCH_PATH -> 'particles'.
    4. a filename stem (or .py path) the HYPERMESH asset resolver resolves under
       assets/hypermesh/ -> 'hypermesh' (box, extrude_demo, ls_*, ...).

  TERRAIN WINS an ambiguous bare-name tie (it is tried first), then particles, then hypermesh:
  if a name existed in more than one search path it opens as the earlier family. As of this
  writing NO bare name collides across the three default asset dirs (<hypergraph>/assets/terrain,
  <ork.data>/particles, <hypergraph>/assets/hypermesh) — notably 'box' is a HYPERMESH-only asset,
  so it resolves cleanly to hypermesh.

  Unresolvable in EVERY family -> one loud FileNotFoundError naming what was searched
  (ops-self-defend: never a silent misroute)."""
  s = str(source)
  low = s.lower()
  if low.endswith(".orj") or (low.endswith(".json") and os.path.isfile(s)):
    fam = _graphdata_file_family(s)
    if fam is not None:
      return (fam, s)

  from ork.hypergraph.dflow.terrain import resolve as terrain_resolve
  from ork.hypergraph.dflow.particles import resolve as particles_resolve

  try:
    return ("terrain", str(terrain_resolve.resolve_dsl_file(s)))
  except FileNotFoundError:
    pass
  try:
    return ("particles", str(particles_resolve.resolve_dsl_file(s)))
  except FileNotFoundError:
    pass
  if _hypermesh_name_resolvable(s):
    return ("hypermesh", s)

  terr_dirs = "\n    ".join(str(d) for d in terrain_resolve.search_path())
  ptc_dirs = "\n    ".join(str(d) for d in particles_resolve.search_path())
  hm_dir = _hypermesh_assets_dir()
  raise FileNotFoundError(
      f"could not resolve dflow source {s!r} in ANY family.\n"
      f"  terrain search path:\n    {terr_dirs}\n"
      f"  particles search path:\n    {ptc_dirs}\n"
      f"  hypermesh assets dir:\n    {hm_dir}\n"
      f"(a .orj / .json GraphData opens directly; set ORK_TERRAIN_SEARCH_PATH / "
      f"ORK_PARTICLES_SEARCH_PATH to override the defaults)")


def _hypermesh_assets_dir():
  try:
    from ork.hypergraph.assets.hypermesh import _resolve as hm_resolve
    return hm_resolve.assets_dir()
  except Exception:
    return "<unavailable>"


def _hypermesh_name_resolvable(source):
  """True if `source` is a hypermesh asset — a filename stem under assets/hypermesh/ OR a .py
  path holding a Hypermesh subclass. Import-light: a bare name only lists the asset dir."""
  s = str(source)
  try:
    from ork.hypergraph.assets.hypermesh import _resolve as hm_resolve
  except Exception:
    return False
  if s.lower().endswith(".py") and os.path.isfile(s):
    return True
  base = os.path.splitext(os.path.basename(s))[0]
  try:
    return base in hm_resolve.list_asset_names()
  except Exception:
    return False


def _detect_family(source):
  """Family label for `source` (see _resolve_family for the priority order). Best-effort:
  an unresolvable source returns '' here so app construction proceeds; the loud combined
  error is raised (and caught + reported) later, when load_family actually resolves."""
  try:
    fam, _resolved = _resolve_family(source)
    return fam
  except Exception:
    return ""


def _load_graphdata(source):
  """(a) .orj / GraphData reflection-JSON -> deserialize -> GraphDataDocument -> the
  GraphData family adapter. Save re-serializes the .orj (serializeJson round-trip)."""
  from ork.hypergraph.dflow.graphdata_document import GraphDataDocument
  from ork.editor.graphdata_node_model import GraphDataNodeGraphModel
  from ork.editor.graphdoc_models import GraphDocumentPropertyModel

  with open(source, "r") as f:
    data = f.read()
  doc = GraphDataDocument.from_json(data)
  node_model = GraphDataNodeGraphModel(doc)
  prop_model = GraphDocumentPropertyModel(document=doc, on_changed=None)

  def _save(path):
    js = doc.to_json()
    with open(path, "w") as f:
      f.write(js)
    print(f"[dflowedit] saved .orj -> {path}", flush=True)
    return path

  return _Binding(node_model=node_model, prop_model=prop_model, title=_stem(source),
                  document=doc, save=_save, family="graphdata")


def _load_terrain(source, *, skybox=None):
  """(b) terrain DSL name/.py -> a LIVE ECS viewport host: TerrainEcsViewportHost owns a
  TerrainRuntime (the in-code ForwardPBR terrain scene the C++ path bakes + renders) and
  serves as the node-model host, so canvas edits + display/bypass flags drive real
  rebakes. The GPU-side scene build happens in viewport_setup (needs a ctx); the document
  is available immediately (pure-python trace). Save stays out (ork.terrain.edit.py owns
  the pywriter). `skybox` = an -e/--envmap override (full .xir path) -> the runtime skybox."""
  from ork.editor.terrain_viewport_host import TerrainEcsViewportHost
  from ork.editor.terrain_node_model import TerrainNodeGraphModel
  from ork.editor.terrain_doc_model import TerrainNodePropertyModel

  host = TerrainEcsViewportHost(source, preview_dim=256, chunk=128, skybox=skybox)
  doc = host.runtime.document
  node_model = TerrainNodeGraphModel(host, host.runtime)   # host = node-model host; runtime = doc/display
  prop_model = TerrainNodePropertyModel(on_changed=None)

  def _viewport_setup(app, ctx):        # PRIMARY: create scene+sim+camera, bind the viewport
    host.gpuInit(ctx)                   # camera + post-fx + live ECS scene (GPU thread)
    host.bindViewport(app.sgv)          # populate the shell's viewport widget

  def _viewport_compose(app, primary_host, ctx):  # CONTRIBUTOR: fold into the primary's scene
    host.composeInto(primary_host, ctx)

  return _Binding(node_model=node_model, prop_model=prop_model,
                  title=host.runtime.source_label,
                  document=doc, save=None, family="terrain",
                  viewport_setup=_viewport_setup, viewport_compose=_viewport_compose,
                  viewport_host=host, transport=host)


def _load_particles(source, *, skybox=None, apply_bench=True):
  """(c) particles DSL name/.py -> import the ParticleSystem subclass, generatedflow() it
  into a live dflow.GraphData, then wrap that graph in the SAME GraphData machinery the
  .orj family uses (GraphDataDocument + GraphDataNodeGraphModel + GraphDocumentPropertyModel)
  so the canvas / propsheet / add / delete / connect behave IDENTICALLY to a .orj source.
  This is the established consumption path (ork.particle.viewer.py: resolve_dsl_file ->
  load_dsl_class -> instance.generatedflow()).

  E7: the graph is ALSO handed to a ParticlesViewportHost that hosts a RUNNING particle
  system built over THAT SAME live GraphData (not a re-load) — the shell's viewport shows the
  particles animating, gated by the family-neutral start/pause/stop transport, and folded over
  a terrain primary in multi-doc. A structural/param edit re-instantiates the drawable (the
  running GraphInst copies literal plug values at instantiation, so an edit shows on the next
  rebake — never a silent stale-graph display).

  TESTBENCH (editor-only stimulus): if the asset module declares a module-level TESTBENCH and
  `apply_bench`, its construction kwargs drive the EDITING instantiation (e.g. fireball's
  emitter_entity="@bench") and its motion program drives the emitter through the graphinst's
  entity resolver on the transport tick. The bench is resolved ONLY here (getattr) — never on a
  production/instantiate path — and never enters a cook hash. apply_bench=False reproduces the
  exact pre-bench graph (production parity), which is how the byte-parity gate builds its baseline.

  The .py DSL is CODE: in-place Save is refused (see DflowEditor._doSave). Save exports a
  fresh doc-less .orj (reusing GraphDataDocument.to_json — the exact .orj save path), leaving
  the .py source untouched."""
  from ork.hypergraph.dflow.particles import resolve as particles_resolve
  from ork.hypergraph.dflow.particles.capabilities import ParticlesEditorCapabilities
  from ork.hypergraph.dflow.graphdata_document import GraphDataDocument
  from ork.hypergraph.dflow.testbench import resolve_testbench
  from ork.editor.graphdata_node_model import GraphDataNodeGraphModel
  from ork.editor.graphdoc_models import GraphDocumentPropertyModel, BenchPropertyModel
  from ork.editor.particles_viewport_host import ParticlesViewportHost

  dsl_path = particles_resolve.resolve_dsl_file(source)
  dsl_module = particles_resolve.load_dsl_module(dsl_path)
  dsl_class = particles_resolve.class_in_module(dsl_module, dsl_path)
  testbench = resolve_testbench(dsl_module) if apply_bench else None

  def _graph_for(enabled):
    """Re-instantiate the DSL for the given bench-enabled state — enabled folds in the bench's
    construction kwargs (a genuinely different EDITING graph); disabled == the production graph."""
    kwargs = dict(testbench.instantiate) if (testbench is not None and enabled) else {}
    return dsl_class(**kwargs).generatedflow()

  bench_enabled = bool(testbench is not None and testbench.enabled)
  graphdata = _graph_for(bench_enabled)            # the populated dflow.GraphData

  # the particles family capability mask (role-based bypassability + no display flags) —
  # ONE object shared by the family-neutral editor document (badges / bypass refusal /
  # display affordance) and the viewport host (effective-graph construction on rebake).
  caps = ParticlesEditorCapabilities()
  doc = GraphDataDocument(graphdata, capabilities=caps)
  node_model = GraphDataNodeGraphModel(doc)
  prop_model = GraphDocumentPropertyModel(document=doc, on_changed=None)

  # the live viewport payload: a running particle system over THE SAME GraphData the canvas edits.
  host = ParticlesViewportHost(graphdata, title=_stem(dsl_path), skybox=skybox, capabilities=caps,
                               testbench=testbench, bench_enabled=bench_enabled,
                               graph_factory=_graph_for)
  # structural edits (add/delete/connect via the node model) re-instantiate the particle system.
  node_model.on_changed(host._requestRebake)

  # the DISTINCT bench propsheet section (enabled toggle + LIVE motion params), or None.
  bench_prop_model = BenchPropertyModel(testbench, host) if testbench is not None else None

  def _save(path):
    js = doc.to_json()                             # graph IS the document -> plain .orj bytes
    with open(path, "w") as f:
      f.write(js)
    print(f"[dflowedit] exported particles DSL -> {path}", flush=True)
    return path

  def _viewport_setup(app, ctx):        # PRIMARY: create scene + running drawable + camera, bind
    host.gpuInit(ctx)
    host.bindViewport(app.sgv)

  def _viewport_compose(app, primary_host, ctx):  # CONTRIBUTOR: fold the particles into the primary
    host.composeInto(primary_host, ctx)

  return _Binding(node_model=node_model, prop_model=prop_model, title=_stem(dsl_path),
                  document=doc, save=_save, family="particles",
                  viewport_setup=_viewport_setup, viewport_compose=_viewport_compose,
                  viewport_host=host, transport=host, dsl_source=str(dsl_path),
                  bench_prop_model=bench_prop_model)


def _graph_produces_mesh(doc):
  """True if any module in the graph has a GpuMesh output plug — i.e. the graph materializes to
  a renderable mesh (box/extrude/L-system) vs a field/roads graph (RouteSpine/RoadbedMask emit
  XfNodeGraph/HfImage/InstanceSet, never a GpuMesh). Reflection-only (plugSpec), no bake."""
  from orkengine.core import dataflow as _dflow
  for (_name, cls) in doc.nodes():
    if not cls:
      continue
    spec = _dflow.plugSpec(cls) or {}
    for p in spec.get("outputs", []):
      if "GpuMesh" in (p.get("type") or ""):
        return True
  return False


def _load_hypermesh(source, *, skybox=None, material_mode=None):
  """(d) HYPERMESH: a bare asset name / .py (a Hypermesh subclass -> generatedflow()) OR a saved
  hypermesh reflection-JSON graph -> a HypermeshDocument, the hypermesh node model, and — when
  the graph materializes a mesh — a live viewport payload (materialize_live -> make_drawable).

  A field/roads (R-module) graph has no mesh terminal: it opens VIEW/EDIT only (no viewport
  host), exactly like a doc-only family, so an R-graph opens with R-module nodes+edges and no
  RoadMesh dependency. A bare-name/.py source is DSL CODE: in-place Save is refused (exports a
  fresh .orj), consistent with the particles path."""
  from orkengine.core import Object as _Object
  from ork.hypergraph.dflow.hypermesh_document import HypermeshDocument
  from ork.editor.hypermesh_node_model import HypermeshNodeGraphModel
  from ork.editor.hypermesh_viewport_host import HypermeshViewportHost
  from ork.editor.graphdoc_models import GraphDocumentPropertyModel

  s = str(source)
  low = s.lower()
  asset = None
  dsl_source = None
  if low.endswith(".orj") or (low.endswith(".json") and os.path.isfile(s)):
    with open(s, "r") as f:
      graph = _Object.deserializeJson(f.read())
    is_animated = not bool(getattr(graph, "cacheable", True))   # animated graphs are not cacheable
    title = _stem(s)
  else:
    from ork.hypergraph.assets.hypermesh import _resolve as hm_resolve
    if low.endswith(".py") and os.path.isfile(s):
      asset_cls = hm_resolve.load_asset_from_path(os.path.abspath(s))
      dsl_source = os.path.abspath(s)
      title = _stem(s)
    else:
      asset_cls = hm_resolve.resolve_asset(os.path.splitext(os.path.basename(s))[0])
      dsl_source = s
      title = os.path.splitext(os.path.basename(s))[0]
    asset = asset_cls()
    graph = asset.generatedflow()
    is_animated = bool(getattr(asset, "is_animated", False))

  doc = HypermeshDocument(graph)
  node_model = HypermeshNodeGraphModel(doc)
  prop_model = GraphDocumentPropertyModel(document=doc, on_changed=None)

  host = None
  viewport_setup = None
  viewport_compose = None
  if _graph_produces_mesh(doc):
    host = HypermeshViewportHost(graph, asset=asset, is_animated=is_animated, title=title,
                                 skybox=skybox, material_mode=material_mode)
    # structural edits (add/delete/connect via the node model) re-bake the mesh viewport.
    node_model.on_changed(host._requestRebake)

    def viewport_setup(app, ctx):     # PRIMARY: build scene + mesh drawable + bind the widget
      host.gpuInit(ctx)
      host.bindViewport(app.sgv)

    def viewport_compose(app, primary_host, ctx):  # CONTRIBUTOR: fold the mesh into the primary
      host.composeInto(primary_host, ctx)
  else:
    print(f"[dflowedit] hypermesh graph {title!r} has no GpuMesh terminal (field/roads graph) — "
          f"opening VIEW/EDIT only (no viewport payload)", flush=True)

  def _save(path):
    js = doc.to_json()
    with open(path, "w") as f:
      f.write(js)
    print(f"[dflowedit] saved hypermesh graph -> {path}", flush=True)
    return path

  return _Binding(node_model=node_model, prop_model=prop_model, title=title,
                  document=doc, save=_save, family="hypermesh",
                  viewport_setup=viewport_setup, viewport_compose=viewport_compose,
                  viewport_host=host, transport=host, dsl_source=dsl_source)


def load_family(source, *, skybox=None, material_mode=None, apply_bench=True):
  """Dispatch to the family loader for `source` (family auto-detected from content;
  see _resolve_family for the priority order + the loud unresolvable error). `skybox` /
  `material_mode` are the -e/--envmap / --material overrides — applied only to the families
  that host a viewport payload (terrain + particles get the skybox; hypermesh gets the skybox +
  material; doc-only graphdata ignores them). `apply_bench` gates the particles TESTBENCH seam
  (editor-only stimulus); False reproduces the exact pre-bench graph."""
  fam, _resolved = _resolve_family(source)
  if fam == "graphdata":
    return _load_graphdata(source)
  if fam == "particles":
    return _load_particles(source, skybox=skybox, apply_bench=apply_bench)
  if fam == "hypermesh":
    return _load_hypermesh(source, skybox=skybox, material_mode=material_mode)
  return _load_terrain(source, skybox=skybox)


################################################################################
# The shell application
################################################################################

class DflowEditor(ComponentizedApplication):
  """Standalone dflow graph editor. Family-neutral: the loaded _Binding supplies the
  canvas node model, the property-sheet model, and (later) a viewport payload; the shell
  wires them into the dock idioms and the generic node editor."""

  def __init__(self, sources, *, offscreen=False, selftest=False, flagtest=False,
               envmap=None, material=None, bypass_ab=None, benches=True, bench_ab=None,
               reset_layout=False, layout_probe=False, layouttest=False, uirecord=None,
               uiplay=None, uiplay_exit=False):
    super().__init__()
    # --uirecord diagnostic tap (ork.uitest session capture; terrainedit's pattern):
    # path captured before createEzApp so _onUiInit can attach once contexts exist.
    self._uirecord_path = uirecord
    self._uirecorder = None
    # --uiplay: frame-locked replay of a recorded session into THIS live editor.
    # Driven from _onGpuUpdate (the render thread — the editors' proven injection
    # spot; event dispatch is main-thread-serialized there). uiplay_exit=True
    # signals app exit shortly after the replay completes (headless replay runs).
    self._uiplay_path = uiplay
    self._uiplay = None
    self._uiplay_exit = bool(uiplay_exit)
    self._uiplay_tick = 0
    self._uiplay_settle = 30        # gpu ticks of warm-up before the clock baselines
    self._uiplay_done_tick = None
    # Replay clock = the UPDATE-thread counter (the recorder's stamp clock). Render
    # ticks run SLOWER than UPS in a live windowed session — counting them stretched
    # playback (owner-observed). _onUpdate publishes the counter (update thread);
    # the render-thread drive reads it (latest-value int publish, recorder pattern).
    self._uiplay_update_counter = 0
    self._uiplay_base = None
    # TESTBENCH stimulus (editor-only): benches=True (default) resolves + applies a particles
    # asset's module-level TESTBENCH in the EDITING instantiation; benches=False reproduces the
    # exact pre-bench graph (production parity — the byte-parity + bypass-determinism gates set it).
    self._benches = bool(benches)
    # BENCH-A/B CAPTURE MODE (offscreen LIVE-params gate driver): {"advance": N, "out": prefix}.
    # Advances the bench sim, captures, tweaks a LIVE motion param via the bench propsheet model
    # (expect: next frame DIFFERS, rebuild counter UNCHANGED), then toggles `enabled` (expect: the
    # rebuild counter INCREMENTS — the honest re-instantiation), and prints a BENCH_AB verdict.
    self._bench_ab = dict(bench_ab) if bench_ab else None
    # BYPASS-A/B CAPTURE MODE (deterministic gate driver, offscreen): a dict
    # {"bypass": [node_names], "advance": N, "out": path}. After the scene builds, the
    # focused binding's model set_bypasseds each name (the canvas path) + rebakes, the sim
    # advances exactly N PLAYING ticks from that effective-graph instance, and the viewport
    # framebuffer is written (RGB numpy .npy) to `out`, then the app exits. Determinism: a
    # fresh process starts the C-library rand() at its default seed and the ONLY per-tick
    # rand() consumer is the emitter (the bypassed chain forces consume none), so two
    # processes that differ ONLY in bypass emit the SAME particle stream — the pixel delta
    # isolates the bypass EXACTLY (an empty bypass reproduces the baseline bit-for-bit).
    self._bypass_ab = dict(bypass_ab) if bypass_ab else None
    # MULTI-DOCUMENT: one shell, N sources open simultaneously (each a _Binding + canvas
    # tab), one composed viewport. A single source is the N==1 degenerate case (behavior
    # byte-for-byte unchanged).
    self._sources = list(sources) if isinstance(sources, (list, tuple)) else [sources]
    # OVERRIDES (-e/--envmap, --material): resolved / validated NOW, before booting the engine, so a
    # bogus value fails LOUDLY at the shell (never reaching the engine's crash-on-missing-envmap path).
    # v1 multi-doc policy: the override applies to ALL payload bindings (envmap -> every terrain +
    # hypermesh payload; material -> every hypermesh payload; terrain keeps its own material system).
    self._envmap = resolve_envmap_override(envmap)
    self._material = material or None
    if self._material is not None:
      from ork.editor.hypermesh_viewport_host import _MATERIAL_MODES
      if self._material not in _MATERIAL_MODES:
        raise ValueError("unknown --material %r; choices: %s"
                         % (self._material, ", ".join(_MATERIAL_MODES)))
    if self._envmap:
      print(f"[dflowedit] envmap override -> {self._envmap}", flush=True)
    if self._material:
      print(f"[dflowedit] material override -> {self._material}", flush=True)
    self._selftest = bool(selftest)
    self._flagtest = bool(flagtest)
    self._offscreen = bool(offscreen or selftest or flagtest or self._bypass_ab
                           or self._bench_ab or layout_probe or layouttest)
    self._title = _stem(self._sources[0])
    self._bindings = []             # per-source _Binding (populated in _onGpuInit)
    self._node_editors = []         # per-binding NodeEditor (parallel to _bindings)
    # (node_editor, host_window) pairs queued by a POST-BOOT Graph-column factory recreate
    # (transfer / tear-out / return); drained on the GPU/render thread so each fresh editor's
    # glyph textures init against the DESTINATION window's own context (a cross-context seam
    # otherwise). One entry per binding per recreate.
    self._ne_pending_gpuinit = []
    self._canvases = []             # per-binding PrimCanvas (created by the Graph factory)
    self._tab_labels = []           # per-binding tab label (created in _onUiInit)
    self._focused_index = 0
    self._focused_binding = None
    self._binding = None            # == the FOCUSED binding (Save/flags/propsheet target)
    # per-doc last-save path (keyed by binding index): after a successful dialog save, a
    # subsequent Save re-writes THAT path with no dialog (Save-As always dialogs). Session-
    # scoped only — never persisted, never a cwd fallback.
    self._save_paths = {}
    self._pending_focus = None      # gate-requested tab switch, APPLIED on the main thread
    self.tabs = None
    self.node_editor = None         # == the FOCUSED NodeEditor (Fit / key routing target)
    self.propsheet = None
    self.sgv = None
    self.scene = None
    self._viewport_host = None      # the PRIMARY host (shared sim), or None (doc-only shell)
    # window-per-field expression editors (owner refinement 2026-07-19): keyed by (node, field),
    # each floats + carries its own controller; opening a field that already has a live window
    # raises it (no duplicate), and the registry tears every window down on editor exit (no leaks).
    from ork.ui.expr_detail_editor import ExprEditorRegistry
    self._expr_registry = ExprEditorRegistry(
        place=lambda count: self._computeExprWindowRect(count))

    # dock substrate (S6 adoption): DockSpace + panels built in _onUiInit. _default_layout
    # captures the canonical arrangement so a live reset (Shift+L) / --reset-layout can
    # restore it regardless of what a persisted session loaded at startup.
    self.dock = None
    self._default_layout = None
    self._dock_glue = None                     # W5 cross-window DockManager wiring
    self._gpu_ready = False                    # set in _onGpuInit; gates factory GPU wiring
    self._reset_layout = bool(reset_layout)    # --reset-layout: ignore saved session
    self._reset_layout_pending = False         # Shift+L: apply on the next GPU tick
    # session persistence is DISABLED for the ephemeral automated gate modes: they run
    # multiple short-lived editor processes and a save-on-exit/restore-on-start round trip
    # would couple them (breaking the bypass/bench cross-process determinism). Interactive
    # runs and the explicit layout gates (layout_probe / layouttest) keep it.
    self._persist_layout = not (bool(selftest) or bool(flagtest)
                                or bool(self._bypass_ab) or bool(self._bench_ab))
    # read-only settle probe (session restore / --reset-layout gate): print the
    # as-constructed dock signature then exit WITHOUT persisting.
    self._layout_probe = bool(layout_probe)
    self._layout_probe_frame = 0
    # scripted DOCK-LAYOUT gate: real-editor structural round-trip + live Shift+L reset
    # + injected titlebar drag (runs on the GPU/main thread, render-sequential).
    self._layouttest = bool(layouttest)
    self._layouttest_frame = 0
    self._layouttest_stage = "settle"
    self._layouttest_settle_at = 0
    self._layouttest_results = {}

    # selftest (offscreen gate): after the scene settles, assert the canvas model is
    # bound, the viewport renders non-black, and the ECS transport advances/freezes,
    # print a verdict, exit CLEAN. flagtest: display-flag pixel diff + bypass rebake.
    self._selftest_frame = 0
    self._selftest_done = False
    self._st = None                 # scripted gate state machine (selftest / flagtest)

    # offscreen framebuffer capture (async readback drained across frames — the C++
    # player's os_snapdrain pattern). Only armed for the offscreen gates.
    self._cap_pending = False
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None
    self._cap_result = None         # {"mean","range","lit","rgb"} of the last capture

    # the viewport is a live ECS host by design; terrain lowers its in-code scene, which
    # needs the ECS reflected classes registered before GPU finalization (ecsedit's
    # mechanism). Detected from the source content (pure python; no engine needed) — if ANY
    # source is terrain the ECS init callback is armed.
    fams = [_detect_family(s) for s in self._sources]
    pre_init = [ecs.ecsInitCallback] if "terrain" in fams else None
    # enable_global_events: Shift+L (reset dock layout) is an editor-app-level chord —
    # the global handler observes it regardless of which panel is hovered.
    self.createEzApp(name="dflowedit", offscreen=self._offscreen, pre_init_fns=pre_init,
                     enable_global_events=True)

  ##############################################################################
  # UI layout (terrainedit dock idioms; family-neutral)
  ##############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.13, 0.13, 0.15, 1)

    # DockSpace substrate (S6 adoption). A full-bleed (margin 0) transparent container;
    # per-panel insets come from the split margin — byte-parity with the prior
    # DockablePanel + lg.split idiom (viewport fill; left @0.4; propsheet @0.55).
    self.dock = lg.makeChild(fill=True, margin=0,
                             uiclass=lev2.ui.DockSpace, args=["dflow_dock"]).widget
    self.dock.clear = False

    self.viewport_dock = self.dock.addPanel(
        uiclass=lev2.ui.SceneGraphViewport,
        args=["Viewport", vec4(0.1, 0.1, 0.12, 1)], title=_DOCK_VIEWPORT_TITLE)
    self.viewport_dock.titlebar_color = vec4(0.15, 0.2, 0.25, 1)
    # placeholder viewport for doc-only families (a family may later populate it via the
    # binding's viewport_setup seam — this shell v1 does not bake/host any family).
    self.sgv = self.viewport_dock.child

    # W5: cross-window DockManager glue (shared with terrainedit via EditorDockGlue). The
    # viewport is PINNED (no factory) — a SceneGraphViewport holds a one-shot Context-bound GPU
    # seam (forkDB/scenegraph) that cannot be rebuilt in a foreign window's context. The Graph
    # node-editor column (toolbar + canvas TabsWidget hosting one NodeEditor per binding) AND the
    # property sheet ARE transferable: each registers a factory that rebuilds a FRESH instance in
    # the destination dock (re-initing the node-editors' glyph GPU seam against that window's
    # context on the GPU thread). The boot below CALLS each factory (one content-construction
    # path), then reproduces the canonical split geometry.
    self._dock_glue = EditorDockGlue(self, self.dock, "dflowedit")
    self._dock_glue.register(_DOCK_LEFT_TITLE, _DOCK_LEFT_TITLE, self._buildGraphPanel,
                             save_state=self._saveGraphState,
                             restore_state=self._restoreGraphState, closeable=False)
    self._dock_glue.register(_DOCK_PROPS_TITLE, _DOCK_PROPS_TITLE, self._buildPropsheetPanel,
                             save_state=self._savePropsheetState,
                             restore_state=self._restorePropsheetState, closeable=False)

    # Graph node-editor column: factory-build (toolbar + canvas TabsWidget), then the canonical
    # LEFT @0.4 split of the viewport. moveChild + setSplitProportion reproduce the prior inline
    # dock.split(...) tree (the propsheet's proven pattern). At BOOT the factory builds only the
    # SHELL (empty canvases) — the per-binding NodeEditors bind in _onGpuInit (bindings + glyph
    # textures need the fully-registered reflected classes + the GPU-init phase).
    self.left_dock = self._buildGraphPanel(self.dock, self.ezapp)
    self.dock.moveChild(panel=self.left_dock, to=self.viewport_dock, zone=tokens.LEFT)
    self.dock.setSplitProportion(self.viewport_dock, self.left_dock, _DOCK_LEFT_PROP)

    # build the property sheet via its factory, then reproduce the canonical split geometry
    # (propsheet BOTTOM @0.55 of the left column). moveChild's split uses the DockSpace
    # default gap (== _DOCK_SPLIT_MARGIN) and setSplitProportion matches dock_layout's
    # restore path, so the serialized default is byte-identical to the prior dock.split().
    self.propsheet_dock = self._buildPropsheetPanel(self.dock, self.ezapp)
    self.dock.moveChild(panel=self.propsheet_dock, to=self.left_dock, zone=tokens.BOTTOM)
    self.dock.setSplitProportion(self.left_dock, self.propsheet_dock, _DOCK_PROPS_PROP)

    # capture the canonical default arrangement, then restore a persisted session over it
    # (silent fall-back to default on any mismatch — a loud log, never a crash).
    self._default_layout = save_layout(self.dock)
    self._maybeRestoreSession()
    self.dock.updateLayout()

    # --uirecord: attach the session tap last, on the main thread, with the UI
    # tree live (tear-out secondaries self-attach via the recorder's rescan).
    if self._uirecord_path:
      import ork.uitest as U
      self._uirecorder = U.record(self.ezapp, self._uirecord_path)
      print(f"[dflowedit] uirecord -> {self._uirecord_path}", flush=True)

    # --uiplay: load + rebase the session now (fail-loud at boot on a bad file);
    # injection starts after _uiplay_settle gpu ticks (see _onGpuUpdate).
    if self._uiplay_path:
      import ork.uitest as U
      self._uiplay = U.play(self.ezapp, self._uiplay_path, rebase=True)
      print(f"[dflowedit] uiplay <- {self._uiplay_path} "
            f"({self._uiplay.session.event_count} events, "
            f"{self._uiplay.skipped_tagged} secondary-tagged skipped, "
            f"span {self._uiplay.max_frame + 1} frames)", flush=True)

  def stopUiRecord(self):
    """Flush + detach the --uirecord session tap (idempotent, exit-safe)."""
    rec = self._uirecorder
    self._uirecorder = None
    if rec is None:
      return
    try:
      sess = rec.stop()
      print(f"[dflowedit] uirecord wrote {sess.event_count} events -> "
            f"{self._uirecord_path}", flush=True)
    except Exception as e:
      # diagnostic tooling must never take the editor down at exit; the periodic
      # flush already banked everything up to the last gesture.
      print(f"[dflowedit] uirecord stop failed: {e}", flush=True)

  ##############################################################################
  # Graph node-editor column factory (the transferable panel) + carry-state
  ##############################################################################

  def _buildGraphPanel(self, dock, window):
    """The Graph node-editor column's SINGLE content-construction path (boot + every cross-window
    recreate). Builds a fresh VerticalPack[toolbar, canvas TabsWidget] DockPanel in 'dock' with one
    empty PrimCanvas per source, and re-stamps the app-singleton shell seams (left_dock /
    left_panel / toolbar / tabs / _canvases). When bindings already exist (a POST-BOOT recreate) it
    also POPULATEs one FRESH NodeEditor per binding over those canvases and queues their glyph
    gpuInit for the destination window's context; at BOOT the bindings do not exist yet, so
    _onGpuInit populates inline against the main context. The per-binding node_models are PRESERVED
    (document-bound state on the bindings) — only the NodeEditors are fresh (node positions live in
    the document, so a fresh editor loses no state)."""
    panel = dock.addPanel(uiclass=lev2.ui.VerticalPack, args=[_DOCK_LEFT_TITLE],
                          title=_DOCK_LEFT_TITLE, closeable=False)
    panel.titlebar_color = vec4(0.2, 0.15, 0.2, 1)
    self.left_dock = panel
    self.left_panel = panel.child
    self.left_panel.margin = 2
    self.left_panel.item_height = 34

    self._setupToolbar()

    # per-binding canvas tabs: one PrimCanvas per source in a TabsWidget. sort_tabs=False keeps
    # the tab index == source/binding index (stable programmatic + poll-driven focus); a lone
    # source runs in page mode (no tab bar) so single-source layout is unchanged.
    self._tab_labels = self._buildTabLabels()
    self.tabs = self.left_panel.makeChild(uiclass=lev2.ui.TabsWidget, args=["canvas_tabs"])
    self.tabs.sort_tabs = False
    self.tabs.draw_tabs = len(self._sources) > 1
    self.tabs.content_background = COL_BG
    self._canvases = []
    for label in self._tab_labels:
      canvas = self.tabs.makeChild(uiclass=lev2.ui.PrimCanvas, args=[label])
      canvas.bg_color = COL_BG
      canvas.draw_background = True
      self._canvases.append(canvas)
    self.tabs.setActiveTab(0)
    self.left_panel.fill_widget = self.tabs

    # POST-BOOT recreate: bindings are live -> bind fresh per-binding NodeEditors now + queue
    # their glyph gpuInit for 'window's own context. At BOOT (bindings empty) this is skipped;
    # _onGpuInit binds them inline against the main context.
    if self._bindings:
      self._populateGraphEditors(window)
    return panel

  def _populateGraphEditors(self, window, ctx=None):
    """Bind one FRESH NodeEditor per EXISTING binding over the shell's canvases and re-stamp the
    app-singleton seams (_node_editors, each binding's node_editor back-ref, key wiring, and — post
    GPU-init — the focused NodeEditor + propsheet). GPU seam: at BOOT (ctx given, _gpu_ready False)
    each editor's glyph textures init INLINE against the main context; a POST-BOOT recreate (ctx
    None) queues each fresh editor's gpuInit for the destination window's context (drained
    pre-render in _onGpuUpdate)."""
    self._node_editors = []
    for i, b in enumerate(self._bindings):
      canvas = self._canvases[i]
      ne = NodeEditor(canvas, b.node_model, title=b.title, orientation="vertical")
      b.node_model.node_editor = ne                 # back-ref (status strip + post-add select)
      ne.on_selection_changed = self._makeSelectHandler(b)
      if ctx is not None:
        ne.uicontext = self.uicontext
        ne.gpuInit(ctx)                             # BOOT: glyph/icon textures against main ctx
      else:
        self._ne_pending_gpuinit.append((ne, window))   # POST-BOOT: init against dest ctx
      self._wireNodeEditorKeys(canvas, ne)
      self._node_editors.append(ne)
    # POST-BOOT: re-stamp the focused NodeEditor + propsheet onto the fresh editors (carry-state
    # then refines nav/selection/view + the focused tab). At BOOT _onGpuInit does the focus.
    if self._gpu_ready:
      self._focusBinding(self._focused_index, force=True)

  def _saveGraphState(self, panel):
    # carry the focused tab + PER-TAB {nav path, selection, view pan/zoom} off the SOURCE editors
    # (the app-singleton _node_editors, still the source's until the factory re-stamps them). Node
    # ids are DOCUMENT tree-path keys, stable across a fresh model-less rebind.
    editors = self._node_editors
    if not editors:
      return None
    tabs = []
    for ne in editors:
      tabs.append({
          "nav_keys":  [getattr(m, "_container_key", None) for (m, _l) in ne.nav_stack[1:]],
          "sel_nodes": set(ne.sel_nodes),
          "sel_edges": set(tuple(e) for e in ne.sel_edges),
          "view":      (ne.view.s, ne.view.ox, ne.view.oy),
      })
    return {"focused_index": self._focused_index, "tabs": tabs}

  def _restoreGraphState(self, panel, state):
    # re-apply the carried per-tab nav path + selection + view onto the FRESH per-tab editors (the
    # factory already rebound _node_editors), then the carried focused tab. A container that no
    # longer exists stops THAT tab's descent (per-tab, terrainedit's pattern).
    if not state:
      return
    editors = self._node_editors
    tabs = state.get("tabs") or []
    for i, ne in enumerate(editors):
      if ne is None or i >= len(tabs):
        continue
      ts = tabs[i]
      for key in ts.get("nav_keys", []):
        if key is not None and key in set(ne.model.nodes()) and ne.model.is_group(key):
          ne._enter_group(key)
        else:
          break
      ne.sel_nodes = set(ts.get("sel_nodes") or set())
      ne.sel_edges = set(tuple(e) for e in (ts.get("sel_edges") or set()))
      v = ts.get("view")
      if v is not None:
        ne.view.s, ne.view.ox, ne.view.oy = v
        ne._did_initial_frame = True   # keep the carried pan/zoom (skip the initial auto-frame)
      ne.mark_view_changed()
      ne.mark_structure_changed()
      ne.mark_selection_changed()
      ne.mark_overlay()
      ne._emit_selection()
    fi = state.get("focused_index", 0)
    if self.tabs is not None and 0 <= fi < len(editors):
      self.tabs.setActiveTab(fi)
      self._focusBinding(fi, force=True)

  ##############################################################################
  # property-sheet factory (the transferable panel) + carry-state
  ##############################################################################

  def _buildPropsheetPanel(self, dock, window):
    """The property-sheet panel's SINGLE construction path (boot + every cross-window
    recreate). Builds + styles a fresh PropertySheet DockPanel in 'dock'. Post-GPU-init
    (a transfer/return rebuild) the change/custom-editor handlers are re-registered so the
    fresh sheet is immediately functional; the displayed model rides the carry-state."""
    panel = dock.addPanel(uiclass=lev2.ui.PropertySheet, args=[_DOCK_PROPS_TITLE],
                          title=_DOCK_PROPS_TITLE, closeable=False)
    panel.titlebar_color = vec4(0.2, 0.2, 0.15, 1)
    ps = panel.child
    ps.bgcolor = vec4(0.12, 0.12, 0.12, 1)
    ps.label_width = 150
    ps.row_height = 26
    self.propsheet = ps
    self.propsheet_dock = panel
    if self._gpu_ready:
      self._wirePropsheet()
    return panel

  @staticmethod
  def _savePropsheetState(panel):
    # carry the currently-bound model so a transferred/returned sheet shows the SAME content
    # in its new window (the model object is process-global, not context-bound).
    return {"model": getattr(panel.child, "model", None)}

  @staticmethod
  def _restorePropsheetState(panel, state):
    model = state.get("model") if state else None
    if model is not None:
      panel.child.model = model
      panel.child.rebuild()

  def _wirePropsheet(self):
    """(Re)register the property-sheet change + custom-editor handlers. Called at boot from
    _onGpuInit, and again by the factory when a transfer/return rebuilds the sheet."""
    self.propsheet.onPropertyChanged(self._onPropsheetChanged)
    self.propsheet.onRequestCustomEditor(self._onRequestCustomEditor)

  def _buildTabLabels(self):
    """Per-source tab labels from the source stem (== the binding title in practice),
    de-duplicated so two same-named sources stay distinguishable in the tab bar."""
    labels = []
    seen = {}
    for src in self._sources:
      base = _stem(src)
      n = seen.get(base, 0) + 1
      seen[base] = n
      labels.append(base if n == 1 else f"{base}#{n}")
    return labels

  def _setupToolbar(self):
    self.toolbar = self.left_panel.makeChild(uiclass=lev2.ui.HorizontalPack, args=["toolbar"])
    self.toolbar.margin = 2
    self.toolbar.item_width = 52
    self.toolbar.bg_color = vec4(0.12, 0.12, 0.15, 1)
    # the toolbar buttons are a WINDOWED convenience (Save / Fit have keyboard/programmatic
    # equivalents); ImageButton construction crashes some remote/headless graphics backends,
    # so skip them offscreen (the gate boots the canvas + propsheet without them) or when a
    # CI/remote run opts out via ORKID_DFLOWEDIT_NO_TOOLBAR.
    if self._offscreen or os.environ.get("ORKID_DFLOWEDIT_NO_TOOLBAR"):
      return

    def make_icon(text):
      return icon_library.from_svg_string(
          '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">'
          '<text x="12" y="12" text-anchor="middle" dominant-baseline="central" '
          'font-family="sans-serif" font-size="6" font-weight="bold" fill="#E6E6E6">'
          f'{text}</text></svg>', 32, 32)

    # Save / Fit + the family-neutral ECS transport (Start / Pause / Stop — present for
    # every family; a doc-only family binds a NullTransport, so they are simply inert).
    specs = [
      ("Save",  vec4(0.15, 0.15, 0.25, 1), lambda b: self._doSave()),
      ("SaveAs", vec4(0.15, 0.18, 0.28, 1), lambda b: self._doSaveAs()),
      ("Fit",   vec4(0.18, 0.18, 0.22, 1), lambda b: self._doFit()),
      ("Start", vec4(0.12, 0.22, 0.12, 1), lambda b: self._transport().start()),
      ("Pause", vec4(0.22, 0.20, 0.12, 1), lambda b: self._transport().pause()),
      ("Stop",  vec4(0.22, 0.12, 0.12, 1), lambda b: self._transport().stop()),
      ("Bench", vec4(0.18, 0.14, 0.22, 1), lambda b: self._showBench()),
    ]
    for name, color, handler in specs:
      btn = self.toolbar.makeChild(uiclass=lev2.ui.ImageButton, args=[f"btn_{name.lower()}"])
      btn.inactive_image = make_icon(name)
      btn.bgcolor = color
      btn.inactive_blend_mode = tokens.ALPHA
      btn.onPressed = handler

  def _transport(self):
    """The shell's ONE transport — the shared simulation (primary host), so Start/Pause/Stop
    act on the composed world regardless of the focused tab. Falls back to the focused
    binding's transport (a NullTransport for a doc-only shell)."""
    if self._viewport_host is not None:
      return self._viewport_host
    fb = self._focused_binding
    return fb.transport if fb is not None else NullTransport()

  ##############################################################################
  # dock layout persistence + live reset (Shift+L / --reset-layout)
  ##############################################################################

  def _sessionLayoutPath(self):
    """Per-user, per-app dock-layout slot keyed by (app name, entry-script path)."""
    return dock_layout_path("dflowedit")

  def _maybeRestoreSession(self):
    """Restore a persisted arrangement over the default construction. --reset-layout
    forces the default; a missing / mismatched session silently keeps the default."""
    if self._reset_layout:
      print("[dflowedit] --reset-layout: default dock arrangement", flush=True)
      return
    if not self._persist_layout:
      return
    path = self._sessionLayoutPath()
    if not os.path.exists(path):
      return
    try:
      with open(path) as f:
        raw = f.read()
      load_layout(self.dock, raw)
      if self._dock_glue is not None:
        self._dock_glue.queue_windows_from_state(raw)   # W6: recreate secondaries on the first GPU pump
      print(f"[dflowedit] restored dock layout <- {path}", flush=True)
    except Exception as e:
      # load_layout validates panel-ids up front and raises BEFORE mutating on a
      # mismatch, so the default construction is intact; re-assert it defensively.
      print(f"[dflowedit] dock layout restore skipped ({e}); using default", flush=True)
      try:
        load_layout(self.dock, self._default_layout)
      except Exception as e2:
        print(f"[dflowedit] default-layout reassert failed: {e2}", flush=True)

  def _saveSession(self):
    if self.dock is None:
      return
    try:
      path = self._sessionLayoutPath()
      doc = save_layout(self.dock)
      if self._dock_glue is not None:
        self._dock_glue.add_windows_to_state(doc)   # W6: persist any open secondary dock windows
      with open(path, "w") as f:
        f.write(to_json(doc))
      print(f"[dflowedit] saved dock layout -> {path}", flush=True)
    except Exception as e:
      print(f"[dflowedit] dock layout save failed: {e}", flush=True)

  def _resetDockLayout(self):
    """Restore the DEFAULT dock arrangement live. Runs OUTSIDE event dispatch (a GPU
    frame tick) so load_layout's moveChild + proportion ops all apply immediately +
    in order; a root updateLayout re-cascade follows so the reset renders this frame."""
    if self.dock is None or self._default_layout is None:
      return
    try:
      load_layout(self.dock, self._default_layout)
      self.dock.updateLayout()
      print("[dflowedit] dock layout reset to default", flush=True)
    except Exception as e:
      print(f"[dflowedit] dock layout reset failed: {e}", flush=True)

  def _onGlobalUiEvent(self, uievent):
    """Editor-app-level chords (observer-only; per-panel dispatch still runs). Shift+L
    resets the dock layout — SUPPRESSED while a text-input widget owns key focus (so
    Shift+L types 'L' in a focused expr/LineEdit instead of mutating the layout)."""
    if uievent.code != tokens.KEY_DOWN.hashed:
      return
    if uievent.keycode == ord("L") and uievent.shift and not (uievent.super or uievent.ctrl):
      if text_input_has_focus(getattr(self, "uicontext", None)):
        return
      # W5: route through the glue so the reset returns any torn-out secondaries first.
      if self._dock_glue is not None:
        self._dock_glue.request_reset()
      else:
        self._reset_layout_pending = True

  def onAppExit(self):
    # clean exit: persist the CURRENT arrangement (a Shift+L reset is a real edit —
    # exit after reset saves the DEFAULT). Skipped for the read-only probe and the
    # ephemeral automated gate modes (determinism: no cross-process session coupling).
    if self._persist_layout and not self._layout_probe:
      self._saveSession()
    super().onAppExit()

  def _layoutProbeTick(self):
    # read-only settle probe: report the as-constructed dock layout (proves session
    # restore / --reset-layout without mutating or persisting anything).
    self._layout_probe_frame += 1
    if self._layout_probe_frame == 60:
      names = sorted(p.name for p in self.dock.allPanels())
      print(f"PROBE_SIG={self.dock.layoutSignature()}", flush=True)
      print(f"PROBE_NAMES={names}", flush=True)
      print(f"PROBE_JSON={to_json(save_layout(self.dock))}", flush=True)
      print(f"PROBE_VALID={self.dock.validateTree()}", flush=True)
      self.ezapp.signalExit()

  def _layouttestTick(self):
    """Real-editor structural round-trip + live Shift+L reset + injected titlebar drag
    (GPU/main thread, render-sequential). Structural oracles (signature / JSON) — the
    live viewport content is not deterministic; the selftest already proves it renders."""
    import ork.uitest as U
    self._layouttest_frame += 1
    f = self._layouttest_frame
    st = self._layouttest_stage
    r = self._layouttest_results
    dock = self.dock

    if st == "settle" and f >= 40:
      r["sig0"]    = dock.layoutSignature()
      j1, j2       = to_json(save_layout(dock)), to_json(save_layout(dock))
      r["json0"]   = j1
      r["deterministic"] = (j1 == j2)
      r["names0"]  = sorted(p.name for p in dock.allPanels())
      r["n0"]      = dock.num_panels
      r["valid0"]  = dock.validateTree()
      print(f"[dflowedit layouttest] default sig={r['sig0']} names={r['names0']} "
            f"n={r['n0']} deterministic={r['deterministic']}", flush=True)
      self._layouttest_stage = "scramble"
    elif st == "scramble":
      dock.moveChild(panel=self.propsheet_dock, to=self.viewport_dock, zone=tokens.RIGHT)
      r["sig_scrambled"] = dock.layoutSignature()
      r["scrambled_json"] = to_json(save_layout(dock))
      r["scramble_changed"] = (r["sig_scrambled"] != r["sig0"])
      print(f"DEFAULT_JSON={r['json0']}", flush=True)
      print(f"SCRAMBLED_JSON={r['scrambled_json']}", flush=True)
      print(f"SCRAMBLED_SIG={r['sig_scrambled']}", flush=True)
      self._layouttest_stage = "load"
    elif st == "load":
      load_layout(dock, r["json0"]); dock.updateLayout()
      r["roundtrip_sig_ok"]  = (dock.layoutSignature() == r["sig0"])
      r["roundtrip_json_ok"] = (to_json(save_layout(dock)) == r["json0"])
      r["valid_loaded"] = dock.validateTree()
      print(f"[dflowedit layouttest] load round-trip: sig_ok={r['roundtrip_sig_ok']} "
            f"json_ok={r['roundtrip_json_ok']}", flush=True)
      self._layouttest_stage = "shiftl_scramble"
    elif st == "shiftl_scramble":
      dock.moveChild(panel=self.propsheet_dock, to=self.viewport_dock, zone=tokens.BOTTOM)
      r["sig_preshiftl"] = dock.layoutSignature()
      U.key_chord(self.ezapp, ord("L"), mods={"shift": True})
      self._layouttest_settle_at = f + 12
      self._layouttest_stage = "shiftl_wait"
    elif st == "shiftl_wait":
      if f >= self._layouttest_settle_at:
        r["shiftl_reset_ok"] = (dock.layoutSignature() == r["sig0"]
                                and r["sig_preshiftl"] != r["sig0"])
        print(f"[dflowedit layouttest] Shift+L reset: ok={r['shiftl_reset_ok']}", flush=True)
        self._layouttest_stage = "tabdrag"
    elif st == "tabdrag":
      lp = self.left_dock
      x0, y0 = lp.localToRoot(24, lp.titlebar_height // 2)
      vp = self.viewport_dock
      x1, y1 = vp.localToRoot(vp.width - 12, vp.height // 2)
      r["sig_pre_drag"] = dock.layoutSignature()
      top = self.ezapp.topWidget
      # The left column ("Graph") is now a TRANSFERABLE panel, so the coordinator would classify
      # an in-window titlebar drop as a TEAR-OUT when the main window has no known screen rect
      # (offscreen) — override main's rect so this in-window drop resolves LOCAL (a moveChild),
      # which is what this phase means to exercise.
      lev2.ui.DockCoordinator.instance().setWindowRectOverride("main", 0, 0, top.width, top.height)
      U.drag(self.ezapp, x0, y0, x1, y1, top.width, top.height, steps=10)
      self._layouttest_settle_at = f + 6
      self._layouttest_stage = "tabdrag_wait"
    elif st == "tabdrag_wait":
      if f >= self._layouttest_settle_at:
        r["drag_moved"] = (dock.layoutSignature() != r["sig_pre_drag"])
        r["valid_post_drag"] = dock.validateTree()
        print(f"[dflowedit layouttest] injected titlebar drag: moved={r['drag_moved']} "
              f"valid={r['valid_post_drag']}", flush=True)
        self._layouttestFinish()
        self._layouttest_stage = "done"
        self.ezapp.signalExit()
    if f > 2000 and self._layouttest_stage != "done":
      print("[dflowedit layouttest] TIMEOUT; FAIL", flush=True)
      self.ezapp.signalExit()

  def _layouttestFinish(self):
    r = self._layouttest_results
    r["names_ok"] = (r.get("names0") == sorted([_DOCK_VIEWPORT_TITLE, _DOCK_LEFT_TITLE,
                                                _DOCK_PROPS_TITLE]))
    r["ok"] = bool(r.get("deterministic") and r.get("valid0") and r.get("names_ok")
                   and r.get("n0") == 3 and r.get("scramble_changed")
                   and r.get("roundtrip_sig_ok") and r.get("roundtrip_json_ok")
                   and r.get("valid_loaded") and r.get("shiftl_reset_ok")
                   and r.get("drag_moved") and r.get("valid_post_drag"))
    print(f"[dflowedit layouttest] deterministic={r.get('deterministic')} "
          f"names_ok={r.get('names_ok')} n0={r.get('n0')} "
          f"scramble_changed={r.get('scramble_changed')} "
          f"roundtrip=({r.get('roundtrip_sig_ok')},{r.get('roundtrip_json_ok')}) "
          f"shiftl_reset_ok={r.get('shiftl_reset_ok')} drag_moved={r.get('drag_moved')} "
          f"-> {'PASS' if r['ok'] else 'FAIL'}", flush=True)

  ##############################################################################
  # GPU init — load the family + bind the canvas / property sheet
  ##############################################################################

  def _onGpuInit(self, ctx):
    self.uicontext = self.ezapp.uicontext
    self._ctx = ctx                                 # stashed for lazily-built detail editors
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    self.uicontext.theme_engine = lev2.ui.ThemeEngine(self.custom_db)

    # load EVERY source binding NOW (post subsystem init): deserialization + plugSpec walk
    # the reflected class descriptions, which only exist after full class registration.
    self._bindings = []
    for src in self._sources:
      try:
        b = load_family(src, skybox=self._envmap, material_mode=self._material,
                        apply_bench=self._benches)
      except Exception as ex:                     # ops-self-defend: a bad load fails loud
        print(f"[dflowedit] FAILED to load {src!r}: {ex}", flush=True)
        import traceback
        traceback.print_exc()
        self.ezapp.signalExit()
        return
      self._bindings.append(b)
      print(f"[dflowedit] loaded family={b.family!r} title={b.title!r} "
            f"nodes={len(list(b.node_model.nodes()))}", flush=True)

    # compose the ONE viewport: the FIRST payload-bearing binding is the PRIMARY (creates
    # scene+sim+camera and binds the widget); each SUBSEQUENT payload-bearing binding is a
    # CONTRIBUTOR folded into the SAME scene. Doc-only bindings contribute nothing (the
    # placeholder viewport stays when NO binding has a payload).
    payload_bindings = [b for b in self._bindings if b.viewport_host is not None]
    self._viewport_host = None
    if payload_bindings:
      primary_b = payload_bindings[0]
      self._viewport_host = primary_b.viewport_host
      if primary_b.viewport_setup is not None:
        try:
          primary_b.viewport_setup(self, ctx)           # create scene+sim+camera, bind
        except Exception as ex:
          print(f"[dflowedit] viewport_setup failed (non-fatal): {ex}", flush=True)
          import traceback
          traceback.print_exc()
          self._viewport_host = None
      if self._viewport_host is not None:
        for cb in payload_bindings[1:]:
          if cb.viewport_compose is None:
            print(f"[dflowedit] {cb.title!r} ({cb.family}) has a payload but no compose "
                  f"seam — skipping its viewport contribution", flush=True)
            continue
          try:
            cb.viewport_compose(self, self._viewport_host, ctx)  # fold into primary scene
            print(f"[dflowedit] composed {cb.title!r} into the shared viewport", flush=True)
          except Exception as ex:
            print(f"[dflowedit] compose failed for {cb.title!r} (non-fatal): {ex}", flush=True)
            import traceback
            traceback.print_exc()

    # one generic node editor per binding, each over its own canvas tab. This is the FIRST-BOOT
    # population (the Graph factory built only the SHELL in _onUiInit with _gpu_ready False, so it
    # did NOT populate) — bind the editors here and init their glyph textures against the MAIN
    # context. A POST-BOOT factory recreate (transfer / tear-out / return) populates from
    # _buildGraphPanel and queues the fresh editors' gpuInit for the destination context.
    self._populateGraphEditors(self.ezapp, ctx=ctx)

    # property sheet follows the focused binding (one change-handler; rebound per focus).
    self._gpu_ready = True             # from here, the factories defer/re-wire on rebuild
    self._wirePropsheet()             # (re)register change + custom-editor handlers
    self._focusBinding(0, force=True)

    # BYPASS-A/B: apply the requested bypass set NOW (before the sim's first compute) via the
    # focused binding's node model — the exact canvas path (set_bypassed + host rebake) — so
    # the effective-graph instance runs from tick 0. An empty set is the baseline (no rebake).
    if self._bypass_ab is not None:
      model = self._binding.node_model if self._binding is not None else None
      names = self._bypass_ab.get("bypass", []) or []
      for nid in names:
        r = model.set_bypassed(nid, True) if model is not None else ("refused", "no model")
        if r is not None:
          print(f"[dflowedit bypass-ab] set_bypassed({nid!r}) refused: {r}", flush=True)
      if names and self._viewport_host is not None:
        self._viewport_host._requestRebake()

  ##############################################################################
  # focus (tab) — the focused binding owns the propsheet + Save/flags/keys/Fit
  ##############################################################################

  def _focusBinding(self, index, *, force=False):
    """Make binding `index` the focused one: switch its canvas tab, rebind the propsheet to
    ITS property model (each binding keeps its own selection), and refresh. Idempotent."""
    if not self._bindings:
      return
    index = max(0, min(index, len(self._bindings) - 1))
    if index == self._focused_index and self._focused_binding is not None and not force:
      return
    self._focused_index = index
    b = self._focused_binding = self._binding = self._bindings[index]
    self.node_editor = self._node_editors[index] if self._node_editors else None
    if self.tabs is not None and self.tabs.getActiveTab() != index:
      self.tabs.setActiveTab(index)
    self.propsheet.model = b.prop_model            # follow the focused binding's selection
    self.propsheet.rebuild()

  def _makeSelectHandler(self, binding):
    """A per-binding canvas-selection handler (each canvas drives ITS binding's propsheet)."""
    return lambda model, nid: self._onNodeEditorSelect(binding, model, nid)

  ##############################################################################
  # selection + edit loop
  ##############################################################################

  def _onNodeEditorSelect(self, binding, model, nid):
    """Canvas selection -> the binding's property model. object_for_nid() hands back the
    family node handle the E2 document's property model binds (a module NAME for GraphData
    families, a DocNode/DocLoop/... for terrain); an empty selection (nid=None) or a
    non-object node (a boundary pill) binds an empty sheet. Only the FOCUSED (visible)
    canvas receives events, so a rebuild here reflects the active tab's selection."""
    obj = model.object_for_nid(nid) if nid is not None else None
    binding.prop_model.set_object(obj)
    if binding is self._focused_binding:
      # selecting a node returns the propsheet to the NODE section (it may have been showing the
      # distinct bench section via the Bench toolbar button).
      self.propsheet.model = binding.prop_model
      self.propsheet.rebuild()

  def _onPropsheetChanged(self, key, value):
    # the property model's setValue already wrote through the bound model. The BENCH section owns
    # its OWN rebake policy (motion params are LIVE -> no rebake; the enabled toggle rebakes via the
    # host), so the generic rebake below is SKIPPED for it — else a live motion-param edit would
    # bump the rebuild counter and the LIVE-params contract would be violated.
    from ork.editor.graphdoc_models import BenchPropertyModel
    if isinstance(getattr(self.propsheet, "model", None), BenchPropertyModel):
      return
    # the property model's setValue already wrote through the FOCUSED binding's document/
    # plug. Refresh the focused canvas so a structural-ish edit reflects (cheap), and — for a
    # composed ECS viewport — schedule a rebake on the shared sim (the primary re-elaborates
    # every contributor, so any binding's edit shows).
    if self.node_editor is not None:
      self.node_editor.mark_structure_changed()
    if self._viewport_host is not None:
      self._viewport_host._requestRebake()

  def _onRequestCustomEditor(self, key, editor_id):
    """An editor.custom propsheet row was clicked. For an expression field (E2.5 S7) open the
    expression editor in a resizable, FLOATING OS SECONDARY WINDOW placed clear of the viewport.
    APPLY validates+writes+rebakes and KEEPS THE WINDOW OPEN (the owner iterates while watching
    the sim react — multiple applies per session; a loud in-window error on an invalid/dishonest
    edit leaves the document untouched and the buffer preserved). CLOSE discards unapplied edits.
    Unknown editor ids are ignored. ONE WINDOW PER (node, field): re-opening a field that already
    has a live window raises it instead of duplicating; different fields (same or other nodes)
    open concurrent windows, each with its own apply/dirty state (ExprEditorRegistry; no leaks)."""
    if editor_id != "expr":
      return
    model = getattr(self.propsheet, "model", None)
    if model is None:
      return
    try:
      source = model.getValue(key)
    except Exception as ex:
      print(f"[dflowedit] expr editor: cannot read {key!r}: {ex}", flush=True)
      return

    node_key = self._exprNodeKey(model)
    field_label, context_name = self._exprRowLabels(model, key)

    def _apply(edited, _key=key):
      model.setValue(_key, edited)                    # parse+validate+write (raises loud on invalid)
      self._onPropsheetChanged(_key, edited)          # refresh focused canvas + host rebake

    from ork.ui.expr_detail_editor import ExprEditorWindow

    def _build(rect, on_closed, _fl=field_label, _cn=context_name, _src=source, _ap=_apply):
      return ExprEditorWindow(
          self.ezapp, self._ctx, _fl, _cn, _src or "", rect,
          on_apply=_ap, on_closed=on_closed)

    # ONE WINDOW PER (node, field): open() raises an existing live window (no duplicate) or builds
    # a new one at the cascaded, viewport-clear placement.
    self._expr_registry.open(node_key, key, _build)

  def _exprNodeKey(self, model):
    """A stable, hashable identity for the propsheet's current node — the registry key's node
    part, so each node's expression fields track independently. Falls back to id() for an
    unhashable node handle."""
    obj = getattr(model, "_obj", None)
    try:
      hash(obj)
      return obj
    except TypeError:
      return id(obj)

  def _exprRowLabels(self, model, key):
    """(field_label, context_name) for an expr row — the field name + its expr.context
    annotation (the vocabulary the editor edits against). Degrades to the row key / 'expr'."""
    field_label = key
    context_name = "expr"
    try:
      ann = model.getAnnotations(key)
    except Exception:
      ann = None
    if ann is not None:
      try:
        ctx = ann.__getattr__("expr.context")
        if ctx:
          context_name = str(ctx)
      except Exception:
        pass
    return field_label, context_name

  def _computeExprWindowRect(self, cascade_index=0):
    """The opening screen rect for the expression window: derived from the LIVE main-window +
    viewport geometry so the window never covers the viewport (expr_window_placement).
    cascade_index (the count of already-open expr windows) staggers concurrent windows so they
    don't stack exactly. Falls back to a safe (staggered) corner rect if the geometry is not yet
    resolvable (pre-viewport)."""
    from ork.ui.expr_window_placement import compute_expr_window_rect, MIN_W, MIN_H, CASCADE_STEP
    try:
      ctx = self._ctx
      mw, mh = ctx.mainSurfaceWidth(), ctx.mainSurfaceHeight()
      mox, moy = ctx.mapCoordToGlobal(0, 0)                  # main client origin (screen)
      main_rect = (mox, moy, mw, mh)
      vrx, vry = self.sgv.localToRoot(0, 0)                  # viewport origin (window-local)
      vgx, vgy = ctx.mapCoordToGlobal(int(vrx), int(vry))    # -> screen
      viewport_rect = (vgx, vgy, self.sgv.width, self.sgv.height)
      screen_rect = self._screenRectFor(mox, moy, mw, mh)
      rect, _strategy = compute_expr_window_rect(main_rect, viewport_rect, screen_rect,
                                                 cascade_index=cascade_index)
      return rect
    except Exception as ex:
      print(f"[dflowedit] expr window placement fell back ({ex})", flush=True)
      off = cascade_index * CASCADE_STEP
      return (80 + off, 80 + off, MIN_W, MIN_H)

  def onAppExit(self):
    # editor teardown: tear down every open expression window (no leaks) before the base exit.
    reg = getattr(self, "_expr_registry", None)
    if reg is not None:
      reg.closeAll()
    super().onAppExit()

  @staticmethod
  def _screenRectFor(x, y, w, h):
    """The monitor rect containing (x,y) — else the primary — else the union spanning the main
    window. Used as the placement's screen bound."""
    try:
      mons = lev2.enumerateGlfwMonitors()
    except Exception:
      mons = []
    primary = None
    for m in mons:
      if x >= m.x and x < m.x + m.width and y >= m.y and y < m.y + m.height:
        return (m.x, m.y, m.width, m.height)
      if getattr(m, "primary", False):
        primary = m
    if primary is not None:
      return (primary.x, primary.y, primary.width, primary.height)
    if mons:
      return (mons[0].x, mons[0].y, mons[0].width, mons[0].height)
    return (0, 0, max(x + w, 1920), max(y + h, 1080))

  ##############################################################################
  # toolbar actions
  ##############################################################################

  def _doSave(self, *, force_dialog=False):
    """Save the ACTIVE tab's doc. Opens the house file requester to STATE where to save
    (never an implicit cwd write — the bug this closes): a DSL-backed doc defaults to the
    ASSET'S dir + '<name>.orj' (Save-As; the .py CODE is never written in-place); an
    .orj-backed doc defaults to its OWN path (overwrite is the default selection). A prior
    successful save in THIS session re-writes that same path with NO dialog; force_dialog
    (Save-As) always dialogs. terrain (save is None) stays unavailable here — terrainedit
    owns the .py pywriter/resugar."""
    b = self._binding
    if b is None:
      return
    if b.save is None:
      msg = f"Save unavailable for family {b.family!r} in dflow.edit v1"
      print(f"[dflowedit] {msg}", flush=True)
      if self.node_editor is not None:
        self.node_editor.show_status(msg)
      return
    source = (self._sources[self._focused_index]
              if self._focused_index < len(self._sources) else b.title)
    remembered = None if force_dialog else self._save_paths.get(self._focused_index)
    plan = save_target.plan_save(b, source, remembered)
    if plan[0] == "resave":
      self._performSave(plan[1])
      return
    _kind, default_dir, default_name = plan
    self._openSaveDialog(f"Save {b.title} (.orj)", default_dir, default_name,
                         on_accept=self._performSave)

  def _doSaveAs(self):
    """Save-As: always dialog, regardless of a remembered path (state the target afresh)."""
    self._doSave(force_dialog=True)

  def _performSave(self, path):
    """Write the ACTIVE doc to `path` (chosen via the requester, or the remembered path),
    record it as this doc's last-save path (subsequent Save re-uses it, no dialog), and
    report on the status line. An unwritable path FAILS LOUDLY (status + stderr), leaving the
    memory untouched — no silent fallback, no crash."""
    b = self._binding
    if b is None or b.save is None:
      return
    out = save_target.normalize_orj(os.path.abspath(str(path)))
    try:
      b.save(out)
    except Exception as ex:
      msg = f"save failed: {ex}"
      print(f"[dflowedit] {msg}", flush=True)
      if self.node_editor is not None:
        self.node_editor.show_status(msg)
      return
    self._save_paths[self._focused_index] = out
    msg = f"saved -> {out}"
    print(f"[dflowedit] {msg}", flush=True)
    if self.node_editor is not None:
      self.node_editor.show_status(msg)

  def _openSaveDialog(self, title, default_dir, default_name, on_accept):
    """The house file requester (secondary window + FilesystemBrowser in save mode — the
    ecsedit/terrainedit idiom), defaulted to (default_dir, default_name), extension filtered
    '.orj'. The window FLOATS (always-on-top tool palette, the ExprEditorWindow pattern).
    onActivate writes via on_accept + closes; Cancel closes WITHOUT writing."""
    from ork.ui.filesystem_browser import FilesystemBrowser
    start_dir = default_dir if (default_dir and os.path.isdir(default_dir)) \
        else os.path.expanduser("~")
    popup = self.ezapp.createSecondaryWindow(
        width=800, height=600, x=200, y=150, title=title,
        decorated=True, resizable=True, floating=True)
    uic = popup.ui_context
    root = lev2.ui.LayoutGroup.create("save_popup_lg")
    root.setRect(0, 0, popup.width, popup.height)
    uic.top = root
    root.margin = 4
    browser_item = root.makeChild(
        uiclass=FilesystemBrowser,
        args=["save_browser", start_dir, ".orj", vec3(0.1, 0.1, 0.1), "save"], fill=True)
    browser = browser_item.widget.uservars.filesystem_browser
    # preselect the family-correct default filename (Save-As '<name>.orj' / the .orj's own name).
    if browser.filename_edit is not None:
      browser.filename_edit.text = default_name
    browser.onActivate = lambda p: (on_accept(p), popup.requestClose())
    browser.onCancel = lambda: popup.requestClose()

  def _doFit(self):
    if self.node_editor is not None:
      self.node_editor.frame(sel_only=False)

  def _showBench(self):
    """Bind the focused binding's DISTINCT bench section into the propsheet (a windowed
    convenience — the gates drive the bench model directly). Selecting a node returns the
    propsheet to the node section."""
    b = self._focused_binding
    if b is None or b.bench_prop_model is None:
      if self.node_editor is not None:
        self.node_editor.show_status("no TESTBENCH on the focused asset")
      return
    self.propsheet.model = b.bench_prop_model
    self.propsheet.rebuild()

  ##############################################################################
  # keyboard routing (terrainedit idiom, minus undo — hover-gated by the ui context)
  ##############################################################################

  def _wireNodeEditorKeys(self, canvas, ne):
    inner = ne._onUiEvent

    def _wrapped(ev):
      code = ev.code
      if code == tokens.KEY_DOWN.hashed:
        ne.handleKeyDown(ev)
        return lev2.ui.HandlerResult()
      if code == tokens.KEY_UP.hashed:
        ne.handleKeyUp(ev)
        return lev2.ui.HandlerResult()
      return inner(ev)

    canvas.onUiEvent = _wrapped

  ##############################################################################
  # frame hooks — drive the ECS viewport host (family-neutral: no-op when a family
  # binds no viewport host) + the scripted offscreen gates
  ##############################################################################

  def _onGpuUpdate(self, ctx):
    # GPU thread (== main/event thread): run the viewport host's rebake pipeline +
    # hold-last-frame swap, then follow user tab clicks. TabWidget has no C++ tab-change
    # callback, so poll the active tab here (same thread as onUiEvent / propsheet.rebuild)
    # and rebind the propsheet to the newly focused binding.
    if self._viewport_host is not None:
      self._viewport_host.gpuUpdate(ctx)
    # W5: build glyph textures for any node editor a POST-BOOT Graph-column factory recreate
    # produced (transfer / tear-out / return). Runs on the GPU thread but BEFORE this frame's
    # render pass (updateTexture asserts !_renderPassActive — draining in onGpuPostFrame aborts on
    # a MAIN-destination return), each editor against its DESTINATION window's own context.
    self._drainNodeEditorGpuInit(ctx)
    # --uiplay: frame-locked replay drive. Render thread == the serialized event-
    # dispatch thread, so injection here can never race live input. On completion:
    # un-virtualize the context clock (the Player virtualizes it for deterministic
    # double-click derivation) and, with uiplay_exit, signal exit a short settle
    # later so deferred structural mutations land before teardown.
    if self._uiplay is not None:
      self._uiplay_tick += 1
      # baseline the replay clock once: after the warm-up AND once the update
      # thread has published a counter (>0) — replay time then advances at the
      # RECORDED clock's rate; the Player's catch-up fires any frames a coarse
      # render-tick sample skipped over.
      if (self._uiplay_base is None and self._uiplay_tick >= self._uiplay_settle
          and self._uiplay_update_counter > 0):
        self._uiplay_base = self._uiplay_update_counter
      if self._uiplay_base is not None and not self._uiplay.done:
        t = self._uiplay_update_counter - self._uiplay_base
        import types
        self._uiplay.on_update(types.SimpleNamespace(counter=t))
        if self._uiplay.done:
          self._uiplay_done_tick = self._uiplay_tick
          uic = self.ezapp.uicontext
          if uic is not None:
            uic.virtual_time_enabled = False
          print(f"[dflowedit] uiplay done: injected "
                f"{self._uiplay.injected_count} events", flush=True)
      elif (self._uiplay_exit and self._uiplay_done_tick is not None
            and self._uiplay_tick >= self._uiplay_done_tick + 20):
        self._uiplay_done_tick = None
        self.ezapp.signalExit()
    # live dock-layout reset (Shift+L) — applied here (render-sequential, outside event
    # dispatch) so the moveChild + proportion re-cascade never races DoRePaintSurface.
    # W5: the two-phase reset FIRST returns every secondary window's panels to main
    # (return-on-close), THEN restores the main default once they have all returned.
    if self._dock_glue is not None:
      self._dock_glue.pump_reset(self._resetDockLayout)
    # scripted DOCK-LAYOUT gate — all dock mutations run here (render-sequential).
    if self._layouttest:
      self._layouttestTick()
    # apply a gate-requested tab switch HERE (main thread) — the offscreen selftest runs on
    # the update thread and must never mutate widgets itself.
    if self._pending_focus is not None and self.tabs is not None:
      self.tabs.setActiveTab(self._pending_focus)
      self._pending_focus = None
    # follow the applied request / user tab clicks: rebind the propsheet to the newly
    # focused binding on this (main) thread.
    if self.tabs is not None and self._bindings:
      cur = self.tabs.getActiveTab()
      if 0 <= cur < len(self._bindings) and cur != self._focused_index:
        self._focusBinding(cur)

  def _onUpdate(self, updinfo):
    # update thread: transport-gated ECS tick (host owns the play/pause/stop gate).
    if self._uirecorder is not None:
      self._uirecorder.on_update(updinfo)
    if self._uiplay is not None:
      self._uiplay_update_counter = int(updinfo.counter)
    if self._viewport_host is not None:
      self._viewport_host.update()
    if self.sgv is not None:
      self.sgv.setDirty()
    if self._selftest and not self._selftest_done:
      self._selftestTick()
    if self._flagtest and not self._selftest_done:
      self._flagtestTick()
    if self._bypass_ab and not self._selftest_done:
      self._bypassAbTick()
    if self._bench_ab and not self._selftest_done:
      self._benchAbTick()
    if self._layout_probe:
      self._layoutProbeTick()

  def _drainNodeEditorGpuInit(self, main_ctx):
    """Build the glyph/icon textures for any node editor a POST-BOOT Graph-column factory recreate
    produced. Called from _onGpuUpdate — on the GPU/render thread but BEFORE the frame's render
    pass, so the texture upload is legal (updateTexture asserts !_renderPassActive) even for a MAIN-
    destination rebuild (return-on-close). Each editor inits against the DESTINATION window's OWN
    context — a texture built on the wrong window's context is a cross-context seam. The device is
    shared across windows, but each node-editor instance owns its textures. A window whose context
    is not yet live (a just-torn-out window) re-queues for the next frame."""
    if not self._ne_pending_gpuinit:
      return
    pending = self._ne_pending_gpuinit
    self._ne_pending_gpuinit = []
    for (ne, window) in pending:
      try:
        wctx, uic = self._windowGpu(window, main_ctx)
        if wctx is None:
          self._ne_pending_gpuinit.append((ne, window))   # context not live yet; retry next frame
          continue
        ne.uicontext = uic
        ne.gpuInit(wctx)
      except Exception as e:
        print(f"[dflowedit] node-editor GPU init deferred/failed: {e}", flush=True)

  def _windowGpu(self, window, main_ctx):
    """(gfx_context, ui_context) for the window a factory built into — the main app (ezapp, the
    live render ctx) or an EzSecondaryWin (its own gfx/ui context)."""
    if window is self.ezapp:
      return main_ctx, self.uicontext
    return getattr(window, "gfx_context", None), getattr(window, "ui_context", None)

  ##############################################################################
  # offscreen framebuffer capture (async readback drained across frames)
  ##############################################################################

  def onGpuPostFrame(self, ctx):
    super().onGpuPostFrame(ctx)
    # W5: realize any pending cross-window tear-outs (main/GPU-thread work), per the
    # DockManager-prescribed pump point.
    if self._dock_glue is not None:
      self._dock_glue.pump()
    if not (self._selftest or self._flagtest or self._bypass_ab or self._bench_ab):
      return
    if self._cap_pending and not self._cap_inflight:
      self._capIssue(ctx)
    elif self._cap_inflight:
      ready = (self._cap_async is None) or bool(self._cap_async.is_ready)
      if ready:
        self._capFinish()

  def _cap_request(self):
    self._cap_result = None
    self._cap_pending = True

  def _capIssue(self, ctx):
    from orkengine.lev2 import CaptureBuffer
    try:
      rtg = ctx.FBI.main_RTG
      self._cap_buf = CaptureBuffer()
      self._cap_async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap_buf, "RGBA8")
      self._cap_inflight = True
    except Exception as e:
      print(f"[dflowedit gate] capture issue error: {e}", flush=True)
      self._cap_pending = False

  def _capFinish(self):
    import numpy
    mean = 0.0; rng = 0.0; lit = False; rgb = None
    try:
      capbuf = self._cap_buf
      w, h = capbuf.width, capbuf.height
      if w > 0 and h > 0:
        arr = numpy.array(capbuf, dtype=numpy.uint8).reshape(h, w, 4)
        rgb = arr[..., :3].copy()
        gray = rgb.astype(numpy.float32).mean(axis=2) / 255.0
        mean = float(gray.mean()); rng = float(gray.max() - gray.min())
        lit = rng > 0.02
    except Exception as e:
      print(f"[dflowedit gate] capture finish error: {e}", flush=True)
    self._cap_result = {"mean": mean, "range": rng, "lit": lit, "rgb": rgb}
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None
    self._cap_pending = False

  ##############################################################################
  # --selftest : canvas-model bound + viewport non-black + ECS transport observable
  ##############################################################################

  def _selftestTick(self):
    # multi-document: exercise per-binding load + tab-switch propsheet rebind + the shared
    # sim's viewport/transport. Single-source keeps the exact original state machine below.
    if len(self._bindings) > 1:
      self._selftestTickMulti()
      return
    ne = self.node_editor
    host = self._viewport_host
    # doc-only family (no viewport host): the original boot gate (model + rebuild counter).
    if host is None:
      self._selftest_frame += 1
      if self._selftest_frame < 30:
        return
      self._selftest_done = True
      ok = (self._binding is not None and ne is not None and ne.model is not None
            and hasattr(ne, "rebuild_count"))
      ncount = len(list(ne.model.nodes())) if (ne and ne.model) else -1
      ecount = -1
      if ne and ne.model and hasattr(ne.model, "edges"):
        try:
          ecount = len(list(ne.model.edges()))
        except Exception:
          ecount = -1
      print(f"[dflowedit selftest] family={self._binding.family if self._binding else None!r} "
            f"model_bound={ne is not None and ne.model is not None} nodes={ncount} "
            f"edges={ecount} "
            f"rebuild_count={getattr(ne, 'rebuild_count', None)} -> {'PASS' if ok else 'FAIL'}",
            flush=True)
      print(f"DFLOWEDIT_SELFTEST_RESULT={'PASS' if ok else 'FAIL'}", flush=True)
      self.ezapp.signalExit()
      return

    # an ANIMATING payload (particles) runs the stronger PIXEL-diff transport proof (frames
    # DIFFER while PLAYING, are IDENTICAL while PAUSED, DIFFER again on resume) — the proof a
    # static family (terrain / a static mesh) cannot give.
    if getattr(host, "animates", False):
      self._selftestTickAnimated(host)
      return

    # live ECS viewport: run the settle -> capture -> transport state machine.
    st = self._st
    if st is None:
      st = self._st = {"stage": "settle", "f": 0, "vp_mean": -1.0,
                       "tick_play": None, "tick_pause0": None, "tick_pause1": None,
                       "tick_resume": None, "tick_stop0": None, "tick_stop1": None,
                       "state_after_stop": None}
    st["f"] += 1
    f = st["f"]
    stage = st["stage"]

    if stage == "settle":
      # let the initial bake + a couple rebuild swaps settle, then capture the viewport.
      if f >= 90:
        self._cap_request()
        st["stage"] = "wait_vp"
    elif stage == "wait_vp":
      if not self._cap_pending and self._cap_result is not None:
        st["vp_mean"] = self._cap_result["mean"]
        st["vp_range"] = self._cap_result["range"]
        st["vp_lit"] = self._cap_result["lit"]
        # transport: playing by default — sample the tick counter, advance frames, resample.
        st["tick_play"] = host.tick_count
        st["at"] = f + 20
        st["stage"] = "played"
    elif stage == "played":
      if f >= st["at"]:
        st["tick_play_end"] = host.tick_count
        host.pause()
        st["tick_pause0"] = host.tick_count
        st["at"] = f + 20
        st["stage"] = "paused"
    elif stage == "paused":
      if f >= st["at"]:
        st["tick_pause1"] = host.tick_count
        host.start()
        st["at"] = f + 20
        st["stage"] = "resumed"
    elif stage == "resumed":
      if f >= st["at"]:
        st["tick_resume"] = host.tick_count
        host.stop()
        st["tick_stop0"] = host.tick_count
        st["state_after_stop"] = host.state
        st["at"] = f + 20
        st["stage"] = "stopped"
    elif stage == "stopped":
      if f >= st["at"]:
        st["tick_stop1"] = host.tick_count
        self._selftestFinish()
        self._selftest_done = True
        self.ezapp.signalExit()
    if f > 1600 and not self._selftest_done:
      print("[dflowedit selftest] TIMEOUT; FAIL", flush=True)
      self._selftest_done = True
      self.ezapp.signalExit()

  def _selftestFinish(self):
    st = self._st
    ne = self.node_editor
    model_bound = ne is not None and ne.model is not None
    nodes = len(list(ne.model.nodes())) if model_bound else -1
    vp_ok = st["vp_lit"] and st["vp_mean"] > 0.02
    play_ok = st["tick_play_end"] > st["tick_play"]                 # advanced while PLAYING
    pause_ok = st["tick_pause1"] == st["tick_pause0"]               # frozen while PAUSED
    resume_ok = st["tick_resume"] > st["tick_pause1"]              # advanced after start()
    stop_ok = (st["tick_stop1"] == st["tick_stop0"]                # frozen after stop()
               and st["state_after_stop"] == "stopped")
    ok = bool(model_bound and vp_ok and play_ok and pause_ok and resume_ok and stop_ok)
    print(f"[dflowedit selftest] family={self._binding.family!r} model_bound={model_bound} "
          f"nodes={nodes} rebuild_count={self._viewport_host.rebuild_count}", flush=True)
    print(f"[dflowedit selftest] viewport_mean={st['vp_mean']:.5f} "
          f"range={st['vp_range']:.5f} lit={st['vp_lit']} -> non_black={vp_ok}", flush=True)
    print(f"[dflowedit selftest] transport ticks play:{st['tick_play']}->{st['tick_play_end']} "
          f"(advanced={play_ok}) pause:{st['tick_pause0']}=={st['tick_pause1']} "
          f"(frozen={pause_ok}) resume->{st['tick_resume']} (advanced={resume_ok}) "
          f"stop:{st['tick_stop0']}=={st['tick_stop1']} state={st['state_after_stop']!r} "
          f"(stopped={stop_ok})", flush=True)
    print(f"[dflowedit selftest] -> {'PASS' if ok else 'FAIL'}", flush=True)
    print(f"DFLOWEDIT_SELFTEST_RESULT={'PASS' if ok else 'FAIL'}", flush=True)

  ##############################################################################
  # --selftest (ANIMATING PAYLOAD) : the PIXEL-diff transport proof. Two captures a window
  # apart while PLAYING must DIFFER (the sim is animating), two while PAUSED must be IDENTICAL
  # (frozen), and a post-resume pair must DIFFER again. This is the transport proof a static
  # family (terrain / a static mesh) cannot give — only a running particle system animates the
  # pixels. tick_count rides along as the secondary observable.
  ##############################################################################

  # per-pair mean-abs 8-bit pixel delta thresholds (whole-frame average; the fire occupies a
  # fraction of the 1280x720 frame so a PLAYING window reads ~0.1 MAD, a FROZEN window reads a
  # measured 0.0000 — external_compute gating stops the sim EXACTLY). CHANGED sits between them
  # with wide margin; FROZEN is just above the (zero) capture noise floor.
  ANIM_CHANGED_MAD = 0.02
  ANIM_FROZEN_MAD = 0.01

  def _selftestTickAnimated(self, host):
    import numpy
    st = self._st
    if st is None:
      st = self._st = {"stage": "settle", "f": 0, "prev": None,
                       "play_mad": None, "pause_mad": None, "resume_mad": None,
                       "vp_mean": -1.0, "vp_range": 0.0, "vp_lit": False,
                       "tick_play0": None, "tick_play1": None,
                       "tick_pause0": None, "tick_pause1": None,
                       "tick_resume0": None, "tick_resume1": None,
                       "state_after_stop": None}
    st["f"] += 1
    f = st["f"]
    stage = st["stage"]

    def mad(a, b):
      if a is None or b is None or a.shape != b.shape:
        return -1.0
      return float(numpy.abs(a.astype(numpy.float32) - b.astype(numpy.float32)).mean())

    # WINDOW = frames between the two captures of a pair (enough sim advance to move pixels).
    W = 24
    # pipeline settle after a transport transition before the first capture of the next pair
    # (the capture reads the last completed frame; the update/GPU threads are pipelined).
    S = 16

    if stage == "settle":
      if f >= 90:
        self._cap_request(); st["stage"] = "vp"
    elif stage == "vp":
      if not self._cap_pending and self._cap_result is not None:
        st["vp_mean"] = self._cap_result["mean"]
        st["vp_range"] = self._cap_result["range"]
        st["vp_lit"] = self._cap_result["lit"]
        st["prev"] = self._cap_result["rgb"]          # PLAYING capture #1 (t0)
        st["tick_play0"] = host.tick_count
        st["at"] = f + W; st["stage"] = "play_b"
    elif stage == "play_b":
      if f >= st["at"]:
        self._cap_request(); st["stage"] = "play_cap"
    elif stage == "play_cap":
      if not self._cap_pending and self._cap_result is not None:
        st["play_mad"] = mad(st["prev"], self._cap_result["rgb"])   # PLAYING t0 vs t0+W
        st["tick_play1"] = host.tick_count
        host.pause()
        st["at"] = f + S; st["stage"] = "pause_a"
    elif stage == "pause_a":
      if f >= st["at"]:
        self._cap_request(); st["stage"] = "pause_a_cap"
    elif stage == "pause_a_cap":
      if not self._cap_pending and self._cap_result is not None:
        st["prev"] = self._cap_result["rgb"]          # PAUSED capture #1
        st["tick_pause0"] = host.tick_count
        st["at"] = f + W; st["stage"] = "pause_b"
    elif stage == "pause_b":
      if f >= st["at"]:
        self._cap_request(); st["stage"] = "pause_cap"
    elif stage == "pause_cap":
      if not self._cap_pending and self._cap_result is not None:
        st["pause_mad"] = mad(st["prev"], self._cap_result["rgb"])  # PAUSED #1 vs #2 (must match)
        st["tick_pause1"] = host.tick_count
        host.start()
        st["at"] = f + S; st["stage"] = "resume_a"
    elif stage == "resume_a":
      if f >= st["at"]:
        self._cap_request(); st["stage"] = "resume_a_cap"
    elif stage == "resume_a_cap":
      if not self._cap_pending and self._cap_result is not None:
        st["prev"] = self._cap_result["rgb"]          # RESUMED capture #1
        st["tick_resume0"] = host.tick_count
        st["at"] = f + W; st["stage"] = "resume_b"
    elif stage == "resume_b":
      if f >= st["at"]:
        self._cap_request(); st["stage"] = "resume_cap"
    elif stage == "resume_cap":
      if not self._cap_pending and self._cap_result is not None:
        st["resume_mad"] = mad(st["prev"], self._cap_result["rgb"])  # RESUMED t0 vs t0+W
        st["tick_resume1"] = host.tick_count
        st["state_after_stop"] = host.stop()
        self._selftestFinishAnimated(host)
        self._selftest_done = True
        self.ezapp.signalExit()
    if f > 1600 and not self._selftest_done:
      print("[dflowedit selftest] TIMEOUT; FAIL", flush=True)
      print("DFLOWEDIT_SELFTEST_RESULT=FAIL", flush=True)
      self._selftest_done = True
      self.ezapp.signalExit()

  def _selftestFinishAnimated(self, host):
    st = self._st
    ne = self.node_editor
    model_bound = ne is not None and ne.model is not None
    nodes = len(list(ne.model.nodes())) if model_bound else -1
    vp_ok = bool(st["vp_lit"] and st["vp_mean"] > 0.02)
    # PIXEL transport proof (the headline):
    play_changed = st["play_mad"] > self.ANIM_CHANGED_MAD
    pause_frozen = 0.0 <= st["pause_mad"] < self.ANIM_FROZEN_MAD
    resume_changed = st["resume_mad"] > self.ANIM_CHANGED_MAD
    # tick_count secondary proof:
    play_adv = st["tick_play1"] > st["tick_play0"]
    pause_froze = st["tick_pause1"] == st["tick_pause0"]
    resume_adv = st["tick_resume1"] > st["tick_resume0"]
    stopped_ok = st["state_after_stop"] == "stopped"
    ok = bool(model_bound and vp_ok and play_changed and pause_frozen and resume_changed
              and play_adv and pause_froze and resume_adv and stopped_ok)
    print(f"[dflowedit selftest] family={self._binding.family!r} model_bound={model_bound} "
          f"nodes={nodes} rebuild_count={host.rebuild_count}", flush=True)
    print(f"[dflowedit selftest] viewport_mean={st['vp_mean']:.5f} "
          f"range={st['vp_range']:.5f} lit={st['vp_lit']} -> non_black={vp_ok}", flush=True)
    print(f"[dflowedit selftest] PIXEL transport: playing_MAD={st['play_mad']:.4f} "
          f"(>{self.ANIM_CHANGED_MAD} changed={play_changed}) paused_MAD={st['pause_mad']:.4f} "
          f"(<{self.ANIM_FROZEN_MAD} frozen={pause_frozen}) resumed_MAD={st['resume_mad']:.4f} "
          f"(>{self.ANIM_CHANGED_MAD} changed={resume_changed})", flush=True)
    print(f"[dflowedit selftest] tick transport: play {st['tick_play0']}->{st['tick_play1']} "
          f"(adv={play_adv}) pause {st['tick_pause0']}=={st['tick_pause1']} (froze={pause_froze}) "
          f"resume {st['tick_resume0']}->{st['tick_resume1']} (adv={resume_adv}) "
          f"stop_state={st['state_after_stop']!r} (stopped={stopped_ok})", flush=True)
    print(f"[dflowedit selftest] -> {'PASS' if ok else 'FAIL'}", flush=True)
    print(f"DFLOWEDIT_SELFTEST_RESULT={'PASS' if ok else 'FAIL'}", flush=True)

  ##############################################################################
  # --selftest (MULTI-DOC) : per-binding load + tab-switch propsheet rebind + the shared
  # composed viewport/transport. Two payload bindings compose into ONE sim.
  ##############################################################################

  def _selftestTickMulti(self):
    host = self._viewport_host
    st = self._st
    if st is None:
      st = self._st = {"stage": "settle", "f": 0, "vp_mean": -1.0, "vp_range": 0.0,
                       "vp_lit": False, "tab_switch_ok": False}
    st["f"] += 1
    f = st["f"]
    stage = st["stage"]

    if stage == "settle":
      if f < 90:
        return
      # (1) per-binding load report — family / nodes / edges / payload, both bindings.
      for i, b in enumerate(self._bindings):
        nm = b.node_model
        nn = len(list(nm.nodes()))
        en = -1
        if hasattr(nm, "edges"):
          try:
            en = len(list(nm.edges()))
          except Exception:
            en = -1
        print(f"[dflowedit selftest:multi] binding[{i}] family={b.family!r} title={b.title!r} "
              f"nodes={nn} edges={en} payload={b.viewport_host is not None}", flush=True)
      # (2) REQUEST a programmatic tab switch — applied on the main thread (_onGpuUpdate),
      # which rebinds the focused-binding propsheet. Observed in the "await_focus" stage.
      st["focus_before"] = self._focused_index
      st["pm_before"] = id(self._bindings[self._focused_index].prop_model)
      st["target"] = (self._focused_index + 1) % len(self._bindings)
      self._pending_focus = st["target"]
      st["stage"] = "await_focus"
    elif stage == "await_focus":
      if self._focused_index != st["target"]:
        return                                  # wait for the main thread to apply + rebind
      target = st["target"]
      pm_after = id(self._bindings[target].prop_model)
      # the propsheet follows the focused binding (each binding owns an independent model):
      # focus moved to target AND the target binds a DIFFERENT property model than before.
      st["tab_switch_ok"] = (self._focused_binding is self._bindings[target]
                             and pm_after != st["pm_before"])
      print(f"[dflowedit selftest:multi] tab switch focus {st['focus_before']}->"
            f"{self._focused_index} propsheet_model {st['pm_before']}->{pm_after} "
            f"rebound={st['tab_switch_ok']}", flush=True)
      # (3) viewport / transport on the shared sim (skip if no payload binding at all).
      if host is None:
        self._selftestFinishMulti(has_host=False)
        self._selftest_done = True
        self.ezapp.signalExit()
        return
      self._cap_request()
      st["stage"] = "wait_vp"
    elif stage == "wait_vp":
      if not self._cap_pending and self._cap_result is not None:
        st["vp_mean"] = self._cap_result["mean"]
        st["vp_range"] = self._cap_result["range"]
        st["vp_lit"] = self._cap_result["lit"]
        st["rgb_on"] = self._cap_result["rgb"]        # composed frame WITH the contributor(s)
        # PER-PAYLOAD VISIBILITY ORACLE (the stronger compose gate): suppress each contributor
        # that exposes the seam, re-capture, and diff -> a NONZERO changed-pixel region is the
        # contributor's ACTUAL pixels in the composed viewport (catches attach-drop AND burial /
        # out-of-frame placement, which a whole-frame non-black check cannot). Contributors with
        # no suppression seam (a composed terrain) are skipped -> the oracle is N/A there.
        st["vis_hosts"] = [b.viewport_host for b in self._bindings[1:]
                           if b.viewport_host is not None
                           and hasattr(b.viewport_host, "setComposedVisible")]
        if st["vis_hosts"]:
          for vh in st["vis_hosts"]:
            vh.setComposedVisible(False)
          # LET THE SUPPRESSED FRAME RENDER before re-capturing: the capture reads the last
          # completed frame, and the update/GPU threads are pipelined, so a capture issued the
          # same tick as the disable would grab a still-visible frame (a false 0-pixel diff).
          st["at"] = f + 20
          st["stage"] = "vis_settle"
        else:
          st["vis_applicable"] = False
          st["tick_play"] = host.tick_count
          st["at"] = f + 20
          st["stage"] = "played"
    elif stage == "vis_settle":
      if f >= st["at"]:
        self._cap_request()
        st["stage"] = "vis_off"
    elif stage == "vis_off":
      if not self._cap_pending and self._cap_result is not None:
        import numpy
        rgb_off = self._cap_result["rgb"]
        for vh in st["vis_hosts"]:
          vh.setComposedVisible(True)                 # restore before the transport legs
        a, b = st.get("rgb_on"), rgb_off
        changed, mad = 0, 0.0
        if a is not None and b is not None and a.shape == b.shape:
          d = numpy.abs(a.astype(numpy.float32) - b.astype(numpy.float32))
          changed = int((d.mean(axis=2) > 8.0).sum())
          mad = float(d.mean())
        st["vis_applicable"] = True
        st["vis_changed"] = changed
        st["vis_mad"] = mad
        st["vis_payloads"] = len(st["vis_hosts"])
        print(f"[dflowedit selftest:multi] visibility oracle: suppressed {len(st['vis_hosts'])} "
              f"contributor payload(s) -> changed_pixels={changed} mean_abs_diff={mad:.4f}",
              flush=True)
        st["tick_play"] = host.tick_count
        st["at"] = f + 20
        st["stage"] = "played"
    elif stage == "played":
      if f >= st["at"]:
        st["tick_play_end"] = host.tick_count
        host.pause()
        st["tick_pause0"] = host.tick_count
        st["at"] = f + 20
        st["stage"] = "paused"
    elif stage == "paused":
      if f >= st["at"]:
        st["tick_pause1"] = host.tick_count
        host.start()
        st["at"] = f + 20
        st["stage"] = "resumed"
    elif stage == "resumed":
      if f >= st["at"]:
        st["tick_resume"] = host.tick_count
        host.stop()
        st["tick_stop0"] = host.tick_count
        st["state_after_stop"] = host.state
        st["at"] = f + 20
        st["stage"] = "stopped"
    elif stage == "stopped":
      if f >= st["at"]:
        st["tick_stop1"] = host.tick_count
        self._selftestFinishMulti(has_host=True)
        self._selftest_done = True
        self.ezapp.signalExit()
    if f > 1600 and not self._selftest_done:
      print("[dflowedit selftest:multi] TIMEOUT; FAIL", flush=True)
      print("DFLOWEDIT_SELFTEST_RESULT=FAIL", flush=True)
      self._selftest_done = True
      self.ezapp.signalExit()

  # a suppressed contributor whose pixels vanish must move at least this many pixels — well above
  # capture noise, well below the composed box's footprint (~thousands of px at the fit scale).
  VIS_MIN_CHANGED = 200

  def _selftestFinishMulti(self, has_host):
    st = self._st
    tab_ok = bool(st.get("tab_switch_ok"))
    all_bound = all((ne is not None and ne.model is not None) for ne in self._node_editors)
    payloads = sum(1 for b in self._bindings if b.viewport_host is not None)
    # per-payload visibility oracle verdict (N/A when no contributor exposes the suppression seam).
    vis_applicable = bool(st.get("vis_applicable", False))
    vis_changed = int(st.get("vis_changed", 0))
    vis_ok = (not vis_applicable) or (vis_changed > self.VIS_MIN_CHANGED)
    if vis_applicable:
      print(f"[dflowedit selftest:multi] visibility oracle -> contributor_pixels={vis_changed} "
            f"(> {self.VIS_MIN_CHANGED}) mean_abs_diff={st.get('vis_mad', 0.0):.4f} "
            f"payloads={st.get('vis_payloads', 0)} -> visible={vis_ok}", flush=True)
    else:
      print("[dflowedit selftest:multi] visibility oracle -> N/A "
            "(no contributor exposes a suppression seam)", flush=True)
    if has_host:
      vp_ok = st["vp_lit"] and st["vp_mean"] > 0.02
      play_ok = st["tick_play_end"] > st["tick_play"]
      pause_ok = st["tick_pause1"] == st["tick_pause0"]
      resume_ok = st["tick_resume"] > st["tick_pause1"]
      stop_ok = (st["tick_stop1"] == st["tick_stop0"] and st["state_after_stop"] == "stopped")
      transport_ok = play_ok and pause_ok and resume_ok and stop_ok
      print(f"[dflowedit selftest:multi] composed payloads={payloads} "
            f"rebuild_count={self._viewport_host.rebuild_count} viewport_mean={st['vp_mean']:.5f} "
            f"range={st['vp_range']:.5f} lit={st['vp_lit']} -> non_black={vp_ok}", flush=True)
      print(f"[dflowedit selftest:multi] transport play:{st['tick_play']}->{st['tick_play_end']} "
            f"(advanced={play_ok}) pause:{st['tick_pause0']}=={st['tick_pause1']} "
            f"(frozen={pause_ok}) resume->{st['tick_resume']} (advanced={resume_ok}) "
            f"stop:{st['tick_stop0']}=={st['tick_stop1']} state={st['state_after_stop']!r} "
            f"(stopped={stop_ok})", flush=True)
    else:
      vp_ok = True
      transport_ok = True
      print(f"[dflowedit selftest:multi] no payload binding (doc-only shell) -> "
            f"viewport/transport N/A", flush=True)
    ok = bool(all_bound and tab_ok and vp_ok and transport_ok and vis_ok)
    print(f"[dflowedit selftest:multi] bindings={len(self._bindings)} all_models_bound={all_bound} "
          f"tab_switch_rebind={tab_ok} contributor_visible={vis_ok} -> {'PASS' if ok else 'FAIL'}",
          flush=True)
    print(f"DFLOWEDIT_SELFTEST_RESULT={'PASS' if ok else 'FAIL'}", flush=True)

  ##############################################################################
  # --flagtest : display-flag pixel change + bypass-triggers-rebake (via the ECS scene)
  ##############################################################################

  def _flagtestTick(self):
    host = self._viewport_host
    if host is None:
      # doc-only family: flags are inert; report NOT-APPLICABLE and pass the boot check.
      self._selftest_done = True
      print("[dflowedit flagtest] no viewport host (doc-only family) -> N/A", flush=True)
      print("DFLOWEDIT_FLAGTEST_RESULT=NA", flush=True)
      self.ezapp.signalExit()
      return
    st = self._st
    if st is None:
      st = self._st = {"stage": "settle", "f": 0}
    st["f"] += 1
    f = st["f"]
    stage = st["stage"]
    model = self._binding.node_model

    if stage == "settle":
      if f >= 90:
        # pick two distinct displayable nodes + one bypassable node on the root level.
        disp = [nid for nid in model.nodes() if model.has_display_flag(nid)]
        byp = [nid for nid in model.nodes() if model.has_bypass_flag(nid)]
        st["disp"] = disp
        st["byp"] = byp
        # a family whose DISPLAY flag does NOT gate its render (particles: ONE fixed particle
        # renderer — the output-node marker is cosmetic, not a materialize selector) runs ONLY
        # the meaningful bypass oracle; the display A/B pixel oracle is inapplicable (N/A), never
        # a spurious FAIL. Terrain / hypermesh (display flag == which node materializes) default
        # True and run the full oracle unchanged.
        st["display_gated"] = getattr(host, "display_flag_gates_render", True)
        if not byp:
          st["stage"] = "insufficient"
          return
        if not st["display_gated"]:
          # particles: (1) the display affordance must be ABSENT (not merely inapplicable) —
          # NO node offers a display flag; (2) a bypass on a NON-bypassable node (POOL /
          # EMITTER) is refused LOUDLY and does not mark the flag; (3) a bypass on a real
          # chain op re-instantiates the sim (rebuild counter bumps).
          st["affordance_absent"] = (len(disp) == 0)
          nonbyp = [nid for nid in model.nodes() if not model.has_bypass_flag(nid)]
          st["nonbyp"] = nonbyp
          refusals = []
          for nid in nonbyp:
            r = model.set_bypassed(nid, True)
            refusals.append((nid, isinstance(r, tuple) and r and r[0] == "refused",
                             not model.is_bypassed(nid), r[1] if isinstance(r, tuple) else ""))
          st["refusals"] = refusals
          st["refusals_ok"] = bool(nonbyp) and all(reff and notset for (_n, reff, notset, _m)
                                                    in refusals)
          st["rc_before"] = host.rebuild_count   # bypass a real chain op -> re-instantiate
          model.set_bypassed(byp[0], True)
          host._requestRebake()
          st["at"] = f + 60
          st["stage"] = "wait_byp"
          return
        if len(disp) < 2:
          st["stage"] = "insufficient"
          return
        model.set_output(disp[0], True)          # display A
        host._requestRebake()
        st["at"] = f + 60
        st["stage"] = "wait_A"
    elif stage == "insufficient":
      print(f"[dflowedit flagtest] insufficient flaggable nodes "
            f"(display={len(st.get('disp', []))}, bypass={len(st.get('byp', []))}) -> FAIL",
            flush=True)
      print("DFLOWEDIT_FLAGTEST_RESULT=FAIL", flush=True)
      self._selftest_done = True
      self.ezapp.signalExit()
    elif stage == "wait_A":
      if f >= st["at"]:
        self._cap_request(); st["stage"] = "cap_A"
    elif stage == "cap_A":
      if not self._cap_pending and self._cap_result is not None:
        st["mean_A"] = self._cap_result["mean"]
        st["rgb_A"] = self._cap_result["rgb"]
        model.set_output(st["disp"][1], True)    # display B (exclusive — replaces A)
        host._requestRebake()
        st["at"] = f + 60
        st["stage"] = "wait_B"
    elif stage == "wait_B":
      if f >= st["at"]:
        self._cap_request(); st["stage"] = "cap_B"
    elif stage == "cap_B":
      if not self._cap_pending and self._cap_result is not None:
        st["mean_B"] = self._cap_result["mean"]
        st["rgb_B"] = self._cap_result["rgb"]
        # bypass oracle: record the rebuild counter, toggle a bypass, expect it to bump.
        st["rc_before"] = host.rebuild_count
        model.set_bypassed(st["byp"][0], True)
        host._requestRebake()
        st["at"] = f + 60
        st["stage"] = "wait_byp"
    elif stage == "wait_byp":
      if f >= st["at"]:
        st["rc_after"] = host.rebuild_count
        self._flagtestFinish()
        self._selftest_done = True
        self.ezapp.signalExit()
    if f > 1600 and not self._selftest_done:
      print("[dflowedit flagtest] TIMEOUT; FAIL", flush=True)
      print("DFLOWEDIT_FLAGTEST_RESULT=FAIL", flush=True)
      self._selftest_done = True
      self.ezapp.signalExit()

  def _flagtestFinish(self):
    import numpy
    st = self._st
    bypass_rebaked = st["rc_after"] > st["rc_before"]
    # a family whose display flag does NOT gate render ran the bypass-only oracle: report
    # display as N/A and verdict solely on the (meaningful) bypass re-instantiation.
    if not st.get("display_gated", True):
      affordance_absent = bool(st.get("affordance_absent"))
      refusals_ok = bool(st.get("refusals_ok"))
      ok = bool(affordance_absent and refusals_ok and bypass_rebaked)
      print("[dflowedit flagtest] display affordance ABSENT for this family (no node offers a "
            f"display flag) -> affordance_absent={affordance_absent}", flush=True)
      for (nid, reff, notset, msg) in st.get("refusals", []):
        print(f"[dflowedit flagtest] bypass refused on non-bypassable {nid!r}: "
              f"refused={reff} flag_unset={notset} :: {msg}", flush=True)
      print(f"[dflowedit flagtest] refusals_ok={refusals_ok} "
            f"(POOL/EMITTER refuse loudly, flag never marked)", flush=True)
      print(f"[dflowedit flagtest] bypass rebuild_count {st['rc_before']}->{st['rc_after']} "
            f"-> bypass_rebaked={bypass_rebaked}", flush=True)
      print(f"[dflowedit flagtest] -> {'PASS' if ok else 'FAIL'}", flush=True)
      print(f"DFLOWEDIT_FLAGTEST_RESULT={'PASS' if ok else 'FAIL'}", flush=True)
      return
    a, b = st["rgb_A"], st["rgb_B"]
    pix_diff = 0.0
    if a is not None and b is not None and a.shape == b.shape:
      pix_diff = float(numpy.abs(a.astype(numpy.float32) - b.astype(numpy.float32)).mean())
    display_changed = pix_diff > 1.0                          # mean abs 8-bit delta
    ok = bool(display_changed and bypass_rebaked)
    print(f"[dflowedit flagtest] display A mean={st['mean_A']:.5f} B mean={st['mean_B']:.5f} "
          f"pixel_abs_diff={pix_diff:.4f} -> display_changed={display_changed}", flush=True)
    print(f"[dflowedit flagtest] bypass rebuild_count {st['rc_before']}->{st['rc_after']} "
          f"-> bypass_rebaked={bypass_rebaked}", flush=True)
    print(f"[dflowedit flagtest] -> {'PASS' if ok else 'FAIL'}", flush=True)
    print(f"DFLOWEDIT_FLAGTEST_RESULT={'PASS' if ok else 'FAIL'}", flush=True)

  ##############################################################################
  # --bypass-ab : deterministic capture driver. Advance the (already-bypassed) effective-
  # graph sim a FIXED number of PLAYING ticks, capture the viewport, write RGB .npy, exit.
  ##############################################################################

  def _bypassAbTick(self):
    import numpy
    host = self._viewport_host
    spec = self._bypass_ab
    st = self._st
    if st is None:
      st = self._st = {"stage": "advance", "f": 0}
    if host is None:
      print("BYPASS_AB_RESULT=FAIL (no viewport host)", flush=True)
      self._selftest_done = True
      self.ezapp.signalExit()
      return
    st["f"] += 1
    f = st["f"]
    stage = st["stage"]
    target = int(spec.get("advance", 120))
    if stage == "advance":
      # advance to the fixed absolute tick target (the rebake applied on the first update,
      # so tick 0 already ran the effective graph), then PAUSE — the last computed frame is
      # frozen at EXACTLY the target tick so the capture is deterministic regardless of the
      # update/GPU pipeline lag (a frozen frame reads identically on any later GPU frame).
      if host.tick_count >= target:
        host.pause()
        st["at"] = f + 24
        st["stage"] = "settle"
    elif stage == "settle":
      if f >= st["at"]:
        self._cap_request()
        st["stage"] = "cap"
    elif stage == "cap":
      if not self._cap_pending and self._cap_result is not None:
        rgb = self._cap_result["rgb"]
        out = spec["out"]
        try:
          numpy.save(out, rgb if rgb is not None else numpy.zeros((1, 1, 3), numpy.uint8))
          # lit-pixel count (frame pixels above a small luma threshold) — the empty-render
          # observable for the all-terminals-bypassed gate.
          lit = 0
          if rgb is not None:
            luma = rgb.astype(numpy.float32).mean(axis=2)
            lit = int((luma > 8.0).sum())
          print(f"[dflowedit bypass-ab] bypass={spec.get('bypass')} tick={host.tick_count} "
                f"mean={self._cap_result['mean']:.5f} lit_pixels={lit} -> {out}", flush=True)
          print("BYPASS_AB_RESULT=OK", flush=True)
        except Exception as ex:
          print(f"BYPASS_AB_RESULT=FAIL ({ex})", flush=True)
        self._selftest_done = True
        self.ezapp.signalExit()
    if f > 3000 and not self._selftest_done:
      print("BYPASS_AB_RESULT=FAIL (timeout)", flush=True)
      self._selftest_done = True
      self.ezapp.signalExit()

  ##############################################################################
  # --bench-ab : LIVE-params gate. Advance the bench sim, capture, tweak a LIVE motion
  # param (radius) via the DISTINCT bench propsheet model (next frame DIFFERS, rebuild
  # counter UNCHANGED), then toggle `enabled` (rebuild counter INCREMENTS — honest rebake).
  ##############################################################################

  def _benchAbTick(self):
    import numpy
    host = self._viewport_host
    spec = self._bench_ab
    st = self._st
    if st is None:
      st = self._st = {"stage": "advance0", "f": 0}
    bm = self._binding.bench_prop_model if self._binding is not None else None
    if host is None or bm is None:
      print("BENCH_AB_RESULT=FAIL (no bench payload — asset has no enabled TESTBENCH?)", flush=True)
      self._selftest_done = True
      self.ezapp.signalExit()
      return
    st["f"] += 1
    f = st["f"]
    stage = st["stage"]
    A = int(spec.get("advance", 90))

    if stage == "advance0":
      if host.tick_count >= A:
        host.pause(); st["at"] = f + 24; st["stage"] = "settle0"
    elif stage == "settle0":
      if f >= st["at"]:
        self._cap_request(); st["stage"] = "cap0"
    elif stage == "cap0":
      if not self._cap_pending and self._cap_result is not None:
        st["f0"] = self._cap_result["rgb"]
        st["rc0"] = host.rebuild_count
        cur = bm.getValue("motion.radius")
        st["radius0"] = cur
        bm.setValue("motion.radius", (float(cur) * 2.2) if cur else 5.0)   # LIVE tweak (no rebake)
        st["rc_after_radius"] = host.rebuild_count      # expect == rc0
        host.start(); st["target"] = host.tick_count + A; st["stage"] = "advance1"
    elif stage == "advance1":
      if host.tick_count >= st["target"]:
        host.pause(); st["at"] = f + 24; st["stage"] = "settle1"
    elif stage == "settle1":
      if f >= st["at"]:
        self._cap_request(); st["stage"] = "cap1"
    elif stage == "cap1":
      if not self._cap_pending and self._cap_result is not None:
        st["f1"] = self._cap_result["rgb"]
        st["rc1"] = host.rebuild_count                  # expect == rc0 (radius was LIVE)
        bm.setValue("enabled", False)                   # honest rebake (graph re-instantiation)
        host.start(); st["target"] = host.tick_count + 40; st["stage"] = "advance2"
    elif stage == "advance2":
      if host.tick_count >= st["target"]:
        st["rc2"] = host.rebuild_count                  # expect > rc1
        self._benchAbFinish()
        self._selftest_done = True
        self.ezapp.signalExit()
    if f > 3000 and not self._selftest_done:
      print("BENCH_AB_RESULT=FAIL (timeout)", flush=True)
      self._selftest_done = True
      self.ezapp.signalExit()

  def _benchAbFinish(self):
    import numpy
    st = self._st

    def _mad(a, b):
      if a is None or b is None or a.shape != b.shape:
        return -1.0
      return float(numpy.abs(a.astype(numpy.float32) - b.astype(numpy.float32)).mean())

    radius_mad = _mad(st.get("f0"), st.get("f1"))
    radius_live = radius_mad > 1.0                       # next frame DIFFERS after the LIVE tweak
    radius_no_rebake = st["rc1"] == st["rc0"] == st["rc_after_radius"]
    enabled_rebakes = st["rc2"] > st["rc1"]
    out = self._bench_ab.get("out")
    if out:
      try:
        numpy.save(out + "_f0.npy", st.get("f0"))
        numpy.save(out + "_f1.npy", st.get("f1"))
      except Exception:
        pass
    ok = bool(radius_live and radius_no_rebake and enabled_rebakes)
    print(f"[dflowedit bench-ab] radius {st.get('radius0')}->x2.2 LIVE: frame_MAD={radius_mad:.4f} "
          f"(>1 changed={radius_live})", flush=True)
    print(f"[dflowedit bench-ab] rebuild rc0={st['rc0']} after_radius={st['rc_after_radius']} "
          f"rc1={st['rc1']} (radius_no_rebake={radius_no_rebake}) enabled_toggle rc1={st['rc1']}"
          f"->rc2={st['rc2']} (enabled_rebakes={enabled_rebakes})", flush=True)
    print(f"BENCH_AB_RESULT={'OK' if ok else 'FAIL'}", flush=True)
