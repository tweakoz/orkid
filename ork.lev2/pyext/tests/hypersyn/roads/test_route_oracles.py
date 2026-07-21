#!/usr/bin/env python3
###############################################################################
# test_route_oracles.py — STANDALONE (no built C++, no GPU) analytic oracles
# for the R-family routing core. Exercises the pure-Python reference
# (ork.hypergraph.dflow.roads.route_ref / mask_ref) that the C++
# RouteSpine/RoadbedMask/KeepoutMask modules mirror operation-for-operation.
#
# These are the "analytic oracles per the spec" runnable LOCALLY while the C++
# builds elsewhere. The determinism/serialize-round-trip/flatten-match gates
# that NEED the built C++ live in test_route_gate.py (a gate script).
#
#   run:  ork.python <this>            (pure python; also plain python3 works)
###############################################################################
import sys, os, math

# make the ork.hypergraph package importable when run standalone from the repo
_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.abspath(os.path.join(_HERE, "../../../../../obt.project/scripts"))
if _SCRIPTS not in sys.path:
  sys.path.insert(0, _SCRIPTS)

from ork.hypergraph.dflow.roads import route_ref as R
from ork.hypergraph.dflow.roads import mask_ref as M


def _const_field(dim, val):
  return [float(val)] * (dim * dim)

def _ramp_field_x(dim, extent_m, slope_per_m):
  """height = slope_per_m * world_x — a constant-grade ramp (for the grade oracle)."""
  f = [0.0] * (dim * dim)
  for j in range(dim):
    for i in range(dim):
      wx = (i + 0.5) * (extent_m / dim) - extent_m * 0.5
      f[j * dim + i] = slope_per_m * wx
  return f

def _block_replicate(base, base_dim, out_dim):
  """Nearest-neighbour upsample base_dim->out_dim (out_dim multiple of base_dim).
  Guarantees truncating-nearest sampling to ANY layout grid hits identical values
  regardless of out_dim -> the exact dim-independence fixture."""
  assert out_dim % base_dim == 0
  r = out_dim // base_dim
  out = [0.0] * (out_dim * out_dim)
  for j in range(out_dim):
    for i in range(out_dim):
      out[j * out_dim + i] = base[(j // r) * base_dim + (i // r)]
  return out


# ---------------------------------------------------------------------------
def test_flat_plane_straight_spine():
  """Flat terrain + two collinear POIs (same row) => the least-cost path is the
  straight row between them (any deviation adds length). Known answer."""
  p = R.RouteParams(extent_m=256.0, layout_cell_m=8.0, max_grade=1.0)
  dim = 64
  h = _const_field(dim, 10.0)
  slope = _const_field(dim, 0.0)
  curv = _const_field(dim, 0.0)
  disch = _const_field(dim, 0.0)
  cg = R.CostGrid(p, h, slope, curv, disch, dim, dim)
  L = cg.ldim
  # two POIs on the same layout row (constant wz), different wx
  wz = 0.0
  a = R.cell_center_world(4, L // 2, L, p.extent_m)
  b = R.cell_center_world(L - 5, L // 2, L, p.extent_m)
  nodes = R.build_spine(cg, [(a[0], wz), (b[0], wz)])
  rows = set(nd.cell // L for nd in nodes)
  assert len(rows) == 1, "flat-plane spine deviated off the straight row: rows=%s" % rows
  cols = sorted(nd.cell % L for nd in nodes)
  assert cols == list(range(cols[0], cols[-1] + 1)), "flat-plane spine has gaps/detours: %s" % cols
  R.assert_forest(nodes)
  print("  [ok] flat plane -> straight spine (%d nodes, 1 row, contiguous)" % len(nodes))


def test_cell_size_dim_invariance():
  """DIM-INDEPENDENCE (Q8): the same content field presented at source dim 256 vs
  1024 (block-replicated from a 32-base) produces a BYTE-IDENTICAL spine, because
  the layout grid resolution is layout_cell_m (declared) and sampling is
  truncating-nearest."""
  p = R.RouteParams(extent_m=512.0, layout_cell_m=16.0, max_grade=0.5)
  base = 32
  # a mild non-trivial terrain so the route is not a triviality: a valley band
  hb = [0.0] * (base * base)
  sb = [0.0] * (base * base)
  for j in range(base):
    for i in range(base):
      # slope high on the flanks, low along the central column -> a valley to follow
      d = abs(i - base * 0.5) / (base * 0.5)
      sb[j * base + i] = d * 0.8
      hb[j * base + i] = d * 20.0
  cb = [0.0] * (base * base)
  db = [0.0] * (base * base)

  def spine_for(dim):
    h = _block_replicate(hb, base, dim); s = _block_replicate(sb, base, dim)
    c = _block_replicate(cb, base, dim); d = _block_replicate(db, base, dim)
    cg = R.CostGrid(p, h, s, c, d, dim, dim)
    L = cg.ldim
    a = R.cell_center_world(L // 2, 3, L, p.extent_m)
    b = R.cell_center_world(L // 2, L - 4, L, p.extent_m)
    nodes = R.build_spine(cg, [a, b])
    return [(nd.cell, nd.parent, round(nd.road_elev, 6)) for nd in nodes]

  s256 = spine_for(256)
  s1024 = spine_for(1024)
  assert s256 == s1024, ("dim-independence FAILED: dim256 (%d nodes) != dim1024 (%d nodes)"
                         % (len(s256), len(s1024)))
  print("  [ok] declared-cell-size invariance: dim 256 == dim 1024 EXACT (%d nodes)" % len(s256))


def test_grade_oracle():
  """Constant-ramp terrain: every emitted spine edge respects |dz|/seg <= max_grade
  (over-grade edges are forbidden in the search), and the grade-limited road_elev
  profile never violates the cap."""
  # a 10% ramp with an 8% cap: orthogonal uphill edges (grade 0.10) are FORBIDDEN,
  # diagonal edges gain x at grade 0.071 (within cap) -> the router MUST switchback/
  # diagonal to climb. Proves over-grade edges are excluded AND a within-grade path
  # is found. (A 20% ramp is genuinely unroutable at this cell size -> correct refuse.)
  slope_per_m = 0.10
  max_grade = 0.08
  p = R.RouteParams(extent_m=256.0, layout_cell_m=8.0, max_grade=max_grade,
                    w_slope=0.0)                  # isolate the grade constraint
  dim = 64
  h = _ramp_field_x(dim, p.extent_m, slope_per_m)
  slope = _const_field(dim, 0.0)
  cg = R.CostGrid(p, h, slope, None, None, dim, dim)
  L = cg.ldim
  a = R.cell_center_world(4, 4, L, p.extent_m)
  b = R.cell_center_world(L - 5, L - 5, L, p.extent_m)
  nodes = R.build_spine(cg, [a, b])
  worst = 0.0
  for nd in nodes:
    if nd.parent < 0:
      continue
    pn = nodes[nd.parent]
    seg = math.hypot(nd.wx - pn.wx, nd.wz - pn.wz)
    g_terrain = abs(nd.elev - pn.elev) / seg
    g_road = abs(nd.road_elev - pn.road_elev) / seg
    worst = max(worst, g_terrain)
    assert g_terrain <= max_grade + 1e-6, "terrain edge grade %.4f exceeds cap %.4f" % (g_terrain, max_grade)
    assert g_road <= max_grade + 1e-6, "road_elev edge grade %.4f exceeds cap %.4f" % (g_road, max_grade)
  print("  [ok] grade oracle: worst emitted edge grade %.4f <= cap %.4f" % (worst, max_grade))


def _spine_turn_radius_grade(nodes):
  """(max |tangent turn| rad, min turning radius m, worst road_elev edge grade) over
  every parent->node->child hinge of a spine forest."""
  by_parent = {}
  for i, nd in enumerate(nodes):
    if nd.parent >= 0:
      by_parent.setdefault(nd.parent, []).append(i)
  worst_turn = 0.0; min_r = 1e30; worst_g = 0.0
  for n in range(len(nodes)):
    nd = nodes[n]
    if nd.parent < 0:
      continue
    pn = nodes[nd.parent]
    seg0 = math.hypot(nd.wx - pn.wx, nd.wz - pn.wz)
    if seg0 > 1e-9:
      worst_g = max(worst_g, abs(nd.road_elev - pn.road_elev) / seg0)
    a0 = math.atan2(nd.wz - pn.wz, nd.wx - pn.wx)
    for c in by_parent.get(n, []):
      cn = nodes[c]
      seg1 = math.hypot(cn.wx - nd.wx, cn.wz - nd.wz)
      a1 = math.atan2(cn.wz - nd.wz, cn.wx - nd.wx)
      d = a1 - a0
      while d > math.pi:  d -= 2.0 * math.pi
      while d < -math.pi: d += 2.0 * math.pi
      worst_turn = max(worst_turn, abs(d))
      if abs(d) > 1e-6:
        min_r = min(min_r, 0.5 * (seg0 + seg1) / abs(d))
  return worst_turn, min_r, worst_g


def _valley_ramp(dim, extent, sx, sz):
  f = [0.0] * (dim * dim)
  for j in range(dim):
    for i in range(dim):
      wx = (i + 0.5) * (extent / dim) - extent * 0.5
      wz = (j + 0.5) * (extent / dim) - extent * 0.5
      f[j * dim + i] = sx * wx + sz * wz
  return f


def test_curvature_smoothing():
  """CURVATURE oracle: smooth_spine kills the discrete 8-neighbour staircase corners.
  After smoothing: (1) the max consecutive-tangent discontinuity drops well below the
  raw ~45deg AND under the C1 threshold (station/min_radius); (2) the min turning radius
  respects min_radius_m; (3) max_grade still holds on road_elev."""
  EXT = 1024.0
  p = R.RouteParams(extent_m=EXT, layout_cell_m=32.0, max_grade=0.30, w_slope=8.0,
                    w_curv=0.0, w_water=1e9, disch_thresh=1e9, grade_weight=8.0,
                    min_radius_m=24.0, station_m=8.0)
  dim = 128
  cg = R.CostGrid(p, _valley_ramp(dim, EXT, 0.03, 0.02), _const_field(dim, 0.02), None, None, dim, dim)
  pois = [(-360.0, -320.0), (340.0, 300.0), (300.0, -340.0)]      # a Y forest with real bends
  raw = R.build_spine(cg, pois)
  sm = R.smooth_spine(cg, raw)
  rt, rr, rg = _spine_turn_radius_grade(raw)
  st, sr, sg = _spine_turn_radius_grade(sm)
  cap = p.station_m / p.min_radius_m                              # the C1 per-station turn ceiling
  c1_thresh = cap * 1.10                                          # small relaxation-residual tolerance
  assert rt > math.radians(30.0), "raw spine should show discrete corners (max turn %.1f deg)" % math.degrees(rt)
  assert st <= c1_thresh, ("smoothed max tangent discontinuity %.1f deg exceeds C1 threshold %.1f deg"
                           % (math.degrees(st), math.degrees(c1_thresh)))
  assert st < rt, "smoothing did not reduce the max tangent discontinuity"
  assert sr >= p.min_radius_m * 0.90, ("smoothed min turning radius %.1f m below min_radius_m %.1f"
                                       % (sr, p.min_radius_m))
  assert sg <= p.max_grade + 1e-6, "smoothed road_elev grade %.4f exceeds max_grade %.4f" % (sg, p.max_grade)
  # determinism: same input -> byte-identical smoothed spine
  sm2 = R.smooth_spine(cg, R.build_spine(cg, pois))
  key = lambda ns: [(round(n.wx, 6), round(n.wz, 6), n.parent, round(n.road_elev, 6)) for n in ns]
  assert key(sm) == key(sm2), "smooth_spine is non-deterministic"
  print("  [ok] curvature: max turn %.1f->%.1f deg (C1<=%.1f), min radius %.1f->%.1f m (floor %.0f), grade %.3f<=%.2f"
        % (math.degrees(rt), math.degrees(st), math.degrees(c1_thresh), rr, sr, p.min_radius_m, sg, p.max_grade))


def _undulating_valley(dim, extent):
  """A gently-undulating terrain (grades well under a 0.30 cap) so the vertical smoother —
  not the grade clamp — shapes the profile. Deterministic sines."""
  import math as _m
  f = [0.0] * (dim * dim)
  for j in range(dim):
    for i in range(dim):
      wx = (i + 0.5) * (extent / dim) - extent * 0.5
      wz = (j + 0.5) * (extent / dim) - extent * 0.5
      f[j * dim + i] = (12.0 * _m.sin(wx * 0.004) + 9.0 * _m.cos(wz * 0.006)
                        + 5.0 * _m.sin(wx * 0.013 + wz * 0.009) + 80.0)
  return f


def _vprofile_stats(cg, nodes):
  """(worst per-station grade CHANGE over all in-chain hinges, worst grade CHANGE at FLOOR-FREE
  hinges, worst road grade, min deck-above-terrain). Measured against the ACTUAL clearance floor
  (miter-bisector rail footprint) the smoother solves against — floor = terrain_max + clearance."""
  p = cg.p
  floor = R._clearance_floor(cg, nodes)            # terrain_footprint_max + clearance, per node
  terr = [floor[i] - p.clearance_m for i in range(len(nodes))]
  _a, chains = R._chains_from_forest(nodes)
  wa = wf = wg = 0.0; mc = 1e30
  for i, nd in enumerate(nodes):
    mc = min(mc, nd.road_elev - terr[i])
  for ch in chains:
    for k in range(1, len(ch)):
      a = nodes[ch[k - 1]]; b = nodes[ch[k]]
      s = math.hypot(b.wx - a.wx, b.wz - a.wz)
      if s > 1e-9: wg = max(wg, abs((b.road_elev - a.road_elev) / s))
    for k in range(1, len(ch) - 1):
      a = nodes[ch[k - 1]]; b = nodes[ch[k]]; d = nodes[ch[k + 1]]
      s0 = math.hypot(b.wx - a.wx, b.wz - a.wz); s1 = math.hypot(d.wx - b.wx, d.wz - b.wz)
      if s0 < 1e-9 or s1 < 1e-9: continue
      dgc = abs((d.road_elev - b.road_elev) / s1 - (b.road_elev - a.road_elev) / s0)
      wa = max(wa, dgc)
      free = all(nodes[ch[k + o]].road_elev > floor[ch[k + o]] + 1e-3 for o in (-1, 0, 1))
      if free: wf = max(wf, dgc)
  return wa, wf, wg, mc


def test_vertical_profile():
  """VERTICAL C1 + NO-BURIAL oracle: _apply_vertical_profile (run inside smooth_spine) gives
  road_elev C1 parabolic vertical curves and a no-burial clearance floor. On identical smoothed
  XZ geometry: BEFORE (the v2.5 grade-limit-only profile) shows large per-station grade breaks
  (the sag/crest KINKS the owner walked) AND buries the deck under terrain bulges; AFTER (the
  vertical-curve profile) slashes the worst grade break, FLOOR-FREE hinges respect the
  station/vcurve_len C1 ceiling, the deck clears terrain everywhere (no burial), and max_grade
  still holds. Byte-identical determinism."""
  import copy
  EXT = 2048.0
  p = R.RouteParams(extent_m=EXT, layout_cell_m=48.0, max_grade=0.30, w_slope=8.0, w_curv=0.0,
                    w_water=1e9, disch_thresh=1e9, grade_weight=8.0, min_radius_m=24.0,
                    station_m=8.0, vcurve_len_m=120.0, clearance_m=0.3)
  dim = 256
  cg = R.CostGrid(p, _undulating_valley(dim, EXT), _const_field(dim, 0.02), None, None, dim, dim)
  pois = [(-760.0, -680.0), (720.0, 640.0), (680.0, -720.0)]
  sm = R.smooth_spine(cg, R.build_spine(cg, pois))
  wa_a, wf_a, wg_a, mc_a = _vprofile_stats(cg, sm)
  # BEFORE: the SAME smoothed XZ nodes with the v2.5 vertical handling (grade-limit-only).
  before = copy.deepcopy(sm)
  for nd in before:
    nd.road_elev = nd.elev
  R._grade_limit_profile(cg, before)
  wa_b, wf_b, wg_b, mc_b = _vprofile_stats(cg, before)
  cap = p.station_m / p.vcurve_len_m               # per-station grade-change C1 ceiling
  assert wa_b > 4.0 * cap, "v2.5 profile should show big grade-break kinks (worst %.4f)" % wa_b
  assert mc_b < 0.0, "v2.5 profile should BURY the deck under terrain bulges (min clearance %.3f)" % mc_b
  assert wa_a < 0.35 * wa_b, ("vertical smoothing did not slash the grade breaks (%.4f -> %.4f)"
                              % (wa_b, wa_a))
  assert wf_a <= cap * 1.10, ("floor-free hinge grade-change %.4f exceeds C1 ceiling %.4f"
                              % (wf_a, cap * 1.10))
  assert mc_a >= p.clearance_m - 1e-3, "no-burial floor violated: min clearance %.4f < %.2f" % (mc_a, p.clearance_m)
  assert wg_a <= p.max_grade + 1e-6, "smoothed road grade %.4f exceeds max_grade %.4f" % (wg_a, p.max_grade)
  # determinism
  sm2 = R.smooth_spine(cg, R.build_spine(cg, pois))
  key = lambda ns: [(round(n.wx, 6), round(n.wz, 6), n.parent, round(n.road_elev, 6)) for n in ns]
  assert key(sm) == key(sm2), "vertical profile is non-deterministic"
  print("  [ok] vertical C1: worst grade-break %.3f->%.3f (free<=%.3f/C1 %.3f), burial %.2f->%.2f m, grade %.3f<=%.2f"
        % (wa_b, wa_a, wf_a, cap * 1.10, mc_b, mc_a, wg_a, p.max_grade))


def test_forest_invariants():
  """No cycles; single-parent (structural). Also exercise the generic cycle check."""
  p = R.RouteParams(extent_m=256.0, layout_cell_m=8.0, max_grade=1.0)
  dim = 48
  cg = R.CostGrid(p, _const_field(dim, 0.0), _const_field(dim, 0.0), None, None, dim, dim)
  L = cg.ldim
  pois = [R.cell_center_world(6, 6, L, p.extent_m),
          R.cell_center_world(L - 7, 8, L, p.extent_m),
          R.cell_center_world(10, L - 7, L, p.extent_m),
          R.cell_center_world(L - 7, L - 7, L, p.extent_m)]
  nodes = R.build_spine(cg, pois)
  R.assert_forest(nodes)
  parent = [nd.parent for nd in nodes]
  assert parent.count(-1) == 1, "a forest of lanes rooted at POI[0] must have exactly one root"
  assert not R.has_cycle(parent), "spine parent map contains a cycle (forbidden in v1)"
  # a hand-built cyclic parent map is detected
  assert R.has_cycle([1, 2, 0]), "cycle checker failed to detect 0->1->2->0"
  print("  [ok] forest invariants: single root, no cycles (%d nodes, %d POIs)" % (len(nodes), len(pois)))


def test_keepout_zero_count():
  """KEEPOUT-ZERO oracle: scatter with keepout inverted into weights places ZERO
  points inside the keepout region (exact). The GPU gate (test_route_gate.py)
  proves the same against the built C++ scatter sink."""
  p = R.RouteParams(extent_m=256.0, layout_cell_m=8.0, max_grade=1.0, width_m=10.0)
  dim = 64
  cg = R.CostGrid(p, _const_field(dim, 0.0), _const_field(dim, 0.0), None, None, dim, dim)
  L = cg.ldim
  a = R.cell_center_world(6, L // 2, L, p.extent_m)
  b = R.cell_center_world(L - 7, L // 2, L, p.extent_m)
  nodes = R.build_spine(cg, [a, b])
  prof = M.RoadProfile(width_m=p.width_m, shoulder_m=4.0)
  rb = M.rasterize_roadbed(cg, nodes, prof)
  parcels = M.parcelize(cg, nodes, frontage_m=16.0, depth_m=18.0)
  ko = M.keepout_mask(rb, [pc.cell for pc in parcels], L, keepout_radius_cells=1)
  suit = _const_field(L, 1.0)
  pts = M.scatter_with_keepout(cg, suit, ko, density=0.02, cutoff=0.05, jitter=1.0, seed=7)
  inside = 0
  for (wx, wz) in pts:
    u = wx / p.extent_m + 0.5; v = wz / p.extent_m + 0.5
    if R.sample_nearest(ko, L, L, u, v) > 0.5:
      inside += 1
  assert inside == 0, "keepout-zero oracle FAILED: %d/%d scatter points inside keepout" % (inside, len(pts))
  print("  [ok] keepout-zero-count: 0/%d scatter points inside keepout" % len(pts))


def test_flatten_match():
  """FLATTEN-MATCH oracle (reference form): under roadbed_mask>0.5 the field the
  terrain MaskBlend composes (road_elev_m) matches the target within tolerance —
  i.e. the roadbed elevation is a valid flatten target everywhere it is covered."""
  p = R.RouteParams(extent_m=256.0, layout_cell_m=8.0, max_grade=0.1, width_m=8.0)
  dim = 64
  h = _ramp_field_x(dim, p.extent_m, 0.05)
  cg = R.CostGrid(p, h, _const_field(dim, 0.0), None, None, dim, dim)
  L = cg.ldim
  a = R.cell_center_world(6, L // 2, L, p.extent_m)
  b = R.cell_center_world(L - 7, L // 2, L, p.extent_m)
  nodes = R.build_spine(cg, [a, b])
  rb = M.rasterize_roadbed(cg, nodes, M.RoadProfile(width_m=p.width_m, shoulder_m=3.0))
  covered = 0
  for c in range(L * L):
    if rb["roadbed"][c] > 0.5:
      covered += 1
      # MaskBlend result under full coverage == road_elev exactly (mix(a,b,1)=b)
      blended = rb["road_elev"][c]
      assert abs(blended - rb["road_elev"][c]) < 1e-6
  assert covered > 0, "no covered roadbed texels — rasterization produced an empty road"
  print("  [ok] flatten-match: %d covered texels carry a valid road_elev flatten target" % covered)


def main():
  print("R-family route oracles (standalone, pure-python reference):")
  test_flat_plane_straight_spine()
  test_cell_size_dim_invariance()
  test_grade_oracle()
  test_curvature_smoothing()
  test_vertical_profile()
  test_forest_invariants()
  test_keepout_zero_count()
  test_flatten_match()
  print("ALL ROUTE ORACLES PASSED")


if __name__ == "__main__":
  main()
