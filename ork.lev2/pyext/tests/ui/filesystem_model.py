#!/usr/bin/env ork.python

################################################################################
# Filesystem Widget Test - Custom Model Example
# Demonstrates subclassing FilesystemModel in Python
# This creates a virtual filesystem backed by a dictionary
################################################################################

import signal
import time
from orkengine.core import vec2, vec3, vec4
from orkengine import lev2
from ork.ui import standard_icons, icon_library

################################################################################
# Custom Python Filesystem Model - Virtual Filesystem
################################################################################

class VirtualFilesystemModel(lev2.ui.FilesystemModel):
  """A custom model that provides a virtual filesystem backed by Python data.

  This demonstrates how to create custom filesystem models for:
  - Archive contents (zip, tar)
  - Asset databases
  - Remote filesystems
  - Mock filesystems for testing
  """

  def __init__(self):
    super().__init__()
    self._current_path = "/"
    # Virtual filesystem structure: path -> {name, type, size, children, data}
    self._entries = {}
    self._build_test_filesystem()

  def _build_test_filesystem(self):
    """Build a mock asset database structure."""
    now = int(time.time())

    # Root directory
    self._entries["/"] = {
      "name": "/",
      "type": "directory",
      "size": 0,
      "modified_time": now,
      "children": ["Models", "Textures", "Sounds", "Scripts"]
    }

    # Models directory
    self._entries["/Models"] = {
      "name": "Models",
      "type": "directory",
      "size": 0,
      "modified_time": now,
      "children": ["character.fbx", "vehicle.gltf", "building.obj", "tree.fbx"]
    }
    self._add_file("/Models/character.fbx", "character.fbx", 2500000, now)
    self._add_file("/Models/vehicle.gltf", "vehicle.gltf", 1800000, now)
    self._add_file("/Models/building.obj", "building.obj", 4200000, now)
    self._add_file("/Models/tree.fbx", "tree.fbx", 350000, now)

    # Textures directory
    self._entries["/Textures"] = {
      "name": "Textures",
      "type": "directory",
      "size": 0,
      "modified_time": now,
      "children": ["Characters", "Environment"]
    }

    # Characters textures
    self._entries["/Textures/Characters"] = {
      "name": "Characters",
      "type": "directory",
      "size": 0,
      "modified_time": now,
      "children": ["skin_diffuse.png", "skin_normal.png", "cloth_diffuse.png"]
    }
    self._add_file("/Textures/Characters/skin_diffuse.png", "skin_diffuse.png", 4194304, now, "png")
    self._add_file("/Textures/Characters/skin_normal.png", "skin_normal.png", 4194304, now, "png")
    self._add_file("/Textures/Characters/cloth_diffuse.png", "cloth_diffuse.png", 2097152, now, "png")

    # Environment textures
    self._entries["/Textures/Environment"] = {
      "name": "Environment",
      "type": "directory",
      "size": 0,
      "modified_time": now,
      "children": ["ground_diffuse.jpg", "ground_normal.jpg", "skybox.hdr"]
    }
    self._add_file("/Textures/Environment/ground_diffuse.jpg", "ground_diffuse.jpg", 1048576, now, "jpg")
    self._add_file("/Textures/Environment/ground_normal.jpg", "ground_normal.jpg", 1048576, now, "jpg")
    self._add_file("/Textures/Environment/skybox.hdr", "skybox.hdr", 8388608, now, "hdr")

    # Sounds directory
    self._entries["/Sounds"] = {
      "name": "Sounds",
      "type": "directory",
      "size": 0,
      "modified_time": now,
      "children": ["footstep.wav", "engine.ogg", "ambient.mp3", "music_theme.ogg"]
    }
    self._add_file("/Sounds/footstep.wav", "footstep.wav", 524288, now, "wav")
    self._add_file("/Sounds/engine.ogg", "engine.ogg", 1572864, now, "ogg")
    self._add_file("/Sounds/ambient.mp3", "ambient.mp3", 3145728, now, "mp3")
    self._add_file("/Sounds/music_theme.ogg", "music_theme.ogg", 5242880, now, "ogg")

    # Scripts directory
    self._entries["/Scripts"] = {
      "name": "Scripts",
      "type": "directory",
      "size": 0,
      "modified_time": now,
      "children": ["main.py", "player.py", "enemy_ai.py", "utils.py"]
    }
    self._add_file("/Scripts/main.py", "main.py", 4096, now, "py")
    self._add_file("/Scripts/player.py", "player.py", 8192, now, "py")
    self._add_file("/Scripts/enemy_ai.py", "enemy_ai.py", 12288, now, "py")
    self._add_file("/Scripts/utils.py", "utils.py", 2048, now, "py")

  def _add_file(self, path, name, size, mtime, ext=None):
    """Helper to add a file entry."""
    if ext is None:
      ext = name.split(".")[-1] if "." in name else ""
    self._entries[path] = {
      "name": name,
      "type": "file",
      "size": size,
      "modified_time": mtime,
      "extension": ext,
      "children": []
    }

  def _get_full_path(self, name):
    """Get full path for a child name in current directory."""
    if self._current_path == "/":
      return "/" + name
    return self._current_path + "/" + name

  # Required FilesystemModel interface methods

  def getCurrentPath(self):
    return self._current_path

  def setCurrentPath(self, path):
    # Normalize path
    if not path.startswith("/"):
      path = "/" + path
    path = path.rstrip("/") if path != "/" else "/"

    if path in self._entries and self._entries[path]["type"] == "directory":
      self._current_path = path
      self.notifyDirectoryChanged(path)
      self.notifyModelChanged()
      return True
    return False

  def getParentPath(self):
    if self._current_path == "/":
      return ""
    parts = self._current_path.rsplit("/", 1)
    return parts[0] if parts[0] else "/"

  def exists(self, path):
    return path in self._entries

  def isDirectory(self, path):
    if path in self._entries:
      return self._entries[path]["type"] == "directory"
    return False

  def getEntries(self):
    """Return list of entries in current directory as list of dicts."""
    entries = []
    if self._current_path not in self._entries:
      return entries

    current = self._entries[self._current_path]
    for child_name in current.get("children", []):
      child_path = self._get_full_path(child_name)
      if child_path in self._entries:
        child = self._entries[child_path]
        entries.append({
          "path": child_path,
          "name": child["name"],
          "type": child["type"],
          "size": child.get("size", 0),
          "modified_time": child.get("modified_time", 0),
          "extension": child.get("extension", ""),
          "mime_type": self._get_mime_type(child.get("extension", "")),
          "is_hidden": child["name"].startswith("."),
          "is_readable": True,
          "is_writable": True
        })
    return entries

  def getEntry(self, path):
    """Return entry metadata for a specific path."""
    if path not in self._entries:
      return {
        "path": path,
        "name": path.split("/")[-1],
        "type": "unknown",
        "size": 0,
        "modified_time": 0,
        "extension": "",
        "mime_type": "",
        "is_hidden": False,
        "is_readable": False,
        "is_writable": False
      }

    entry = self._entries[path]
    return {
      "path": path,
      "name": entry["name"],
      "type": entry["type"],
      "size": entry.get("size", 0),
      "modified_time": entry.get("modified_time", 0),
      "extension": entry.get("extension", ""),
      "mime_type": self._get_mime_type(entry.get("extension", "")),
      "is_hidden": entry["name"].startswith("."),
      "is_readable": True,
      "is_writable": True
    }

  def getDisplayName(self, path):
    if path in self._entries:
      return self._entries[path]["name"]
    return path.split("/")[-1]

  def isReadOnly(self):
    return False

  def createDirectory(self, name):
    """Create a new directory in current path."""
    new_path = self._get_full_path(name)
    if new_path in self._entries:
      return False

    now = int(time.time())
    self._entries[new_path] = {
      "name": name,
      "type": "directory",
      "size": 0,
      "modified_time": now,
      "children": []
    }
    self._entries[self._current_path]["children"].append(name)
    self.notifyModelChanged()
    return True

  def deleteItem(self, path):
    """Delete an item."""
    if path not in self._entries or path == "/":
      return False

    # Remove from parent's children
    parent_path = self.getParentPath() if path != "/" else None
    if parent_path and parent_path in self._entries:
      name = self._entries[path]["name"]
      if name in self._entries[parent_path]["children"]:
        self._entries[parent_path]["children"].remove(name)

    # Remove entry
    del self._entries[path]
    self.notifyModelChanged()
    return True

  def renameItem(self, path, new_name):
    """Rename an item."""
    if path not in self._entries:
      return ""

    # Calculate new path
    parent = path.rsplit("/", 1)[0] if "/" in path else "/"
    new_path = (parent + "/" + new_name) if parent != "/" else "/" + new_name

    if new_path in self._entries:
      return ""  # Name collision

    # Update entry
    entry = self._entries.pop(path)
    old_name = entry["name"]
    entry["name"] = new_name
    self._entries[new_path] = entry

    # Update extension if file
    if entry["type"] == "file" and "." in new_name:
      entry["extension"] = new_name.split(".")[-1]

    # Update parent's children list
    if parent in self._entries:
      children = self._entries[parent]["children"]
      if old_name in children:
        idx = children.index(old_name)
        children[idx] = new_name

    self.notifyModelChanged()
    return new_path

  def hasThumbnail(self, path):
    """Check if we have a thumbnail for this path."""
    if path not in self._entries:
      return False
    ext = self._entries[path].get("extension", "").lower()
    return ext in ["png", "jpg", "jpeg", "gif", "bmp", "hdr", "exr"]

  def getThumbnailProvider(self, path, size):
    """Return a thumbnail provider function.

    In a real implementation, this would return a function that
    loads/renders the actual thumbnail image. For this demo,
    we return None as we have no actual image data.
    """
    # Return None - no actual thumbnails in this virtual filesystem
    # In a real implementation:
    # return lambda: load_and_resize_image(path, size)
    return None

  def _get_mime_type(self, extension):
    """Get MIME type for extension."""
    mime_types = {
      "png": "image/png",
      "jpg": "image/jpeg",
      "jpeg": "image/jpeg",
      "gif": "image/gif",
      "hdr": "image/vnd.radiance",
      "fbx": "model/fbx",
      "gltf": "model/gltf+json",
      "obj": "model/obj",
      "wav": "audio/wav",
      "ogg": "audio/ogg",
      "mp3": "audio/mpeg",
      "py": "text/x-python",
    }
    return mime_types.get(extension.lower(), "application/octet-stream")

################################################################################

class FilesystemModelTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    icon_size = 20

    ############################################################################
    # Main VPack - toolbar + filesystem view
    ############################################################################

    main_vpack_layout = lg_group.makeChild(uiclass=lev2.ui.VerticalPack, args=["main_vpack"])
    main_vpack_layout.layout.fill(lg_group.layout)
    self.main_vpack = main_vpack_layout.widget
    self.main_vpack.margin = 4
    self.main_vpack.item_height = 32
    self.main_vpack.fill = True
    self.main_vpack.bg_color = vec4(0, 0, 0, 1)

    ############################################################################
    # Toolbar
    ############################################################################

    self.toolbar = self.main_vpack.makeChild(uiclass=lev2.ui.Toolbar, args=["toolbar"])
    self.toolbar.fixed_height = 32

    # Style toolbar
    self.toolbar.bgcolor = vec4(0.12, 0.12, 0.15, 1)
    self.toolbar.button_hover_color = vec4(0.25, 0.25, 0.3, 1)
    self.toolbar.button_pressed_color = vec4(0.2, 0.4, 0.6, 1)
    self.toolbar.button_toggled_color = vec4(0.25, 0.45, 0.65, 1)
    self.toolbar.separator_color = vec4(0.3, 0.3, 0.35, 1)
    self.toolbar.icon_size = icon_size
    self.toolbar.button_padding = 4
    self.toolbar.item_spacing = 2
    self.toolbar.edge_padding = 6

    # Navigation buttons
    btn_home = self.toolbar.addButton("home", standard_icons.get('home', icon_size, icon_size), "Home Directory")
    btn_parent = self.toolbar.addButton("parent", standard_icons.get('parent', icon_size, icon_size), "Parent Directory")

    self.toolbar.addSeparator()

    # View mode buttons
    btn_list = self.toolbar.addButton("list", standard_icons.get('file_text', icon_size, icon_size), "List View")
    btn_list.toggle_mode = True
    btn_list.toggled = True

    btn_icons = self.toolbar.addButton("icons", standard_icons.get('folder', icon_size, icon_size), "Icon View")
    btn_icons.toggle_mode = True

    self.toolbar.addSeparator()

    # Icon size +/- buttons (folder with +/- overlay)
    def make_folder_size_icon(symbol):
      return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
        <path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z" fill="#4D99CC"/>
        <text x="12" y="14" text-anchor="middle" dominant-baseline="central" font-family="sans-serif" font-size="18" font-weight="bold" fill="#FFFFFF" stroke="#000000" stroke-width="0.5">{symbol}</text>
      </svg>'''

    icon_plus = icon_library.from_svg_string(make_folder_size_icon("+"), icon_size, icon_size)
    icon_minus = icon_library.from_svg_string(make_folder_size_icon("-"), icon_size, icon_size)

    btn_size_up = self.toolbar.addButton("size_up", icon_plus, "Increase Icon Size")
    btn_size_down = self.toolbar.addButton("size_down", icon_minus, "Decrease Icon Size")

    ############################################################################
    # Filesystem view (fills remaining space)
    ############################################################################

    self.fs_view = self.main_vpack.makeChild(uiclass=lev2.ui.FilesystemView, args=["filesystem"])

    # Create custom virtual filesystem model
    self.model = VirtualFilesystemModel()
    self.model.directories_first = True
    self.model.sort_field = lev2.ui.FilesystemSortField.Name

    self.fs_view.model = self.model

    # Set view mode
    self.fs_view.view_mode = lev2.ui.FilesystemViewMode.List

    # Disable size and date columns
    self.fs_view.show_size_column = False
    self.fs_view.show_date_column = False

    # Enable multi-select
    self.fs_view.allow_multiselect = True

    # Wire up toolbar buttons
    btn_home.onPressed(lambda: self.fs_view.navigateTo("/"))
    btn_parent.onPressed(lambda: self.fs_view.navigateUp())

    def set_list_view(toggled):
      if toggled:
        self.fs_view.view_mode = lev2.ui.FilesystemViewMode.List
        btn_icons.toggled = False

    def set_icon_view(toggled):
      if toggled:
        self.fs_view.view_mode = lev2.ui.FilesystemViewMode.Icon
        btn_list.toggled = False

    btn_list.onToggled(set_list_view)
    btn_icons.onToggled(set_icon_view)

    def increase_icon_size():
      self.fs_view.icon_size = min(256, self.fs_view.icon_size + 16)
      self.fs_view.refresh()

    def decrease_icon_size():
      self.fs_view.icon_size = max(32, self.fs_view.icon_size - 16)
      self.fs_view.refresh()

    btn_size_up.onPressed(increase_icon_size)
    btn_size_down.onPressed(decrease_icon_size)

    # Set selection callback
    def on_select(path):
      print(f"Selected: {path}")
      entry = self.model.getEntry(path)
      print(f"  Type: {entry['type']}")
      if entry['type'] == 'file':
        size_kb = entry['size'] / 1024
        print(f"  Size: {size_kb:.1f} KB")
      print(f"  MIME: {entry['mime_type']}")

    self.fs_view.onSelect(on_select)

    # Set activation callback (double-click)
    def on_activate(path):
      print(f"Activated: {path}")
      if not self.model.isDirectory(path):
        print(f"  Would load asset: {path}")

    self.fs_view.onActivate(on_activate)

    # Set directory changed callback
    def on_dir_changed(path):
      print(f"Directory changed to: {path}")

    self.fs_view.onDirectoryChanged(on_dir_changed)

    # Style to match outliner
    self.fs_view.bgcolor = vec4(0.15, 0.15, 0.15, 1)
    self.fs_view.text_color = vec4(0.9, 0.9, 0.9, 1)
    self.fs_view.selected_color = vec4(0.2, 0.4, 0.6, 1)
    self.fs_view.hover_color = vec4(0.25, 0.25, 0.3, 1)
    self.fs_view.directory_color = vec4(0.7, 0.85, 1.0, 1)
    self.fs_view.header_bgcolor = vec4(0.12, 0.12, 0.15, 1)
    self.fs_view.item_height = 24

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def onGpuInit(self, ctx):
    # Set up theme engine for SDF rendering
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

    # Create folder icon for icon view (pass image, C++ converts to texture lazily)
    folder_svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
      <path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z" fill="#4D99CC"/>
    </svg>'''
    self.fs_view.folder_icon = icon_library.from_svg_string(folder_svg, 64, 64)

    # Create file icon (simple page)
    file_svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
      <path d="M14 2H6c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6z" fill="#808080"/>
      <path d="M14 2v6h6" fill="#606060"/>
    </svg>'''
    self.fs_view.file_icon = icon_library.from_svg_string(file_svg, 64, 64)

  def onUpdate(self, updinfo):
    pass

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

FilesystemModelTest().ezapp.mainThreadLoop()
