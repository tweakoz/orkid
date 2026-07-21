#!/usr/bin/env ork.python
################################################################################
# S3 gate: injected drag CANCEL (lockstep).
#
#  A 2-tab leaf [A,B] (A active). A scripted drag grabs tab A's header, drags
#  it OUT of the bar, then releases over the leaf's own CENTER (a self-drop =
#  no valid target). END_DRAG cancels: no move, hint dismissed.
#
#    --mode baseline : [A,B] (A active), capture.
#    --mode cancel   : drag tab A out and drop on own CENTER, capture.
#  Orchestrator: cancel capture BYTE-EQUAL to baseline; signature unchanged;
#  no overlay left on the stack.
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

TAB_A     = (14, 14)     # tab A header
OWN_CENTER = (400, 300)  # center of the single leaf -> self-drop CENTER (no-op)

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
    self.A = self.dock.addPanel(uiclass=lev2.ui.Box, args=["A", COLORS["A"]], title="A"); self.A.name = "A"
    self.B = self.dock.addPanel(uiclass=lev2.ui.Box, args=["B", COLORS["B"]], title="B"); self.B.name = "B"
    self.dock.activatePanel(self.A)   # A active in both modes (PUSH won't change it)
    self.sig0 = self.dock.layoutSignature()
    self.frame = 0; self.ready = False; self.ready_counter = 0; self.driven = False
    self.captured = False; self._inflight = False; self._cap = None; self._buf = None

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updata):
    self.frame = int(updata.counter)
    if not self.ready:
      if self.A.width > 0:
        self.ready = True; self.ready_counter = self.frame
      return
    rel = self.frame - self.ready_counter
    if self.mode == "cancel" and rel == 3 and not self.driven:
      self.driven = True
      U.drag(self.ezapp, TAB_A[0], TAB_A[1], OWN_CENTER[0], OWN_CENTER[1], W, H, steps=8)
      print(f"SIG={self.dock.layoutSignature()}", flush=True)
      print(f"SIG_UNCHANGED={self.dock.layoutSignature() == self.sig0}", flush=True)
      print(f"HAS_OVERLAY={self.ezapp.uicontext.hasOverlays()}", flush=True)
      print(f"VALID={self.dock.validateTree()}", flush=True)
    elif self.mode == "baseline" and rel == 3:
      print(f"SIG={self.dock.layoutSignature()}", flush=True)

  def onGpuPostFrame(self, ctx):
    if self.captured or not self.ready:
      return
    rel = self.frame - self.ready_counter
    if rel < 14:
      return
    if not self._inflight:
      self._buf = lev2.CaptureBuffer()
      self._cap = ctx.FBI.captureAsFormat(ctx.FBI.main_RTG.buffer(0), self._buf, "RGBA8")
      self._inflight = True
      return
    if self._cap is not None and not bool(self._cap.is_ready):
      return
    import numpy
    arr = numpy.array(self._buf, dtype=numpy.uint8).reshape(self._buf.height, self._buf.width, 4)
    self.captured = True
    from PIL import Image
    Image.fromarray(arr[..., :3]).save(f"/tmp/dragcancel_{self.mode}.png")
    self.ezapp.signalExit()

################################################################################

def run_capture(mode):
  App(mode).ezapp.mainThreadLoop(); sys.exit(0)

def _parse(out):
  d = {}
  for line in out.splitlines():
    for k in ("SIG", "SIG_UNCHANGED", "HAS_OVERLAY", "VALID"):
      if line.startswith(k + "="):
        d[k] = line[len(k) + 1:]
  return d

def _pngeq(a, b):
  import numpy
  from PIL import Image
  ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
  ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
  d = numpy.abs(ia - ib)
  return int(d.max()) == 0, int((d.max(axis=2) > 0).sum())

def orchestrate():
  sp = os.path.abspath(__file__)
  res = {}
  for m in ("baseline", "cancel"):
    r = subprocess.run([sp, "--mode", m], capture_output=True, text=True)
    sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
    res[m] = (r.returncode, _parse(r.stdout))
  problems = []
  for m, (rc, _) in res.items():
    if rc != 0: problems.append(f"{m} run failed")
  c = res["cancel"][1]
  if c.get("SIG_UNCHANGED") != "True": problems.append("cancel changed the layout signature")
  if c.get("HAS_OVERLAY") != "False": problems.append("cancel left an overlay on the stack")
  if c.get("VALID") != "True": problems.append("cancel validateTree not clean")
  eq, diff = _pngeq("/tmp/dragcancel_cancel.png", "/tmp/dragcancel_baseline.png")
  print(f"[cancel vs baseline] byte_equal={eq} differing={diff}", flush=True)
  if not eq: problems.append(f"cancel capture not BYTE-EQUAL to baseline (differing={diff})")
  if problems:
    print("=== S3 drag-cancel gate FAILED ===", flush=True)
    for p in problems: print("  - " + p, flush=True)
    sys.exit(1)
  print("=== S3 drag-cancel gate PASSED ===", flush=True)
  sys.exit(0)

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["baseline", "cancel"], default=None)
  args = ap.parse_args()
  if args.mode:
    run_capture(args.mode)
  orchestrate()

if __name__ == "__main__":
  main()
