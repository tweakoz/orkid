#!/usr/bin/env ork.python
################################################################################
# S1 gate: DockSpace.moveChild move matrix.
#
#  Exercises every attach mode of the move primitive and asserts the structural
#  invariants after EACH move:
#    - validateTree() clean (no stale anchor::Layout),
#    - tab-insert (CENTER) collapses the emptied source leaf,
#    - each of LEFT/RIGHT/TOP/BOTTOM yields a valid 2-leaf split of the right
#      orientation,
#    - last-panel-out collapses the leaf and frees its split guide (guide count
#      returns to the single-leaf baseline),
#    - a collapse -> re-split round-trip returns layoutSignature to the original.
#
#  Runs in an offscreen ezapp (so FontMan is live for DockPanel construction);
#  pure structural assertions, no pixel capture.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

def build_two(lg, name):
  """A DockSpace with panel A (left) and B (right) in separate leaves."""
  dock = lg.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=[name]).widget
  dock.clear = False
  a = dock.addPanel(uiclass=lev2.ui.LayoutGroup, args=[name + "A"], title="A")
  b = dock.split(target=a, placement=tokens.RIGHT, proportion=0.5,
                 uiclass=lev2.ui.LayoutGroup, args=[name + "B"], title="B")
  return dock, a, b

################################################################################

class App:
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self, width=800, height=600, offscreen=True)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.done = False
    self._run()
    self.done = True

  def _run(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0

    ####################################
    # tab-insert (CENTER) -> source leaf collapses to a single leaf
    ####################################
    dock, a, b = build_two(lg, "d_center")
    sig_two = dock.layoutSignature()
    assert dock.validateTree(), "center: pre-move validateTree failed"
    assert "P:" in sig_two, f"two-panel split has no guide: {sig_two}"
    assert len(dock.vertical_guides) == 1, f"expected 1 split guide, got {len(dock.vertical_guides)}"

    dock.moveChild(panel=a, to=b, zone=tokens.CENTER)
    assert dock.validateTree(), "center: post-move validateTree failed"
    assert dock.num_panels == 2, f"center: lost panels ({dock.num_panels})"
    sig_one = dock.layoutSignature()
    assert "P:" not in sig_one, f"center: split guide not freed after collapse: {sig_one}"
    assert len(dock.vertical_guides) == 0, \
        f"center: split guide count did not return to baseline: {len(dock.vertical_guides)}"
    print(f"[movematrix] CENTER: {sig_two} -> {sig_one}  OK", flush=True)

    ####################################
    # each zone split -> valid 2-leaf split of the correct orientation
    ####################################
    for zname, ztok, orient in [
        ("LEFT",  tokens.LEFT,   "V"),
        ("RIGHT", tokens.RIGHT,  "V"),
        ("TOP",   tokens.TOP,    "H"),
        ("BOTTOM",tokens.BOTTOM, "H")]:
      dock, a, b = build_two(lg, "d_" + zname)
      dock.moveChild(panel=a, to=b, zone=ztok)
      assert dock.validateTree(), f"{zname}: validateTree failed"
      assert dock.num_panels == 2, f"{zname}: lost panels"
      sig = dock.layoutSignature()
      assert f"{orient}:P:0.500" in sig, f"{zname}: wrong split orientation: {sig}"
      print(f"[movematrix] zone {zname} -> {sig}  OK", flush=True)

    ####################################
    # collapse -> re-split round-trip returns to the original signature
    ####################################
    dock, a, b = build_two(lg, "d_rt")
    sig0 = dock.layoutSignature()
    dock.moveChild(panel=a, to=b, zone=tokens.CENTER)   # collapse (A tab-inserts into B's leaf)
    assert dock.validateTree(), "roundtrip: mid validateTree failed"
    dock.moveChild(panel=a, to=b, zone=tokens.LEFT)     # re-split A back to the left
    assert dock.validateTree(), "roundtrip: post validateTree failed"
    sig1 = dock.layoutSignature()
    assert sig1 == sig0, f"roundtrip signature mismatch:\n  orig={sig0}\n  back={sig1}"
    print(f"[movematrix] round-trip signature stable: {sig1}  OK", flush=True)

    print("=== S1 move-matrix gate PASSED ===", flush=True)

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updinfo):
    if self.done:
      self.ezapp.signalExit()

################################################################################

App().ezapp.mainThreadLoop()
sys.exit(0)
