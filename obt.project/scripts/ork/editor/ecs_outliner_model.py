################################################################################
# ECS Outliner Model - Outliner model for ECS scene hierarchy
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

from orkengine import lev2
from orkengine import ecs

################################################################################

# Categories shown at the root of the outliner tree
CATEGORIES = ["Archetypes", "Spawners", "Systems"]

# Available component types for the "Add Component" factory
COMPONENT_TYPES = [
  "SceneGraphComponent",
  "BulletObjectComponent",
  "PythonComponent",
]

# Available system types for the "Add System" factory
SYSTEM_TYPES = [
  "SceneGraphSystem",
  "BulletSystem",
  "PythonSystem",
]

################################################################################

class EcsOutlinerModel(lev2.ui.OutlinerModel):
  """Outliner model that queries a live ecs.SceneData.

  Tree structure:
    Archetypes/
      BallArchetype/
        SceneGraphComponent
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

  @property
  def scene_data(self):
    return self.editor.scene_data

  # ============================================================
  # OutlinerModel interface
  # ============================================================

  def getChildren(self, parent_key):
    if parent_key == "":
      return list(CATEGORIES)

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
      return []

    elif category == "Spawners":
      if len(parts) == 1:
        return [f"Spawners/{s.name}" for s in self.scene_data.spawners]
      return []

    elif category == "Systems":
      if len(parts) == 1:
        return [f"Systems/{s.className}" for s in self.scene_data.systemDatas]
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

    # Archetypes have component children
    if category == "Archetypes" and len(parts) == 2:
      arch = self._findArchetype(parts[1])
      return arch is not None and len(arch.components) > 0

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
      } for sys_type in SYSTEM_TYPES
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
        } for ct in COMPONENT_TYPES if ct not in existing]

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

    return ""

  def renameItem(self, old_key, new_name):
    parts = old_key.split("/")
    category = parts[0]

    # Can't rename categories, systems, or components
    if len(parts) == 1:
      return None
    if category == "Systems":
      return None
    if category == "Archetypes" and len(parts) == 3:
      return None  # Can't rename components

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

  def _hasSystem(self, class_name):
    for s in self.scene_data.systemDatas:
      if s.className == class_name:
        return True
    return False
