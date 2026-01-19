################################################################################
# Scene Outliner Model - Base class for scene hierarchy outliner
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

from orkengine import lev2

################################################################################

class SceneOutlinerModel(lev2.ui.OutlinerModel):
  """Base outliner model that queries scenegraph directly.

  Subclasses should override:
    - getNodeTypes(): Return list of node type definitions
    - findNode(name, item_type): Find a node by name and type
    - createNode(name, item_type): Create a node of given type
    - deleteNode(name, item_type): Delete a node
    - renameNode(old_name, new_name, item_type): Rename a node

  Node type definition format:
    {
      "key": "Nodes",              # Outliner group key
      "item_type": "node",         # Internal type identifier
      "display_name": "Node",      # Display name for factory
      "type_token": tokens.model,  # CrcString token for scenegraph query
      "default_name": lambda n: f"node{n}"  # Name generator
    }
  """

  def __init__(self, editor):
    super().__init__()
    self.editor = editor
    self.allow_rename = True
    self.allow_delete = True
    self.allow_add = True
    self._node_types = None

  def _getScenegraph(self):
    """Get the scenegraph instance. Override if needed."""
    return self.editor.scenegraph

  def getNodeTypes(self):
    """Return list of node type definitions. Override in subclass."""
    raise NotImplementedError("Subclass must implement getNodeTypes()")

  def _getNodeTypesCached(self):
    """Get node types with caching."""
    if self._node_types is None:
      self._node_types = self.getNodeTypes()
    return self._node_types

  def _getNodeTypeByKey(self, key):
    """Find node type definition by group key."""
    for nt in self._getNodeTypesCached():
      if nt["key"] == key:
        return nt
    return None

  def _getNodeTypeByItemType(self, item_type):
    """Find node type definition by item_type."""
    for nt in self._getNodeTypesCached():
      if nt["item_type"] == item_type:
        return nt
    return None

  def getChildren(self, parent_key):
    if parent_key == "":
      return [nt["key"] for nt in self._getNodeTypesCached()]

    nt = self._getNodeTypeByKey(parent_key)
    if nt:
      sg = self._getScenegraph()
      if "drawable_type" in nt:
        nodes = sg.drawableNodesWithType(nt["drawable_type"])
      elif "light_type" in nt:
        nodes = sg.lightNodesWithType(nt["light_type"])
      else:
        return []
      return [f"{parent_key}/{n.name}" for n in nodes]

    return []

  def getDisplayName(self, key):
    if "/" in key:
      return key.split("/")[-1]
    return key

  def hasChildren(self, key):
    nt = self._getNodeTypeByKey(key)
    if nt:
      sg = self._getScenegraph()
      if "drawable_type" in nt:
        return len(sg.drawableNodesWithType(nt["drawable_type"])) > 0
      elif "light_type" in nt:
        return len(sg.lightNodesWithType(nt["light_type"])) > 0
    return False

  def getValue(self, key):
    return None

  def _getItemType(self, key):
    """Determine item type from key."""
    for nt in self._getNodeTypesCached():
      if key == nt["key"]:
        return "group"
      elif key.startswith(nt["key"] + "/"):
        return nt["item_type"]
    return None

  def getFactories(self, parent_key):
    nt = self._getNodeTypeByKey(parent_key)
    if nt:
      return [{
        "id": nt["item_type"],
        "display_name": nt["display_name"],
        "default_name_generator": nt.get("default_name", lambda m: f"item{0}")
      }]
    return []

  def findNode(self, name, item_type):
    """Find a node by name and type. Override in subclass."""
    raise NotImplementedError("Subclass must implement findNode()")

  def createNode(self, name, item_type):
    """Create a node. Override in subclass."""
    raise NotImplementedError("Subclass must implement createNode()")

  def deleteNode(self, name, item_type):
    """Delete a node. Override in subclass."""
    raise NotImplementedError("Subclass must implement deleteNode()")

  def renameNode(self, old_name, new_name, item_type):
    """Rename a node. Override in subclass."""
    raise NotImplementedError("Subclass must implement renameNode()")

  def createItem(self, parent_key, name, factory_id):
    nt = self._getNodeTypeByKey(parent_key)
    if nt and nt["item_type"] == factory_id:
      if self.findNode(name, factory_id) is not None:
        return ""  # Name collision
      self.createNode(name, factory_id)
      new_key = f"{parent_key}/{name}"
      self.notifyItemAdded(new_key)
      return new_key
    return ""

  def renameItem(self, old_key, new_name):
    item_type = self._getItemType(old_key)
    if item_type == "group":
      return None  # Can't rename groups

    old_name = old_key.split("/")[-1]

    # Check for collision
    if self.findNode(new_name, item_type) is not None:
      return None

    # Build new key
    last_slash = old_key.rfind("/")
    new_key = (old_key[:last_slash + 1] + new_name) if last_slash >= 0 else new_name

    # Perform rename
    self.renameNode(old_name, new_name, item_type)
    return new_key

  def removeItem(self, key):
    item_type = self._getItemType(key)
    if item_type == "group":
      return  # Can't delete groups

    name = key.split("/")[-1]
    self.deleteNode(name, item_type)
