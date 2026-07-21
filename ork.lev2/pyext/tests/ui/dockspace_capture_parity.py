#!/usr/bin/env ork.python
################################################################################
# S0 gate: DockSpace 2-panel capture parity vs the current DockablePanel+lg.split
#  idiom.
#
#  Builds the same 2-panel layout two ways, captures each offscreen to a PNG
#  (the code_view.py --offscreen --capture idiom: OrkEzApp offscreen +
#  FBI.captureAsFormat), and asserts:
#    - pixel parity (byte-equal, else a tight diff bound — reported),
#    - validateTree() clean for both constructions,
#    - layoutSignature() equality between the two dock trees.
#
#  Modes (each mode is its own process so each ezapp gets a clean GPU context):
#    --mode baseline  --capture P   (DockablePanel + lg.split)
#    --mode dockspace --capture P   (DockSpace.addPanel / .split)
#    --compare A B                  (pixel diff, no GPU)
#  No args = orchestrate all three + verdict.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

W, H = 800, 600
COL_BG    = vec4(0.10, 0.10, 0.12, 1)
TB_L      = vec4(0.18, 0.16, 0.22, 1)
TB_R      = vec4(0.16, 0.20, 0.24, 1)
BOX_L     = vec4(0.30, 0.10, 0.10, 1)
BOX_R     = vec4(0.10, 0.10, 0.30, 1)

################################################################################

class App:
  def __init__(self, mode, capture):
    self.mode = mode
    self.capture = capture
    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H, offscreen=True)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = COL_BG

    if mode == "baseline":
      left_item = lg.makeChild(fill=True, margin=2,
                               uiclass=lev2.ui.DockablePanel, args=["left"])
      left = left_item.widget
      left.title_override = "LEFT"
      left.titlebar_color = TB_L
      left.createChild(uiclass=lev2.ui.Box, args=["boxL", BOX_L])
      right_item = lg.split(layout=left_item.layout, proportion=0.5,
                            placement=tokens.RIGHT, margin=2,
                            uiclass=lev2.ui.DockablePanel, args=["right"])
      right = right_item.widget
      right.title_override = "RIGHT"
      right.titlebar_color = TB_R
      right.createChild(uiclass=lev2.ui.Box, args=["boxR", BOX_R])
      self._sig_root = lg
    else:
      # DockSpace is a full-bleed (margin 0) transparent container; panel insets
      # come from the split margin, exactly as the baseline split does.
      dock = lg.makeChild(fill=True, margin=0,
                          uiclass=lev2.ui.DockSpace, args=["dock"]).widget
      dock.clear = False
      pL = dock.addPanel(uiclass=lev2.ui.Box, args=["boxL", BOX_L], title="LEFT")
      pL.titlebar_color = TB_L
      pR = dock.split(target=pL, placement=tokens.RIGHT, proportion=0.5, margin=2,
                      uiclass=lev2.ui.Box, args=["boxR", BOX_R], title="RIGHT")
      pR.titlebar_color = TB_R
      self._sig_root = dock
      self._lg = lg

    print(f"SIG={self._sig_root.layoutSignature()}", flush=True)
    print(f"VALID_ROOT={self._sig_root.validateTree()}", flush=True)
    print(f"VALID_LG={lg.validateTree()}", flush=True)

    self.frame = 0
    self.captured = False
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updinfo):
    self.frame += 1

  def onGpuPostFrame(self, ctx):
    if self.captured:
      return
    if self.frame >= 8 and not self._cap_inflight:
      rtg = ctx.FBI.main_RTG
      self._cap_buf = lev2.CaptureBuffer()
      self._cap_async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap_buf, "RGBA8")
      self._cap_inflight = True
    elif self._cap_inflight:
      if self._cap_async is None or bool(self._cap_async.is_ready):
        self._finish()

  def _finish(self):
    import numpy
    capbuf = self._cap_buf
    w, h = capbuf.width, capbuf.height
    arr = numpy.array(capbuf, dtype=numpy.uint8).reshape(h, w, 4)
    self.captured = True
    try:
      from PIL import Image
      Image.fromarray(arr[..., :3]).save(self.capture)
      print(f"WROTE {self.capture} ({w}x{h})", flush=True)
    except Exception as e:
      print(f"png save failed: {e}", flush=True)
    self.ezapp.signalExit()

################################################################################

def run_capture(mode, capture):
  app = App(mode, capture)
  app.ezapp.mainThreadLoop()
  sys.exit(0)

################################################################################

def compare(a, b):
  import numpy
  from PIL import Image
  ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
  ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
  if ia.shape != ib.shape:
    print(f"COMPARE shape mismatch {ia.shape} vs {ib.shape}", flush=True)
    return False
  diff = numpy.abs(ia - ib)
  total = ia.shape[0] * ia.shape[1]
  differing = int(numpy.count_nonzero(diff.max(axis=2) > 0))
  maxdiff = int(diff.max())
  pct = 100.0 * differing / total
  print(f"COMPARE pixels={total} differing={differing} ({pct:.4f}%) maxdiff={maxdiff}", flush=True)
  # byte-equal is the target; accept a very tight bound (<0.05% of pixels, small
  # magnitude) as parity given the extra DockSpace/TabWidget nesting layer.
  if differing == 0:
    print("COMPARE verdict=BYTE_EQUAL", flush=True)
    return True
  if pct < 0.05 and maxdiff <= 4:
    print("COMPARE verdict=TIGHT_BOUND", flush=True)
    return True
  print("COMPARE verdict=FAIL", flush=True)
  return False

################################################################################

def orchestrate():
  self_path = os.path.abspath(__file__)
  base_png = "/tmp/dockparity_baseline.png"
  dock_png = "/tmp/dockparity_dockspace.png"

  def run(mode, png):
    r = subprocess.run([self_path, "--mode", mode, "--capture", png],
                       capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    sys.stderr.write(r.stderr)
    sig = None
    valids = []
    for line in r.stdout.splitlines():
      if line.startswith("SIG="):
        sig = line[4:]
      if line.startswith("VALID_"):
        valids.append(line.split("=", 1)[1].strip())
    ok = (r.returncode == 0) and os.path.exists(png)
    all_valid = all(v == "True" for v in valids) and len(valids) > 0
    return ok, sig, all_valid

  ok_b, sig_b, valid_b = run("baseline", base_png)
  ok_d, sig_d, valid_d = run("dockspace", dock_png)

  problems = []
  if not ok_b:  problems.append("baseline capture failed")
  if not ok_d:  problems.append("dockspace capture failed")
  if not valid_b: problems.append("baseline validateTree not clean")
  if not valid_d: problems.append("dockspace validateTree not clean")
  if sig_b != sig_d:
    problems.append(f"layoutSignature mismatch:\n  baseline={sig_b}\n  dockspace={sig_d}")
  else:
    print(f"SIGNATURE EQUAL: {sig_b}", flush=True)

  pix_ok = compare(base_png, dock_png) if (ok_b and ok_d) else False
  if not pix_ok:
    problems.append("pixel parity failed")

  if problems:
    print("=== S0 DockSpace capture-parity gate FAILED ===", flush=True)
    for p in problems:
      print("  - " + p, flush=True)
    sys.exit(1)
  print("=== S0 DockSpace capture-parity gate PASSED ===", flush=True)
  sys.exit(0)

################################################################################

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["baseline", "dockspace"], default=None)
  ap.add_argument("--capture", default=None)
  ap.add_argument("--compare", nargs=2, default=None)
  args = ap.parse_args()

  if args.compare:
    sys.exit(0 if compare(args.compare[0], args.compare[1]) else 1)
  if args.mode:
    run_capture(args.mode, args.capture or f"/tmp/dockparity_{args.mode}.png")
  orchestrate()

if __name__ == "__main__":
  main()
