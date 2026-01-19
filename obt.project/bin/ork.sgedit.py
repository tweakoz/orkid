#!/usr/bin/env ork.python

################################################################################
# Scene Editor
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import signal, os, argparse
from orkengine.core import vec3, vec4, quat, VarMap, CrcStringProxy, Transform
from orkengine import lev2
from ork.editor import SceneEditorBase, SceneLoader

tokens = CrcStringProxy()

################################################################################

parser = argparse.ArgumentParser(description="Scene Editor")
parser.add_argument("--scene", "-s", type=str, help="Scene file to load on startup (.osgr)")
args = parser.parse_args()

################################################################################

class SceneEditor(SceneEditorBase):
  """Scene editor with model loading and cycling support."""

  def __init__(self):
    super().__init__(
      left_dock_proportion=0.25,
      propsheet_proportion=0.6,
      file_extension=".osgr",
      home_dir=os.path.expanduser("~")
    )

    # Model registry (populated in onGpuInit)
    self.models = {}       # short_name -> XgmModel
    self.model_names = []  # ordered list for cycling
    self.model_paths = {}  # short_name -> full_path

    self._createApp(name="SceneEditor", fullscreen=True, ssaa=1)
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())

  ##############################################
  # Node type definitions
  ##############################################

  def getNodeTypes(self):
    """Define node types for outliner."""
    return [
      {
        "key": "Nodes",
        "item_type": "node",
        "display_name": "Node",
        "drawable_type": tokens.model,
        "default_name": lambda model: f"node{len(model._getScenegraph().drawableNodesWithType(tokens.model))}"
      },
      {
        "key": "PointLights",
        "item_type": "pointlight",
        "display_name": "Point Light",
        "light_type": tokens.pointlight,
        "default_name": lambda model: f"pl{len(model._getScenegraph().lightNodesWithType(tokens.pointlight))}"
      }
    ]

  ##############################################
  # Node CRUD operations
  ##############################################

  def findNode(self, name, item_type):
    """Find a node by name and type."""
    if item_type == "node":
      for node in self.scenegraph.drawableNodesWithType(tokens.model):
        if node.name == name:
          return node
    elif item_type == "pointlight":
      for node in self.scenegraph.lightNodesWithType(tokens.pointlight):
        if node.name == name:
          return node
    return None

  def createNode(self, name, item_type):
    """Create a node of given type."""
    if item_type == "node":
      self._createDrawableNode(name)
    elif item_type == "pointlight":
      self._createPointLight(name)

  def deleteNode(self, name, item_type):
    """Delete a node."""
    if item_type == "node":
      node = self.findNode(name, "node")
      if node:
        self._purgatory.add(node)
        self.layer.removeDrawableNode(node)
        if self.selected_node is node:
          self.selected_node = None
          self._enableManip(False)
    elif item_type == "pointlight":
      node = self.findNode(name, "pointlight")
      if node:
        self._purgatory.add(node)
        self.layer.removeLightNode(node)
        if self.selected_light is node:
          self.selected_light = None
          self.light_editor.unbind()
          self.color_collapsable.expanded = False
          self._enableManip(False)

  def renameNode(self, old_name, new_name, item_type):
    """Rename a node."""
    node = self.findNode(old_name, item_type)
    if node:
      node.name = new_name

  ##############################################
  # Drawable node creation with model support
  ##############################################

  def _createDrawableNode(self, name, model_name=None, model_path=None):
    """Create a new drawable node with specified model."""
    if self.findNode(name, "node") is not None:
      return

    # Determine model to use
    if model_path and model_path in self.model_paths.values():
      # Find short name from path
      for short, path in self.model_paths.items():
        if path == model_path:
          model_name = short
          break
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

    # Store metadata in userdata
    sg_node.user.model_name = model_name
    sg_node.user.model_path = self.model_paths.get(model_name, "")

  def _createPointLight(self, name):
    """Create a new point light."""
    if self.findNode(name, "pointlight") is not None:
      return
    light_data = lev2.PointLightData()
    light_data.color = vec3(100, 100, 100)
    light_node = light_data.createNode(name, self.layer)
    xform = Transform()
    xform.translation = vec3(0, 5, 0)
    light_node.worldTransform = xform
    light_node.setMatrix(xform.composed)
    light_node.user.lightdata = light_data

  ##############################################
  # Model cycling (application-specific feature)
  ##############################################

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
      return

    name = node.name
    xform = node.worldTransform

    # Create new node with new model
    model = self.models[model_name]
    drawable = model.createDrawable()
    new_sg_node = self.scenegraph.createDrawableNodeOnLayers([self.layer], name, drawable)
    new_sg_node.worldTransform = xform
    new_sg_node.user.model_name = model_name
    new_sg_node.user.model_path = self.model_paths.get(model_name, "")

    # Transfer selection highlight if this node was selected
    if self.selected_node is node:
      new_sg_node.modcolor = vec4(1, 0.3, 0.3, 1)
      self.selected_node = new_sg_node

    # Move old node to purgatory
    self._purgatory.add(node)
    self.layer.removeDrawableNode(node)
    print(f"Changed {name} to model: {model_name}")

  def getExtraKeyboardShortcuts(self):
    """Add 'M' for model cycling."""
    return {
      ord("M"): (lambda node: self._cycleModel(node), True, False)
    }

  ##############################################
  # Scene loading customization
  ##############################################

  def _onSceneLoaded(self, result):
    """Post-process loaded nodes to set model_name from model_path."""
    for node in result['nodes']:
      model_path = getattr(node.user, 'model_path', None)
      if model_path:
        # Find or load the model
        short_name = model_path.split("/")[-1].split(".")[0]
        if short_name not in self.models:
          # Try to load the model
          try:
            self.models[short_name] = lev2.XgmModel(model_path)
            self.model_names.append(short_name)
            self.model_paths[short_name] = model_path
          except Exception as e:
            print(f"Warning: Could not load model {model_path}: {e}")
            continue
        node.user.model_name = short_name

  ##############################################
  # Initial scene
  ##############################################

  def _createInitialScene(self):
    """Create default initial scene content."""
    self._createDrawableNode("node0")
    self._createPointLight("pl0")

  ##############################################
  # GPU initialization
  ##############################################

  def onGpuInit(self, ctx):
    # Call base class initialization
    super().onGpuInit(ctx)

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
    for path in model_paths:
      short_name = path.split("/")[-1].split(".")[0]
      try:
        self.models[short_name] = lev2.XgmModel(path)
        self.model_names.append(short_name)
        self.model_paths[short_name] = path
        print(f"Loaded model '{short_name}': {path}")
      except Exception as e:
        print(f"Failed to load model {path}: {e}")

    # Create initial scene or load from command line
    if args.scene and os.path.exists(args.scene):
      self._loadScene(args.scene)
    else:
      self._createInitialScene()

    print("Scene Editor Ready")

################################################################################

SceneEditor().ezapp.mainThreadLoop()
