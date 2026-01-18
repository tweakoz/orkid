#!/usr/bin/env ork.python

################################################################################
# Scene Editor Mockup Test
# Layout using lg.split() for draggable anchor guides:
#   - Main hsplit: left panel (25%) | viewport (75%)
#   - Left vsplit: outliner (40%) / property sheet (60%)
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math, sys, signal, random, os, json
from orkengine.core import vec2, vec3, vec4, quat, VarMap, CrcStringProxy, Transform
from orkengine import lev2
from ork.ui import icon_library
from ork.ui.filesystem_browser import FilesystemBrowser
from ork.ui.transform_edit import TransformEdit

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
    """Setup 16 cubes in outliner."""
    self.cube_names = [f"Cube_{i:02d}" for i in range(16)]

    scene_data = VarMap()
    cubes = VarMap()
    for name in self.cube_names:
      setattr(cubes, name, "cube")
    scene_data.Cubes = cubes

    self.outliner.data = scene_data
    self.outliner.expandAll()

    def on_select(key):
      name = key.split("/")[-1] if "/" in key else key
      if name in self.cube_names:
        self._selectCube(name)

    self.outliner.onSelect(on_select)
    self.outliner.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.outliner.item_height = 22

  ##############################################

  def _setupPropertySheet(self):
    """Setup property sheet with TransformEdit."""
    self.propsheet_vpack = self.propsheet_dock.createChild(
      uiclass=lev2.ui.VerticalPack,
      args=["propsheet_content"]
    )
    self.propsheet_vpack.margin = 4
    self.propsheet_vpack.item_height = 84

    self.xform_edit_vpack = self.propsheet_vpack.makeChild(
      uiclass=TransformEdit,
      args=["xform_edit"]
    )
    self.xform_editor = self.xform_edit_vpack.uservars.transform_edit

  ##############################################

  def _selectCube(self, name):
    """Select a cube - bind manip and property sheet to its transform."""
    if not hasattr(self, 'cube_transforms') or name not in self.cube_transforms:
      return
    xform = self.cube_transforms[name]
    self.manip_interface = lev2.DecompTransformManipulator(xform)
    self.manip_controller.target = self.manip_interface
    self.xform_editor.transform = xform
    self.selected_cube = name
    self._enableManip(True)
    print(f"Selected: {name}")

  ##############################################

  def _openLoadPopup(self):
    """Open a file browser popup for loading a scene."""
    self._openFileBrowserPopup("Load Scene", self._onLoadFileSelected, "load")

  def _openSavePopup(self):
    """Open a file browser popup for saving a scene."""
    self._openFileBrowserPopup("Save Scene", self._onSaveFileSelected, "save")

  def _openFileBrowserPopup(self, title, on_file_selected, mode):
    """Create a secondary window with FilesystemBrowser."""
    popup_win = self.ezapp.createSecondaryWindow(
      width=800, height=600, x=200, y=150,
      title=title, decorated=True, resizable=True
    )

    uic = popup_win.ui_context
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, popup_win.width, popup_win.height)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["browser", home_dir, ".json", vec3(0.1, 0.1, 0.1), mode],
      fill=True
    )
    browser = browser_item.widget.uservars.filesystem_browser

    def on_activate(path):
      on_file_selected(path)
      popup_win.requestClose()

    def on_cancel():
      popup_win.requestClose()

    browser.onActivate = on_activate
    browser.onCancel = on_cancel

    if not hasattr(self, '_popup_windows'):
      self._popup_windows = []
    self._popup_windows.append(popup_win)

  def _onLoadFileSelected(self, path):
    """Load scene from JSON file."""
    if not hasattr(self, 'cube_transforms'):
      return
    try:
      with open(path, 'r') as f:
        data = json.load(f)
      for name, xd in data.get("cubes", {}).items():
        if name in self.cube_transforms:
          xform = self.cube_transforms[name]
          t = xd.get("translation", [0, 0.5, 0])
          xform.translation = vec3(t[0], t[1], t[2])
          o = xd.get("orientation", [0, 0, 0, 1])
          xform.orientation = quat(o[0], o[1], o[2], o[3])
          xform.scale = xd.get("scale", 1.0)
      print(f"Loaded scene from: {path}")
    except Exception as e:
      print(f"Failed to load: {e}")

  def _onSaveFileSelected(self, path):
    """Save scene to JSON file."""
    if not hasattr(self, 'cube_transforms'):
      return
    if not path.endswith('.json'):
      path += '.json'
    data = {"cubes": {}}
    for name, xform in self.cube_transforms.items():
      t, o = xform.translation, xform.orientation
      data["cubes"][name] = {
        "translation": [t.x, t.y, t.z],
        "orientation": [o.x, o.y, o.z, o.w],
        "scale": xform.scale
      }
    with open(path, 'w') as f:
      json.dump(data, f, indent=2)
    print(f"Saved scene to: {path}")

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

    # Create 16 cubes in 4x4 grid, spaced 2m apart
    cube_prim = createCubePrim(ctx=ctx, size=1.0)
    pipeline_cube = createPipeline(app=self, ctx=ctx, rendermodel="FORWARD_PBR", techname="std_mono_fwd")

    self.cube_nodes = {}
    self.cube_transforms = {}
    for i, name in enumerate(self.cube_names):
      node = cube_prim.createNode(name, self.layer, pipeline_cube)
      xform = Transform()
      xform.translation = vec3((i % 4) * 2 - 3, 0.5, (i // 4) * 2 - 3)
      node.worldTransform = xform
      self.cube_nodes[name] = node
      self.cube_transforms[name] = xform

    # Setup ManipController (initially no target)
    self.manip_controller = lev2.ManipController()
    self.manip_interface = None
    self.manip_controller.mode = lev2.ManipMode.TRANSLATE
    self.selected_cube = None

    # Create ManipGizmo drawable (renders in scenegraph)
    self.gizmo_data = lev2.ManipGizmoDrawableData()
    self.gizmo_data.controller = self.manip_controller
    self.gizmo_drawable = self.gizmo_data.createDrawable()
    self.gizmo_node = self.scenegraph.createDrawableNodeOnLayers(
        [self.layer], "manip-gizmo", self.gizmo_drawable)
    self.gizmo_node.sortkey = 999  # Render on top

    # Start with manipulator disabled (no selection yet)
    self.manip_enabled = False
    self.gizmo_node.enabled = False

    ############################################
    # Setup camera
    ############################################

    self.camname = "EditorCamera"
    self.cameralut = lev2.CameraDataLut()
    self.camera, self.uicam = setupUiCameraX(cameralut=self.cameralut, camname=self.camname)

    self.uicam.distance = 1
    self.uicam.lookAt(vec3(12, 10, 12), vec3(0, 0, 0), vec3(0, 1, 0))
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

    print("Scene Editor Ready - Select a cube in the outliner")

  ##############################################

  def _enableManip(self, enable):
    """Enable or disable manipulator rendering and input."""
    self.manip_enabled = enable
    self.gizmo_node.enabled = enable
    if enable:
      if self.manip_interface:
        self.manip_controller.target = self.manip_interface
    else:
      self.manip_controller.target = None

  def _onCameraEvent(self, uievent):
    """Handle camera manipulation events (orbit, pan, zoom) and mode keys."""
    if uievent.code == tokens.KEY_DOWN.hashed:
      # Escape - disable manipulator
      if uievent.keycode == 256:
        self._enableManip(False)
        print("Manipulator disabled (press T/R/S to enable)")
        return lev2.ui.HandlerResult()
      # T/R/S - enable and set mode
      elif uievent.keycode == ord("T"):
        self._enableManip(True)
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
        self._enableManip(True)
        self.manip_controller.mode = lev2.ManipMode.ROTATE
        print("Mode: ROTATE")
        return lev2.ui.HandlerResult()
      elif uievent.keycode == ord("S"):
        self._enableManip(True)
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
    self.scenegraph.updateScene(self.cameralut)
    self.sgv.setDirty()
    if self.selected_cube:
      self.xform_editor.sync()

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

SceneEditorTest().ezapp.mainThreadLoop()
