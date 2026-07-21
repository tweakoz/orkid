#!/usr/bin/env ork.python
################################################################################
# S4 gate: DockSpace layout persistence round-trip + fresh-construction restore.
#
#   --mode orig      : build a nontrivial layout (3 splits both orientations +
#                      a 2-tab leaf, 2nd tab active); save twice (byte-determinism)
#                      -> /tmp/dock_layout.json; capture -> orig.png.
#   --mode restored  : build the SAME panels (by id) in a scrambled arrangement,
#                      load_layout(saved json); capture -> restored.png; re-save.
#   --mode fresh     : build the SAME panels in yet another construction order,
#                      load_layout(saved json); capture -> fresh.png; re-save.
#
#  Orchestrator asserts: save-twice byte-identical; restored/fresh captures
#  BYTE-EQUAL to orig; re-saved JSON == orig JSON (structure + active tab);
#  validateTree clean.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2
from ork.ui.dock_layout import save_layout, load_layout, to_json

tokens = CrcStringProxy()

W, H = 900, 640
JSON_PATH = "/tmp/dock_layout.json"

COLORS = {
  "panel_0": vec4(0.60, 0.12, 0.12, 1),
  "panel_1": vec4(0.12, 0.55, 0.15, 1),
  "panel_2": vec4(0.14, 0.16, 0.62, 1),
  "panel_3": vec4(0.60, 0.55, 0.12, 1),
  "panel_4": vec4(0.12, 0.55, 0.58, 1),
}

################################################################################

def _mk(dock, ref, name, zone=None, prop=0.5):
  if ref is None:
    p = dock.addPanel(uiclass=lev2.ui.PrimCanvas, args=[name], title=name)
  else:
    p = dock.split(target=ref, placement=zone, proportion=prop,
                   uiclass=lev2.ui.PrimCanvas, args=[name], title=name)
  p.name = name
  return p

def build_orig(dock):
  p0 = _mk(dock, None, "panel_0")
  p1 = _mk(dock, p0, "panel_1", tokens.RIGHT, 0.42)
  p2 = _mk(dock, p0, "panel_2", tokens.BOTTOM, 0.55)
  p3 = _mk(dock, p1, "panel_3", tokens.BOTTOM, 0.37)
  p4 = _mk(dock, p2, "panel_4", tokens.RIGHT, 0.5)     # transient split...
  dock.moveChild(panel=p4, to=p3, zone=tokens.CENTER)  # ...tabbed into p3's leaf
  dock.activatePanel(p4)                               # 2nd tab active
  return [p0, p1, p2, p3, p4]

def build_scrambled(dock):
  a = _mk(dock, None, "panel_4")
  b = _mk(dock, a, "panel_2", tokens.RIGHT, 0.3)
  c = _mk(dock, b, "panel_0", tokens.BOTTOM, 0.7)
  d = _mk(dock, a, "panel_3", tokens.BOTTOM, 0.5)
  e = _mk(dock, c, "panel_1", tokens.RIGHT, 0.6)
  return [a, b, c, d, e]

def build_fresh(dock):
  a = _mk(dock, None, "panel_2")
  b = _mk(dock, a, "panel_0", tokens.BOTTOM, 0.5)
  c = _mk(dock, a, "panel_4", tokens.RIGHT, 0.5)
  d = _mk(dock, c, "panel_1", tokens.TOP, 0.5)
  e = _mk(dock, b, "panel_3", tokens.RIGHT, 0.5)
  return [a, b, c, d, e]

################################################################################

class App:
  def __init__(self, mode):
    self.mode = mode
    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H, offscreen=True)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    self.dock = lg.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=["dock"]).widget
    self.dock.clear = False

    if mode == "orig":
      self.panels = build_orig(self.dock)
    elif mode == "restored":
      self.panels = build_scrambled(self.dock)
    else:
      self.panels = build_fresh(self.dock)

    lg.setRect(0, 0, W, H)     # force layout so setProportion/hit-test see geometry
    self.dock.updateLayout()

    if mode == "orig":
      j1 = to_json(save_layout(self.dock))
      j2 = to_json(save_layout(self.dock))
      print(f"DETERMINISTIC={j1 == j2}", flush=True)
      with open(JSON_PATH, "w") as f:
        f.write(j1)
      print(f"ORIGJSON={j1}", flush=True)
    else:
      with open(JSON_PATH) as f:
        saved = f.read()
      load_layout(self.dock, saved)
      self.dock.updateLayout()
      print(f"RESAVE={to_json(save_layout(self.dock))}", flush=True)

    print(f"SIG={self.dock.layoutSignature()}", flush=True)
    print(f"VALID={self.dock.validateTree()}", flush=True)

    self._canvases = [(p.child, COLORS[p.name]) for p in self.panels]
    self.frame = 0
    self._gpu_frames = 0
    self.captured = False
    self._inflight = False
    self._cap = None
    self._buf = None

  def onGpuInit(self, ctx):
    for c, col in self._canvases:
      c.gpuInit(ctx)
      layer = c.createLayer("main")
      qp = lev2.ui.QuadPrimitive(pipeline=c.pipelineSolid)
      qd = lev2.ui.QuadData()
      qd.setPosition(0, 0)
      qd.setSize(4000, 4000)
      qd.setColor(col)
      qp.addQuad(qd)
      layer.addPrimitive(qp)

  def onUpdate(self, updinfo):
    self.frame += 1
    for c, _ in self._canvases:
      c.markDirty()

  def onGpuPostFrame(self, ctx):
    if self.captured:
      return
    # #87 settle barrier: count RENDERED frames on THIS (GPU/capture) thread, not the
    # update-thread counter. Panel titlebar text warms up over the first several
    # composited frames (a general DockSpace settle: orig ~frame 8; a load_layout
    # restore ~frame 12 — the reparent churn re-dirties the text surfaces). A capture
    # keyed to the update-thread frame count sits on the restored settle knee and, on a
    # 1-render update/GPU phase slip, snapshots a pre-settle frame => the intermittent
    # 1/4 restored-vs-orig divergence with structural oracles exact. 24 rendered frames
    # clears both knees with wide margin (per-frame markDirty keeps redrawing).
    self._gpu_frames += 1
    if self._gpu_frames < 24:
      return
    if not self._inflight:
      rtg = ctx.FBI.main_RTG
      self._buf = lev2.CaptureBuffer()
      self._cap = ctx.FBI.captureAsFormat(rtg.buffer(0), self._buf, "RGBA8")
      self._inflight = True
      return
    if self._cap is not None and not bool(self._cap.is_ready):
      return
    import numpy
    arr = numpy.array(self._buf, dtype=numpy.uint8).reshape(self._buf.height, self._buf.width, 4)
    self.captured = True
    try:
      from PIL import Image
      Image.fromarray(arr[..., :3]).save(f"/tmp/dockpersist_{self.mode}.png")
    except Exception as e:
      print(f"png save failed: {e}", flush=True)
    self.ezapp.signalExit()

################################################################################

def run_capture(mode):
  App(mode).ezapp.mainThreadLoop()
  sys.exit(0)

def _parse(out):
  d = {}
  for line in out.splitlines():
    for k in ("DETERMINISTIC", "ORIGJSON", "RESAVE", "SIG", "VALID"):
      if line.startswith(k + "="):
        d[k] = line[len(k) + 1:]
  return d

def _pngdiff(a, b):
  import numpy
  from PIL import Image
  ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
  ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
  if ia.shape != ib.shape:
    return -1, -1, None
  d = numpy.abs(ia - ib).max(axis=2)
  differing = int((d > 0).sum())
  if differing:
    ys, xs = numpy.where(d > 0)
    bbox = (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))
  else:
    bbox = None
  return differing, int(d.max()), bbox

def orchestrate():
  self_path = os.path.abspath(__file__)
  res = {}
  for mode in ("orig", "restored", "fresh"):
    r = subprocess.run([self_path, "--mode", mode], capture_output=True, text=True)
    sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
    res[mode] = (r.returncode, _parse(r.stdout))

  problems = []
  for m, (rc, _) in res.items():
    if rc != 0: problems.append(f"{m} run failed (rc={rc})")

  if res["orig"][1].get("DETERMINISTIC") != "True":
    problems.append("save-twice not byte-deterministic")

  orig_json = res["orig"][1].get("ORIGJSON")
  for m in ("restored", "fresh"):
    if res[m][1].get("RESAVE") != orig_json:
      problems.append(f"{m} re-saved JSON != orig JSON")
    if res[m][1].get("VALID") != "True":
      problems.append(f"{m} validateTree not clean")
    if res[m][1].get("SIG") != res["orig"][1].get("SIG"):
      problems.append(f"{m} layoutSignature != orig")
    diff, mx, bbox = _pngdiff(f"/tmp/dockpersist_{m}.png", "/tmp/dockpersist_orig.png")
    print(f"[{m} vs orig] differing={diff} maxdiff={mx} bbox={bbox}", flush=True)
    if diff != 0:
      problems.append(f"{m} capture not BYTE-EQUAL to orig (differing={diff} maxdiff={mx} bbox={bbox})")

  if problems:
    print("=== S4 persistence round-trip gate FAILED ===", flush=True)
    for p in problems: print("  - " + p, flush=True)
    sys.exit(1)
  print("=== S4 persistence round-trip gate PASSED ===", flush=True)
  sys.exit(0)

################################################################################

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["orig", "restored", "fresh"], default=None)
  args = ap.parse_args()
  if args.mode:
    run_capture(args.mode)
  orchestrate()

if __name__ == "__main__":
  main()
