################################################################################
# ECS Reference Manager - Manage scene imports and references
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

from orkengine import core
from orkengine import lev2
from orkengine import ecs

################################################################################

class EcsReferenceManagerModel(lev2.ui.OutlinerModel):
  """Outliner model for the Reference Manager tab.

  Tree structure (plan section 3B):
    <namespace> <- <path>
      Archetypes/
        [x] TreeArchetype           (selected for import)
        [ ] RockArchetype           (available but not selected)
      Spawners/
        [x] tree_row_1
      Systems/
        [ ] SceneGraphSystemData    or [!] ... (conflict)

  Features:
    - "+ Import File" via getFactories on root
    - Checkbox toggling via double-click
    - "Create Reference" via getFactories on imported archetypes
    - "Un-import" via removeItem on namespace
    - Conflict detection for SystemData
  """

  def __init__(self, editor):
    super().__init__()
    self.editor = editor
    self.allow_rename = False
    self.allow_delete = True
    self.allow_add = True

  @property
  def scene_data(self):
    return self.editor.scene_data

  def _getController(self):
    return getattr(self.editor.runtime, 'controller', None)

  def _getImportedScene(self, ns):
    ctrl = self._getController()
    if ctrl:
      return ctrl.findImportedScene(ns)
    return None

  # ============================================================
  # OutlinerModel interface
  # ============================================================

  def getChildren(self, parent_key):
    if parent_key == "":
      imports = self.scene_data.imports
      if not imports:
        return []
      return sorted(imports.keys())

    parts = parent_key.split("/")

    # Info-only placeholder
    if parts[0].startswith("("):
      return []

    ns = parts[0]
    imports = self.scene_data.imports
    if ns not in imports:
      return []
    import_data = imports[ns]

    if len(parts) == 1:
      # Sub-categories under a namespace
      return [
        f"{ns}/Archetypes",
        f"{ns}/Spawners",
        f"{ns}/Systems",
      ]

    if len(parts) == 2:
      sub = parts[1]
      imported_scene = self._getImportedScene(ns)
      if not imported_scene:
        return [f"{parent_key}/(scene not loaded)"]

      if sub == "Archetypes":
        selected = set(import_data.selectedArchetypes)
        return [f"{parent_key}/{'[x]' if a.name in selected else '[ ]'} {a.name}"
                for a in imported_scene.archetypes]

      elif sub == "Spawners":
        selected = set(import_data.selectedSpawners)
        return [f"{parent_key}/{'[x]' if s.name in selected else '[ ]'} {s.name}"
                for s in imported_scene.spawners]

      elif sub == "Systems":
        selected = set(import_data.selectedSystems)
        local_systems = {s.className for s in self.scene_data.systemDatas}
        items = []
        for s in imported_scene.systemDatas:
          name = s.className
          if name in local_systems:
            items.append(f"{parent_key}/[!] {name} (conflict)")
          else:
            prefix = "[x]" if name in selected else "[ ]"
            items.append(f"{parent_key}/{prefix} {name}")
        return items

    return []

  def getDisplayName(self, key):
    parts = key.split("/")
    ns = parts[0]

    # Root level: show "namespace <- path"
    if len(parts) == 1:
      if ns.startswith("("):
        return ns
      imports = self.scene_data.imports
      if ns in imports:
        path = imports[ns].sourcePath
        if path:
          return f'{ns} \u2190 {path}'
      return ns

    return parts[-1]

  def hasChildren(self, key):
    parts = key.split("/")
    if len(parts) == 1:
      return True  # namespaces have sub-categories
    if len(parts) == 2:
      sub = parts[1]
      if sub.startswith("("):
        return False
      return sub in ("Archetypes", "Spawners", "Systems")
    return False

  def getValue(self, key):
    return None

  def getFactories(self, parent_key):
    parts = parent_key.split("/") if parent_key else []

    # On an imported archetype item: allow creating a ReferenceArchetype
    if len(parts) == 3 and parts[1] == "Archetypes":
      item_display = parts[2]
      if item_display.startswith("[x]") or item_display.startswith("[ ]"):
        arch_name = item_display[4:]
        ns = parts[0]
        return [{
          "id": "create_reference",
          "display_name": f"Create Reference to {arch_name}",
          "default_name_generator": lambda m, n=arch_name: f"{n}Ref"
        }]

    return []

  def createItem(self, parent_key, name, factory_id):
    if factory_id == "create_reference":
      # Create a ReferenceArchetype in the local scene
      parts = parent_key.split("/")
      if len(parts) == 3 and parts[1] == "Archetypes":
        ns = parts[0]
        item_display = parts[2]
        arch_name = item_display[4:]  # skip "[x] " or "[ ] "

        ref_arch = ecs.ReferenceArchetype()
        ref_arch.importNamespace = ns
        ref_arch.archetypeName = arch_name
        ref_arch.name = name
        self.scene_data.addSceneObject(ref_arch)

        # Also notify the main outliner
        if hasattr(self.editor, 'outliner_model'):
          self.editor.outliner_model.notifyModelReset()
        return ""  # don't add to reference manager tree

    return ""

  def removeItem(self, key):
    parts = key.split("/")
    if len(parts) == 1 and not parts[0].startswith("("):
      # Remove an entire import namespace
      ns = parts[0]
      self.scene_data.removeImport(ns)
      self.notifyModelReset()
      # Also refresh outliner imports
      if hasattr(self.editor, 'outliner_model'):
        self.editor.outliner_model.notifyModelReset()

  def renameItem(self, old_key, new_name):
    return None  # no renaming

  # ============================================================
  # Toggle selection (called on double-click from ecsedit.py)
  # ============================================================

  def toggleSelection(self, key):
    """Toggle the import-selection state of an item (checkbox)."""
    parts = key.split("/")
    if len(parts) != 3:
      return

    ns = parts[0]
    sub = parts[1]
    item_display = parts[2]

    # Can't toggle conflicts or info items
    if item_display.startswith("[!]") or item_display.startswith("("):
      return
    if not (item_display.startswith("[x]") or item_display.startswith("[ ]")):
      return

    item_name = item_display[4:]  # skip "[x] " or "[ ] "
    is_selected = item_display.startswith("[x]")

    imports = self.scene_data.imports
    if ns not in imports:
      return
    import_data = imports[ns]

    if sub == "Archetypes":
      sel = list(import_data.selectedArchetypes)
      if is_selected:
        sel = [s for s in sel if s != item_name]
      else:
        sel.append(item_name)
      import_data.selectedArchetypes = sel

    elif sub == "Spawners":
      sel = list(import_data.selectedSpawners)
      if is_selected:
        sel = [s for s in sel if s != item_name]
      else:
        sel.append(item_name)
      import_data.selectedSpawners = sel

    elif sub == "Systems":
      sel = list(import_data.selectedSystems)
      if is_selected:
        sel = [s for s in sel if s != item_name]
      else:
        sel.append(item_name)
      import_data.selectedSystems = sel

    self.notifyModelReset()
