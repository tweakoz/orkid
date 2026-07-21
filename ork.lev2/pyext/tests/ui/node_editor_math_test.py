#!/usr/bin/env python3
################################################################################
# Headless unit asserts for the node-editor view-transform + picking math.
#
# Pure python (NO lev2 / NO GPU) — runnable via plain `python3`. Exercises the
# reusable primitives in ork.ui.node_editor_math that back the C0 node editor,
# in BOTH orientations (vertical is the Houdini default, horizontal the flag).
################################################################################

import os
import sys
import math

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.normpath(os.path.join(_HERE, "..", "..", "..", "..",
                                         "obt.project", "scripts"))
sys.path.insert(0, _SCRIPTS)

# NOTE: the layered-DAG auto-layout that once lived here (auto_layout_dag) was superseded
# by the fuller Sugiyama engine in ork.editor.graph_layout and DELETED — its oracles now
# live in ork.lev2/pyext/tests/ui/graph_layout_gate.py. This file keeps only the pure
# view-transform / geometry / picking primitives that remain in node_editor_math.
from ork.ui.node_editor_math import (
    ViewTransform,
    node_size, node_rect, port_anchors, hit_test_node, pick_port,
    wire_points, polyline_dist2, type_color,
    ZOOM_MIN, ZOOM_MAX,
)

_FAILS = []


def check(cond, msg):
  if cond:
    print(f"  ok  : {msg}")
  else:
    print(f"  FAIL: {msg}")
    _FAILS.append(msg)


def approx(a, b, eps=1e-4):
  return abs(a - b) <= eps


################################################################################

def test_roundtrip():
  print("[roundtrip graph->screen->graph identity]")
  cases = [(1.0, 0.0, 0.0), (0.37, 120.0, -45.0), (2.75, -800.0, 640.0),
           (0.1, 33.0, 33.0), (3.0, -1.0, 999.0)]
  pts = [(0.0, 0.0), (137.5, -42.25), (-1000.0, 512.0), (3.14159, 2.71828)]
  for (s, ox, oy) in cases:
    vt = ViewTransform(s, ox, oy)
    for (gx, gy) in pts:
      sx, sy = vt.graph_to_screen(gx, gy)
      rx, ry = vt.screen_to_graph(sx, sy)
      check(approx(rx, gx) and approx(ry, gy),
            f"s={s} pan=({ox},{oy}) pt=({gx},{gy}) round-trips")


def test_zoom_to_cursor():
  print("[zoom-to-cursor keeps anchor point fixed]")
  for start_s in (1.0, 0.5, 2.0):
    for (cx, cy) in ((640.0, 360.0), (0.0, 0.0), (123.0, 987.0)):
      vt = ViewTransform(start_s, 50.0, -20.0)
      g0 = vt.screen_to_graph(cx, cy)
      for factor in (1.1, 1.1, 0.9, 0.5, 2.0):
        vt.zoom_at(cx, cy, factor)
        g = vt.screen_to_graph(cx, cy)
        check(approx(g[0], g0[0], 1e-3) and approx(g[1], g0[1], 1e-3),
              f"start_s={start_s} cursor=({cx},{cy}) factor={factor}: anchor fixed")


def test_zoom_clamp():
  print("[zoom clamps to sane range]")
  vt = ViewTransform(1.0, 0.0, 0.0)
  for _ in range(200):
    vt.zoom_at(400.0, 300.0, 1.5)
  check(vt.s <= ZOOM_MAX + 1e-6, f"zoom-in clamps at {ZOOM_MAX} (got {vt.s:.4f})")
  for _ in range(200):
    vt.zoom_at(400.0, 300.0, 0.5)
  check(vt.s >= ZOOM_MIN - 1e-6, f"zoom-out clamps at {ZOOM_MIN} (got {vt.s:.4f})")


def test_node_pick():
  print("[node picking hits the right node (vertical)]")
  wa, ha = node_size(1, 1, "box", "vertical")
  wb, hb = node_size(2, 1, "box", "vertical")
  rects = [("A", 100.0, 100.0, wa, ha), ("B", 400.0, 300.0, wb, hb)]
  check(hit_test_node(110.0, 110.0, rects) == "A", "inside A -> A")
  check(hit_test_node(410.0, 310.0, rects) == "B", "inside B -> B")
  check(hit_test_node(280.0, 220.0, rects) is None, "empty space -> None")
  over = [("under", 100.0, 100.0, 200.0, 200.0), ("over", 150.0, 150.0, 200.0, 200.0)]
  check(hit_test_node(200.0, 200.0, over) == "over", "overlap picks top-most node")


def test_port_anchors_vertical():
  print("[plug anchors: vertical = inputs top / outputs bottom]")
  pos = (250.0, 300.0)
  ins, outs = port_anchors(pos, 3, 2, "box", "vertical")
  x, y, w, h = node_rect(pos, 3, 2, "box", "vertical")
  for (px, py) in ins:
    check(approx(py, y), f"input on TOP edge (y={py})")
    check(x <= px <= x + w, f"input within width ({px})")
  for (px, py) in outs:
    check(approx(py, y + h), f"output on BOTTOM edge (y={py})")
    check(x <= px <= x + w, f"output within width ({px})")
  # ordered left-to-right along the edge
  check(ins[0][0] < ins[1][0] < ins[2][0], "inputs ordered along top edge")
  check(outs[0][0] < outs[1][0], "outputs ordered along bottom edge")


def test_port_anchors_horizontal():
  print("[plug anchors: horizontal = inputs left / outputs right]")
  pos = (10.0, 20.0)
  ins, outs = port_anchors(pos, 2, 1, "box", "horizontal")
  x, y, w, h = node_rect(pos, 2, 1, "box", "horizontal")
  for (px, py) in ins:
    check(approx(px, x), "input on LEFT edge")
  for (px, py) in outs:
    check(approx(px, x + w), "output on RIGHT edge")


def test_port_pick():
  print("[plug picking hits the right plug]")
  ins_a, outs_a = port_anchors((100.0, 100.0), 2, 1, "box", "vertical")
  geoms = [{"id": "A", "inputs": ins_a, "outputs": outs_a}]
  check(pick_port(*ins_a[0], geoms, 9.0) == ("A", "in", 0), "exact input-0")
  check(pick_port(*ins_a[1], geoms, 9.0) == ("A", "in", 1), "exact input-1")
  check(pick_port(*outs_a[0], geoms, 9.0) == ("A", "out", 0), "exact output-0")
  check(pick_port(ins_a[0][0] + 40, ins_a[0][1] + 40, geoms, 9.0) is None, "far -> None")
  check(pick_port(ins_a[0][0] + 2, ins_a[0][1] + 2, geoms, 9.0) == ("A", "in", 0),
        "nearest-plug resolution")


def test_wire_pick_vertical():
  print("[wire picking via polyline distance (vertical tangents)]")
  src = (100.0, 100.0)
  dst = (300.0, 400.0)
  pts = wire_points(src, dst, "vertical", segments=24)
  check(polyline_dist2(src[0], src[1], pts) < 1.0, "src endpoint on wire")
  check(math.sqrt(polyline_dist2(1000.0, 1000.0, pts)) > 100.0, "far point is far")
  # tangent leaves src going DOWN (y increases) in vertical
  check(pts[1][1] > src[1], "vertical wire leaves src downward")
  mid = pts[len(pts) // 2]
  check(math.sqrt(polyline_dist2(mid[0], mid[1] + 1.0, pts)) < 5.0, "near-curve sample near")


def test_type_color_deterministic():
  print("[type->color deterministic + distinct]")
  check(type_color("field") == type_color("field"), "same type -> identical color")
  check(type_color("field") != type_color("scalar"), "different type -> different color")
  for name in ("field", "scalar", "mesh"):
    c = type_color(name)
    check(all(0.0 <= ch <= 1.0 for ch in c), f"{name} channels in [0,1]")


def test_frame_bounds_aspects():
  print("[frame-to-bounds centers content at varied aspect ratios]")
  for (vw, vh) in ((1600, 400), (400, 1600), (900, 900), (1200, 980)):
    vt = ViewTransform(1.0, 0.0, 0.0)
    vt.frame_bounds(0.0, 0.0, 200.0, 600.0, vw, vh)   # a TALL graph
    cx, cy = vt.graph_to_screen(100.0, 300.0)
    check(approx(cx, vw * 0.5, 0.6) and approx(cy, vh * 0.5, 0.6),
          f"view {vw}x{vh}: content center -> view center")
    # fits inside the viewport on both axes
    x0, y0 = vt.graph_to_screen(0.0, 0.0)
    x1, y1 = vt.graph_to_screen(200.0, 600.0)
    check(x0 >= -1 and x1 <= vw + 1 and y0 >= -1 and y1 <= vh + 1,
          f"view {vw}x{vh}: content fits both axes")
    check(ZOOM_MIN <= vt.s <= ZOOM_MAX, f"view {vw}x{vh}: zoom in range")


################################################################################

def main():
  print("=" * 70)
  print("node_editor_math headless tests")
  print("=" * 70)
  test_roundtrip()
  test_zoom_to_cursor()
  test_zoom_clamp()
  test_node_pick()
  test_port_anchors_vertical()
  test_port_anchors_horizontal()
  test_port_pick()
  test_wire_pick_vertical()
  test_type_color_deterministic()
  test_frame_bounds_aspects()
  print("=" * 70)
  if _FAILS:
    print(f"RESULT: {len(_FAILS)} FAILURE(S)")
    for m in _FAILS:
      print("   -", m)
    return 1
  print("RESULT: ALL PASS")
  return 0


if __name__ == "__main__":
  sys.exit(main())
