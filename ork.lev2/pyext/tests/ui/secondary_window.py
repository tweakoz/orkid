#!/usr/bin/env ork.python
################################################################################
# Multi-window test: Main window + secondary window with EvTestBox on each
# Tests graphics and UI events on both windows
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import signal
import sys
import argparse
from ork import path as ork_path
from orkengine.core import vec4
from orkengine import lev2
from ork.ui.filesystem_browser import FilesystemBrowser
from ork.ui.test import outliner_data

################################################################################

parser = argparse.ArgumentParser(description="Multi-window test")
parser.add_argument("--auto-close", action="store_true", help="Auto-close after 5 seconds")
parser.add_argument("--ffm", action="store_true", help="Enable focus-follows-mouse on secondary window")
parser.add_argument("--f2f", action="store_true", help="Enable focus-to-front on secondary window")
parser.add_argument("--fos", action="store_true", help="Enable focus-on-show on secondary window")
parser.add_argument("--aot", action="store_true", help="Secondary window is always on top")
parser.add_argument("-f", "--fullscreen", action="store_true", help="Primary window fullscreen")
parser.add_argument("--fsmon", type=str, default="", help="Secondary window fullscreen on named monitor")
args = parser.parse_args()

AUTO_CLOSE = args.auto_close
FOCUS_FOLLOWS_MOUSE = args.ffm
FOCUS_TO_FRONT = args.f2f
FOCUS_ON_SHOW = args.fos
FLOATING = args.aot
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
                                      top=100,
                                      fullscreen=args.fullscreen)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()


    # Main window layout with EvTestBox (1x1 grid)
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    self.lg_primary = lg.makeChild(
      fill=True,
      uiclass=lev2.ui.Outliner,
      args=["outliner"]
    )

    self.secondary_win = None
    self.frame_count = 0
    self.outliner = self.lg_primary.widget

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
    sec_kwargs = dict(
      width=self.win_width,
      height=self.win_height,
      x=sec_x,
      y=100,
      title="MultiWindowTest::Secondary",
      decorated=True,
      resizable=True,
      floating=FLOATING,
      focus_on_show=FOCUS_ON_SHOW,
      focus_follows_mouse=FOCUS_FOLLOWS_MOUSE,
      focus_to_front=FOCUS_TO_FRONT,
    )

    if args.fsmon:
      sec_kwargs["fullscreen_monitor"] = args.fsmon

    self.secondary_win = self.ezapp.createSecondaryWindow(**sec_kwargs)

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
