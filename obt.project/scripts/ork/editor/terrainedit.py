################################################################################
# TerrainEditor — ork.terrain.edit.py (JUL09 S1; E3 C1 node-editor canvas).
#
# A desktop terrain-graph editor: viewport + toolbar + generic GPU node-editor canvas
# + property sheet + rebake loop. Every edit mutates the structured DOCUMENT (owner
# law L2), then re-elaborates + rebakes via TerrainRuntime; the property sheet and the
# node-editor canvas bind the DOCUMENT, never the derived GraphData.
#
# Layout rides the ui.DockSpace substrate: a viewport panel (fill), a left column
# split LEFT @0.35 hosting VerticalPack[Toolbar, NodeEditor], and a property sheet
# split BOTTOM @0.55 of the left column. NOT ECS-coupled — no ecs modules are
# imported. The node-editor canvas REPLACES the earlier document Outliner (E3 C1);
# the generic lev2.ui.Outliner + graphdoc_models stay for other editors. Dock layout
# persists per (app-name, entry-path) via ork.ui.app_state; Shift+L resets it live,
# --reset-layout ignores the saved session.
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
from ork.ui.node_editor import NodeEditor, COL_BG
from ork.ui.dock_layout import save_layout, load_layout, to_json
from ork.ui.dock_editor_glue import EditorDockGlue
from ork.ui.app_state import dock_layout_path, text_input_has_focus

from ork.editor.terrain_runtime import TerrainRuntime, terrain_debug_material_labels
from ork.editor.undo_stack import UndoStack
from ork.editor.terrain_node_model import TerrainNodeGraphModel
from ork.editor.terrain_doc_model import (
    TerrainNodePropertyModel, TerrainParamsPropertyModel)

tokens = CrcStringProxy()

_REBAKE_IDLE_S = 0.25   # settle window: rebake this long after the last edit tick
_SWAP_HOLD_MAX_S = 3.0  # hold-last-frame cap: swap to the fresh scenegraph anyway after this

# default dock layout: viewport (fill) | left column [toolbar + node-editor] split
# LEFT @0.35; property sheet split BOTTOM @0.55 of the left column. Panel titles are
# BOTH the titlebar labels AND the stable persistence ids (name-keyed layout JSON).
_DOCK_VIEWPORT_TITLE = "Viewport"
_DOCK_LEFT_TITLE     = "Terrain"
_DOCK_PROPS_TITLE    = "propsheet"
_DOCK_LEFT_PROP      = 0.35
_DOCK_PROPS_PROP     = 0.55
_DOCK_SPLIT_MARGIN   = 2


class TerrainEditor(ComponentizedApplication):

  def __init__(self, source, *, dsl_class=None, extent_m=None,
               preview_dim=1024, full_dim=4096, chunk=128, dsl_kwargs=None,
               offscreen=False, selftest=False, keytest=False,
               reset_layout=False, layouttest=False, layout_probe=False):
    super().__init__()
    self.runtime = TerrainRuntime(preview_dim=preview_dim, full_dim=full_dim, chunk=chunk)
    self.runtime.load(source, dsl_class=dsl_class, extent_m=extent_m,
                      **(dsl_kwargs or {}))
    self._full_res = False
    self._rebake_pending = False
    self._last_edit_time = 0.0
    self._sgv_swap_pending = False     # hold-last-frame: fresh sg built, rebind deferred
    self._sgv_swap_started = 0.0
    self._sliced_precook = False       # MT3: a sliced re-bake pre-cook is in progress
    # undo/redo (S2): snapshot checkpoints of (doc-JSON, kwargs, display key). Every
    # document/session mutation records one; drag ticks coalesce by edit key.
    self._undo = UndoStack(limit=100)
    self._undo_state = self.runtime.capture_undo_state()
    self._rebuilding = False          # guards the update-thread sim tick during a rebuild
    self._selected_obj = None
    self._home_dir = os.path.expanduser("~")

    # dock substrate (S6 adoption): the DockSpace + its panels are built in
    # _onUiInit. _default_layout captures the canonical default arrangement so a
    # live reset (Shift+L) / --reset-layout can restore it regardless of what a
    # persisted session loaded at startup.
    self.dock = None
    self._default_layout = None
    self._dock_glue = None                     # W5 cross-window DockManager wiring
    self._gpu_ready = False                    # set in _onGpuInit; gates factory GPU wiring
    self._reset_layout = bool(reset_layout)    # --reset-layout: ignore saved session
    self._reset_layout_pending = False         # Shift+L: apply on the next GPU tick
    # session persistence is DISABLED for the ephemeral automated gate modes (selftest /
    # keytest): a save-on-exit/restore-on-start round trip would leave stray session state
    # coupling determinism-sensitive runs. Interactive runs + the explicit layout gates keep it.
    self._persist_layout = not (bool(selftest) or bool(keytest))

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

    # scripted offscreen DOCK-LAYOUT gate: exercise the real editor's DockSpace
    # (default signature, save/scramble/load round-trip, live Shift+L reset,
    # injected tab-drag) and print machine verdicts.
    self._layouttest = bool(layouttest)
    self._layouttest_frame = 0
    self._layouttest_stage = "settle"
    self._layouttest_settle_at = 0
    self._layouttest_results = {}
    # layout_probe: settle, print the AS-CONSTRUCTED dock signature (proves session
    # restore / --reset-layout), then exit WITHOUT persisting (read-only probe).
    self._layout_probe = bool(layout_probe)
    self._layout_probe_frame = 0

    if self._selftest or self._keytest or self._layouttest:
      import tempfile as _tf
      self._selftest_dir = _tf.mkdtemp(prefix="tered_edit_selftest_")

    self.prop_model = TerrainNodePropertyModel(None, on_changed=self._onPropertyEdited)
    self.params_model = TerrainParamsPropertyModel(self.runtime, on_changed=self._onParamsEdited)
    # node-editor canvas (E3 C1) — built in _onUiInit (needs the loaded document); the
    # adapter binds the DOCUMENT through the generic editor. _ne_title is the root crumb.
    self._ne_title = self._deriveTitle(source)
    self.node_model = None
    self.node_editor = None
    self._ne_glue = None
    self._last_ne_rebuild = 0     # perf instrument (ORKID_NE_PERF): idle rebuild delta

    # ECS module init injected BEFORE GPU finalization (ecsedit's mechanism) — the
    # runtime hosts an in-code ECS scene, so the SceneData system-class registry must
    # be populated before build_scene_data()/_start_simulation() run in _onGpuInit.
    # enable_global_events: Shift+L (reset dock layout) is an editor-app-level
    # chord — the global handler observes it regardless of which panel is hovered.
    self.createEzApp(name="terrainedit", pre_init_fns=self._getPreInitFns(),
                     enable_global_events=True,
                     offscreen=(offscreen or self._selftest or self._keytest
                                or self._layouttest or self._layout_probe))

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

    # DockSpace substrate (S6 adoption). A full-bleed (margin 0) transparent
    # container; per-panel insets come from the split margin — byte-parity with the
    # prior DockablePanel + lg.split idiom (viewport fill; left @0.35; propsheet @0.55).
    self.dock = lg.makeChild(fill=True, margin=0,
                             uiclass=lev2.ui.DockSpace, args=["terrain_dock"]).widget
    self.dock.clear = False

    self.viewport_dock = self.dock.addPanel(
        uiclass=lev2.ui.SceneGraphViewport,
        args=["Viewport", vec4(0.1, 0.1, 0.12, 1)], title=_DOCK_VIEWPORT_TITLE)
    self.viewport_dock.titlebar_color = vec4(0.15, 0.2, 0.25, 1)
    self.sgv = self.viewport_dock.child

    self.left_dock = self.dock.split(
        target=self.viewport_dock, placement=tokens.LEFT, proportion=_DOCK_LEFT_PROP,
        margin=_DOCK_SPLIT_MARGIN, uiclass=lev2.ui.VerticalPack, args=[_DOCK_LEFT_TITLE],
        title=_DOCK_LEFT_TITLE)
    self.left_dock.titlebar_color = vec4(0.2, 0.15, 0.2, 1)

    self.left_panel = self.left_dock.child
    self.left_panel.margin = 2
    self.left_panel.item_height = 34

    self._setupToolbar()

    # node-editor canvas (E3 C1) — a PrimCanvas hosting the generic NodeEditor bound to
    # the terrain DOCUMENT adapter, as the left dock's fill widget (was the Outliner).
    self.ne_canvas = self.left_panel.makeChild(uiclass=lev2.ui.PrimCanvas, args=["ne_canvas"])
    self.ne_canvas.bg_color = COL_BG
    self.ne_canvas.draw_background = True
    self.left_panel.fill_widget = self.ne_canvas
    self.node_model = TerrainNodeGraphModel(self, self.runtime)
    self._ne_glue = self.node_model._glue
    self.node_editor = NodeEditor(self.ne_canvas, self.node_model,
                                  title=self._ne_title, orientation="vertical")
    # SSAA (ss=3, set by the NodeEditor ctor) verified on this platform 2026-07-18:
    # the multisurface gate renders primitives crisply through the resolve (owner Mac,
    # canvas non-black, geometry correct). Residuals live in the SSAA slice, not here:
    # bright bg_colors resolve dark, and a SceneGraphViewport at ss>0 still resolves
    # black (filed) - neither affects this dark-bg primitives-only canvas.
    self._ne_glue.node_editor = self.node_editor
    self.node_editor.on_selection_changed = self._onNodeEditorSelect
    self._wireNodeEditorKeys()

    # W5: cross-window DockManager glue. The viewport + node-editor left column are
    # PINNED (no factory) — both host one-shot Context-bound GPU seams (SceneGraphViewport
    # forkDB/scenegraph; NodeEditor+PrimCanvas glyph textures built in gpuInit) that cannot
    # be rebuilt in a foreign window's context. The property sheet has NO GPU seam, so it is
    # the transferable factory panel: the boot below CALLS its factory (one construction path).
    self._dock_glue = EditorDockGlue(self, self.dock, "terrainedit")
    self._dock_glue.register(_DOCK_PROPS_TITLE, _DOCK_PROPS_TITLE, self._buildPropsheetPanel,
                             save_state=self._savePropsheetState,
                             restore_state=self._restorePropsheetState, closeable=False)

    # build the property sheet via its factory (addPanel -> the root leaf), then reproduce
    # the canonical split geometry (propsheet BOTTOM @0.55 of the left column). moveChild's
    # split uses the DockSpace default gap (== _DOCK_SPLIT_MARGIN) and setSplitProportion
    # matches dock_layout's restore path, so the serialized default is byte-identical to the
    # prior inline dock.split(...) construction.
    self.propsheet_dock = self._buildPropsheetPanel(self.dock, self.ezapp)
    self.dock.moveChild(panel=self.propsheet_dock, to=self.left_dock, zone=tokens.BOTTOM)
    self.dock.setSplitProportion(self.left_dock, self.propsheet_dock, _DOCK_PROPS_PROP)

    # capture the canonical default arrangement, then restore a persisted session
    # over it (silent fall-back to default on any mismatch — a loud log, never a crash).
    self._default_layout = save_layout(self.dock)
    self._maybeRestoreSession()
    self.dock.updateLayout()

  ##############################################################################
  # property-sheet factory (the transferable panel) + carry-state
  ##############################################################################

  def _buildPropsheetPanel(self, dock, window):
    """The property-sheet panel's SINGLE construction path (boot + every cross-window
    recreate). Builds + styles a fresh PropertySheet DockPanel in 'dock'; the manager
    stamps its stable id. Post-GPU-init (a transfer/return rebuild) the live bindings are
    re-wired so the fresh sheet is immediately functional; at boot _onGpuInit does that."""
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
    # carry the currently-bound model so a transferred/returned sheet shows the SAME
    # content in its new window (the model object is process-global, not context-bound).
    return {"model": getattr(panel.child, "model", None)}

  @staticmethod
  def _restorePropsheetState(panel, state):
    model = state.get("model") if state else None
    if model is not None:
      panel.child.model = model
      panel.child.rebuild()

  def _wirePropsheet(self):
    """Bind the live property-sheet models + change handler. Called at boot from
    _onGpuInit, and again by the factory when a transfer/return rebuilds the sheet."""
    self.propsheet.model = self.prop_model
    self.propsheet.onPropertyChanged(self._onPropsheetChanged)
    self._refreshParamsBinding()      # bind the Terrain Parameters model to the runtime

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
    specs = [
      ("New", vec4(0.22, 0.22, 0.15, 1), lambda b: self._doNewTerrain()),
      ("Open", vec4(0.15, 0.25, 0.15, 1), lambda b: self._openTerrainPopup()),
      ("Save", vec4(0.15, 0.15, 0.25, 1), lambda b: self._saveDocPopup()),
      ("Bake", vec4(0.22, 0.18, 0.12, 1), lambda b: self._requestRebake()),
      ("Prev", vec4(0.18, 0.18, 0.22, 1), lambda b: self._toggleResolution()),
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

  @staticmethod
  def _deriveTitle(source):
    """Short root-crumb label for the node-editor path bar (the asset stem)."""
    try:
      stem = os.path.splitext(os.path.basename(str(source)))[0]
      return stem or "terrain"
    except Exception:
      return "terrain"

  def _wireNodeEditorKeys(self):
    """Route KEYBOARD events over the node-editor canvas to its key handlers. The UI
    context routes keys to the widget UNDER THE CURSOR (and bubbles up that widget's OWN
    parent chain only), so this is inherently hover-gated: viewport camera chords
    (X/C/V/Z) and the canvas mouse-emulation keys (z/x/c) never leak across — they live
    in disjoint widget subtrees. Cmd/Ctrl+Z(/Y) drives undo/redo while over the canvas."""
    canvas = self.ne_canvas
    ne = self.node_editor
    # Capture the NodeEditor's bound pointer/wheel handler DIRECTLY (it set
    # canvas.onUiEvent = self._onUiEvent in its ctor). canvas.onUiEvent now round-trips
    # (the C++ getter caches the handler), but reading ne._onUiEvent keeps this
    # independent of the canvas re-wrapping the callable.
    inner = ne._onUiEvent

    def _wrapped(ev):
      code = ev.code
      if code == tokens.KEY_DOWN.hashed:
        if ev.super or ev.ctrl:
          kc = ev.keycode
          if kc == ord("Z") and ev.shift:
            self._doRedo()
          elif kc == ord("Z"):
            self._doUndo()
          elif kc == ord("Y"):
            self._doRedo()
          return lev2.ui.HandlerResult()
        ne.handleKeyDown(ev)
        return lev2.ui.HandlerResult()
      if code == tokens.KEY_UP.hashed:
        ne.handleKeyUp(ev)
        return lev2.ui.HandlerResult()
      return inner(ev)

    canvas.onUiEvent = _wrapped

  ##############################################################################
  # dock layout persistence + live reset (Shift+L / --reset-layout)
  ##############################################################################

  def _sessionLayoutPath(self):
    """Per-user, per-app dock-layout slot keyed by (app name, entry-script path)."""
    return dock_layout_path("terrainedit")

  def _maybeRestoreSession(self):
    """Restore a persisted arrangement over the default construction. --reset-layout
    forces the default; a missing / mismatched session silently keeps the default."""
    if self._reset_layout:
      print("[terrainedit] --reset-layout: default dock arrangement", flush=True)
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
      print(f"[terrainedit] restored dock layout <- {path}", flush=True)
    except Exception as e:
      # load_layout validates panel-ids up front and raises BEFORE mutating on a
      # mismatch, so the default construction is intact; re-assert it defensively.
      print(f"[terrainedit] dock layout restore skipped ({e}); using default", flush=True)
      try:
        load_layout(self.dock, self._default_layout)
      except Exception as e2:
        print(f"[terrainedit] default-layout reassert failed: {e2}", flush=True)

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
      print(f"[terrainedit] saved dock layout -> {path}", flush=True)
    except Exception as e:
      print(f"[terrainedit] dock layout save failed: {e}", flush=True)

  def _resetDockLayout(self):
    """Restore the DEFAULT dock arrangement live. Runs OUTSIDE event dispatch (a GPU
    frame tick) so load_layout's moveChild + proportion ops all apply immediately +
    in order; a root updateLayout re-cascade follows so the reset renders this frame."""
    if self.dock is None or self._default_layout is None:
      return
    try:
      load_layout(self.dock, self._default_layout)
      self.dock.updateLayout()
      print("[terrainedit] dock layout reset to default", flush=True)
    except Exception as e:
      print(f"[terrainedit] dock layout reset failed: {e}", flush=True)

  def _onGlobalUiEvent(self, uievent):
    """Editor-app-level chords (observer-only; per-panel dispatch still runs).
    Shift+L resets the dock layout — SUPPRESSED while a text-input widget owns key
    focus (so Shift+L types 'L' in a focused CodeView/LineEdit instead)."""
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
    # exit after reset saves the DEFAULT, not a transient). Skipped for the read-only
    # probe and the ephemeral automated gate modes (no stray/coupling session state).
    if self._persist_layout and not self._layout_probe:
      self._saveSession()
    super().onAppExit()

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

    # node-editor + propsheet wiring. Icons/glyph textures MUST be prebuilt in the GPU-
    # init phase (creating textures during the render callback aborts on Vulkan).
    self.node_editor.uicontext = self.uicontext
    self.node_editor.gpuInit(ctx)
    self._gpu_ready = True             # from here, the propsheet factory re-wires on rebuild
    self._wirePropsheet()             # bind models + change handler on the boot-built sheet

    if self._selftest:
      self._selftest_ctrl0 = self.runtime.controller   # baseline sim (pre-rebuild)

  ##############################################################################
  # selection + edit loop
  ##############################################################################

  def _onNodeEditorSelect(self, model, nid):
    """Canvas selection -> property sheet (mirrors the old outliner onSelect). EMPTY
    selection (a background click, nid=None) binds the Terrain Parameters model — this
    replaces the synthetic 'Terrain Parameters' outliner row. A boundary pill (no
    document object) shows an empty sheet."""
    if nid is None:
      self._selected_obj = None
      self.params_model.refresh()
      self.propsheet.model = self.params_model
      self.propsheet.rebuild()
      return
    obj = model.object_for_nid(nid)
    self._selected_obj = obj
    self.propsheet.model = self.prop_model
    self.prop_model.set_object(obj)          # obj may be None (a pill) -> empty sheet
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
    self._resetNodeEditorToRoot()          # the structure may differ from the current level
    self._ne_glue.fire_changed()           # invalidate the level caches + rebuild the canvas
    self._requestRebake()

  def _doUndo(self):
    label = self._undo.undo()
    print(f"[terrainedit] undo: {label if label else '(nothing to undo)'}", flush=True)

  def _doRedo(self):
    label = self._undo.redo()
    print(f"[terrainedit] redo: {label if label else '(nothing to redo)'}", flush=True)

  def _resetNodeEditorToRoot(self):
    """Pop the node editor back to the root level (used after a document swap —
    undo / open / new — whose structure may not contain the current level's container)."""
    if self.node_editor is None:
      return
    while len(self.node_editor.nav_stack) > 1:
      self.node_editor.up_level()

  def _onPropertyEdited(self, key):
    # model-side hook (fires alongside onPropertyChanged) — refresh the canvas when a
    # STRUCTURAL value (loop count / switch selector) changed (the label + reachability).
    from ork.hypergraph.dflow.terrain.doc import DocLoop, DocSwitch
    if isinstance(self._selected_obj, (DocLoop, DocSwitch)):
      self._ne_glue.fire_changed()

  def _onParamsEdited(self, key):
    # a Terrain-Parameter edit may RE-TRACE the document (a NEW document object) — refresh
    # the canvas so its level caches resolve the live objects. The propsheet stays on the
    # params model (selection unchanged); the rebake is scheduled by _onPropsheetChanged.
    self._ne_glue.fire_changed()

  def _refreshParamsBinding(self):
    """Bind the Terrain Parameters model to the current runtime. Every session has at
    least the session rows (dim / extent); DSL sessions add the ctor kwargs."""
    self.params_model.set_runtime(self.runtime)

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
    # live dock-layout reset (Shift+L) — applied here (render-sequential, outside event
    # dispatch) so the moveChild + proportion re-cascade never races DoRePaintSurface.
    # W5: the two-phase reset FIRST returns every secondary window's panels to main
    # (return-on-close), THEN restores the main default once they have all returned.
    if self._dock_glue is not None:
      self._dock_glue.pump_reset(self._resetDockLayout)
    # scripted DOCK-LAYOUT gate — all dock mutations run here (render-sequential),
    # matching the scenegraph-swap discipline (no race vs DoRePaintSurface).
    if self._layouttest:
      self._layouttestTick()
    # MT3 (JUL13 §2.6/§E5): a re-bake (scene already live) becomes a SLICED pre-cook that
    # advances as budgeted slices across frames — the GPU thread stays live, the OLD
    # scenegraph keeps presenting (hold-last-frame), and the two-phase swap runs UNCHANGED
    # once the cook lands (its materialize then hits capture-currency / warm cache, no
    # hitch). begin_sliced_rebuild declines (-> burst) for initial loads / dim changes.
    if self._rebake_pending and (time.time() - self._last_edit_time) >= _REBAKE_IDLE_S:
      self._rebake_pending = False
      if self.runtime.begin_sliced_rebuild(ctx, dim=self._current_dim()):
        self._sliced_precook = True
      else:
        self._doRebuildSwap(ctx)                                        # burst (pre-MT3 path)
    # while the pre-cook runs, the scheduler slices it at each render beginFrame and the old
    # scenegraph keeps presenting; swap the moment it completes.
    if self._sliced_precook and self.runtime.sliced_rebuild_ready():
      self._sliced_precook = False
      print(f"[terrainedit] MT3 sliced re-bake done: "
            f"{self.runtime.frames_during_last_rebake()} GPU frames presented during cook "
            f"(blocking presents ~0)", flush=True)
      self._doRebuildSwap(ctx)
    if self._sgv_swap_pending:
      timed_out = (time.time() - self._sgv_swap_started) > _SWAP_HOLD_MAX_S
      if self.runtime.display_ready() or timed_out:
        if timed_out:
          print(f"[terrainedit] hold-last-frame: fresh terrain not staged in "
                f"{_SWAP_HOLD_MAX_S:.0f}s — swapping anyway", flush=True)
        self.sgv.scenegraph = self.runtime.scenegraph                  # rebind (render-sequential)
        self._sgv_swap_pending = False

  def _doRebuildSwap(self, ctx):
    """The two-phase swap (Phase A materialize on the GPU thread, Phase B fresh sg + sim).
    After an MT3 pre-cook its bake hits capture-currency / warm cache, so it does not hitch;
    the burst fallback (initial load / dim change) pays the full cook here as before. Shared
    by the MT3-completion path and the burst fallback."""
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
      if self.node_editor is not None:    # surface the failure on the canvas (Fix 3b)
        self.node_editor.show_status(f"rebuild failed: {e}")
    finally:
      self._rebuilding = False

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
    if self._layout_probe:
      self._layoutProbeTick()
    self._nePerfTick(updinfo)

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

  def _nePerfTick(self, updinfo):
    # gate-8 instrument (opt-in ORKID_NE_PERF=1): print the node-editor's Python-side
    # rebuild-count delta per second. When the editor is open and untouched the delta
    # must be 0 (the canvas only rebuilds on view/structure/selection change).
    if self.node_editor is None or not os.environ.get("ORKID_NE_PERF"):
      return
    self._ne_perf_t = getattr(self, "_ne_perf_t", 0.0) + updinfo.deltatime
    if self._ne_perf_t >= 1.0:
      self._ne_perf_t -= 1.0
      rc = self.node_editor.rebuild_count
      print(f"[terrainedit ne-perf] rebuild_count={rc} delta/sec={rc - self._last_ne_rebuild}",
            flush=True)
      self._last_ne_rebuild = rc

  def onGpuPostFrame(self, ctx):
    # ComponentizedApplication.onGpuPostFrame only broadcasts to components — override to
    # capture the settled framebuffer (GPU thread, post-render) for the DISPLAY gate. The
    # readback is ASYNC: issue it, then DRAIN it across subsequent frames (the C++ player's
    # os_snapdrain pattern) — an in-frame cap.wait() deadlocks.
    super().onGpuPostFrame(ctx)
    # W5: realize any pending cross-window tear-outs (createSecondaryWindow is main/GPU-
    # thread work; this hook is the DockManager-prescribed pump point).
    if self._dock_glue is not None:
      self._dock_glue.pump()
    if not (self._selftest or self._keytest or self._layouttest):
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

  ##############################################################################
  # scripted offscreen DOCK-LAYOUT gate (real editor: signature round-trip, live
  # Shift+L reset, injected titlebar drag) — runs on the GPU thread (render-seq).
  ##############################################################################

  def _layouttestTick(self):
    import ork.uitest as U
    self._layouttest_frame += 1
    f = self._layouttest_frame
    st = self._layouttest_stage
    r = self._layouttest_results
    dock = self.dock

    def _sig():
      return dock.layoutSignature()

    if st == "settle" and f >= 40:
      r["sig0"]    = _sig()
      j1           = to_json(save_layout(dock))
      j2           = to_json(save_layout(dock))
      r["json0"]   = j1
      r["deterministic"] = (j1 == j2)
      r["names0"]  = sorted(p.name for p in dock.allPanels())
      r["n0"]      = dock.num_panels
      r["valid0"]  = dock.validateTree()
      print(f"[terrainedit layouttest] default sig={r['sig0']} names={r['names0']} "
            f"n={r['n0']} deterministic={r['deterministic']}", flush=True)
      self._layouttest_stage = "scramble"

    elif st == "scramble":
      # a real reshape: dock the property sheet to the RIGHT of the viewport
      dock.moveChild(panel=self.propsheet_dock, to=self.viewport_dock, zone=tokens.RIGHT)
      r["sig_scrambled"] = _sig()
      r["scrambled_json"] = to_json(save_layout(dock))
      r["scramble_changed"] = (r["sig_scrambled"] != r["sig0"])
      # emit the default + a scrambled arrangement for the disk-session restore probe
      print(f"DEFAULT_JSON={r['json0']}", flush=True)
      print(f"SCRAMBLED_JSON={r['scrambled_json']}", flush=True)
      print(f"SCRAMBLED_SIG={r['sig_scrambled']}", flush=True)
      self._layouttest_stage = "load"

    elif st == "load":
      load_layout(dock, r["json0"])
      dock.updateLayout()
      r["sig_loaded"]   = _sig()
      r["json_loaded"]  = to_json(save_layout(dock))
      r["roundtrip_sig_ok"]  = (r["sig_loaded"] == r["sig0"])
      r["roundtrip_json_ok"] = (r["json_loaded"] == r["json0"])
      r["valid_loaded"] = dock.validateTree()
      print(f"[terrainedit layouttest] load round-trip: sig_ok={r['roundtrip_sig_ok']} "
            f"json_ok={r['roundtrip_json_ok']}", flush=True)
      self._layouttest_stage = "shiftl_scramble"

    elif st == "shiftl_scramble":
      dock.moveChild(panel=self.propsheet_dock, to=self.viewport_dock, zone=tokens.BOTTOM)
      r["sig_preshiftl"] = _sig()
      # inject the real editor-app-level chord; the global handler sets the pending
      # flag, and the NEXT _onGpuUpdate applies the reset (render-sequential).
      U.key_chord(self.ezapp, ord("L"), mods={"shift": True})
      self._layouttest_settle_at = f + 12
      self._layouttest_stage = "shiftl_wait"

    elif st == "shiftl_wait":
      if f >= self._layouttest_settle_at:
        r["sig_postshiftl"] = _sig()
        r["shiftl_reset_ok"] = (r["sig_postshiftl"] == r["sig0"]
                                and r["sig_preshiftl"] != r["sig0"])
        print(f"[terrainedit layouttest] Shift+L reset: ok={r['shiftl_reset_ok']}", flush=True)
        self._layouttest_stage = "tabdrag"

    elif st == "tabdrag":
      # injected drag on a REAL editor panel: grab the left panel's titlebar and drop
      # it on the viewport's RIGHT zone (the DockPanel drag-source -> moveChild path).
      # DockPanel .x/.y are tab-LOCAL; localToRoot maps them to window space for the drag.
      lp = self.left_dock
      x0, y0 = lp.localToRoot(24, lp.titlebar_height // 2)
      vp = self.viewport_dock
      x1, y1 = vp.localToRoot(vp.width - 12, vp.height // 2)
      r["sig_pre_drag"] = _sig()
      top = self.ezapp.topWidget
      U.drag(self.ezapp, x0, y0, x1, y1, top.width, top.height, steps=10)
      self._layouttest_settle_at = f + 6
      self._layouttest_stage = "tabdrag_wait"

    elif st == "tabdrag_wait":
      if f >= self._layouttest_settle_at:
        r["sig_post_drag"] = _sig()
        r["drag_moved"] = (r["sig_post_drag"] != r["sig_pre_drag"])
        r["valid_post_drag"] = dock.validateTree()
        print(f"[terrainedit layouttest] injected titlebar drag: moved={r['drag_moved']} "
              f"valid={r['valid_post_drag']}", flush=True)
        # settle back to default for a clean liveness capture
        self._resetDockLayout()
        self._cap_request("layouttest_default")
        self._layouttest_stage = "wait_cap"

    elif st == "wait_cap":
      if not self._cap_pending:
        self._layouttestFinish()
        self._layouttest_stage = "done"
        self.ezapp.signalExit()

    if f > 2000 and self._layouttest_stage != "done":
      print("[terrainedit layouttest] TIMEOUT; FAIL", flush=True)
      r["timeout"] = True
      self.ezapp.signalExit()

  def _layouttestFinish(self):
    r = self._layouttest_results
    cap = self._caps_by_label.get("layouttest_default")
    r["liveness_lit"] = bool(cap and cap["lit"])
    r["names_ok"] = (r.get("names0") == sorted([_DOCK_VIEWPORT_TITLE, _DOCK_LEFT_TITLE,
                                                _DOCK_PROPS_TITLE]))
    r["ok"] = bool(r.get("deterministic") and r.get("valid0")
                   and r.get("names_ok") and r.get("n0") == 3
                   and r.get("scramble_changed")
                   and r.get("roundtrip_sig_ok") and r.get("roundtrip_json_ok")
                   and r.get("valid_loaded")
                   and r.get("shiftl_reset_ok")
                   and r.get("drag_moved") and r.get("valid_post_drag")
                   and r.get("liveness_lit"))
    print(f"[terrainedit layouttest] deterministic={r.get('deterministic')} "
          f"names_ok={r.get('names_ok')} n0={r.get('n0')} "
          f"scramble_changed={r.get('scramble_changed')} "
          f"roundtrip=({r.get('roundtrip_sig_ok')},{r.get('roundtrip_json_ok')}) "
          f"shiftl_reset_ok={r.get('shiftl_reset_ok')} "
          f"drag_moved={r.get('drag_moved')} liveness_lit={r.get('liveness_lit')} "
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
    grow it with the canvas add flow (Tab after a selected node)."""
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
    self._refreshParamsBinding()      # rebind Terrain Parameters for the new source
    self._resetNodeEditorToRoot()
    self._ne_glue.fire_changed()      # rebuild the canvas from the freshly-loaded document
    self._requestRebake()

  def _doSaveDoc(self, path):
    # Save writes a runnable .py DSL asset (the ork.scene.viewer.py / hyperecs consume
    # form). doc-JSON stays INTERNAL ONLY (undo checkpoints) — off the user save surface.
    try:
      out = self.runtime.save_dsl_py(path)
      print(f"[terrainedit] saved .py DSL -> {out}", flush=True)
    except Exception as e:
      print(f"[terrainedit] save failed: {e}", flush=True)
