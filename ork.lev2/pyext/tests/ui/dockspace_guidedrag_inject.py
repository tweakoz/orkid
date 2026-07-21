#!/usr/bin/env ork.python
################################################################################
# W0 gate: DockSpace divider (split-guide) RESIZE via injected guide-drag.
#
#  A split divider must be grabbable + draggable whether it was created by a
#  DECLARED split (splitPanel, margin=2) or a RUNTIME dock (moveChild edge zone).
#  The runtime path historically baked a -1 margin -> a 0px grab band -> the
#  divider was un-resizable. This gate injects a real guide-drag (ork.uitest.drag
#  -> app.injectUiEvent, lockstep) onto each divider and asserts the proportion
#  moves — the RUNTIME mode is THE regression (it must FAIL pre-fix, PASS after).
#
#    --mode decl_drag   : declared A|B split, injected drag on the divider.
#    --mode decl_direct  : declared A|B split, setSplitProportion(--prop) — the
#                          byte-parity oracle the injected drag is checked against.
#    --mode runtime      : moveChild(B -> RIGHT of A) makes a runtime split, then
#                          injected drag on ITS divider. REGRESSION mode.
#    --mode clamp        : runtime split, injected drag far past the sibling —
#                          the 32px anti-crossing clamp must hold (no crossing).
#    --mode band         : declared split, injected PUSH 4px OFF the divider line
#                          (outside the 2px gap, inside the 6px band) — resizes only
#                          if DockSpace::doRouteUiEvent intercepts the guide.
#
#  Orchestrator: decl_drag proportion moved off 0.5 + byte-equal vs decl_direct;
#  decl_drag deterministic (two runs byte-equal); runtime proportion moved (the
#  gate); clamp proportion clamped (<1.0, no crossing); validateTree() clean all.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys, re, argparse, subprocess

ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2
import ork.uitest as U

tokens = CrcStringProxy()
W, H = 800, 600
RED  = vec4(0.55, 0.12, 0.12, 1)
BLUE = vec4(0.12, 0.16, 0.55, 1)

DRAG_DX  = 120     # 0.5 -> ~0.65 in an 800px container
CLAMP_DX = 2000    # far past the sibling -> must clamp below 1.0
BAND_OFFSET = 4    # PUSH this far OFF the divider line: outside the 2px visual gap,
                   # inside the 6px grab band -> only resizes if doRouteUiEvent intercepts

################################################################################

class App:
  def __init__(self, mode, run, prop, out):
    self.mode = mode
    self.run  = run
    self.arg_prop = prop
    self.out  = out
    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H,
                                      freerun=False, target_ups=60.0, target_fps=60.0)
    self.ezapp.topWidget.enableUiDraw()
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    self.dock = lg.makeChild(fill=True, margin=0, uiclass=lev2.ui.DockSpace, args=["dock"]).widget
    self.dock.clear = False

    # A|B split. DECLARED modes use splitPanel(margin=2); RUNTIME/CLAMP modes
    # build a single 2-tab leaf then moveChild B out to the RIGHT edge zone.
    self.A = self.dock.addPanel(uiclass=lev2.ui.Box, args=["A", RED], title="A"); self.A.name = "A"
    if mode in ("decl_drag", "decl_direct", "band"):
      self.B = self.dock.split(target=self.A, placement=tokens.RIGHT, proportion=0.5,
                               uiclass=lev2.ui.Box, args=["B", BLUE], title="B", margin=2)
      self.B.name = "B"
    else:
      self.B = self.dock.addPanel(uiclass=lev2.ui.Box, args=["B", BLUE], title="B"); self.B.name = "B"
      self.dock.moveChild(panel=self.B, to=self.A, zone=tokens.RIGHT)

    lg.setRect(0, 0, W, H)
    self.dock.updateLayout()

    self.frame = 0; self.ready = False; self.ready_counter = 0; self.driven = False
    self.prop_before = None
    self.captured = False; self._inflight = False; self._cap = None; self._buf = None

  # exact (full-precision) proportion of the single divider
  def _prop(self):
    t = self.dock.serializeLayout()
    if t and "proportion" in t:
      return float(t["proportion"])
    return None

  # root-space x of the vertical divider line (center of the visual gap)
  def _guide_x(self):
    ax, _ = self.A.localToRoot(0, 0)
    bx, _ = self.B.localToRoot(0, 0)
    pl, pr = (self.A, self.B) if ax <= bx else (self.B, self.A)
    lx, _ = pl.localToRoot(0, 0)
    rx, _ = pr.localToRoot(0, 0)
    return (lx + pl.width + rx) // 2

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updata):
    self.frame = int(updata.counter)
    if not self.ready:
      if self.A.width > 0:
        self.ready = True; self.ready_counter = self.frame
      return
    rel = self.frame - self.ready_counter
    if rel == 3 and not self.driven:
      self.driven = True
      self.prop_before = self._prop()
      gx = self._guide_x(); gy = H // 2
      if self.mode == "decl_direct":
        self.dock.setSplitProportion(self.A, self.B, float(self.arg_prop))
      elif self.mode == "clamp":
        U.drag(self.ezapp, gx, gy, gx + CLAMP_DX, gy, W, H, steps=8)
      elif self.mode == "band":
        # PUSH lands BAND_OFFSET px off the line (over panel content, not the gap)
        U.drag(self.ezapp, gx + BAND_OFFSET, gy, gx + BAND_OFFSET + DRAG_DX, gy, W, H, steps=6)
      else:  # decl_drag, runtime
        U.drag(self.ezapp, gx, gy, gx + DRAG_DX, gy, W, H, steps=6)
      print(f"GUIDE_X={gx}", flush=True)
      print(f"PROP_BEFORE={self.prop_before!r}", flush=True)
      print(f"PROP_AFTER={self._prop()!r}", flush=True)
      print(f"SIG={self.dock.layoutSignature()}", flush=True)
      print(f"VALID={self.dock.validateTree()}", flush=True)

  def onGpuPostFrame(self, ctx):
    if self.captured or not self.ready:
      return
    if (self.frame - self.ready_counter) < 14:
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
    Image.fromarray(arr[..., :3]).save(self.out)
    self.ezapp.signalExit()

################################################################################

def run_capture(args):
  App(args.mode, args.run, args.prop, args.out).ezapp.mainThreadLoop()
  sys.exit(0)

def _parse(out):
  d = {}
  for line in out.splitlines():
    for k in ("GUIDE_X", "PROP_BEFORE", "PROP_AFTER", "SIG", "VALID"):
      if line.startswith(k + "="):
        d[k] = line[len(k) + 1:]
  return d

def _sig_prop(sig):
  m = re.search(r"P:([0-9.]+)", sig or "")
  return m.group(1) if m else None

def _pngeq(a, b):
  import numpy
  from PIL import Image
  ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
  ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
  if ia.shape != ib.shape:
    return False, -1, -1
  d = numpy.abs(ia - ib)
  return (int(d.max()) == 0), int(numpy.count_nonzero(d.max(axis=2) > 0)), int(d.max())

def _run(mode, out, prop=None, run=0):
  sp = os.path.abspath(__file__)
  cmd = [sp, "--mode", mode, "--out", out, "--run", str(run)]
  if prop is not None:
    cmd += ["--prop", repr(float(prop))]
  r = subprocess.run(cmd, capture_output=True, text=True)
  sys.stdout.write(r.stdout); sys.stderr.write(r.stderr)
  return r.returncode, _parse(r.stdout)

def orchestrate():
  problems = []

  DD0 = "/tmp/dockresize_decl_drag0.png"
  DD1 = "/tmp/dockresize_decl_drag1.png"
  DIR = "/tmp/dockresize_decl_direct.png"
  RUN = "/tmp/dockresize_runtime.png"
  CLP = "/tmp/dockresize_clamp.png"

  ####################################
  # (a) DECLARED: injected drag moves the divider off 0.5
  ####################################
  rc, dd = _run("decl_drag", DD0, run=0)
  if rc != 0: problems.append("decl_drag run failed")
  if dd.get("VALID") != "True": problems.append("decl_drag: validateTree False")
  pa_decl = float(dd["PROP_AFTER"]) if dd.get("PROP_AFTER") not in (None, "None") else None
  print(f"[decl_drag] before={dd.get('PROP_BEFORE')} after={dd.get('PROP_AFTER')} sigP={_sig_prop(dd.get('SIG'))}", flush=True)
  if pa_decl is None or abs(pa_decl - 0.5) < 0.05:
    problems.append(f"decl_drag: divider did not move (after={pa_decl})")
  if _sig_prop(dd.get("SIG")) in (None, "0.500"):
    problems.append(f"decl_drag: layoutSignature proportion unchanged (P:{_sig_prop(dd.get('SIG'))})")

  ####################################
  # determinism: a second decl_drag is byte-equal
  ####################################
  rc2, dd2 = _run("decl_drag", DD1, run=1)
  if rc2 != 0: problems.append("decl_drag(rerun) failed")
  eqd, diffd, mxd = _pngeq(DD0, DD1)
  print(f"[decl_drag determinism] byte_equal={eqd} differing={diffd} maxdiff={mxd}", flush=True)
  if not eqd: problems.append(f"decl_drag not deterministic (differing={diffd} maxdiff={mxd})")

  ####################################
  # (a) byte-parity: injected drag == setSplitProportion(exact drag result)
  ####################################
  if pa_decl is not None:
    rcx, di = _run("decl_direct", DIR, prop=pa_decl, run=0)
    if rcx != 0: problems.append("decl_direct run failed")
    if di.get("VALID") != "True": problems.append("decl_direct: validateTree False")
    eqp, diffp, mxp = _pngeq(DD0, DIR)
    print(f"[decl_drag vs decl_direct] byte_equal={eqp} differing={diffp} maxdiff={mxp}", flush=True)
    if not eqp: problems.append(f"drag != setSplitProportion parity (differing={diffp} maxdiff={mxp})")
    if di.get("SIG") != dd.get("SIG"): problems.append("decl_direct SIG != decl_drag SIG")

  ####################################
  # (b) RUNTIME: THE regression — a drag-created split must be resizable
  ####################################
  rcr, rn = _run("runtime", RUN, run=0)
  if rcr != 0: problems.append("runtime run failed")
  if rn.get("VALID") != "True": problems.append("runtime: validateTree False")
  pb_rt = float(rn["PROP_BEFORE"]) if rn.get("PROP_BEFORE") not in (None, "None") else None
  pa_rt = float(rn["PROP_AFTER"]) if rn.get("PROP_AFTER") not in (None, "None") else None
  print(f"[runtime] before={pb_rt} after={pa_rt} sigP={_sig_prop(rn.get('SIG'))}", flush=True)
  if pa_rt is None or abs(pa_rt - 0.5) < 0.05:
    problems.append(f"RUNTIME split un-resizable: divider stayed at {pa_rt} (0px grab band regression)")

  ####################################
  # (c) CLAMP: drag far past the sibling — 32px anti-crossing must hold
  ####################################
  rcc, cl = _run("clamp", CLP, run=0)
  if rcc != 0: problems.append("clamp run failed")
  if cl.get("VALID") != "True": problems.append("clamp: validateTree False")
  pa_cl = float(cl["PROP_AFTER"]) if cl.get("PROP_AFTER") not in (None, "None") else None
  print(f"[clamp] after={pa_cl} sigP={_sig_prop(cl.get('SIG'))}", flush=True)
  if pa_cl is None or not (0.90 < pa_cl < 1.0):
    problems.append(f"clamp: divider not clamped below 1.0 (after={pa_cl}) — crossing not prevented")

  ####################################
  # (e) BAND: a PUSH 4px OFF the divider (outside the gap, inside the 6px band)
  #     must still grab + resize — proves the doRouteUiEvent guide-intercept.
  ####################################
  rcb, bd = _run("band", "/tmp/dockresize_band.png", run=0)
  if rcb != 0: problems.append("band run failed")
  if bd.get("VALID") != "True": problems.append("band: validateTree False")
  pa_bd = float(bd["PROP_AFTER"]) if bd.get("PROP_AFTER") not in (None, "None") else None
  print(f"[band] before={bd.get('PROP_BEFORE')} after={bd.get('PROP_AFTER')} sigP={_sig_prop(bd.get('SIG'))}", flush=True)
  if pa_bd is None or abs(pa_bd - 0.5) < 0.05:
    problems.append(f"BAND: off-line grab failed — divider stayed at {pa_bd} (no doRouteUiEvent intercept)")

  ####################################
  if problems:
    print("=== W0 dockspace guide-drag gate FAILED ===", flush=True)
    for p in problems: print("  - " + p, flush=True)
    sys.exit(1)
  print("=== W0 dockspace guide-drag gate PASSED ===", flush=True)
  sys.exit(0)

################################################################################

def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["decl_drag", "decl_direct", "runtime", "clamp", "band"], default=None)
  ap.add_argument("--out", default="/tmp/dockresize.png")
  ap.add_argument("--prop", default=None)
  ap.add_argument("--run", type=int, default=0)
  args = ap.parse_args()
  if args.mode:
    run_capture(args)
  orchestrate()

if __name__ == "__main__":
  main()
