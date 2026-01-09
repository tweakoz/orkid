#!/usr/bin/env ork.python

################################################################################
# Filesystem Widget Test
# Demonstrates filesystem browser with list and icon view modes
################################################################################

import signal
import os
from orkengine.core import vec2, vec3, vec4
from orkengine import lev2

################################################################################

class FilesystemTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    # Create filesystem view widget
    fs_layout = lg_group.makeChild(uiclass=lev2.ui.FilesystemView, args=["filesystem"])
    self.fs_view = fs_layout.widget

    # Set up layout - anchor to parent
    root_layout = lg_group.layout
    fs_layout.layout.top.anchorTo(root_layout.top)
    fs_layout.layout.left.anchorTo(root_layout.left)
    fs_layout.layout.bottom.anchorTo(root_layout.bottom)
    fs_layout.layout.right.anchorTo(root_layout.right)

    # Create a local filesystem model starting at home directory
    home_dir = os.path.expanduser("~")
    self.model = lev2.ui.LocalFilesystemModel(home_dir)
    self.model.show_hidden = False
    self.model.directories_first = True
    self.model.sort_field = lev2.ui.FilesystemSortField.Name
    self.model.sort_order = lev2.ui.FilesystemSortOrder.Ascending

    self.fs_view.model = self.model

    # Set view mode (List or Icon)
    self.fs_view.view_mode = lev2.ui.FilesystemViewMode.List

    # Enable multi-select
    self.fs_view.allow_multiselect = True

    # Set selection callback
    def on_select(path):
      print(f"Selected: {path}")
      entry = self.model.getEntry(path)
      print(f"  Type: {entry['type']}")
      if entry['type'] == 'file':
        print(f"  Size: {entry['size']}")
      print(f"  Extension: {entry['extension']}")

    self.fs_view.onSelect(on_select)

    # Set activation callback (double-click)
    def on_activate(path):
      print(f"Activated: {path}")
      # Directory navigation is handled automatically
      # For files, you could open them here
      if not self.model.isDirectory(path):
        print(f"  Would open file: {path}")

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

  def onUpdate(self, updinfo):
    pass

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

FilesystemTest().ezapp.mainThreadLoop()
