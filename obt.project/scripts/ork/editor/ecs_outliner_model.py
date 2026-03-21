################################################################################
# ECS Outliner Model - Outliner model for ECS scene hierarchy
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

from orkengine import core
from orkengine import lev2
from orkengine import ecs

################################################################################

# Categories shown at the root of the outliner tree
CATEGORIES = ["Archetypes", "Spawners", "Systems"]

def _reflectionNameToFactoryId(name):
  """Convert a reflection class name to a factory ID.
     e.g. 'BoidsComponentData' -> 'BoidsComponent'
          'EcsBulletObjectComponentData' -> 'BulletObjectComponent'
          'EcsBulletSystemData' -> 'BulletSystem'
  """
  if name.startswith("Ecs"):
    name = name[3:]
  if name.endswith("Data"):
    name = name[:-4]
  return name

def _enumerateComponentTypes():
  return [_reflectionNameToFactoryId(n)
          for n in core.enumerateInstantiableSubclassesOf("ComponentData")]

def _enumerateSystemTypes():
  return [_reflectionNameToFactoryId(n)
          for n in core.enumerateInstantiableSubclassesOf("SystemData")]

def _enumerateDrawableDataTypes():
  return core.enumerateInstantiableSubclassesOf("DrawableData")

################################################################################

class EcsOutlinerModel(lev2.ui.OutlinerModel):
  """Outliner model that queries a live ecs.SceneData.

  Tree structure:
    Archetypes/
      BallArchetype/
        SceneGraphComponent/
          nodename
        PythonComponent
    Spawners/
      ball_spawner/
    Systems/
      SceneGraphSystem/
  """

  def __init__(self, editor):
    super().__init__()
    self.editor = editor
    self.allow_rename = True
    self.allow_delete = True
    self.allow_add = True
    self._visible_categories = set(CATEGORIES)  # all visible by default

  @property
  def scene_data(self):
    return self.editor.scene_data

  # ============================================================
  # Namespace helpers
  # ============================================================

  def _getTopLevelNamespaces(self):
    """Return set of top-level namespace segments from scene_data.imports.
    For imports like {"env": ..., "env:props": ..., "fx": ...},
    returns {"env", "fx"}."""
    result = set()
    for ns_key in self.scene_data.imports.keys():
      result.add(ns_key.split(":")[0])
    return result

  def _isNamespace(self, name):
    """Check if name is a top-level namespace (or prefix of a nested one)."""
    for ns_key in self.scene_data.imports.keys():
      if ns_key == name or ns_key.startswith(name + ":"):
        return True
    return False

  def _getChildNamespaces(self, parent_ns):
    """Given a parent namespace like "env", return immediate child
    namespace segments. For imports {"env": ..., "env:props": ...,
    "env:props:sub": ...}, _getChildNamespaces("env") returns {"props"}.
    """
    prefix = parent_ns + ":"
    result = set()
    for ns_key in self.scene_data.imports.keys():
      if ns_key.startswith(prefix):
        remainder = ns_key[len(prefix):]
        child_seg = remainder.split(":")[0]
        result.add(child_seg)
    return result

  def _getImportedScene(self, ns_key):
    """Get the loaded imported SceneData for a namespace key.
    Returns None if controller not available or scene not loaded."""
    controller = getattr(self.editor.runtime, 'controller', None)
    if controller:
      return controller.findImportedScene(ns_key)
    return None

  def _namespaceHasSelectedItems(self, top_ns, category):
    """Check if a top-level namespace (or any of its children) has selected
    items for the given category."""
    imports = self.scene_data.imports
    for ns_key in imports:
      if ns_key == top_ns or ns_key.startswith(top_ns + ":"):
        if len(self._getSelectedNames(ns_key, category)) > 0:
          return True
    return False

  def _parseImportPath(self, category, parts):
    """Given parts like ["Archetypes", "env", "props", "ChairArch", "Comp"],
    determine if this is an imported item path.
    Returns (ns_key, object_name, remainder_parts) or None.

    Walks segments from parts[1] onward, building a colon-joined namespace
    candidate. The longest match against scene_data.imports wins.
    The segment after the namespace match is the object name.
    """
    if len(parts) < 2:
      return None
    imports = self.scene_data.imports
    best_ns = None
    best_idx = 0
    candidate = ""
    for i in range(1, len(parts)):
      if candidate:
        candidate += ":" + parts[i]
      else:
        candidate = parts[i]
      if candidate in imports:
        best_ns = candidate
        best_idx = i
    if best_ns is None:
      # Also check if parts[1] is a prefix of a namespace (intermediate folder)
      if len(parts) >= 2 and self._isNamespace(parts[1]):
        # Build the partial namespace from segments
        partial = parts[1]
        for i in range(2, len(parts)):
          next_partial = partial + ":" + parts[i]
          if any(k == next_partial or k.startswith(next_partial + ":") for k in imports):
            partial = next_partial
          else:
            # parts[i] is beyond the namespace — it's an object name
            # but only if partial is a real namespace
            if partial in imports:
              return (partial, parts[i], list(parts[i+1:]))
            else:
              # partial is an intermediate folder, not a full namespace
              return (partial, None, [])
        # All segments consumed, we're at a namespace folder
        if partial in imports:
          return (partial, None, [])
        else:
          return (partial, None, [])
      return None
    obj_idx = best_idx + 1
    if obj_idx >= len(parts):
      return (best_ns, None, [])
    return (best_ns, parts[obj_idx], list(parts[obj_idx+1:]))

  def _findInScene(self, scene, collection, name):
    """Find an object by name in an imported scene's collection.
    collection is "archetypes", "spawners", or "systemDatas"."""
    for obj in getattr(scene, collection):
      attr = "name" if collection != "systemDatas" else "className"
      if getattr(obj, attr) == name:
        return obj
    return None

  # ============================================================
  # OutlinerModel interface
  # ============================================================

  def toggleCategory(self, category):
    """Toggle visibility of a root category. Returns new visibility state."""
    if category in self._visible_categories:
      self._visible_categories.discard(category)
      self.notifyModelReset()
      return False
    else:
      self._visible_categories.add(category)
      self.notifyModelReset()
      return True

  def isCategoryVisible(self, category):
    return category in self._visible_categories

  def getChildren(self, parent_key):
    if parent_key == "":
      return [c for c in CATEGORIES if c in self._visible_categories]

    parts = parent_key.split("/")
    category = parts[0]

    if category == "Archetypes":
      if len(parts) == 1:
        children = [f"Archetypes/{a.name}" for a in self.scene_data.archetypes]
        for ns in sorted(self._getTopLevelNamespaces()):
          if self._namespaceHasSelectedItems(ns, category):
            children.append(f"Archetypes/{ns}")
        return children
      # Check if this is a namespace path
      parsed = self._parseImportPath(category, parts)
      if parsed is not None:
        return self._getImportChildren(parent_key, category, parsed)
      # Local archetype children
      if len(parts) == 2:
        arch = self._findArchetype(parts[1])
        if arch:
          return [f"{parent_key}/{c.className}" for c in arch.components]
      elif len(parts) == 3:
        comp = self._findComponent(parts[1], parts[2])
        if comp and comp.className == "SceneGraphComponentData":
          nodedatas = comp.nodedatas
          return [f"{parent_key}/{name}" for name in nodedatas.keys()]
      return []

    elif category == "Spawners":
      if len(parts) == 1:
        children = [f"Spawners/{s.name}" for s in self.scene_data.spawners]
        for ns in sorted(self._getTopLevelNamespaces()):
          if self._namespaceHasSelectedItems(ns, category):
            children.append(f"Spawners/{ns}")
        return children
      parsed = self._parseImportPath(category, parts)
      if parsed is not None:
        return self._getImportChildren(parent_key, category, parsed)
      return []

    elif category == "Systems":
      if len(parts) == 1:
        children = [f"Systems/{s.className}" for s in self.scene_data.systemDatas]
        for ns in sorted(self._getTopLevelNamespaces()):
          if self._namespaceHasSelectedItems(ns, category):
            children.append(f"Systems/{ns}")
        return children
      parsed = self._parseImportPath(category, parts)
      if parsed is not None:
        return self._getImportChildren(parent_key, category, parsed)
      return []

    return []

  def _getSelectedNames(self, ns_key, category):
    """Return the set of selected (enabled) item names for a namespace+category."""
    imports = self.scene_data.imports
    if ns_key not in imports:
      return set()
    import_data = imports[ns_key]
    if category == "Archetypes":
      return set(import_data.selectedArchetypes)
    elif category == "Spawners":
      return set(import_data.selectedSpawners)
    elif category == "Systems":
      return set(import_data.selectedSystems)
    return set()

  def _getImportChildren(self, parent_key, category, parsed):
    """Return children for a namespace folder or imported object."""
    ns_key, obj_name, remainder = parsed
    if obj_name is None:
      # Namespace folder — show child namespace folders + selected imported objects
      children = []
      for child_ns in sorted(self._getChildNamespaces(ns_key)):
        children.append(f"{parent_key}/{child_ns}")
      imported_scene = self._getImportedScene(ns_key)
      if imported_scene:
        selected = self._getSelectedNames(ns_key, category)
        if category == "Archetypes":
          for a in imported_scene.archetypes:
            if a.name in selected:
              children.append(f"{parent_key}/{a.name}")
        elif category == "Spawners":
          for s in imported_scene.spawners:
            if s.name in selected:
              children.append(f"{parent_key}/{s.name}")
        elif category == "Systems":
          for s in imported_scene.systemDatas:
            if s.className in selected:
              children.append(f"{parent_key}/{s.className}")
      return children
    # Imported object — show its children
    if category == "Archetypes" and len(remainder) == 0:
      imported_scene = self._getImportedScene(ns_key)
      if imported_scene:
        arch = self._findInScene(imported_scene, "archetypes", obj_name)
        if arch:
          return [f"{parent_key}/{c.className}" for c in arch.components]
    if category == "Archetypes" and len(remainder) == 1:
      imported_scene = self._getImportedScene(ns_key)
      if imported_scene:
        arch = self._findInScene(imported_scene, "archetypes", obj_name)
        if arch:
          comp_name = remainder[0]
          for c in arch.components:
            if c.className == comp_name and comp_name == "SceneGraphComponentData":
              return [f"{parent_key}/{name}" for name in c.nodedatas.keys()]
    return []

  def getDisplayName(self, key):
    if "/" in key:
      return key.split("/")[-1]
    return key

  def hasChildren(self, key):
    parts = key.split("/")
    category = parts[0]

    if category in CATEGORIES and len(parts) == 1:
      has_imports = len(self._getTopLevelNamespaces()) > 0
      if category == "Archetypes":
        return len(self.scene_data.archetypes) > 0 or has_imports
      elif category == "Spawners":
        return len(self.scene_data.spawners) > 0 or has_imports
      elif category == "Systems":
        return len(self.scene_data.systemDatas) > 0 or has_imports

    # Check for namespace paths
    if len(parts) >= 2:
      parsed = self._parseImportPath(category, parts)
      if parsed is not None:
        ns_key, obj_name, remainder = parsed
        if obj_name is None:
          return True  # namespace folder always has children
        if category == "Archetypes" and len(remainder) == 0:
          imported_scene = self._getImportedScene(ns_key)
          if imported_scene:
            arch = self._findInScene(imported_scene, "archetypes", obj_name)
            return arch is not None and len(arch.components) > 0
        if category == "Archetypes" and len(remainder) == 1:
          imported_scene = self._getImportedScene(ns_key)
          if imported_scene:
            arch = self._findInScene(imported_scene, "archetypes", obj_name)
            if arch:
              comp_name = remainder[0]
              for c in arch.components:
                if c.className == comp_name and comp_name == "SceneGraphComponentData":
                  return len(c.nodedatas) > 0
        return False

    # Local archetype has component children
    if category == "Archetypes" and len(parts) == 2:
      arch = self._findArchetype(parts[1])
      return arch is not None and len(arch.components) > 0

    # SceneGraphComponent has node children
    if category == "Archetypes" and len(parts) == 3:
      comp = self._findComponent(parts[1], parts[2])
      if comp and comp.className == "SceneGraphComponentData":
        return len(comp.nodedatas) > 0

    return False

  def getValue(self, key):
    return None

  def getFactories(self, parent_key):
    parts = parent_key.split("/")
    category = parts[0]

    # No factories for imported items
    if len(parts) >= 2:
      parsed = self._parseImportPath(category, parts)
      if parsed is not None:
        return []

    if category == "Archetypes" and len(parts) == 1:
      return [{
        "id": "archetype",
        "display_name": "Archetype",
        "default_name_generator": lambda m: f"Archetype{len(self.scene_data.archetypes)}"
      }]

    if category == "Spawners" and len(parts) == 1:
      return [{
        "id": "spawner",
        "display_name": "Spawner",
        "default_name_generator": lambda m: f"spawner{len(self.scene_data.spawners)}"
      }]

    if category == "Systems" and len(parts) == 1:
      return [{
        "id": sys_type,
        "display_name": sys_type,
        "default_name_generator": lambda m, st=sys_type: st
      } for sys_type in _enumerateSystemTypes()
        if not self._hasSystem(sys_type)]

    # Components under an archetype
    if category == "Archetypes" and len(parts) == 2:
      arch = self._findArchetype(parts[1])
      if arch:
        existing = {c.className for c in arch.components}
        return [{
          "id": ct,
          "display_name": ct,
          "default_name_generator": lambda m, c=ct: c
        } for ct in _enumerateComponentTypes() if ct not in existing]

    # Nodes under a SceneGraphComponent
    if category == "Archetypes" and len(parts) == 3:
      comp = self._findComponent(parts[1], parts[2])
      if comp and comp.className == "SceneGraphComponentData":
        return [{
          "id": "sgnode",
          "display_name": "Node",
          "default_name_generator": lambda m: f"node{len(comp.nodedatas)}"
        }]

    return []

  def createItem(self, parent_key, name, factory_id):
    parts = parent_key.split("/")
    category = parts[0]

    # Can't create under imported items
    if len(parts) >= 2:
      parsed = self._parseImportPath(category, parts)
      if parsed is not None:
        return ""

    if category == "Archetypes" and len(parts) == 1 and factory_id == "archetype":
      if self._findArchetype(name):
        return ""
      if self._isNamespace(name):
        return ""  # name collides with import namespace
      self.scene_data.declareArchetype(name)
      new_key = f"Archetypes/{name}"
      self.notifyItemAdded(new_key)
      return new_key

    if category == "Spawners" and len(parts) == 1 and factory_id == "spawner":
      if self._findSpawner(name):
        return ""
      if self._isNamespace(name):
        return ""
      self.scene_data.declareSpawner(name)
      new_key = f"Spawners/{name}"
      self.notifyItemAdded(new_key)
      return new_key

    if category == "Systems" and len(parts) == 1:
      if self._hasSystem(factory_id):
        return ""
      self.scene_data.declareSystem(factory_id)
      new_key = f"Systems/{factory_id}"
      self.notifyItemAdded(new_key)
      return new_key

    # Adding component to archetype
    if category == "Archetypes" and len(parts) == 2:
      arch = self._findArchetype(parts[1])
      if arch:
        existing = {c.className for c in arch.components}
        if factory_id in existing:
          return ""
        arch.declareComponent(factory_id)
        new_key = f"{parent_key}/{factory_id}"
        self.notifyItemAdded(new_key)
        return new_key

    # Adding node to SceneGraphComponent
    if category == "Archetypes" and len(parts) == 3 and factory_id == "sgnode":
      comp = self._findComponent(parts[1], parts[2])
      if comp and comp.className == "SceneGraphComponentData":
        if name in comp.nodedatas:
          return ""
        comp.addNode(name, "std_forward")
        new_key = f"{parent_key}/{name}"
        self.notifyItemAdded(new_key)
        return new_key

    return ""

  def renameItem(self, old_key, new_name):
    parts = old_key.split("/")
    category = parts[0]

    if len(parts) == 1:
      return None
    # Reject rename for imported items
    if len(parts) >= 2:
      parsed = self._parseImportPath(category, parts)
      if parsed is not None:
        return None
    if category == "Systems":
      return None
    if category == "Archetypes" and len(parts) == 3:
      return None
    if category == "Archetypes" and len(parts) == 4:
      return None

    # Reject renaming to a namespace name
    if self._isNamespace(new_name):
      return None

    old_name = parts[-1]

    if category == "Archetypes" and len(parts) == 2:
      arch = self._findArchetype(old_name)
      if arch and not self._findArchetype(new_name):
        self.scene_data.renameSceneObject(arch, new_name)
        return f"Archetypes/{new_name}"

    if category == "Spawners" and len(parts) == 2:
      sp = self._findSpawner(old_name)
      if sp and not self._findSpawner(new_name):
        self.scene_data.renameSceneObject(sp, new_name)
        return f"Spawners/{new_name}"

    return None

  def removeItem(self, key):
    parts = key.split("/")
    category = parts[0]

    if len(parts) == 1:
      return
    # Reject removal for imported items
    if len(parts) >= 2:
      parsed = self._parseImportPath(category, parts)
      if parsed is not None:
        return

    if category == "Archetypes":
      if len(parts) == 2:
        arch = self._findArchetype(parts[1])
        if arch:
          self.scene_data.removeSceneObject(arch)
      elif len(parts) == 3:
        arch = self._findArchetype(parts[1])
        if arch:
          comp_name = parts[2]
          for c in arch.components:
            if c.className == comp_name:
              arch.removeComponent(c)
              break
      elif len(parts) == 4:
        comp = self._findComponent(parts[1], parts[2])
        if comp and comp.className == "SceneGraphComponentData":
          comp.removeNode(parts[3])

    elif category == "Spawners" and len(parts) == 2:
      sp = self._findSpawner(parts[1])
      if sp:
        self.scene_data.removeSceneObject(sp)

    elif category == "Systems" and len(parts) == 2:
      self.scene_data.removeSystem(parts[1])

  # ============================================================
  # Helpers
  # ============================================================

  def _findArchetype(self, name):
    for a in self.scene_data.archetypes:
      if a.name == name:
        return a
    return None

  def _findSpawner(self, name):
    for s in self.scene_data.spawners:
      if s.name == name:
        return s
    return None

  def _findComponent(self, arch_name, comp_class_name):
    arch = self._findArchetype(arch_name)
    if arch:
      for c in arch.components:
        if c.className == comp_class_name:
          return c
    return None

  def _hasSystem(self, class_name):
    for s in self.scene_data.systemDatas:
      if s.className == class_name:
        return True
    return False
