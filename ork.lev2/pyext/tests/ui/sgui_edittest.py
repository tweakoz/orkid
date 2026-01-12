#!/usr/bin/env ork.python

################################################################################
# Scene Editor Mockup Test
# Layout using lg.split() for draggable anchor guides:
#   - Main hsplit: left panel (25%) | viewport (75%)
#   - Left vsplit: outliner (40%) / property sheet (60%)
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math, sys, signal, random, os
from orkengine.core import vec2, vec3, vec4, quat, VarMap, CrcStringProxy, Transform
from orkengine import lev2
from ork.ui import icon_library
from ork.ui.filesystem_browser import FilesystemBrowser

tokens = CrcStringProxy()
home_dir = os.path.expanduser("~")

################################################################################

l2exdir = (lev2.lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir)
from lev2utils.cameras import setupUiCameraX
from lev2utils.shaders import createPipeline
from lev2utils.primitives import createGridData, createCubePrim

################################################################################

class SceneEditorTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=True, name="SceneEditorTest")
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.15, 0.15, 0.15, 1)
    lg.clearColorGuide = vec4(0.4, 0.4, 0.2, 1)  # Yellow-ish guides

    ############################################
    # Start with viewport DockablePanel filling whole area
    ############################################

    viewport_dock_item = lg.makeChild(
      fill=True,
      margin=2,
      uiclass=lev2.ui.DockablePanel,
      args=["viewport_dock"]
    )
    self.viewport_dock = viewport_dock_item.widget
    self.viewport_dock.titlebar_color = vec4(0.15, 0.2, 0.25, 1)

    # Create viewport as child of dock panel
    self.sgv = self.viewport_dock.createChild(
      uiclass=lev2.ui.SceneGraphViewport,
      args=["Viewport", vec4(0.1, 0.1, 0.12, 1)]
    )

    ############################################
    # Split LEFT from viewport to create outliner dock
    # (25% left for outliner, 75% right for viewport)
    ############################################

    left_dock_item = lg.split(
      layout=viewport_dock_item.layout,
      proportion=0.25,
      placement=tokens.LEFT,
      margin=2,
      uiclass=lev2.ui.DockablePanel,
      args=["left_dock"]
    )
    self.left_dock = left_dock_item.widget
    self.left_dock.titlebar_color = vec4(0.2, 0.15, 0.2, 1)

    # Create left panel (vpack with toolbar + outliner) as child
    self.left_panel = self.left_dock.createChild(
      uiclass=lev2.ui.VerticalPack,
      args=["Scene"]
    )
    self.left_panel.margin = 2
    self.left_panel.item_height = 36  # Height for toolbar row

    # Add toolbar hpack to top of vpack
    self.toolbar_hpack = self.left_panel.makeChild(uiclass=lev2.ui.HorizontalPack, args=["toolbar"])
    self.toolbar_hpack.margin = 2
    self.toolbar_hpack.item_width = 48
    self.toolbar_hpack.bg_color = vec4(0.12, 0.12, 0.15, 1)

    # Create text icons for LOAD and SAVE (like HOLD/CLR in FilesystemBrowser)
    def make_text_icon(text, color="#E6E6E6"):
      return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
        <text x="12" y="12" text-anchor="middle" dominant-baseline="central" font-family="sans-serif" font-size="7" font-weight="bold" fill="{color}">{text}</text>
      </svg>'''

    icon_size = 32
    icon_load = icon_library.from_svg_string(make_text_icon("LOAD"), icon_size, icon_size)
    icon_save = icon_library.from_svg_string(make_text_icon("SAVE"), icon_size, icon_size)

    # Add Load button
    self.btn_load = self.toolbar_hpack.makeChild(uiclass=lev2.ui.ImageButton, args=["btn_load"])
    self.btn_load.inactive_image = icon_load
    self.btn_load.bgcolor = vec4(0.15, 0.25, 0.15, 1)
    self.btn_load.inactive_blend_mode = tokens.ALPHA
    self.btn_load.onPressed = lambda btn: self._openLoadPopup()

    # Add Save button
    self.btn_save = self.toolbar_hpack.makeChild(uiclass=lev2.ui.ImageButton, args=["btn_save"])
    self.btn_save.inactive_image = icon_save
    self.btn_save.bgcolor = vec4(0.15, 0.15, 0.25, 1)
    self.btn_save.inactive_blend_mode = tokens.ALPHA
    self.btn_save.onPressed = lambda btn: self._openSavePopup()

    # Add outliner below toolbar (fills remaining space)
    self.outliner_item = self.left_panel.makeChild(uiclass=lev2.ui.Outliner, args=["outliner"])
    self.outliner = self.outliner_item
    self.left_panel.fill_widget = self.outliner  # Outliner fills remaining space

    ############################################
    # Split BOTTOM from left dock to create property sheet dock
    # (60% bottom for propsheet, 40% top for left panel)
    ############################################

    propsheet_dock_item = lg.split(
      layout=left_dock_item.layout,
      proportion=0.6,
      placement=tokens.BOTTOM,
      margin=2,
      uiclass=lev2.ui.DockablePanel,
      args=["propsheet_dock"]
    )
    self.propsheet_dock = propsheet_dock_item.widget
    self.propsheet_dock.titlebar_color = vec4(0.2, 0.2, 0.15, 1)

    # Create property sheet as child of dock panel
    self.propsheet = self.propsheet_dock.createChild(
      uiclass=lev2.ui.PropertySheet,
      args=["Properties"]
    )

    ############################################
    # Setup outliner data (mock scene hierarchy)
    ############################################

    self._setupOutliner()
    self._setupPropertySheet()

    ############################################

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def _setupOutliner(self):
    """Setup mock scene hierarchy in outliner."""
    scene_data = VarMap()

    # Scene root
    root = VarMap()

    # Cameras
    cameras = VarMap()
    cameras.MainCamera = "PerspectiveCamera"
    cameras.TopCamera = "OrthographicCamera"
    root.Cameras = cameras

    # Lights
    lights = VarMap()
    lights.DirectionalLight = "Sun"
    lights.PointLight1 = "Fill"
    lights.PointLight2 = "Rim"
    root.Lights = lights

    # Objects
    objects = VarMap()

    player = VarMap()
    player.Mesh = "player_mesh"
    player.Material = "player_mat"
    player.Collider = "capsule"
    objects.Player = player

    ground = VarMap()
    ground.Mesh = "ground_mesh"
    ground.Material = "ground_mat"
    objects.Ground = ground

    props = VarMap()
    props.Tree1 = "tree_prefab"
    props.Tree2 = "tree_prefab"
    props.Rock1 = "rock_prefab"
    objects.Props = props

    root.Objects = objects

    scene_data.Scene = root

    self.outliner.data = scene_data
    self.outliner.model.allow_rename = True
    self.outliner.model.allow_delete = True
    self.outliner.model.allow_add = True
    self.outliner.model.allow_multiselect = True
    self.outliner.expandAll()

    # Selection callback - update property sheet when selection changes
    def on_select(key):
      print(f"Selected: {key}")
      self._updatePropertySheetForSelection(key)

    self.outliner.onSelect(on_select)

    # Style
    self.outliner.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.outliner.text_color = vec4(0.9, 0.9, 0.9, 1)
    self.outliner.selected_color = vec4(0.2, 0.4, 0.6, 1)
    self.outliner.hover_color = vec4(0.2, 0.2, 0.25, 1)
    self.outliner.item_height = 22

  ##############################################

  def _setupPropertySheet(self):
    """Setup property sheet with default data."""
    data = VarMap()

    transform = VarMap()
    transform.position_x = 0.0
    transform.position_y = 0.0
    transform.position_z = 0.0
    transform.rotation_x = 0.0
    transform.rotation_y = 0.0
    transform.rotation_z = 0.0
    transform.scale_x = 1.0
    transform.scale_y = 1.0
    transform.scale_z = 1.0
    data.Transform = transform

    data.Name = "Selected Object"
    data.Visible = True
    data.Layer = "Default"

    self.propsheet.data = data
    self.propsheet.expandAll()

    def on_property_changed(key, value):
      print(f"Property changed: {key} = {value}")

    self.propsheet.onPropertyChanged(on_property_changed)

    # Style
    self.propsheet.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.propsheet.label_color = vec4(0.85, 0.85, 0.85, 1)
    self.propsheet.group_color = vec4(0.16, 0.16, 0.2, 1)
    self.propsheet.row_height = 26
    self.propsheet.label_width = 100

  ##############################################

  def _updatePropertySheetForSelection(self, key):
    """Update property sheet based on outliner selection."""
    data = VarMap()

    transform = VarMap()
    transform.position_x = random.uniform(-10, 10)
    transform.position_y = random.uniform(0, 5)
    transform.position_z = random.uniform(-10, 10)
    transform.rotation_x = 0.0
    transform.rotation_y = random.uniform(0, 360)
    transform.rotation_z = 0.0
    transform.scale_x = 1.0
    transform.scale_y = 1.0
    transform.scale_z = 1.0
    data.Transform = transform

    # Extract name from key path
    name = key.split("/")[-1] if "/" in key else key
    data.Name = name
    data.Visible = True
    data.Layer = "Default"

    self.propsheet.data = data
    self.propsheet.expandAll()

  ##############################################

  def _syncPropertySheetFromTransform(self):
    """Sync property sheet with cube transform values."""
    if not hasattr(self, 'cube_transform'):
      return

    pos = self.cube_transform.translation
    rot = self.cube_transform.orientation
    scale = self.cube_transform.scale

    # Check if changed
    cur = (pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, rot.w, scale)
    if hasattr(self, '_last_transform') and self._last_transform == cur:
      return
    self._last_transform = cur

    # Convert quaternion to angle-axis
    import math
    # Angle from quaternion: angle = 2 * acos(w)
    # Clamp w to [-1, 1] to avoid numerical issues
    w_clamped = max(-1.0, min(1.0, rot.w))
    angle = 2.0 * math.acos(w_clamped)

    # Axis from quaternion: axis = (x, y, z) / sin(angle/2)
    sin_half = math.sin(angle / 2.0)
    if abs(sin_half) > 1e-6:
      axis_x = rot.x / sin_half
      axis_y = rot.y / sin_half
      axis_z = rot.z / sin_half
    else:
      # Near zero rotation, axis is arbitrary
      axis_x = 1.0
      axis_y = 0.0
      axis_z = 0.0

    data = VarMap()
    transform = VarMap()
    transform.position_x = pos.x
    transform.position_y = pos.y
    transform.position_z = pos.z
    transform.angle = math.degrees(angle)
    transform.axis_x = axis_x
    transform.axis_y = axis_y
    transform.axis_z = axis_z
    transform.scale_x = scale
    transform.scale_y = scale
    transform.scale_z = scale
    data.Transform = transform
    data.Name = "Cube"
    data.Visible = True
    data.Layer = "Default"

    # Set annotations for slider ranges (before setting data)
    if not hasattr(self, '_propsheet_annotations_set'):
      self._propsheet_annotations_set = True
      model = self.propsheet.model
      pos_annot = VarMap()
      pos_annot.min = -10.0
      pos_annot.max = 10.0
      model.setAnnotations("Transform/position_x", pos_annot)
      model.setAnnotations("Transform/position_y", pos_annot)
      model.setAnnotations("Transform/position_z", pos_annot)

      angle_annot = VarMap()
      angle_annot.min = -360.0
      angle_annot.max = 360.0
      model.setAnnotations("Transform/angle", angle_annot)

      axis_annot = VarMap()
      axis_annot.min = -1.0
      axis_annot.max = 1.0
      model.setAnnotations("Transform/axis_x", axis_annot)
      model.setAnnotations("Transform/axis_y", axis_annot)
      model.setAnnotations("Transform/axis_z", axis_annot)

      scale_annot = VarMap()
      scale_annot.min = 0.1
      scale_annot.max = 10.0
      model.setAnnotations("Transform/scale_x", scale_annot)
      model.setAnnotations("Transform/scale_y", scale_annot)
      model.setAnnotations("Transform/scale_z", scale_annot)

    self.propsheet.data = data

  ##############################################

  def _openLoadPopup(self):
    """Open a file browser popup for loading a scene."""
    print("Opening Load popup...")
    self._openFileBrowserPopup("Load Scene", self._onLoadFileSelected)

  def _openSavePopup(self):
    """Open a file browser popup for saving a scene."""
    print("Opening Save popup...")
    self._openFileBrowserPopup("Save Scene", self._onSaveFileSelected)

  def _openFileBrowserPopup(self, title, on_file_selected):
    """Create a secondary window with FilesystemBrowser."""
    # Create secondary window for file browser
    popup_win = self.ezapp.createSecondaryWindow(
      width=800,
      height=600,
      x=200,
      y=150,
      title=title,
      decorated=True,
      resizable=True
    )

    # Set up UI on secondary window
    uic = popup_win.ui_context
    win_w = popup_win.width
    win_h = popup_win.height

    # Create root layout group
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, win_w, win_h)
    uic.top = root
    root.margin = 4

    # Create FilesystemBrowser filling the popup
    browser_item = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["browser", home_dir, ""],
      fill=True
    )
    browser = browser_item.widget.uservars.filesystem_browser

    # Wire up file activation callback
    def on_activate(path):
      print(f"File activated: {path}")
      on_file_selected(path)
      # Close the popup window
      popup_win.requestClose()

    browser.onActivate = on_activate

    # Store reference to prevent garbage collection
    if not hasattr(self, '_popup_windows'):
      self._popup_windows = []
    self._popup_windows.append(popup_win)

  def _onLoadFileSelected(self, path):
    """Handle file selection from Load popup."""
    print(f"Loading scene from: {path}")
    # TODO: Implement actual scene loading

  def _onSaveFileSelected(self, path):
    """Handle file selection from Save popup."""
    print(f"Saving scene to: {path}")
    # TODO: Implement actual scene saving

  ##############################################

  def onGpuInit(self, ctx):
    # Setup theme engine
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

    ############################################
    # Setup scenegraph
    ############################################

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

    # Grid
    self.grid_data = createGridData()
    self.grid_node = self.layer.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

    # Cube with Transform for manipulation
    cube_prim = createCubePrim(ctx=ctx, size=1.0)
    pipeline_cube = createPipeline(app=self, ctx=ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd")
    self.cube_node = cube_prim.createNode("cube", self.layer, pipeline_cube)

    # Create a Transform for manipulation
    self.cube_transform = Transform()
    self.cube_transform.translation = vec3(0, 0.5, 0)
    self.cube_transform.orientation = quat()
    self.cube_transform.scale = 1.0
    self.cube_node.worldTransform = self.cube_transform

    ############################################
    # Setup ManipController and Gizmo
    ############################################

    self.manip_controller = lev2.ManipController()
    self.manip_interface = lev2.DecompTransformManipulator(self.cube_transform)
    self.manip_controller.target = self.manip_interface
    self.manip_controller.mode = lev2.ManipMode.TRANSLATE

    # Create ManipGizmo drawable (renders in scenegraph)
    self.gizmo_data = lev2.ManipGizmoDrawableData()
    self.gizmo_data.controller = self.manip_controller
    self.gizmo_drawable = self.gizmo_data.createDrawable()
    self.gizmo_node = self.scenegraph.createDrawableNodeOnLayers(
        [self.layer], "manip-gizmo", self.gizmo_drawable)
    self.gizmo_node.sortkey = 999  # Render on top

    ############################################
    # Setup camera
    ############################################

    self.camname = "EditorCamera"
    self.cameralut = lev2.CameraDataLut()
    self.camera, self.uicam = setupUiCameraX(cameralut=self.cameralut, camname=self.camname)

    self.uicam.distance = 1
    self.uicam.lookAt(vec3(5, 4, 5), vec3(0, 0.5, 0), vec3(0, 1, 0))
    self.uicam.updateMatrices()
    self.camera.copyFrom(self.uicam.cameradata)

    ############################################
    # Assign scenegraph to viewport
    ############################################

    self.sgv.cameraName = self.camname
    self.sgv.scenegraph = self.scenegraph
    self.sgv.camera_evhandler = lambda ev: self._onCameraEvent(ev)
    self.sgv.bindManipController(self.manip_controller)
    self.sgv.forkDB()
    self.scenegraph.lightingmanager.gpuInit(ctx)

    print("Scene Editor Ready")
    print("  T - Translate mode (press again to toggle LOCAL/WORLD)")
    print("  R - Rotate mode")
    print("  S - Scale mode")

  ##############################################

  def _onCameraEvent(self, uievent):
    """Handle camera manipulation events (orbit, pan, zoom) and mode keys."""
    # Check for mode switching keys first
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == ord("T"):
        if self.manip_controller.mode == lev2.ManipMode.TRANSLATE:
          if self.manip_controller.space == lev2.ManipSpace.LOCAL:
            self.manip_controller.space = lev2.ManipSpace.WORLD
            print("Mode: TRANSLATE (WORLD)")
          else:
            self.manip_controller.space = lev2.ManipSpace.LOCAL
            print("Mode: TRANSLATE (LOCAL)")
        else:
          self.manip_controller.mode = lev2.ManipMode.TRANSLATE
          space_name = "LOCAL" if self.manip_controller.space == lev2.ManipSpace.LOCAL else "WORLD"
          print(f"Mode: TRANSLATE ({space_name})")
        return lev2.ui.HandlerResult()
      elif uievent.keycode == ord("R"):
        self.manip_controller.mode = lev2.ManipMode.ROTATE
        print("Mode: ROTATE")
        return lev2.ui.HandlerResult()
      elif uievent.keycode == ord("S"):
        self.manip_controller.mode = lev2.ManipMode.SCALE
        print("Mode: SCALE")
        return lev2.ui.HandlerResult()

    # Then handle camera events
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################

  def onUpdate(self, updinfo):
    abstime = updinfo.absolutetime

    # Update scenegraph
    self.scenegraph.updateScene(self.cameralut)
    self.sgv.setDirty()

    # Sync property sheet with cube transform
    self._syncPropertySheetFromTransform()

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

SceneEditorTest().ezapp.mainThreadLoop()
