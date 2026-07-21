#!/usr/bin/env ork.python
################################################################################
# connect_ux_gate — drag-to-node connection UX gate for the generic node editor
# (ork.ui.node_editor.NodeEditor). Offscreen + lockstep, END-TO-END INJECTED via
# app.injectUiEvent + ork.uitest (the harness this UX ships with).
#
# The new UX (remove-don't-deprecate; the old plug-to-plug drop is GONE):
#   * drag starts from an OUTPUT plug; release over a NODE BODY connects.
#   * one qualifying input  -> connect immediately (single-candidate fast path).
#   * several qualifying     -> a DropdownMenu of input plugs at the cursor; a
#                               connected input is marked "(replaces <src>.<plug>)".
#   * zero qualifying / empty canvas -> clean cancel (no menu, no edge).
#   * unconnected input plugs are no longer drawn (nor hit-tested); connected
#     inputs + all outputs still draw.
#
# Sub-gates (each is a FRESH offscreen process => a clean GPU context):
#   baseline     : settle + capture, no injection (parity reference for cancels).
#   menu         : drag GLOB.RelTime -> GRAV (4 qualifying floats) opens a menu ->
#                  hasOverlays; click entry 1 (Mass) -> edge GLOB.RelTime->GRAV.Mass
#                  exists, menu dismissed. Two runs -> byte-identical final capture.
#   single       : drag POOL.pool -> TURB (exactly one 'pool' input) connects with
#                  NO menu (hasOverlays False) + edge exists.
#   zero         : drag GLOB.RelTime -> TURB (no float input) -> no edge, no overlay,
#                  capture == baseline.
#   empty        : drag GLOB.RelTime -> empty canvas -> no edge, no overlay,
#                  capture == baseline.
#   replace      : pre-wire GLOB.RelTime->GRAV.G; drag GLOB2.RelTime -> GRAV, select
#                  the "(replaces GLOB.RelTime)" entry -> edge is now
#                  GLOB2.RelTime->GRAV.G and the old edge is gone.
#   render       : a graph with connected + unconnected inputs; count "lit" pixels
#                  just above each input anchor -> unconnected inputs draw NO dot,
#                  connected inputs still draw theirs.
#
#   run:  ork.python ork.lev2/pyext/tests/ui/connect_ux_gate.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

_HERE = os.path.dirname(os.path.abspath(__file__))
_WORKTREE = os.path.abspath(os.path.join(_HERE, "..", "..", "..", ".."))
_SCRIPTS = os.path.join(_WORKTREE, "obt.project", "scripts")
if _SCRIPTS not in sys.path:
  sys.path.insert(0, _SCRIPTS)

W, H = 1000, 800

# node positions (graph space, top-left). Well separated so a body-center hit-test
# and an output-plug hit are both unambiguous.
POS = {
    "GLOB":  (100.0, 120.0),
    "GLOB2": (100.0, 460.0),
    "GRAV":  (450.0, 290.0),
    "TURB":  (800.0, 120.0),
    "POOL":  (800.0, 460.0),
}


################################################################################
# model construction (in-code particles GraphData wrapped in the shell's document +
# node-model — the real can_connect / connect / disconnect + plug schema).
################################################################################

def _build_model(prewire=None):
  from orkengine import lev2
  from orkengine.core import dataflow as _dflow
  from ork.hypergraph.dflow.graphdata_document import GraphDataDocument
  from ork.editor.graphdata_node_model import GraphDataNodeGraphModel
  P = lev2.particles
  g = _dflow.GraphData.createShared()
  g.create("GLOB",  P.Globals)
  g.create("GLOB2", P.Globals)
  g.create("GRAV",  P.Gravity)
  g.create("TURB",  P.Turbulence)
  g.create("POOL",  P.Pool)
  doc = GraphDataDocument(g)
  model = GraphDataNodeGraphModel(doc)
  for nid, (x, y) in POS.items():
    model.set_pos(nid, x, y)
  for (si, sp, di, dp) in (prewire or []):
    model.connect(si, sp, di, dp)
  return model


def _load_node_editor():
  """The NodeEditor class under test."""
  from ork.ui.node_editor import NodeEditor, COL_BG
  return NodeEditor, COL_BG


################################################################################
# offscreen leaf app
################################################################################

class ConnectUxLeaf:
  def __init__(self, mode, capture):
    from orkengine.core import vec4, CrcStringProxy
    from orkengine import lev2
    self.lev2 = lev2
    self.vec4 = vec4
    self.tokens = CrcStringProxy()
    self.mode = mode
    self.capture = capture
    NodeEditor, COL_BG = _load_node_editor()
    self.COL_BG = COL_BG

    prewire = None
    self._NodeEditor = NodeEditor
    self._prewire = None
    if mode == "replace":
      self._prewire = [("GLOB", "RelTime", "GRAV", "G")]
    elif mode == "render":
      self._prewire = [("GLOB", "pool", "GRAV", "pool"), ("GLOB", "RelTime", "GRAV", "G")]
    # the model (an in-code particles GraphData) is built in onGpuInit — the reflection
    # substrate a particle module needs is only registered during app init (dflowedit builds
    # its family graphs in _onGpuInit for the same reason; a pre-init build silently loses them).
    self.model = None
    self.ne = None

    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H,
                                      freerun=False, target_ups=60.0, target_fps=60.0)
    self.ezapp.topWidget.enableUiDraw()
    lg = self.ezapp.topLayoutGroup
    lg.margin = 0
    lg.clearColorStd = COL_BG
    item = lg.makeChild(fill=True, uiclass=lev2.ui.PrimCanvas, args=["necanvas"])
    self.canvas = item.widget
    self.canvas.bg_color = COL_BG
    self.canvas.draw_background = True

    self.ready = False
    self.ready_counter = 0
    self.stage = 0
    self._sf = 0
    self.results = {}
    self.overlay_at_menu = None
    self.overlay_after = None
    self._cap_pending = False
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None
    self._cap_rgb = None
    self._done = False

  def onGpuInit(self, ctx):
    self.model = _build_model(self._prewire)
    self.ne = self._NodeEditor(self.canvas, self.model, title="cux", orientation="vertical")
    self.ne.uicontext = self.ezapp.uicontext
    self.ne.gpuInit(ctx)

  # ---- geometry helpers (root pixel coords) --------------------------------

  def _anchor_root(self, gx, gy):
    lx, ly = self.ne.view.graph_to_screen(gx, gy)
    return (int(round(self.canvas.x + lx)), int(round(self.canvas.y + ly)))

  def _out_root(self, nid, out_plug):
    outs = self.model.outputs(nid)
    idx = next(k for k, (pn, _t) in enumerate(outs) if pn == out_plug)
    info = self.ne._plug_info(nid, "out", idx)
    return self._anchor_root(info[2][0], info[2][1])

  def _body_root(self, nid):
    _k, x, y, w, h = self.ne._node_geom(self.model, nid)
    return self._anchor_root(x + w * 0.5, y + h * 0.5)

  def _edges(self):
    return set(tuple(e) for e in self.model.edges())

  def _has_overlays(self):
    ctx = self.ezapp.uicontext
    return bool(ctx.hasOverlays()) if ctx is not None else False

  # ---- update loop ---------------------------------------------------------

  def onUpdate(self, updata):
    import ork.uitest as U
    counter = int(updata.counter)
    ctx = self.ezapp.uicontext
    if ctx is not None:
      ctx.virtual_time_enabled = True
      ctx.virtual_time = counter * 0.1
    if not self.ready:
      if self.ne is not None and self.canvas.width > 0:
        self.ready = True
        self.ready_counter = counter
      elif counter > 600:
        print("FATAL never ready", flush=True)
        self.ezapp.signalExit()
      return
    if self._done:
      return
    rel = counter - self.ready_counter
    if rel < 8:
      return
    getattr(self, "_tick_" + self.mode.replace("-", "_"))(U, rel)

  # ---- per-mode ticks ------------------------------------------------------

  def _tick_baseline(self, U, rel):
    self._cap_then(lambda: self._finish(True, {}))

  def _tick_render(self, U, rel):
    self._cap_then(self._render_verdict)

  def _tick_menu(self, U, rel):
    if self.stage == 0:
      self.before = self._edges()
      sx, sy = self._out_root("GLOB", "RelTime")
      tx, ty = self._body_root("GRAV")
      U.drag(self.ezapp, sx, sy, tx, ty, W, H, steps=8)
      self.overlay_at_menu = self._has_overlays()
      self.menu_root = self.ne._last_root
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 4:
      # click menu item index 1 (Mass): PUSH selects at (miY - y())/ITEM_HEIGHT.
      rx, ry = self.menu_root
      U.click(self.ezapp, rx + 6, ry + 1 * 28 + 14, W, H)
      self.overlay_after = self._has_overlays()
      self.after = self._edges()
      self.stage = 2
      self._sf = rel
    elif self.stage == 2 and rel - self._sf >= 4:
      new = self.after - self.before
      self._cap_then(lambda: self._finish(
          ("GLOB", "RelTime", "GRAV", "Mass") in self.after
          and self.overlay_at_menu and not self.overlay_after,
          {"MENU_OPENED": self.overlay_at_menu, "MENU_DISMISSED": not self.overlay_after,
           "NEW_EDGES": sorted(new), "EDGE_OK": ("GLOB", "RelTime", "GRAV", "Mass") in self.after}))

  def _tick_single(self, U, rel):
    if self.stage == 0:
      self.before = self._edges()
      sx, sy = self._out_root("POOL", "pool")
      tx, ty = self._body_root("TURB")
      U.drag(self.ezapp, sx, sy, tx, ty, W, H, steps=8)
      self.overlay_after = self._has_overlays()
      self.after = self._edges()
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 4:
      new = self.after - self.before
      self._cap_then(lambda: self._finish(
          ("POOL", "pool", "TURB", "pool") in self.after and not self.overlay_after,
          {"NO_MENU": not self.overlay_after, "NEW_EDGES": sorted(new),
           "EDGE_OK": ("POOL", "pool", "TURB", "pool") in self.after}))

  def _tick_zero(self, U, rel):
    if self.stage == 0:
      self.before = self._edges()
      sx, sy = self._out_root("GLOB", "RelTime")
      tx, ty = self._body_root("TURB")
      U.drag(self.ezapp, sx, sy, tx, ty, W, H, steps=8)
      self.overlay_after = self._has_overlays()
      self.after = self._edges()
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 4:
      self._cap_then(lambda: self._finish(
          self.after == self.before and not self.overlay_after,
          {"NO_MENU": not self.overlay_after, "NO_EDIT": self.after == self.before}))

  def _tick_empty(self, U, rel):
    if self.stage == 0:
      self.before = self._edges()
      sx, sy = self._out_root("GLOB", "RelTime")
      tx, ty = self._anchor_root(600.0, 720.0)   # empty graph space, far from any node
      U.drag(self.ezapp, sx, sy, tx, ty, W, H, steps=8)
      self.overlay_after = self._has_overlays()
      self.after = self._edges()
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 4:
      self._cap_then(lambda: self._finish(
          self.after == self.before and not self.overlay_after,
          {"NO_MENU": not self.overlay_after, "NO_EDIT": self.after == self.before}))

  def _tick_replace(self, U, rel):
    if self.stage == 0:
      self.before = self._edges()
      sx, sy = self._out_root("GLOB2", "RelTime")
      tx, ty = self._body_root("GRAV")
      U.drag(self.ezapp, sx, sy, tx, ty, W, H, steps=8)
      self.overlay_at_menu = self._has_overlays()
      self.menu_root = self.ne._last_root
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 4:
      # entry 0 among GRAV qualifying floats = G (marked "(replaces GLOB.RelTime)").
      rx, ry = self.menu_root
      U.click(self.ezapp, rx + 6, ry + 0 * 28 + 14, W, H)
      self.after = self._edges()
      self.stage = 2
      self._sf = rel
    elif self.stage == 2 and rel - self._sf >= 4:
      moved = (("GLOB2", "RelTime", "GRAV", "G") in self.after
               and ("GLOB", "RelTime", "GRAV", "G") not in self.after)
      self._finish(moved and self.overlay_at_menu,
                   {"MENU_OPENED": self.overlay_at_menu,
                    "NEW_EDGE": ("GLOB2", "RelTime", "GRAV", "G") in self.after,
                    "OLD_EDGE_GONE": ("GLOB", "RelTime", "GRAV", "G") not in self.after})

  # ---- render dot-count verdict --------------------------------------------

  def _render_verdict(self):
    import numpy
    rgb = self._cap_rgb
    model = self.model
    connected_ins = {(e[2], e[3]) for e in model.edges()}
    lit_conn = lit_unconn = 0
    n_conn = n_unconn = 0
    # sample a tight 3x3 patch a few px ABOVE the top-edge input anchor: a drawn input dot
    # (radius ~3.4 graph units) lights this band, an absent one leaves background — the band
    # stops short of the node's own (always-lit) header edge just below the anchor.
    off = 3
    for nid in model.nodes():
      p = model.pos(nid)
      ins = model.inputs(nid)
      from ork.ui import node_editor_math as gm
      anchors, _o = gm.port_anchors(p, len(ins), len(model.outputs(nid)), "box", "vertical")
      for k, (gx, gy) in enumerate(anchors):
        rx, ry = self._anchor_root(gx, gy)
        lit = self._patch_lit(rgb, rx, ry - off)
        if (nid, ins[k][0]) in connected_ins:
          n_conn += 1
          lit_conn += int(lit)
        else:
          n_unconn += 1
          lit_unconn += int(lit)
    self._finish(True, {"N_UNCONN": n_unconn, "LIT_UNCONN": lit_unconn,
                        "N_CONN": n_conn, "LIT_CONN": lit_conn})

  def _patch_lit(self, rgb, cx, cy):
    if rgb is None:
      return False
    h, w, _ = rgb.shape
    x0, x1 = max(0, cx - 1), min(w, cx + 2)
    y0, y1 = max(0, cy - 1), min(h, cy + 2)
    if x0 >= x1 or y0 >= y1:
      return False
    patch = rgb[y0:y1, x0:x1, :3]
    return bool(int(patch.max()) > 70)

  # ---- capture drain -------------------------------------------------------

  def _cap_then(self, cb):
    if not self._cap_pending and self._cap_rgb is None:
      self._cap_pending = True
      self._cap_cb = cb
    elif self._cap_rgb is not None:
      cb2 = self._cap_cb
      self._cap_cb = None
      cb2()

  def onGpuPostFrame(self, ctx):
    if self._done:
      return
    if self._cap_pending and not self._cap_inflight:
      lev2 = self.lev2
      rtg = ctx.FBI.main_RTG
      self._cap_buf = lev2.CaptureBuffer()
      self._cap_async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap_buf, "RGBA8")
      self._cap_inflight = True
    elif self._cap_inflight and (self._cap_async is None or bool(self._cap_async.is_ready)):
      import numpy
      cb = self._cap_buf
      w, h = cb.width, cb.height
      arr = numpy.array(cb, dtype=numpy.uint8).reshape(h, w, 4)
      self._cap_rgb = arr[..., :3].copy()
      self._cap_inflight = False
      self._cap_pending = False

  def _finish(self, ok, kv):
    self._done = True
    if self.capture and self._cap_rgb is not None:
      try:
        from PIL import Image
        d = os.path.dirname(self.capture)
        if d:
          os.makedirs(d, exist_ok=True)
        Image.fromarray(self._cap_rgb).save(self.capture)
        import numpy
        nb = int(numpy.count_nonzero(self._cap_rgb.max(axis=2) > 24))
        print(f"WROTE {self.capture} nonblack={nb}", flush=True)
      except Exception as e:
        print(f"png save failed: {e}", flush=True)
    for k, v in kv.items():
      print(f"{k}={v}", flush=True)
    print(f"CUX_{self.mode.upper().replace('-', '_')}={'PASS' if ok else 'FAIL'}", flush=True)
    self.ezapp.signalExit()


def run_mode(mode, capture):
  app = ConnectUxLeaf(mode, capture)
  app.ezapp.mainThreadLoop()
  sys.exit(0)


################################################################################
# orchestrator
################################################################################

def _run(mode, png=None, timeout=180):
  argv = [os.path.abspath(__file__), "--mode", mode]
  if png:
    argv += ["--capture", png]
  try:
    r = subprocess.run([sys.executable if not _orkpython() else _orkpython()] + argv,
                       capture_output=True, text=True, timeout=timeout)
  except subprocess.TimeoutExpired:
    print(f"  (mode {mode} TIMED OUT)", flush=True)
    return {"rc": -1}
  sys.stdout.write("\n".join("    [%s] %s" % (mode, ln)
                             for ln in r.stdout.splitlines()
                             if "=" in ln or ln.startswith("WROTE")) + "\n")
  if r.returncode != 0:
    sys.stderr.write(r.stderr[-800:])
  out = {"rc": r.returncode}
  for ln in r.stdout.splitlines():
    if ln.startswith("WROTE "):
      out["WROTE"] = True
    for tok in ln.split():
      if "=" in tok:
        k, v = tok.split("=", 1)
        if k and k[0].isalpha():
          out[k] = v
  return out


def _orkpython():
  import shutil
  return shutil.which("ork.python")


def _png_equal(a, b):
  import numpy
  from PIL import Image
  ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
  ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
  if ia.shape != ib.shape:
    return (False, f"shape {ia.shape} vs {ib.shape}", 0)
  diff = numpy.abs(ia - ib)
  differing = int(numpy.count_nonzero(diff.max(axis=2) > 0))
  return (differing == 0, f"differing={differing}", differing)


def orchestrate():
  tmp = "/tmp/connect_ux_gate"
  os.makedirs(tmp, exist_ok=True)
  fails = []

  def need(cond, msg):
    (fails.append(msg) if not cond else None)
    print(("  FAIL: " if not cond else "  ok  : ") + msg, flush=True)

  print("=" * 78)
  print("connect_ux_gate — drag-to-node connection UX")
  print("=" * 78)

  base = os.path.join(tmp, "baseline.png")
  rBase = _run("baseline", base)
  need(rBase.get("rc") == "0" or rBase.get("CUX_BASELINE") == "PASS", "baseline settled + captured")

  print("[menu: multi-candidate dropdown -> select -> edge]", flush=True)
  mA = os.path.join(tmp, "menu_A.png")
  mB = os.path.join(tmp, "menu_B.png")
  rmA = _run("menu", mA)
  rmB = _run("menu", mB)
  need(rmA.get("MENU_OPENED") == "True", "drag output->node body OPENED the dropdown (hasOverlays)")
  need(rmA.get("EDGE_OK") == "True", "selecting entry 1 created GLOB.RelTime->GRAV.Mass")
  need(rmA.get("MENU_DISMISSED") == "True", "menu dismissed after selection")
  need(rmA.get("CUX_MENU") == "PASS" and rmB.get("CUX_MENU") == "PASS", "both menu runs PASS")
  det_ok, det_msg, _ = _png_equal(mA, mB) if (rmA.get("WROTE") and rmB.get("WROTE")) else (False, "no png", 0)
  need(det_ok, f"two menu runs are byte-identical ({det_msg})")

  print("[single: fast path, no menu]", flush=True)
  rS = _run("single")
  need(rS.get("NO_MENU") == "True", "single-candidate did NOT open a menu (fast path)")
  need(rS.get("EDGE_OK") == "True", "single-candidate connected POOL.pool->TURB.pool")

  print("[zero + empty: clean cancel, no edit, parity vs baseline]", flush=True)
  zPng = os.path.join(tmp, "zero.png")
  ePng = os.path.join(tmp, "empty.png")
  rZ = _run("zero", zPng)
  rE = _run("empty", ePng)
  need(rZ.get("NO_EDIT") == "True" and rZ.get("NO_MENU") == "True", "zero-qualifying cancels (no edit, no menu)")
  need(rE.get("NO_EDIT") == "True" and rE.get("NO_MENU") == "True", "empty-canvas cancels (no edit, no menu)")
  zpar, zmsg, _ = _png_equal(base, zPng) if (rBase.get("WROTE") and rZ.get("WROTE")) else (False, "no png", 0)
  epar, emsg, _ = _png_equal(base, ePng) if (rBase.get("WROTE") and rE.get("WROTE")) else (False, "no png", 0)
  need(zpar, f"zero-cancel capture == baseline ({zmsg})")
  need(epar, f"empty-cancel capture == baseline ({emsg})")

  print("[replace: marked entry re-wires the input]", flush=True)
  rR = _run("replace")
  need(rR.get("MENU_OPENED") == "True", "replace drag opened the dropdown")
  need(rR.get("NEW_EDGE") == "True", "GLOB2.RelTime->GRAV.G created")
  need(rR.get("OLD_EDGE_GONE") == "True", "old GLOB.RelTime->GRAV.G removed")

  print("[render: unconnected input dots not drawn, connected dots kept]", flush=True)
  # The pre-merge A/B (a "render-legacy" run loaded HEAD's node_editor via `git show HEAD:` and
  # asserted "LEGACY drew a dot at every unconnected input") was a one-time development proof.
  # Once this connect-UX change BECAME HEAD, that baseline loaded the SAME post-merge file, so
  # it compared the editor to itself and a re-gate flagged it FAIL. Removed; the NEW-behavior
  # assertions below (no unconnected dot, connected dots kept) stand on their own.
  rnPng = os.path.join(tmp, "render.png")
  rN = _run("render", rnPng)
  nu = int(rN.get("N_UNCONN", -1))
  lu_new = int(rN.get("LIT_UNCONN", -1))
  lc_new = int(rN.get("LIT_CONN", -1)); nc = int(rN.get("N_CONN", -1))
  need(nu > 0, f"graph has unconnected inputs to test ({nu})")
  need(lu_new == 0, f"NEW draws NO dot at any unconnected input anchor ({lu_new}/{nu})")
  need(nc > 0 and lc_new == nc, f"NEW still draws connected-input dots ({lc_new}/{nc})")
  print(f"  plug dots: {nu} unconnected inputs draw NO dot (lit={lu_new}); "
        f"connected kept {lc_new}/{nc}", flush=True)

  print("=" * 78)
  if fails:
    print(f"CONNECT_UX_GATE: FAIL ({len(fails)} check(s))")
    for f in fails:
      print("   -", f)
    sys.exit(1)
  print("CONNECT_UX_GATE: PASS")
  sys.exit(0)


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", default=None,
                  choices=["baseline", "menu", "single", "zero", "empty", "replace",
                           "render"])
  ap.add_argument("--capture", default=None)
  args = ap.parse_args()
  if args.mode:
    run_mode(args.mode, args.capture)
  else:
    orchestrate()


if __name__ == "__main__":
  main()
