#!/usr/bin/env ork.python
################################################################################
# PrimCanvas SVG export oracle (headless).
#
# Fix 1 (prim_canvas.cpp:_doSvgExport) made canvas-space and SVG-space share the
# same top-left/Y-down convention (post-9359c9e20). This test builds a quad, a
# rotated quad, a TriList triangle and a text item at KNOWN Y-down coordinates,
# triggers exportSvg(), parses the written SVG with stdlib xml, and asserts the
# exported coordinates are an IDENTITY passthrough of the inputs (not
# canvas_h - y - size, and not a negated rotation).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import math
import tempfile
import xml.etree.ElementTree as ET

from orkengine import core   # core before lev2 (import order law)
from orkengine import lev2
from orkengine.core import vec2, vec4, CrcStringProxy

tokens = CrcStringProxy()

SVGNS = {"svg": "http://www.w3.org/2000/svg"}

CANVAS_W = 400
CANVAS_H = 300

# Known input coordinates (canvas space, top-left/Y-down)
QUAD_A = dict(pos=(10.0, 20.0), size=(100.0, 50.0), rot=0.0)
QUAD_B = dict(pos=(220.0, 20.0), size=(40.0, 40.0), rot=math.radians(30.0))
TRI_VERTS = [(50.0, 150.0), (150.0, 150.0), (100.0, 220.0)]
TEXT_POS = (200.0, 100.0)
TEXT_STR = "ORACLE"
TEXT_BASELINE_OFFSET = 12.0  # existing, never-flipped behavior (Fix 1 leaves it alone)

MAX_SETTLE_FRAMES = 120


class SvgExportOracle:

  def __init__(self):
    self.ok = True
    self.fail_reasons = []
    self.svg_fd, self.svg_path = tempfile.mkstemp(prefix="prim_canvas_", suffix=".svg")
    os.close(self.svg_fd)
    os.remove(self.svg_path)  # exportSvg() must create it fresh

    self.ezapp = lev2.OrkEzApp.create(self, width=CANVAS_W, height=CANVAS_H, offscreen=True)
    self.ezapp.topWidget.enableUiDraw()  # required or Surface widgets (PrimCanvas) never DoDraw()

    lg_group = self.ezapp.topLayoutGroup
    canvas_layout = lg_group.makeChild(uiclass=lev2.ui.PrimCanvas, args=["svg_test_canvas"])
    self.canvas = canvas_layout.widget

    root_layout = lg_group.layout
    canvas_layout.layout.top.anchorTo(root_layout.top)
    canvas_layout.layout.left.anchorTo(root_layout.left)
    canvas_layout.layout.bottom.anchorTo(root_layout.bottom)
    canvas_layout.layout.right.anchorTo(root_layout.right)

    self.canvas.draw_background = False  # keep exported <rect> set to just our quads

    self.frame_count = 0
    self.export_requested_frame = None

  def onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)
    layer = self.canvas.createLayer("main")

    # Quad A: axis-aligned
    quad_prim = lev2.ui.QuadPrimitive(pipeline=self.canvas.pipelineSolid)
    qa = lev2.ui.QuadData()
    qa.setPosition(*QUAD_A["pos"])
    qa.setSize(*QUAD_A["size"])
    qa.setColor(vec4(1, 0, 0, 1))
    quad_prim.addQuad(qa)

    # Quad B: rotated (validates the rotation-sign fix)
    qb = lev2.ui.QuadData()
    qb.setPosition(*QUAD_B["pos"])
    qb.setSize(*QUAD_B["size"])
    qb.setRotation(QUAD_B["rot"])
    qb.setColor(vec4(0, 1, 0, 1))
    quad_prim.addQuad(qb)
    layer.addPrimitive(quad_prim)

    # Triangle: known Y-down verts
    tri_prim = lev2.ui.TriListPrimitive(pipeline=self.canvas.pipelineVtxSolid)
    for (x, y) in TRI_VERTS:
      vd = lev2.ui.VertexData()
      vd.setPosition(x, y)
      vd.setColor(vec4(0, 0, 1, 1))
      tri_prim.addVertex(vd)
    layer.addPrimitive(tri_prim)

    # Text item
    font = lev2.FontManager.fontForId("i14")
    text_prim = lev2.ui.TextPrimitive(font=font, color=vec4(1, 1, 1, 1))
    text_prim.addItem(TEXT_STR, vec2(*TEXT_POS))
    layer.addPrimitive(text_prim)

  def onUpdate(self, updinfo):
    self.frame_count += 1

    if self.export_requested_frame is None:
      if self.canvas.width > 0 and self.canvas.height > 0:
        self.canvas.exportSvg(self.svg_path)
        self.export_requested_frame = self.frame_count
      elif self.frame_count > MAX_SETTLE_FRAMES:
        self.fail_reasons.append(
            f"canvas never reached nonzero size after {MAX_SETTLE_FRAMES} frames "
            f"(width={self.canvas.width} height={self.canvas.height})")
        self.ezapp.signalExit()
      return

    # Give the export a couple frames past the request to land on disk, then stop.
    if os.path.exists(self.svg_path) or (self.frame_count - self.export_requested_frame) > 5:
      self.ezapp.signalExit()

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()


def _local(tag):
  return tag.split("}", 1)[-1] if "}" in tag else tag


def _check(results, name, condition, detail=""):
  results.append(condition)
  status = "PASS" if condition else "FAIL"
  suffix = f" ({detail})" if detail and not condition else ""
  print(f"  {status}: {name}{suffix}", flush=True)


def _evaluate(app):
  if app.fail_reasons:
    for r in app.fail_reasons:
      print(f"FAIL: {r}", flush=True)
    return 1

  if not os.path.exists(app.svg_path):
    print(f"FAIL: SVG was never written to {app.svg_path}", flush=True)
    return 1

  tree = ET.parse(app.svg_path)
  root = tree.getroot()

  rects = [e for e in root.iter() if _local(e.tag) == "rect"]
  polygons = [e for e in root.iter() if _local(e.tag) == "polygon"]
  texts = [e for e in root.iter() if _local(e.tag) == "text"]

  results = []

  print("\n[Quad A: axis-aligned identity passthrough]", flush=True)
  _check(results, "exactly 2 <rect> elements (background disabled)", len(rects) == 2,
         detail=f"got {len(rects)}")
  if len(rects) >= 1:
    ra = rects[0]
    ax, ay = QUAD_A["pos"]
    aw, ah = QUAD_A["size"]
    _check(results, f"rect[0].x == {ax}", float(ra.get("x", "nan")) == ax, ra.get("x"))
    _check(results, f"rect[0].y == {ay} (NOT canvas_h-y-h={CANVAS_H - ay - ah})",
           float(ra.get("y", "nan")) == ay, ra.get("y"))
    _check(results, f"rect[0].width == {aw}", float(ra.get("width", "nan")) == aw, ra.get("width"))
    _check(results, f"rect[0].height == {ah}", float(ra.get("height", "nan")) == ah, ra.get("height"))
    _check(results, "rect[0] has no rotate transform", ra.get("transform") is None,
           ra.get("transform"))

  print("\n[Quad B: rotated - positive Y-down rotation sense]", flush=True)
  if len(rects) >= 2:
    rb = rects[1]
    bx, by = QUAD_B["pos"]
    bw, bh = QUAD_B["size"]
    expected_deg = math.degrees(QUAD_B["rot"])
    _check(results, f"rect[1].x == {bx}", float(rb.get("x", "nan")) == bx, rb.get("x"))
    _check(results, f"rect[1].y == {by}", float(rb.get("y", "nan")) == by, rb.get("y"))
    xform = rb.get("transform", "")
    deg = None
    if xform.startswith("rotate("):
      deg = float(xform[len("rotate("):].split(",")[0])
    _check(results, f"rect[1] rotate() == +{expected_deg} deg (positive, not negated)",
           deg is not None and abs(deg - expected_deg) < 0.15, xform)

  print("\n[TriList: Y-down vertex passthrough]", flush=True)
  _check(results, "exactly 1 <polygon> element", len(polygons) == 1, detail=f"got {len(polygons)}")
  if len(polygons) >= 1:
    pts_str = polygons[0].get("points", "")
    pts = []
    for pair in pts_str.split():
      px, py = pair.split(",")
      pts.append((float(px), float(py)))
    _check(results, f"polygon verts == {TRI_VERTS} (NOT canvas_h - y)",
           pts == TRI_VERTS, pts_str)

  print("\n[Text: was never flipped, stays as-is]", flush=True)
  _check(results, "exactly 1 <text> element", len(texts) == 1, detail=f"got {len(texts)}")
  if len(texts) >= 1:
    t = texts[0]
    tx, ty = TEXT_POS
    _check(results, f"text.x == {tx}", float(t.get("x", "nan")) == tx, t.get("x"))
    _check(results, f"text.y == {ty + TEXT_BASELINE_OFFSET} (position.y + baseline offset)",
           float(t.get("y", "nan")) == ty + TEXT_BASELINE_OFFSET, t.get("y"))
    _check(results, f"text content == '{TEXT_STR}'", (t.text or "") == TEXT_STR, t.text)

  passed, failed = results.count(True), results.count(False)
  print(f"\n{'=' * 60}", flush=True)
  print(f"Results: {passed} passed, {failed} failed", flush=True)
  print(f"SVG written to: {app.svg_path}", flush=True)
  print(f"{'=' * 60}", flush=True)

  return 0 if failed == 0 else 1


def main():
  app = SvgExportOracle()
  rc = 1
  try:
    app.ezapp.mainThreadLoop()
    # Evaluate BEFORE teardown: prints the PASS/FAIL verdict table to stdout
    # regardless of what happens during shutdown() below.
    rc = _evaluate(app)
  finally:
    # The background LoaderThread (lev2_init.cpp) used to race Context teardown
    # and SIGSEGV inside shutdown()/interpreter exit on NVIDIA hosts (bare
    # OrkEzApp.create(offscreen=True) + mainThreadLoop()); fixed (#32) by winding
    # the loader down in OrkEzApp::_onGpuExit + an atexit safety net. The verdict
    # above is still printed before shutdown() so evaluation never depends on it.
    app.ezapp.shutdown()
  return rc


if __name__ == "__main__":
  sys.exit(main())
