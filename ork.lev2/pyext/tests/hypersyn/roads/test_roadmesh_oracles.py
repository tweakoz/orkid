#!/usr/bin/env python3
###############################################################################
# test_roadmesh_oracles.py — STANDALONE (no built C++, no GPU) analytic oracles
# for the R-family RoadMesh v2 SKINNER. Exercises the pure-Python reference
# (ork.hypergraph.dflow.roads.roadmesh_ref) that the C++ RoadMeshModule mirrors
# operation-for-operation.
#
# These run LOCALLY while the C++ builds elsewhere. The C++-vs-reference PARITY +
# meshvet weld gates that NEED the built C++ live in test_roadmesh_gate.py.
#
#   run:  ork.python <this>            (pure python; also plain python3 works)
###############################################################################
import sys, os, math

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS = os.path.abspath(os.path.join(_HERE, "../../../../../obt.project/scripts"))
if _SCRIPTS not in sys.path:
  sys.path.insert(0, _SCRIPTS)

from ork.hypergraph.dflow.roads import roadmesh_ref as RM
from ork.hypergraph.dflow.roads import route_ref as R


def _node(parent, wx, wz, y, width, arclen):
  return RM.RmNode(wx, wz, y, parent, width, arclen)


# ---------------------------------------------------------------------------
def test_straight_quad_strip():
  """A straight 2-node spine => exactly ONE quad (4 verts / 1 face / 4 corners),
  gid=road, +Y normal, U in {0,1}, V from arc-length."""
  W = 6.0
  nodes = [
      _node(-1,   0.0, 0.0, 10.0, W, 0.0),
      _node( 0,   0.0, 40.0, 10.0, W, 40.0),
  ]
  m = RM.build_roadmesh(nodes, RM.RoadProfile(v_meters_per_tile=8.0))
  assert m.num_verts == 4, "straight strip verts=%d != 4" % m.num_verts
  assert m.num_faces == 1, "straight strip faces=%d != 1" % m.num_faces
  assert m.num_corners == 4, "straight strip corners=%d != 4" % m.num_corners
  assert m.n_quads == 1 and m.n_junctions == 0
  assert m.faces[0][1] == RM.GID_ROAD
  ok, bad = RM.face_normals_up(m)
  assert ok, "straight strip face %d normal not +Y" % bad
  # width: the quad spans exactly W across (left rail to right rail)
  xs = [p[0] for p in m.P]
  assert abs((max(xs) - min(xs)) - W) < 1e-6, "strip width %.4f != %.1f" % (max(xs) - min(xs), W)
  assert RM.uv_u_in_unit(m), "U out of [0,1]"
  # 3-node straight chain => 2 quads. The mid THROUGH node shares ONE ring between the
  # two segments (exact meet, no miter gap) => 6 verts (2/node), the mid seam an
  # interior (shared) edge — not a coincident-vert crack.
  nodes3 = [
      _node(-1, 0.0,   0.0, 10.0, W, 0.0),
      _node( 0, 0.0,  40.0, 10.0, W, 40.0),
      _node( 1, 0.0,  80.0, 10.0, W, 80.0),
  ]
  m3 = RM.build_roadmesh(nodes3)
  assert m3.num_faces == 2 and m3.num_verts == 6, "3-node chain: %dv/%df != 6v/2f" % (m3.num_verts, m3.num_faces)
  eh = RM.edge_health(m3)
  assert eh["nonmanifold"] == 0, "straight chain has non-manifold edges: %s" % eh
  assert eh["interior"] >= 1, "straight chain mid-seam not welded (interior edges=%d)" % eh["interior"]
  print("  [ok] straight quad-strip: 1 edge->1 quad/4v/4c; 2 edges->2 quads sharing the mid ring")


def _y_fork(width=6.0, span=60.0):
  """A symmetric Y: P (root, endpoint) -> C (fork) -> A,B (leaf endpoints)."""
  ang = math.radians(35.0)
  ax = -math.sin(ang) * span; az = math.cos(ang) * span
  bx =  math.sin(ang) * span; bz = math.cos(ang) * span
  return [
      _node(-1, 0.0, -span, 10.0, width, 0.0),           # 0 P root
      _node( 0, 0.0,   0.0, 10.0, width, span),          # 1 C fork
      _node( 1,  ax,   az,  10.0, width, span + span),   # 2 A
      _node( 1,  bx,   bz,  10.0, width, span + span),   # 3 B
  ]


def test_y_fork_junction():
  """A symmetric Y-fork: ONE junction patch, a 6-vertex polygon (2*degree, degree=3),
  +Y winding, ear-clip area-sum matches the polygon area, and the mouth seams weld."""
  nodes = _y_fork()
  prof = RM.RoadProfile()
  m = RM.build_roadmesh(nodes, prof)
  assert m.n_junctions == 1, "Y-fork junction count %d != 1" % m.n_junctions
  assert m.n_quads == 3, "Y-fork quad count %d != 3" % m.n_quads
  poly = m.junction_polys[0]
  assert len(poly) == 6, "Y-fork junction polygon has %d verts (expected 2*degree=6)" % len(poly)
  # exact counts: verts = 2*(non-junction=3) + 4*deg(=3) = 6 + 12 = 18; faces = 3+1 = 4
  assert m.num_verts == 18, "Y-fork verts=%d != 18" % m.num_verts
  assert m.num_faces == 4, "Y-fork faces=%d != 4" % m.num_faces
  ok, bad = RM.face_normals_up(m)
  assert ok, "Y-fork face %d normal not +Y" % bad
  ok, njc, worst = RM.junction_area_matches_earclip(m)
  assert ok, "Y-fork ear-clip area-sum mismatch (worst rel err %.3e)" % worst
  print("  [ok] Y-fork: 1 patch (6-gon), 3 quads, 18v/4f, +Y, ear-clip area-sum err=%.2e" % worst)


def test_weld_integrity():
  """No junction mouth seam is a boundary edge (position-welded); no non-manifold
  edges anywhere. The road PERIMETER is legitimately boundary (open ribbon)."""
  nodes = _y_fork()
  prof = RM.RoadProfile()
  m = RM.build_roadmesh(nodes, prof)
  ok, checked = RM.junction_seam_edges_shared(m, nodes, prof)
  assert ok, "a junction mouth seam is a boundary edge (crack) — weld failed"
  eh = RM.edge_health(m)
  assert eh["nonmanifold"] == 0, "non-manifold edges present: %s" % eh
  assert eh["boundary"] > 0, "expected an open-ribbon perimeter boundary"
  print("  [ok] weld integrity: %d mouth seams all shared; 0 non-manifold; %d perimeter boundary edges"
        % (checked, eh["boundary"]))


def test_uv_continuity():
  """V (arc-length UV) monotone along every root->leaf path; U in [0,1]. Across the
  fork weld, the road V carries through (junction patch has its own local chart)."""
  nodes = _y_fork()
  prof = RM.RoadProfile()
  m = RM.build_roadmesh(nodes, prof)
  ok, worst = RM.uv_v_monotonic_paths(m, nodes, prof)
  assert ok, "V not monotone along a path (worst decrease %.4f)" % worst
  assert RM.uv_u_in_unit(m), "U out of [0,1]"
  print("  [ok] UV continuity: V monotone along all paths, U in [0,1]")


def test_multiway_meet():
  """A 4-way meet (root fork with 4 children) auto-satisfies: an 8-gon patch, +Y,
  ear-clip valid. Proves >4-way is a general n-gon, not a refusal."""
  W = 6.0; span = 60.0
  kids = []
  for k in range(4):
    a = math.radians(45.0 + 90.0 * k)
    kids.append(_node(0, math.cos(a) * span, math.sin(a) * span, 10.0, W, span))
  nodes = [_node(-1, 0.0, 0.0, 10.0, W, 0.0)] + kids
  m = RM.build_roadmesh(nodes)
  assert m.n_junctions == 1 and len(m.junction_polys[0]) == 8, \
      "4-way patch polygon %d verts != 8" % len(m.junction_polys[0])
  ok, bad = RM.face_normals_up(m)
  assert ok, "4-way face %d normal not +Y" % bad
  ok, njc, worst = RM.junction_area_matches_earclip(m)
  assert ok, "4-way ear-clip area-sum mismatch"
  print("  [ok] 4-way meet auto-satisfied: 8-gon patch, +Y, ear-clip valid")


def test_self_defense():
  """Documented degenerate refusals: zero-length edge, zero width, near-tangent fork."""
  W = 6.0
  # zero-length edge
  try:
    RM.build_roadmesh([_node(-1, 0.0, 0.0, 0.0, W, 0.0), _node(0, 0.0, 0.0, 0.0, W, 0.0)])
    assert False, "zero-length edge not refused"
  except RuntimeError as e:
    assert "zero-length" in str(e)
  # zero width
  try:
    RM.build_roadmesh([_node(-1, 0.0, 0.0, 0.0, 0.0, 0.0), _node(0, 0.0, 40.0, 0.0, 0.0, 40.0)])
    assert False, "zero width not refused"
  except RuntimeError as e:
    assert "width" in str(e)
  # near-tangent fork: two children leaving the root in NEARLY the same direction
  nodes = [
      _node(-1, 0.0, 0.0, 0.0, W, 0.0),
      _node( 0, 0.2, 60.0, 0.0, W, 60.0),
      _node( 0, -0.2, 60.0, 0.0, W, 60.0),
  ]
  try:
    RM.build_roadmesh(nodes)
    assert False, "near-tangent fork not refused"
  except RuntimeError as e:
    assert "NEAR-TANGENT" in str(e), "wrong refusal: %s" % e
  print("  [ok] self-defense: zero-length, zero-width, near-tangent all refuse loudly")


def test_determinism():
  """Same input => byte-identical mesh (positions + UV + faces)."""
  nodes = _y_fork()
  a = RM.build_roadmesh(nodes)
  b = RM.build_roadmesh(_y_fork())
  assert a.P == b.P and a.UV == b.UV and a.faces == b.faces, "RoadMesh build is non-deterministic"
  print("  [ok] determinism: identical input -> byte-identical mesh (%d verts, %d faces)"
        % (a.num_verts, a.num_faces))


def test_shoulder_grounding():
  """v2.5 GROUNDING oracle: with a terrain sampler wired, RoadMesh grows an outer skirt
  whose LIFT_GROUND ring rides EXACTLY on the terrain (byte-exact), the deck keeps its
  elevation, and the skirt welds crack-free (0 non-manifold; drivable faces +Y)."""
  def terrain_h(x, z):
    return 3.0 * math.sin(x * 0.02) + 2.0 * math.cos(z * 0.015) - 4.0
  nodes = _y_fork()
  prof = RM.RoadProfile(shoulder_m=3.0)
  m = RM.build_roadmesh(nodes, prof, terrain_h=terrain_h)
  assert m.n_shoulders > 0, "no shoulder skirt faces emitted"
  # (1) grounding byte-exact: every grounded outer vert Y == terrain_h(x,z)
  worst = 0.0; nground = 0
  for i, (region, lift, _z, _w) in enumerate(m.CD):
    if region == RM.REGION_SHOULDER and lift == RM.LIFT_GROUND:
      px, py, pz = m.P[i]
      worst = max(worst, abs(py - terrain_h(px, pz))); nground += 1
  assert nground > 0 and worst == 0.0, "shoulder grounding not byte-exact: worst=%.3e over %d" % (worst, nground)
  # (2) deck untouched: region-road verts keep the deck y (10.0 here)
  for i, (region, lift, _z, _w) in enumerate(m.CD):
    if region == RM.REGION_ROAD:
      assert abs(m.P[i][1] - 10.0) < 1e-9, "deck vert %d moved off road_elev" % i
  # (3) weld integrity incl. the new skirt seams
  eh = RM.edge_health(m)
  assert eh["nonmanifold"] == 0, "shoulder mesh has non-manifold edges: %s" % eh
  ok, bad = RM.face_normals_up(m)
  assert ok, "drivable face %d normal not +Y with shoulders" % bad
  ok2, checked = RM.junction_seam_edges_shared(m, nodes, prof)
  assert ok2, "junction mouth seam broke with shoulders"
  print("  [ok] shoulder grounding: %d outer verts EXACT on terrain (0.0 m), 0 non-manifold, %d skirt faces"
        % (nground, m.n_shoulders))


def _chain_nodes(pts, width=6.0):
  nodes = []; arc = 0.0
  for i, (x, z) in enumerate(pts):
    if i > 0:
      arc += math.hypot(x - pts[i - 1][0], z - pts[i - 1][1])
    nodes.append(RM.RmNode(x, z, 10.0, i - 1, width, arc))
  return nodes


def _bank_of(nodes, prof):
  children, _inc = RM._incident(nodes)
  return RM._superelevation(nodes, children, prof), children


def _worst_rollrate(nodes, bank, children, prof):
  """worst per-station |bank change| and the roll-rate ceiling (max_bank*spacing/runoff, using
  the chain's mean spacing — the value the slew limiter enforces)."""
  worst = 0.0; worst_ceil = 0.0
  for chain in RM._chains(nodes, children):
    total = sum(math.hypot(nodes[chain[k]].wx - nodes[chain[k - 1]].wx,
                           nodes[chain[k]].wz - nodes[chain[k - 1]].wz) for k in range(1, len(chain)))
    spacing = total / max(1, len(chain) - 1)
    ceil = prof.max_bank_rad * spacing / prof.bank_runoff_m
    for k in range(1, len(chain)):
      d = abs(bank[chain[k]] - bank[chain[k - 1]])
      if d > worst: worst = d; worst_ceil = ceil
      else: worst_ceil = max(worst_ceil, ceil)
  return worst, worst_ceil


def test_superelevation():
  """SUPERELEVATION oracle: the deck banks into plan-view curves. (1) STRAIGHT spine -> bank
  EXACTLY 0. (2) a sustained circular arc of radius R -> mid-arc bank == max_bank*min(1,ref/R)
  (capped f(curvature)), ramping to 0 on the tangents (roll runoff). (3) roll rate |bank(i+1)-
  bank(i)| <= the ceiling (C1 roll, no kinks). (4) on a real forked spine: |bank| <= max_bank,
  bank == 0 at every junction (flat apron), determinism byte-identical."""
  prof = RM.RoadProfile(max_bank_rad=math.radians(6.0), bank_runoff_m=30.0, bank_ref_radius_m=24.0)
  st = 8.0
  # (1) STRAIGHT -> zero bank
  straight = _chain_nodes([(0.0, k * st) for k in range(40)])
  b_s, ch_s = _bank_of(straight, prof)
  assert max(abs(x) for x in b_s) < 1e-9, "straight spine has nonzero bank"
  # (2)+(3) STRAIGHT lead-in -> ARC radius Rad -> STRAIGHT lead-out
  for Rad in (24.0, 48.0):
    pts = []; z = 0.0
    for _k in range(12):
      pts.append((0.0, z)); z += st
    cz = z; dang = st / Rad; nseg = int(round((math.pi * 0.8) / dang))
    for k in range(1, nseg + 1):
      a = dang * k
      pts.append((-Rad + Rad * math.cos(a), cz + Rad * math.sin(a)))
    ax, az = pts[-1]; a = dang * nseg
    tx, tz = -math.sin(a), math.cos(a)
    for k in range(1, 13):
      pts.append((ax + tx * st * k, az + tz * st * k))
    nodes = _chain_nodes(pts)
    bank, children = _bank_of(nodes, prof)
    mid = 12 + nseg // 2
    expected = prof.max_bank_rad * min(1.0, prof.bank_ref_radius_m / Rad)
    assert abs(abs(bank[mid]) - expected) < 5e-3, \
        "arc Rad=%.0f mid bank %.4f != capped f(curv) %.4f" % (Rad, bank[mid], expected)
    assert abs(bank[3]) < 1e-6, "bank not zero on the approach-tangent straight (Rad=%.0f)" % Rad
    assert abs(bank[-4]) < 1e-6, "bank not zero on the run-out straight (Rad=%.0f)" % Rad
    assert max(abs(x) for x in bank) <= prof.max_bank_rad + 1e-9, "bank exceeds max_bank (Rad=%.0f)" % Rad
    worst, ceil = _worst_rollrate(nodes, bank, children, prof)
    assert worst <= ceil + 1e-9, "roll rate %.5f exceeds ceiling %.5f (Rad=%.0f)" % (worst, ceil, Rad)
  # (4) real forked spine
  p = R.RouteParams(extent_m=768.0, layout_cell_m=32.0, max_grade=1.0, width_m=6.0,
                    min_radius_m=24.0, station_m=8.0)
  dim = 48
  # a diagonal ramp so the router bends (curves to bank) rather than running dead straight
  ramp = [0.0] * (dim * dim)
  for j in range(dim):
    for i in range(dim):
      ramp[j * dim + i] = 0.02 * ((i + 0.5) * (768.0 / dim)) + 0.015 * ((j + 0.5) * (768.0 / dim))
  cg = R.CostGrid(p, ramp, [0.02] * (dim * dim), None, None, dim, dim)
  L = cg.ldim
  pois = [R.cell_center_world(L // 2, 3, L, p.extent_m),
          R.cell_center_world(3, L - 4, L, p.extent_m),
          R.cell_center_world(L - 4, L - 4, L, p.extent_m)]
  spine = R.smooth_spine(cg, R.build_spine(cg, pois))
  nodes = RM.nodes_from_spine(spine)
  bank, children = _bank_of(nodes, prof)
  assert max(abs(x) for x in bank) <= prof.max_bank_rad + 1e-9, "spine bank exceeds max_bank"
  for J in range(len(nodes)):
    if RM._is_junction(children, J):
      assert abs(bank[J]) < 1e-9, "junction node %d banks (apron must stay flat)" % J
  worst, ceil = _worst_rollrate(nodes, bank, children, prof)
  assert worst <= ceil + 1e-9, "spine roll rate %.5f exceeds ceiling %.5f" % (worst, ceil)
  bank2, _ = _bank_of(RM.nodes_from_spine(R.smooth_spine(cg, R.build_spine(cg, pois))), prof)
  assert bank == bank2, "superelevation is non-deterministic"
  print("  [ok] superelevation: straight=0, arc R24->%.1f deg / R48->%.1f deg (capped f(1/R)), roll C1, junctions flat"
        % (math.degrees(prof.max_bank_rad), math.degrees(prof.max_bank_rad * 0.5)))


def test_no_burial_clearance():
  """NO-BURIAL oracle: over a REAL forked spine on undulating terrain, every DECK + APRON vertex
  rides >= clearance_m above the terrain under it (the shoulder-OUTER ring is exempt — it is
  terrain-pinned by design). RouteSpine's vertical smoother floors the deck; RoadMesh floors the
  junction apron; both sample the SAME bilinear height. Also checks the deck is never buried."""
  EXT = 2048.0
  clearance = 0.3
  p = R.RouteParams(extent_m=EXT, layout_cell_m=48.0, max_grade=0.30, w_slope=8.0, w_curv=0.0,
                    w_water=1e9, disch_thresh=1e9, grade_weight=8.0, min_radius_m=24.0,
                    station_m=8.0, vcurve_len_m=120.0, clearance_m=clearance)
  dim = 256
  h = [0.0] * (dim * dim)
  for j in range(dim):
    for i in range(dim):
      wx = (i + 0.5) * (EXT / dim) - EXT * 0.5; wz = (j + 0.5) * (EXT / dim) - EXT * 0.5
      h[j * dim + i] = (12.0 * math.sin(wx * 0.004) + 9.0 * math.cos(wz * 0.006)
                        + 5.0 * math.sin(wx * 0.013 + wz * 0.009) + 80.0)
  cg = R.CostGrid(p, h, [0.02] * (dim * dim), None, None, dim, dim)
  spine = R.smooth_spine(cg, R.build_spine(cg, [(-760.0, -680.0), (720.0, 640.0), (680.0, -720.0)]))
  nodes = RM.nodes_from_spine(spine)
  def terrain_h(x, z):
    return R._bilinear(h, dim, dim, x / EXT + 0.5, z / EXT + 0.5)
  prof = RM.RoadProfile(shoulder_m=3.0, clearance_m=clearance)
  m = RM.build_roadmesh(nodes, prof, terrain_h=terrain_h)
  worst = 1e30; worst_i = -1; nchk = 0
  for i, (region, lift, _z, _w) in enumerate(m.CD):
    if region == RM.REGION_SHOULDER and lift == RM.LIFT_GROUND:
      continue                                   # terrain-pinned outer skirt — exempt
    px, py, pz = m.P[i]
    c = py - terrain_h(px, pz)
    if c < worst: worst = c; worst_i = i
    nchk += 1
  assert worst >= clearance - 1e-4, ("burial: deck/apron vert %d only %.4f m above terrain (< %.2f)"
                                     % (worst_i, worst, clearance))
  print("  [ok] no-burial clearance: worst deck/apron vert %.4f m above terrain over %d verts (floor %.2f)"
        % (worst, nchk, clearance))


def test_on_real_spine():
  """End-to-end on a REAL route_ref spine (flat plane, multi-POI fork) — the skinner
  handles a router-produced forest, not just hand-built fixtures."""
  # authoring guidance: the routing cell should be a few x the road half-width so forks
  # fit a clean apron (cell 32m vs width 5m => hw 2.5m, ratio ~13x). A fine grid + wide
  # road + tight fork is the documented refuse (raise layout_cell_m / narrow width_m).
  p = R.RouteParams(extent_m=768.0, layout_cell_m=32.0, max_grade=1.0, width_m=5.0)
  dim = 48
  cg = R.CostGrid(p, [0.0] * (dim * dim), [0.0] * (dim * dim), None, None, dim, dim)
  L = cg.ldim
  pois = [R.cell_center_world(L // 2, 3, L, p.extent_m),
          R.cell_center_world(3, L - 4, L, p.extent_m),
          R.cell_center_world(L - 4, L - 4, L, p.extent_m)]
  spine = R.build_spine(cg, pois)
  nodes = RM.nodes_from_spine(spine)
  m = RM.build_roadmesh(nodes, RM.RoadProfile())
  ok, bad = RM.face_normals_up(m)
  assert ok, "real-spine face %d normal not +Y" % bad
  eh = RM.edge_health(m)
  assert eh["nonmanifold"] == 0, "real-spine non-manifold edges: %s" % eh
  ok, njc, worst = RM.junction_area_matches_earclip(m)
  assert ok, "real-spine junction area-sum mismatch"
  print("  [ok] real route_ref spine: %d nodes -> %d verts / %d faces (%d quads, %d junctions), clean"
        % (len(nodes), m.num_verts, m.num_faces, m.n_quads, m.n_junctions))


def main():
  print("R-family RoadMesh oracles (standalone, pure-python reference):")
  test_straight_quad_strip()
  test_y_fork_junction()
  test_weld_integrity()
  test_uv_continuity()
  test_multiway_meet()
  test_self_defense()
  test_determinism()
  test_shoulder_grounding()
  test_superelevation()
  test_no_burial_clearance()
  test_on_real_spine()
  print("ALL ROADMESH ORACLES PASSED")


if __name__ == "__main__":
  main()
