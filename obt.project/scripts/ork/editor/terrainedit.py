################################################################################
# TerrainEditor — ork.terrain.edit.py v1 (JUL09 S1).
#
# A minimal desktop terrain-graph editor: viewport + toolbar + document outliner +
# property sheet + rebake loop. Every edit mutates the structured DOCUMENT (owner
# law L2), then re-elaborates + rebakes via TerrainRuntime; the property sheet and
# outliner bind the DOCUMENT, never the derived GraphData.
#
# Forks the SceneEditorBase / ecsedit dock idioms (viewport DockablePanel fill +
# left split VerticalPack[Toolbar, Outliner] + bottom-split PropertySheet) but is
# NOT ECS-coupled — no ecs modules are imported.
#
# Rebake happens on the GPU thread (_onGpuUpdate) at the PREVIEW dim during editing
# and full-res on demand; property edits schedule a rebake on value settle (idle
# debounce) rather than per drag-tick.
################################################################################

import glob
import os
import time

# terrain dim-flow trace: per-node bake register dims + drawable layout + scene-data
# layout (see [terrain-dim] lines) — ON by default in the editor so a stale-size /
# stale-dim register shows up in the log instead of as a mesh discontinuity.
os.environ.setdefault("ORKID_TERRAIN_DIMLOG", "1")

from orkengine.core import vec3, vec4, VarMap, CrcStringProxy
from orkengine import lev2
from orkengine import ecs
from orkengine.lev2 import PostFxNodeACES, PostFxNodeHSVG, PbrCommon
from ork.app.application import ComponentizedApplication
from ork.ui import icon_library
from ork.ui.filesystem_browser import FilesystemBrowser

from ork.editor.terrain_runtime import TerrainRuntime, terrain_debug_material_labels
from ork.editor.undo_stack import UndoStack
from ork.editor.terrain_doc_model import (
    TerrainDocOutlinerModel, TerrainNodePropertyModel,
    TerrainParamsPropertyModel, TERRAIN_PARAMS_KEY)

tokens = CrcStringProxy()

_REBAKE_IDLE_S = 0.25   # settle window: rebake this long after the last edit tick
_SWAP_HOLD_MAX_S = 3.0  # hold-last-frame cap: swap to the fresh scenegraph anyway after this


class TerrainEditor(ComponentizedApplication):

  def __init__(self, source, *, dsl_class=None, extent_m=None,
               preview_dim=1024, full_dim=4096, chunk=128, dsl_kwargs=None,
               offscreen=False, selftest=False, keytest=False):
    super().__init__()
    self.runtime = TerrainRuntime(preview_dim=preview_dim, full_dim=full_dim, chunk=chunk)
    self.runtime.load(source, dsl_class=dsl_class, extent_m=extent_m,
                      **(dsl_kwargs or {}))
    self._full_res = False
    self._rebake_pending = False
    self._last_edit_time = 0.0
    self._sgv_swap_pending = False     # hold-last-frame: fresh sg built, rebind deferred
    self._sgv_swap_started = 0.0
    # undo/redo (S2): snapshot checkpoints of (doc-JSON, kwargs, display key). Every
    # document/session mutation records one; drag ticks coalesce by edit key.
    self._undo = UndoStack(limit=100)
    self._undo_state = self.runtime.capture_undo_state()
    self._rebuilding = False          # guards the update-thread sim tick during a rebuild
    self._selected_obj = None
    self._home_dir = os.path.expanduser("~")

    # post-fx + envmap (E/G/T/H keys) — same preset lists/defaults as ork.ecsplay /
    # scene.viewer. Nodes are built ONCE in _onGpuInit and held here so their state
    # survives the per-edit scene rebuild (the runtime re-splices the same objects).
    self._satset = [0.0, 0.1, 0.2, 0.5, 0.75, 0.8, 1.0, 1.25, 1.5, 1.75, 2.0]
    self._gamset = [0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.4]
    self._expset = [0.0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 5.0]
    self._sat_idx = self._satset.index(1.0)
    self._gam_idx = self._gamset.index(1.0)
    self._exp_idx = self._expset.index(1.0)
    self._aces = None
    self._hsvg = None
    stage_dir = os.environ.get("OBT_STAGE", "")
    envmap_files = sorted(glob.glob(os.path.join(stage_dir, "assetcache", "envmaps2", "*.xir")))
    self._envmap_names = [os.path.splitext(os.path.basename(f))[0] for f in envmap_files]
    self._envmap_paths = [f"<assetcache>/envmaps2/{n}.xir" for n in self._envmap_names]
    self._skybox_cache = {}
    self._skybox_index = -1
    # M (material mode) — pre-wired, render-inert until the M lane merges (logs + fires
    # a guarded systemNotify the M-lane system will consume).
    self._mat_modes = terrain_debug_material_labels()  # data-driven: declared + runtime's debug looks
    self._mat_mode = 0

    # scripted offscreen self-test (the headless two-thread rebuild gate): drive a rebuild
    # after the scene settles, confirm the sim swapped live, then signalExit.
    self._selftest = bool(selftest)
    self._selftest_ok = False
    self._selftest_frame = 0
    self._selftest_stage = "settle"
    self._selftest_ctrl0 = None
    self._selftest_prev_ctrl = None
    self._selftest_rounds = 3          # hammer the swap N times (offscreen-unthrottled race)
    self._selftest_round = 0
    self._selftest_settle_at = 0
    self._selftest_after_cap = None
    self._cap_pending = False
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None
    self._cap_path = None
    self._captures = []                # [{round, label, path, rng, mean, lit}] — DISPLAY proof
    self._cap_label = None
    self._selftest_dir = None

    # scripted offscreen KEY gate: drive the post-fx/env key handlers directly and prove
    # G shifts luma + persists across a rebuild, and E swaps the radiance maps (non-crash).
    self._keytest = bool(keytest)
    self._keytest_results = {}
    self._keytest_frame = 0
    self._keytest_stage = "settle"
    self._keytest_settle_at = 0
    self._keytest_prev_ctrl = None
    self._keytest_gamma_set = None
    self._keytest_rad0 = None
    self._keytest_rad1 = None
    self._caps_by_label = {}

    if self._selftest or self._keytest:
      import tempfile as _tf
      self._selftest_dir = _tf.mkdtemp(prefix="tered_edit_selftest_")

    self.outliner_model = TerrainDocOutlinerModel(self.runtime.document)
    self.prop_model = TerrainNodePropertyModel(None, on_changed=self._onPropertyEdited)
    self.params_model = TerrainParamsPropertyModel(self.runtime, on_changed=self._onParamsEdited)

    # ECS module init injected BEFORE GPU finalization (ecsedit's mechanism) — the
    # runtime hosts an in-code ECS scene, so the SceneData system-class registry must
    # be populated before build_scene_data()/_start_simulation() run in _onGpuInit.
    self.createEzApp(name="terrainedit", pre_init_fns=self._getPreInitFns(),
                     offscreen=(offscreen or self._selftest or self._keytest))

  def _getPreInitFns(self):
    """Extension point (mirrors ecsedit): pre-init fns for createEzApp — registers ECS
    reflected classes so the in-code scene lowers without an empty system registry."""
    return [ecs.ecsInitCallback]

  ##############################################################################
  # UI layout
  ##############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.13, 0.13, 0.15, 1)

    viewport_item = lg.makeChild(fill=True, margin=2, uiclass=lev2.ui.DockablePanel,
                                 args=["viewport_dock"])
    self.viewport_dock = viewport_item.widget
    self.viewport_dock.titlebar_color = vec4(0.15, 0.2, 0.25, 1)
    self.sgv = self.viewport_dock.createChild(
        uiclass=lev2.ui.SceneGraphViewport, args=["Viewport", vec4(0.1, 0.1, 0.12, 1)])

    left_item = lg.split(layout=viewport_item.layout, proportion=0.35,
                         placement=tokens.LEFT, margin=2,
                         uiclass=lev2.ui.DockablePanel, args=["left_dock"])
    self.left_dock = left_item.widget
    self.left_dock.titlebar_color = vec4(0.2, 0.15, 0.2, 1)

    self.left_panel = self.left_dock.createChild(uiclass=lev2.ui.VerticalPack, args=["Terrain"])
    self.left_panel.margin = 2
    self.left_panel.item_height = 34

    self._setupToolbar()

    self.outliner = self.left_panel.makeChild(uiclass=lev2.ui.Outliner, args=["outliner"])
    self.left_panel.fill_widget = self.outliner
    self.outliner.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.outliner.item_height = 22

    propsheet_item = lg.split(layout=left_item.layout, proportion=0.55,
                              placement=tokens.BOTTOM, margin=2,
                              uiclass=lev2.ui.DockablePanel, args=["propsheet_dock"])
    self.propsheet_dock = propsheet_item.widget
    self.propsheet_dock.titlebar_color = vec4(0.2, 0.2, 0.15, 1)
    self.propsheet = self.propsheet_dock.createChild(
        uiclass=lev2.ui.PropertySheet, args=["propsheet"])
    self.propsheet.bgcolor = vec4(0.12, 0.12, 0.12, 1)
    self.propsheet.label_width = 150
    self.propsheet.row_height = 26

  def _setupToolbar(self):
    self.toolbar = self.left_panel.makeChild(uiclass=lev2.ui.HorizontalPack, args=["toolbar"])
    self.toolbar.margin = 2
    self.toolbar.item_width = 52
    self.toolbar.bg_color = vec4(0.12, 0.12, 0.15, 1)

    def make_icon(text):
      return icon_library.from_svg_string(
          '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">'
          '<text x="12" y="12" text-anchor="middle" dominant-baseline="central" '
          'font-family="sans-serif" font-size="6" font-weight="bold" fill="#E6E6E6">'
          f'{text}</text></svg>', 32, 32)

    self._res_btn = None
    self._caps_btn = None
    specs = [
      ("New", vec4(0.22, 0.22, 0.15, 1), lambda b: self._doNewTerrain()),
      ("Open", vec4(0.15, 0.25, 0.15, 1), lambda b: self._openTerrainPopup()),
      ("Save", vec4(0.15, 0.15, 0.25, 1), lambda b: self._saveDocPopup()),
      ("Bake", vec4(0.22, 0.18, 0.12, 1), lambda b: self._requestRebake()),
      ("Prev", vec4(0.18, 0.18, 0.22, 1), lambda b: self._toggleResolution()),
      ("Caps", vec4(0.16, 0.26, 0.20, 1), lambda b: self._toggleCaptures()),
    ]
    for name, color, handler in specs:
      btn = self.toolbar.makeChild(uiclass=lev2.ui.ImageButton, args=[f"btn_{name.lower()}"])
      btn.inactive_image = make_icon(name)
      btn.bgcolor = color
      btn.inactive_blend_mode = tokens.ALPHA
      btn.onPressed = handler
      if name == "Prev":
        self._res_btn = btn
        self._make_icon = make_icon
      if name == "Caps":
        self._caps_btn = btn

  def _toggleCaptures(self):
    """Outliner header toggle: show/hide capture rows (the ecsedit filter-toggle
    idiom). Captures are the bake's output contract — not deletable — so hiding
    them declutters the processing chain; the button color reflects state."""
    vis = self.outliner_model.set_show_captures(not self.outliner_model.show_captures)
    if self._caps_btn is not None:
      self._caps_btn.bgcolor = (vec4(0.16, 0.26, 0.20, 1) if vis
                                else vec4(0.10, 0.11, 0.12, 1))
    print(f"[terrainedit] outliner captures {'shown' if vis else 'hidden'}", flush=True)

  ##############################################################################
  # GPU init
  ##############################################################################

  def _onGpuInit(self, ctx):
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    self.uicontext.theme_engine = lev2.ui.ThemeEngine(self.custom_db)

    self.runtime.set_context(ctx)
    self.runtime.setup_camera()

    # post-fx nodes built ONCE + held; the runtime re-splices these SAME objects into
    # every fresh scenegraph, so gamma/exposure/saturation survive the per-edit rebuild.
    self._aces = PostFxNodeACES()
    self._aces.exposure = self._expset[self._exp_idx]
    self._aces.gpuInit(ctx, 8, 8)
    self._hsvg = PostFxNodeHSVG()
    self._hsvg.hue        = 0.0
    self._hsvg.saturation = self._satset[self._sat_idx]
    self._hsvg.value      = 1.0
    self._hsvg.gamma      = self._gamset[self._gam_idx]
    self._hsvg.gpuInit(ctx, 8, 8)
    self.runtime.postfx_nodes = [self._aces, self._hsvg]

    # in-code ECS scene from the DOCUMENT — the C++ terrain path bakes + renders it.
    self.runtime.create_live_scene(ctx, dim=self._current_dim())

    self.sgv.cameraName = "spawncam"
    self.sgv.scenegraph = self.runtime.scenegraph
    self.sgv.camera_evhandler = lambda ev: self._on_viewport_event(ev)
    self.sgv.forkDB()
    self.runtime.bind_viewport(self.sgv)

    # outliner + propsheet wiring
    self.outliner.model = self.outliner_model
    self.outliner.onSelect(self._onOutlinerSelect)
    self.outliner.onBadgeClick(self._onOutlinerBadge)
    self.outliner.onKeyDown(self._onOutlinerKey)   # Delete/Backspace -> delete node (undoable)
    self.outliner.onAdd(self._onOutlinerAdd)       # (inline add-mode commit path, kept wired)
    self.outliner.onShiftEnter(self._onOutlinerAddMenu)  # Shift+Enter -> add-module MENU (ecsedit idiom)
    self.outliner_model.display_key_provider = lambda: self.runtime.display_key
    self.propsheet.model = self.prop_model
    self.propsheet.onPropertyChanged(self._onPropsheetChanged)
    self._refreshParamsBinding()      # top-level "Terrain Parameters" row (DSL sessions)
    self.outliner.expandAll()

    if self._selftest:
      self._selftest_ctrl0 = self.runtime.controller   # baseline sim (pre-rebuild)

  ##############################################################################
  # selection + edit loop
  ##############################################################################

  def _onOutlinerSelect(self, key):
    if key == TERRAIN_PARAMS_KEY:
      # top-level "Terrain Parameters" (the DSL ctor kwargs) — bind the params model.
      self._selected_obj = None
      self.params_model.refresh()
      self.propsheet.model = self.params_model
      self.propsheet.rebuild()
      return
    obj = self.outliner_model.object_for_key(key)
    self._selected_obj = obj
    self.propsheet.model = self.prop_model
    self.prop_model.set_object(obj)
    self.propsheet.rebuild()

  def _onPropsheetChanged(self, key, value):
    # the document write already happened in the model's setValue (node set_param OR a
    # Terrain-Parameter set_dsl_kwarg re-trace); schedule a rebake on the GPU thread once
    # edits settle (not per drag-tick). ONE edit pipeline for both param kinds.
    # UNDOABLE: consecutive edits of the same property coalesce into one step.
    self._recordEdit(f"edit {key}", coalesce_key=f"prop:{id(self._selected_obj)}:{key}")
    self._last_edit_time = time.time()
    self._rebake_pending = True

  ##############################################################################
  # undo/redo (S2) — snapshot checkpoints; restore refreshes models + rebakes
  ##############################################################################

  def _recordEdit(self, label, coalesce_key=None):
    """Record an already-applied mutation as an undoable step (pre-state comes from
    the cached snapshot, post-state is captured NOW)."""
    post = self.runtime.capture_undo_state()
    self._undo.record(pre=self._undo_state, post=post,
                      restore=self._restoreUndoState,
                      label=label, coalesce_key=coalesce_key)
    self._undo_state = post

  def _restoreUndoState(self, state):
    self.runtime.restore_undo_state(state)
    self._undo_state = state
    # the document object was REPLACED (from_json) — refresh everything that caches it
    self._selected_obj = None
    self.prop_model.set_object(None)
    self.params_model.refresh()
    self.outliner_model.set_document(self.runtime.document)
    self.outliner.expandAll()
    self._requestRebake()

  def _doUndo(self):
    label = self._undo.undo()
    print(f"[terrainedit] undo: {label if label else '(nothing to undo)'}", flush=True)

  def _doRedo(self):
    label = self._undo.redo()
    print(f"[terrainedit] redo: {label if label else '(nothing to redo)'}", flush=True)

  def _onOutlinerKey(self, selected_key, keycode):
    if keycode in (259, 261) and selected_key:   # Backspace / Delete
      self._deleteSelected(selected_key)

  def _onOutlinerAdd(self, new_key):
    # model.createItem already mutated the document and the C++ add flow selected
    # the new row (propsheet rebound via onSelect). Record the undo step + rebake.
    print(f"[terrainedit] added {new_key}", flush=True)
    self._recordEdit(f"add {new_key}")
    self._requestRebake()

  def _onOutlinerAddMenu(self, key):
    """Shift+Enter on a row -> the add-module CONTEXT MENU (the ecsedit
    DropdownMenu idiom). A node row inserts after it; a loop row appends into
    its body — same createItem mutation as the inline add mode."""
    factories = self.outliner_model.getFactories(key)
    if not factories:
      print(f"[terrainedit] add: {key!r} accepts no modules "
            f"(select a node or loop row)", flush=True)
      return
    paths = [f"/{f['id']}" for f in factories]
    rx, ry = self.outliner.localToRoot(0, 0)
    lev2.ui.DropdownMenu.show(
        context=self.uicontext,
        paths=paths,
        x=rx, y=ry,
        on_selected=lambda val, k=key: self._onAddMenuSelected(k, val))

  def _onAddMenuSelected(self, key, value):
    op = value.lstrip("/")
    new_key = self.outliner_model.createItem(key, op, op)
    if not new_key:
      return                              # refusal already printed loudly
    self.outliner.selected_key = new_key  # fires onSelect -> propsheet rebinds
    print(f"[terrainedit] added {new_key}", flush=True)
    self._recordEdit(f"add {new_key}")
    self._requestRebake()

  def _visibleModelKeys(self):
    """Flattened outliner document rows in display order (synthetic top extras
    excluded — they resolve to no document object)."""
    out = []
    def _walk(pk):
      for k in self.outliner_model.getChildren(pk):
        if self.outliner_model.object_for_key(k) is not None:
          out.append(k)
          _walk(k)
    _walk("")
    return out

  def _deleteSelected(self, key):
    from ork.hypergraph.dflow.terrain.doc import DocNode, TerrainDocParamError, delete_node
    obj = self.outliner_model.object_for_key(key)
    if not isinstance(obj, DocNode):
      print(f"[terrainedit] delete: {key!r} is not a deletable node (v1: nodes only)",
            flush=True)
      return
    order_before = self._visibleModelKeys()
    try:
      delete_node(self.runtime.document, obj)
    except TerrainDocParamError as ex:
      print(f"[terrainedit] {ex}", flush=True)
      return
    print(f"[terrainedit] deleted {key}", flush=True)
    self._recordEdit(f"delete {key}")
    self._selected_obj = None
    self.prop_model.set_object(None)
    self.outliner_model.set_document(self.runtime.document)
    self.outliner.expandAll()
    # chain-delete UX: select the row that took the deleted row's place (next in
    # display order; the previous one at the end) so Delete can repeat through a
    # run of nodes. setSelectedKey fires onSelect -> the propsheet rebinds.
    try:
      idx = order_before.index(key)
    except ValueError:
      idx = -1
    if idx >= 0:
      order_after = set(self._visibleModelKeys())
      candidates = order_before[idx + 1:] + order_before[:idx][::-1]
      nxt = next((k for k in candidates if k in order_after), None)
      if nxt is not None:
        self.outliner.selected_key = nxt
    self._requestRebake()

  def _onOutlinerBadge(self, key, badge_id):
    # badge toggles ride the SAME settle->rebake pipeline as property edits.
    # bypass mutates the DOCUMENT (persisted); display is runtime SESSION state.
    from ork.hypergraph.dflow.terrain.doc import DocNode, TerrainDocParamError
    obj = self.outliner_model.object_for_key(key)
    if obj is None:
      return
    if badge_id == "bypass":
      # bypass routes by the object's set_bypassed method — DocNode AND the structural
      # constructs (DocLoop / DocGroupCall) expose it; a refusal (generator group /
      # switch / source node / capture) is LOUD and non-mutating.
      if not hasattr(obj, "set_bypassed"):
        return
      try:
        obj.set_bypassed(not obj.bypassed)
      except TerrainDocParamError as ex:
        print(f"[terrain] {ex}", flush=True)
        return
      self._recordEdit(f"bypass {key}")      # UNDOABLE (owner): each toggle = one step
    elif badge_id == "display":
      if not isinstance(obj, DocNode):        # display (select-as-output) is NODE-only
        return
      try:
        self.runtime.set_display_key(None if self.runtime.display_key == key else key)
      except ValueError as ex:
        print(f"[terrain] {ex}", flush=True)
        return
      self._recordEdit(f"display {key}")     # UNDOABLE (owner): display rides checkpoints
    else:
      return
    self.outliner_model.notifyModelReset()   # re-cache badges (exclusive display moved)
    self._last_edit_time = time.time()
    self._rebake_pending = True

  def _onPropertyEdited(self, key):
    # model-side hook (fires alongside onPropertyChanged) — refresh the outliner
    # labels when a STRUCTURAL value (loop count / switch selector) changed.
    from ork.hypergraph.dflow.terrain.doc import DocLoop, DocSwitch
    if isinstance(self._selected_obj, (DocLoop, DocSwitch)):
      self.outliner_model.set_document(self.runtime.document)
      self.outliner.expandAll()

  def _onParamsEdited(self, key):
    # a Terrain-Parameter edit RE-TRACED the document (a NEW document object) — refresh
    # the outliner so its cached doc objects are the live ones (the structural-edit path).
    # notifyModelReset does NOT fire onSelect, so the propsheet stays on the params model
    # (selection resets gracefully — the params sheet keeps showing). The rebake is
    # scheduled by _onPropsheetChanged (one pipeline).
    self.outliner_model.set_document(self.runtime.document)
    self.outliner.expandAll()

  def _refreshParamsBinding(self):
    """Bind the Terrain Parameters model to the current runtime. The top-level
    outliner entry is ALWAYS present now — every session has at least the session
    rows (dim); DSL sessions add the ctor kwargs."""
    self.params_model.set_runtime(self.runtime)
    self.outliner_model.set_top_extras([(TERRAIN_PARAMS_KEY, "Terrain Parameters")])

  def _requestRebake(self):
    self._rebake_pending = True
    self._last_edit_time = 0.0   # rebake ASAP (explicit button)

  def _toggleResolution(self):
    self._full_res = not self._full_res
    if self._res_btn is not None:
      self._res_btn.inactive_image = self._make_icon("Full" if self._full_res else "Prev")
    self._requestRebake()

  def _current_dim(self):
    return self.runtime.full_dim if self._full_res else self.runtime.preview_dim

  ##############################################################################
  # frame hooks
  ##############################################################################

  def _onGpuUpdate(self, ctx):
    # The WHOLE rebuild runs on the GPU thread — for the terrain this is required on BOTH
    # affinities that would otherwise conflict:
    #   * Phase A (build_scene_data) EAGERLY materializes the PBR material on the ctx
    #     (assignImages) — GPU-thread-only.
    #   * Phase B (createSimulation) triggers the terrain's DEFERRED bake — also GPU work
    #     (unlike ecsedit's GPU-free createSimulation, so it can NOT run on the update thread).
    #   * the sgv.scenegraph swap must be SEQUENTIAL with SceneGraphViewport::DoRePaintSurface
    #     (which reads _scenegraph across its acquire/release) — same (GPU/render) thread, no race.
    # The update thread's sim tick is paused via _rebuilding while the swap runs.
    self.runtime.gpuUpdate(ctx)
    if self._rebake_pending and (time.time() - self._last_edit_time) >= _REBAKE_IDLE_S:
      self._rebake_pending = False
      self.runtime.schedule_rebuild()
      self._rebuilding = True
      try:
        if self.runtime.prepare_rebuild(ctx, dim=self._current_dim()):   # GPU: materialize
          self.runtime.apply_pending_rebuild()                           # GPU: fresh sg + sim bake
          self._applyMaterialMode()   # the fresh sim starts at 'declared' — re-apply the held mode
          # HOLD-LAST-FRAME (owner, jul10): do NOT rebind yet — the OLD scenegraph keeps
          # presenting its last frame until the fresh sim has staged its terrain, so an
          # edit rebake never shows black/empty frames (the A/B-compare contract).
          self._sgv_swap_pending = True
          self._sgv_swap_started = time.time()
      except Exception as e:  # ops-self-defend: a bad edit must not kill the editor
        print(f"[terrainedit] rebuild failed: {e}", flush=True)
      finally:
        self._rebuilding = False
    if self._sgv_swap_pending:
      timed_out = (time.time() - self._sgv_swap_started) > _SWAP_HOLD_MAX_S
      if self.runtime.display_ready() or timed_out:
        if timed_out:
          print(f"[terrainedit] hold-last-frame: fresh terrain not staged in "
                f"{_SWAP_HOLD_MAX_S:.0f}s — swapping anyway", flush=True)
        self.sgv.scenegraph = self.runtime.scenegraph                  # rebind (render-sequential)
        self._sgv_swap_pending = False

  def _onUpdate(self, updinfo):
    # Sim tick on the update thread — skipped while a GPU-thread rebuild swaps the simulation
    # (the destroy/create must not race a live updateSimulation tick).
    if not self._rebuilding:
      self.runtime.update()
    self.sgv.setDirty()
    if self._selftest:
      self._selftestTick()
    if self._keytest:
      self._keytestTick()

  def onGpuPostFrame(self, ctx):
    # ComponentizedApplication.onGpuPostFrame only broadcasts to components — override to
    # capture the settled framebuffer (GPU thread, post-render) for the DISPLAY gate. The
    # readback is ASYNC: issue it, then DRAIN it across subsequent frames (the C++ player's
    # os_snapdrain pattern) — an in-frame cap.wait() deadlocks.
    super().onGpuPostFrame(ctx)
    if not (self._selftest or self._keytest):
      return
    if self._cap_pending and not self._cap_inflight:
      self._capIssue(ctx)
    elif self._cap_inflight:
      ready = (self._cap_async is None) or bool(self._cap_async.is_ready)
      if ready:
        self._capFinish()

  def _cap_request(self, name):
    self._cap_path = os.path.join(self._selftest_dir, name + ".png")
    self._cap_label = name
    self._cap_pending = True

  def _capIssue(self, ctx):
    """Issue an async readback of the MAIN composited render target (FBI.main_RTG — the same
    buffer the C++ player's --snapshot captures)."""
    from orkengine.lev2 import CaptureBuffer
    try:
      rtg = ctx.FBI.main_RTG
      self._cap_buf = CaptureBuffer()
      self._cap_async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap_buf, "RGBA8")
      self._cap_inflight = True
    except Exception as e:
      print(f"[terrainedit selftest] capture issue error: {e}", flush=True)
      self._cap_pending = False

  def _capFinish(self):
    """The readback landed — decode -> PNG + a non-black metric (proves the terrain DRAWS)."""
    import numpy
    lit = False; rng = 0.0; mean = 0.0
    try:
      capbuf = self._cap_buf
      w, h = capbuf.width, capbuf.height
      if w > 0 and h > 0:
        arr = numpy.array(capbuf, dtype=numpy.uint8).reshape(h, w, 4)
        rgb = arr[..., :3]
        gray = rgb.astype(numpy.float32).mean(axis=2) / 255.0
        rng = float(gray.max() - gray.min()); mean = float(gray.mean())
        lit = rng > 0.02
        from PIL import Image as _PILImage
        _PILImage.fromarray(rgb[::-1]).save(self._cap_path)   # flip Y for viewing
    except Exception as e:
      print(f"[terrainedit selftest] capture finish error: {e}", flush=True)
    cap = {"round": self._selftest_round, "label": self._cap_label, "path": self._cap_path,
           "rng": rng, "mean": mean, "lit": lit}
    self._captures.append(cap)
    if self._cap_label is not None:
      self._caps_by_label[self._cap_label] = cap
    print(f"[terrainedit selftest] capture {self._cap_label} (round {self._selftest_round}): "
          f"range={rng:.4f} mean={mean:.4f} lit={lit} -> {self._cap_path}", flush=True)
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None
    self._cap_pending = False

  def _selftestTick(self):
    """Scripted offscreen DISPLAY gate (update thread): capture the initial (round 0) frame,
    then for each of N rounds request a rebuild, confirm the sim swapped, let the new terrain
    render, and capture the post-rebuild frame. Every capture must be LIT (non-black) — proves
    the terrain still DRAWS after a rebuild, not just that the sim is live."""
    self._selftest_frame += 1
    f = self._selftest_frame
    st = self._selftest_stage

    if st == "settle" and f >= 40:
      self._cap_request("round0_initial")
      self._selftest_after_cap = "rebuild"
      self._selftest_stage = "wait_cap"
    elif st == "wait_cap":
      if not self._cap_pending:
        self._selftest_stage = self._selftest_after_cap
    elif st == "rebuild":
      self._selftest_prev_ctrl = self.runtime.controller
      self._rebake_pending = True
      self._last_edit_time = 0.0           # trigger the GPU-thread rebuild next _onGpuUpdate
      self._selftest_round += 1
      self._selftest_stage = "await"
      print(f"[terrainedit selftest] round {self._selftest_round}: requesting rebuild", flush=True)
    elif st == "await":
      c = self.runtime.controller
      if c is not None and c is not self._selftest_prev_ctrl and self.runtime._sys_ref is not None:
        print(f"[terrainedit selftest] round {self._selftest_round}: sim swapped -> live", flush=True)
        # capture DURING the hold window: the viewport must still present a LIT frame
        # (the held last frame, or the fresh terrain if the swap already completed) —
        # NEVER black between rebakes (the hold-last-frame observable).
        self._cap_request(f"round{self._selftest_round}_during_hold")
        self._selftest_settle_at = f + 40   # let the new terrain bake + render
        self._selftest_after_cap = "post_settle"
        self._selftest_stage = "wait_cap"
    elif st == "post_settle":
      if f >= self._selftest_settle_at:
        self._cap_request(f"round{self._selftest_round}_postrebuild")
        self._selftest_after_cap = ("rebuild" if self._selftest_round < self._selftest_rounds
                                    else "finish")
        self._selftest_stage = "wait_cap"
    elif st == "finish":
      lit_all = all(c["lit"] for c in self._captures)
      # round 0 + (during_hold + postrebuild) per round
      got_all = len(self._captures) == (1 + 2 * self._selftest_rounds)
      self._selftest_ok = lit_all and got_all
      self._selftest_stage = "done"
      print(f"[terrainedit selftest] captures={len(self._captures)} all_lit={lit_all} "
            f"-> {'PASS' if self._selftest_ok else 'FAIL (blank frame after rebuild)'}", flush=True)
      self.ezapp.signalExit()
    if f > 2000 and self._selftest_stage != "done":
      print("[terrainedit selftest] TIMEOUT; FAIL", flush=True)
      self.ezapp.signalExit()

  ##############################################################################
  # scripted offscreen KEY gate (post-fx/env keys survive a rebuild)
  ##############################################################################

  def _keytestTick(self):
    """Capture baseline -> cycle GAMMA (G) -> capture (luma must SHIFT) -> trigger a
    rebuild -> capture (gamma must PERSIST: the SAME held HSVG node is re-spliced) ->
    cycle ENVMAP (E) -> capture (radiance maps object must SWAP; non-crash). Every step
    drives the real key HANDLER, offscreen."""
    self._keytest_frame += 1
    f = self._keytest_frame
    st = self._keytest_stage

    if st == "settle" and f >= 40:
      self._cap_request("keys_baseline"); self._keytest_stage = "wait_baseline"
    elif st == "wait_baseline":
      if not self._cap_pending:
        for _ in range(5):                                    # G handler x5 -> big luma shift
          self._keytest_gamma_set = self._cycleGamma()
        self._keytest_settle_at = f + 20; self._keytest_stage = "settle_gamma"
    elif st == "settle_gamma":
      if f >= self._keytest_settle_at:
        self._cap_request("keys_gamma"); self._keytest_stage = "wait_gamma"
    elif st == "wait_gamma":
      if not self._cap_pending:
        self._keytest_prev_ctrl = self.runtime.controller
        self._rebake_pending = True; self._last_edit_time = 0.0   # GPU-thread rebuild
        self._keytest_stage = "await_rebuild"
        print("[terrainedit keytest] requesting rebuild (gamma-persist check)", flush=True)
    elif st == "await_rebuild":
      c = self.runtime.controller
      if c is not None and c is not self._keytest_prev_ctrl and self.runtime._sys_ref is not None:
        self._keytest_settle_at = f + 40; self._keytest_stage = "settle_rebuild"
    elif st == "settle_rebuild":
      if f >= self._keytest_settle_at:
        self._cap_request("keys_gamma_rebuild"); self._keytest_stage = "wait_rebuild"
    elif st == "wait_rebuild":
      if not self._cap_pending:
        self._keytest_rad0 = self.runtime.radiance_maps
        self._cycleEnvmap()                                    # E handler
        self._keytest_rad1 = self.runtime.radiance_maps
        self._keytest_settle_at = f + 60; self._keytest_stage = "settle_envmap"
    elif st == "settle_envmap":
      if f >= self._keytest_settle_at:
        self._cap_request("keys_envmap"); self._keytest_stage = "wait_envmap"
    elif st == "wait_envmap":
      if not self._cap_pending:
        # E must SURVIVE a dflow-edit rebake: rebuild AGAIN — the fresh scene stages
        # with the SELECTED envmap (the scene data carries it), and the held tone-map
        # nodes (gamma) still report their values.
        self._keytest_prev_ctrl = self.runtime.controller
        self._rebake_pending = True; self._last_edit_time = 0.0
        self._keytest_stage = "await_rebuild2"
        print("[terrainedit keytest] requesting rebuild (envmap/tone-persist check)", flush=True)
    elif st == "await_rebuild2":
      c = self.runtime.controller
      if c is not None and c is not self._keytest_prev_ctrl and self.runtime._sys_ref is not None:
        self._keytest_settle_at = f + 40; self._keytest_stage = "settle_rebuild2"
    elif st == "settle_rebuild2":
      if f >= self._keytest_settle_at:
        self._cap_request("keys_envmap_rebuild"); self._keytest_stage = "wait_envmap_rebuild"
    elif st == "wait_envmap_rebuild":
      if not self._cap_pending:
        self._keytestFinish(); self._keytest_stage = "done"; self.ezapp.signalExit()
    if f > 2000 and self._keytest_stage != "done":
      print("[terrainedit keytest] TIMEOUT; FAIL", flush=True)
      self._keytest_results["timeout"] = True
      self.ezapp.signalExit()

  def _keytestFinish(self):
    base = self._caps_by_label.get("keys_baseline")
    gam = self._caps_by_label.get("keys_gamma")
    gam_rb = self._caps_by_label.get("keys_gamma_rebuild")
    r = self._keytest_results
    # G: a measurable luma shift (either direction — the key took effect).
    r["gamma_delta_mean"] = (gam["mean"] - base["mean"]) if (base and gam) else 0.0
    r["gamma_shift"] = bool(base and gam and abs(r["gamma_delta_mean"]) > 0.005)
    # persists across the rebuild: the held HSVG node reports the set value AND the rebuilt
    # frame still shows the shifted look (close to the pre-rebuild gamma frame).
    node_ok = (self._hsvg is not None
               and abs(self._hsvg.gamma - (self._keytest_gamma_set or 0.0)) < 1e-6)
    look_ok = bool(gam and gam_rb and abs(gam_rb["mean"] - gam["mean"]) < 0.05)
    r["gamma_persists_rebuild"] = bool(node_ok and look_ok)
    # E: radiance maps object SWAPS (identity) + non-crash. LIT-after-swap verify is windowed.
    if not self._envmap_paths:
      r["envmap_swapped"] = True
      r["envmap_note"] = "no envmaps on disk; swap untestable (handler ran non-crash)"
    else:
      r["envmap_swapped"] = (self._keytest_rad1 is not None
                             and self._keytest_rad1 is not self._keytest_rad0)
    r["envmap_noncrash"] = bool(self._caps_by_label.get("keys_envmap") is not None)
    # E + tone mapping SURVIVE a second rebuild: the scene data carries the envmap
    # selection (skybox_path), the rebuilt frame matches the post-E frame, and the held
    # HSVG node still reports the set gamma (covers all tone knobs — same node splice).
    env = self._caps_by_label.get("keys_envmap")
    env_rb = self._caps_by_label.get("keys_envmap_rebuild")
    if not self._envmap_paths:
      r["envmap_persists_rebuild"] = True
    else:
      sel_path = self._envmap_paths[self._skybox_index]
      path_ok = (self.runtime.skybox_path == sel_path)
      look_ok2 = bool(env and env_rb and abs(env_rb["mean"] - env["mean"]) < 0.05)
      r["envmap_persists_rebuild"] = bool(path_ok and look_ok2)
    tone_ok2 = (self._hsvg is not None
                and abs(self._hsvg.gamma - (self._keytest_gamma_set or 0.0)) < 1e-6)
    r["tone_persists_rebuild2"] = bool(tone_ok2)
    r["ok"] = bool(r["gamma_shift"] and r["gamma_persists_rebuild"]
                   and r["envmap_swapped"] and r["envmap_noncrash"]
                   and r["envmap_persists_rebuild"] and r["tone_persists_rebuild2"])
    print(f"[terrainedit keytest] gamma_shift={r['gamma_shift']} "
          f"(dmean={r['gamma_delta_mean']:.4f}) persists={r['gamma_persists_rebuild']} "
          f"envmap_swap={r['envmap_swapped']} noncrash={r['envmap_noncrash']} "
          f"envmap_persists={r['envmap_persists_rebuild']} tone_persists2={r['tone_persists_rebuild2']} "
          f"-> {'PASS' if r['ok'] else 'FAIL'}", flush=True)

  def _onUiEvent(self, uievent):
    return None

  ##############################################################################
  # viewport keys — post-fx (E/G/T/H) + envmap + material-mode (M); everything else
  # (camera chords X pan / C dolly / V zoom / Z rotate / F focus, orbit drag) falls
  # THROUGH to the uicam. Post-fx/env act IMMEDIATELY (live; no rebake needed).
  ##############################################################################

  def _on_viewport_event(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed and (uievent.super or uievent.ctrl):
      kc = uievent.keycode
      if kc == ord("Z") and uievent.shift:
        self._doRedo(); return lev2.ui.HandlerResult()
      if kc == ord("Z"):
        self._doUndo(); return lev2.ui.HandlerResult()
      if kc == ord("Y"):
        self._doRedo(); return lev2.ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed and not uievent.super:
      kc = uievent.keycode
      if kc == ord("E"):
        self._cycleEnvmap(); return lev2.ui.HandlerResult()
      if kc == ord("G"):
        self._cycleGamma(); return lev2.ui.HandlerResult()
      if kc == ord("T"):
        self._cycleExposure(); return lev2.ui.HandlerResult()
      if kc == ord("H"):
        self._cycleSaturation(); return lev2.ui.HandlerResult()
      if kc == ord("M"):
        self._cycleMaterialMode(); return lev2.ui.HandlerResult()
    return self.runtime.handle_camera_event(uievent)

  def _cycleGamma(self):
    self._gam_idx = (self._gam_idx + 1) % len(self._gamset)
    val = self._gamset[self._gam_idx]
    if self._hsvg is not None:
      self._hsvg.gamma = val
    print(f"[terrainedit] gamma -> {val:.2f}", flush=True)
    return val

  def _cycleSaturation(self):
    self._sat_idx = (self._sat_idx + 1) % len(self._satset)
    val = self._satset[self._sat_idx]
    if self._hsvg is not None:
      self._hsvg.saturation = val
    print(f"[terrainedit] saturation -> {val:.2f}", flush=True)
    return val

  def _cycleExposure(self):
    self._exp_idx = (self._exp_idx + 1) % len(self._expset)
    val = self._expset[self._exp_idx]
    if self._aces is not None:
      self._aces.exposure = val
    print(f"[terrainedit] ACES exposure -> {val:.2f}", flush=True)
    return val

  def _cycleEnvmap(self):
    if not self._envmap_paths:
      print("[terrainedit] E: no envmaps in <staging>/assetcache/envmaps2/", flush=True)
      return None
    self._skybox_index = (self._skybox_index + 1) % len(self._envmap_paths)
    path_str = self._envmap_paths[self._skybox_index]
    skybox = self._skybox_cache.get(path_str)
    if skybox is None:
      skybox = PbrCommon.requestRadianceMapsAsync(path_str)
      self._skybox_cache[path_str] = skybox
    self.runtime.set_radiance_maps(skybox)     # live-applied + re-applied on every rebuild
    # the DATA carries the selection: every rebuilt scene embeds the chosen envmap in its
    # scene params, so the fresh sim STAGES with the selection instead of re-applying the
    # startup default over the stash (E must survive dflow-edit rebakes).
    self.runtime.skybox_path = path_str
    print(f"[terrainedit] envmap -> {self._envmap_names[self._skybox_index]}", flush=True)
    return skybox

  def _cycleMaterialMode(self):
    self._mat_mode = (self._mat_mode + 1) % len(self._mat_modes)
    mode = self._mat_modes[self._mat_mode]
    print(f"[terrainedit] material mode -> {mode!r}", flush=True)
    self._applyMaterialMode()
    return mode

  def _applyMaterialMode(self):
    """Fire the held mode at the CURRENT sim. Called on [M] and again after every sim
    rebuild. Mode 0 is sent like any other — the LIVE sim may be on a debug mode and
    must be told to return to declared (skipping 0 stuck the cycle on the last mode);
    on a fresh sim it lands as a no-op (already declared)."""
    ctrl = self.runtime.controller
    sysref = self.runtime._sys_ref
    if ctrl is not None and sysref is not None:
      try:
        ctrl.systemNotify(sysref, tokens.SetTerrainMaterialMode, {tokens.mode: self._mat_mode})
      except Exception as e:
        print(f"[terrainedit] material-mode notify skipped: {e}", flush=True)

  ##############################################################################
  # file popups (secondary window + FilesystemBrowser)
  ##############################################################################

  def _openTerrainPopup(self):
    self._filePopup("Open Terrain", ".py", "load", self._doOpenTerrain)

  def _saveDocPopup(self):
    self._filePopup("Save Terrain (.py DSL)", ".py", "save", self._doSaveDoc)

  def _filePopup(self, title, ext, mode, callback):
    popup = self.ezapp.createSecondaryWindow(
        width=800, height=600, x=200, y=150, title=title,
        decorated=True, resizable=True, floating=True)
    uic = popup.ui_context
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, popup.width, popup.height)
    uic.top = root
    root.margin = 4
    browser_item = root.makeChild(
        uiclass=FilesystemBrowser,
        args=["browser", self._home_dir, ext, vec3(0.1, 0.1, 0.1), mode], fill=True)
    browser = browser_item.widget.uservars.filesystem_browser
    browser.onActivate = lambda p: (callback(p), popup.requestClose())
    browser.onCancel = lambda: popup.requestClose()

  def _doOpenTerrain(self, path):
    try:
      self.runtime.load(path)
    except Exception as e:
      print(f"[terrainedit] open failed: {e}", flush=True)
      return
    self._afterDocumentLoad()

  def _doNewTerrain(self):
    """New terrain = the minimal viable document (fbm -> height/normal captures);
    grow it with the outliner add flow."""
    self.runtime.load("new")
    print("[terrainedit] new terrain (minimal viable)", flush=True)
    self._afterDocumentLoad()

  def _afterDocumentLoad(self):
    # a different document: undo history does not span opens
    self._undo.clear()
    self._undo_state = self.runtime.capture_undo_state()
    self._selected_obj = None
    self.propsheet.model = self.prop_model
    self.prop_model.set_object(None)
    self.propsheet.rebuild()
    self.outliner_model.set_document(self.runtime.document)
    self._refreshParamsBinding()      # show/hide "Terrain Parameters" for the new source
    self.outliner.expandAll()
    self._requestRebake()

  def _doSaveDoc(self, path):
    # Save writes a runnable .py DSL asset (the ork.scene.viewer.py / hyperecs consume
    # form). doc-JSON stays INTERNAL ONLY (undo checkpoints) — off the user save surface.
    try:
      out = self.runtime.save_dsl_py(path)
      print(f"[terrainedit] saved .py DSL -> {out}", flush=True)
    except Exception as e:
      print(f"[terrainedit] save failed: {e}", flush=True)
