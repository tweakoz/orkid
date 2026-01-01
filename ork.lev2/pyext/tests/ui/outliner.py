#!/usr/bin/env ork.python

################################################################################
# Outliner Widget Test
# Demonstrates tree view with hierarchical data using both VarMap and custom model
################################################################################

import signal
from orkengine.core import vec2, vec3, vec4, VarMap
from orkengine import lev2

################################################################################
# Custom Python Model Example
# Demonstrates subclassing OutlinerModel in Python
################################################################################

class CustomModel(lev2.ui.OutlinerModel):
  """A custom model that stores data in a Python dictionary."""

  def __init__(self):
    super().__init__()
    # Internal data structure: dict of key -> (display_name, children_keys, value)
    self._items = {}
    self._root_children = []

  def addRootItem(self, key, display_name, value=None):
    """Add an item at root level."""
    self._items[key] = {"name": display_name, "children": [], "value": value}
    self._root_children.append(key)
    self.notifyItemAdded(key)

  def addChildItem(self, parent_key, key, display_name, value=None):
    """Add a child item under a parent."""
    self._items[key] = {"name": display_name, "children": [], "value": value}
    if parent_key in self._items:
      self._items[parent_key]["children"].append(key)
    self.notifyItemAdded(key)

  def getChildren(self, parent_key):
    """Return list of child keys for a parent (empty string = root)."""
    if parent_key == "":
      return self._root_children
    if parent_key in self._items:
      return self._items[parent_key]["children"]
    return []

  def getDisplayName(self, key):
    """Return display name for a key."""
    if key in self._items:
      return self._items[key]["name"]
    return key

  def hasChildren(self, key):
    """Check if item has children."""
    if key in self._items:
      return len(self._items[key]["children"]) > 0
    return False

  def getValue(self, key):
    """Get optional value for a key."""
    if key in self._items:
      return self._items[key].get("value")
    return None

################################################################################

class OutlinerTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    # Create outliner widget
    outliner_layout = lg_group.makeChild(uiclass=lev2.ui.Outliner, args=["outliner"])
    self.outliner = outliner_layout.widget

    # Set up layout - anchor to parent
    root_layout = lg_group.layout
    outliner_layout.layout.top.anchorTo(root_layout.top)
    outliner_layout.layout.left.anchorTo(root_layout.left)
    outliner_layout.layout.bottom.anchorTo(root_layout.bottom)
    outliner_layout.layout.right.anchorTo(root_layout.right)

    # Choose which model approach to use:
    # Option 1: VarMap-based (backward compatible)
    # self.outliner.data = self._buildTestData()

    # Option 2: Custom Python model
    self.custom_model = self._buildCustomModel()
    self.outliner.model = self.custom_model

    self.outliner.expandAll()

    # Set selection callback
    def on_select(key):
      print(f"Selected: {key}")
      # Get value from model
      model = self.outliner.model
      if model:
        value = model.getValue(key)
        print(f"  Value: {value}")

    self.outliner.onSelect(on_select)

    # Style
    self.outliner.bgcolor = vec4(0.15, 0.15, 0.15, 1)
    self.outliner.text_color = vec4(0.9, 0.9, 0.9, 1)
    self.outliner.selected_color = vec4(0.2, 0.4, 0.6, 1)
    self.outliner.hover_color = vec4(0.25, 0.25, 0.3, 1)
    self.outliner.item_height = 24

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def _buildCustomModel(self):
    """Build hierarchical test data using custom Python model."""
    model = CustomModel()

    # Scene hierarchy
    model.addRootItem("Scene", "Scene")

    # Cameras
    model.addChildItem("Scene", "Scene/Cameras", "Cameras")
    model.addChildItem("Scene/Cameras", "Scene/Cameras/MainCamera", "MainCamera", "perspective")
    model.addChildItem("Scene/Cameras", "Scene/Cameras/TopCamera", "TopCamera", "orthographic")
    model.addChildItem("Scene/Cameras", "Scene/Cameras/SideCamera", "SideCamera", "orthographic")

    # Lights
    model.addChildItem("Scene", "Scene/Lights", "Lights")
    model.addChildItem("Scene/Lights", "Scene/Lights/SunLight", "SunLight", "directional")
    model.addChildItem("Scene/Lights", "Scene/Lights/PointLight1", "PointLight1", "point")
    model.addChildItem("Scene/Lights", "Scene/Lights/SpotLight1", "SpotLight1", "spot")

    # Objects
    model.addChildItem("Scene", "Scene/Objects", "Objects")

    # Character group
    model.addChildItem("Scene/Objects", "Scene/Objects/Character", "Character")
    model.addChildItem("Scene/Objects/Character", "Scene/Objects/Character/Body", "Body", "mesh")
    model.addChildItem("Scene/Objects/Character", "Scene/Objects/Character/Head", "Head", "mesh")
    model.addChildItem("Scene/Objects/Character", "Scene/Objects/Character/LeftArm", "LeftArm", "mesh")
    model.addChildItem("Scene/Objects/Character", "Scene/Objects/Character/RightArm", "RightArm", "mesh")

    # Environment group
    model.addChildItem("Scene/Objects", "Scene/Objects/Environment", "Environment")
    model.addChildItem("Scene/Objects/Environment", "Scene/Objects/Environment/Ground", "Ground", "mesh")
    model.addChildItem("Scene/Objects/Environment", "Scene/Objects/Environment/Sky", "Sky", "dome")
    model.addChildItem("Scene/Objects/Environment", "Scene/Objects/Environment/Tree1", "Tree1", "mesh")
    model.addChildItem("Scene/Objects/Environment", "Scene/Objects/Environment/Tree2", "Tree2", "mesh")
    model.addChildItem("Scene/Objects/Environment", "Scene/Objects/Environment/Rock1", "Rock1", "mesh")

    # Materials
    model.addRootItem("Materials", "Materials")
    model.addChildItem("Materials", "Materials/CharacterSkin", "CharacterSkin", "pbr")
    model.addChildItem("Materials", "Materials/GroundGrass", "GroundGrass", "pbr")
    model.addChildItem("Materials", "Materials/TreeBark", "TreeBark", "pbr")
    model.addChildItem("Materials", "Materials/SkyDome", "SkyDome", "unlit")

    # Settings
    model.addRootItem("Settings", "Settings")
    model.addChildItem("Settings", "Settings/RenderQuality", "RenderQuality", "high")
    model.addChildItem("Settings", "Settings/ShadowResolution", "ShadowResolution", 2048)
    model.addChildItem("Settings", "Settings/AntiAliasing", "AntiAliasing", "MSAA4x")

    return model

  def _buildTestData(self):
    """Build hierarchical test data using VarMap (backward compatible approach)."""
    data = VarMap()

    # Scene hierarchy example
    scene = VarMap()

    # Cameras
    cameras = VarMap()
    cameras.MainCamera = "perspective"
    cameras.TopCamera = "orthographic"
    cameras.SideCamera = "orthographic"
    scene.Cameras = cameras

    # Lights
    lights = VarMap()
    lights.SunLight = "directional"
    lights.PointLight1 = "point"
    lights.SpotLight1 = "spot"
    scene.Lights = lights

    # Objects
    objects = VarMap()

    # Character group
    character = VarMap()
    character.Body = "mesh"
    character.Head = "mesh"
    character.LeftArm = "mesh"
    character.RightArm = "mesh"
    objects.Character = character

    # Environment group
    environment = VarMap()
    environment.Ground = "mesh"
    environment.Sky = "dome"
    environment.Tree1 = "mesh"
    environment.Tree2 = "mesh"
    environment.Rock1 = "mesh"
    objects.Environment = environment

    scene.Objects = objects
    data.Scene = scene

    # Materials
    materials = VarMap()
    materials.CharacterSkin = "pbr"
    materials.GroundGrass = "pbr"
    materials.TreeBark = "pbr"
    materials.SkyDome = "unlit"
    data.Materials = materials

    # Settings
    settings = VarMap()
    settings.RenderQuality = "high"
    settings.ShadowResolution = 2048
    settings.AntiAliasing = "MSAA4x"
    data.Settings = settings

    return data

  def onGpuInit(self, ctx):
    # Set up theme engine for SDF rendering
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

  def onUpdate(self, updinfo):
    pass

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

OutlinerTest().ezapp.mainThreadLoop()
