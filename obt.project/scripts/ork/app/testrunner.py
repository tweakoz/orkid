#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
TestRunnerApp - Visual test runner with Outliner UI

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

################################################################################
# Per-test state
################################################################################

class TestInfo:
  def __init__(self, name, commands, is_tmux=False):
    self.name = name
    self.commands = commands    # list of strings (single) or list of lists (tmux)
    self.is_tmux = is_tmux
    self.status = "pending"    # pending | running | passed | failed
    self.exit_code = -1
    self.duration = 0.0

################################################################################
# Outliner model: arbitrary depth hierarchy (dicts = groups, lists = tests)
################################################################################

_STATUS_ICONS = {
  "pending": "\u25CB",   # ○
  "running": "\u25C9",   # ◉
  "passed":  "\u25CF",   # ●
  "failed":  "\u2717",   # ✗
}

class TestRunnerModel(lev2.ui.OutlinerModel):

  def __init__(self):
    super().__init__()
    self._lock = threading.Lock()
    self._children = {}    # key -> ordered list of child keys
    self._root_children = []
    self._tests = {}       # key -> TestInfo (leaves only)
    self._display_names = {}  # key -> short name (last segment)
    self.allow_rename = False
    self.allow_delete = False
    self.allow_add = False

  def populate(self, data, prefix=""):
    """Recursively populate from nested dicts.
    List of strings = single command test.
    List of lists = multi-command tmux test."""
    for name, value in data.items():
      key = prefix + "/" + name if prefix else name
      self._display_names[key] = name
      if isinstance(value, dict):
        # group node
        self._children[key] = []
        if prefix:
          self._children[prefix].append(key)
        else:
          self._root_children.append(key)
        self.populate(value, key)
      elif isinstance(value, list) and len(value) > 0 and isinstance(value[0], list):
        # tmux test (list of lists)
        self._tests[key] = TestInfo(name, value, is_tmux=True)
        if prefix:
          self._children[prefix].append(key)
        else:
          self._root_children.append(key)
      else:
        # single command test (list of strings)
        self._tests[key] = TestInfo(name, value)
        if prefix:
          self._children[prefix].append(key)
        else:
          self._root_children.append(key)

  # -- OutlinerModel interface --

  def getChildren(self, parent_key):
    with self._lock:
      if parent_key == "":
        return list(self._root_children)
      return list(self._children.get(parent_key, []))

  def getDisplayName(self, key):
    with self._lock:
      name = self._display_names.get(key, key)
      # test leaf
      if key in self._tests:
        info = self._tests[key]
        icon = _STATUS_ICONS.get(info.status, "?")
        if info.status in ("passed", "failed"):
          return "%s %s (%.1fs)" % (icon, info.name, info.duration)
        return "%s %s" % (icon, info.name)
      # group node — aggregate from all descendant tests
      if key in self._children:
        descendants = self._descendantTests(key)
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

  def hasChildren(self, key):
    with self._lock:
      return key in self._children

  def getValue(self, key):
    return None

  # -- internal helpers --

  def _descendantTests(self, key):
    """Return all leaf test keys under a group (recursive). Must hold _lock."""
    result = []
    for child in self._children.get(key, []):
      if child in self._tests:
        result.append(child)
      elif child in self._children:
        result.extend(self._descendantTests(child))
    return result

  def _ancestorKeys(self, key):
    """Return all ancestor group keys for a given key."""
    parts = key.split("/")
    ancestors = []
    for i in range(1, len(parts)):
      ancestors.append("/".join(parts[:i]))
    return ancestors

  # -- state updates (called from background threads) --

  def setTestStatus(self, key, status, exit_code=-1, duration=0.0):
    with self._lock:
      info = self._tests.get(key)
      if info is None:
        return
      info.status = status
      info.exit_code = exit_code
      info.duration = duration
      ancestors = self._ancestorKeys(key)
    self.notifyItemChanged(key)
    for ancestor in ancestors:
      self.notifyItemChanged(ancestor)

  def getTestInfo(self, key):
    with self._lock:
      return self._tests.get(key)

  def allTestKeys(self):
    with self._lock:
      return list(self._tests.keys())

  def descendantTestKeys(self, group_key):
    with self._lock:
      return self._descendantTests(group_key)

################################################################################
# App
################################################################################

class TestRunnerApp:

  def __init__(self, tests, title="Test Runner", width=640, height=720, auto_run=False):
    self._auto_run = auto_run

    # -- EzApp boilerplate --
    self.ezapp = lev2.OrkEzApp.create(self, name=title, width=width, height=height)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    # -- Outliner --
    outliner_layout = lg_group.makeChild(uiclass=lev2.ui.Outliner, args=["outliner"])
    self.outliner = outliner_layout.widget

    root_layout = lg_group.layout
    outliner_layout.layout.top.anchorTo(root_layout.top)
    outliner_layout.layout.left.anchorTo(root_layout.left)
    outliner_layout.layout.bottom.anchorTo(root_layout.bottom)
    outliner_layout.layout.right.anchorTo(root_layout.right)

    # -- Model --
    self._model = TestRunnerModel()
    self._model.populate(tests)

    self.outliner.model = self._model
    self.outliner.expandAll()

    # -- Selection callback: click to run --
    def on_select(key):
      info = self._model.getTestInfo(key)
      if info is None:
        # group node — run all children
        self._runGroup(key)
        return

      if info.status in ("pending", "passed", "failed"):
        t = threading.Thread(target=self._runTest, args=(key,), daemon=True)
        t.start()

    self.outliner.onSelect(on_select)

    # -- Style --
    self.outliner.bgcolor = vec4(0.15, 0.15, 0.15, 1)
    self.outliner.text_color = vec4(0.9, 0.9, 0.9, 1)
    self.outliner.selected_color = vec4(0.2, 0.4, 0.6, 1)
    self.outliner.hover_color = vec4(0.25, 0.25, 0.3, 1)
    self.outliner.item_height = 24

    # -- Ctrl+C --
    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()
    signal.signal(signal.SIGINT, onCtrlC)

  # -- GPU init: theme setup --

  def onGpuInit(self, ctx):
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme
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
      exit_code = command.run(info.commands, do_log=True)
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
