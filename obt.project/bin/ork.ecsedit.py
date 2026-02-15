#!/usr/bin/env ork.python

################################################################################
# ECS Scene Editor
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import os, sys, argparse
from orkengine.core import vec2, vec3, vec4, quat, CrcStringProxy, Transform, lev2_pyexdir
from orkengine import lev2
from orkengine import ecs
from ork.app.application import ComponentizedApplication
from ork.app.frame_profiler import FrameProfilerComponent
from ork.ui import standard_icons
from ork.editor.ecs_outliner_model import EcsOutlinerModel
from ork.ecs import EcsRuntime

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData

################################################################################

parser = argparse.ArgumentParser(description="ECS Scene Editor")
parser.add_argument("--scene", "-s", type=str, help="Scene file to load on startup (.json)")
args = parser.parse_args()

################################################################################

class EcsEditor(ComponentizedApplication):

  EDIT = 0
  PLAYING = 1
  PAUSED = 2

  def __init__(self):
    super().__init__()

    self.profiler = self.addComponent("profiler", FrameProfilerComponent)

    # Shared ECS runtime (camera, scenegraph, simulation lifecycle)
    self.runtime = EcsRuntime()

    # Editor state
    self._mode = self.EDIT
    self._selected_key = ""
    self._selected_object = None  # archetype, spawner, or system

    # Deferred rebuild flag — set by callbacks, consumed in _onUpdate
    self._needs_rebuild = False

    # Will be set during init
    self.outliner_model = None
    self.grid_node = None
    self.gizmo_node = None

    # Create app with ECS module init injected before finalization
    self.createEzApp( name="OrkidEcsEditor", width=1440, height=900, pre_init_fns=[ecs.ecsInitCallback])

  @property
  def scene_data(self):
    return self.runtime.scene_data

  @scene_data.setter
  def scene_data(self, value):
    self.runtime.scene_data = value

  ##############################################################################
  # UI Setup
  ##############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.15, 0.15, 0.15, 1)
    lg.clearColorGuide = vec4(0.4, 0.4, 0.2, 1)

    # Viewport dock (fills remaining area)
    viewport_dock_item = lg.makeChild(
      fill=True, margin=2,
      uiclass=lev2.ui.DockablePanel, args=["viewport_dock"])
    self.viewport_dock = viewport_dock_item.widget
    self.viewport_dock.titlebar_color = vec4(0.15, 0.2, 0.25, 1)
    self.sgv = self.viewport_dock.createChild(
      uiclass=lev2.ui.SceneGraphViewport,
      args=["Viewport", vec4(0.1, 0.1, 0.12, 1)])

    # Left dock (split from viewport)
    left_dock_item = lg.split(
      layout=viewport_dock_item.layout,
      proportion=0.35, placement=tokens.LEFT, margin=2,
      uiclass=lev2.ui.DockablePanel, args=["left_dock"])
    self.left_dock = left_dock_item.widget
    self.left_dock.titlebar_color = vec4(0.2, 0.15, 0.2, 1)

    # Left panel contents
    self.left_panel = self.left_dock.createChild(
      uiclass=lev2.ui.VerticalPack, args=["ECS Editor"])
    self.left_panel.margin = 2
    self.left_panel.item_height = 36

    # Toolbar
    self._setupToolbar()

    # Pick debug
    self._setupPickDebug()

    # Outliner
    self.outliner = self.left_panel.makeChild(
      uiclass=lev2.ui.Outliner, args=["outliner"])
    self.left_panel.fill_widget = self.outliner
    self.outliner.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.outliner.item_height = 22

    # Property sheet dock (split from left dock, bottom portion)
    propsheet_dock_item = lg.split(
      layout=left_dock_item.layout,
      proportion=0.4, placement=tokens.BOTTOM, margin=2,
      uiclass=lev2.ui.DockablePanel, args=["propsheet_dock"])
    self.propsheet_dock = propsheet_dock_item.widget
    self.propsheet_dock.titlebar_color = vec4(0.2, 0.2, 0.15, 1)

    self._setupPropertySheet()

  ##############################################################################
  # Toolbar
  ##############################################################################

  def _setupToolbar(self):
    self.toolbar = self.left_panel.makeChild(
      uiclass=lev2.ui.Toolbar, args=["toolbar"])
    self.toolbar.bgcolor = vec4(0.18, 0.18, 0.18, 1)
    self.toolbar.button_hover_color = vec4(0.3, 0.3, 0.35, 1)
    self.toolbar.button_pressed_color = vec4(0.25, 0.45, 0.65, 1)
    self.toolbar.button_toggled_color = vec4(0.35, 0.55, 0.75, 1)
    self.toolbar.separator_color = vec4(0.35, 0.35, 0.35, 1)
    self.toolbar.icon_size = 20
    self.toolbar.button_padding = 4
    self.toolbar.item_spacing = 3
    self.toolbar.edge_padding = 4
    self.toolbar.show_tooltips = True
    self.toolbar.tooltip_delay_ms = 400

    icon_size = 20

    # File operations
    self.btn_new = self.toolbar.addButton(
      "new", standard_icons.get('new', icon_size, icon_size), "New Scene")
    self.btn_load = self.toolbar.addButton(
      "open", standard_icons.get('open', icon_size, icon_size), "Load Scene")
    self.btn_save = self.toolbar.addButton(
      "save", standard_icons.get('save', icon_size, icon_size), "Save Scene")

    self.toolbar.addSeparator()

    # Transport controls
    self.btn_play = self.toolbar.addButton(
      "play", standard_icons.get('play', icon_size, icon_size), "Play")
    self.btn_play.toggle_mode = True

    self.btn_pause = self.toolbar.addButton(
      "pause", standard_icons.get('pause', icon_size, icon_size), "Pause")
    self.btn_pause.toggle_mode = True
    self.btn_pause.enabled = False

    self.btn_stop = self.toolbar.addButton(
      "stop", standard_icons.get('stop', icon_size, icon_size), "Stop")
    self.btn_stop.enabled = False

    self.toolbar.addSeparator()

    # Manip mode buttons
    self.btn_translate = self.toolbar.addButton(
      "translate", standard_icons.get('translate', icon_size, icon_size), "Translate (T)")
    self.btn_translate.toggle_mode = True
    self.btn_rotate = self.toolbar.addButton(
      "rotate", standard_icons.get('rotate', icon_size, icon_size), "Rotate (R)")
    self.btn_rotate.toggle_mode = True
    self.btn_scale = self.toolbar.addButton(
      "scale", standard_icons.get('scale', icon_size, icon_size), "Scale (S)")
    self.btn_scale.toggle_mode = True

  ##############################################################################
  # Pick Debug
  ##############################################################################

  def _setupPickDebug(self):
    self.pick_collapsable = self.left_panel.makeChild(
      uiclass=lev2.ui.Collapsable, args=["Pick Debug"])
    self.pick_collapsable.header_height = 24
    self.pick_collapsable.header_bg_color = vec4(0.2, 0.2, 0.25, 1)
    self.pick_collapsable.expanded = False

    self.pick_hpack = lev2.ui.HorizontalPack.wfactory(["pick_hpack"])
    self.pick_hpack.margin = 2
    self.pick_hpack.uniform = True
    self.pick_hpack.fixed_height = 192
    self.pick_hpack.bg_color = vec4(0.1, 0.1, 0.1, 1)
    self.pick_collapsable.setChild(self.pick_hpack)

    imgbg = vec4(0.15, 0.15, 0.15, 1)
    self.pick_views = []
    for name in ["pick_id", "pick_pos", "pick_nrm"]:
      iv = self.pick_hpack.makeChild(uiclass=lev2.ui.ImageView, args=[name, imgbg])
      iv.maintain_aspect_ratio = True
      iv.flip_x = True
      iv.flip_y = True
      iv.crosshair_enabled = False
      self.pick_views.append(iv)

  ##############################################################################
  # Property Sheet
  ##############################################################################

  def _setupPropertySheet(self):
    self.propsheet = self.propsheet_dock.createChild(
      uiclass=lev2.ui.PropertySheet, args=["propsheet"])
    self.propsheet.row_height = 24
    self.propsheet.label_width = 120
    self.refl_model = lev2.ui.ReflectionPropertySheetModel()
    self.propsheet.model = self.refl_model
    self.propsheet.onPropertyChanged(self._onPropertyChanged)
    self.propsheet.onRequestCustomEditor(self._onRequestCustomEditor)
    self._curve_editor = None

  def _onPropertyChanged(self, key, value):
    """Called when any property is edited in the property sheet."""
    if "userparams/" in key:
      # SceneGraphSystemData userparam changed — apply live (no rebuild needed)
      param_name = key.split("/")[-1]
      if self.runtime.scenegraph:
        self.runtime.scenegraph.applyRuntimeParams({param_name: value})
    elif "/assetpath" in key:
      # Asset path changed — recreate simulation to reload drawables
      self._requestRebuild()

  def _onRequestCustomEditor(self, key, editor_id):
    if editor_id == "transformcurveeditor":
      self._openTransformCurveEditor()

  def _openTransformCurveEditor(self):
    if self._selected_object is None:
      return
    curve = getattr(self._selected_object, 'curve', None)
    if curve is None:
      return

    # Close existing editor if open
    if self._curve_editor is not None:
      self._closeTransformCurveEditor()

    top = self.ezapp.topLayoutGroup
    h = int(top.height * 0.25)
    y = top.height - h

    editor = lev2.ui.TransformCurveEditor.wfactory(["curve_editor", curve])
    editor.onClose = lambda: self._closeTransformCurveEditor()
    editor.onCurveChanged = lambda: self._requestRebuild()
    self._curve_editor = editor

    self.uicontext.pushOverlay(editor, 0, y, top.width, h, dismiss_on_click_outside=False)

  def _closeTransformCurveEditor(self):
    self.uicontext.popOverlay()
    self._curve_editor = None

  ##############################################################################
  # GPU Init — one-time setup (reusable objects + viewport bindings)
  ##############################################################################

  def _onGpuInit(self, ctx):
    # Theme
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    self.uicontext.theme_engine = lev2.ui.ThemeEngine(self.custom_db)

    # Manipulator (reusable across scenegraphs)
    self.manip_controller = lev2.ManipController()
    self.manip_interface = None
    self.manip_controller.mode = lev2.ManipMode.TRANSLATE
    self.manip_enabled = False

    # Reusable drawable data (nodes are per-scenegraph, data is shared)
    self.grid_data = createGridData()
    self.gizmo_data = lev2.ManipGizmoDrawableData()
    self.gizmo_data.controller = self.manip_controller
    self.gizmo_drawable = self.gizmo_data.createDrawable()

    # Camera (reusable — persists across scenegraph rebuilds)
    self.runtime.setup_camera()
    self.runtime.uicam.distance = 1

    # Viewport one-time bindings
    self.sgv.cameraName = "spawncam"
    self.sgv.camera_evhandler = lambda ev: self._onCameraEvent(ev)
    self.sgv.bindManipController(self.manip_controller)
    self.sgv.forkDB()

    # App-level key shortcuts (Phase 1 — before widget routing)
    self.uicontext.app_preview_handler = lambda ev: self._onAppKeyShortcut(ev)

    # Pick debug visualization material
    self.pickid_viz_mtl = lev2.FreestyleMaterial()
    self.pickid_viz_mtl.gpuInit(ctx, "orkshader://ui_pickid_viz")
    permu = lev2.FxPipelinePermutation()
    permu.technique = self.pickid_viz_mtl.shader.technique("pickid_viz")
    self.pickid_viz_pipeline = self.pickid_viz_mtl.fxcache.findPipeline(permu)
    self.pickid_viz_pipeline.sharedMaterial = self.pickid_viz_mtl
    self.pickid_viz_pipeline.bindParam(
      self.pickid_viz_mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    self.p_colormap = self.pickid_viz_mtl.param("ColorMap")
    self.p_nrmmap = self.pickid_viz_mtl.param("NrmMap")
    self.pick_views[0].pipeline = self.pickid_viz_pipeline

    # Outliner model
    self.outliner_model = EcsOutlinerModel(self)
    self.outliner.model = self.outliner_model
    self.outliner.expandAll()

    # Wire outliner callbacks
    self.outliner.onSelect(self._onOutlinerSelect)
    self.outliner.onRename(self._onOutlinerRename)
    self.outliner.onDelete(self._onOutlinerDelete)
    self.outliner.onAdd(self._onOutlinerAdd)
    self.outliner.onShiftEnter(self._onOutlinerShiftEnter)

    # Wire toolbar callbacks
    self.btn_new.onPressed(self._onNew)
    self.btn_load.onPressed(self._onLoad)
    self.btn_save.onPressed(self._onSave)
    self.btn_play.onToggled(self._onPlayToggled)
    self.btn_pause.onToggled(self._onPauseToggled)
    self.btn_stop.onPressed(self._onStop)
    self.btn_translate.onToggled(lambda t: self._onManipButton("translate", t))
    self.btn_rotate.onToggled(lambda t: self._onManipButton("rotate", t))
    self.btn_scale.onToggled(lambda t: self._onManipButton("scale", t))

    # Create initial edit simulation (creates fresh scenegraph + binds to viewport)
    self._createEditSimulation()

    # Load initial scene if provided
    if args.scene and os.path.exists(args.scene):
      self._loadScene(args.scene)

    print("ECS Editor Ready")

  ##############################################################################
  # Scenegraph factory — fresh scenegraph with editor overlays
  ##############################################################################

  def _createScenegraphWithOverlays(self):
    """Create a fresh scenegraph with grid and gizmo overlay nodes.

    Reuses grid_data and gizmo_drawable (created once in _onGpuInit).
    Returns (scenegraph, layer, grid_node, gizmo_node).
    """
    sg, layer = self.runtime.create_scenegraph(enable_pick=True)

    grid_node = layer.createDrawableNodeFromData("grid", self.grid_data)
    grid_node.sortkey = 1
    grid_node.pickable = False

    gizmo_node = sg.createDrawableNodeOnLayers(
      [layer], "manip-gizmo", self.gizmo_drawable)
    gizmo_node.sortkey = 999
    gizmo_node.pickable = False
    gizmo_node.enabled = self.manip_enabled

    return sg, layer, grid_node, gizmo_node

  ##############################################################################
  # Edit Simulation Lifecycle
  ##############################################################################

  def _createEditSimulation(self):
    """Create (or recreate) the edit-mode ECS simulation with a fresh scenegraph."""
    self._destroyEditSimulation()

    # Fresh scenegraph with editor overlays
    sg, layer, self.grid_node, self.gizmo_node = \
      self._createScenegraphWithOverlays()

    self.runtime.stage_simulation()
    self.runtime.bind_to_viewport(self.sgv)

  def _requestRebuild(self):
    """Request a deferred edit-simulation rebuild (consumed in _onUpdate)."""
    if self._mode == self.EDIT:
      self._needs_rebuild = True

  def _destroyEditSimulation(self):
    """Tear down the edit-mode simulation."""
    self.runtime.destroy_simulation()
    if hasattr(self, 'sgv') and self.sgv:
      self.sgv.onPreRender = None

  ##############################################################################
  # Outliner callbacks
  ##############################################################################

  def _onOutlinerSelect(self, key):
    self._selected_key = key
    parts = key.split("/") if key else []
    self._selected_object = None

    # Disable manipulator when selection changes
    self._enableManip(False)
    self.manip_interface = None

    if len(parts) < 2:
      self.refl_model.clearKeyOverrides()
      self.refl_model.object = None
      self.propsheet.rebuild()
      return

    category = parts[0]
    name = parts[1]

    # Clear key overrides for non-spawner selections
    if category != "Spawners":
      self.refl_model.clearKeyOverrides()

    if category == "Archetypes":
      arch = self.outliner_model._findArchetype(name)
      if arch:
        if len(parts) == 3:
          comp_name = parts[2]
          for c in arch.components:
            if c.className == comp_name:
              self._selected_object = c
              break
        else:
          self._selected_object = arch
    elif category == "Spawners":
      sp = self.outliner_model._findSpawner(name)
      if sp:
        self._selected_object = sp
        # Register archetype dropdown override
        self.refl_model.clearKeyOverrides()
        self.refl_model.addKeyOverride(
            "Archetype",
            lev2.ui.PropertyType.String,
            lambda: sp.archetype.name if sp.archetype else "(none)",
            lambda val: self._setSpawnerArchetype(sp, val),
            lambda: [a.name for a in self.scene_data.archetypes])
        # Enable manipulator on spawner's transform
        xform = sp.transform
        self.manip_interface = lev2.DecompTransformManipulator(xform)
        self._enableManip(True)
    elif category == "Systems":
      for s in self.scene_data.systemDatas:
        if s.className == name:
          self._selected_object = s
          break

    self.refl_model.object = self._selected_object
    self.propsheet.rebuild()
    self.propsheet.expandAll()

  def _setSpawnerArchetype(self, sp, name):
    for arch in self.scene_data.archetypes:
      if arch.name == name:
        sp.archetype = arch
        self._requestRebuild()
        return

  def _onOutlinerRename(self, old_key, new_name):
    self.outliner_model.renameItem(old_key, new_name)

  def _onOutlinerDelete(self, key):
    self.outliner_model.removeItem(key)
    self.refl_model.object = None
    self.propsheet.rebuild()
    self._requestRebuild()

  def _onOutlinerAdd(self, key):
    pass  # Model handles creation via factories

  def _onOutlinerShiftEnter(self, key):
    """Shift+Enter on an archetype opens the add-component dropdown."""
    parts = key.split("/") if key else []
    if len(parts) >= 2 and parts[0] == "Archetypes":
      arch = self.outliner_model._findArchetype(parts[1])
      if arch:
        self._showAddComponentDropdown(arch)
        return
    # For other items, fall through to default add behavior
    if self.outliner_model.allow_add:
      factories = self.outliner_model.getFactories(key)
      if factories:
        self.outliner.startAdding(key)

  def _showAddComponentDropdown(self, arch):
    """Show dropdown to add a component to this archetype."""
    from ork.editor.ecs_outliner_model import COMPONENT_TYPES
    existing = {c.className for c in arch.components}
    available = [ct for ct in COMPONENT_TYPES if ct not in existing]
    if not available:
      return

    paths = [f"/{ct}" for ct in available]
    rx, ry = self.outliner.localToRoot(0, 0)
    lev2.ui.DropdownMenu.show(
      context=self.uicontext,
      paths=paths,
      x=rx, y=ry,
      on_selected=lambda val: self._onComponentSelected(arch, val))

  def _onComponentSelected(self, arch, value):
    comp_name = value.lstrip("/")
    arch.declareComponent(comp_name)
    self.outliner_model.notifyModelReset()
    self.outliner.expandAll()
    print(f"Added component: {comp_name}")

  ##############################################################################
  # File I/O
  ##############################################################################

  def _onNew(self):
    if self._mode != self.EDIT:
      return
    self.scene_data = ecs.SceneData()
    self._selected_object = None
    self.refl_model.object = None
    self.propsheet.rebuild()
    self._createEditSimulation()
    self.outliner_model.notifyModelReset()
    self.outliner.expandAll()
    print("New scene created")

  def _onLoad(self):
    if self._mode != self.EDIT:
      return
    from ork.ui.filesystem_browser import FilesystemBrowser
    home = os.path.expanduser("~")
    popup = self.ezapp.createSecondaryWindow(
      width=800, height=600, x=200, y=150,
      title="Load ECS Scene", decorated=True, resizable=True, floating=True)
    uic = popup.ui_context
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, popup.width, popup.height)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["browser", home, ".json", vec3(0.1, 0.1, 0.1), "load"],
      fill=True)
    browser = browser_item.widget.uservars.filesystem_browser
    browser.onActivate = lambda p: (self._loadScene(p), popup.requestClose())
    browser.onCancel = lambda: popup.requestClose()

  def _onSave(self):
    if self._mode != self.EDIT:
      return
    from ork.ui.filesystem_browser import FilesystemBrowser
    home = os.path.expanduser("~")
    popup = self.ezapp.createSecondaryWindow(
      width=800, height=600, x=200, y=150,
      title="Save ECS Scene", decorated=True, resizable=True, floating=True)
    uic = popup.ui_context
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, popup.width, popup.height)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["browser", home, ".json", vec3(0.1, 0.1, 0.1), "save"],
      fill=True)
    browser = browser_item.widget.uservars.filesystem_browser
    browser.onActivate = lambda p: (self._saveScene(p), popup.requestClose())
    browser.onCancel = lambda: popup.requestClose()

  def _loadScene(self, path):
    if self.runtime.load_scene(path):
      self._selected_object = None
      self.refl_model.object = None
      self.propsheet.rebuild()
      self._createEditSimulation()
      self.outliner_model.notifyModelReset()
      self.outliner.expandAll()
      print(f"Loaded: {path}")

  def _saveScene(self, path):
    try:
      if not path.endswith(".json"):
        path += ".json"
      json_str = self.scene_data.serializeJson()
      with open(path, "w") as f:
        f.write(json_str)
      print(f"Saved: {path}")
    except Exception as e:
      print(f"Save failed: {e}")

  ##############################################################################
  # Transport controls
  ##############################################################################

  def _onPlayToggled(self, toggled):
    if toggled:
      self._startPlay()
    else:
      self._stopPlay()

  def _onPauseToggled(self, toggled):
    if toggled:
      self._pausePlay()
    else:
      self._resumePlay()

  def _onStop(self):
    self._stopPlay()

  def _startPlay(self):
    if self._mode != self.EDIT:
      return
    self._enableManip(False)
    self._syncManipButtons()
    try:
      # Tear down edit simulation
      self._destroyEditSimulation()

      # Fresh scenegraph for play mode with editor overlays
      sg, layer, self.grid_node, self.gizmo_node = \
        self._createScenegraphWithOverlays()

      self.runtime.start_simulation()
      self.runtime.bind_to_viewport(self.sgv)

      self._mode = self.PLAYING
      self._updateTransportUI()
      print("Simulation started")
    except Exception as e:
      print(f"Play failed: {e}")
      self._mode = self.EDIT
      self._createEditSimulation()
      self._updateTransportUI()

  def _pausePlay(self):
    if self._mode != self.PLAYING:
      return
    self.runtime.controller.stopSimulation()
    self._mode = self.PAUSED
    self._updateTransportUI()
    print("Simulation paused")

  def _resumePlay(self):
    if self._mode != self.PAUSED:
      return
    self.runtime.controller.startSimulation()
    self._mode = self.PLAYING
    self._updateTransportUI()
    print("Simulation resumed")

  def _stopPlay(self):
    if self._mode == self.EDIT:
      return
    try:
      self.runtime.destroy_simulation()
      self.sgv.onPreRender = None

      # Recreate edit simulation
      self._createEditSimulation()

      # Re-enable manip if a spawner was selected
      if self._selected_object is not None and hasattr(self._selected_object, 'transform'):
        xform = self._selected_object.transform
        self.manip_interface = lev2.DecompTransformManipulator(xform)
        self._enableManip(True)
        self._syncManipButtons()

      self._mode = self.EDIT
      self._updateTransportUI()
      print("Simulation stopped")
    except Exception as e:
      print(f"Stop failed: {e}")
      self._mode = self.EDIT
      self.runtime.destroy_simulation()
      self._createEditSimulation()
      self._updateTransportUI()

  def _updateTransportUI(self):
    if self._mode == self.EDIT:
      self.btn_play.toggled = False
      self.btn_play.enabled = True
      self.btn_pause.toggled = False
      self.btn_pause.enabled = False
      self.btn_stop.enabled = False
    elif self._mode == self.PLAYING:
      self.btn_play.toggled = True
      self.btn_play.enabled = False
      self.btn_pause.enabled = True
      self.btn_pause.toggled = False
      self.btn_stop.enabled = True
    elif self._mode == self.PAUSED:
      self.btn_play.enabled = False
      self.btn_pause.toggled = True
      self.btn_stop.enabled = True

  ##############################################################################
  # Manipulator
  ##############################################################################

  def _enableManip(self, enable):
    self.manip_enabled = enable
    if self.gizmo_node:
      self.gizmo_node.enabled = enable
    if enable and self.manip_interface:
      self.manip_controller.target = self.manip_interface
    else:
      self.manip_controller.target = None

  def _syncManipButtons(self):
    """Sync toolbar toggle buttons to current manip state."""
    if self.manip_enabled:
      mode = self.manip_controller.mode
      self.btn_translate.toggled = (mode == lev2.ManipMode.TRANSLATE)
      self.btn_rotate.toggled = (mode == lev2.ManipMode.ROTATE)
      self.btn_scale.toggled = (mode == lev2.ManipMode.SCALE)
    else:
      self.btn_translate.toggled = False
      self.btn_rotate.toggled = False
      self.btn_scale.toggled = False

  def _onManipButton(self, mode_name, toggled):
    modes = {"translate": lev2.ManipMode.TRANSLATE,
             "rotate": lev2.ManipMode.ROTATE,
             "scale": lev2.ManipMode.SCALE}
    buttons = {"translate": self.btn_translate,
               "rotate": self.btn_rotate,
               "scale": self.btn_scale}
    if toggled and self.manip_interface:
      self.manip_controller.mode = modes[mode_name]
      self._enableManip(True)
      for name, btn in buttons.items():
        if name != mode_name:
          btn.toggled = False
    elif not toggled:
      any_on = any(b.toggled for b in buttons.values())
      if not any_on:
        self._enableManip(False)

  ##############################################################################
  # Camera event handling
  ##############################################################################

  def _onCameraEvent(self, uievent):
    # Picking on mouse click
    if uievent.code == tokens.PUSH.hashed:
      print(f"PUSH event: shift={uievent.shift} ctrl={uievent.ctrl} alt={uievent.alt}")
      if not uievent.shift and not uievent.ctrl and not uievent.alt:
        hovering_gizmo = self.manip_enabled and self.manip_controller.hoveredAxis != lev2.ManipAxis.NONE
        print(f"  manip_enabled={self.manip_enabled} hovering_gizmo={hovering_gizmo}")
        if not hovering_gizmo:
          vp_x = self.viewport_dock.x + self.sgv.x
          vp_y = self.viewport_dock.y + self.sgv.y
          local_x, local_y = uievent.x - vp_x, uievent.y - vp_y
          in_bounds = 0 <= local_x < self.sgv.width and 0 <= local_y < self.sgv.height
          print(f"  local=({local_x},{local_y}) sgv_size=({self.sgv.width},{self.sgv.height}) in_bounds={in_bounds}")
          if in_bounds:
            gizmo_was = self.gizmo_node.enabled
            self.gizmo_node.enabled = False
            print(f"  calling pickWithScreenCoord...")
            self.runtime.scenegraph.pickWithScreenCoord(
              self.runtime.camera, vec2(local_x, local_y),
              0, 0, self.sgv.width, self.sgv.height,
              self._onPickResult)
            self.gizmo_node.enabled = gizmo_was

    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode
      if kc == 256:  # ESC
        if self.manip_enabled:
          self._enableManip(False)
          self._syncManipButtons()
        return lev2.ui.HandlerResult()
      elif kc == ord("T"):
        if self.manip_interface:
          self._enableManip(True)
          if self.manip_controller.mode == lev2.ManipMode.TRANSLATE:
            self.manip_controller.space = (
              lev2.ManipSpace.WORLD
              if self.manip_controller.space == lev2.ManipSpace.LOCAL
              else lev2.ManipSpace.LOCAL)
          else:
            self.manip_controller.mode = lev2.ManipMode.TRANSLATE
          self._syncManipButtons()
        return lev2.ui.HandlerResult()
      elif kc == ord("R"):
        if self.manip_interface:
          self._enableManip(True)
          self.manip_controller.mode = lev2.ManipMode.ROTATE
          self._syncManipButtons()
        return lev2.ui.HandlerResult()
      elif kc == ord("S"):
        if self.manip_interface:
          self._enableManip(True)
          self.manip_controller.mode = lev2.ManipMode.SCALE
          self._syncManipButtons()
        return lev2.ui.HandlerResult()

    return self.runtime.handle_camera_event(uievent)

  ##############################################################################
  # Picking
  ##############################################################################

  def _updatePickDebugViews(self):
    """Update pick debug visualization textures."""
    SG = self.runtime.scenegraph
    if SG is None:
      return
    self.pick_views[0].texture = SG.pick_tex_id
    self.pick_views[1].texture = SG.pick_tex_pos
    self.pick_views[2].texture = SG.pick_tex_nrm
    self.pickid_viz_pipeline.bindParam(self.p_colormap, SG.pick_tex_id)
    self.pickid_viz_pipeline.bindParam(self.p_nrmmap, SG.pick_tex_nrm)
    for iv in self.pick_views:
      iv.crosshair_enabled = True
      iv.crosshair_pos = vec2(0, 0)
      iv.setDirty()

  def _decodePickedNode(self, pfc):
    """Decode pick result → DrawableNode or None."""
    from orkengine.core import u32vec4
    obj = pfc.value(0)
    if obj is not None and isinstance(obj, u32vec4):
      pick_id = int(obj.x)
      if pick_id > 0 or (pick_id == 0 and int(obj.w) == 0):
        return pfc.decodePickID(pick_id)
    return None

  def _onPickResult(self, pfc):
    """Handle pick result — map drawable node → spawner → select."""
    print(f"_onPickResult called: pfc={pfc}")
    self._updatePickDebugViews()
    node = self._decodePickedNode(pfc)
    print(f"  decoded node: {node}")
    if node is None:
      return

    # Read entity ref from node userdata (stored by SceneGraphSystem)
    try:
      entref = node.user.entref
    except Exception:
      return

    # Look up entity → spawner via simulation
    controller = self.runtime.controller
    if controller is None:
      return
    sim = controller.simulation
    if sim is None:
      return
    entity = sim.findEntityByRef(entref)
    if entity is None:
      return
    spawner = entity.spawner
    if spawner is None:
      return

    # Select the spawner in outliner + property sheet
    spawner_name = spawner.name
    key = f"Spawners/{spawner_name}"
    self.outliner.selected_key = key
    self._onOutlinerSelect(key)

  ##############################################################################
  # App-level key shortcuts
  ##############################################################################

  def _onAppKeyShortcut(self, uievent):
    """App-level key shortcuts (Phase 1 — runs before widget routing)."""
    if uievent.code == tokens.KEY_DOWN.hashed and uievent.super:
      kc = uievent.keycode
      if kc == 262:  # Command+Right Arrow → Play
        if self._mode == self.EDIT:
          self.btn_play.toggled = True
          self._startPlay()
        result = lev2.ui.HandlerResult()
        result.setHandler(self.sgv)
        return result
      elif kc == 264:  # Command+Down Arrow → Stop
        if self._mode != self.EDIT:
          self._stopPlay()
        result = lev2.ui.HandlerResult()
        result.setHandler(self.sgv)
        return result
    return lev2.ui.HandlerResult()

  ##############################################################################
  # Update loop
  ##############################################################################

  def _onUpdate(self, updinfo):
    # Deferred rebuild — safe point between frames
    if self._needs_rebuild:
      self._needs_rebuild = False
      self._createEditSimulation()

    if self._mode == self.EDIT and self.runtime.controller:
      self.runtime.update()
      # Live sync property sheet when manipulating
      if self.manip_enabled and self.manip_interface and self._selected_object is not None:
        self.refl_model.notifyExternalValueChanged("")
    elif self._mode == self.PLAYING and self.runtime.controller:
      self.runtime.update()

    self.sgv.setDirty()

  ##############################################################################
  # GPU exit
  ##############################################################################

  def _onGpuExit(self, ctx):
    # The editor owns the scenegraph GPU lifecycle directly.
    # ECS simulations use an injected scene and never did their own GPU init,
    # so calling controller.gpuExit() would crash the uninitialized render FSM.
    pass

################################################################################

app = EcsEditor()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
