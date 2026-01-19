#!/usr/bin/env ork.python

################################################################################
# Scene Editor
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math, sys, signal, os, json, argparse
from orkengine.core import vec2, vec3, vec4, quat, mtx4, VarMap, CrcStringProxy, Transform, u32vec4
from orkengine import lev2
from ork.ui import icon_library
from ork.ui.filesystem_browser import FilesystemBrowser
from ork.ui.transform_edit import TransformEdit
from ork.ui.color_picker import ColorPicker

tokens = CrcStringProxy()

################################################################################
# LightPropertyEditor - MVC controller for light properties
################################################################################

class LightPropertyEditor:
  """Controller that mediates between light model data and UI views."""

  def __init__(self, color_picker, intensity_edit):
    self._color_picker = color_picker
    self._intensity_edit = intensity_edit
    self._light_data = None  # Currently bound light data (model)
    self._syncing_from_model = False  # Guard against feedback loops

    # Connect view callbacks to controller
    self._color_picker.onColorChanged = self._onViewChanged
    self._intensity_edit.onValueChanged(lambda v: self._onViewChanged())

  def bind(self, light_data):
    """Bind to a light's data and sync view from model."""
    self._light_data = light_data
    self._syncViewFromModel()

  def unbind(self):
    """Unbind from current light."""
    self._light_data = None

  def _syncViewFromModel(self):
    """Update view to reflect model state. Does not trigger write-back."""
    if not self._light_data:
      return
    self._syncing_from_model = True
    c = self._light_data.color
    intensity = max(c.x, c.y, c.z, 0.001)
    self._color_picker.current_color = vec4(c.x / intensity, c.y / intensity, c.z / intensity, 1.0)
    self._intensity_edit.value = intensity
    self._syncing_from_model = False

  def _onViewChanged(self):
    """Called when user edits the view. Writes to model."""
    if self._syncing_from_model or not self._light_data:
      return
    c = self._color_picker.current_color
    i = self._intensity_edit.value
    self._light_data.color = vec3(c.x * i, c.y * i, c.z * i)

################################################################################

home_dir = os.path.expanduser("~")

parser = argparse.ArgumentParser(description="Scene Editor")
parser.add_argument("--scene", "-s", type=str, help="Scene file to load on startup (.osgr)")
args = parser.parse_args()

################################################################################
# SceneModel - OutlinerModel for scene hierarchy
# Queries scenegraph directly - no caching
################################################################################

class SceneModel(lev2.ui.OutlinerModel):
  """Outliner model that queries the scenegraph directly."""

  def __init__(self, editor):
    super().__init__()
    self.editor = editor
    self.allow_rename = True
    self.allow_delete = True
    self.allow_add = True

  def _getScenegraph(self):
    return self.editor.scenegraph

  def getChildren(self, parent_key):
    if parent_key == "":
      return ["Nodes", "PointLights"]
    elif parent_key == "Nodes":
      sg = self._getScenegraph()
      nodes = sg.drawableNodesWithType(tokens.model)
      return [f"Nodes/{n.name}" for n in nodes]
    elif parent_key == "PointLights":
      sg = self._getScenegraph()
      lights = sg.lightNodesWithType(tokens.pointlight)
      return [f"PointLights/{n.name}" for n in lights]
    return []

  def getDisplayName(self, key):
    # Extract name from key (last component after /)
    if "/" in key:
      return key.split("/")[-1]
    return key

  def hasChildren(self, key):
    if key == "Nodes":
      sg = self._getScenegraph()
      return len(sg.drawableNodesWithType(tokens.model)) > 0
    elif key == "PointLights":
      sg = self._getScenegraph()
      return len(sg.lightNodesWithType(tokens.pointlight)) > 0
    return False

  def getValue(self, key):
    return None

  def _getItemType(self, key):
    if key in ("Nodes", "PointLights"):
      return "group"
    elif key.startswith("Nodes/"):
      return "node"
    elif key.startswith("PointLights/"):
      return "pointlight"
    return None

  def _nodeExists(self, key):
    name = key.split("/")[-1] if "/" in key else key
    item_type = self._getItemType(key)
    if item_type == "node":
      return self.editor._findDrawableNode(name) is not None
    elif item_type == "pointlight":
      return self.editor._findLightNode(name) is not None
    elif item_type == "group":
      return key in ("Nodes", "PointLights")
    return False

  def getFactories(self, parent_key):
    if parent_key == "Nodes":
      return [{
        "id": "node",
        "display_name": "Node",
        "default_name_generator": lambda model: f"node{len(model._getScenegraph().drawableNodesWithType(tokens.model))}"
      }]
    elif parent_key == "PointLights":
      return [{
        "id": "pointlight",
        "display_name": "Point Light",
        "default_name_generator": lambda model: f"pl{len(model._getScenegraph().lightNodesWithType(tokens.pointlight))}"
      }]
    return []

  def createItem(self, parent_key, name, factory_id):
    if parent_key == "Nodes" and factory_id == "node":
      if self.editor._findDrawableNode(name) is not None:
        return ""  # Name collision
      self.editor._createNode(name)
      new_key = f"Nodes/{name}"
      self.notifyItemAdded(new_key)
      return new_key
    elif parent_key == "PointLights" and factory_id == "pointlight":
      if self.editor._findLightNode(name) is not None:
        return ""  # Name collision
      self.editor._createPointLight(name)
      new_key = f"PointLights/{name}"
      self.notifyItemAdded(new_key)
      return new_key
    return ""

  def renameItem(self, old_key, new_name):
    item_type = self._getItemType(old_key)
    if item_type not in ("node", "pointlight"):
      return None  # Can't rename groups

    old_name = old_key.split("/")[-1]

    # Check for collision
    if item_type == "node" and self.editor._findDrawableNode(new_name) is not None:
      return None
    if item_type == "pointlight" and self.editor._findLightNode(new_name) is not None:
      return None

    # Build new key
    last_slash = old_key.rfind("/")
    new_key = (old_key[:last_slash + 1] + new_name) if last_slash >= 0 else new_name

    # Update scenegraph via editor
    self.editor._renameNode(old_name, new_name, item_type)
    return new_key

  def removeItem(self, key):
    item_type = self._getItemType(key)
    if item_type not in ("node", "pointlight"):
      return  # Can't delete groups

    name = key.split("/")[-1]
    self.editor._deleteNode(name, item_type)

################################################################################

l2exdir = (lev2.lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir)
from lev2utils.cameras import setupUiCameraX
from lev2utils.primitives import createGridData

################################################################################

class SceneEditor:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=True, ssaa=1, name="SceneEditor")
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.15, 0.15, 0.15, 1)
    lg.clearColorGuide = vec4(0.4, 0.4, 0.2, 1)

    # Viewport dock (fills area)
    viewport_dock_item = lg.makeChild(fill=True, margin=2, uiclass=lev2.ui.DockablePanel, args=["viewport_dock"])
    self.viewport_dock = viewport_dock_item.widget
    self.viewport_dock.titlebar_color = vec4(0.15, 0.2, 0.25, 1)
    self.sgv = self.viewport_dock.createChild(uiclass=lev2.ui.SceneGraphViewport, args=["Viewport", vec4(0.1, 0.1, 0.12, 1)])

    # Left dock (25%)
    left_dock_item = lg.split(layout=viewport_dock_item.layout, proportion=0.25, placement=tokens.LEFT, margin=2, uiclass=lev2.ui.DockablePanel, args=["left_dock"])
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

    # Property sheet dock (60% bottom of left)
    propsheet_dock_item = lg.split(layout=left_dock_item.layout, proportion=0.6, placement=tokens.BOTTOM, margin=2, uiclass=lev2.ui.DockablePanel, args=["propsheet_dock"])
    self.propsheet_dock = propsheet_dock_item.widget
    self.propsheet_dock.titlebar_color = vec4(0.2, 0.2, 0.15, 1)
    self._setupPropertySheet()

    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())

  def _setupToolbar(self):
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

  def _openFilePopup(self, title, callback, mode):
    popup = self.ezapp.createSecondaryWindow(width=800, height=600, x=200, y=150, title=title, decorated=True, resizable=True, floating=True)
    uic = popup.ui_context
    root = lev2.ui.LayoutGroup.create("popup_lg")
    root.setRect(0, 0, popup.width, popup.height)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(uiclass=FilesystemBrowser, args=["browser", home_dir, ".osgr", vec3(0.1, 0.1, 0.1), mode], fill=True)
    browser = browser_item.widget.uservars.filesystem_browser
    browser.onActivate = lambda p: (callback(p), popup.requestClose())
    browser.onCancel = lambda: popup.requestClose()

  ##############################################
  # Helper methods to find nodes by name (query scenegraph)
  ##############################################

  def _findDrawableNode(self, name):
    """Find a drawable node by name, returns None if not found."""
    for node in self.scenegraph.drawableNodesWithType(tokens.model):
      if node.name == name:
        return node
    return None

  def _findLightNode(self, name):
    """Find a light node by name, returns None if not found."""
    for node in self.scenegraph.lightNodesWithType(tokens.pointlight):
      if node.name == name:
        return node
    return None

  ##############################################
  # Load/Save
  ##############################################

  def _loadScene(self, path):
    if not os.path.exists(path):
      return
    try:
      with open(path, 'r') as f:
        data = json.load(f)
      # Clear existing scene
      for node in list(self.scenegraph.drawableNodesWithType(tokens.model)):
        self._deleteNode(node.name, "node")
      for node in list(self.scenegraph.lightNodesWithType(tokens.pointlight)):
        self._deleteNode(node.name, "pointlight")
      # Load nodes
      for name, nd in data.get("nodes", {}).items():
        model_name = nd.get("model_name")
        self._createNode(name, model_name)
        node = self._findDrawableNode(name)
        if node:
          xform = Transform()
          t = nd.get("translation", [0, 0.5, 0])
          xform.translation = vec3(t[0], t[1], t[2])
          o = nd.get("orientation", [0, 0, 0, 1])
          xform.orientation = quat(o[0], o[1], o[2], o[3])
          xform.scale = nd.get("scale", 1.0)
          node.worldTransform = xform
      # Load lights
      for name, ld in data.get("lights", {}).items():
        self._createPointLight(name)
        node = self._findLightNode(name)
        if node:
          xform = Transform()
          t = ld.get("translation", [0, 5, 0])
          xform.translation = vec3(t[0], t[1], t[2])
          node.worldTransform = xform
          node.setMatrix(xform.composed)
          c = ld.get("color", [100, 100, 100])
          lightdata = node.user.lightdata
          lightdata.color = vec3(c[0], c[1], c[2])
      # Notify outliner to refresh from scenegraph
      self.scene_model.notifyModelReset()
      print(f"Loaded: {path}")
    except Exception as e:
      print(f"Load failed: {e}")

  def _saveScene(self, path):
    if not path.endswith('.osgr'):
      path += '.osgr'
    data = {"nodes": {}, "lights": {}}
    for node in self.scenegraph.drawableNodesWithType(tokens.model):
      xform = node.worldTransform
      t, o = xform.translation, xform.orientation
      data["nodes"][node.name] = {
        "translation": [t.x, t.y, t.z],
        "orientation": [o.x, o.y, o.z, o.w],
        "scale": xform.scale,
        "model_name": node.user.model_name
      }
    for node in self.scenegraph.lightNodesWithType(tokens.pointlight):
      xform = node.worldTransform
      t = xform.translation
      lightdata = node.user.lightdata
      c = lightdata.color
      data["lights"][node.name] = {"translation": [t.x, t.y, t.z], "color": [c.x, c.y, c.z]}
    with open(path, 'w') as f:
      json.dump(data, f, indent=2)
    print(f"Saved: {path}")

  ##############################################
  # Node/Light creation and management
  ##############################################

  def _createNode(self, name, model_name=None):
    """Create a new node with specified model (default: first model)."""
    if self._findDrawableNode(name) is not None:
      return
    if model_name is None:
      model_name = self.model_names[0] if self.model_names else None
    if model_name is None or model_name not in self.models:
      return
    model = self.models[model_name]
    drawable = model.createDrawable()
    sg_node = self.scenegraph.createDrawableNodeOnLayers([self.layer], name, drawable)
    xform = Transform()
    xform.translation = vec3(0, 0.5, 0)
    sg_node.worldTransform = xform
    # Store model_name in userdata
    sg_node.user.model_name = model_name

  def _cycleModel(self, node):
    """Cycle to the next model for the given node."""
    if node is None or not self.model_names:
      return
    current_model = node.user.model_name
    current_idx = self.model_names.index(current_model) if current_model in self.model_names else 0
    next_idx = (current_idx + 1) % len(self.model_names)
    next_model = self.model_names[next_idx]
    self._setModel(node, next_model)

  def _setModel(self, node, model_name):
    """Set a specific model for the given node."""
    if node is None or model_name not in self.models:
      return
    current_model = node.user.model_name
    if current_model == model_name:
      return  # Already using this model

    name = node.name
    xform = node.worldTransform

    # Create new node with new model
    model = self.models[model_name]
    drawable = model.createDrawable()
    new_sg_node = self.scenegraph.createDrawableNodeOnLayers([self.layer], name, drawable)
    new_sg_node.worldTransform = xform
    new_sg_node.user.model_name = model_name

    # Transfer selection highlight if this node was selected
    if self.selected_node is node:
      new_sg_node.modcolor = vec4(1, 0.3, 0.3, 1)
      self.selected_node = new_sg_node

    # Move old node to purgatory
    self._purgatory.add(node)
    self.layer.removeDrawableNode(node)
    print(f"Changed {name} to model: {model_name}")

  def _createPointLight(self, name):
    """Create a new point light."""
    if self._findLightNode(name) is not None:
      return
    light_data = lev2.PointLightData()
    light_data.color = vec3(100, 100, 100)
    light_node = light_data.createNode(name, self.layer)
    xform = Transform()
    xform.translation = vec3(0, 5, 0)
    light_node.worldTransform = xform
    light_node.setMatrix(xform.composed)
    # Store lightdata in userdata
    light_node.user.lightdata = light_data

  def _deleteNode(self, name, item_type):
    """Delete a node or light."""
    if item_type == "node":
      node = self._findDrawableNode(name)
      if node:
        self._purgatory.add(node)
        self.layer.removeDrawableNode(node)
        if self.selected_node is node:
          self.selected_node = None
          self._enableManip(False)
    elif item_type == "pointlight":
      node = self._findLightNode(name)
      if node:
        self._purgatory.add(node)
        self.layer.removeLightNode(node)
        if self.selected_light is node:
          self.selected_light = None
          self.light_editor.unbind()
          self.color_collapsable.expanded = False
          self._enableManip(False)

  def _renameNode(self, old_name, new_name, item_type):
    """Rename a node or light."""
    if item_type == "node":
      node = self._findDrawableNode(old_name)
      if node:
        node.name = new_name
    elif item_type == "pointlight":
      node = self._findLightNode(old_name)
      if node:
        node.name = new_name

  def _selectNode(self, name):
    """Select a node by name."""
    node = self._findDrawableNode(name)
    if node is None:
      return
    self._selectDrawableNode(node)

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

  def _selectLight(self, name):
    """Select a light by name."""
    node = self._findLightNode(name)
    if node is None:
      return
    self._selectLightNode(node)

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

  def _enableManip(self, enable):
    self.manip_enabled = enable
    self.gizmo_node.enabled = enable
    if enable and self.manip_interface:
      self.manip_controller.target = self.manip_interface
    else:
      self.manip_controller.target = None

  ##############################################

  def onGpuInit(self, ctx):
    # Theme
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    self.uicontext.theme_engine = lev2.ui.ThemeEngine(self.custom_db)

    # Scenegraph
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

    # Load models
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
    self.models = {}  # short_name -> model
    self.model_names = []  # ordered list for cycling
    for path in model_paths:
      short_name = path.split("/")[-1].split(".")[0]
      try:
        self.models[short_name] = lev2.XgmModel(path)
        self.model_names.append(short_name)
        print(f"Loaded model '{short_name}': {path}")
      except Exception as e:
        print(f"Failed to load model {path}: {e}")

    # Selection state (node references, not names)
    self.selected_node = None   # DrawableNode or None
    self.selected_light = None  # LightNode or None
    self._purgatory = set()  # Holds removed nodes until true deletion is supported

    # Create initial node
    self._createNode("Node_01")

    # Create initial light
    self._createPointLight("pl0")

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

    # Setup outliner model (queries scenegraph directly)
    self.scene_model = SceneModel(self)
    self.outliner.model = self.scene_model
    self.outliner.expandAll()

    def on_select(key):
      name = key.split("/")[-1] if "/" in key else key
      if key.startswith("Nodes/"):
        self._selectNode(name)
      elif key.startswith("PointLights/"):
        self._selectLight(name)
      else:
        # Deselect
        if self.selected_node is not None:
          self.selected_node.modcolor = vec4(1, 1, 1, 1)
        self.selected_node = None
        self.selected_light = None
        self.light_editor.unbind()
        self.color_collapsable.expanded = False
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

    if args.scene and os.path.exists(args.scene):
      self._loadScene(args.scene)

    print("Scene Editor Ready")

  ##############################################

  def _onPickResult(self, pfc):
    obj = pfc.value(0)
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

    if obj is not None and isinstance(obj, u32vec4):
      pick_id = int(obj.x)
      is_valid = (pick_id > 0) or (pick_id == 0 and int(obj.w) == 0)
      if is_valid:
        decoded = pfc.decodePickID(pick_id)
        # Check if it's a drawable node
        for node in self.scenegraph.drawableNodesWithType(tokens.model):
          if node is decoded:
            self._selectDrawableNode(node)
            self.outliner.selected_key = f"Nodes/{node.name}"
            self._enableManip(False)
            break

  def _onCameraEvent(self, uievent):
    if uievent.code == tokens.PUSH.hashed:
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
      if kc == 256:  # ESC
        if self.manip_enabled:
          self._enableManip(False)
        else:
          if self.selected_node is not None:
            self.selected_node.modcolor = vec4(1, 1, 1, 1)
          self.selected_node = None
          self.selected_light = None
          self.light_editor.unbind()
          self.color_collapsable.expanded = False
          self.outliner.selected_key = ""
          self.manip_controller.target = None
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
      elif kc == ord("M"):
        if self.selected_node is not None:
          self._cycleModel(self.selected_node)
        return lev2.ui.HandlerResult()
      elif kc == ord("F"):
        target = None
        if self.selected_node is not None:
          target = self.selected_node.worldTransform.translation
        elif self.selected_light is not None:
          target = self.selected_light.worldTransform.translation
        if target:
          eye = self.camera.eye
          offset = eye - self.camera.target
          dist = offset.length
          direction = offset.normalized if dist > 0.001 else vec3(0, 0, 1)
          self.uicam.lookAt(target + direction * dist, target, vec3(0, 1, 0))
          self.uicam.updateMatrices()
          self.camera.copyFrom(self.uicam.cameradata)
        return lev2.ui.HandlerResult()

    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()

  ##############################################

  def onUpdate(self, updinfo):
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

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

SceneEditor().ezapp.mainThreadLoop()
