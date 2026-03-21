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
CATEGORIES = ["Archetypes", "Spawners", "Systems", "Imports"]

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
        # List all archetypes
        return [f"Archetypes/{a.name}" for a in self.scene_data.archetypes]
      elif len(parts) == 2:
        # List components of an archetype
        arch_name = parts[1]
        arch = self._findArchetype(arch_name)
        if arch:
          return [f"{parent_key}/{c.className}" for c in arch.components]
      elif len(parts) == 3:
        # List nodes of a SceneGraphComponent
        comp = self._findComponent(parts[1], parts[2])
        if comp and comp.className == "SceneGraphComponentData":
          nodedatas = comp.nodedatas
          return [f"{parent_key}/{name}" for name in nodedatas.keys()]
      return []

    elif category == "Spawners":
      if len(parts) == 1:
        return [f"Spawners/{s.name}" for s in self.scene_data.spawners]
      return []

    elif category == "Systems":
      if len(parts) == 1:
        return [f"Systems/{s.className}" for s in self.scene_data.systemDatas]
      return []

    elif category == "Imports":
      if len(parts) == 1:
        imports = self.scene_data.imports
        return [f"Imports/{ns}" for ns in sorted(imports.keys())]
      elif len(parts) == 2:
        # Show sub-categories under an import namespace
        return [f"{parent_key}/Archetypes", f"{parent_key}/Spawners", f"{parent_key}/Systems"]
      elif len(parts) == 3:
        ns = parts[1]
        sub = parts[2]
        # Try to get the imported scene from the controller
        controller = getattr(self.editor.runtime, 'controller', None)
        if controller:
          imported_scene = controller.findImportedScene(ns)
          if imported_scene:
            if sub == "Archetypes":
              return [f"{parent_key}/{a.name}" for a in imported_scene.archetypes]
            elif sub == "Spawners":
              return [f"{parent_key}/{s.name}" for s in imported_scene.spawners]
            elif sub == "Systems":
              return [f"{parent_key}/{s.className}" for s in imported_scene.systemDatas]
      return []

    return []

  def getDisplayName(self, key):
    if "/" in key:
      return key.split("/")[-1]
    return key

  def hasChildren(self, key):
    parts = key.split("/")
    category = parts[0]

    if category in CATEGORIES and len(parts) == 1:
      if category == "Archetypes":
        return len(self.scene_data.archetypes) > 0
      elif category == "Spawners":
        return len(self.scene_data.spawners) > 0
      elif category == "Systems":
        return len(self.scene_data.systemDatas) > 0
      elif category == "Imports":
        return len(self.scene_data.imports) > 0

    # Archetypes have component children
    if category == "Archetypes" and len(parts) == 2:
      arch = self._findArchetype(parts[1])
      return arch is not None and len(arch.components) > 0

    # SceneGraphComponent has node children
    if category == "Archetypes" and len(parts) == 3:
      comp = self._findComponent(parts[1], parts[2])
      if comp and comp.className == "SceneGraphComponentData":
        return len(comp.nodedatas) > 0

    # Import namespaces always have sub-categories
    if category == "Imports" and len(parts) == 2:
      return True

    # Import sub-categories may have items
    if category == "Imports" and len(parts) == 3:
      ns = parts[1]
      sub = parts[2]
      controller = getattr(self.editor.runtime, 'controller', None)
      if controller:
        imported_scene = controller.findImportedScene(ns)
        if imported_scene:
          if sub == "Archetypes":
            return len(imported_scene.archetypes) > 0
          elif sub == "Spawners":
            return len(imported_scene.spawners) > 0
          elif sub == "Systems":
            return len(imported_scene.systemDatas) > 0

    return False

  def getValue(self, key):
    return None

  def getFactories(self, parent_key):
    parts = parent_key.split("/")
    category = parts[0]

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

    if category == "Archetypes" and len(parts) == 1 and factory_id == "archetype":
      # Check name collision
      if self._findArchetype(name):
        return ""
      self.scene_data.declareArchetype(name)
      new_key = f"Archetypes/{name}"
      self.notifyItemAdded(new_key)

      return new_key

    if category == "Spawners" and len(parts) == 1 and factory_id == "spawner":
      if self._findSpawner(name):
        return ""
      self.scene_data.declareSpawner(name)
      new_key = f"Spawners/{name}"
      self.notifyItemAdded(new_key)

      return new_key

    if category == "Systems" and len(parts) == 1:
      # factory_id is the system class name
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

    # Can't rename categories, systems, components, or imported objects
    if len(parts) == 1:
      return None
    if category == "Imports":
      return None  # imported objects are read-only
    if category == "Systems":
      return None
    if category == "Archetypes" and len(parts) == 3:
      return None  # Can't rename components
    if category == "Archetypes" and len(parts) == 4:
      return None  # Can't rename nodes (yet)

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
      return  # Can't remove categories
    if category == "Imports":
      return  # imported objects are read-only (use Reference Manager to un-import)

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
        # Remove node from SceneGraphComponent
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
