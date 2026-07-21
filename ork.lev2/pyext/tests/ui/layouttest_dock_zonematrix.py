#!/usr/bin/env ork.python
################################################################################
# S2 gate: DockSpace.zoneHitTest edge-band matrix.
#
#  A probe grid over a single full-bleed leaf (800x600) asserts the hit-test
#  zone at each point against the band model (outer 25% = LEFT/RIGHT/TOP/BOTTOM,
#  interior = CENTER, nearest edge wins on a corner), including the exact band
#  boundaries. Plus a 2-leaf check that the correct target panel/leaf is hit.
#
#  Offscreen ezapp so the layout is live; hit-test is read-only (asserts run in
#  onUpdate after the first layout settles).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

W, H = 800, 600
BAND = 0.25

################################################################################

def expected_zone(x, y, w, h):
  dl = x / w; dr = 1.0 - x / w
  dt = y / h; db = 1.0 - y / h
  best = BAND; z = "CENTER"
  if dl < best: best = dl; z = "LEFT"
  if dr < best: best = dr; z = "RIGHT"
  if dt < best: best = dt; z = "TOP"
  if db < best: best = db; z = "BOTTOM"
  return z

################################################################################

class App:
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H, offscreen=True)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.margin = 0

    # single full-bleed leaf for the exact band matrix
    self.dock1 = lg.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=["dock1"]).widget
    self.dock1.clear = False
    self.a = self.dock1.addPanel(uiclass=lev2.ui.LayoutGroup, args=["A"], title="A")

    self.frame = 0
    self.done = False
    self.failed = False

  def _run(self):
    fails = []
    n = 0

    # ---- exact band matrix over the single leaf [0,0,800,600] ----
    xs = [40, 150, 199, 200, 300, 400, 500, 600, 601, 650, 760]
    ys = [30, 110, 149, 150, 200, 300, 400, 450, 451, 490, 570]
    for y in ys:
      for x in xs:
        n += 1
        hit = self.dock1.zoneHitTest(x, y)
        exp = expected_zone(x, y, W, H)
        if hit is None:
          fails.append(f"({x},{y}) no leaf hit (expected {exp})")
          continue
        panel, zone = hit
        if zone != exp:
          fails.append(f"({x},{y}) zone={zone} expected={exp}")
        if panel is not self.a:
          fails.append(f"({x},{y}) hit wrong panel")

    print(f"[zonematrix] probed {n} points over single leaf; {len(fails)} mismatches", flush=True)
    for f in fails[:12]:
      print("  MISS " + f, flush=True)

    # ---- 2-leaf leaf-selection: A left, B right ----
    lg2 = self.ezapp.topLayoutGroup
    dock2 = lg2.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=["dock2"]).widget
    dock2.clear = False
    a2 = dock2.addPanel(uiclass=lev2.ui.LayoutGroup, args=["A2"], title="A2")
    b2 = dock2.split(target=a2, placement=tokens.RIGHT, proportion=0.5,
                     uiclass=lev2.ui.LayoutGroup, args=["B2"], title="B2")
    dock2.updateLayout()
    hitL = dock2.zoneHitTest(200, 300)   # left leaf interior
    hitR = dock2.zoneHitTest(600, 300)   # right leaf interior
    two_ok = True
    if hitL is None or hitL[0] is not a2 or hitL[1] != "CENTER":
      two_ok = False; fails.append(f"2-leaf left probe wrong: {hitL}")
    if hitR is None or hitR[0] is not b2 or hitR[1] != "CENTER":
      two_ok = False; fails.append(f"2-leaf right probe wrong: {hitR}")
    print(f"[zonematrix] 2-leaf selection {'OK' if two_ok else 'FAIL'} "
          f"(L->{hitL}, R->{hitR})", flush=True)

    self.failed = len(fails) > 0
    print("=== S2 zone-matrix gate " + ("FAILED" if self.failed else "PASSED") + " ===", flush=True)

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updinfo):
    self.frame += 1
    if self.frame == 5 and not self.done:
      self._run()
      self.done = True
    if self.done:
      self.ezapp.signalExit()

################################################################################

app = App()
app.ezapp.mainThreadLoop()
sys.exit(1 if app.failed else 0)
