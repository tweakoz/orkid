#!/usr/bin/env ork.python
################################################################################
# D1 gate: TerrainEditor default dock layout — DockSpace adoption reproduces the
#  PRE-ADOPTION construction (DockablePanel + lg.split) BYTE-FOR-BYTE.
#
#  The pre-adoption idiom (baseline) and the adopted DockSpace idiom (dockspace)
#  are each built at the terrain editor's EXACT default params (viewport fill;
#  left column split LEFT @0.35; property sheet split BOTTOM @0.55; per-panel
#  split margin 2; matching titlebar colors + titles). Panel BODIES are solid
#  Boxes (deterministic — isolates the layout, not the live viewport content).
#
#  Each mode is its own process (clean GPU context). Asserts pixel BYTE-EQUALITY,
#  layoutSignature equality, and validateTree clean for both.
#
#    --mode baseline  --capture P    (DockablePanel + lg.split — the prior idiom)
#    --mode dockspace --capture P    (DockSpace.addPanel / .split — adopted)
#  No args = orchestrate both + verdict.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2

# lock the parity gate to the editor's real default-layout params (single source)
from ork.editor.terrainedit import (
    _DOCK_VIEWPORT_TITLE, _DOCK_LEFT_TITLE, _DOCK_PROPS_TITLE,
    _DOCK_LEFT_PROP, _DOCK_PROPS_PROP, _DOCK_SPLIT_MARGIN)

tokens = CrcStringProxy()

W, H = 1000, 700
CBG   = vec4(0.13, 0.13, 0.15, 1)
TB_V  = vec4(0.15, 0.20, 0.25, 1)   # viewport titlebar
TB_L  = vec4(0.20, 0.15, 0.20, 1)   # left column titlebar
TB_P  = vec4(0.20, 0.20, 0.15, 1)   # property sheet titlebar
BOX_V = vec4(0.10, 0.10, 0.12, 1)
BOX_L = vec4(0.11, 0.13, 0.11, 1)
BOX_P = vec4(0.12, 0.12, 0.12, 1)

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
    lg.clearColorStd = CBG

    if mode == "baseline":
      vi = lg.makeChild(fill=True, margin=_DOCK_SPLIT_MARGIN,
                        uiclass=lev2.ui.DockablePanel, args=["viewport_dock"])
      vp = vi.widget; vp.titlebar_color = TB_V
      vp.createChild(uiclass=lev2.ui.Box, args=[_DOCK_VIEWPORT_TITLE, BOX_V])
      li = lg.split(layout=vi.layout, proportion=_DOCK_LEFT_PROP, placement=tokens.LEFT,
                    margin=_DOCK_SPLIT_MARGIN, uiclass=lev2.ui.DockablePanel, args=["left_dock"])
      lp = li.widget; lp.titlebar_color = TB_L
      lp.createChild(uiclass=lev2.ui.Box, args=[_DOCK_LEFT_TITLE, BOX_L])
      pi = lg.split(layout=li.layout, proportion=_DOCK_PROPS_PROP, placement=tokens.BOTTOM,
                    margin=_DOCK_SPLIT_MARGIN, uiclass=lev2.ui.DockablePanel, args=["propsheet_dock"])
      pp = pi.widget; pp.titlebar_color = TB_P
      pp.createChild(uiclass=lev2.ui.Box, args=[_DOCK_PROPS_TITLE, BOX_P])
      self._sig_root = lg
    else:
      dock = lg.makeChild(fill=True, margin=0,
                          uiclass=lev2.ui.DockSpace, args=["terrain_dock"]).widget
      dock.clear = False
      vp = dock.addPanel(uiclass=lev2.ui.Box, args=[_DOCK_VIEWPORT_TITLE, BOX_V],
                         title=_DOCK_VIEWPORT_TITLE); vp.titlebar_color = TB_V
      lp = dock.split(target=vp, placement=tokens.LEFT, proportion=_DOCK_LEFT_PROP,
                      margin=_DOCK_SPLIT_MARGIN, uiclass=lev2.ui.Box,
                      args=[_DOCK_LEFT_TITLE, BOX_L], title=_DOCK_LEFT_TITLE); lp.titlebar_color = TB_L
      pp = dock.split(target=lp, placement=tokens.BOTTOM, proportion=_DOCK_PROPS_PROP,
                      margin=_DOCK_SPLIT_MARGIN, uiclass=lev2.ui.Box,
                      args=[_DOCK_PROPS_TITLE, BOX_P], title=_DOCK_PROPS_TITLE); pp.titlebar_color = TB_P
      self._sig_root = dock

    print(f"SIG={self._sig_root.layoutSignature()}", flush=True)
    print(f"VALID={self._sig_root.validateTree()}", flush=True)
    print(f"VALID_LG={lg.validateTree()}", flush=True)

    self.frame = 0
    self._gpu_frames = 0
    self.captured = False
    self._inflight = False
    self._cap = None
    self._buf = None

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updinfo):
    self.frame += 1

  def onGpuPostFrame(self, ctx):
    if self.captured:
      return
    # settle text warm-up on the capture thread (see dockspace_persist_roundtrip #87)
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
    from PIL import Image
    Image.fromarray(arr[..., :3]).save(self.capture)
    print(f"WROTE {self.capture}", flush=True)
    self.ezapp.signalExit()

################################################################################

def run_capture(mode, capture):
  App(mode, capture).ezapp.mainThreadLoop()
  sys.exit(0)

def compare(a, b):
  import numpy
  from PIL import Image
  ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
  ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
  if ia.shape != ib.shape:
    print(f"COMPARE shape mismatch {ia.shape} vs {ib.shape}", flush=True)
    return False, -1
  diff = numpy.abs(ia - ib)
  differing = int(numpy.count_nonzero(diff.max(axis=2) > 0))
  maxd = int(diff.max())
  print(f"COMPARE differing={differing} maxdiff={maxd}", flush=True)
  return (differing == 0), differing

def orchestrate():
  sp = os.path.abspath(__file__)
  base_png = "/tmp/terrainedit_parity_baseline.png"
  dock_png = "/tmp/terrainedit_parity_dockspace.png"

  def run(mode, png):
    r = subprocess.run([sp, "--mode", mode, "--capture", png], capture_output=True, text=True)
    sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
    sig = None; valids = []
    for line in r.stdout.splitlines():
      if line.startswith("SIG="):
        sig = line[4:]
      if line.startswith("VALID"):
        valids.append(line.split("=", 1)[1].strip())
    ok = (r.returncode == 0) and os.path.exists(png)
    return ok, sig, all(v == "True" for v in valids) and len(valids) > 0

  ok_b, sig_b, valid_b = run("baseline", base_png)
  ok_d, sig_d, valid_d = run("dockspace", dock_png)

  problems = []
  if not ok_b: problems.append("baseline capture failed")
  if not ok_d: problems.append("dockspace capture failed")
  if not valid_b: problems.append("baseline validateTree not clean")
  if not valid_d: problems.append("dockspace validateTree not clean")
  if sig_b != sig_d:
    problems.append(f"layoutSignature mismatch:\n  baseline={sig_b}\n  dockspace={sig_d}")
  else:
    print(f"SIGNATURE EQUAL: {sig_b}", flush=True)

  if ok_b and ok_d:
    eq, differing = compare(base_png, dock_png)
    if not eq:
      problems.append(f"pixel parity not BYTE-EQUAL (differing={differing})")
  else:
    problems.append("skipped pixel compare (a capture failed)")

  if problems:
    print("=== TerrainEditor layout-parity gate FAILED ===", flush=True)
    for p in problems: print("  - " + p, flush=True)
    sys.exit(1)
  print("=== TerrainEditor layout-parity gate PASSED (byte-equal) ===", flush=True)
  sys.exit(0)

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["baseline", "dockspace"], default=None)
  ap.add_argument("--capture", default=None)
  args = ap.parse_args()
  if args.mode:
    run_capture(args.mode, args.capture or f"/tmp/terrainedit_parity_{args.mode}.png")
  orchestrate()

if __name__ == "__main__":
  main()
