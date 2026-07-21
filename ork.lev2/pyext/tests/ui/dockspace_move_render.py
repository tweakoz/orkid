#!/usr/bin/env ork.python
################################################################################
# S1 gate: a PrimCanvas moved between leaves must still RENDER in its new region.
#
#  Two offscreen captures (each its own process / clean GPU context):
#    --mode before : build A(red) | B(blue), capture.
#    --mode after  : build A(red) | B(blue), then moveChild(A -> RIGHT of B),
#                    capture.  Also prints layoutSignature + validateTree.
#  Default (no mode) runs both and asserts the pixel oracle:
#    before  -> left=A(red), right=B(blue)
#    after   -> left=B(blue), right=A(red)   (moved canvas renders in new region;
#               old region shows the remaining panel)
#    plus validateTree clean and layoutSignature unchanged (no stale Layout).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

RED  = vec4(0.30, 0.10, 0.10, 1)   # panel A
BLUE = vec4(0.10, 0.10, 0.30, 1)   # panel B

################################################################################

class App:
  def __init__(self, mode):
    self.mode = mode
    self.ezapp = lev2.OrkEzApp.create(self, width=800, height=600, offscreen=True)
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

    if mode == "after":
      # move the (live) canvas A to the RIGHT of B: source leaf collapses, a new
      # leaf split off B's region hosts A.
      self.dock.moveChild(panel=self.A, to=self.B, zone=tokens.RIGHT)

    print(f"SIG={self.dock.layoutSignature()}", flush=True)
    print(f"VALID={self.dock.validateTree()}", flush=True)

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
    lcol = [int(v) for v in arr[350, 150, :3]]
    rcol = [int(v) for v in arr[350, 650, :3]]
    print(f"LCOL={lcol}", flush=True)
    print(f"RCOL={rcol}", flush=True)
    self.ezapp.signalExit()

################################################################################

def run_capture(mode):
  App(mode).ezapp.mainThreadLoop()
  sys.exit(0)

def _parse(out):
  d = {}
  for line in out.splitlines():
    for key in ("SIG", "VALID", "LCOL", "RCOL"):
      if line.startswith(key + "="):
        d[key] = line[len(key) + 1:]
  return d

def orchestrate():
  self_path = os.path.abspath(__file__)
  def run(mode):
    r = subprocess.run([self_path, "--mode", mode], capture_output=True, text=True)
    sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
    return r.returncode, _parse(r.stdout)

  rc_b, b = run("before")
  rc_a, a = run("after")

  def is_red(s):  p = eval(s); return p[0] > p[2] + 10
  def is_blue(s): p = eval(s); return p[2] > p[0] + 10

  problems = []
  if rc_b != 0: problems.append("before run failed")
  if rc_a != 0: problems.append("after run failed")
  try:
    if not is_red(b["LCOL"]):  problems.append(f"before-left not A(red): {b.get('LCOL')}")
    if not is_blue(b["RCOL"]): problems.append(f"before-right not B(blue): {b.get('RCOL')}")
    if not is_blue(a["LCOL"]): problems.append(f"after-left not B(blue): {a.get('LCOL')}")
    if not is_red(a["RCOL"]):  problems.append(f"after-right not moved-A(red): {a.get('RCOL')}")
  except Exception as ex:
    problems.append(f"missing capture output: {ex}")
  if a.get("VALID") != "True": problems.append(f"after validateTree not clean: {a.get('VALID')}")
  if a.get("SIG") != b.get("SIG"):
    problems.append(f"layoutSignature changed (stale node?): {b.get('SIG')} -> {a.get('SIG')}")

  if problems:
    print("=== S1 moveChild render-oracle gate FAILED ===", flush=True)
    for p in problems: print("  - " + p, flush=True)
    sys.exit(1)
  print("=== S1 moveChild render-oracle gate PASSED ===", flush=True)
  sys.exit(0)

################################################################################

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["before", "after"], default=None)
  args = ap.parse_args()
  if args.mode:
    run_capture(args.mode)
  orchestrate()

if __name__ == "__main__":
  main()
