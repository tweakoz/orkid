#!/usr/bin/env ork.python
################################################################################
# Global event handler test
#   - Creates a primary window with a TextBox
#   - Creates a secondary window with a TextBox
#   - Registers a global event handler that prints KEY_DOWN / KEY_UP from
#     either window, regardless of which one currently has focus.
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import signal
from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class GlobalEventsTest:

  def __init__(self):
    self.win_w = 480
    self.win_h = 320

    self.ezapp = lev2.OrkEzApp.create(self,
                                      name="GlobalEventsTest::Primary",
                                      width=self.win_w,
                                      height=self.win_h,
                                      left=100,
                                      top=100)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    # Primary window: a TextBox in the top layout group
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    self.primary_tb = lg.makeChild(
      uiclass=lev2.ui.TextBox,
      args=["primary_tb", vec4(0.15, 0.15, 0.25, 1), "primary"],
      fill=True,
    ).widget
    self.primary_tb.setText("PRIMARY WINDOW\n\nFocus this window and press keys.\n"
                            "Global handler should print regardless of which window has focus.")

    # Register global event handler — fires for events from any window
    self._global_token = self.ezapp.addGlobalEventHandler(self._onGlobalEvent)

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()
    signal.signal(signal.SIGINT, onCtrlC)

  ##############################################

  def _onGlobalEvent(self, ev):
    code = ev.code
    if code == tokens.KEY_DOWN.hashed:
      label = "KEY_DOWN"
    elif code == tokens.KEY_UP.hashed:
      label = "KEY_UP"
    else:
      return  # ignore non-keyboard events

    kc = ev.keycode
    printable = chr(kc) if 32 <= kc < 127 else "?"
    mods = []
    if ev.shift: mods.append("SHIFT")
    if ev.ctrl:  mods.append("CTRL")
    if ev.alt:   mods.append("ALT")
    if ev.super: mods.append("SUPER")
    mod_str = ("+" + "+".join(mods)) if mods else ""
    print(f"[GLOBAL] {label} keycode={kc} ('{printable}'){mod_str}", flush=True)

  ##############################################

  def onGpuInit(self, ctx):
    # Create the secondary window with its own TextBox
    sec_x = 100 + self.win_w + 20
    self.secondary_win = self.ezapp.createSecondaryWindow(
      width=self.win_w,
      height=self.win_h,
      x=sec_x,
      y=100,
      title="GlobalEventsTest::Secondary",
      decorated=True,
      resizable=True,
    )

    uic = self.secondary_win.ui_context
    win_w = self.secondary_win.width
    win_h = self.secondary_win.height

    root = lev2.ui.LayoutGroup.create("sec_lg")
    root.setRect(0, 0, win_w, win_h)
    root.margin = 4
    uic.top = root

    self.secondary_tb = root.makeChild(
      uiclass=lev2.ui.TextBox,
      args=["secondary_tb", vec4(0.25, 0.15, 0.15, 1), "secondary"],
      fill=True,
    ).widget
    self.secondary_tb.setText("SECONDARY WINDOW\n\nFocus this window and press keys.\n"
                              "Global handler should print here too.")

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

################################################################################

GlobalEventsTest().ezapp.mainThreadLoop()
print("Global events test passed!")
