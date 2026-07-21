#!/usr/bin/env ork.python
###############################################################################
# test_roadmesh_gate.py — R-family v2 RoadMesh gates that NEED THE BUILT C++
# (run on a fleet node the coordinator assigns; NOT runnable until the roads-v2
# C++ — hmdflow_module_roadmesh.cpp — is in the binary). Two tiers:
#
#   TIER A (headless, NO GPU) — runs on any node incl. a headless Mac session:
#     * A2 SERIALIZE ROUND-TRIP byte-idempotence for a RouteSpine->RoadMesh graph.
#       Proves reflection + describeX + the ClassToucher touch (lev2_init.cpp) +
#       pyext binding for the RoadMeshModule.
#     * ClassToucher check: zero '"class": ""' (an untouched module serializes its
#       class name as "" and null-deserializes SILENTLY).
#     * scalar survival: v_meters_per_tile / gids round-trip.
#
#   TIER B (GPU compute node) — realized in test_roadmesh_tierb.py:
#     * C++<->python-reference PARITY: the baked mesh's vert/face/corner counts ==
#       the roadmesh_ref reference on the SAME spine (exact); positions within tol.
#     * WELD INTEGRITY (meshvet): position-welded edge health has 0 non-manifold
#       edges and every junction mouth seam is shared (no boundary at the seam) on
#       a junction-heavy composite.
#     * DETERMINISM: two cold bakes -> byte-identical mesh.
#
#   run (Tier A):  ork.python <this>            (headless; opq/core/lev2 subsystems)
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys
from orkengine import core        # core before lev2
from orkengine import lev2
from orkengine import ecs         # full class registration (bare imports serialize EMPTY)
from orkengine.core import Object
from orkengine.core import dataflow

hm = lev2.hypermesh


def _no_empty_class(js, label):
  assert '"class": ""' not in js and '"class":""' not in js, \
      f"{label}: empty class name — RoadMeshModule is NOT touched in the ClassToucher (lev2_init.cpp)"


def _roundtrip(obj, label):
  js1 = obj.serializeJson()
  _no_empty_class(js1, label)
  obj2 = Object.deserializeJson(js1)
  assert obj2 is not None, f"{label}: deserializeJson returned null"
  js2 = obj2.serializeJson()
  assert js1 == js2, (f"{label}: round-trip NOT byte-identical ({len(js1)}B vs {len(js2)}B) — "
                      "a RoadMesh field is written-but-not-read or read-but-not-written")
  return js1, obj2


def _build_roadmesh_graph():
  """RouteSpine -> RoadMesh (the v2 skinner edge)."""
  g = dataflow.GraphData.createShared()
  spine = hm.RouteSpineModule.createShared()
  spine.set_pois([-300.0, -260.0, 280.0, 120.0, 40.0, 300.0])  # >= 2 POIs -> a fork
  spine.extent_m = 1024.0
  spine.layout_cell_m = 32.0
  spine.field_dim = 256
  spine.max_grade = 0.20
  spine.width_m = 6.0
  g.addModule(spine, "route_spine")

  rm = hm.RoadMeshModule.createShared()
  rm.v_meters_per_tile = 8.0
  rm.junction_setback_scale = 1.5
  rm.road_gid = 1
  rm.junction_gid = 2
  g.addModule(rm, "road_mesh")
  g.connect(rm.inputs.In, spine.outputs.Out)   # XfNodeGraph edge
  return g, spine, rm


def _tier_a():
  g, spine, rm = _build_roadmesh_graph()
  js1, g2 = _roundtrip(g, "RoadMesh-graph")
  print(f"RoadMesh-graph JSON bytes={len(js1)}", flush=True)
  assert '"hypermesh::RoadMeshModuleData"' in js1, \
      "expected class hypermesh::RoadMeshModuleData missing from JSON (unregistered/untouched?)"
  rm2 = g2.findModule("road_mesh")
  assert rm2 is not None, "road_mesh lost through round-trip"
  assert abs(rm2.v_meters_per_tile - 8.0) < 1e-6, "v_meters_per_tile drifted"
  assert rm2.road_gid == 1 and rm2.junction_gid == 2, "gid split drifted through round-trip"
  print("RoadMesh scalar + gid survival: OK", flush=True)
  print("ROADMESH_GATE_TIER_A_RESULT=PASS", flush=True)


def _tier_b():
  print("ROADMESH_GATE_TIER_B=see test_roadmesh_tierb.py (GPU node) — parity + weld + determinism", flush=True)


def main():
  # PURE serdes for Tier A — NO GPU (ClassToucher runs graphics-free in lev2appinit).
  lev2.lev2appinit(use_subsystems=['opq', 'core', 'lev2'])
  try:
    _tier_a()
    _tier_b()
    return 0
  finally:
    core.coreappexit()   # ALWAYS — a skipped coreappexit hangs teardown + masks the traceback


if __name__ == "__main__":
  sys.exit(main())
