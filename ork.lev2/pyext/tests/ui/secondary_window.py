#!/usr/bin/env ork.python
################################################################################
# Multi-window test: Main window + secondary window with EvTestBox on each
# Tests graphics and UI events on both windows
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import signal
import sys
from orkengine.core import vec4
from orkengine import lev2

################################################################################

# Auto-close after 5 seconds for automated testing
AUTO_CLOSE = "--auto-close" in sys.argv

################################################################################

class MultiWindowTest:

  def __init__(self):
    super().__init__()

    # Window sizes: 640x720 each, side by side
    self.win_width = 640
    self.win_height = 720

    self.ezapp = lev2.OrkEzApp.create(self, width=self.win_width, height=self.win_height, left=100, top=100)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    # Main window layout with EvTestBox (1x1 grid)
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    self.main_griditems = lg.makeGrid(
      width=1,
      height=1,
      margin=4,
      uiclass=lev2.ui.EvTestBox,
      args=["main_evtb", vec4(0.2, 0.3, 0.5, 1)]
    )
    self.main_evtestbox = self.main_griditems[0].widget

    self.secondary_win = None
    self.frame_count = 0

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def onGpuInit(self, ctx):
    print("Main window GPU init")

    # GPU init the main window EvTestBox
    self.main_evtestbox.gpuInit(ctx)

    # Create secondary window - side by side with main
    sec_x = 100 + self.win_width + 20  # 20px gap between windows
    self.secondary_win = self.ezapp.createSecondaryWindow(
      width=self.win_width,
      height=self.win_height,
      x=sec_x,
      y=100,
      title="Secondary Window",
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
    self.sec_griditems = root.makeGrid(
      width=1,
      height=1,
      margin=4,
      uiclass=lev2.ui.EvTestBox,
      args=["sec_evtb", vec4(0.5, 0.3, 0.2, 1)]
    )
    self.sec_evtestbox = self.sec_griditems[0].widget
    print(f"[PY] EvTestBox: x={self.sec_evtestbox.x} y={self.sec_evtestbox.y} w={self.sec_evtestbox.width} h={self.sec_evtestbox.height}", flush=True)

    # GPU init for secondary window
    def on_sec_gpu_init(sec_ctx):
      print(f"[PY] GPU init: EvTestBox: x={self.sec_evtestbox.x} y={self.sec_evtestbox.y} w={self.sec_evtestbox.width} h={self.sec_evtestbox.height}", flush=True)
      root.gpuInit(sec_ctx)
      self.sec_evtestbox.gpuInit(sec_ctx)

    self.secondary_win.onGpuInit = on_sec_gpu_init

    # No need to set onDraw - C++ handles drawing ui::Context automatically

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
