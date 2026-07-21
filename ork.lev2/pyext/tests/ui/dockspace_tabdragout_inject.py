#!/usr/bin/env ork.python
################################################################################
# S3 gate: TabWidget drag-OUT, END-TO-END through INJECTED events (lockstep).
#
#  Layout: leaf_A hosts tabs [A,B]; leaf_C hosts [C] (V split at 0.5).
#  A scripted drag (ork.uitest.play via app.injectUiEvent, from onUpdate in
#  lockstep) grabs tab A's HEADER, drags it out of the bar into the RIGHT zone
#  of leaf_C, and releases -> the Context derives BEGIN_DRAG/DRAG/END_DRAG,
#  TabWidget hands off to the DockSpace drag session, END_DRAG -> moveChild.
#
#    --mode injected : drive the drag; capture final.
#    --mode direct   : dock.moveChild(A -> RIGHT of C) directly; capture final.
#  Orchestrator: injected capture BYTE-EQUAL to direct; injected run twice ->
#  identical captures (determinism); validateTree clean.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2
import ork.uitest as U

tokens = CrcStringProxy()
W, H = 800, 600

COLORS = {"A": vec4(0.60, 0.12, 0.12, 1), "B": vec4(0.12, 0.55, 0.15, 1),
          "C": vec4(0.14, 0.16, 0.62, 1)}

TAB_A = (10, 15)         # tab A header (leaf_A tab bar, top-left)
RIGHT_OF_C = (750, 300)  # leaf_C [400..800] RIGHT band (x>700)

################################################################################

class App:
  def __init__(self, mode):
    self.mode = mode
    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H,
                                      freerun=False, target_ups=60.0, target_fps=60.0)
    self.ezapp.topWidget.enableUiDraw()
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    self.dock = lg.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=["dock"]).widget
    self.dock.clear = False

    # Box content = immediate-mode solid color (resize-safe; a runtime-resized
    # PrimCanvas Surface renders black until re-init, which is orthogonal here).
    self.A = self.dock.addPanel(uiclass=lev2.ui.Box, args=["A", COLORS["A"]], title="A"); self.A.name = "A"
    self.C = self.dock.split(target=self.A, placement=tokens.RIGHT, proportion=0.5,
                             uiclass=lev2.ui.Box, args=["C", COLORS["C"]], title="C"); self.C.name = "C"
    self.B = self.dock.addPanel(uiclass=lev2.ui.Box, args=["B", COLORS["B"]], title="B"); self.B.name = "B"
    # leaf_A now hosts [A,B]; leaf_C hosts [C]
    self.panels = [self.A, self.B, self.C]

    if mode == "direct":
      self.dock.moveChild(panel=self.A, to=self.C, zone=tokens.RIGHT)

    self.frame = 0
    self.ready = False
    self.ready_counter = 0
    self.driven = False
    self.captured = False
    self._inflight = False
    self._cap = None
    self._buf = None

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updata):
    self.frame = int(updata.counter)
    if not self.ready:
      if self.A.width > 0 and self.C.width > 0:
        self.ready = True
        self.ready_counter = self.frame
      return
    rel = self.frame - self.ready_counter
    if self.mode == "injected" and rel == 3 and not self.driven:
      self.driven = True
      U.drag(self.ezapp, TAB_A[0], TAB_A[1], RIGHT_OF_C[0], RIGHT_OF_C[1], W, H, steps=8)
      print(f"SIG={self.dock.layoutSignature()}", flush=True)
      print(f"VALID={self.dock.validateTree()}", flush=True)
    elif self.mode == "direct" and rel == 3:
      print(f"SIG={self.dock.layoutSignature()}", flush=True)
      print(f"VALID={self.dock.validateTree()}", flush=True)

  def onGpuPostFrame(self, ctx):
    if self.captured:
      return
    if not self.ready:
      return
    rel = self.frame - self.ready_counter
    if rel < 12:
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
      Image.fromarray(arr[..., :3]).save(f"/tmp/tabdragout_{self.mode}.png")
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
    for k in ("SIG", "VALID"):
      if line.startswith(k + "="):
        d[k] = line[len(k) + 1:]
  return d

def _pngeq(a, b):
  import numpy
  from PIL import Image
  ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
  ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
  if ia.shape != ib.shape:
    return False, -1, -1
  d = numpy.abs(ia - ib)
  return int(d.max()) == 0, int((d.max(axis=2) > 0).sum()), int(d.max())

def orchestrate():
  self_path = os.path.abspath(__file__)
  def run(mode, tag):
    r = subprocess.run([self_path, "--mode", mode], capture_output=True, text=True)
    sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
    os.replace(f"/tmp/tabdragout_{mode}.png", f"/tmp/tabdragout_{tag}.png")
    return r.returncode, _parse(r.stdout)

  rc_i1, i1 = run("injected", "inj1")
  rc_i2, i2 = run("injected", "inj2")
  rc_d,  d  = run("direct",   "direct")

  problems = []
  for tag, rc in (("inj1", rc_i1), ("inj2", rc_i2), ("direct", rc_d)):
    if rc != 0: problems.append(f"{tag} run failed (rc={rc})")
  if i1.get("VALID") != "True": problems.append("injected validateTree not clean")
  if i1.get("SIG") != d.get("SIG"): problems.append(f"injected SIG != direct SIG ({i1.get('SIG')} vs {d.get('SIG')})")

  eq_det, diff_det, mx_det = _pngeq("/tmp/tabdragout_inj1.png", "/tmp/tabdragout_inj2.png")
  print(f"[determinism inj1 vs inj2] byte_equal={eq_det} differing={diff_det} maxdiff={mx_det}", flush=True)
  if not eq_det: problems.append(f"injected not deterministic (differing={diff_det})")

  eq, diff, mx = _pngeq("/tmp/tabdragout_inj1.png", "/tmp/tabdragout_direct.png")
  print(f"[injected vs direct] byte_equal={eq} differing={diff} maxdiff={mx}", flush=True)
  if not eq: problems.append(f"injected != direct moveChild (differing={diff} maxdiff={mx})")

  if problems:
    print("=== S3 tab drag-out gate FAILED ===", flush=True)
    for p in problems: print("  - " + p, flush=True)
    sys.exit(1)
  print("=== S3 tab drag-out gate PASSED ===", flush=True)
  sys.exit(0)

################################################################################

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["injected", "direct"], default=None)
  args = ap.parse_args()
  if args.mode:
    run_capture(args.mode)
  orchestrate()

if __name__ == "__main__":
  main()
