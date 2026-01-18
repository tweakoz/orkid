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
from orkengine.core import vec2, vec3, vec4, quat, mtx4, VarMap, CrcStringProxy, Transform, u32vec4
from orkengine import lev2
from ork.ui import icon_library
from ork.ui.filesystem_browser import FilesystemBrowser
from ork.ui.transform_edit import TransformEdit
from ork.ui.color_picker import ColorPicker

tokens = CrcStringProxy()
home_dir = os.path.expanduser("~")

################################################################################
# DrawablePool - manages reusable drawables for a model
################################################################################

class NodePool:
  """Pool of nodes for a single model. Nodes are created on demand and retained forever."""

  def __init__(self, model, scenegraph, layer):
    self.model = model
    self.scenegraph = scenegraph
    self.layer = layer
    self.available = []  # Disabled nodes ready for reuse
    self.in_use = []     # Currently active nodes
    self._counter = 0

  def acquire(self, base_name):
    """Get a node from pool or create new one. Returns enabled node."""
    if self.available:
      node = self.available.pop()
      #print(f"NodePool.acquire: REUSING node {node}")
    else:
      # Create new node
      drawable = self.model.createDrawable()
      node_name = f"{base_name}_m{id(self.model)}_{self._counter}"
      self._counter += 1
      node = self.scenegraph.createDrawableNodeOnLayers([self.layer], node_name, drawable)
      #print(f"NodePool.acquire: CREATED new node {node} with drawable {drawable}")
    node.enabled = True
    self.in_use.append(node)
    return node

  def release(self, node):
    """Return node to pool (disable but don't delete)."""
    if node in self.in_use:
      self.in_use.remove(node)
    node.enabled = False
    if node not in self.available:
      self.available.append(node)

################################################################################

l2exdir = (lev2.lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir)
from lev2utils.cameras import setupUiCameraX
from lev2utils.primitives import createGridData

################################################################################

class SceneEditorTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=True, ssaa=1, name="SceneEditorTest")
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

    # Add pick buffer debug views in a collapsable (small images showing what pick buffer sees)
    self.pick_collapsable = self.left_panel.makeChild(uiclass=lev2.ui.Collapsable, args=["Pick Debug"])
    self.pick_collapsable.header_height = 24
    self.pick_collapsable.header_bg_color = vec4(0.2, 0.2, 0.25, 1)
    self.pick_collapsable.expanded = False

    self.pick_hpack = lev2.ui.HorizontalPack.wfactory(["pick_debug_hpack"])
    self.pick_hpack.margin = 2
    self.pick_hpack.item_width = 128
    self.pick_hpack.fixed_height = 192
    self.pick_hpack.bg_color = vec4(0.1, 0.1, 0.1, 1)
    self.pick_collapsable.setChild(self.pick_hpack)

    imgbg = vec4(0.15, 0.15, 0.15, 1)
    self.pick_img_id = self.pick_hpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_id", imgbg])
    self.pick_img_pos = self.pick_hpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_pos", imgbg])
    self.pick_img_nrm = self.pick_hpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_nrm", imgbg])

    for imgview in [self.pick_img_id, self.pick_img_pos, self.pick_img_nrm]:
      imgview.maintain_aspect_ratio = True
      imgview.flip_x = True
      imgview.flip_y = True

    # Add outliner below pick debug (fills remaining space)
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
    """Setup 16 cubes and lights in outliner."""
    self.cube_names = [f"Cube_{i:02d}" for i in range(16)]
    self.light_names = ["pointlight1"]

    scene_data = VarMap()
    cubes = VarMap()
    for name in self.cube_names:
      setattr(cubes, name, "cube")
    scene_data.Cubes = cubes

    lights = VarMap()
    for name in self.light_names:
      setattr(lights, name, "pointlight")
    scene_data.Lights = lights

    self.outliner.data = scene_data
    self.outliner.expandAll()

    def on_select(key):
      name = key.split("/")[-1] if "/" in key else key
      if name in self.cube_names:
        self._selectCube(name)
      elif name in self.light_names:
        self._selectLight(name)
      else:
        # Selected a category header - clear selection
        if hasattr(self, 'selected_cube') and self.selected_cube and self.selected_cube in self.cube_nodes:
          self.cube_nodes[self.selected_cube].modcolor = vec4(1, 1, 1, 1)
        self.selected_cube = None
        self.selected_light = None
      # Disable manip on any outliner selection - only T/R/S keys enable it
      self._enableManip(False)

    self.outliner.onSelect(on_select)
    self.outliner.bgcolor = vec4(0.12, 0.12, 0.14, 1)
    self.outliner.item_height = 22

  ##############################################

  def _setupPropertySheet(self):
    """Setup property sheet with TransformEdit and Light Color in Collapsables."""
    self.propsheet_vpack = self.propsheet_dock.createChild(
      uiclass=lev2.ui.VerticalPack,
      args=["propsheet_content"]
    )
    self.propsheet_vpack.margin = 4
    self.propsheet_vpack.item_height = 100

    # Wrap TransformEdit in a Collapsable
    self.xform_collapsable = self.propsheet_vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["Transform"])
    self.xform_collapsable.header_height = 24
    self.xform_collapsable.header_bg_color = vec4(0.2, 0.2, 0.25, 1)
    self.xform_collapsable.expanded = True

    # Create TransformEdit as child of Collapsable
    self.xform_edit_widget = TransformEdit.wfactory(["xform_edit"])
    self.xform_edit_widget.fixed_height = 84
    self.xform_collapsable.setChild(self.xform_edit_widget)
    self.xform_editor = self.xform_edit_widget.uservars.transform_edit

    # Light Color Collapsable (only visible when light selected)
    self.color_collapsable = self.propsheet_vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["Light Color"])
    self.color_collapsable.header_height = 24
    self.color_collapsable.header_bg_color = vec4(0.25, 0.2, 0.15, 1)
    self.color_collapsable.expanded = True

    # Create VPack for color picker and intensity
    self.color_vpack = lev2.ui.VerticalPack.wfactory(["color_vpack"])
    self.color_vpack.fixed_height = 200
    self.color_vpack.margin = 2
    self.color_vpack.item_height = 24
    self.color_collapsable.setChild(self.color_vpack)

    # Color picker (normalized 0-1 color)
    self.color_picker_widget = self.color_vpack.makeChild(
      uiclass=ColorPicker,
      args=["light_color", vec3(0.3, 0.3, 0.3), vec4(1, 1, 1, 1)]
    )
    self.color_picker_widget.fixed_height = 160
    self.color_picker = self.color_picker_widget.uservars.color_picker

    # Intensity slider
    self.intensity_edit = self.color_vpack.makeChild(
      uiclass=lev2.ui.F32Edit,
      args=["intensity", "Intensity", 100.0, -1000.0, 1000.0]
    )
    self.intensity_edit.drag_rate = 1.0
    self.intensity_edit.precision = 1

    def on_light_color_changed():
      if hasattr(self, 'selected_light') and self.selected_light:
        color = self.color_picker.current_color
        intensity = self.intensity_edit.value
        self.light_data[self.selected_light].color = vec3(
          color.x * intensity,
          color.y * intensity,
          color.z * intensity
        )

    self.color_picker.onColorChanged = on_light_color_changed
    self.intensity_edit.onValueChanged(lambda val: on_light_color_changed())

    # Start with color editor collapsed (no light selected)
    self.color_collapsable.expanded = False

  ##############################################

  def _selectCube(self, name):
    """Select a cube - bind manip and property sheet to its transform."""
    if not hasattr(self, 'cube_transforms') or name not in self.cube_transforms:
      return
    # Clear previous cube selection highlight
    if hasattr(self, 'selected_cube') and self.selected_cube and self.selected_cube in self.cube_nodes:
      self.cube_nodes[self.selected_cube].modcolor = vec4(1, 1, 1, 1)
    # Clear light selection
    self.selected_light = None
    self.color_collapsable.expanded = False

    # Highlight new selection in red
    self.cube_nodes[name].modcolor = vec4(1, 0.3, 0.3, 1)

    xform = self.cube_transforms[name]
    self.manip_interface = lev2.DecompTransformManipulator(xform)
    self.manip_controller.target = self.manip_interface
    self.xform_editor.transform = xform
    self.xform_editor.position_only = False  # Cubes support full transform
    self.selected_cube = name
    #print(f"Selected cube: {name}")

  ##############################################

  def _cycleModel(self, name):
    """Cycle to the next model for the given cube."""
    if name not in self.cube_nodes or not self.model_names:
      return

    current_model = self.cube_model_name[name]
    current_idx = self.model_names.index(current_model)
    next_idx = (current_idx + 1) % len(self.model_names)
    next_model = self.model_names[next_idx]

    self._setModel(name, next_model)

  def _setModel(self, name, model_name):
    """Set a specific model for the given cube."""
    if name not in self.cube_nodes or model_name not in self.node_pools:
      return

    current_model = self.cube_model_name[name]
    if current_model == model_name:
      return  # Already using this model

    old_node = self.cube_nodes[name]
    was_selected = (self.selected_cube == name)

    # Release old node back to its pool
    old_node.modcolor = vec4(1, 1, 1, 1)
    self.node_pools[current_model].release(old_node)

    # Acquire new node from new model's pool
    new_node = self.node_pools[model_name].acquire(name)

    # Copy transform to new node
    xform = self.cube_transforms[name]
    new_node.worldTransform = xform

    # Preserve selection highlight
    if was_selected:
      new_node.modcolor = vec4(1, 0.3, 0.3, 1)

    # Update tracking
    self.cube_nodes[name] = new_node
    self.cube_model_name[name] = model_name

    #print(f"{name}: switched to model '{model_name}'")

  ##############################################

  def _selectLight(self, name):
    """Select a light - bind manip and property sheet to its transform and color."""
    if not hasattr(self, 'light_transforms') or name not in self.light_transforms:
      return
    # Clear previous cube selection highlight
    if hasattr(self, 'selected_cube') and self.selected_cube and self.selected_cube in self.cube_nodes:
      self.cube_nodes[self.selected_cube].modcolor = vec4(1, 1, 1, 1)
    self.selected_cube = None

    # Set light selection
    self.selected_light = name
    self.color_collapsable.expanded = True

    # Update color picker and intensity from light color
    # Light color = normalized_color * intensity
    color = self.light_data[name].color
    intensity = max(color.x, color.y, color.z, 0.001)  # Avoid division by zero
    normalized = vec4(color.x / intensity, color.y / intensity, color.z / intensity, 1.0)
    self.color_picker.current_color = normalized
    self.intensity_edit.value = intensity

    # Bind transform (point lights only support translation)
    xform = self.light_transforms[name]
    self.manip_interface = lev2.DecompTransformManipulator(xform)
    self.manip_controller.target = self.manip_interface
    self.manip_controller.mode = lev2.ManipMode.TRANSLATE  # Force translate for point lights
    self.xform_editor.transform = xform
    self.xform_editor.position_only = True  # Point lights only support position
    print(f"Selected light: {name}")

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
      title=title, decorated=True, resizable=True, floating=True
    )

    uic = popup_win.ui_context
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, popup_win.width, popup_win.height)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["browser", home_dir, ".osgr", vec3(0.1, 0.1, 0.1), mode],
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
      # Load cubes
      for name, xd in data.get("cubes", {}).items():
        if name in self.cube_transforms:
          xform = self.cube_transforms[name]
          t = xd.get("translation", [0, 0.5, 0])
          xform.translation = vec3(t[0], t[1], t[2])
          o = xd.get("orientation", [0, 0, 0, 1])
          xform.orientation = quat(o[0], o[1], o[2], o[3])
          xform.scale = xd.get("scale", 1.0)
          # Switch model if needed
          model_name = xd.get("model")
          if model_name and model_name in self.node_pools:
            self._setModel(name, model_name)
      # Load lights (point lights only have position and color)
      for name, ld in data.get("lights", {}).items():
        if name in self.light_transforms:
          xform = self.light_transforms[name]
          t = ld.get("translation", [0, 5, 0])
          xform.translation = vec3(t[0], t[1], t[2])
          # Update light node matrix
          self.light_nodes[name].setMatrix(xform.composed)
          # Update light color
          c = ld.get("color", [100, 100, 100])
          self.light_data[name].color = vec3(c[0], c[1], c[2])
      print(f"Loaded scene from: {path}")
    except Exception as e:
      print(f"Failed to load: {e}")

  def _onSaveFileSelected(self, path):
    """Save scene to JSON file."""
    if not hasattr(self, 'cube_transforms'):
      return
    if not path.endswith('.osgr'):
      path += '.osgr'
    data = {"cubes": {}, "lights": {}}
    # Save cubes
    for name, xform in self.cube_transforms.items():
      t, o = xform.translation, xform.orientation
      data["cubes"][name] = {
        "translation": [t.x, t.y, t.z],
        "orientation": [o.x, o.y, o.z, o.w],
        "scale": xform.scale,
        "model": self.cube_model_name.get(name)
      }
    # Save lights (point lights only have position and color)
    for name, xform in self.light_transforms.items():
      t = xform.translation
      c = self.light_data[name].color
      data["lights"][name] = {
        "translation": [t.x, t.y, t.z],
        "color": [c.x, c.y, c.z]
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

    # Enable pick buffer
    self.scenegraph.enablePickHud()

    # Grid (not pickable)
    self.grid_data = createGridData()
    self.grid_node = self.layer.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1
    self.grid_node.pickable = False

    ############################################
    # Load 4 models and create node pools (keyed by short name)
    ############################################

    model_paths = [
      "data://tests/pbr_calib",
      "data://tests/misc_gltf_samples/plants/plant1.glb",
      "data://tests/misc_gltf_samples/art_and_sculpture/lion.glb",
      "data://tests/misc_gltf_samples/art_and_sculpture/fracvase.glb",
      "data://tests/misc_gltf_samples/vehicles/car.glb",
      "data://tests/misc_gltf_samples/weapons/warhammer.glb",
      "data://tests/misc_gltf_samples/weapons/shield.glb",
      "data://tests/misc_gltf_samples/DamagedHelmet.glb",
      "data://tests/misc_gltf_samples/furnishings/oillamp.glb",
      "data://tests/misc_gltf_samples/art_and_sculpture/sitter.glb",
    ]
    self.node_pools = {}  # short_name -> NodePool
    self.model_names = []  # ordered list for cycling
    for path in model_paths:
      # Extract short name (filename without path and extension)
      short_name = path.split("/")[-1].split(".")[0]
      try:
        model = lev2.XgmModel(path)
        self.node_pools[short_name] = NodePool(model, self.scenegraph, self.layer)
        self.model_names.append(short_name)
        print(f"Loaded model '{short_name}': {path}")
      except Exception as e:
        print(f"Failed to load model {path}: {e}")

    ############################################
    # Create 16 objects in 4x4 grid, spaced 2m apart
    ############################################

    self.cube_nodes = {}       # name -> current active node
    self.cube_transforms = {}  # name -> transform (shared across node swaps)
    self.cube_model_name = {}  # name -> which model short_name is active
    default_model = self.model_names[0] if self.model_names else None
    for i, name in enumerate(self.cube_names):
      node = self.node_pools[default_model].acquire(name)
      xform = Transform()
      xform.translation = vec3((i % 4) * 2 - 3, 0.5, (i // 4) * 2 - 3)
      node.worldTransform = xform
      self.cube_nodes[name] = node
      self.cube_transforms[name] = xform
      self.cube_model_name[name] = default_model

    # Setup ManipController (initially no target)
    self.manip_controller = lev2.ManipController()
    self.manip_interface = None
    self.manip_controller.mode = lev2.ManipMode.TRANSLATE
    self.selected_cube = None
    self.selected_light = None

    # Create ManipGizmo drawable (renders in scenegraph)
    self.gizmo_data = lev2.ManipGizmoDrawableData()
    self.gizmo_data.controller = self.manip_controller
    self.gizmo_drawable = self.gizmo_data.createDrawable()
    self.gizmo_node = self.scenegraph.createDrawableNodeOnLayers(
        [self.layer], "manip-gizmo", self.gizmo_drawable)
    self.gizmo_node.sortkey = 999  # Render on top
    self.gizmo_node.pickable = False

    # Start with manipulator disabled (no selection yet)
    self.manip_enabled = False
    self.gizmo_node.enabled = False

    ############################################
    # Setup point light at (0, 5, 0)
    ############################################

    self.light_data = {}
    self.light_nodes = {}
    self.light_transforms = {}

    # Create pointlight1
    light_data = lev2.PointLightData()
    light_data.color = vec3(100, 100, 100)  # Bright white light
    light_node = light_data.createNode("pointlight1", self.layer)
    light_xform = Transform()
    light_xform.translation = vec3(0, 5, 0)
    light_node.setMatrix(light_xform.composed)

    self.light_data["pointlight1"] = light_data
    self.light_nodes["pointlight1"] = light_node
    self.light_transforms["pointlight1"] = light_xform

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

  def _onPickResult(self, pfc):
    """Handle pick buffer result."""
    #print(f"Pick dump: {pfc.dump()}")
    obj = pfc.value(0)
    #print(f"Pick result: obj={obj}, type={type(obj)}")

    # Update pick buffer debug images
    SG = self.scenegraph
    self.pick_img_id.texture = SG.pick_tex_id
    self.pick_img_pos.texture = SG.pick_tex_pos
    self.pick_img_nrm.texture = SG.pick_tex_nrm
    self.pick_img_id.setDirty()
    self.pick_img_pos.setDirty()
    self.pick_img_nrm.setDirty()

    if obj is not None and isinstance(obj, u32vec4):
      # obj is a u32vec4 with raw pick values:
      #   .x = pick ID (index into drawable encoding table)
      pick_id = int(obj.x)
      # Background clear typically has .w = 0x3f800000 (1.0 as float bits)
      is_valid_hit = (pick_id > 0) or (pick_id == 0 and int(obj.w) == 0)
      #print(f"  pick_id={pick_id}, is_valid_hit={is_valid_hit}")

      if is_valid_hit:
        # Decode pick_id to get the actual node
        decoded_node = pfc.decodePickID(pick_id)
        #print(f"  decoded_node={decoded_node}, type={type(decoded_node)}")

        # Find which cube slot owns this node
        cube_name = None
        for name, node in self.cube_nodes.items():
          if node is decoded_node:
            cube_name = name
            break

        if cube_name:
          #print(f"  mapped to cube: {cube_name}")
          self._selectCube(cube_name)
          self.outliner.selected_key = f"Cubes/{cube_name}"
          self._enableManip(False)  # Only T/R/S keys enable manip
        #else:
        #  print(f"  no cube found for decoded node")

  def _onCameraEvent(self, uievent):
    """Handle camera manipulation events (orbit, pan, zoom) and mode keys."""
    # Handle click-to-select (left click without modifiers)
    if uievent.code == tokens.PUSH.hashed:
      if not uievent.shift and not uievent.ctrl and not uievent.alt:
        # Check if manip is handling this (don't pick if mouse is over gizmo)
        if self.manip_enabled and self.manip_controller.hoveredAxis != lev2.ManipAxis.NONE:
          pass  # Let manip handle it
        else:
          # Use pick buffer - need viewport-local coords
          # The sgv widget's x,y are relative to its parent (viewport_dock)
          # uievent.x,y are in root coordinates
          # We need coordinates relative to the viewport's content area
          vp_root_x = self.viewport_dock.x + self.sgv.x
          vp_root_y = self.viewport_dock.y + self.sgv.y
          local_x = uievent.x - vp_root_x
          local_y = uievent.y - vp_root_y
          #print(f"Pick coords: event({uievent.x},{uievent.y}) vp_root({vp_root_x},{vp_root_y}) local({local_x},{local_y}) vp_size({self.sgv.width}x{self.sgv.height})")
          #print(f"  dock.x={self.viewport_dock.x} dock.y={self.viewport_dock.y} sgv.x={self.sgv.x} sgv.y={self.sgv.y}")
          #print(f"  camera eye={self.camera.eye} target={self.camera.target}")
          # Clamp to viewport bounds
          if local_x >= 0 and local_x < self.sgv.width and local_y >= 0 and local_y < self.sgv.height:
            # Disable gizmo during pick (not safe for pick buffer yet)
            gizmo_was_enabled = self.gizmo_node.enabled
            self.gizmo_node.enabled = False
            self.scenegraph.pickWithScreenCoord(
              self.camera,
              vec2(local_x, local_y),
              0, 0, self.sgv.width, self.sgv.height,
              self._onPickResult
            )
            self.gizmo_node.enabled = gizmo_was_enabled
          else:
            print(f"  Click outside viewport bounds")

    if uievent.code == tokens.KEY_DOWN.hashed:
      # Escape - if manip showing, disable it; otherwise deselect all
      if uievent.keycode == 256:
        if self.manip_enabled:
          self._enableManip(False)
          #print("Manipulator disabled (press T/R/S to enable)")
        else:
          # Deselect all
          if self.selected_cube and self.selected_cube in self.cube_nodes:
            self.cube_nodes[self.selected_cube].modcolor = vec4(1, 1, 1, 1)
          self.selected_cube = None
          self.selected_light = None
          self.color_collapsable.expanded = False
          self.outliner.selected_key = ""
          self.manip_controller.target = None
          #print("Deselected all")
        return lev2.ui.HandlerResult()
      # T/R/S - enable and set mode
      elif uievent.keycode == ord("T"):
        self._enableManip(True)
        if self.manip_controller.mode == lev2.ManipMode.TRANSLATE:
          if self.manip_controller.space == lev2.ManipSpace.LOCAL:
            self.manip_controller.space = lev2.ManipSpace.WORLD
            #print("Mode: TRANSLATE (WORLD)")
          else:
            self.manip_controller.space = lev2.ManipSpace.LOCAL
            #print("Mode: TRANSLATE (LOCAL)")
        else:
          self.manip_controller.mode = lev2.ManipMode.TRANSLATE
          space_name = "LOCAL" if self.manip_controller.space == lev2.ManipSpace.LOCAL else "WORLD"
          #print(f"Mode: TRANSLATE ({space_name})")
        return lev2.ui.HandlerResult()
      elif uievent.keycode == ord("R"):
        if self.selected_light:
          #print("Rotate not supported for point lights")
          return lev2.ui.HandlerResult()
        self._enableManip(True)
        self.manip_controller.mode = lev2.ManipMode.ROTATE
        #print("Mode: ROTATE")
        return lev2.ui.HandlerResult()
      elif uievent.keycode == ord("S"):
        if self.selected_light:
          #print("Scale not supported for point lights")
          return lev2.ui.HandlerResult()
        self._enableManip(True)
        self.manip_controller.mode = lev2.ManipMode.SCALE
        #print("Mode: SCALE")
        return lev2.ui.HandlerResult()
      # M - cycle model on selected cube
      elif uievent.keycode == ord("M"):
        if self.selected_cube:
          self._cycleModel(self.selected_cube)
        #else:
        #  print("No cube selected (M cycles models on cubes)")
        return lev2.ui.HandlerResult()
      # F - focus on selected object
      elif uievent.keycode == ord("F"):
        target_pos = None
        if self.selected_cube:
          target_pos = self.cube_transforms[self.selected_cube].translation
        elif self.selected_light:
          target_pos = self.light_transforms[self.selected_light].translation
        if target_pos:
          # Get current camera direction and distance
          eye = self.camera.eye
          old_target = self.camera.target
          offset = eye - old_target
          distance = offset.length
          direction = offset.normalized if distance > 0.001 else vec3(0, 0, 1)
          # New eye position preserves angle and distance
          new_eye = target_pos + direction * distance
          self.uicam.lookAt(new_eye, target_pos, vec3(0, 1, 0))
          self.uicam.updateMatrices()
          self.camera.copyFrom(self.uicam.cameradata)
          #print(f"Focused on {self.selected_cube or self.selected_light}")
        #else:
        #  print("No object selected to focus on")
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
      # Animate selection highlight (pulse between red and white)
      t = math.sin(updinfo.absolutetime * 8.0) * 0.5 + 0.5  # 0 to 1 pulsing
      r = 1.0
      g = t  # 0.3 to 1.0
      b = t  # 0.3 to 1.0
      self.cube_nodes[self.selected_cube].modcolor = vec4(r, g, b, 1)
    elif self.selected_light:
      self.xform_editor.sync()
      # Update light node matrix from transform
      xform = self.light_transforms[self.selected_light]
      self.light_nodes[self.selected_light].setMatrix(xform.composed)

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

SceneEditorTest().ezapp.mainThreadLoop()
