#!/usr/bin/env ork.python
################################################################################
# Multi-window test: Main window + secondary window with EvTestBox on each
# Tests graphics and UI events on both windows
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import signal
import sys
from ork import path as ork_path
from orkengine.core import vec4
from orkengine import lev2
from ork.ui.filesystem_browser import FilesystemBrowser
from ork.ui.test import outliner_data

################################################################################

# Auto-close after 5 seconds for automated testing
AUTO_CLOSE = "--auto-close" in sys.argv
shader_path = ork_path.data / "platform_lev2" / "shaders" / "fxv2"

################################################################################

class MultiWindowTest:

  def __init__(self):
    super().__init__()

    # Window sizes: 640x720 each, side by side
    self.win_width = 640
    self.win_height = 720

    self.ezapp = lev2.OrkEzApp.create(self,
                                      name = "MultiWindowTest::Primary", 
                                      width=self.win_width, 
                                      height=self.win_height, 
                                      left=100, 
                                      top=100)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()


    # Main window layout with EvTestBox (1x1 grid)
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    self.lg_outliner = lg.makeChild(
      fill=True,
      uiclass=lev2.ui.Outliner,
      args=["outliner"]
    )

    self.secondary_win = None
    self.frame_count = 0
    self.outliner = self.lg_outliner.widget

    # Use VarMap data (simple approach)
    self.outliner.data = outliner_data.test_data()
    self.outliner.model.allow_rename = True  # Enable rename support
    self.outliner.model.allow_delete = True  # Enable delete support
    self.outliner.model.allow_add = True     # Enable add support
    self.outliner.model.allow_multiselect = True     # Enable add support
    self.outliner.expandAll()



    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self, ctx):
    print("Main window GPU init")

    # Create secondary window - side by side with main
    sec_x = 100 + self.win_width + 20  # 20px gap between windows
    self.secondary_win = self.ezapp.createSecondaryWindow(
      width=self.win_width,
      height=self.win_height,
      x=sec_x,
      y=100,
      title="MultiWindowTest::Secondary",
      decorated=True,
      resizable=True
    )

    # Set up EvTestBox on secondary window
    uic = self.secondary_win.ui_context

    # Create a LayoutGroup as the top widget
    # Use logical window size (same as config dimensions)
    win_w = self.secondary_win.width
    win_h = self.secondary_win.height
    print(f"[PY] Secondary window setup: win_w={win_w} win_h={win_h}", flush=True)
    root = lev2.ui.LayoutGroup.create("sec_lg")
    root.setRect(0, 0, win_w, win_h)
    uic.top = root
    root.margin = 4
    print(f"[PY] Root widget rect set to: 0,0,{win_w},{win_h}", flush=True)

    # 1x1 grid of EvTestBox for secondary window
    self.lg_secondary = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["browser", str(shader_path), "*.fxv2"],
      fill=True,
    )
    self.sec_evtestbox = self.lg_secondary.widget
    print(f"[PY] EvTestBox: x={self.sec_evtestbox.x} y={self.sec_evtestbox.y} w={self.sec_evtestbox.width} h={self.sec_evtestbox.height}", flush=True)
    print("Secondary window created")

  ##############################################

  def onUpdate(self, updinfo):
    self.frame_count += 1

    # Auto-close after 5 seconds (300 frames at 60fps) if --auto-close flag
    if AUTO_CLOSE and self.frame_count > 300:
      print("Test complete - closing")
      self.ezapp.signalExit()

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

MultiWindowTest().ezapp.mainThreadLoop()
print("Multi-window test passed!")
