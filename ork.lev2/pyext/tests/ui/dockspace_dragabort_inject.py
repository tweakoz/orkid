#!/usr/bin/env ork.python
################################################################################
# F1/F3 gate: window-exit drag ABORT + session resurrection (lockstep, offscreen).
#
#  Reproduces the owner's live kill chain (~/dock_jul21.jsonl, gesture 1..3): a
#  titlebar drag whose cursor crosses the window boundary fires a synthesized
#  LOST_KEYFOCUS mid-drag. At HEAD that silently cleared the ui::Context drag
#  capture WITHOUT telling the captured widget, so the drag died, the DockSpace
#  session wedged and the DockDragHint overlay leaked — eating every subsequent
#  PUSH (one failed drag killed the whole session).
#
#  Two scripted gestures in one run:
#    GESTURE 1 (kill chain): PUSH panel A's titlebar, drag over panel B's leaf
#      (arms the drop hint), inject LOST_KEYFOCUS, keep dragging back over the left
#      edge and OUT of the window, RELEASE out-of-window. A healthy engine survives
#      the focus loss (capture is preserved), the out-of-window RELEASE cancels
#      cleanly: no live drag, no orphaned overlay, tree valid, layout unchanged.
#    GESTURE 2 (THE RESURRECTION ORACLE): a second drag on a DIFFERENT panel (B)
#      onto A's leaf CENTER must arm and commit a real moveChild — the layout
#      signature MUST change. This is exactly what died in the owner's gestures 2/3
#      (the orphaned hint from gesture 1 ate the PUSH).
#
#  RED at HEAD (pre-fix): gesture 1 leaks the hint (HAS_OVERLAY_G1=True) and its
#  orphan covers B's leaf, so gesture 2's PUSH is eaten (SIG_CHANGED_G2=False).
#  GREEN after F1 (cancel-correct capture) + F3 (overlay routing honesty).
#
#  Subprocess child + parse-stdout _pass protocol (copied from
#  dockspace_dragcancel_inject.py); state observables only (no pixel capture).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys, argparse, subprocess

ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2
import ork.uitest as U

tokens = CrcStringProxy()
W, H = 800, 600
COLORS = {"A": vec4(0.60, 0.12, 0.12, 1), "B": vec4(0.12, 0.55, 0.15, 1)}

# geometry (post-split): A alone in the LEFT leaf (~0..400), B alone in the RIGHT
# leaf (~400..800); each leaf's DockPanel titlebar is the top 40px band.
A_TITLE     = (150, 20)    # panel A titlebar
B_TITLE     = (600, 20)    # panel B titlebar
OVER_B_LEAF = (600, 150)   # inside B's leaf -> a valid CENTER drop for a drag of A
A_CENTER    = (200, 300)   # dead-center of A's leaf -> valid CENTER drop for a drag of B

VDT = 1.0  # virtual seconds per frame — wide enough that the two gestures' PUSHes
           # never derive a spurious DOUBLECLICK (which would eat the titlebar PUSH).

################################################################################

class App:
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H,
                                      freerun=False, target_ups=60.0, target_fps=60.0)
    self.ezapp.topWidget.enableUiDraw()
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    self.dock = lg.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=["dock"]).widget
    self.dock.clear = False
    self.A = self.dock.addPanel(uiclass=lev2.ui.Box, args=["A", COLORS["A"]], title="A"); self.A.name = "A"
    self.B = self.dock.addPanel(uiclass=lev2.ui.Box, args=["B", COLORS["B"]], title="B"); self.B.name = "B"
    # split: B out of A's leaf, docked to A's RIGHT -> two single-tab leaves.
    self.dock.moveChild(panel=self.B, to=self.A, zone=tokens.RIGHT)
    self.dock.updateLayout()
    self.sig0 = self.dock.layoutSignature()
    self.frame = 0
    self.ready = False; self.ready_counter = 0
    self.g1_done = False; self.g2_done = False; self.finished = False

  def onGpuInit(self, ctx):
    # virtualize the click clock so gesture spacing is deterministic (no doubleclick)
    self.ezapp.uicontext.virtual_time_enabled = True

  def onUpdate(self, updata):
    self.frame = int(updata.counter)
    self.ezapp.uicontext.virtual_time = self.frame * VDT
    if not self.ready:
      if self.A.width > 0 and self.B.width > 0:
        self.ready = True; self.ready_counter = self.frame
      return
    rel = self.frame - self.ready_counter
    if rel == 3 and not self.g1_done:
      self._gesture1_killchain()
      self.g1_done = True
    elif rel == 8 and not self.g2_done:
      self._gesture2_resurrection()
      self.g2_done = True
    elif rel == 12 and not self.finished:
      self.finished = True
      self.ezapp.signalExit()

  ##############################################################################

  def _titlebar_drag(self, x0, y0, steps, release_xy, lost_after=None):
    """PUSH (x0,y0); walk each (x,y) in steps as a DRAG; optionally inject
    LOST_KEYFOCUS after the step index `lost_after`; RELEASE at release_xy."""
    U.push(self.ezapp, x0, y0, W, H)
    for i, (x, y) in enumerate(steps):
      U.move(self.ezapp, x, y, W, H, button="left")
      if lost_after is not None and i == lost_after:
        U.lost_keyfocus(self.ezapp)   # window-exit synthesized focus loss
    U.release(self.ezapp, release_xy[0], release_xy[1], W, H)

  def _gesture1_killchain(self):
    # A titlebar -> over B's leaf (hint arms) -> LOST_KEYFOCUS -> back over the left
    # edge and OUT of the window -> RELEASE out-of-window (a cancel; no commit).
    steps = [(300, 100), OVER_B_LEAF, (5, 300), (-60, 320)]
    self._titlebar_drag(A_TITLE[0], A_TITLE[1], steps, release_xy=(-60, 320), lost_after=1)
    print(f"DRAG_ACTIVE_G1={self.dock.drag_active}", flush=True)
    print(f"HAS_OVERLAY_G1={self.ezapp.uicontext.hasOverlays()}", flush=True)
    print(f"VALID_G1={self.dock.validateTree()}", flush=True)
    print(f"SIG_UNCHANGED_G1={self.dock.layoutSignature() == self.sig0}", flush=True)

  def _gesture2_resurrection(self):
    sig_before = self.dock.layoutSignature()
    # B titlebar -> into A's leaf CENTER -> RELEASE : must commit a real moveChild.
    steps = [(400, 150), (300, 250), A_CENTER]
    self._titlebar_drag(B_TITLE[0], B_TITLE[1], steps, release_xy=A_CENTER)
    sig_after = self.dock.layoutSignature()
    print(f"SIG_CHANGED_G2={sig_after != sig_before}", flush=True)
    print(f"DRAG_ACTIVE_G2={self.dock.drag_active}", flush=True)
    print(f"HAS_OVERLAY_G2={self.ezapp.uicontext.hasOverlays()}", flush=True)
    print(f"VALID_G2={self.dock.validateTree()}", flush=True)
    print(f"NUM_PANELS_G2={self.dock.num_panels}", flush=True)

################################################################################

def run_child():
  App().ezapp.mainThreadLoop(); sys.exit(0)

def _parse(out):
  d = {}
  keys = ("DRAG_ACTIVE_G1", "HAS_OVERLAY_G1", "VALID_G1", "SIG_UNCHANGED_G1",
          "SIG_CHANGED_G2", "DRAG_ACTIVE_G2", "HAS_OVERLAY_G2", "VALID_G2", "NUM_PANELS_G2")
  for line in out.splitlines():
    for k in keys:
      if line.startswith(k + "="):
        d[k] = line[len(k) + 1:].strip()
  return d

def orchestrate():
  sp = os.path.abspath(__file__)
  r = subprocess.run([sp, "--child"], capture_output=True, text=True, timeout=120)
  sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
  d = _parse(r.stdout)
  problems = []
  if r.returncode != 0:
    problems.append(f"child exited rc={r.returncode}")
  # gesture 1: window-exit drag aborts CLEANLY (this is what wedged at HEAD)
  if d.get("HAS_OVERLAY_G1") != "False": problems.append("gesture 1 leaked a drag-hint overlay")
  if d.get("DRAG_ACTIVE_G1") != "False": problems.append("gesture 1 left the drag active")
  if d.get("VALID_G1")       != "True":  problems.append("gesture 1 left an invalid tree")
  if d.get("SIG_UNCHANGED_G1") != "True": problems.append("gesture 1 (a canceled drag) mutated the layout")
  # gesture 2: THE RESURRECTION ORACLE — a fresh drag must arm + commit a real move
  if d.get("SIG_CHANGED_G2") != "True": problems.append("RESURRECTION FAILED: gesture 2 did not commit a moveChild (session wedged)")
  if d.get("VALID_G2")       != "True":  problems.append("gesture 2 left an invalid tree")
  if d.get("DRAG_ACTIVE_G2") != "False": problems.append("gesture 2 left the drag active")
  if d.get("HAS_OVERLAY_G2") != "False": problems.append("gesture 2 leaked an overlay")
  if d.get("NUM_PANELS_G2")  != "2":     problems.append(f"gesture 2 lost a panel (num_panels={d.get('NUM_PANELS_G2')})")
  if problems:
    print("=== F1/F3 drag-abort gate FAILED ===", flush=True)
    for p in problems: print("  - " + p, flush=True)
    sys.exit(1)
  print("=== F1/F3 drag-abort gate PASSED ===", flush=True)
  sys.exit(0)

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--child", action="store_true")
  args = ap.parse_args()
  if args.child:
    run_child()
  orchestrate()

if __name__ == "__main__":
  main()
