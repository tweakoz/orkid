#!/usr/bin/env ork.python
################################################################################
# S3 gate: in-bar tab REORDER via injected events (lockstep).
#
#  Single 2-tab leaf [A,B]. An injected drag that STAYS in the tab bar drags
#  tab A's header onto tab B's slot -> reorder to [B,A]. A reorder-back drag
#  returns to [A,B].
#
#    --mode baseline  : [A,B], capture.
#    --mode reorder   : drag A onto B's slot -> [B,A], capture.
#    --mode roundtrip : reorder then reorder-back -> [A,B], capture.
#  Orchestrator: reorder tab order changes + capture differs from baseline;
#  roundtrip order == baseline and capture BYTE-EQUAL to baseline.
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
from ork.ui import dock_layout

tokens = CrcStringProxy()
W, H = 800, 600
COLORS = {"A": vec4(0.60, 0.12, 0.12, 1), "B": vec4(0.12, 0.55, 0.15, 1)}

LEFT_TAB  = (14, 14)   # tab at index 0
RIGHT_TAB = (44, 14)   # tab at index 1  (short "A"/"B" labels ~30px each)

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
    self.dock.activatePanel(self.A)   # A active in every mode; only tab ORDER varies
    # leaf hosts [A,B] as a single 2-tab leaf
    self.frame = 0; self.ready = False; self.ready_counter = 0; self.driven = False
    self.captured = False; self._inflight = False; self._cap = None; self._buf = None

  def _order(self):
    t = self.dock.serializeLayout()
    return t["leaf"] if (t and "leaf" in t) else None

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updata):
    self.frame = int(updata.counter)
    if not self.ready:
      if self.A.width > 0:
        self.ready = True; self.ready_counter = self.frame
      return
    rel = self.frame - self.ready_counter
    if self.mode == "reorder" and rel == 3 and not self.driven:
      self.driven = True
      U.drag(self.ezapp, LEFT_TAB[0], LEFT_TAB[1], RIGHT_TAB[0], RIGHT_TAB[1], W, H, steps=6)
      print(f"ORDER={self._order()}", flush=True)
    elif self.mode == "roundtrip":
      if rel == 3:
        U.drag(self.ezapp, LEFT_TAB[0], LEFT_TAB[1], RIGHT_TAB[0], RIGHT_TAB[1], W, H, steps=6)
      elif rel == 6:
        U.drag(self.ezapp, RIGHT_TAB[0], RIGHT_TAB[1], LEFT_TAB[0], LEFT_TAB[1], W, H, steps=6)
        print(f"ORDER={self._order()}", flush=True)
    elif self.mode == "baseline" and rel == 3:
      print(f"ORDER={self._order()}", flush=True)

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
    Image.fromarray(arr[..., :3]).save(f"/tmp/tabreorder_{self.mode}.png")
    self.ezapp.signalExit()

################################################################################

def run_capture(mode):
  App(mode).ezapp.mainThreadLoop(); sys.exit(0)

def _parse(out):
  d = {}
  for line in out.splitlines():
    if line.startswith("ORDER="):
      d["ORDER"] = line[6:]
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
  for m in ("baseline", "reorder", "roundtrip"):
    r = subprocess.run([sp, "--mode", m], capture_output=True, text=True)
    sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
    res[m] = (r.returncode, _parse(r.stdout))
  problems = []
  for m, (rc, _) in res.items():
    if rc != 0: problems.append(f"{m} run failed")
  base_order = res["baseline"][1].get("ORDER")
  reorder_order = res["reorder"][1].get("ORDER")
  rt_order = res["roundtrip"][1].get("ORDER")
  print(f"orders: baseline={base_order} reorder={reorder_order} roundtrip={rt_order}", flush=True)
  if reorder_order == base_order:
    problems.append(f"reorder did not change tab order (still {base_order})")
  if rt_order != base_order:
    problems.append(f"roundtrip order {rt_order} != baseline {base_order}")
  eq_r, diff_r = _pngeq("/tmp/tabreorder_reorder.png", "/tmp/tabreorder_baseline.png")
  print(f"[reorder vs baseline] byte_equal={eq_r} differing={diff_r}", flush=True)
  if eq_r:
    problems.append("reorder capture identical to baseline (order should be visible)")
  eq_rt, diff_rt = _pngeq("/tmp/tabreorder_roundtrip.png", "/tmp/tabreorder_baseline.png")
  print(f"[roundtrip vs baseline] byte_equal={eq_rt} differing={diff_rt}", flush=True)
  if not eq_rt:
    problems.append(f"roundtrip capture not BYTE-EQUAL to baseline (differing={diff_rt})")
  if problems:
    print("=== S3 tab reorder gate FAILED ===", flush=True)
    for p in problems: print("  - " + p, flush=True)
    sys.exit(1)
  print("=== S3 tab reorder gate PASSED ===", flush=True)
  sys.exit(0)

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["baseline", "reorder", "roundtrip"], default=None)
  args = ap.parse_args()
  if args.mode:
    run_capture(args.mode)
  orchestrate()

if __name__ == "__main__":
  main()
