#!/usr/bin/env ork.python
################################################################################
# wire_delete_gate — wire-disconnection UX gate for the generic node editor
# (ork.ui.node_editor.NodeEditor). Offscreen + lockstep, END-TO-END INJECTED via
# app.injectUiEvent + ork.uitest (mouse) and injected KEY_DOWN (Delete), routed to
# the editor exactly as the dflowedit host wires it (KEY_DOWN -> ne.handleKeyDown).
#
# The connect-UX merge removed input-plug interactivity, which killed the classic
# "grab the wire's plug end and pull it off" disconnect. This gate covers the two
# disconnect paths designed for the NEW interaction model:
#   * DETACH drag  : press a CONNECTED input dot and drag OFF. Release on empty
#                    canvas disconnects the wire; release on another node re-routes
#                    it (re-enters the connect dropdown, source end fixed).
#   * SELECT+DELETE: click a wire (distance-to-curve) to select it (yellow/thick
#                    highlight), then Delete/Backspace disconnects it. Clicking
#                    empty canvas deselects.
# Both paths go through the document disconnect (the same path connect/replace use).
#
# Sub-gates (each is a FRESH offscreen process => a clean GPU context):
#   baseline : settle + capture, no injection (parity reference for deselect).
#   detach   : drag GRAV.G's connected input dot -> empty canvas -> edge gone,
#              no overlay. Two runs => byte-identical final capture (determinism).
#   select   : click the GLOB.RelTime->GRAV.G wire midpoint -> sel_edges holds it;
#              capture differs from baseline AT the wire (the selection highlight).
#   delete   : click the wire -> inject Delete (261) -> edge gone from the document.
#   deselect : click the wire (select) then click empty canvas -> sel_edges empty,
#              capture == baseline (parity).
#   reroute  : drag GRAV.G's connected input dot onto GRAV2 -> dropdown -> select
#              entry 0 (G) -> edge is now GLOB.RelTime->GRAV2.G, old GRAV.G gone.
#   negative : drag the POOL.pool OUTPUT dot onto TURB body -> a CONNECT drag (edge
#              POOL.pool->TURB.pool ADDED); an output drag never detaches.
#
#   run:  ork.python ork.lev2/pyext/tests/ui/wire_delete_gate.py
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

# node positions (graph space, top-left). The GLOB.RelTime->GRAV.G wire runs through
# open space (clickable midpoint); GRAV2/TURB/POOL are well separated.
POS = {
    "GLOB":  (100.0, 120.0),
    "GRAV":  (450.0, 420.0),
    "GRAV2": (100.0, 640.0),
    "TURB":  (820.0, 120.0),
    "POOL":  (820.0, 460.0),
}


def _build_model(prewire=None):
  from orkengine import lev2
  from orkengine.core import dataflow as _dflow
  from ork.hypergraph.dflow.graphdata_document import GraphDataDocument
  from ork.editor.graphdata_node_model import GraphDataNodeGraphModel
  P = lev2.particles
  g = _dflow.GraphData.createShared()
  g.create("GLOB",  P.Globals)
  g.create("GRAV",  P.Gravity)
  g.create("GRAV2", P.Gravity)
  g.create("TURB",  P.Turbulence)
  g.create("POOL",  P.Pool)
  doc = GraphDataDocument(g)
  model = GraphDataNodeGraphModel(doc)
  for nid, (x, y) in POS.items():
    model.set_pos(nid, x, y)
  for (si, sp, di, dp) in (prewire or []):
    model.connect(si, sp, di, dp)
  return model


################################################################################
# offscreen leaf app
################################################################################

class WireDeleteLeaf:
  def __init__(self, mode, capture):
    from orkengine.core import CrcStringProxy
    from orkengine import lev2
    from ork.ui.node_editor import NodeEditor, COL_BG
    self.lev2 = lev2
    self.tokens = CrcStringProxy()
    self.mode = mode
    self.capture = capture
    self.COL_BG = COL_BG
    self._NodeEditor = NodeEditor
    # every mode but the pure output-connect starts with the GLOB.RelTime->GRAV.G wire.
    self._prewire = None if mode == "negative" else [("GLOB", "RelTime", "GRAV", "G")]
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
    self._cap_pending = False
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None
    self._cap_rgb = None
    self._done = False

  def onGpuInit(self, ctx):
    self.model = _build_model(self._prewire)
    self.ne = self._NodeEditor(self.canvas, self.model, title="wdel", orientation="vertical")
    self.ne.uicontext = self.ezapp.uicontext
    self.ne.gpuInit(ctx)
    # keyboard routing (dflowedit idiom): a KEY_DOWN ui event -> ne.handleKeyDown.
    inner = self.ne._onUiEvent
    tk = self.tokens
    def _wrapped(ev):
      if ev.code == tk.KEY_DOWN.hashed:
        self.ne.handleKeyDown(ev)
        return self.lev2.ui.HandlerResult()
      if ev.code == tk.KEY_UP.hashed:
        self.ne.handleKeyUp(ev)
        return self.lev2.ui.HandlerResult()
      return inner(ev)
    self.canvas.onUiEvent = _wrapped

  # ---- geometry helpers (root pixel coords) --------------------------------

  def _anchor_root(self, gx, gy):
    lx, ly = self.ne.view.graph_to_screen(gx, gy)
    return (int(round(self.canvas.x + lx)), int(round(self.canvas.y + ly)))

  def _out_root(self, nid, out_plug):
    outs = self.model.outputs(nid)
    idx = next(k for k, (pn, _t) in enumerate(outs) if pn == out_plug)
    info = self.ne._plug_info(nid, "out", idx)
    return self._anchor_root(info[2][0], info[2][1])

  def _in_root(self, nid, in_plug):
    ins = self.model.inputs(nid)
    idx = next(k for k, (pn, _t) in enumerate(ins) if pn == in_plug)
    info = self.ne._plug_info(nid, "in", idx)
    return self._anchor_root(info[2][0], info[2][1])

  def _body_root(self, nid):
    _k, x, y, w, h = self.ne._node_geom(self.model, nid)
    return self._anchor_root(x + w * 0.5, y + h * 0.5)

  def _wire_mid_root(self, edge):
    from ork.ui import node_editor_math as gm
    ep = self.ne._wire_endpoints(self.model, edge)
    pts = gm.wire_points(ep[0], ep[1], "vertical", segments=22)
    mid = pts[len(pts) // 2]
    return self._anchor_root(mid[0], mid[1])

  def _edges(self):
    return set(tuple(e) for e in self.model.edges())

  def _sel_edges(self):
    return set(tuple(e) for e in self.ne.sel_edges)

  def _has_overlays(self):
    ctx = self.ezapp.uicontext
    return bool(ctx.hasOverlays()) if ctx is not None else False

  # ---- update loop ---------------------------------------------------------

  def onUpdate(self, updata):
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
    getattr(self, "_tick_" + self.mode)(rel)

  # ---- per-mode ticks ------------------------------------------------------

  def _tick_baseline(self, rel):
    self._cap_then(lambda: self._finish(True, {}))

  def _tick_detach(self, rel):
    import ork.uitest as U
    if self.stage == 0:
      self.before = self._edges()
      sx, sy = self._in_root("GRAV", "G")             # connected input dot (wire dest end)
      tx, ty = self._anchor_root(650.0, 730.0)        # empty graph space, far from any node
      U.drag(self.ezapp, sx, sy, tx, ty, W, H, steps=8)
      self.overlay_after = self._has_overlays()
      self.after = self._edges()
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 4:
      gone = ("GLOB", "RelTime", "GRAV", "G") not in self.after
      self._cap_then(lambda: self._finish(
          gone and not self.overlay_after,
          {"EDGE_GONE": gone, "NO_OVERLAY": not self.overlay_after,
           "EDGES_AFTER": sorted(self.after)}))

  def _tick_select(self, rel):
    import ork.uitest as U
    if self.stage == 0:
      mx, my = self._wire_mid_root(("GLOB", "RelTime", "GRAV", "G"))
      U.click(self.ezapp, mx, my, W, H)
      self.sel = self._sel_edges()
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 2:
      ok = ("GLOB", "RelTime", "GRAV", "G") in self.sel
      self._cap_then(lambda: self._finish(ok, {"SEL_OK": ok, "SEL": sorted(self.sel)}))

  def _tick_delete(self, rel):
    import ork.uitest as U
    if self.stage == 0:
      self.before = self._edges()
      mx, my = self._wire_mid_root(("GLOB", "RelTime", "GRAV", "G"))
      U.click(self.ezapp, mx, my, W, H)
      self.sel = self._sel_edges()
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 2:
      U.key_down(self.ezapp, 261)                      # Delete (widget-focused, key-registry)
      U.key_up(self.ezapp, 261)
      self.after = self._edges()
      self.stage = 2
      self._sf = rel
    elif self.stage == 2 and rel - self._sf >= 2:
      gone = ("GLOB", "RelTime", "GRAV", "G") not in self.after
      self._cap_then(lambda: self._finish(
          gone and bool(self.sel),
          {"SELECTED": bool(self.sel), "EDGE_GONE": gone, "EDGES_AFTER": sorted(self.after)}))

  def _tick_deselect(self, rel):
    import ork.uitest as U
    if self.stage == 0:
      mx, my = self._wire_mid_root(("GLOB", "RelTime", "GRAV", "G"))
      U.click(self.ezapp, mx, my, W, H)
      self.sel1 = self._sel_edges()
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 10:      # clear the double-click re-arm window
      ex, ey = self._anchor_root(650.0, 730.0)         # empty canvas
      U.click(self.ezapp, ex, ey, W, H)
      self.sel2 = self._sel_edges()
      self.stage = 2
      self._sf = rel
    elif self.stage == 2 and rel - self._sf >= 2:
      # edge still present (deselect must NOT delete), selection cleared.
      still = ("GLOB", "RelTime", "GRAV", "G") in self._edges()
      self._cap_then(lambda: self._finish(
          bool(self.sel1) and not self.sel2 and still,
          {"WAS_SELECTED": bool(self.sel1), "NOW_EMPTY": not self.sel2, "EDGE_KEPT": still}))

  def _tick_reroute(self, rel):
    import ork.uitest as U
    if self.stage == 0:
      self.before = self._edges()
      sx, sy = self._in_root("GRAV", "G")              # grab the wire's dest end
      tx, ty = self._body_root("GRAV2")                # drop onto a fresh Gravity
      U.drag(self.ezapp, sx, sy, tx, ty, W, H, steps=8)
      self.overlay_at_menu = self._has_overlays()
      self.menu_root = self.ne._last_root
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 4:
      # entry 0 among GRAV2's qualifying floats = G (fresh node, same plug order).
      rx, ry = self.menu_root
      U.click(self.ezapp, rx + 6, ry + 0 * 28 + 14, W, H)
      self.after = self._edges()
      self.stage = 2
      self._sf = rel
    elif self.stage == 2 and rel - self._sf >= 4:
      moved = (("GLOB", "RelTime", "GRAV2", "G") in self.after
               and ("GLOB", "RelTime", "GRAV", "G") not in self.after)
      self._finish(moved and self.overlay_at_menu,
                   {"MENU_OPENED": self.overlay_at_menu,
                    "NEW_EDGE": ("GLOB", "RelTime", "GRAV2", "G") in self.after,
                    "OLD_EDGE_GONE": ("GLOB", "RelTime", "GRAV", "G") not in self.after})

  def _tick_negative(self, rel):
    import ork.uitest as U
    if self.stage == 0:
      self.before = self._edges()
      sx, sy = self._out_root("POOL", "pool")          # OUTPUT dot -> CONNECT drag
      tx, ty = self._body_root("TURB")
      U.drag(self.ezapp, sx, sy, tx, ty, W, H, steps=8)
      self.overlay_after = self._has_overlays()
      self.after = self._edges()
      self.stage = 1
      self._sf = rel
    elif self.stage == 1 and rel - self._sf >= 4:
      added = ("POOL", "pool", "TURB", "pool") in self.after
      # an output drag must ADD an edge (connect), never remove one (detach).
      no_loss = self.before.issubset(self.after)
      self._finish(added and no_loss and not self.overlay_after,
                   {"CONNECT_ADDED": added, "NO_EDGE_LOST": no_loss,
                    "NEW_EDGES": sorted(self.after - self.before)})

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
    print(f"WDEL_{self.mode.upper()}={'PASS' if ok else 'FAIL'}", flush=True)
    self.ezapp.signalExit()


def run_mode(mode, capture):
  app = WireDeleteLeaf(mode, capture)
  app.ezapp.mainThreadLoop()
  sys.exit(0)


################################################################################
# orchestrator
################################################################################

def _orkpython():
  import shutil
  return shutil.which("ork.python")


def _run(mode, png=None, timeout=180):
  argv = [os.path.abspath(__file__), "--mode", mode]
  if png:
    argv += ["--capture", png]
  try:
    r = subprocess.run([_orkpython() or sys.executable] + argv,
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


def _png_diff(a, b):
  import numpy
  from PIL import Image
  ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
  ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
  if ia.shape != ib.shape:
    return (-1, f"shape {ia.shape} vs {ib.shape}")
  diff = numpy.abs(ia - ib)
  differing = int(numpy.count_nonzero(diff.max(axis=2) > 0))
  return (differing, f"differing={differing}")


def orchestrate():
  tmp = "/tmp/wire_delete_gate"
  os.makedirs(tmp, exist_ok=True)
  fails = []

  def need(cond, msg):
    (fails.append(msg) if not cond else None)
    print(("  FAIL: " if not cond else "  ok  : ") + msg, flush=True)

  print("=" * 78)
  print("wire_delete_gate — wire disconnection UX (detach drag + select/Delete)")
  print("=" * 78)

  base = os.path.join(tmp, "baseline.png")
  rBase = _run("baseline", base)
  need(rBase.get("WDEL_BASELINE") == "PASS", "baseline settled + captured")

  print("[detach: drag connected input dot -> empty canvas disconnects]", flush=True)
  dA = os.path.join(tmp, "detach_A.png")
  dB = os.path.join(tmp, "detach_B.png")
  rdA = _run("detach", dA)
  rdB = _run("detach", dB)
  need(rdA.get("EDGE_GONE") == "True", "detach drag removed GLOB.RelTime->GRAV.G from the document")
  need(rdA.get("NO_OVERLAY") == "True", "detach onto empty canvas opened no menu")
  need(rdA.get("WDEL_DETACH") == "PASS" and rdB.get("WDEL_DETACH") == "PASS", "both detach runs PASS")
  ddiff, dmsg = _png_diff(dA, dB) if (rdA.get("WROTE") and rdB.get("WROTE")) else (-1, "no png")
  need(ddiff == 0, f"two detach runs are byte-identical ({dmsg})")

  print("[select: click wire selects it (highlight delta vs baseline)]", flush=True)
  selPng = os.path.join(tmp, "select.png")
  rSel = _run("select", selPng)
  need(rSel.get("SEL_OK") == "True", "clicking the wire midpoint selected the edge")
  sdiff, smsg = _png_diff(base, selPng) if (rBase.get("WROTE") and rSel.get("WROTE")) else (-1, "no png")
  need(sdiff > 0, f"selection highlight changes the capture at the wire ({smsg})")

  print("[delete: click wire then Delete disconnects]", flush=True)
  rDel = _run("delete")
  need(rDel.get("SELECTED") == "True", "wire selected before Delete")
  need(rDel.get("EDGE_GONE") == "True", "Delete disconnected the selected wire")

  print("[deselect: click empty canvas clears selection, parity vs baseline]", flush=True)
  desPng = os.path.join(tmp, "deselect.png")
  rDes = _run("deselect", desPng)
  need(rDes.get("WAS_SELECTED") == "True", "wire was selected first")
  need(rDes.get("NOW_EMPTY") == "True", "clicking empty canvas cleared the wire selection")
  need(rDes.get("EDGE_KEPT") == "True", "deselect did NOT delete the wire")
  ediff, emsg = _png_diff(base, desPng) if (rBase.get("WROTE") and rDes.get("WROTE")) else (-1, "no png")
  need(ediff == 0, f"deselect capture == baseline ({emsg})")

  print("[reroute: drag connected input dot onto another node re-wires it]", flush=True)
  rRr = _run("reroute")
  need(rRr.get("MENU_OPENED") == "True", "detach onto GRAV2 opened the connect dropdown")
  need(rRr.get("NEW_EDGE") == "True", "GLOB.RelTime->GRAV2.G created")
  need(rRr.get("OLD_EDGE_GONE") == "True", "old GLOB.RelTime->GRAV.G removed")

  print("[negative: dragging an OUTPUT dot still starts a CONNECT drag]", flush=True)
  rNeg = _run("negative")
  need(rNeg.get("CONNECT_ADDED") == "True", "output drag connected POOL.pool->TURB.pool")
  need(rNeg.get("NO_EDGE_LOST") == "True", "output drag removed no edge (not a detach)")

  print("=" * 78)
  if fails:
    print(f"WIRE_DELETE_GATE: FAIL ({len(fails)} check(s))")
    for f in fails:
      print("   -", f)
    sys.exit(1)
  print("WIRE_DELETE_GATE: PASS")
  sys.exit(0)


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", default=None,
                  choices=["baseline", "detach", "select", "delete", "deselect",
                           "reroute", "negative"])
  ap.add_argument("--capture", default=None)
  args = ap.parse_args()
  if args.mode:
    run_mode(args.mode, args.capture)
  else:
    orchestrate()


if __name__ == "__main__":
  main()
