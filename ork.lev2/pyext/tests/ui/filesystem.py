#!/usr/bin/env ork.python

################################################################################
# Filesystem Browser Widget Test
# Demonstrates the reusable FilesystemBrowser composite widget
################################################################################

import signal
from orkengine.core import vec3, vec4
from orkengine import lev2
from ork import path as ork_path
from ork.ui.filesystem_browser import FilesystemBrowser

################################################################################

class FilesystemBrowserTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=False, name="FilesystemBrowserTest")
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    ############################################################################
    # Create FilesystemBrowser using uifactory
    ############################################################################

    shader_path = ork_path.data / "platform_lev2" / "shaders" / "fxv2"

    browser_item = lg_group.makeChild(
      uiclass=FilesystemBrowser,
      args=["browser", str(shader_path), "*.fxv2"]
    )
    browser_item.layout.fill(lg_group.layout)

    # Get the browser instance from uservars
    self.browser = browser_item.widget.uservars.filesystem_browser

    # Set up callbacks
    self.browser.onSelect = self._on_select
    self.browser.onActivate = self._on_activate
    self.browser.onDirectoryChanged = self._on_directory_changed

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def _on_select(self, path):
    """Called when a file/directory is selected."""
    print(f"Selected: {path}")

  def _on_activate(self, path):
    """Called when a file is activated (double-clicked)."""
    print(f"Activated: {path}")
    if not self.browser.model.isDirectory(path):
      print(f"  Would open file: {path}")

  def _on_directory_changed(self, path):
    """Called when the current directory changes."""
    print(f"Directory changed to: {path}")

  def onGpuInit(self, ctx):
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

FilesystemBrowserTest().ezapp.mainThreadLoop()
