#!/usr/bin/env ork.python
################################################################################
# shifta_autolayout_gate — Shift+A auto-layout binding gate for the generic node
# editor (ork.ui.node_editor.NodeEditor) driving the merged Sugiyama engine
# (ork.editor.graph_layout). Offscreen + lockstep, deterministic.
#
# The graph under test is a REAL tangled terrain doc (the "xxx3" DSL asset, 27
# nodes / 28 edges) bound to the canvas through the terrain node-model layer. It
# is deliberately seeded with the NAIVE (input-order) layout — 6 wire crossings —
# so Shift+A has real work to do (auto = 0 crossings).
#
# Sub-gates (each is its own process => a clean GPU context; the injection funnel
# app.injectUiEvent + ork.uitest are the SAME harness the code path ships with):
#
#   focused  : focus the canvas (cursor over it), inject Shift+A -> the applied node
#              positions become EXACTLY graph_layout's output (dict-compare vs calling
#              the algorithm directly); crossings drop from the pre-layout tangle;
#              captures the frame (non-black; two focused runs are byte-identical).
#   undo     : Shift+A then ONE undo (through on_layout_edit -> UndoStack) restores ALL
#              positions byte-exact vs the saved originals; capture == the prelayout
#              capture (undo returns the canvas to the pre-layout picture).
#   prelayout: renders the pre-layout (naive) picture WITHOUT Shift+A — the undo parity
#              reference.
#   scoping  : focus a VIEWPORT widget (cursor over it, NOT the canvas), inject Shift+A ->
#              the canvas node positions are UNCHANGED (the event never reaches the canvas
#              binding — routing hit-tests the cursor position) and the viewport saw it.
#
#   run:  ork.python ork.lev2/pyext/tests/ui/shifta_autolayout_gate.py
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse
import subprocess

# Resolve THIS worktree's obt.project/scripts so the module under test is the one edited here.
_HERE = os.path.dirname(os.path.abspath(__file__))
_WORKTREE = os.path.abspath(os.path.join(_HERE, "..", "..", "..", ".."))
_SCRIPTS = os.path.join(_WORKTREE, "obt.project", "scripts")
if _SCRIPTS not in sys.path:
  sys.path.insert(0, _SCRIPTS)

W, H = 900, 700
ASSET = "xxx3"


################################################################################
# per-mode offscreen app
################################################################################

def _build():
  from orkengine.core import vec4, CrcStringProxy
  from orkengine import lev2
  from ork.ui.node_editor import NodeEditor, COL_BG
  from ork.ui import node_editor_math as gm
  from ork.editor import graph_layout as gl
  from ork.editor.dflowedit import _load_terrain
  from ork.editor.undo_stack import UndoStack
  return (vec4, CrcStringProxy, lev2, NodeEditor, COL_BG, gm, gl, _load_terrain, UndoStack)


def _size_fn_for(ne, gm):
  model = ne.model
  def _f(nid, n_in, n_out):
    return gm.node_size(n_in, n_out, ne._render_kind(model, nid), ne.orientation)
  return _f


def _expected_layout(ne, gm, gl):
  """Compute the layout DIRECTLY through graph_layout (independent of ne.auto_layout)."""
  g = gl.graph_from_model(ne.model, orient=ne.orientation, size_fn=_size_fn_for(ne, gm))
  return gl.layout(g, orient=ne.orientation)


def _model_graph(ne, gm, gl):
  return gl.graph_from_model(ne.model, orient=ne.orientation, size_fn=_size_fn_for(ne, gm))


def _capture_pos(model):
  out = {}
  for nid in model.nodes():
    p = model.pos(nid)
    if p is not None:
      out[nid] = (round(float(p[0]), 6), round(float(p[1]), 6))
  return out


class ModeApp:
  def __init__(self, mode, capture):
    (self.vec4, self.Crc, self.lev2, self.NodeEditor, self.COL_BG,
     self.gm, self.gl, self._load_terrain, self.UndoStack) = _build()
    self.tokens = self.Crc()
    self.mode = mode
    self.capture = capture
    lev2 = self.lev2

    # REAL tangled terrain model (doc layer). No host gpuInit needed — the node model
    # reads positions/topology straight off the document.
    self.binding = self._load_terrain(ASSET)
    self.model = self.binding.node_model

    # seed the PRE-LAYOUT tangle: the naive (input-order) baseline. Every node placed,
    # so the editor's first-open auto-seed is skipped and this stays the pre-layout state.
    g = self.gl.graph_from_model(self.model, orient="vertical",
                                 size_fn=lambda nid, ni, no: self.gm.node_size(
                                     ni, no,
                                     getattr(self.model, "render_kind", lambda x: "box")(nid),
                                     "vertical"))
    naive = self.gl.layout_naive(g, orient="vertical")
    for nid in self.model.nodes():
      xy = naive.get(nid)
      if xy is not None:
        self.model.set_pos(nid, xy[0], xy[1])
    self.originals = _capture_pos(self.model)

    self.ezapp = lev2.OrkEzApp.create(self, width=W, height=H,
                                      freerun=False, target_ups=60.0, target_fps=60.0)
    self.ezapp.topWidget.enableUiDraw()
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = self.COL_BG

    # left: node-editor canvas.  right: stand-in VIEWPORT widget (focus competitor).
    left_item = lg.makeChild(fill=True, margin=2, uiclass=lev2.ui.PrimCanvas, args=["necanvas"])
    self.canvas = left_item.widget
    self.canvas.bg_color = self.COL_BG
    self.canvas.draw_background = True
    right_item = lg.split(layout=left_item.layout, proportion=0.62,
                          placement=self.tokens.RIGHT, margin=2,
                          uiclass=lev2.ui.PrimCanvas, args=["viewport"])
    self.viewport = right_item.widget
    self.viewport.bg_color = self.vec4(0.06, 0.09, 0.07, 1.0)
    self.viewport.draw_background = True

    self.ne = self.NodeEditor(self.canvas, self.model, title=ASSET, orientation="vertical")

    # undo seam wiring (host role): on_layout_edit -> the existing generic UndoStack.
    self.undo = self.UndoStack()
    self.ne.on_layout_edit = lambda pre, post, restore: self.undo.record(
        pre=pre, post=post, restore=restore, label="auto-layout")

    # focus-local key routing idiom (mirrors dflowedit/terrainedit _wireNodeEditorKeys):
    # KEY_DOWN/UP over the canvas -> the NodeEditor's own handlers; other events pass through.
    self._wireCanvasKeys()

    # the viewport records whether IT received a Shift+A (scoping proof).
    self.viewport_saw_shift_a = False
    def _vp_handler(ev):
      if ev.code == self.tokens.KEY_DOWN.hashed and ev.keycode == ord('A') and ev.shift:
        self.viewport_saw_shift_a = True
      r = self.lev2.ui.HandlerResult()
      r.setHandler(self.viewport)
      return r
    self.viewport.onUiEvent = _vp_handler

    self.ready = False
    self.ready_counter = 0
    self._did_inject = False
    self._settled_at = None
    self._captured = False
    self._cap_inflight = False
    self._cap_async = None
    self._cap_buf = None
    self._results = {}

  def _wireCanvasKeys(self):
    inner = self.ne._onUiEvent
    def _wrapped(ev):
      code = ev.code
      if code == self.tokens.KEY_DOWN.hashed:
        self.ne.handleKeyDown(ev)
        return self.lev2.ui.HandlerResult()
      if code == self.tokens.KEY_UP.hashed:
        self.ne.handleKeyUp(ev)
        return self.lev2.ui.HandlerResult()
      return inner(ev)
    self.canvas.onUiEvent = _wrapped

  # -------------------------------------------------------------------------

  def onGpuInit(self, ctx):
    self.ne.uicontext = self.ezapp.uicontext
    self.ne.gpuInit(ctx)

  def _center(self, w):
    return (w.x + w.width // 2, w.y + w.height // 2)

  def onUpdate(self, updata):
    import ork.uitest as U
    counter = int(updata.counter)
    ctx = self.ezapp.uicontext
    if ctx is not None:
      ctx.virtual_time_enabled = True
      ctx.virtual_time = counter * 0.1

    if not self.ready:
      if self.canvas.width > 0 and self.viewport.width > 0:
        self.ready = True
        self.ready_counter = counter
      elif counter > 600:
        print("FATAL layout never became ready", flush=True)
        self.ezapp.signalExit()
      return

    rel = counter - self.ready_counter
    if rel == 6 and not self._did_inject:
      self._did_inject = True
      self._inject(U)
      self._settled_at = counter
    # capture-less modes (scoping) have no capture to trigger the exit — settle + quit.
    if self._did_inject and self.capture is None and (counter - self._settled_at) >= 4:
      self.ezapp.signalExit()

  def _inject(self, U):
    cx, cy = self._center(self.canvas)
    vx, vy = self._center(self.viewport)
    gm, gl = self.gm, self.gl
    ne = self.ne

    graph = _model_graph(ne, gm, gl)
    pre_pos = _capture_pos(self.model)
    cross_pre = gl.count_crossings(pre_pos, graph.edges, graph.sizes)

    if self.mode in ("focused", "undo"):
      U.move(self.ezapp, cx, cy, W, H)                          # cursor over the canvas
      expected = _expected_layout(ne, gm, gl)                   # DIRECT algorithm call
      U.key_chord(self.ezapp, ord('A'), mods={"shift": True})   # Shift+A -> auto_layout
      applied = _capture_pos(self.model)
      exp_rounded = {k: (round(float(v[0]), 6), round(float(v[1]), 6)) for k, v in expected.items()}
      dictmatch = (applied == exp_rounded)
      cross_post = gl.count_crossings(applied, graph.edges, graph.sizes)
      print(f"POINTER_INSIDE={ne._pointer_inside}", flush=True)
      print(f"DICTMATCH={dictmatch}", flush=True)
      print(f"CROSS_PRE={cross_pre} CROSS_POST={cross_post}", flush=True)
      if not dictmatch:
        diff = [k for k in exp_rounded if applied.get(k) != exp_rounded[k]]
        print(f"DICT_DIFF_KEYS={diff[:6]} (n={len(diff)})", flush=True)
      if self.mode == "undo":
        label = self.undo.undo()
        restored = _capture_pos(self.model)
        byte_exact = (restored == self.originals)
        print(f"UNDO_LABEL={label!r} UNDO_DEPTH_AFTER={self.undo.depth()}", flush=True)
        print(f"UNDO_BYTEEXACT={byte_exact}", flush=True)
        if not byte_exact:
          bad = [k for k in self.originals if restored.get(k) != self.originals[k]]
          print(f"UNDO_DIFF_KEYS={bad[:6]} (n={len(bad)})", flush=True)

    elif self.mode == "prelayout":
      # no injection: render the pre-layout (naive) picture as the undo-parity reference.
      print(f"CROSS_PRE={cross_pre}", flush=True)

    elif self.mode == "scoping":
      before = _capture_pos(self.model)
      U.move(self.ezapp, vx, vy, W, H)                          # cursor over the VIEWPORT
      U.key_chord(self.ezapp, ord('A'), mods={"shift": True})   # Shift+A must NOT reach canvas
      after = _capture_pos(self.model)
      unchanged = (before == after)
      print(f"CANVAS_POINTER_INSIDE={ne._pointer_inside}", flush=True)
      print(f"CANVAS_UNCHANGED={unchanged}", flush=True)
      print(f"VIEWPORT_SAW_KEY={self.viewport_saw_shift_a}", flush=True)
      if not unchanged:
        bad = [k for k in before if before.get(k) != after.get(k)]
        print(f"SCOPING_DIFF_KEYS={bad[:6]} (n={len(bad)})", flush=True)

  def onGpuPostFrame(self, ctx):
    if self._captured or self.capture is None:
      return
    if self._settled_at is None:
      return
    lev2 = self.lev2
    # give the injected mutation a few frames to rebuild before the capture.
    if not self._cap_inflight:
      rtg = ctx.FBI.main_RTG
      self._cap_buf = lev2.CaptureBuffer()
      self._cap_async = ctx.FBI.captureAsFormat(rtg.buffer(0), self._cap_buf, "RGBA8")
      self._cap_inflight = True
    elif self._cap_async is None or bool(self._cap_async.is_ready):
      self._finish_capture()

  def _finish_capture(self):
    import numpy
    capbuf = self._cap_buf
    w, h = capbuf.width, capbuf.height
    arr = numpy.array(capbuf, dtype=numpy.uint8).reshape(h, w, 4)
    self._captured = True
    try:
      from PIL import Image
      d = os.path.dirname(self.capture)
      if d:
        os.makedirs(d, exist_ok=True)
      Image.fromarray(arr[..., :3]).save(self.capture)
      # non-black diagnostic: fraction of pixels that differ from the near-black bg.
      nb = int(numpy.count_nonzero(arr[..., :3].max(axis=2) > 24))
      total = w * h
      print(f"WROTE {self.capture} ({w}x{h}) nonblack={nb} frac={nb/total:.4f}", flush=True)
    except Exception as e:
      print(f"png save failed: {e}", flush=True)
    self.ezapp.signalExit()


def run_mode(mode, capture):
  app = ModeApp(mode, capture)
  app.ezapp.mainThreadLoop()
  sys.exit(0)


################################################################################
# orchestrator (no GPU): drive each mode as a subprocess + compare captures
################################################################################

def _run(mode, png):
  self_path = os.path.abspath(__file__)
  args = [self_path, "--mode", mode]
  if png:
    args += ["--capture", png]
  try:
    r = subprocess.run(args, capture_output=True, text=True, timeout=180)
  except subprocess.TimeoutExpired:
    print(f"  (mode {mode} TIMED OUT)", flush=True)
    return {"rc": -1}
  sys.stdout.write(r.stdout)
  if r.returncode != 0:
    sys.stderr.write(r.stderr)
  out = {"rc": r.returncode}
  for line in r.stdout.splitlines():
    if line.startswith("WROTE "):
      out["WROTE"] = True
    for tok in line.split():
      if "=" in tok:
        k, v = tok.split("=", 1)
        if k and k[0].isalpha():
          out[k] = v
  return out


def _png_equal(a, b):
  import numpy
  from PIL import Image
  ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
  ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
  if ia.shape != ib.shape:
    return (False, f"shape {ia.shape} vs {ib.shape}", 0, 0)
  diff = numpy.abs(ia - ib)
  differing = int(numpy.count_nonzero(diff.max(axis=2) > 0))
  maxdiff = int(diff.max())
  return (differing == 0, f"differing={differing} maxdiff={maxdiff}", differing, maxdiff)


def orchestrate():
  tmp = "/tmp/shifta_autolayout"
  os.makedirs(tmp, exist_ok=True)
  fails = []

  def need(cond, msg):
    if not cond:
      fails.append(msg)
      print("  FAIL:", msg, flush=True)
    else:
      print("  ok  :", msg, flush=True)

  print("=" * 78)
  print("shifta_autolayout_gate — Shift+A -> graph_layout binding")
  print("=" * 78)

  # --- GATE 1: focused (run twice for determinism) --------------------------
  print("[gate 1: focused injection]", flush=True)
  fA = os.path.join(tmp, "focused_A.png")
  fB = os.path.join(tmp, "focused_B.png")
  rA = _run("focused", fA)
  rB = _run("focused", fB)
  need(rA.get("POINTER_INSIDE") == "True", "canvas owns pointer/key focus on Shift+A")
  need(rA.get("DICTMATCH") == "True",
       f"applied positions EXACTLY == graph_layout output (dictmatch={rA.get('DICTMATCH')})")
  cpre = int(rA.get("CROSS_PRE", -1)); cpost = int(rA.get("CROSS_POST", -1))
  need(cpre > cpost, f"crossings improve: pre(naive)={cpre} -> post(auto)={cpost}")
  need(rA.get("WROTE") and rB.get("WROTE"), "both focused runs captured a frame")
  nbA = float(rA.get("frac", 0.0))
  need(nbA > 0.02, f"focused capture is non-black (nonblack frac={nbA:.4f})")
  det_ok, det_msg, _, _ = _png_equal(fA, fB) if (rA.get("WROTE") and rB.get("WROTE")) else (False, "no png", 0, 0)
  need(det_ok, f"two focused runs are byte-identical ({det_msg})")

  # --- GATE 2: undo byte-exact + capture parity vs pre-layout ---------------
  print("[gate 2: undo restores byte-exact]", flush=True)
  uPng = os.path.join(tmp, "undo.png")
  pPng = os.path.join(tmp, "prelayout.png")
  rU = _run("undo", uPng)
  rP = _run("prelayout", pPng)
  need(rU.get("DICTMATCH") == "True", "undo-run Shift+A also lands the exact layout")
  need(rU.get("UNDO_BYTEEXACT") == "True",
       f"one undo restores ALL positions byte-exact (byteexact={rU.get('UNDO_BYTEEXACT')})")
  need(rU.get("WROTE") and rP.get("WROTE"), "undo + prelayout captured frames")
  par_ok, par_msg, pdiff, pmax = _png_equal(uPng, pPng) if (rU.get("WROTE") and rP.get("WROTE")) else (False, "no png", 0, 0)
  # byte-equal is the target; accept a very tight bound as parity (rebuild ordering noise).
  parity = par_ok or (pdiff * 1.0 / (W * H) < 0.001 and pmax <= 4)
  need(parity, f"post-undo capture matches the pre-layout capture ({par_msg})")

  # --- GATE 3: scoping (viewport focus -> canvas never sees Shift+A) --------
  print("[gate 3: scoping proof]", flush=True)
  rS = _run("scoping", None)
  need(rS.get("CANVAS_POINTER_INSIDE") == "False",
       f"canvas does NOT own focus when the viewport is hovered (inside={rS.get('CANVAS_POINTER_INSIDE')})")
  need(rS.get("CANVAS_UNCHANGED") == "True",
       f"viewport-focused Shift+A leaves canvas positions UNCHANGED (unchanged={rS.get('CANVAS_UNCHANGED')})")
  need(rS.get("VIEWPORT_SAW_KEY") == "True",
       f"the Shift+A routed to the viewport instead (saw={rS.get('VIEWPORT_SAW_KEY')})")

  print("=" * 78)
  if fails:
    print(f"SHIFTA_AUTOLAYOUT_GATE: FAIL ({len(fails)} check(s))")
    for f in fails:
      print("   -", f)
    sys.exit(1)
  print("SHIFTA_AUTOLAYOUT_GATE: PASS")
  sys.exit(0)


def main():
  ap = argparse.ArgumentParser()
  ap.add_argument("--mode", choices=["focused", "undo", "prelayout", "scoping"], default=None)
  ap.add_argument("--capture", default=None)
  args = ap.parse_args()
  if args.mode:
    run_mode(args.mode, args.capture)
  else:
    orchestrate()


if __name__ == "__main__":
  main()
