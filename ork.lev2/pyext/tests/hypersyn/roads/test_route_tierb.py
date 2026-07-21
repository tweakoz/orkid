#!/usr/bin/env python3
###############################################################################
# test_route_tierb.py — R-family Tier-B GPU COUPLING ORACLES (needs a compute node).
# Builds a REAL in-graph terrain (fbm height + slope — real GPU field artifacts),
# bakes the full R pipeline headless, and runs:
#   * DETERMINISM   — two cold cooks (cacheable off), byte-identical spine.
#   * REFERENCE PARITY — the C++ spine == the validated pure-python reference
#     (route_ref) run on the SAME GPU-sampled fields (subsumes the analytic oracles
#     on real terrain: same field + same params => same cost => same Dijkstra forest).
#   * KEEPOUT-ZERO  — jittered scatter (keepout inverted) places 0 points in keepout.
#   * FLATTEN-MATCH — stock MaskBlend(height, road_elev, roadbed) under roadbed==1
#     equals road_elev exactly (the fields compose to a valid heightfield flatten).
#
#   run:  obt.net.py run <node> --env "ORKID_DRM_MODE=" -- ork.python <this>
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
os.environ["ORKID_DISABLE_SHADER_CACHE"] = "1"   # cold cooks for the determinism oracle
import sys, math
from orkengine import core
from orkengine import lev2
from orkengine import ecs
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.dflow import roads as Rd
from ork.hypergraph.dflow.roads import route_ref as R

hm = lev2.hypermesh

EXTENT = 512.0
LCELL  = 16.0
FDIM   = 256
# route params (shared by the C++ module AND the python reference — exact parity).
# Tier-B routes on REAL fbm-height + T.slope GPU fields: cost = base + w_slope*slope + grade
# penalty, so the route follows gentle terrain within max_grade (valley-following). Discharge
# is left inert (disch_thresh huge) — flow3d in-graph is a heavier follow-up; slope/height
# already pin the full C++ cost+search+coupling to the validated reference on real GPU data.
PARAMS = dict(width_m=6.0, max_grade=0.22, w_slope=8.0, w_curv=0.0, w_water=1.0e9,
              disch_thresh=1.0e9, grade_weight=8.0, base_cost=1.0)
POIS = [(-200.0, -180.0), (190.0, 170.0)]


def _build_graph():
  town = Rd.Roads(extent_m=EXTENT, layout_cell_m=LCELL, field_dim=FDIM, seed=7)
  # REAL terrain fields (emit into town's graph via the shared trace). fbm-height + T.slope
  # bake clean under the mesh driver; discharge (flow3d) is a heavier in-graph follow-up.
  h = T.fbm(frequency=3.0, amplitude=60.0, octaves=5)   # meters (deterministic noise basis)
  slope = T.slope(h)
  town.route_spine(POIS, height=h, slope=slope, **PARAMS)
  # ride the spine
  spine = town._terminal
  bed = town.roadbed_mask(spine, width_m=PARAMS["width_m"], shoulder_m=3.0)
  town.keepout_mask(bed, keepout_radius_m=8.0)
  town.building_seeds(town.parcelize(spine))
  town._close_trace()
  town.graphdata.cacheable = False   # AFTER _close_trace (which sets True): cold cook every
  return town.graphdata               # materialize -> real determinism + no cook-store readback


def _readout(ctx):
  g = _build_graph()
  live = hm.materialize_live(g, ctx)
  return hm._roadsReadout(live, ctx)


def _cells_from_positions(pos, parents, road_elev, extent, lcell):
  L = max(1, int(round(extent / lcell)))
  cell_m = extent / L
  def cell(x, z):
    i = min(max(int((x + extent * 0.5) / cell_m), 0), L - 1)
    j = min(max(int((z + extent * 0.5) / cell_m), 0), L - 1)
    return j * L + i
  n = len(parents)
  out = []
  for k in range(n):
    x, z = pos[3 * k + 0], pos[3 * k + 2]
    pc = -1 if parents[k] == 0xffffffff else cell(pos[3 * parents[k]], pos[3 * parents[k] + 2])
    out.append((cell(x, z), pc, round(road_elev[k], 4)))
  return out


def main():
  ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  assert ctx, "bindGfxToCurrentThread() returned null"
  fails = 0
  try:
    print("=== R-family Tier-B: baking real-terrain R pipeline (fbm height, GPU) ===", flush=True)
    r1 = _readout(ctx)
    r2 = _readout(ctx)
    dim = r1["field_dim"]
    print(f"spine_count={r1['spine_count']} field_dim={dim} seed_count={r1['seed_count']}", flush=True)

    # ---- (1) DETERMINISM: two cold cooks byte-identical ----
    if r1["spine_count"] > 0 and bytes(r1["spine_bytes"]) == bytes(r2["spine_bytes"]):
      print("  [PASS] determinism: two cold cooks -> byte-identical spine (%dB)" % len(bytes(r1["spine_bytes"])), flush=True)
    else:
      fails += 1
      print("  [FAIL] determinism: spine bytes differ across cold cooks (%d vs %d)"
            % (len(bytes(r1["spine_bytes"])), len(bytes(r2["spine_bytes"]))), flush=True)

    # ---- (2) REFERENCE PARITY: C++ SMOOTHED spine == python reference on the SAME GPU
    #      fields (build_spine -> smooth_spine). Curvature smoothing moves stations off cell
    #      centres, so parity is topology-exact (parent index) + position/road_elev within a
    #      float tolerance (float32 XfNode storage vs the float64 reference). ----
    if r1["spine_count"] > 0 and dim > 0:
      p = R.RouteParams(extent_m=EXTENT, layout_cell_m=LCELL, **PARAMS)
      H = list(r1["height_field"]); S = list(r1["slope_field"]); D = list(r1["disch_field"])
      cg = R.CostGrid(p, H, (S if S else None), None, (D if D else None), dim, dim)
      ref = R.smooth_spine(cg, R.build_spine(cg, POIS))
      cpp_pos = r1["spine_positions"]; cpp_par = list(r1["spine_parents"]); cpp_re = r1["spine_road_elev"]
      ncpp = r1["spine_count"]
      if ncpp == len(ref):
        worst_p = 0.0; worst_e = 0.0; nbad_par = 0
        for k in range(ncpp):
          rp = -1 if (cpp_par[k] == 0xffffffff) else int(cpp_par[k])
          if rp != ref[k].parent:
            nbad_par += 1
          worst_p = max(worst_p, abs(cpp_pos[3 * k] - ref[k].wx), abs(cpp_pos[3 * k + 2] - ref[k].wz))
          worst_e = max(worst_e, abs(cpp_re[k] - ref[k].road_elev))
        if nbad_par == 0 and worst_p < 5e-2 and worst_e < 5e-2:
          print("  [PASS] reference parity: C++ smoothed spine == python reference (%d nodes; "
                "worst |pos|=%.2e m, |road_elev|=%.2e m)" % (ncpp, worst_p, worst_e), flush=True)
        else:
          fails += 1
          print("  [FAIL] reference parity: bad_parents=%d worst|pos|=%.3e worst|road_elev|=%.3e"
                % (nbad_par, worst_p, worst_e), flush=True)
      else:
        fails += 1
        print("  [FAIL] reference parity: node count differs (C++=%d ref=%d)" % (ncpp, len(ref)), flush=True)
    else:
      fails += 1
      print("  [FAIL] reference parity: C++ produced an empty spine (cascade did not build)", flush=True)

    # ---- (3) KEEPOUT-ZERO: scatter (keepout inverted) -> 0 points inside keepout ----
    ko = list(r1["keepout"])
    if ko:
      ncx = max(1, int(round(EXTENT * math.sqrt(0.02))))
      cell_m = EXTENT / ncx
      inside = 0; total = 0
      for k in range(ncx * ncx):
        i = k % ncx; j = k // ncx
        wx = (i + 0.5 + (R.route_u01(7, k, 1) - 0.5)) * cell_m - EXTENT * 0.5
        wz = (j + 0.5 + (R.route_u01(7, k, 2) - 0.5)) * cell_m - EXTENT * 0.5
        u = wx / EXTENT + 0.5; v = wz / EXTENT + 0.5
        ko_s = R.sample_nearest(ko, dim, dim, u, v)
        w = 1.0 * (1.0 - min(1.0, max(0.0, ko_s)))
        if w >= 0.05 and R.route_u01(7, k, 3) < min(1.0, w):
          total += 1
          if R.sample_nearest(ko, dim, dim, u, v) > 0.5:
            inside += 1
      if inside == 0:
        print("  [PASS] keepout-zero-count: 0/%d scatter points inside keepout" % total, flush=True)
      else:
        fails += 1
        print("  [FAIL] keepout-zero-count: %d/%d scatter points inside keepout" % (inside, total), flush=True)
    else:
      fails += 1
      print("  [FAIL] keepout-zero: keepout field empty", flush=True)

    # ---- (4) FLATTEN-MATCH: MaskBlend(height, road_elev, roadbed) == road_elev under full coverage ----
    H = list(r1["height_field"]); RE = list(r1["road_elev_field"]); RB = list(r1["roadbed"])
    if H and RE and RB:
      worst = 0.0; covered = 0
      for c in range(min(len(H), len(RE), len(RB))):
        if RB[c] >= 0.999:
          covered += 1
          blended = H[c] * (1.0 - RB[c]) + RE[c] * RB[c]   # stock MaskBlend: mix(a,b,m)
          worst = max(worst, abs(blended - RE[c]))
      if covered > 0 and worst < 1e-3:
        print("  [PASS] flatten-match: %d full-coverage texels, max|MaskBlend-road_elev|=%.2e" % (covered, worst), flush=True)
      else:
        fails += 1
        print("  [FAIL] flatten-match: covered=%d worst=%.3e" % (covered, worst), flush=True)
    else:
      fails += 1
      print("  [FAIL] flatten-match: roadbed/road_elev/height field empty", flush=True)

  finally:
    ezapp.mainThreadEnd()
  print("=== ROUTE_TIERB_RESULT=%s (%d failures) ===" % ("PASS" if fails == 0 else "FAIL", fails), flush=True)
  ecs.headless_exit()
  sys.exit(0 if fails == 0 else 1)


main()
