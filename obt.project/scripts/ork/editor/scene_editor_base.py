################################################################################
# Scene Editor Base - Base class for scene graph editors
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import os
import sys
from orkengine.core import vec2, vec3, vec4, quat, VarMap, CrcStringProxy, Transform, lev2_pyexdir
from orkengine import lev2
from ork.ui import icon_library
from ork.ui.filesystem_browser import FilesystemBrowser
from ork.ui.transform_edit import TransformEdit
from ork.ui.color_picker import ColorPicker
from ork.app.application import ComponentizedApplication

from .light_editor import LightPropertyEditor
from .scene_io import SceneLoader, SceneSaver

tokens = CrcStringProxy()

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCameraX
from lev2utils.primitives import createGridData

################################################################################

class SceneEditorBase(ComponentizedApplication):
  """Base class for scene graph editors.

  Provides:
    - Standard UI layout (viewport, outliner, property sheet)
    - Transform and light property editing
    - 3D manipulator/gizmo
    - Selection management with highlighting
    - Pick-to-select
    - Camera controls with standard shortcuts (T/R/S, ESC, F)
    - File load/save popup helpers

  Subclass must implement:
    - getNodeTypes(): Return node type definitions for outliner
    - findNode(name, item_type): Find node by name
    - createNode(name, item_type): Create node
    - deleteNode(name, item_type): Delete node
    - renameNode(old_name, new_name, item_type): Rename node
    - onSelectionChanged(node, item_type): Handle selection (optional)
    - getExtraKeyboardShortcuts(): Additional shortcuts (optional)

  Configuration via constructor args:
    - left_dock_proportion: Width of left panel (default: 0.25)
    - propsheet_proportion: Height of property sheet (default: 0.6)
    - sg_params: Scenegraph parameters (VarMap)
    - file_extension: Scene file extension (default: ".osgr")
    - home_dir: Starting directory for file browser
  """

  def __init__(self,
               left_dock_proportion=0.25,
               propsheet_proportion=0.6,
               sg_params=None,
               file_extension=".osgr",
               home_dir=None):
    super().__init__()  # ComponentizedApplication.__init__

    self._left_dock_proportion = left_dock_proportion
    self._propsheet_proportion = propsheet_proportion
    self._sg_params = sg_params
    self._file_extension = file_extension
    self._home_dir = home_dir or os.path.expanduser("~")

    # Selection state (node references, not names)
    self.selected_node = None   # DrawableNode or None
    self.selected_light = None  # LightNode or None
    self._purgatory = set()     # Holds removed nodes until true deletion

    # Mouse position tracking for focus-pick
    self._last_mouse_x = 0
    self._last_mouse_y = 0

    # Will be set during GPU init
    self.scenegraph = None
    self.layer = None
    self.scene_model = None

  def _onUiInit(self):
    """Set up the editor UI layout."""
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.15, 0.15, 0.15, 1)
    lg.clearColorGuide = vec4(0.4, 0.4, 0.2, 1)

    # Viewport dock (fills area)
    viewport_dock_item = lg.makeChild(fill=True, margin=2, uiclass=lev2.ui.DockablePanel, args=["viewport_dock"])
    self.viewport_dock = viewport_dock_item.widget
    self.viewport_dock.titlebar_color = vec4(0.15, 0.2, 0.25, 1)
    self.sgv = self.viewport_dock.createChild(uiclass=lev2.ui.SceneGraphViewport, args=["Viewport", vec4(0.1, 0.1, 0.12, 1)])

    # Left dock
    left_dock_item = lg.split(layout=viewport_dock_item.layout, proportion=self._left_dock_proportion, placement=tokens.LEFT, margin=2, uiclass=lev2.ui.DockablePanel, args=["left_dock"])
    self.left_dock = left_dock_item.widget
    self.left_dock.titlebar_color = vec4(0.2, 0.15, 0.2, 1)

    # Left panel with toolbar + outliner
    self.left_panel = self.left_dock.createChild(uiclass=lev2.ui.VerticalPack, args=["Scene"])
    self.left_panel.margin = 2
    self.left_panel.item_height = 36

    # Toolbar
    self._setupToolbar()

    # Pick debug views
    self._setupPickDebug()

    # Outliner
    self.outliner = self.left_panel.makeChild(uiclass=lev2.ui.Outliner, args=["outliner"])
    self.left_panel.fill_widget = self.outliner
    self.outliner.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.outliner.item_height = 22

    # Property sheet dock
    propsheet_dock_item = lg.split(layout=left_dock_item.layout, proportion=self._propsheet_proportion, placement=tokens.BOTTOM, margin=2, uiclass=lev2.ui.DockablePanel, args=["propsheet_dock"])
    self.propsheet_dock = propsheet_dock_item.widget
    self.propsheet_dock.titlebar_color = vec4(0.2, 0.2, 0.15, 1)
    self._setupPropertySheet()

  def _setupToolbar(self):
    """Set up the toolbar with LOAD/SAVE buttons."""
    self.toolbar = self.left_panel.makeChild(uiclass=lev2.ui.HorizontalPack, args=["toolbar"])
    self.toolbar.margin = 2
    self.toolbar.item_width = 48
    self.toolbar.bg_color = vec4(0.12, 0.12, 0.15, 1)

    def make_icon(text):
      return icon_library.from_svg_string(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><text x="12" y="12" text-anchor="middle" dominant-baseline="central" font-family="sans-serif" font-size="7" font-weight="bold" fill="#E6E6E6">{text}</text></svg>',
        32, 32)

    for name, color, handler in [
      ("LOAD", vec4(0.15, 0.25, 0.15, 1), lambda b: self._openFilePopup("Load Scene", self._loadScene, "load")),
      ("SAVE", vec4(0.15, 0.15, 0.25, 1), lambda b: self._openFilePopup("Save Scene", self._saveScene, "save")),
    ]:
      btn = self.toolbar.makeChild(uiclass=lev2.ui.ImageButton, args=[f"btn_{name.lower()}"])
      btn.inactive_image = make_icon(name)
      btn.bgcolor = color
      btn.inactive_blend_mode = tokens.ALPHA
      btn.onPressed = handler

  def _setupPickDebug(self):
    """Set up pick debug visualization."""
    self.pick_collapsable = self.left_panel.makeChild(uiclass=lev2.ui.Collapsable, args=["Pick Debug"])
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

  def _setupPropertySheet(self):
    """Set up property sheet with transform and light color editors."""
    self.propsheet = self.propsheet_dock.createChild(uiclass=lev2.ui.VerticalPack, args=["propsheet"])
    self.propsheet.margin = 4
    self.propsheet.item_height = 100

    # Transform collapsable
    self.xform_collapsable = self.propsheet.makeChild(uiclass=lev2.ui.Collapsable, args=["Transform"])
    self.xform_collapsable.header_height = 24
    self.xform_collapsable.header_bg_color = vec4(0.2, 0.2, 0.25, 1)
    self.xform_collapsable.expanded = True

    self.xform_widget = TransformEdit.wfactory(["xform_edit"])
    self.xform_widget.fixed_height = 84
    self.xform_collapsable.setChild(self.xform_widget)
    self.xform_editor = self.xform_widget.uservars.transform_edit

    # Light color collapsable
    self.color_collapsable = self.propsheet.makeChild(uiclass=lev2.ui.Collapsable, args=["Light Color"])
    self.color_collapsable.header_height = 24
    self.color_collapsable.header_bg_color = vec4(0.25, 0.2, 0.15, 1)
    self.color_collapsable.expanded = False

    self.color_vpack = lev2.ui.VerticalPack.wfactory(["color_vpack"])
    self.color_vpack.fixed_height = 200
    self.color_vpack.margin = 2
    self.color_vpack.item_height = 24
    self.color_collapsable.setChild(self.color_vpack)

    self.color_picker_widget = self.color_vpack.makeChild(uiclass=ColorPicker, args=["light_color", vec3(0.3, 0.3, 0.3), vec4(1, 1, 1, 1)])
    self.color_picker_widget.fixed_height = 160
    self.color_picker = self.color_picker_widget.uservars.color_picker

    self.intensity_edit = self.color_vpack.makeChild(uiclass=lev2.ui.F32Edit, args=["intensity", "Intensity", 100.0, -1000.0, 1000.0])
    self.intensity_edit.drag_rate = 1.0
    self.intensity_edit.precision = 1

    # Create MVC controller for light properties
    self.light_editor = LightPropertyEditor(self.color_picker, self.intensity_edit)

  ##############################################
  # File I/O
  ##############################################

  def _openFilePopup(self, title, callback, mode):
    """Open a file browser popup."""
    popup = self.ezapp.createSecondaryWindow(width=800, height=600, x=200, y=150, title=title, decorated=True, resizable=True, floating=True)
    uic = popup.ui_context
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, popup.width, popup.height)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(uiclass=FilesystemBrowser, args=["browser", self._home_dir, self._file_extension, vec3(0.1, 0.1, 0.1), mode], fill=True)
    browser = browser_item.widget.uservars.filesystem_browser
    browser.onActivate = lambda p: (callback(p), popup.requestClose())
    browser.onCancel = lambda: popup.requestClose()

  def _loadScene(self, path):
    """Load scene from file. Override for custom behavior."""
    if not os.path.exists(path):
      return
    try:
      # Clear existing scene
      self._clearScene()

      # Get particle presets if available
      particle_presets = self.getParticlePresets()

      # Use standalone loader
      result = SceneLoader.load(path, self.scenegraph, self.layer, particle_presets=particle_presets)

      # Post-process loaded nodes (subclass hook)
      self._onSceneLoaded(result)

      # Notify outliner to refresh
      self.scene_model.notifyModelReset()
      print(f"Loaded: {path}")
    except Exception as e:
      print(f"Load failed: {e}")

  def _saveScene(self, path):
    """Save scene to file. Override for custom behavior."""
    try:
      # Get type tokens from node types
      node_types = self.getNodeTypes()
      drawable_type = None
      light_type = None
      particle_type = None
      for nt in node_types:
        if nt.get("item_type") == "particle":
          particle_type = nt.get("drawable_type")
        elif "drawable_type" in nt:
          drawable_type = nt["drawable_type"]
        elif "light_type" in nt:
          light_type = nt["light_type"]

      path = SceneSaver.save(path, self.scenegraph, drawable_type, light_type, particle_type)
      print(f"Saved: {path}")
    except Exception as e:
      print(f"Save failed: {e}")

  def _clearScene(self):
    """Clear all nodes from scene. Override for custom node types."""
    node_types = self.getNodeTypes()
    for nt in node_types:
      if "drawable_type" in nt:
        for node in list(self.scenegraph.drawableNodesWithType(nt["drawable_type"])):
          self.deleteNode(node.name, nt["item_type"])
      elif "light_type" in nt:
        for node in list(self.scenegraph.lightNodesWithType(nt["light_type"])):
          self.deleteNode(node.name, nt["item_type"])

  def _onSceneLoaded(self, result):
    """Called after scene is loaded. Override to post-process nodes."""
    pass

  def _newScene(self):
    """Create a new empty scene. Clears existing and creates initial content."""
    self._clearSelection(clear_outliner=True)
    self._clearScene()
    self._createInitialScene()
    self.scene_model.notifyModelReset()
    print("New scene created")

  def _createInitialScene(self):
    """Create initial scene content. Override in subclass for custom setup."""
    pass

  ##############################################
  # Selection management
  ##############################################

  def _selectDrawableNode(self, node):
    """Select a drawable node."""
    # Clear previous selection
    if self.selected_node is not None:
      self.selected_node.modcolor = vec4(1, 1, 1, 1)
    if self.selected_light is not None:
      self.selected_light = None
      self.light_editor.unbind()
      self.color_collapsable.expanded = False

    # Set new selection
    self.selected_node = node
    node.modcolor = vec4(1, 0.3, 0.3, 1)
    xform = node.worldTransform
    self.manip_interface = lev2.DecompTransformManipulator(xform)
    self.manip_controller.target = self.manip_interface
    self.xform_editor.transform = xform
    self.xform_editor.position_only = False

    self.onSelectionChanged(node, "node")

  def _selectLightNode(self, node):
    """Select a light node."""
    # Clear previous selection
    if self.selected_node is not None:
      self.selected_node.modcolor = vec4(1, 1, 1, 1)
      self.selected_node = None

    # Set new selection
    self.selected_light = node
    self.color_collapsable.expanded = True
    lightdata = node.user.lightdata
    self.light_editor.bind(lightdata)

    xform = node.worldTransform
    self.manip_interface = lev2.DecompTransformManipulator(xform)
    self.manip_controller.target = self.manip_interface
    self.manip_controller.mode = lev2.ManipMode.TRANSLATE
    self.xform_editor.transform = xform
    self.xform_editor.position_only = True

    self.onSelectionChanged(node, "light")

  def _clearSelection(self, clear_outliner=False):
    """Clear current selection."""
    if self.selected_node is not None:
      self.selected_node.modcolor = vec4(1, 1, 1, 1)
    self.selected_node = None
    self.selected_light = None
    self.light_editor.unbind()
    self.color_collapsable.expanded = False
    if clear_outliner:
      self.outliner.selected_key = ""
    self.manip_controller.target = None

  def _enableManip(self, enable):
    """Enable or disable the manipulator."""
    self.manip_enabled = enable
    self.gizmo_node.enabled = enable
    if enable and self.manip_interface:
      self.manip_controller.target = self.manip_interface
    else:
      self.manip_controller.target = None

  ##############################################
  # Subclass hooks (override these)
  ##############################################

  def getNodeTypes(self):
    """Return list of node type definitions. Must override."""
    raise NotImplementedError("Subclass must implement getNodeTypes()")

  def getParticlePresets(self):
    """Return dict of particle preset factories. Override if using particles.

    Returns:
      dict of preset_name -> factory function, or None if no particles
      Factory signature: factory(scenegraph, layer, name) -> node
    """
    return None

  def onCycleParticlePreset(self, node):
    """Called when Option-P is pressed on a particle node. Override to implement preset cycling."""
    pass

  def findNode(self, name, item_type):
    """Find a node by name and type. Must override."""
    raise NotImplementedError("Subclass must implement findNode()")

  def createNode(self, name, item_type):
    """Create a node of given type. Must override."""
    raise NotImplementedError("Subclass must implement createNode()")

  def deleteNode(self, name, item_type):
    """Delete a node. Must override."""
    raise NotImplementedError("Subclass must implement deleteNode()")

  def renameNode(self, old_name, new_name, item_type):
    """Rename a node. Must override."""
    raise NotImplementedError("Subclass must implement renameNode()")

  def onSelectionChanged(self, node, item_type):
    """Called when selection changes. Override for custom behavior."""
    pass

  def getExtraKeyboardShortcuts(self):
    """Return dict of additional keyboard shortcuts. Override to add custom shortcuts.

    Format: { keycode: (handler_func, requires_selection, allowed_for_lights) }
    """
    return {}

  ##############################################
  # GPU initialization
  ##############################################

  def _onGpuInit(self, ctx):
    """Initialize GPU resources. Call super() then add custom setup."""
    # Theme
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    self.uicontext.theme_engine = lev2.ui.ThemeEngine(self.custom_db)

    # Scenegraph
    if self._sg_params is not None:
      sg_params = self._sg_params
    else:
      sg_params = VarMap()
      sg_params.SkyboxIntensity = 2.0
      sg_params.DiffuseIntensity = 1.0
      sg_params.SpecularIntensity = 1.0
      sg_params.AmbientLevel = vec3(0.15)
      sg_params.preset = "ForwardPBR"
      sg_params.ssaa = 4
      sg_params.enable_skybox = False
      sg_params.clearcolor = vec3(0.08, 0.08, 0.1)

    self.scenegraph = lev2.scenegraph.Scene(sg_params)
    self.layer = self.scenegraph.createLayer("std_forward")
    self.scenegraph.enablePickHud()

    # Grid
    self.grid_data = createGridData()
    self.grid_node = self.layer.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1
    self.grid_node.pickable = False

    # Manipulator
    self.manip_controller = lev2.ManipController()
    self.manip_interface = None
    self.manip_controller.mode = lev2.ManipMode.TRANSLATE
    self.manip_enabled = False

    self.gizmo_data = lev2.ManipGizmoDrawableData()
    self.gizmo_data.controller = self.manip_controller
    self.gizmo_drawable = self.gizmo_data.createDrawable()
    self.gizmo_node = self.scenegraph.createDrawableNodeOnLayers([self.layer], "manip-gizmo", self.gizmo_drawable)
    self.gizmo_node.sortkey = 999
    self.gizmo_node.pickable = False
    self.gizmo_node.enabled = False

    # Camera
    self.camname = "EditorCamera"
    self.cameralut = lev2.CameraDataLut()
    self.camera, self.uicam = setupUiCameraX(cameralut=self.cameralut, camname=self.camname)
    self.uicam.distance = 1
    self.uicam.lookAt(vec3(8, 6, 8), vec3(0, 0, 0), vec3(0, 1, 0))
    self.uicam.updateMatrices()
    self.camera.copyFrom(self.uicam.cameradata)

    # Setup outliner (subclass provides model via _createOutlinerModel)
    self.scene_model = self._createOutlinerModel()
    self.outliner.model = self.scene_model
    self.outliner.expandAll()
    self._setupOutlinerCallbacks()

    # Viewport setup
    self.sgv.cameraName = self.camname
    self.sgv.scenegraph = self.scenegraph
    self.sgv.camera_evhandler = lambda ev: self._onCameraEvent(ev)
    self.sgv.bindManipController(self.manip_controller)
    self.sgv.forkDB()
    self.scenegraph.lightingmanager.gpuInit(ctx)

    # Pick visualization
    self.pickid_viz_mtl = lev2.FreestyleMaterial()
    self.pickid_viz_mtl.gpuInit(ctx, "orkshader://ui_pickid_viz")
    permu = lev2.FxPipelinePermutation()
    permu.technique = self.pickid_viz_mtl.shader.technique("pickid_viz")
    self.pickid_viz_pipeline = self.pickid_viz_mtl.fxcache.findPipeline(permu)
    self.pickid_viz_pipeline.sharedMaterial = self.pickid_viz_mtl
    self.pickid_viz_pipeline.bindParam(self.pickid_viz_mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    self.p_colormap = self.pickid_viz_mtl.param("ColorMap")
    self.p_nrmmap = self.pickid_viz_mtl.param("NrmMap")
    self.pick_views[0].pipeline = self.pickid_viz_pipeline

  def _createOutlinerModel(self):
    """Create the outliner model. Override to use custom model class."""
    from .outliner_model import SceneOutlinerModel

    editor = self

    class DefaultOutlinerModel(SceneOutlinerModel):
      def getNodeTypes(self):
        return editor.getNodeTypes()

      def findNode(self, name, item_type):
        return editor.findNode(name, item_type)

      def createNode(self, name, item_type):
        return editor.createNode(name, item_type)

      def deleteNode(self, name, item_type):
        return editor.deleteNode(name, item_type)

      def renameNode(self, old_name, new_name, item_type):
        return editor.renameNode(old_name, new_name, item_type)

    return DefaultOutlinerModel(self)

  def _setupOutlinerCallbacks(self):
    """Set up outliner selection/rename/delete callbacks."""

    def on_select(key):
      name = key.split("/")[-1] if "/" in key else key
      node_types = self.getNodeTypes()

      for nt in node_types:
        if key.startswith(nt["key"] + "/"):
          node = self.findNode(name, nt["item_type"])
          if node:
            if "drawable_type" in nt:
              self._selectDrawableNode(node)
            elif "light_type" in nt:
              self._selectLightNode(node)
          return

      # Deselect if clicking on group or empty
      self._clearSelection()
      self._enableManip(False)

    def on_rename(old_key, new_name):
      self.scene_model.renameItem(old_key, new_name)

    def on_delete(key):
      self.scene_model.removeItem(key)

    def on_add(key):
      pass  # Model handles creation

    self.outliner.onSelect(on_select)
    self.outliner.onRename(on_rename)
    self.outliner.onDelete(on_delete)
    self.outliner.onAdd(on_add)

  ##############################################
  # Event handling
  ##############################################

  def _updatePickDebugViews(self):
    """Update pick debug visualization."""
    SG = self.scenegraph
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
    """Decode pick result and return the picked node, or None."""
    from orkengine.core import u32vec4
    obj = pfc.value(0)
    if obj is not None and isinstance(obj, u32vec4):
      pick_id = int(obj.x)
      is_valid = (pick_id > 0) or (pick_id == 0 and int(obj.w) == 0)
      if is_valid:
        decoded = pfc.decodePickID(pick_id)
        node_types = self.getNodeTypes()
        for nt in node_types:
          if "drawable_type" in nt:
            for node in self.scenegraph.drawableNodesWithType(nt["drawable_type"]):
              if node is decoded:
                return node, nt
    return None, None

  def _onPickResult(self, pfc):
    """Handle pick result from 3D viewport."""
    self._updatePickDebugViews()
    node, nt = self._decodePickedNode(pfc)
    if node:
      self._selectDrawableNode(node)
      self.outliner.selected_key = f"{nt['key']}/{node.name}"

  def _onFocusPickResult(self, pfc):
    """Handle pick result for focus-only (no selection change)."""
    self._updatePickDebugViews()
    node, nt = self._decodePickedNode(pfc)
    if node:
      self._focusOnTarget(node.worldTransform.translation)

  def _focusOnTarget(self, target):
    """Focus camera on a target position."""
    eye = self.camera.eye
    offset = eye - self.camera.target
    dist = offset.length
    direction = offset.normalized if dist > 0.001 else vec3(0, 0, 1)
    self.uicam.lookAt(target + direction * dist, target, vec3(0, 1, 0))
    self.uicam.updateMatrices()
    self.camera.copyFrom(self.uicam.cameradata)

  def _onCameraEvent(self, uievent):
    """Handle camera/viewport events."""
    # Track mouse position for focus-pick
    if uievent.code == tokens.MOVE.hashed:
      self._last_mouse_x = uievent.x
      self._last_mouse_y = uievent.y

    if uievent.code == tokens.PUSH.hashed:
      self._last_mouse_x = uievent.x
      self._last_mouse_y = uievent.y
      if not uievent.shift and not uievent.ctrl and not uievent.alt:
        if not (self.manip_enabled and self.manip_controller.hoveredAxis != lev2.ManipAxis.NONE):
          vp_x = self.viewport_dock.x + self.sgv.x
          vp_y = self.viewport_dock.y + self.sgv.y
          local_x, local_y = uievent.x - vp_x, uievent.y - vp_y
          if 0 <= local_x < self.sgv.width and 0 <= local_y < self.sgv.height:
            gizmo_was = self.gizmo_node.enabled
            self.gizmo_node.enabled = False
            self.scenegraph.pickWithScreenCoord(self.camera, vec2(local_x, local_y), 0, 0, self.sgv.width, self.sgv.height, self._onPickResult)
            self.gizmo_node.enabled = gizmo_was

    if uievent.code == tokens.KEY_DOWN.hashed:
      kc = uievent.keycode

      # Standard shortcuts
      if kc == 256:  # ESC
        if self.manip_enabled:
          self._enableManip(False)
        else:
          self._clearSelection(clear_outliner=True)
        return lev2.ui.HandlerResult()

      elif kc == ord("T"):
        self._enableManip(True)
        if self.manip_controller.mode == lev2.ManipMode.TRANSLATE:
          self.manip_controller.space = lev2.ManipSpace.WORLD if self.manip_controller.space == lev2.ManipSpace.LOCAL else lev2.ManipSpace.LOCAL
        else:
          self.manip_controller.mode = lev2.ManipMode.TRANSLATE
        return lev2.ui.HandlerResult()

      elif kc == ord("R"):
        if self.selected_light is None:
          self._enableManip(True)
          self.manip_controller.mode = lev2.ManipMode.ROTATE
        return lev2.ui.HandlerResult()

      elif kc == ord("S"):
        if self.selected_light is None:
          self._enableManip(True)
          self.manip_controller.mode = lev2.ManipMode.SCALE
        return lev2.ui.HandlerResult()

      elif kc == ord("F"):
        # Focus on object under cursor (pick without changing selection)
        vp_x = self.viewport_dock.x + self.sgv.x
        vp_y = self.viewport_dock.y + self.sgv.y
        local_x = self._last_mouse_x - vp_x
        local_y = self._last_mouse_y - vp_y
        if 0 <= local_x < self.sgv.width and 0 <= local_y < self.sgv.height:
          gizmo_was = self.gizmo_node.enabled
          self.gizmo_node.enabled = False
          self.scenegraph.pickWithScreenCoord(self.camera, vec2(local_x, local_y), 0, 0, self.sgv.width, self.sgv.height, self._onFocusPickResult)
          self.gizmo_node.enabled = gizmo_was
        return lev2.ui.HandlerResult()

      elif kc == ord("N") and uievent.super:
        # Command-N: New scene
        self._newScene()
        return lev2.ui.HandlerResult()

      elif kc == ord("N") and uievent.shift:
        # Shift-N: Create new node with unique name
        node_types = self.getNodeTypes()
        for nt in node_types:
          if "drawable_type" in nt:
            # Generate unique name
            idx = len(self.scenegraph.drawableNodesWithType(nt["drawable_type"]))
            name = f"node{idx}"
            while self.findNode(name, nt["item_type"]) is not None:
              idx += 1
              name = f"node{idx}"
            self.createNode(name, nt["item_type"])
            self.scene_model.notifyItemAdded(f"{nt['key']}/{name}")
            # Select the new node
            node = self.findNode(name, nt["item_type"])
            if node:
              self._selectDrawableNode(node)
              self.outliner.selected_key = f"{nt['key']}/{name}"
            break
        return lev2.ui.HandlerResult()

      elif kc == ord("L") and uievent.shift:
        # Create new light with unique name
        node_types = self.getNodeTypes()
        for nt in node_types:
          if "light_type" in nt:
            # Generate unique name
            idx = len(self.scenegraph.lightNodesWithType(nt["light_type"]))
            name = f"pl{idx}"
            while self.findNode(name, nt["item_type"]) is not None:
              idx += 1
              name = f"pl{idx}"
            self.createNode(name, nt["item_type"])
            self.scene_model.notifyItemAdded(f"{nt['key']}/{name}")
            # Select the new light
            node = self.findNode(name, nt["item_type"])
            if node:
              self._selectLightNode(node)
              self.outliner.selected_key = f"{nt['key']}/{name}"
            break
        return lev2.ui.HandlerResult()

      elif kc == ord("P") and uievent.shift:
        # Shift-P: Create new particle system with unique name
        node_types = self.getNodeTypes()
        for nt in node_types:
          if nt.get("item_type") == "particle":
            # Generate unique name
            idx = len(self.scenegraph.drawableNodesWithType(nt["drawable_type"]))
            name = f"ptc{idx}"
            while self.findNode(name, nt["item_type"]) is not None:
              idx += 1
              name = f"ptc{idx}"
            self.createNode(name, nt["item_type"])
            self.scene_model.notifyItemAdded(f"{nt['key']}/{name}")
            # Select the new particle node
            node = self.findNode(name, nt["item_type"])
            if node:
              self._selectDrawableNode(node)
              self.outliner.selected_key = f"{nt['key']}/{name}"
            break
        return lev2.ui.HandlerResult()

      elif kc == ord("P") and uievent.alt:
        # Option-P: Cycle particle preset on selected particle node
        if self.selected_node is not None:
          preset = getattr(self.selected_node.user, 'particle_preset', None)
          if preset is not None:
            self.onCycleParticlePreset(self.selected_node)
        return lev2.ui.HandlerResult()

      # Check extra shortcuts from subclass
      extra = self.getExtraKeyboardShortcuts()
      if kc in extra:
        handler, requires_selection, allowed_for_lights = extra[kc]
        if requires_selection:
          if self.selected_node is not None:
            handler(self.selected_node)
          elif self.selected_light is not None and allowed_for_lights:
            handler(self.selected_light)
        else:
          handler(None)
        return lev2.ui.HandlerResult()

    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################
  # Update loop
  ##############################################

  def _onUpdate(self, updinfo):
    """Update loop. Call super() then add custom update logic."""
    import math
    self.scenegraph.updateScene(self.cameralut)
    self.sgv.setDirty()

    if self.selected_node is not None:
      self.xform_editor.sync()
      t = math.sin(updinfo.absolutetime * 8.0) * 0.5 + 0.5
      self.selected_node.modcolor = vec4(1, t, t, 1)
    elif self.selected_light is not None:
      self.xform_editor.sync()
      xform = self.selected_light.worldTransform
      self.selected_light.setMatrix(xform.composed)

  def _onUiEvent(self, uievent):
    """UI event handler. Override for custom behavior."""
    return None
