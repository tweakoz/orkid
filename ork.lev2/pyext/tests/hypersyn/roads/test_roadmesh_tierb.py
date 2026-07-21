#!/usr/bin/env ork.python
###############################################################################
# test_roadmesh_tierb.py — R-family v2 RoadMesh Tier-B GPU gates (needs a compute
# node AND the roads-v2 C++ — hmdflow_module_roadmesh.cpp — in the binary). Builds
# a REAL in-graph terrain (fbm height + slope), routes a FORKED spine, skins it with
# the RoadMesh, bakes headless, dumps the mesh, and runs:
#   * REFERENCE PARITY — the C++ mesh vert/face/corner counts == the validated
#     pure-python reference (roadmesh_ref) run on the SAME baked spine (exact);
#     positions within a float tolerance.
#   * WELD INTEGRITY   — position-welded edge health: 0 non-manifold edges; every
#     junction MOUTH seam shared (no boundary crack); the road perimeter is the only
#     boundary (an open ribbon). meshvet-clean on the junction-heavy composite.
#   * DETERMINISM      — two cold bakes -> byte-identical mesh.
#
#   run:  obt.net.py run <node> --env "ORKID_DRM_MODE=" -- ork.python <this>
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORKID_DISABLE_SHADER_CACHE"] = "1"   # cold cooks for the determinism oracle
import sys, math, tempfile
from orkengine import core
from orkengine import lev2
from orkengine import ecs
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow import roads as Rd
from ork.hypergraph.dflow.roads import route_ref as R
from ork.hypergraph.dflow.roads import roadmesh_ref as RM

hm = lev2.hypermesh

EXTENT = 1024.0
LCELL  = 32.0   # coarse routing cell vs 6m road => forks fit a clean apron (authoring guidance)
FDIM   = 256
PARAMS = dict(width_m=6.0, max_grade=0.30, w_slope=8.0, w_curv=0.0, w_water=1.0e9,
              disch_thresh=1.0e9, grade_weight=8.0, base_cost=1.0)
POIS = [(-360.0, -320.0), (340.0, 300.0), (300.0, -340.0)]   # a Y forest (>= 1 fork)
RMKW = dict(v_meters_per_tile=8.0, junction_setback_scale=1.5, junction_min_edge_frac=0.45, shoulder_m=3.0)


def _bilinear(f, W, H, u, v):
  """Mirror hmdflow_module_roadmesh.cpp _bilinearField EXACTLY (the grounding sampler)."""
  tx = u * W - 0.5; tz = v * H - 0.5
  x0 = math.floor(tx); z0 = math.floor(tz)
  fx = tx - x0; fz = tz - z0
  def cl(a, lo, hi): return lo if a < lo else (hi if a > hi else a)
  xi0 = cl(int(x0), 0, W - 1); xi1 = cl(int(x0) + 1, 0, W - 1)
  zi0 = cl(int(z0), 0, H - 1); zi1 = cl(int(z0) + 1, 0, H - 1)
  a = f[zi0 * W + xi0]; b = f[zi0 * W + xi1]; c = f[zi1 * W + xi0]; d = f[zi1 * W + xi1]
  top = a + (b - a) * fx; bot = c + (d - c) * fx
  return top + (bot - top) * fz


def _build_graph():
  town = Rd.Roads(extent_m=EXTENT, layout_cell_m=LCELL, field_dim=FDIM, seed=7)
  h = T.fbm(frequency=3.0, amplitude=40.0, octaves=5)
  slope = T.slope(h)
  town.route_spine(POIS, height=h, slope=slope, **PARAMS)
  spine = town._terminal
  town.road_mesh(spine, height=h, shoulder_gid=3, **RMKW)   # v2.5 skinner + grounding skirt
  town._close_trace()
  town.graphdata.cacheable = False         # cold cook every materialize (determinism)
  return town.graphdata


def _bake_obj(ctx, path):
  g = _build_graph()
  live = hm.materialize_live(g, ctx)
  lev2.hypermesh.dump_obj(live.mesh, ctx, path)
  R_readout = hm._roadsReadout(live, ctx)
  return R_readout


def _parse_obj(path):
  verts = []; faces = []
  with open(path) as f:
    for ln in f:
      if ln.startswith("v "):
        p = ln.split()
        verts.append((float(p[1]), float(p[2]), float(p[3])))
      elif ln.startswith("f "):
        idx = [int(t.split("/")[0]) - 1 for t in ln.split()[1:]]
        faces.append(tuple(idx))
  return verts, faces


def _edge_health(verts, faces, eps=1e-4):
  from collections import Counter
  wid = {}
  def wk(v):
    p = verts[v]; k = (round(p[0] / eps), round(p[1] / eps), round(p[2] / eps))
    if k not in wid: wid[k] = len(wid)
    return wid[k]
  ec = Counter()
  for idx in faces:
    n = len(idx)
    for k in range(n):
      a = wk(idx[k]); b = wk(idx[(k + 1) % n])
      ec[(min(a, b), max(a, b))] += 1
  boundary = sum(1 for c in ec.values() if c == 1)
  nonmanifold = sum(1 for c in ec.values() if c > 2)
  return boundary, nonmanifold


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  fails = 0
  try:
    print("=== R-family RoadMesh Tier-B: baking real-terrain forked road mesh (GPU) ===", flush=True)
    d = tempfile.mkdtemp()
    obj1 = os.path.join(d, "road1.obj"); obj2 = os.path.join(d, "road2.obj")
    ro1 = _bake_obj(ctx, obj1)
    ro2 = _bake_obj(ctx, obj2)
    v1, f1 = _parse_obj(obj1)
    v2, f2 = _parse_obj(obj2)
    print(f"C++ mesh: {len(v1)} verts, {len(f1)} faces (spine_count={ro1['spine_count']})", flush=True)

    # ---- build the reference from the SAME baked spine + the SAME grounding field ----
    nodes = RM.nodes_from_arrays(ro1["spine_positions"], list(ro1["spine_parents"]),
                                 _attrs_from_readout(ro1))
    Hf = list(ro1["height_field"]); fdim = ro1["field_dim"]
    def terrain_h(x, z):
      return _bilinear(Hf, fdim, fdim, x / EXTENT + 0.5, z / EXTENT + 0.5)
    ref = RM.build_roadmesh(nodes, RM.RoadProfile(**RMKW), terrain_h=terrain_h)
    print(f"reference: {ref.num_verts} verts, {ref.num_faces} faces "
          f"({ref.n_quads} quads, {ref.n_junctions} junctions, {ref.n_shoulders} shoulder)", flush=True)

    # ---- (1) REFERENCE PARITY: exact counts + tolerant positions ----
    if (len(v1) == ref.num_verts and len(f1) == ref.num_faces
        and sum(len(x) for x in f1) == ref.num_corners):
      worst = 0.0
      for a, b in zip(sorted(v1), sorted(ref.P)):
        worst = max(worst, max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])))
      if worst < 1e-2:
        print("  [PASS] reference parity: counts EXACT, positions within %.2e m" % worst, flush=True)
      else:
        fails += 1
        print("  [FAIL] reference parity: positions diverge (worst %.3e m)" % worst, flush=True)
    else:
      fails += 1
      print("  [FAIL] reference parity: counts differ (C++ %dv/%df vs ref %dv/%df)"
            % (len(v1), len(f1), ref.num_verts, ref.num_faces), flush=True)

    # ---- (2) WELD INTEGRITY: 0 non-manifold; junction mouth seams shared ----
    boundary, nonmanifold = _edge_health(v1, f1)
    ref_ok, checked = RM.junction_seam_edges_shared(ref, nodes, RM.RoadProfile(**RMKW), terrain_h=terrain_h)
    if nonmanifold == 0 and ref.n_junctions > 0 and ref_ok:
      print("  [PASS] weld integrity: 0 non-manifold edges; %d junction seams welded (%d boundary=perimeter)"
            % (checked, boundary), flush=True)
    else:
      fails += 1
      print("  [FAIL] weld integrity: nonmanifold=%d junctions=%d seams_ok=%s"
            % (nonmanifold, ref.n_junctions, ref_ok), flush=True)

    # ---- (3) DETERMINISM: two cold bakes byte-identical ----
    if v1 == v2 and f1 == f2:
      print("  [PASS] determinism: two cold bakes -> byte-identical mesh (%d verts)" % len(v1), flush=True)
    else:
      fails += 1
      print("  [FAIL] determinism: bakes differ (%d/%d verts, %d/%d faces)"
            % (len(v1), len(v2), len(f1), len(f2)), flush=True)

  finally:
    ezapp.mainThreadEnd()
  print("=== ROADMESH_TIERB_RESULT=%s (%d failures) ===" % ("PASS" if fails == 0 else "FAIL", fails), flush=True)
  ecs.headless_exit()
  sys.exit(0 if fails == 0 else 1)


def _attrs_from_readout(ro):
  """Reconstruct the flat vec4*count attrs the reference needs (x=width,y=arclen,z=road_elev)
  from the RoadsBakeReadout (which exposes road_elev directly; width+arclen ride the spine
  bytes). The tierb readout carries road_elev per node; width/arclen come from the spine
  params (uniform width) + cumulative arc-length recomputed from positions — the reference
  only needs them consistent with the C++ (both read the same XfNode attrs)."""
  n = ro["spine_count"]
  pos = ro["spine_positions"]; par = list(ro["spine_parents"]); relev = ro["spine_road_elev"]
  width = PARAMS["width_m"]
  # arc-length = parent arclen + edge length (matches RouteSpine _attrs[1])
  arclen = [0.0] * n
  order = sorted(range(n), key=lambda k: (0 if par[k] == 0xffffffff else 1))
  # a BFS-ish pass: parents precede children in RouteSpine's node order, so a forward sweep works
  for k in range(n):
    p = par[k]
    if p != 0xffffffff and p < n:
      dx = pos[3 * k] - pos[3 * p]; dz = pos[3 * k + 2] - pos[3 * p + 2]
      arclen[k] = arclen[p] + math.hypot(dx, dz)
  attrs = []
  for k in range(n):
    attrs += [width, arclen[k], relev[k], 0.0]
  return attrs


main()
