#!/usr/bin/env ork.python

################################################################################
# Outliner Widget Test - Custom Model Example
# Demonstrates subclassing OutlinerModel in Python
################################################################################

import signal
from orkengine.core import vec2, vec3, vec4, VarMap
from orkengine import lev2

################################################################################
# Custom Python Model
################################################################################

class CustomModel(lev2.ui.OutlinerModel):
  """A custom model that stores data in a Python dictionary."""

  def __init__(self):
    super().__init__()
    # Internal data structure: dict of key -> (display_name, children_keys, value)
    self._items = {}
    self._root_children = []
    self.allow_rename = True  # Enable rename support
    self.allow_delete = True  # Enable delete support
    self.allow_add = True     # Enable add support

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
      name = self._items[key].get("name")
      return name if name else key
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

  def removeItem(self, key):
    """Remove an item by key."""
    if key not in self._items:
      return

    # Remove from parent's children list
    last_slash = key.rfind("/")
    if last_slash >= 0:
      parent_key = key[:last_slash]
      if parent_key in self._items:
        children = self._items[parent_key]["children"]
        if key in children:
          children.remove(key)
    else:
      # Root level item
      if key in self._root_children:
        self._root_children.remove(key)

    # Recursively remove children
    item_data = self._items.get(key)
    if item_data:
      for child_key in list(item_data["children"]):
        self.removeItem(child_key)

    # Remove the item itself
    del self._items[key]

  def getFactories(self, parent_key):
    """Return list of factories for creating children under parent."""
    # Only allow adding to items that have children (groups)
    if parent_key == "" or (parent_key in self._items and len(self._items[parent_key]["children"]) >= 0):
      return [
        {"id": "group", "display_name": "Group"},
        {"id": "mesh", "display_name": "Mesh"},
        {"id": "light", "display_name": "Light"},
        {"id": "camera", "display_name": "Camera"},
      ]
    return []

  def createItem(self, parent_key, name, factory_id):
    """Create a new item using a factory."""
    # Build the full key
    if parent_key:
      new_key = parent_key + "/" + name
    else:
      new_key = name

    # Check if key already exists
    if new_key in self._items:
      return ""

    # Create the item with appropriate default value
    if factory_id == "group":
      value = None  # Groups have no value
    else:
      value = factory_id  # Use factory_id as the value (e.g., "mesh", "light")

    # Add to items
    self._items[new_key] = {"name": name, "children": [], "value": value}

    # Add to parent's children
    if parent_key:
      if parent_key in self._items:
        self._items[parent_key]["children"].append(new_key)
    else:
      self._root_children.append(new_key)

    self.notifyItemAdded(new_key)
    return new_key

  def renameItem(self, old_key, new_name):
    """Rename an item - changes both key and display name."""
    if old_key not in self._items:
      return None

    # Build new key: parent_path + "/" + new_name
    last_slash = old_key.rfind("/")
    if last_slash >= 0:
      new_key = old_key[:last_slash + 1] + new_name
    else:
      new_key = new_name

    # Move item data to new key
    item_data = self._items.pop(old_key)
    item_data["name"] = new_name
    self._items[new_key] = item_data

    # Update parent's children list
    if last_slash >= 0:
      parent_key = old_key[:last_slash]
      if parent_key in self._items:
        children = self._items[parent_key]["children"]
        idx = children.index(old_key)
        children[idx] = new_key
    else:
      # Root level item
      idx = self._root_children.index(old_key)
      self._root_children[idx] = new_key

    # Recursively update children keys
    self._updateChildrenKeys(old_key, new_key, item_data["children"])

    # Don't call notifyModelReset() - the outliner handles the rebuild
    # and preserves expanded state
    return new_key

  def _updateChildrenKeys(self, old_prefix, new_prefix, children):
    """Recursively update children keys after parent rename."""
    for i, old_child_key in enumerate(list(children)):
      # Build new child key
      new_child_key = new_prefix + old_child_key[len(old_prefix):]

      # Move child data
      if old_child_key in self._items:
        child_data = self._items.pop(old_child_key)
        self._items[new_child_key] = child_data
        children[i] = new_child_key

        # Recurse for grandchildren
        self._updateChildrenKeys(old_child_key, new_child_key, child_data["children"])

################################################################################

class OutlinerModelTest:

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

    # Use custom Python model
    self.custom_model = self._buildCustomModel()
    self.outliner.model = self.custom_model
    self.outliner.expandAll()

    # Set selection callback
    def on_select(key):
      print(f"Selected: {key}")
      model = self.outliner.model
      if model:
        value = model.getValue(key)
        print(f"  Value: {value}")

    self.outliner.onSelect(on_select)

    # Set rename callback
    def on_rename(old_key, new_name):
      print(f"Rename: {old_key} -> {new_name}")
      self.custom_model.renameItem(old_key, new_name)

    self.outliner.onRename(on_rename)

    # Set delete callback
    def on_delete(key):
      print(f"Deleted: {key}")

    self.outliner.onDelete(on_delete)

    # Set add callback
    def on_add(key):
      print(f"Added: {key}")

    self.outliner.onAdd(on_add)

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

OutlinerModelTest().ezapp.mainThreadLoop()
