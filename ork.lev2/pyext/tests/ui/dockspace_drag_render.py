#!/usr/bin/env ork.python
################################################################################
# S2 gate: titlebar drag choreography (hint overlay + END_DRAG -> moveChild).
#
#  Each mode is its own offscreen process (clean GPU). The drag session is
#  driven directly (beginPanelDrag / updatePanelDrag / endPanelDrag — the raw
#  ui.Event type is read-only, so this is the established precedent) at a SAFE
#  point (in __init__, before the render threads start), after forcing a layout
#  so the zone hit-test has real leaf geometry.
#
#    --mode baseline : A(red) | B(blue), no drag.
#    --mode direct   : moveChild(A -> RIGHT of B).
#    --mode drag     : beginPanelDrag(A) -> updatePanelDrag(RIGHT of B) ->
#                      endPanelDrag(RIGHT of B).
#    --mode hint     : beginPanelDrag(A) -> updatePanelDrag(RIGHT of B), left
#                      ACTIVE (no end) so the hint overlay is captured.
#    --mode cancel   : beginPanelDrag(A) -> update/end on A's OWN leaf CENTER
#                      (self-drop, no valid target) -> no move, hint gone.
#
#  Orchestrator asserts: hint pixels visible over the target region (mid-drag);
#  drag final capture BYTE-EQUAL to direct moveChild; cancel == baseline (no
#  move, overlay gone) + signature unchanged.
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
RED  = vec4(0.30, 0.10, 0.10, 1)
BLUE = vec4(0.10, 0.10, 0.30, 1)

# probe points (root space): B's leaf is [400..800]; its RIGHT band is x>700.
# A's own leaf is [0..400]; (200,300) is its CENTER (self-drop -> no valid target).
RIGHT_OF_B = (750, 300)
CENTER_OF_A = (200, 300)

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
    self.A = self.dock.addPanel(uiclass=lev2.ui.PrimCanvas, args=["cvA"], title="A")
    self.B = self.dock.split(target=self.A, placement=tokens.RIGHT, proportion=0.5,
                             uiclass=lev2.ui.PrimCanvas, args=["cvB"], title="B")
    self.canvasA = self.A.child
    self.canvasB = self.B.child
    self._colors = ((self.canvasA, RED), (self.canvasB, BLUE))

    # force a layout so the zone hit-test sees real leaf geometry
    lg.setRect(0, 0, W, H)
    self.dock.updateLayout()

    if mode == "direct":
      self.dock.moveChild(panel=self.A, to=self.B, zone=tokens.RIGHT)
    elif mode == "drag":
      self.dock.beginPanelDrag(self.A)
      self.dock.updatePanelDrag(*RIGHT_OF_B)
      self.dock.endPanelDrag(*RIGHT_OF_B)
    elif mode == "hint":
      self.dock.beginPanelDrag(self.A)
      self.dock.updatePanelDrag(*RIGHT_OF_B)   # left active — hint stays up
    elif mode == "cancel":
      self.dock.beginPanelDrag(self.A)
      self.dock.updatePanelDrag(*CENTER_OF_A)  # self-drop -> no valid target
      self.dock.endPanelDrag(*CENTER_OF_A)

    print(f"SIG={self.dock.layoutSignature()}", flush=True)
    print(f"VALID={self.dock.validateTree()}", flush=True)
    print(f"DRAG_ACTIVE={self.dock.drag_active}", flush=True)
    print(f"HAS_OVERLAY={self.ezapp.uicontext.hasOverlays()}", flush=True)

    self.frame = 0
    self.captured = False
    self._inflight = False
    self._cap = None
    self._buf = None

  def onGpuInit(self, ctx):
    for c, col in self._colors:
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
    self.canvasA.markDirty()
    self.canvasB.markDirty()

  def onGpuPostFrame(self, ctx):
    if self.captured:
      return
    if self.frame < 10:
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
    out = f"/tmp/dockdrag_{self.mode}.png"
    try:
      from PIL import Image
      Image.fromarray(arr[..., :3]).save(out)
    except Exception as e:
      print(f"png save failed: {e}", flush=True)
    # region samples: B-nonhint (500,300), B-right-hint (700,300)
    print(f"BNH={[int(v) for v in arr[300,500,:3]]}", flush=True)   # B, no hint
    print(f"HINT={[int(v) for v in arr[300,700,:3]]}", flush=True)  # B right = hint region
    self.ezapp.signalExit()

################################################################################

def run_capture(mode):
  App(mode).ezapp.mainThreadLoop()
  sys.exit(0)

def _parse(out):
  d = {}
  for line in out.splitlines():
    for k in ("SIG", "VALID", "DRAG_ACTIVE", "HAS_OVERLAY", "BNH", "HINT"):
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
  return (int(d.max()) == 0), int(numpy.count_nonzero(d.max(axis=2) > 0)), int(d.max())

def orchestrate():
  self_path = os.path.abspath(__file__)
  res = {}
  for mode in ("baseline", "direct", "drag", "hint", "cancel"):
    r = subprocess.run([self_path, "--mode", mode], capture_output=True, text=True)
    sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
    res[mode] = (r.returncode, _parse(r.stdout))

  problems = []
  for m, (rc, _) in res.items():
    if rc != 0: problems.append(f"{m} run failed (rc={rc})")

  # mid-drag hint visible over the target (right) region of B
  h = res["hint"][1]
  try:
    bnh = eval(h["BNH"]); hint = eval(h["HINT"])
    # hint (cyan a=0.4) over B(blue) lifts green+blue vs the non-hint B region
    if not (hint[1] > bnh[1] + 15 and hint[2] > bnh[2] + 10):
      problems.append(f"hint not visible over target region: BNH={bnh} HINT={hint}")
  except Exception as ex:
    problems.append(f"hint capture parse failed: {ex}")
  if res["hint"][1].get("DRAG_ACTIVE") != "True":
    problems.append("hint mode: drag not active")
  if res["hint"][1].get("HAS_OVERLAY") != "True":
    problems.append("hint mode: overlay not present")

  # drag final == direct moveChild (byte-equal)
  eq, diff, mx = _pngeq("/tmp/dockdrag_drag.png", "/tmp/dockdrag_direct.png")
  print(f"[drag vs direct] byte_equal={eq} differing={diff} maxdiff={mx}", flush=True)
  if not eq: problems.append(f"drag != direct moveChild (differing={diff} maxdiff={mx})")
  if res["drag"][1].get("SIG") != res["direct"][1].get("SIG"):
    problems.append("drag SIG != direct SIG")
  if res["drag"][1].get("DRAG_ACTIVE") != "False" or res["drag"][1].get("HAS_OVERLAY") != "False":
    problems.append("drag mode: overlay/drag not cleared after END_DRAG")

  # cancel == baseline (no move), signature unchanged, overlay gone
  eqc, diffc, mxc = _pngeq("/tmp/dockdrag_cancel.png", "/tmp/dockdrag_baseline.png")
  print(f"[cancel vs baseline] byte_equal={eqc} differing={diffc} maxdiff={mxc}", flush=True)
  if not eqc: problems.append(f"cancel != baseline (differing={diffc} maxdiff={mxc})")
  if res["cancel"][1].get("SIG") != res["baseline"][1].get("SIG"):
    problems.append("cancel SIG changed")
  if res["cancel"][1].get("HAS_OVERLAY") != "False":
    problems.append("cancel mode: overlay not dismissed")

  if problems:
    print("=== S2 drag-choreography gate FAILED ===", flush=True)
    for p in problems: print("  - " + p, flush=True)
    sys.exit(1)
  print("=== S2 drag-choreography gate PASSED ===", flush=True)
  sys.exit(0)

################################################################################

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["baseline", "direct", "drag", "hint", "cancel"], default=None)
  args = ap.parse_args()
  if args.mode:
    run_capture(args.mode)
  orchestrate()

if __name__ == "__main__":
  main()
