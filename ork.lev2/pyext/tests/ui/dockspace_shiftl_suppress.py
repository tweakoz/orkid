#!/usr/bin/env ork.python
################################################################################
# Gate: Shift+L (reset-layout) editor-app chord SUPPRESSION on text-input focus.
#
#  The suppression predicate (ork.ui.app_state.text_input_has_focus) is the SHARED
#  logic the dock-adopted editors use in their global-event handler. This gate
#  drives it against REAL context focus state (an F32Edit grabbing the caret) and
#  asserts, via injected events through the same global-handler funnel:
#    - NO text focus  -> Shift+L FIRES  (predicate False),
#    - F32Edit focused -> Shift+L is SUPPRESSED (predicate True), layout signature
#      UNCHANGED, and the focused widget still RECEIVES keys (a typed value commits).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine.core import vec4, vec3, CrcStringProxy
from orkengine import lev2
import ork.uitest as U
from ork.ui.app_state import text_input_has_focus

tokens = CrcStringProxy()
W, H = 800, 600
ENTER = 257


class App:
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H,
                                      freerun=False, target_ups=60.0, target_fps=60.0)
    self.ezapp.topWidget.enableUiDraw()
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    # a real focus-grabbing text widget (F32Edit) beside a DockSpace: clicking the
    # field takes the caret (keyboard_focus_widget) -> the suppression predicate flips.
    field_item = lg.makeChild(fill=True, uiclass=lev2.ui.F32Edit,
                              args=["fld", "Val", 0.0, 0.0, 1e6])
    self.field = field_item.widget
    self._committed = []
    self.field.onValueCommitted(lambda v: self._committed.append(v))
    dock_item = lg.split(layout=field_item.layout, placement=tokens.RIGHT, proportion=0.7,
                         margin=0, uiclass=lev2.ui.DockSpace, args=["dock"])
    self.dock = dock_item.widget
    self.dock.clear = False
    self.A = self.dock.addPanel(uiclass=lev2.ui.Box, args=["A", vec4(0.5, 0.2, 0.2, 1)], title="A")
    self.B = self.dock.split(target=self.A, placement=tokens.RIGHT, proportion=0.5,
                             uiclass=lev2.ui.Box, args=["B", vec4(0.2, 0.2, 0.5, 1)], title="B")

    lg.setRect(0, 0, W, H)
    self.dock.updateLayout()

    self.uic = self.ezapp.uicontext
    self.reset_fires = 0
    self._tok = self.ezapp.addGlobalEventHandler(self._onGlobal)

    self.frame = 0
    self.ready = False
    self.ready_counter = 0
    self.results = {}
    self.done = False

  # the SHARED editor-app-level Shift+L decision (mirrors terrainedit._onGlobalUiEvent)
  def _onGlobal(self, ev):
    if ev.code != tokens.KEY_DOWN.hashed:
      return
    if ev.keycode == ord("L") and ev.shift and not (ev.super or ev.ctrl):
      if text_input_has_focus(self.uic):
        return
      self.reset_fires += 1

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updata):
    self.frame = int(updata.counter)
    if not self.ready:
      if self.B.width > 0:
        self.ready = True
        self.ready_counter = self.frame
        self.sig0 = self.dock.layoutSignature()
      return
    rel = self.frame - self.ready_counter
    r = self.results

    if rel == 3:
      # unfocused: predicate False, Shift+L FIRES
      r["focus_before"] = text_input_has_focus(self.uic)
      U.key_chord(self.ezapp, ord("L"), mods={"shift": True})
      r["fires_unfocused"] = self.reset_fires
      r["sig_after_unfocused"] = self.dock.layoutSignature()
    elif rel == 6:
      # click the F32Edit -> it grabs the caret (keyboard_focus_widget)
      cx = self.field.x + self.field.width // 2
      cy = self.field.y + self.field.height // 2
      U.click(self.ezapp, cx, cy, W, H)
    elif rel == 9:
      r["focus_after"] = text_input_has_focus(self.uic)
      before = self.reset_fires
      U.key_chord(self.ezapp, ord("L"), mods={"shift": True})
      r["fired_delta_focused"] = self.reset_fires - before
      r["sig_after_focused"] = self.dock.layoutSignature()
    elif rel == 12:
      # prove the focused widget still RECEIVES keys: type '7' then ENTER -> commit
      U.key_chord(self.ezapp, ord("7"), mods=None)
      U.key_chord(self.ezapp, ENTER, mods=None)
    elif rel == 15 and not self.done:
      self.done = True
      r["committed"] = list(self._committed)
      self._verdict()
      self.ezapp.signalExit()

    if self.ready and (self.frame - self.ready_counter) > 400 and not self.done:
      self.done = True
      print("TIMEOUT", flush=True)
      self.ezapp.signalExit()

  def _verdict(self):
    r = self.results
    problems = []
    if r.get("focus_before") is not False:
      problems.append(f"focus_before should be False, got {r.get('focus_before')}")
    if r.get("fires_unfocused") != 1:
      problems.append(f"Shift+L did not fire while unfocused (fires={r.get('fires_unfocused')})")
    if r.get("sig_after_unfocused") != self.sig0:
      problems.append("layout signature changed by the observer-only handler")
    if r.get("focus_after") is not True:
      problems.append(f"F32Edit did not take focus (focus_after={r.get('focus_after')})")
    if r.get("fired_delta_focused") != 0:
      problems.append(f"Shift+L NOT suppressed while focused (delta={r.get('fired_delta_focused')})")
    if r.get("sig_after_focused") != self.sig0:
      problems.append("layout signature changed while a text widget owned focus")
    if not r.get("committed"):
      problems.append("focused widget did not receive keys (no value committed)")
    print(f"RESULTS {r}", flush=True)
    if problems:
      print("=== Shift+L suppression gate FAILED ===", flush=True)
      for p in problems:
        print("  - " + p, flush=True)
      self.ok = False
    else:
      print("=== Shift+L suppression gate PASSED ===", flush=True)
      self.ok = True


def main():
  app = App()
  app.ezapp.mainThreadLoop()
  sys.exit(0 if getattr(app, "ok", False) else 1)


if __name__ == "__main__":
  main()
