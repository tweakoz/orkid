#!/usr/bin/env ork.python
################################################################################
# S4 gate: dock_layout persistence error paths.
#
#   - duplicate panel id  -> loud refusal at save time (naming the dup)
#   - missing panel id    -> loud error at load time (naming the missing id)
#   - extra live panel    -> documented default: appended to the last leaf
#
#  Offscreen ezapp (FontMan live for DockPanel construction); logic in onUpdate
#  after the first layout settles.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2
from ork.ui import dock_layout

tokens = CrcStringProxy()
W, H = 800, 600

################################################################################

def _mk(dock, ref, name, zone=None, prop=0.5):
  if ref is None:
    p = dock.addPanel(uiclass=lev2.ui.LayoutGroup, args=[name], title=name)
  else:
    p = dock.split(target=ref, placement=zone, proportion=prop,
                   uiclass=lev2.ui.LayoutGroup, args=[name], title=name)
  p.name = name
  return p

################################################################################

class App:
  def __init__(self):
    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H, offscreen=True)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()
    self.lg = self.ezapp.topLayoutGroup
    self.lg.margin = 0
    self.frame = 0
    self.done = False
    self.failed = False

  def _new_dock(self, name):
    d = self.lg.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=[name]).widget
    d.clear = False
    return d

  def _run(self):
    fails = []

    ####################################
    # duplicate id -> loud refusal at save
    ####################################
    d = self._new_dock("dup")
    a = _mk(d, None, "same")
    b = _mk(d, a, "other", tokens.RIGHT, 0.5)
    b.name = "same"            # force a duplicate id
    self.lg.setRect(0, 0, W, H); d.updateLayout()
    try:
      dock_layout.save_layout(d)
      fails.append("duplicate id: save did NOT refuse")
    except ValueError as ex:
      if "same" not in str(ex):
        fails.append(f"duplicate id: message did not name the dup: {ex}")
      else:
        print(f"[persist-err] duplicate refused loudly: {ex}", flush=True)

    ####################################
    # missing id -> loud error at load (names the id)
    ####################################
    d2 = self._new_dock("miss")
    x = _mk(d2, None, "x0")
    y = _mk(d2, x, "x1", tokens.RIGHT, 0.5)
    self.lg.setRect(0, 0, W, H); d2.updateLayout()
    saved = dock_layout.save_layout(d2)
    # a fresh dock missing panel 'x1'
    d3 = self._new_dock("miss2")
    _mk(d3, None, "x0")
    self.lg.setRect(0, 0, W, H); d3.updateLayout()
    try:
      dock_layout.load_layout(d3, saved)
      fails.append("missing id: load did NOT error")
    except ValueError as ex:
      if "x1" not in str(ex):
        fails.append(f"missing id: message did not name the missing id: {ex}")
      else:
        print(f"[persist-err] missing id errored loudly: {ex}", flush=True)

    ####################################
    # extra live panel -> appended to the last leaf (documented default)
    ####################################
    d4 = self._new_dock("extra_src")
    e0 = _mk(d4, None, "e0")
    e1 = _mk(d4, e0, "e1", tokens.RIGHT, 0.5)
    self.lg.setRect(0, 0, W, H); d4.updateLayout()
    saved2 = dock_layout.save_layout(d4)  # layout of {e0 | e1}

    d5 = self._new_dock("extra_dst")
    f0 = _mk(d5, None, "e0")
    f1 = _mk(d5, f0, "e1", tokens.RIGHT, 0.5)
    fx = _mk(d5, f1, "e_extra", tokens.BOTTOM, 0.5)   # extra, not in saved layout
    self.lg.setRect(0, 0, W, H); d5.updateLayout()
    dock_layout.load_layout(d5, saved2)
    d5.updateLayout()
    # e_extra must land in the last leaf (need[-1] == 'e1' by tree order)
    after = dock_layout.save_layout(d5)
    # find the leaf containing e_extra
    def _leaf_with(node, pid):
      if "leaf" in node:
        return node["leaf"] if pid in node["leaf"] else None
      return _leaf_with(node["a"], pid) or _leaf_with(node["b"], pid)
    leaf = _leaf_with(after["tree"], "e_extra")
    if not d5.validateTree():
      fails.append("extra: validateTree not clean after load")
    if leaf is None or "e1" not in leaf:
      fails.append(f"extra: e_extra not appended to the last leaf (with e1): leaf={leaf}")
    else:
      print(f"[persist-err] extra panel appended to last leaf: {sorted(leaf)}", flush=True)

    self.failed = len(fails) > 0
    for f in fails:
      print("  FAIL " + f, flush=True)
    print("=== S4 persistence error-paths gate " + ("FAILED" if self.failed else "PASSED") + " ===", flush=True)

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
