#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
TestRunnerApp - Visual test runner with FilesystemView UI

Library module providing a reusable test runner UI.
Define a nested dict where dicts are groups and lists are test commands.
Arbitrary nesting depth is supported.

Usage:
    from ork.app.testrunner import TestRunnerApp

    tests = {
        "Unit Tests": {
            "Math": ["ork.test.python.unittests.all.py"],
        },
        "Multi Process": {
            "Client+Server": [
                ["server.py", "--port", "8080"],
                ["client.py", "--port", "8080"],
            ],
        },
        "Shadows": {
            "_commands": ["ork.test.shadows.py"],
            "_description": "Shadow mapping test",
            "_options": {
                "Fullscreen": ["-f"],
                "Mode": {"A": ["--mode", "a"], "B": ["--mode", "b"]},
            },
        },
    }
    TestRunnerApp(tests, title="My Tests").run()
"""

import shlex
import signal
import subprocess
import threading
import time

from obt import command
from obt import tmux
from orkengine.core import vec4
from orkengine import lev2
from ork.ui import standard_icons, icon_library

################################################################################
# Per-test state
################################################################################

class TestInfo:
  def __init__(self, name, commands, is_tmux=False, description="", options_spec=None):
    self.name = name
    self.commands = commands    # list of strings (single) or list of lists (tmux)
    self.is_tmux = is_tmux
    self.description = description
    self.options_spec = options_spec or {}  # raw _options dict
    self.options_state = {}    # runtime state: {"BoolOpt": True, "EnumOpt": "A"}
    self.status = "pending"    # pending | running | passed | failed
    self.exit_code = -1
    self.duration = 0.0

################################################################################
# Status display
################################################################################

_STATUS_ICONS = {
  "pending": "\u25CB",   # ○
  "running": "\u25C9",   # ◉
  "passed":  "\u25CF",   # ●
  "failed":  "\u2717",   # ✗
}

def _svg_wrap(content, viewbox="0 0 24 24"):
  return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="%s">%s</svg>' % (viewbox, content)

_STATUS_SVGS = {
  "pending": _svg_wrap('<circle cx="12" cy="12" r="8" fill="#808080"/>'),
  "running": _svg_wrap('<circle cx="12" cy="12" r="8" fill="#CCAA00"/>'),
  "passed":  _svg_wrap('<circle cx="12" cy="12" r="8" fill="#44AA44"/>'),
  "failed":  _svg_wrap('<path d="M6 6L18 18M18 6L6 18" stroke="#CC4444" stroke-width="3" stroke-linecap="round"/>'),
}

# Pre-rotated yin-yang SVG frames for "running" animation
_RUNNING_NUM_FRAMES = 24
_RUNNING_FRAMES_SVG = []
for _i in range(_RUNNING_NUM_FRAMES):
  _angle = _i * (360.0 / _RUNNING_NUM_FRAMES)
  _RUNNING_FRAMES_SVG.append(_svg_wrap(
    '<g transform="rotate(%.1f, 12, 12)">'
    '<circle cx="12" cy="12" r="8" fill="#CCAA00"/>'
    '<path d="M12 4 A8 8 0 0 1 12 20 A4 4 0 0 1 12 12 A4 4 0 0 0 12 4" fill="#1A1A1A"/>'
    '<circle cx="12" cy="8" r="1.5" fill="#CCAA00"/>'
    '<circle cx="12" cy="16" r="1.5" fill="#1A1A1A"/>'
    '</g>' % _angle
  ))

################################################################################
# Filesystem model: tests as virtual files, groups as directories
################################################################################

class TestRunnerFilesystemModel(lev2.ui.FilesystemModel):

  def __init__(self):
    super().__init__()
    self._lock = threading.Lock()
    self._current_path = "/"
    self._entries = {}       # path -> entry dict {name, type, children, extension, description}
    self._tests = {}         # path -> TestInfo (leaves only)
    self._display_names = {} # path -> short name
    self._custom_icons = {}  # path -> SVG string (custom per-entry icon)
    self._on_status_changed = None
    self._option_defaults = {}  # option_name -> latest value (bool or str)

    # Root entry
    self._entries["/"] = {
      "name": "/",
      "type": "directory",
      "children": []
    }

  def populate(self, data, prefix=""):
    """Recursively populate from nested dicts.
    Dict with _commands key = test-with-options.
    Dict without _commands key = group.
    List of strings = single command test.
    List of lists = multi-command tmux test."""
    parent_path = prefix if prefix else "/"
    for name, value in data.items():
      path = prefix + "/" + name if prefix else "/" + name
      self._display_names[path] = name

      if isinstance(value, dict) and "_commands" in value:
        # Test with options (stored as a file, options shown via option columns)
        commands = value["_commands"]
        description = value.get("_description", "")
        options_spec = value.get("_options", {})
        is_tmux = isinstance(commands, list) and len(commands) > 0 and isinstance(commands[0], list)
        info = TestInfo(name, commands, is_tmux=is_tmux, description=description, options_spec=options_spec)
        # Initialize options_state with defaults
        for opt_name, opt_value in options_spec.items():
          if isinstance(opt_value, list):
            info.options_state[opt_name] = False
          elif isinstance(opt_value, dict):
            info.options_state[opt_name] = next(iter(opt_value))
        self._tests[path] = info
        self._entries[path] = {
          "name": name,
          "type": "file",
          "extension": "test",
          "description": description,
        }
        self._entries[parent_path]["children"].append(name)
        # Custom icon (SVG string)
        if "_icon" in value:
          self._custom_icons[path] = value["_icon"]

      elif isinstance(value, dict):
        # group node -> directory
        self._entries[path] = {
          "name": name,
          "type": "directory",
          "children": []
        }
        self._entries[parent_path]["children"].append(name)
        self.populate(value, path)
      elif isinstance(value, list) and len(value) > 0 and isinstance(value[0], list):
        # tmux test (list of lists)
        self._tests[path] = TestInfo(name, value, is_tmux=True)
        self._entries[path] = {
          "name": name,
          "type": "file",
          "extension": "test"
        }
        self._entries[parent_path]["children"].append(name)
      else:
        # single command test (list of strings)
        self._tests[path] = TestInfo(name, value)
        self._entries[path] = {
          "name": name,
          "type": "file",
          "extension": "test"
        }
        self._entries[parent_path]["children"].append(name)

  # -- FilesystemModel PURE_OVERRIDE interface --

  def getCurrentPath(self):
    return self._current_path

  def setCurrentPath(self, path):
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
    """Return list of entry dicts for current directory."""
    entries = []
    with self._lock:
      if self._current_path not in self._entries:
        return entries
      current = self._entries[self._current_path]
      for child_name in current.get("children", []):
        child_path = self._get_full_path(child_name)
        if child_path in self._entries:
          child = self._entries[child_path]
          # Build description text
          description = child.get("description", "")
          if child_path in self._tests:
            info = self._tests[child_path]
            if not description:
              description = info.description
          entries.append({
            "path": child_path,
            "name": child["name"],
            "type": child["type"],
            "size": 0,
            "modified_time": 0,
            "extension": child.get("extension", ""),
            "mime_type": "",
            "is_hidden": False,
            "is_readable": True,
            "is_writable": False,
            "description": description,
          })
    return entries

  def getEntry(self, path):
    """Return entry metadata for a specific path."""
    with self._lock:
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
          "is_writable": False,
          "description": "",
        }
      entry = self._entries[path]
      description = entry.get("description", "")
      if path in self._tests:
        info = self._tests[path]
        if not description:
          description = info.description
      return {
        "path": path,
        "name": entry["name"],
        "type": entry["type"],
        "size": 0,
        "modified_time": 0,
        "extension": entry.get("extension", ""),
        "mime_type": "",
        "is_hidden": False,
        "is_readable": True,
        "is_writable": False,
        "description": description,
      }

  def getDisplayName(self, path):
    with self._lock:
      name = self._display_names.get(path, path.split("/")[-1])
      # test leaf
      if path in self._tests:
        info = self._tests[path]
        icon = _STATUS_ICONS.get(info.status, "?")
        if info.status in ("passed", "failed"):
          return "%s %s (%.1fs)" % (icon, info.name, info.duration)
        return "%s %s" % (icon, info.name)
      # group node — aggregate from all descendant tests
      if path in self._entries and self._entries[path]["type"] == "directory" and path != "/":
        descendants = self._descendantTests(path)
        total = len(descendants)
        if total == 0:
          return name
        passed = sum(1 for k in descendants if self._tests[k].status == "passed")
        failed = sum(1 for k in descendants if self._tests[k].status == "failed")
        running = any(self._tests[k].status == "running" for k in descendants)
        if running:
          return "%s (running...)" % name
        if passed + failed == total:
          if failed == 0:
            return "%s (%d/%d passed)" % (name, passed, total)
          else:
            return "%s (%d/%d passed, %d failed)" % (name, passed, total, failed)
        return name
      return name

  def modelIdentifier(self):
    return "testrunner"

  # -- OVERRIDE methods --

  def isReadOnly(self):
    return True

  def getIcon(self, path, size=64):
    """Return status icon for tests, tinted folder for groups."""
    svg_string = None
    with self._lock:
      # Custom per-entry icon (shown except when running — animation takes over)
      if path in self._custom_icons:
        if path not in self._tests or self._tests[path].status != "running":
          svg_string = self._custom_icons[path]
      elif path in self._tests:
        status = self._tests[path].status
        if status != "running":
          svg_string = _STATUS_SVGS.get(status)
      elif path in self._entries and self._entries[path]["type"] == "directory" and path != "/":
        descendants = self._descendantTests(path)
        if descendants:
          any_running = any(self._tests[k].status == "running" for k in descendants)
          any_failed = any(self._tests[k].status == "failed" for k in descendants)
          all_passed = all(self._tests[k].status == "passed" for k in descendants)
          if any_running:
            pass  # handled by getIconSequence
          elif any_failed:
            color = "#CC4444"
            svg_string = _svg_wrap(
              '<path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z" fill="%s"/>' % color
            )
          elif all_passed:
            color = "#44AA44"
            svg_string = _svg_wrap(
              '<path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z" fill="%s"/>' % color
            )
          else:
            color = "#4D99CC"
            svg_string = _svg_wrap(
              '<path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z" fill="%s"/>' % color
            )
    if svg_string:
      return icon_library.from_svg_string(svg_string, size, size)
    return None

  def getIconSequence(self, path, size=64):
    """Return animated yin-yang frames for running tests."""
    is_running = False
    with self._lock:
      if path in self._tests:
        is_running = (self._tests[path].status == "running")
      elif path in self._entries and self._entries[path]["type"] == "directory" and path != "/":
        descendants = self._descendantTests(path)
        if descendants:
          is_running = any(self._tests[k].status == "running" for k in descendants)
    if is_running:
      return [icon_library.from_svg_string(svg, size, size) for svg in _RUNNING_FRAMES_SVG]
    return []

  # -- internal helpers --

  def _get_full_path(self, name):
    """Get full path for a child name in current directory."""
    if self._current_path == "/":
      return "/" + name
    return self._current_path + "/" + name

  def _descendantTests(self, path):
    """Return all leaf test paths under a directory. Must hold _lock."""
    result = []
    entry = self._entries.get(path)
    if entry is None or entry["type"] != "directory":
      return result
    for child_name in entry.get("children", []):
      child_path = ("/" + child_name) if path == "/" else (path + "/" + child_name)
      if child_path in self._tests:
        result.append(child_path)
      elif child_path in self._entries and self._entries[child_path]["type"] == "directory":
        result.extend(self._descendantTests(child_path))
    return result

  def _ancestorPaths(self, path):
    """Return all ancestor directory paths for a given path."""
    if path == "/":
      return []
    parts = path.split("/")
    ancestors = ["/"]
    for i in range(2, len(parts)):
      ancestors.append("/".join(parts[:i]))
    return ancestors

  def _buildEffectiveArgs(self, test_path):
    """Build list of extra args from options_state for a test path."""
    info = self._tests.get(test_path)
    if info is None or not info.options_state:
      return []
    args = []
    for opt_name, opt_value in info.options_state.items():
      spec = info.options_spec.get(opt_name)
      if isinstance(spec, list):
        if opt_value:
          args.extend(spec)
      elif isinstance(spec, dict):
        choice_args = spec.get(opt_value, [])
        args.extend(choice_args)
    return args

  # -- Per-item options (model-driven micro-renderers) --

  def getItemOptions(self, path):
    """Return list of option defs for a specific item."""
    info = self._tests.get(path)
    if info is None or not info.options_spec:
      return []
    options = []
    for opt_name, opt_spec in info.options_spec.items():
      state = info.options_state.get(opt_name)
      if isinstance(opt_spec, list):
        # Boolean option
        options.append({
          "name": opt_name,
          "type": "checkbox",
          "bool_val": bool(state),
          "string_val": "",
          "choices": [],
        })
      elif isinstance(opt_spec, dict):
        # Enum/dropdown option
        options.append({
          "name": opt_name,
          "type": "dropdown",
          "bool_val": False,
          "string_val": str(state),
          "choices": list(opt_spec.keys()),
        })
    return options

  def setItemOption(self, path, option_name, value):
    """Set an option value by name. Returns True if changed.
    Caches the value so new items with the same option key inherit it."""
    info = self._tests.get(path)
    if info is None or option_name not in info.options_spec:
      return False
    spec = info.options_spec[option_name]
    if isinstance(spec, list):
      v = value.get("bool_val", False) if isinstance(value, dict) else value["bool_val"]
      info.options_state[option_name] = v
      self._option_defaults[option_name] = v
    elif isinstance(spec, dict):
      v = value.get("string_val", "") if isinstance(value, dict) else value["string_val"]
      info.options_state[option_name] = v
      self._option_defaults[option_name] = v
    # Apply cached value to all other tests with the same option
    for other_path, other_info in self._tests.items():
      if other_path != path and option_name in other_info.options_spec:
        other_info.options_state[option_name] = info.options_state[option_name]
    self.notifyModelChanged()
    return True

  # -- state updates (called from background threads) --

  def setTestStatus(self, path, status, exit_code=-1, duration=0.0):
    with self._lock:
      info = self._tests.get(path)
      if info is None:
        return
      info.status = status
      info.exit_code = exit_code
      info.duration = duration
    self.notifyModelChanged()
    if self._on_status_changed:
      self._on_status_changed()

  def getTestInfo(self, path):
    with self._lock:
      return self._tests.get(path)

  def allTestKeys(self):
    with self._lock:
      return list(self._tests.keys())

  def descendantTestKeys(self, group_path):
    with self._lock:
      return self._descendantTests(group_path)

################################################################################
# App
################################################################################

class TestRunnerApp:

  def __init__(self, tests, title="Test Runner", width=640, height=720, auto_run=False, default_view="list"):
    self._auto_run = auto_run
    self._default_view = default_view

    # -- EzApp boilerplate --
    self.ezapp = lev2.OrkEzApp.create(self, name=title, width=width, height=height)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    icon_size = 20

    # -- Main VPack: toolbar + filesystem view --
    main_vpack_layout = lg_group.makeChild(uiclass=lev2.ui.VerticalPack, args=["main_vpack"])
    main_vpack_layout.layout.fill(lg_group.layout)
    self.main_vpack = main_vpack_layout.widget
    self.main_vpack.margin = 4
    self.main_vpack.item_height = 32
    self.main_vpack.fill = True
    self.main_vpack.bg_color = vec4(0, 0, 0, 1)

    # -- Toolbar --
    self.toolbar = self.main_vpack.makeChild(uiclass=lev2.ui.Toolbar, args=["toolbar"])
    self.toolbar.fixed_height = 32
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
    btn_list.toggled = (self._default_view == "list")

    btn_icons = self.toolbar.addButton("icons", standard_icons.get('folder', icon_size, icon_size), "Icon View")
    btn_icons.toggle_mode = True
    btn_icons.toggled = (self._default_view == "icon")

    self.toolbar.addSeparator()

    # Icon size buttons
    btn_icon_minus = self.toolbar.addButton("icon_minus", standard_icons.get('minus', icon_size, icon_size), "Smaller Icons")
    btn_icon_plus = self.toolbar.addButton("icon_plus", standard_icons.get('plus', icon_size, icon_size), "Larger Icons")

    # -- FilesystemView --
    self.fs_view = self.main_vpack.makeChild(uiclass=lev2.ui.FilesystemView, args=["filesystem"])

    # -- Model --
    self._model = TestRunnerFilesystemModel()
    self._model.populate(tests)
    self._model.directories_first = True
    self._model.sort_field = lev2.ui.FilesystemSortField.Name
    self._model._on_status_changed = lambda: self.fs_view.clearIconCache()

    self.fs_view.model = self._model
    self.fs_view.view_mode = lev2.ui.FilesystemViewMode.Icon if self._default_view == "icon" else lev2.ui.FilesystemViewMode.List
    self.fs_view.show_size_column = False
    self.fs_view.show_type_column = False
    self.fs_view.show_date_column = False
    self.fs_view.draw_options_bar = True

    # Enable description column if any tests have descriptions
    if self._checkHasDescriptions(tests):
      self.fs_view.show_description_column = True
    # Option columns are driven by model.getOptionColumns() (no text column needed)
    self.fs_view.show_options_column = False

    # -- Wire toolbar buttons --
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

    def icon_size_smaller():
      cur = self.fs_view.icon_size
      self.fs_view.icon_size = max(32, cur - 16)
      self.fs_view.clearIconCache()
      self.fs_view.refresh()

    def icon_size_larger():
      cur = self.fs_view.icon_size
      self.fs_view.icon_size = min(256, cur + 16)
      self.fs_view.clearIconCache()
      self.fs_view.refresh()

    btn_icon_minus.onPressed(icon_size_smaller)
    btn_icon_plus.onPressed(icon_size_larger)

    # -- Callbacks --
    def on_activate(path):
      """Double-click: run test or navigate into directory."""
      info = self._model.getTestInfo(path)
      if info is not None and info.status in ("pending", "passed", "failed"):
        t = threading.Thread(target=self._runTest, args=(path,), daemon=True)
        t.start()

    def on_select(path):
      """Single click: print test info."""
      info = self._model.getTestInfo(path)
      if info is not None:
        print("Test: %s [%s]" % (info.name, info.status))

    self.fs_view.onActivate(on_activate)
    self.fs_view.onSelect(on_select)

    # -- Style --
    self.fs_view.bgcolor = vec4(0.15, 0.15, 0.15, 1)
    self.fs_view.text_color = vec4(0.9, 0.9, 0.9, 1)
    self.fs_view.selected_color = vec4(0.2, 0.4, 0.6, 1)
    self.fs_view.hover_color = vec4(0.25, 0.25, 0.3, 1)
    self.fs_view.directory_color = vec4(0.7, 0.85, 1.0, 1)
    self.fs_view.header_bgcolor = vec4(0.12, 0.12, 0.15, 1)
    self.fs_view.item_height = 24

    # -- Ctrl+C --
    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()
    signal.signal(signal.SIGINT, onCtrlC)

  # -- helpers for checking if test data uses descriptions/options --

  @staticmethod
  def _checkHasDescriptions(data):
    for name, value in data.items():
      if isinstance(value, dict):
        if "_commands" in value and "_description" in value:
          return True
        if "_commands" not in value:
          if TestRunnerApp._checkHasDescriptions(value):
            return True
    return False


  # -- GPU init: theme setup --

  def onGpuInit(self, ctx):
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

    # Default icons for icon view
    folder_svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">' \
      '<path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z" fill="#4D99CC"/>' \
      '</svg>'
    file_svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">' \
      '<path d="M14 2H6c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6z" fill="#808080"/>' \
      '<path d="M14 2v6h6" fill="#606060"/>' \
      '</svg>'
    self.fs_view.folder_icon = icon_library.from_svg_string(folder_svg, 64, 64)
    self.fs_view.file_icon = icon_library.from_svg_string(file_svg, 64, 64)
    self.fs_view.icon_size = 128

    if self._auto_run:
      self.runAll()

  def onUpdate(self, updinfo):
    pass

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

  # -- Test execution --

  def runAll(self):
    for key in self._model.allTestKeys():
      t = threading.Thread(target=self._runTest, args=(key,), daemon=True)
      t.start()

  def _runFailed(self):
    for key in self._model.allTestKeys():
      info = self._model.getTestInfo(key)
      if info and info.status == "failed":
        t = threading.Thread(target=self._runTest, args=(key,), daemon=True)
        t.start()

  def _runGroup(self, group_key):
    for key in self._model.descendantTestKeys(group_key):
      info = self._model.getTestInfo(key)
      if info and info.status in ("pending", "passed", "failed"):
        t = threading.Thread(target=self._runTest, args=(key,), daemon=True)
        t.start()

  def _runTest(self, key):
    info = self._model.getTestInfo(key)
    if info is None:
      return
    if info.is_tmux:
      self._runTmuxTest(key, info)
    else:
      self._runSingleTest(key, info)

  def _runSingleTest(self, key, info):
    self._model.setTestStatus(key, "running")
    t0 = time.time()
    try:
      # Build effective command with option args appended
      extra_args = self._model._buildEffectiveArgs(key)
      commands = info.commands + extra_args
      exit_code = command.run(commands, do_log=True)
      elapsed = time.time() - t0
      status = "passed" if exit_code == 0 else "failed"
      self._model.setTestStatus(key, status, exit_code, duration=elapsed)
    except Exception as e:
      elapsed = time.time() - t0
      self._model.setTestStatus(key, "failed", -1, duration=elapsed)

  def _runTmuxTest(self, key, info):
    session_name = "test_" + key.replace("/", "_").replace(" ", "_")
    self._model.setTestStatus(key, "running")
    try:
      orientation = "vertical" if len(info.commands) <= 3 else "horizontal"
      session = tmux.Session(session_name, orientation=orientation, kill_first=True)
      for cmd_list in info.commands:
        session.command([shlex.join(cmd_list)])
      # execute detached (bypass session.execute() to avoid attach)
      session.kill()
      session.bind_zoom_panes()
      for item in session.post_chain:
        session.cmd_chain.add(item)
      session.select_layout()
      if len(info.commands) > 3:
        session.cmd_chain.add(["tmux", "select-layout", "-t", session_name, "tiled"])
      session.cmd_chain.execute()
      # open a terminal attached to the session
      subprocess.Popen([
        "osascript", "-e",
        'tell application "Terminal" to do script "tmux attach-session -t %s"' % session_name
      ])
    except Exception as e:
      print("tmux launch failed: %s" % e)
      self._model.setTestStatus(key, "failed", -1)

  # -- Main loop --

  def run(self):
    self.ezapp.mainThreadLoop()
