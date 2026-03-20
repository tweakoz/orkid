################################################################################
# ECS Scene Editor - Base class for extensible ECS editors
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import os, sys, time, math
from obt import command as obt_command
from orkengine import core
from orkengine.core import vec2, vec3, vec4, quat, CrcStringProxy, Transform, lev2_pyexdir
from orkengine import lev2
from orkengine import ecs
from ork.app.application import ComponentizedApplication
from ork.ui import standard_icons
from ork.editor.ecs_outliner_model import EcsOutlinerModel, _enumerateComponentTypes
from ork.ecs import EcsRuntime

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()
from lev2utils.primitives import createGridData

################################################################################

class EcsEditor(ComponentizedApplication):
  """Base ECS scene editor — extensible by downstream projects.

  Subclass and override the extension-point methods (marked with
  'Extension point' in docstrings) to add custom toolbar buttons,
  keyboard shortcuts, component types, overlay nodes, or per-frame logic.

  Constructor kwargs:
    scene      — path to a .json scene file to load on startup
    fullscreen — launch in fullscreen mode
    name       — application window title (default: "OrkidEcsEditor")
    width      — window width  (default: 1440)
    height     — window height (default: 900)
  """

  EDIT = 0
  PLAYING = 1
  PAUSED = 2

  def __init__(self, **kwargs):
    super().__init__()

    self._init_scene = kwargs.get("scene", None)
    self._init_fullscreen = kwargs.get("fullscreen", False)
    self._init_name = kwargs.get("name", "OrkidEcsEditor")
    self._init_width = kwargs.get("width", 1440)
    self._init_height = kwargs.get("height", 900)
    self._init_ssaa = kwargs.get("ssaa", 0)
    self._init_temporal = kwargs.get("temporal", 0)

    # Shared ECS runtime (camera, scenegraph, simulation lifecycle)
    self.runtime = EcsRuntime()

    # Editor state
    self._mode = self.EDIT
    self._selected_key = ""
    self._selected_object = None  # archetype, spawner, or system
    self._highlighted_spawner_name = None

    # Deferred rebuild flag — set by callbacks, consumed in _onUpdate
    self._needs_rebuild = False

    # 3D curve visualization state
    self._curve_path_data = None    # CurvePathDrawableData
    self._curve_line_node = None    # DrawableNode for line strip
    self._curve_cp_node = None      # DrawableNode for control point spheres
    self._curve_for_viz = None      # The TransformCurve being visualized
    self._curve_point_manip = None  # CurvePointManipulator (when a point is selected)

    # Deferred operations queue — (execute_at_time, callback) pairs
    self._deferred_ops = []

    # Bake lighting state
    self._bake_countdown = 0
    self._bake_output_base = ""

    # Will be set during init
    self.outliner_model = None
    self.grid_node = None
    self.gizmo_node = None

    # Create app with ECS module init injected before finalization
    ezapp_kwargs = dict(
      name=self._init_name,
      width=self._init_width,
      height=self._init_height,
      enable_audio=True,
      enable_audio_output=True,
      enable_audio_synth=True,
      pre_init_fns=self._getPreInitFns(),
    )
    if self._init_fullscreen:
      ezapp_kwargs["fullscreen"] = True
    self.createEzApp(**ezapp_kwargs)

  ##############################################################################
  # Extension points — override in subclasses
  ##############################################################################

  def _getPreInitFns(self):
    """Extension point: return list of pre-init functions for createEzApp.
    Default includes ecs.ecsInitCallback. Override to add more."""
    return [ecs.ecsInitCallback]

  def _getExtraToolbarButtons(self):
    """Extension point: return list of extra toolbar button defs.
    Each item is a dict with keys: name, icon, tooltip, callback.
    They are added after the standard buttons."""
    return []

  def _getToolbarButtonsAfterBake(self):
    """Extension point: return list of toolbar button defs to insert
    immediately after the bake-lighting button (before outliner filters).
    Same dict format as _getExtraToolbarButtons."""
    return []

  def _getExtraKeyboardShortcuts(self):
    """Extension point: return dict of keycode → handler(uievent).
    These are checked in _onCameraEvent for KEY_DOWN events."""
    return {}

  def _onEditorGpuInit(self, ctx):
    """Extension point: called after base GPU init completes.
    Add custom materials, drawables, or GPU resources here."""
    pass

  def _onEditorUpdate(self, updinfo):
    """Extension point: called each frame after base update logic."""
    pass

  def _onEditorGpuUpdate(self, ctx):
    """Extension point: called each frame after base GPU update."""
    pass

  def _onSelectionChanged(self, selected_object, key):
    """Extension point: called when outliner selection changes.
    Called after refl_model.object is set but before propsheet.rebuild().
    Override to add custom key overrides via self.refl_model.addKeyOverride()."""
    pass

  def _onSceneLoaded(self, path):
    """Extension point: called after a scene is successfully loaded."""
    pass

  def _onSimulationStarted(self):
    """Extension point: called after play-mode simulation starts."""
    pass

  def _onSimulationStopped(self):
    """Extension point: called after simulation stops and edit mode resumes."""
    pass

  def _getExtraComponentTypes(self):
    """Extension point: return list of additional component type names
    for the add-component dropdown."""
    return []

  def _createExtraOverlayNodes(self, sg, layer):
    """Extension point: create additional scenegraph overlay nodes.
    Called each time the scenegraph is rebuilt."""
    pass

  ##############################################################################
  # Properties
  ##############################################################################

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

    self.toolbar.addSeparator()

    # Lighting operations
    self.btn_bake_lighting = self.toolbar.addButton(
      "bake_lighting", standard_icons.get('bake_lighting', icon_size, icon_size),
      "Bake Lighting (render all probes to equirectangular PNGs)")

    # Buttons after bake (from subclass)
    after_bake = self._getToolbarButtonsAfterBake()
    for btn_def in after_bake:
      btn = self.toolbar.addButton(
        btn_def["name"], btn_def["icon"], btn_def["tooltip"])
      if "callback" in btn_def:
        btn.onPressed(btn_def["callback"])
      setattr(self, f"btn_{btn_def['name']}", btn)

    # Outliner filter toggles
    self.toolbar.addSeparator()
    self.btn_filter_arch = self.toolbar.addTextButton(
      "filter_arch", "Arch", "Toggle Archetypes")
    self.btn_filter_arch.toggle_mode = True
    self.btn_filter_arch.toggled = False
    self.btn_filter_spawn = self.toolbar.addTextButton(
      "filter_spawn", "Spawn", "Toggle Spawners")
    self.btn_filter_spawn.toggle_mode = True
    self.btn_filter_spawn.toggled = True
    self.btn_filter_sys = self.toolbar.addTextButton(
      "filter_sys", "Sys", "Toggle Systems")
    self.btn_filter_sys.toggle_mode = True
    self.btn_filter_sys.toggled = True

    # Extra buttons from subclass
    extras = self._getExtraToolbarButtons()
    if extras:
      self.toolbar.addSeparator()
      for btn_def in extras:
        btn = self.toolbar.addButton(
          btn_def["name"], btn_def["icon"], btn_def["tooltip"])
        if "toggle_mode" in btn_def:
          btn.toggle_mode = btn_def["toggle_mode"]
        if "callback" in btn_def:
          btn.onPressed(btn_def["callback"])
        if "on_toggled" in btn_def:
          btn.onToggled(btn_def["on_toggled"])
        # Store on the editor so subclasses can reference
        setattr(self, f"btn_{btn_def['name']}", btn)

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
    if "userparams/" in key:
      param_name = key.split("/")[-1]
      if self.runtime.scenegraph:
        self.runtime.scenegraph.applyRuntimeParams({param_name: value})
    elif "/assetpath" in key:
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

    if self._curve_editor is not None:
      self._closeTransformCurveEditor()

    top = self.ezapp.topLayoutGroup
    h = int(top.height * 0.47)
    y = top.height - h

    editor = lev2.ui.TransformCurveEditor.create("curve_editor", curve)
    editor.onClose = lambda: self._closeTransformCurveEditor()
    editor.onCurveChanged = lambda: self._onCurveEditorChanged()
    self._curve_editor = editor

    if self._curve_for_viz is None:
      self._showCurveViz(curve)

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

    if self._init_ssaa > 0:
      self.sgv.supersample = self._init_ssaa
    if self._init_temporal > 0:
      self.sgv.temporal_frames = self._init_temporal

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
    self.outliner.onDoubleClick(self._onOutlinerDoubleClick)
    self.outliner.onKeyDown(self._onOutlinerKeyDown)

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
    self.btn_bake_lighting.onPressed(self._onBakeLighting)
    self.btn_filter_arch.onToggled(lambda t: self._onFilterToggle("Archetypes", self.btn_filter_arch))
    self.btn_filter_spawn.onToggled(lambda t: self._onFilterToggle("Spawners", self.btn_filter_spawn))
    self.btn_filter_sys.onToggled(lambda t: self._onFilterToggle("Systems", self.btn_filter_sys))

    # Apply initial filter state (archetypes hidden by default)
    if not self.btn_filter_arch.toggled:
      self.outliner_model.toggleCategory("Archetypes")

    # Create initial edit simulation (creates fresh scenegraph + binds to viewport)
    self._createEditSimulation()

    # Load initial scene if provided (deferred to let GPU init settle)
    if self._init_scene and os.path.exists(self._init_scene):
      scene_path = self._init_scene
      self.defer(lambda: self._loadScene(scene_path), delay=2.0)

    # Subclass GPU init
    self._onEditorGpuInit(ctx)

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
    grid_node.enabled = False

    gizmo_node = sg.createDrawableNodeOnLayers(
      [layer], "manip-gizmo", self.gizmo_drawable)
    gizmo_node.pickable = False
    gizmo_node.enabled = self.manip_enabled

    # Subclass overlay nodes
    self._createExtraOverlayNodes(sg, layer)

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

    # Re-create curve viz if we had one active
    if self._curve_for_viz is not None:
      curve = self._curve_for_viz
      self._curve_path_data = None
      self._curve_line_node = None
      self._curve_cp_node = None
      self._showCurveViz(curve)

  def _requestRebuild(self):
    """Request a deferred edit-simulation rebuild (consumed in _onUpdate)."""
    if self._mode == self.EDIT:
      self._needs_rebuild = True

  def defer(self, callback, delay=0.0):
    """Schedule a callback to run on the main thread after delay seconds."""
    self._deferred_ops.append((time.monotonic() + delay, callback))

  def _destroyEditSimulation(self):
    """Tear down the edit-mode simulation."""
    self._highlighted_spawner_name = None
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

    # Disable manipulator and hide curve viz when selection changes
    self._enableManip(False)
    self.manip_interface = None
    self._hideCurveViz()

    if len(parts) < 2:
      self.refl_model.clearKeyOverrides()
      self.refl_model.object = None
      self.propsheet.rebuild()
      self._onSelectionChanged(None, key)
      return

    category = parts[0]
    name = parts[1]

    # Clear key overrides for non-spawner selections
    if category != "Spawners":
      self.refl_model.clearKeyOverrides()

    if category == "Archetypes":
      arch = self.outliner_model._findArchetype(name)
      if arch:
        if len(parts) == 4:
          # Node selected under SceneGraphComponent
          comp = self.outliner_model._findComponent(parts[1], parts[2])
          if comp and comp.className == "SceneGraphComponentData":
            node_name = parts[3]
            nodedatas = comp.nodedatas
            if node_name in nodedatas:
              nid = nodedatas[node_name]
              self._selected_object = nid.drawabledata
              self._setupNodePropertyOverrides(nid, comp)
        elif len(parts) == 3:
          comp_name = parts[2]
          for c in arch.components:
            if c.className == comp_name:
              self._selected_object = c
              break
          # Show 3D curve viz when TransformCurveComponentData is selected
          if self._selected_object is not None and self._selected_object.className == "TransformCurveComponentData":
            curve = getattr(self._selected_object, 'curve', None)
            if curve:
              self._showCurveViz(curve)
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
        # Show 3D curve viz if spawner has a TransformCurveComponent
        curve = self._findCurveForSpawner(sp)
        if curve:
          self._showCurveViz(curve)
    elif category == "Systems":
      for s in self.scene_data.systemDatas:
        if s.className == name:
          self._selected_object = s
          break

    self.refl_model.object = self._selected_object
    self._onSelectionChanged(self._selected_object, key)
    self.propsheet.rebuild()
    self.propsheet.expandAll()

  def _setupNodePropertyOverrides(self, nid, comp):
    """Set up property sheet key overrides for a SceneGraphNodeItemData."""
    from ork.editor.ecs_outliner_model import _enumerateDrawableDataTypes
    self.refl_model.clearKeyOverrides()

    # Layer name
    self.refl_model.addKeyOverride(
        "Layer",
        lev2.ui.PropertyType.String,
        lambda: nid.layername,
        lambda val: setattr(nid, 'layername', val),
        None)

    # Drawable type dropdown
    def get_drawable_class():
      return nid.drawableClassName

    def set_drawable_class(val):
      nid.drawableClassName = val
      # Update selected object to the new drawable data
      self._selected_object = nid.drawabledata
      self.refl_model.object = self._selected_object
      self.propsheet.rebuild()
      self.propsheet.expandAll()

    self.refl_model.addKeyOverride(
        "DrawableType",
        lev2.ui.PropertyType.String,
        get_drawable_class,
        set_drawable_class,
        lambda: ["(none)"] + _enumerateDrawableDataTypes())

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

  def _onOutlinerDoubleClick(self, key):
    """Double-click a spawner to select its archetype."""
    parts = key.split("/") if key else []
    if len(parts) == 2 and parts[0] == "Spawners":
      sp = self.outliner_model._findSpawner(parts[1])
      if sp and sp.archetype:
        arch_key = f"Archetypes/{sp.archetype.name}"
        self.outliner.selected_key = arch_key
        self._onOutlinerSelect(arch_key)

  def _onOutlinerKeyDown(self, selected_key, keycode):
    """Handle unhandled keys from outliner."""
    if keycode == ord("A") and selected_key:
      parts = selected_key.split("/")
      if len(parts) == 2 and parts[0] == "Spawners":
        sp = self.outliner_model._findSpawner(parts[1])
        if sp and sp.archetype:
          arch_key = f"Archetypes/{sp.archetype.name}"
          self.outliner.selected_key = arch_key
          self._onOutlinerSelect(arch_key)

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
    existing = {c.className for c in arch.components}
    available = [ct for ct in _enumerateComponentTypes() if ct not in existing]
    # Include extra component types from subclass
    extras = self._getExtraComponentTypes()
    available.extend(ct for ct in extras if ct not in existing)
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
      self._onSceneLoaded(path)

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
  # Bake Lighting
  ##############################################################################

  def _initBakeFSM(self):
    """Create the HFSM for the bake lighting workflow."""
    from orkengine.core import fsm

    data = fsm.FsmData()
    self._bake_idle = data.createState(None, "IDLE")
    self._bake_baking = data.createState(None, "BAKING")

    data.addTransition(self._bake_idle, "bake_requested", self._bake_baking)
    data.addTransition(self._bake_baking, "bake_done", self._bake_idle)

    self._bake_baking.onEnter = lambda inst: self._bakeFSM_onBaking()

    self._bake_fsm_data = data
    self._bake_fsm = fsm.FsmInstance(data)
    self._bake_fsm.changeState(self._bake_idle)
    self._bake_fsm.update()

  def _bakeFSM_onBaking(self):
    """BAKING state: send Bake or BakeSelected request."""
    ctrl = self.runtime.controller
    if not ctrl or not ctrl.simulation:
      self._bake_fsm.sendEvent("bake_done")
      return

    # Hide gizmos and selection highlights during bake
    self._enableManip(False)
    self._bake_grid_was_enabled = False
    if hasattr(self, 'grid_node') and self.grid_node:
      self._bake_grid_was_enabled = self.grid_node.enabled
      self.grid_node.enabled = False

    # Build expected filenames before bake
    expected_files = []
    sd = self.runtime.scene_data
    if sd:
      idx = 0
      for arch in sd.archetypes:
        for comp in arch.components:
          if comp.className == "ProbeComponentData":
            folder = core.Path.expandPathString(comp.outputFolder) if comp.outputFolder else self._bake_output_base
            prefix = comp.outputPrefix if comp.outputPrefix else "probe"
            # If baking selected, only expect files for the selected spawner
            if not self._bake_selected_spawner or self._bake_selected_spawner_arch == arch.name:
              expected_files.append(os.path.join(folder, f"{prefix}_{idx}.png"))
            idx += 1

    def on_bake_done():
      print("BakeFSM: bake complete")
      # Restore gizmos and grid to prior state
      if hasattr(self, 'grid_node') and self.grid_node:
        self.grid_node.enabled = self._bake_grid_was_enabled
      if self.manip_interface:
        self._enableManip(True)
      for p in expected_files:
        if os.path.exists(p):
          print(f"  Opening in HDRI Studio: {p}")
          obt_command.runasync(["ork.hdri.studio.py", "-i", p])
      self._bake_fsm.sendEvent("bake_done")

    if self._bake_selected_spawner:
      print(f"BakeFSM: sending BakeSelected request for spawner '{self._bake_selected_spawner}'")
      ctrl.systemRequestWithCallback(
        self._probe_sys, tokens.BakeSelected, self._bake_selected_object, on_bake_done)
    else:
      print("BakeFSM: sending Bake request (all probes)")
      ctrl.systemRequestWithCallback(
        self._probe_sys, tokens.Bake, self._bake_output_base, on_bake_done)

  def _bakeFSM_update(self, ctx):
    """Called from _onGpuUpdate to drive the bake FSM."""
    if not hasattr(self, '_bake_fsm'):
      return
    self._bake_fsm.update()

  def _onBakeLighting(self):
    """Trigger BakeLighting via HFSM. Bakes selected probe spawner if one is selected, otherwise all."""
    if self._mode != self.EDIT:
      print("BakeLighting requires EDIT mode")
      return
    if not self.runtime.controller:
      return
    sim = self.runtime.controller.simulation
    if not sim:
      return

    self._bake_output_base = "/tmp/ecs_probes"
    os.makedirs(self._bake_output_base, exist_ok=True)

    if not hasattr(self, '_bake_fsm'):
      self._initBakeFSM()

    # Get probe system handle for event-based communication
    self._probe_sys = self.runtime.controller.findSystemHandle("ProbeSystem")

    # Check if a probe spawner is currently selected
    self._bake_selected_spawner = None
    self._bake_selected_spawner_arch = None
    self._bake_selected_object = None
    if hasattr(self, '_selected_object') and self._selected_object is not None:
      # Check if the selected object is a spawner with a ProbeComponent archetype
      if hasattr(self._selected_object, 'archetype') and self._selected_object.archetype:
        for comp in self._selected_object.archetype.components:
          if comp.className == "ProbeComponentData":
            self._bake_selected_spawner = self._selected_object.name
            self._bake_selected_spawner_arch = self._selected_object.archetype.name
            self._bake_selected_object = self._selected_object
            break

    if self._bake_fsm.currentState == self._bake_idle:
      if self._bake_selected_spawner:
        print(f"BakeFSM: bake requested for selected spawner '{self._bake_selected_spawner}'")
      else:
        print("BakeFSM: bake requested (all probes)")
      self._bake_fsm.sendEvent("bake_requested")
      self._bake_fsm.update()

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
      self._onSimulationStarted()
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
      self._onSimulationStopped()
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

  def _onFilterToggle(self, category, btn):
    visible = self.outliner_model.toggleCategory(category)
    btn.toggled = visible
    self.outliner.expandAll()

  ##############################################################################
  # 3D Curve Visualization
  ##############################################################################

  def _showCurveViz(self, curve):
    """Show 3D curve path + control point spheres for the given TransformCurve."""
    self._hideCurveViz()

    sg = self.runtime.scenegraph
    layer = self.runtime.layer
    if sg is None or layer is None:
      return

    data = lev2.CurvePathDrawableData()
    data.curve = curve
    self._curve_path_data = data
    self._curve_for_viz = curve

    line_drw = data.createDrawable()
    self._curve_line_node = sg.createDrawableNodeOnLayers([layer], "curve-line", line_drw)
    self._curve_line_node.sortkey = 50
    self._curve_line_node.pickable = False

    cp_drw = data.createControlPointDrawable()
    self._curve_cp_node = sg.createDrawableNodeOnLayers([layer], "curve-cp", cp_drw)
    self._curve_cp_node.sortkey = 51
    self._curve_cp_node.pickable = False

  def _hideCurveViz(self):
    """Remove 3D curve visualization nodes."""
    layer = getattr(self.runtime, 'layer', None)
    if layer:
      if self._curve_line_node:
        layer.removeDrawableNode(self._curve_line_node)
      if self._curve_cp_node:
        layer.removeDrawableNode(self._curve_cp_node)
    self._curve_path_data = None
    self._curve_line_node = None
    self._curve_cp_node = None
    self._curve_for_viz = None
    self._curve_point_manip = None

  def _findCurveForSpawner(self, spawner):
    """If spawner's archetype has a TransformCurveComponent, return its curve."""
    arch = spawner.archetype if spawner else None
    if arch is None:
      return None
    for comp in arch.components:
      if comp.className == "TransformCurveComponentData":
        return comp.curve
    return None

  def _onCurvePointMoved(self):
    """Callback when gizmo moves a control point."""
    if self._curve_path_data:
      self._curve_path_data.updateControlPoints()
    self.sgv.setDirty()
    self.refl_model.notifyExternalValueChanged("")

  def _onCurveEditorChanged(self):
    """Callback when 2D curve editor modifies the curve — sync 3D viz."""
    if self._curve_path_data:
      self._curve_path_data.updateControlPoints()
    self.sgv.setDirty()

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
          if in_bounds and self._curve_path_data is not None:
            # CPU hit test on curve control points
            aspect = float(self.sgv.width) / float(self.sgv.height)
            vp_mtx = self.runtime.camera.vpMatrix(aspect)
            hit_idx = self._curve_path_data.hitTestScreenCoord(
              vp_mtx, vec2(local_x, local_y),
              self.sgv.width, self.sgv.height, 20.0)
            if hit_idx >= 0:
              print(f"  curve point hit: {hit_idx}")
              manip = lev2.CurvePointManipulator(self._curve_for_viz, hit_idx)
              manip.onPointMoved = self._onCurvePointMoved
              self._curve_point_manip = manip
              self._curve_path_data.selectedPointIndex = hit_idx
              self._curve_path_data.updateControlPoints()
              self.manip_interface = manip
              self._enableManip(True)
              self.manip_controller.mode = lev2.ManipMode.TRANSLATE
              self._syncManipButtons()
              return self.runtime.handle_camera_event(uievent)
            elif self._curve_point_manip is not None:
              # Clicked away from control points — revert to spawner transform manip
              self._curve_point_manip = None
              self._curve_path_data.selectedPointIndex = -1
              self._curve_path_data.updateControlPoints()
              if self._selected_object is not None and hasattr(self._selected_object, 'transform'):
                xform = self._selected_object.transform
                self.manip_interface = lev2.DecompTransformManipulator(xform)
                self._enableManip(True)
                self._syncManipButtons()
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

      # Check extra shortcuts from subclass first
      extra_shortcuts = self._getExtraKeyboardShortcuts()
      if kc in extra_shortcuts:
        return extra_shortcuts[kc](uievent)

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
      elif kc == ord("G"):
        # Toggle grid visibility
        if self.grid_node:
          self.grid_node.enabled = not self.grid_node.enabled
        return lev2.ui.HandlerResult()
      elif kc == ord("F"):
        # Focus camera on selected spawner
        if isinstance(self._selected_object, ecs.SpawnData):
          tgt = self._selected_object.transform.translation
          uicam = self.runtime.uicam
          dist = uicam.distance if uicam.distance > 0.1 else 5.0
          eye = tgt + vec3(0, dist * 0.3, dist)
          uicam.lookAt(eye, tgt, vec3(0, 1, 0))
          uicam.updateMatrices()
          self.runtime.camera.copyFrom(uicam.cameradata)
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
    """Decode pick result -> DrawableNode or None."""
    from orkengine.core import u32vec4
    obj = pfc.value(0)
    if obj is not None and isinstance(obj, u32vec4):
      pick_id = int(obj.x)
      if pick_id > 0 or (pick_id == 0 and int(obj.w) == 0):
        return pfc.decodePickID(pick_id)
    return None

  def _resolvePickToSpawner(self, decoded):
    """Resolve a decoded pick result to a spawner, or None.
    Handles both DrawableNode (with user.entref) and
    direct uint64 entity ref from custom pick systems."""
    controller = self.runtime.controller
    if controller is None:
      return None
    sim = controller.simulation
    if sim is None:
      return None

    entref = None

    # Case 1: direct uint64 entity ref (from custom ECS pick systems)
    if isinstance(decoded, int):
      entref = decoded
    # Case 2: DrawableNode with user.entref (from SceneGraphSystem)
    else:
      try:
        entref = decoded.user.entref
      except Exception:
        return None

    if entref is None:
      return None
    entity = sim.findEntityByRef(entref)
    if entity is None:
      return None
    return entity.spawner

  def _onPickResult(self, pfc):
    """Handle pick result — map drawable node -> spawner -> select."""
    print(f"_onPickResult called: pfc={pfc}")
    self._updatePickDebugViews()
    node = self._decodePickedNode(pfc)
    print(f"  decoded node: {node}")
    if node is None:
      return

    spawner = self._resolvePickToSpawner(node)
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
      if kc == 262:  # Command+Right Arrow -> Play
        if self._mode == self.EDIT:
          self.btn_play.toggled = True
          self._startPlay()
        result = lev2.ui.HandlerResult()
        result.setHandler(self.sgv)
        return result
      elif kc == 264:  # Command+Down Arrow -> Stop / Reinit
        if self._mode != self.EDIT:
          self._stopPlay()
        else:
          # Already in edit mode — reinit scene (forces asset reload)
          print("Reinitializing edit simulation...")
          self._createEditSimulation()
        result = lev2.ui.HandlerResult()
        result.setHandler(self.sgv)
        return result
    return lev2.ui.HandlerResult()

  ##############################################################################
  # Update loop
  ##############################################################################

  def _onGpuUpdate(self, ctx):
    self.runtime.gpuUpdate(ctx)
    # Process deferred operations (main thread)
    now = time.monotonic()
    ready = [op for op in self._deferred_ops if now >= op[0]]
    self._deferred_ops = [op for op in self._deferred_ops if now < op[0]]
    for _, callback in ready:
      callback()
    # Bake lighting HFSM — driven here (outside beginFrame/endFrame)
    self._bakeFSM_update(ctx)
    # Subclass GPU update
    self._onEditorGpuUpdate(ctx)

  def _onUpdate(self, updinfo):
    # Deferred rebuild — safe point between frames
    if self._needs_rebuild:
      self._needs_rebuild = False
      self._createEditSimulation()

    if self._mode == self.EDIT and self.runtime.controller:
      # Sync spawner transform -> entity scenegraph nodes BEFORE update
      # (so _serviceEventQueues processes it this frame)
      if self.manip_enabled and self.manip_interface and isinstance(self._selected_object, ecs.SpawnData):
        if self.runtime._sys_ref:
          self.runtime.controller.systemNotify(
            self.runtime._sys_ref,
            tokens.SyncTransformBySpawnData,
            {tokens.name: self._selected_object.name})
      self.runtime.update()
      # Live sync property sheet when manipulating
      if self.manip_enabled and self.manip_interface and self._selected_object is not None:
        self.refl_model.notifyExternalValueChanged("")
    elif self._mode == self.PLAYING and self.runtime.controller:
      self.runtime.update()

    # Selection highlight: animate selected spawner's entities red<->white
    self._updateSelectionHighlight()

    # Subclass update
    self._onEditorUpdate(updinfo)

    self.sgv.setDirty()

  def _updateSelectionHighlight(self):
    if not self.runtime.controller or not self.runtime._sys_ref:
      return

    # Determine current spawner name (if a spawner is selected)
    cur_name = None
    if isinstance(self._selected_object, ecs.SpawnData):
      cur_name = self._selected_object.name

    # Reset previous highlight if selection changed
    if self._highlighted_spawner_name and self._highlighted_spawner_name != cur_name:
      self.runtime.controller.systemNotify(
        self.runtime._sys_ref,
        tokens.HighlightBySpawnData,
        {tokens.name: self._highlighted_spawner_name, tokens.color: vec4(1, 1, 1, 1)})
      self._highlighted_spawner_name = None

    # Animate current selection
    if cur_name:
      t = math.sin(time.monotonic() * 8.0) * 0.5 + 0.5
      self.runtime.controller.systemNotify(
        self.runtime._sys_ref,
        tokens.HighlightBySpawnData,
        {tokens.name: cur_name, tokens.color: vec4(1 + t, 2*t, 2*t, 1)})
      self._highlighted_spawner_name = cur_name

  ##############################################################################
  # GPU exit
  ##############################################################################

  def _onGpuExit(self, ctx):
    # The editor owns the scenegraph GPU lifecycle directly.
    # ECS simulations use an injected scene and never did their own GPU init,
    # so calling controller.gpuExit() would crash the uninitialized render FSM.
    pass
