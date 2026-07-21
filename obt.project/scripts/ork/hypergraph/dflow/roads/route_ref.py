###############################################################################
# route_ref.py — the PURE-PYTHON reference implementation of the R-family
# RouteSpine routing core. This file is the executable spec that the C++
# RouteSpineModuleInst (hmdflow_module_routespine.cpp) mirrors OPERATION-FOR-
# OPERATION — the scatter.py <-> hfdflow_scatter.cpp precedent. When the C++
# algorithm changes, this file changes with it and the parity gate pins them.
#
# INVARIANTS this reference establishes (asserted by test_route_oracles.py):
#   * DIM-INDEPENDENCE (Q8): the routing grid is a DECLARED cost grid at
#     layout_cell_m. layout_dim = round(extent_m/layout_cell_m) depends ONLY on
#     declared params — never on the terrain field's runtime dim. Source fields
#     are sampled with TRUNCATING NEAREST-TEXEL (the scatter-parity precedent,
#     hfdflow_scatter.cpp:82-90). Two source dims that resample identically to
#     the layout grid produce a byte-identical spine.
#   * DETERMINISM: no floating RNG in the search. Dijkstra with a lexicographic
#     (dist, cell_index) tie-break and a lower-predecessor-index rule on equal
#     tentative distance. Same file + terrain artifacts => same spine anywhere.
#   * FOREST (Q2): the spine is a single-parent predecessor tree (a forest of
#     lanes). Cycles are refused loudly — they are the v2 currency.
#   * SLOPE (max_grade): an edge whose |dz|/world_dist exceeds max_grade is
#     FORBIDDEN, so every emitted segment respects the grade cap by construction.
#     v1 routes AROUND water (discharge above threshold is forbidden terrain).
###############################################################################

import math

# ---------------------------------------------------------------------------
# stateless counter-hash RNG (the SAME _mix64 chain as hfdflow_scatter.cpp:35-50
# and scatter.py). Used ONLY for jittered frontage spacing downstream (parcels /
# spurs); the core route search is RNG-free. Kept here so every R-family random
# draw shares one cross-machine-stable stream.
# ---------------------------------------------------------------------------
_U64 = (1 << 64) - 1

def _mix64(x):
  x &= _U64
  x ^= x >> 30; x = (x * 0xBF58476D1CE4E5B9) & _U64
  x ^= x >> 27; x = (x * 0x94D049BB133111EB) & _U64
  x ^= x >> 31
  return x & _U64

def route_hash(seed, idx, stream):
  return _mix64(_mix64(_mix64((seed ^ 0x9E3779B97F4A7C15) & _U64) ^ (idx & _U64)) ^ (stream & _U64))

def route_u01(seed, idx, stream):
  return (route_hash(seed, idx, stream) >> 11) * (1.0 / 9007199254740992.0)  # 2^-53


# ---------------------------------------------------------------------------
# layout grid framing (declared, dim-independent)
# ---------------------------------------------------------------------------
def layout_dim(extent_m, layout_cell_m):
  """The cost-grid resolution. Depends ONLY on declared params (NOT terrain dim)."""
  return max(1, int(round(float(extent_m) / float(layout_cell_m))))

def cell_center_world(i, j, ldim, extent_m):
  """World XZ of layout cell (i,j) center — the scatter cell convention
  (hfdflow_scatter.cpp:182-183): origin-centered field, cell size = extent/ldim."""
  cell_m = float(extent_m) / float(ldim)
  wx = (i + 0.5) * cell_m - extent_m * 0.5
  wz = (j + 0.5) * cell_m - extent_m * 0.5
  return wx, wz

def sample_nearest(field, w, h, u, v):
  """Truncating nearest-texel sample of a flat HxW float field (channel 0).
  Mirrors hfdflow_scatter.cpp:_sampleNearest EXACTLY (int() truncates, u>=0)."""
  xi = int(u * w); yi = int(v * h)
  if xi < 0: xi = 0
  elif xi > w - 1: xi = w - 1
  if yi < 0: yi = 0
  elif yi > h - 1: yi = h - 1
  return field[yi * w + xi]


def _bilinear(field, W, H, u, v):
  """Bilinear sample of a flat W*H channel-0 field at uv. Mirrors the RoadMesh grounding
  sampler (roadmesh_ref._bilinear / hmdflow_module_roadmesh.cpp _bilinearField) EXACTLY so
  the clearance floor rides the SAME terrain the shoulder grounds on."""
  if W < 1 or H < 1 or not field:
    return 0.0
  tx = u * W - 0.5; tz = v * H - 0.5
  x0 = math.floor(tx); z0 = math.floor(tz)
  fx = tx - x0; fz = tz - z0
  def cl(a, lo, hi): return lo if a < lo else (hi if a > hi else a)
  xi0 = cl(int(x0), 0, W - 1); xi1 = cl(int(x0) + 1, 0, W - 1)
  zi0 = cl(int(z0), 0, H - 1); zi1 = cl(int(z0) + 1, 0, H - 1)
  a = field[zi0 * W + xi0]; b = field[zi0 * W + xi1]
  c = field[zi1 * W + xi0]; d = field[zi1 * W + xi1]
  top = a + (b - a) * fx; bot = c + (d - c) * fx
  return top + (bot - top) * fz


# ---------------------------------------------------------------------------
# cost field — f(slope, curvature, discharge) at LAYOUT resolution (spec §0)
# ---------------------------------------------------------------------------
class RouteParams:
  """The reflected RouteSpineModuleData scalar env (A8: tweakables are params).
  Kept in ONE place so the C++ describeX + the DSL kwargs + this reference stay
  aligned. All values are natural units (meters / dimensionless / [0,1])."""
  def __init__(self, extent_m=1024.0, layout_cell_m=8.0, width_m=6.0,
               max_grade=0.12, w_slope=6.0, w_curv=3.0, w_water=1.0e4,
               disch_thresh=0.55, grade_weight=4.0, base_cost=1.0, seed=1,
               min_radius_m=24.0, station_m=8.0, vcurve_len_m=120.0, clearance_m=0.3):
    self.extent_m      = float(extent_m)
    self.layout_cell_m = float(layout_cell_m)
    self.width_m       = float(width_m)
    self.max_grade     = float(max_grade)     # tan(theta) cap on the road grade
    self.w_slope       = float(w_slope)       # per-cell steepness penalty weight
    self.w_curv        = float(w_curv)        # ridge (convex-up) penalty weight
    self.w_water       = float(w_water)       # discharge>thresh -> ~forbidden cost
    self.disch_thresh  = float(disch_thresh)  # normalized discharge => water
    self.grade_weight  = float(grade_weight)  # edge grade penalty multiplier
    self.base_cost     = float(base_cost)
    self.seed          = int(seed)
    self.min_radius_m  = float(min_radius_m)  # curvature floor: the smoothed spine never turns
                                              #   tighter than this radius (meters) — the fillet param
    self.station_m     = float(station_m)     # resampled spine station spacing (meters) along arc-len
    self.vcurve_len_m  = float(vcurve_len_m)  # VERTICAL curve length: horizontal metres to absorb a UNIT
                                              #   (1.0) grade change (a road-eng K-value). The per-station
                                              #   grade-change ceiling is station_m/vcurve_len_m -> a grade
                                              #   change dg spreads over dg*vcurve_len_m of arc (constant
                                              #   rate of grade change == a parabolic sag/crest curve).
    self.clearance_m   = float(clearance_m)   # min metres the deck rides ABOVE the terrain under its
                                              #   footprint (no-burial floor; the vertical smoother solves
                                              #   road_elev >= terrain+clearance so the deck never sinks).


class CostGrid:
  """Per-cell node cost + elevation + water mask at the declared layout grid,
  resampled from the terrain fields via truncating nearest-texel."""
  def __init__(self, p, height, normal_slope, curvature, discharge, fw, fh):
    # height/normal_slope/curvature/discharge: flat fw*fh float arrays (channel 0).
    #   normal_slope = terrain slope in [0,1+] (gradient magnitude proxy from `normal`)
    #   curvature    = signed (convex-up>0 ridge, concave<0 valley)
    #   discharge    = normalized flow_discharge in [0,1]
    self.p = p
    # keep the RAW height field (+ its dims) so the no-burial clearance floor bilinear-samples
    # the SAME field the RoadMesh shoulder grounds on (byte-exact cross-module terrain).
    self.height = height
    self.fw = int(fw); self.fh = int(fh)
    self.ldim = layout_dim(p.extent_m, p.layout_cell_m)
    L = self.ldim
    self.cell_m = p.extent_m / L
    self.cost  = [0.0] * (L * L)
    self.elev  = [0.0] * (L * L)
    self.water = [False] * (L * L)
    for j in range(L):
      for i in range(L):
        wx, wz = cell_center_world(i, j, L, p.extent_m)
        u = wx / p.extent_m + 0.5
        v = wz / p.extent_m + 0.5
        slope = sample_nearest(normal_slope, fw, fh, u, v) if normal_slope else 0.0
        curv  = sample_nearest(curvature,    fw, fh, u, v) if curvature   else 0.0
        disch = sample_nearest(discharge,    fw, fh, u, v) if discharge   else 0.0
        elev  = sample_nearest(height,       fw, fh, u, v) if height      else 0.0
        c = j * L + i
        self.elev[c] = elev
        water = disch >= p.disch_thresh
        self.water[c] = water
        cost = (p.base_cost
                + p.w_slope * max(0.0, slope)
                + p.w_curv  * max(0.0, curv))     # ridge penalty; valleys (curv<0) neutral
        if water:
          cost += p.w_water                       # v1 routes AROUND water
        self.cost[c] = cost

  def cell_of_world(self, wx, wz):
    """Snap a world XZ to a layout cell index (clamped)."""
    L = self.ldim
    i = int((wx + self.p.extent_m * 0.5) / self.cell_m)
    j = int((wz + self.p.extent_m * 0.5) / self.cell_m)
    i = min(max(i, 0), L - 1); j = min(max(j, 0), L - 1)
    return j * L + i

  def bilinear_height(self, wx, wz):
    """Bilinear terrain height under world XZ from the RAW height field — the SAME sampler
    the RoadMesh shoulder grounds on (roadmesh_ref._bilinear / _bilinearField). Empty field
    (unconnected Height) -> 0.0. Used by the no-burial clearance floor."""
    if not self.height or self.fw < 1 or self.fh < 1:
      return 0.0
    u = wx / self.p.extent_m + 0.5
    v = wz / self.p.extent_m + 0.5
    return _bilinear(self.height, self.fw, self.fh, u, v)


# 8-neighbourhood, fixed deterministic order (dx,dy) — the C++ mirrors this order.
_NEI = ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1))
_SQRT2 = math.sqrt(2.0)


def _edge_cost(cg, a, b, ddiag):
  """Undirected edge cost a<->b, or None if FORBIDDEN (water / over-grade)."""
  p = cg.p
  if cg.water[a] or cg.water[b]:
    return None
  world_dist = cg.cell_m * (_SQRT2 if ddiag else 1.0)
  grade = abs(cg.elev[b] - cg.elev[a]) / world_dist
  if grade > p.max_grade:
    return None                                    # over-grade edge is impassable
  return 0.5 * (cg.cost[a] + cg.cost[b]) * world_dist * (1.0 + p.grade_weight * grade)


def dijkstra_tree(cg, root):
  """Least-cost predecessor tree from `root` over all non-water cells.
  Deterministic: heap ordered by (dist, cell); equal-tentative ties adopt the
  LOWER predecessor cell index. Returns (dist[], parent[]) with parent[root]=-1
  and parent[c]=-1 for unreached cells (dist=inf)."""
  import heapq
  L = cg.ldim
  N = L * L
  INF = float('inf')
  dist = [INF] * N
  parent = [-1] * N
  if cg.water[root]:
    raise RuntimeError("RouteSpine: POI cell %d is INSIDE water (discharge>=thresh) — "
                       "cannot root a route there (satisfy: move the POI or raise disch_thresh)" % root)
  dist[root] = 0.0
  heap = [(0.0, root)]
  done = [False] * N
  while heap:
    d, c = heapq.heappop(heap)
    if done[c]:
      continue
    done[c] = True
    ci = c % L; cj = c // L
    for (dx, dy) in _NEI:
      ni = ci + dx; nj = cj + dy
      if ni < 0 or nj < 0 or ni >= L or nj >= L:
        continue
      nc = nj * L + ni
      if done[nc]:
        continue
      w = _edge_cost(cg, c, nc, dx != 0 and dy != 0)
      if w is None:
        continue
      nd = d + w
      if nd < dist[nc] - 1e-12:
        dist[nc] = nd; parent[nc] = c
        heapq.heappush(heap, (nd, nc))
      elif abs(nd - dist[nc]) <= 1e-12 and parent[nc] != -1 and c < parent[nc]:
        parent[nc] = c                             # equal-dist tie-break: lower predecessor
  return dist, parent


class SpineNode:
  __slots__ = ("cell", "parent", "wx", "wz", "elev", "road_elev", "width", "arclen",
               "hx", "hz", "is_poi")
  def __init__(self, cell, wx, wz, elev, width):
    self.cell = cell; self.parent = -1
    self.wx = wx; self.wz = wz; self.elev = elev; self.road_elev = elev
    self.width = width; self.arclen = 0.0
    self.hx = 0.0; self.hz = 1.0; self.is_poi = False


def build_spine(cg, poi_worlds):
  """Extract the spine FOREST: Dijkstra from POI[0]; union every other POI's
  least-cost predecessor chain back to the root. Result is a single-parent tree
  (a forest of lanes). Refuses loudly on an unreachable POI. Returns a list of
  SpineNode in deterministic node order (root-first BFS)."""
  if len(poi_worlds) < 2:
    raise RuntimeError("RouteSpine: need >= 2 POIs to route a spine (got %d)" % len(poi_worlds))
  poi_cells = [cg.cell_of_world(wx, wz) for (wx, wz) in poi_worlds]
  root = poi_cells[0]
  dist, parent = dijkstra_tree(cg, root)

  # collect the union of predecessor chains from each POI to the root
  on_spine = set()
  for k, pc in enumerate(poi_cells):
    if dist[pc] == float('inf'):
      raise RuntimeError("RouteSpine: POI #%d (cell %d) UNREACHABLE from root under the cost "
                         "field (blocked by water/grade) — refuse (v1 has no bridges/cuts)" % (k, pc))
    c = pc
    while c != -1:
      if c in on_spine:
        break
      on_spine.add(c)
      c = parent[c]
  on_spine.add(root)

  # deterministic node ordering: BFS from root over the induced spine subtree,
  # children visited in ascending cell index (so node indices are machine-stable).
  L = cg.ldim
  children = {}
  for c in on_spine:
    pc = parent[c]
    if pc != -1 and pc in on_spine:
      children.setdefault(pc, []).append(c)
  for pc in children:
    children[pc].sort()
  order = []
  cell_to_node = {}
  from collections import deque
  q = deque([root]); cell_to_node[root] = 0; order.append(root)
  while q:
    c = q.popleft()
    for ch in children.get(c, []):
      if ch not in cell_to_node:
        cell_to_node[ch] = len(order); order.append(ch); q.append(ch)

  # build nodes + parent links + frames (heading = parent->node travel direction)
  nodes = []
  for c in order:
    ci = c % L; cj = c // L
    wx, wz = cell_center_world(ci, cj, L, cg.p.extent_m)
    n = SpineNode(c, wx, wz, cg.elev[c], cg.p.width_m)
    nodes.append(n)
  poi_set = set(poi_cells)
  for ni, c in enumerate(order):
    n = nodes[ni]
    n.is_poi = c in poi_set
    pc = parent[c]
    if pc != -1 and pc in cell_to_node:
      pn = cell_to_node[pc]
      n.parent = pn
      p = nodes[pn]
      hx = n.wx - p.wx; hz = n.wz - p.wz
      m = math.hypot(hx, hz)
      if m > 1e-9:
        n.hx = hx / m; n.hz = hz / m
      seg = math.hypot(n.wx - p.wx, n.wz - p.wz)
      n.arclen = p.arclen + seg
  # root heading = toward its first child (if any)
  if len(nodes) > 1 and nodes[0].parent == -1:
    kids = children.get(order[0], [])
    if kids:
      k0 = nodes[cell_to_node[kids[0]]]
      hx = k0.wx - nodes[0].wx; hz = k0.wz - nodes[0].wz
      m = math.hypot(hx, hz)
      if m > 1e-9:
        nodes[0].hx = hx / m; nodes[0].hz = hz / m

  _grade_limit_profile(cg, nodes)
  assert_forest(nodes)
  return nodes


def _grade_limit_profile(cg, nodes):
  """road_elev_m = a grade-limited elevation profile along arc length. A forward
  pass from the root clamps each node's road elevation so |dz|/seg <= max_grade,
  then a backward pass symmetrizes — so the roadbed the terrain re-bake flattens
  to never violates the grade cap even where the raw terrain does."""
  mg = cg.p.max_grade
  for ni in range(1, len(nodes)):
    n = nodes[ni]
    if n.parent < 0:
      continue
    p = nodes[n.parent]
    seg = math.hypot(n.wx - p.wx, n.wz - p.wz)
    lim = mg * seg
    dz = n.elev - p.road_elev
    if dz > lim: n.road_elev = p.road_elev + lim
    elif dz < -lim: n.road_elev = p.road_elev - lim
    else: n.road_elev = n.elev


# ---------------------------------------------------------------------------
# VERTICAL PROFILE SMOOTHING (grade / C1) + NO-BURIAL CLEARANCE. The grade-limited
# road_elev is C0 (piecewise-linear in Y): the grade jumps at every station -> the sag/
# crest "kinks" the owner walked. _apply_vertical_profile gives road_elev C1 continuity
# with PARABOLIC vertical curves — a Jacobi relaxation (the EXACT structural mirror of the
# XZ _curvature_limit) that caps the per-station GRADE CHANGE at station/vcurve_len_m, so a
# grade change dg is spread over dg*vcurve_len_m of arc at a CONSTANT rate of grade change
# (== a parabola). Endpoints (anchors) hold, so junction elevations stay continuous.
#
# NO-BURIAL: the SAME relaxation carries a per-node FLOOR = bilinear(terrain) under the deck
# footprint + clearance_m (a hard lower bound clamped every iteration). road_elev — consumed
# by BOTH the RoadMesh deck AND the roadbed_mask flatten — comes out C1 AND never below the
# terrain, so the deck can't sink under a between-station bulge. max_grade is RE-enforced last
# (a safety clamp; a no-op on feasible terrain where grades sit well under the cap).
# Mirrored OPERATION-FOR-OPERATION by hmdflow_module_routespine.cpp.
# ---------------------------------------------------------------------------
_VCURVE_ITERS = 64      # vertical-curve relaxation passes (fixed -> deterministic)
_VCURVE_RELAX = 0.5     # Jacobi relaxation toward the neighbour midpoint at tight grade breaks


def _clearance_floor(cg, nodes):
  """Per-node no-burial floor = MAX terrain under the deck footprint (centre + both rails,
  bilinear on the shared height) + clearance_m. The rail direction is the MITER BISECTOR of the
  incident travel dirs — IDENTICAL to RoadMesh PASS-1's node-ring heading — so the sampled
  footprint lands on the deck's actual ±half-width rail positions and the clearance holds
  exactly there (not just at the centreline)."""
  p = cg.p
  hw = p.width_m * 0.5
  N = len(nodes)
  children = [[] for _ in range(N)]
  for i, nd in enumerate(nodes):
    if nd.parent >= 0:
      children[nd.parent].append(i)
  floor = [0.0] * N
  for i, nd in enumerate(nodes):
    hx = hz = 0.0
    if nd.parent >= 0:
      dx = nd.wx - nodes[nd.parent].wx; dz = nd.wz - nodes[nd.parent].wz
      m = math.hypot(dx, dz)
      if m > 1e-9: hx += dx / m; hz += dz / m
    for c in children[i]:
      dx = nodes[c].wx - nd.wx; dz = nodes[c].wz - nd.wz
      m = math.hypot(dx, dz)
      if m > 1e-9: hx += dx / m; hz += dz / m
    m = math.hypot(hx, hz)
    if m > 1e-9: hx /= m; hz /= m
    else: hx, hz = 0.0, 1.0
    rx, rz = hz, -hx                              # right of the miter heading — the lateral rail dir
    tc = cg.bilinear_height(nd.wx, nd.wz)
    tr = cg.bilinear_height(nd.wx + rx * hw, nd.wz + rz * hw)
    tl = cg.bilinear_height(nd.wx - rx * hw, nd.wz - rz * hw)
    floor[i] = max(tc, tr, tl) + p.clearance_m
  return floor


def _vertical_curve_limit(elev, station_m, vcurve_len_m, floor=None):
  """Jacobi relaxation that caps the per-station GRADE CHANGE at station/vcurve_len_m radians-
  of-slope (endpoints fixed) — the vertical analogue of _curvature_limit. Only stations whose
  grade break exceeds the cap move (toward the neighbour midpoint), so gentle constant-grade
  runs are untouched; tight sag/crest breaks spread into parabolic curves. A per-node `floor`
  (no-burial lower bound) is clamped after every pass. Fixed iteration count -> deterministic."""
  n = len(elev)
  E = list(elev)
  if floor is not None:
    for i in range(n):
      if E[i] < floor[i]: E[i] = floor[i]
  if n < 3 or vcurve_len_m <= 1e-6 or station_m <= 1e-9:
    return E
  cap = station_m / vcurve_len_m
  for _ in range(_VCURVE_ITERS):
    Q = list(E)
    for i in range(1, n - 1):
      g0 = (E[i] - E[i - 1]) / station_m
      g1 = (E[i + 1] - E[i]) / station_m
      if abs(g1 - g0) > cap:
        mid = 0.5 * (E[i - 1] + E[i + 1])
        Q[i] = E[i] + _VCURVE_RELAX * (mid - E[i])
    if floor is not None:
      for i in range(n):
        if Q[i] < floor[i]: Q[i] = floor[i]
    E = Q
  return E


def _enforce_max_grade(nodes, mg):
  """Forward clamp of the EXISTING road_elev profile so |road_elev[i]-road_elev[parent]|/seg
  <= max_grade (does NOT reset toward terrain — unlike _grade_limit_profile). The vertical-
  smoothing safety net; a no-op where grades already sit under the cap."""
  for ni in range(1, len(nodes)):
    n = nodes[ni]
    if n.parent < 0:
      continue
    p = nodes[n.parent]
    seg = math.hypot(n.wx - p.wx, n.wz - p.wz)
    lim = mg * seg
    dz = n.road_elev - p.road_elev
    if dz > lim: n.road_elev = p.road_elev + lim
    elif dz < -lim: n.road_elev = p.road_elev - lim


def _apply_vertical_profile(cg, nodes):
  """The full vertical pipeline on the smoothed forest: (1) base grade-limited profile
  (follows terrain); (2) per-chain parabolic vertical-curve smoothing toward it, solved
  against the no-burial clearance floor; (3) max_grade re-enforced. Anchors are pinned so
  chain endpoints (junction/leaf elevations) stay continuous across the forest."""
  p = cg.p
  _grade_limit_profile(cg, nodes)                          # base C0 profile (grade-limited terrain)
  floor = _clearance_floor(cg, nodes)                      # no-burial lower bound
  _anchors, chains = _chains_from_forest(nodes)
  for chain in chains:
    elev = [nodes[i].road_elev for i in chain]
    fl   = [floor[i] for i in chain]
    total = 0.0
    for k in range(1, len(chain)):
      total += abs(nodes[chain[k]].arclen - nodes[chain[k - 1]].arclen)
    spacing = total / max(1, len(chain) - 1)
    E = _vertical_curve_limit(elev, spacing, p.vcurve_len_m, floor=fl)
    E[0]  = max(elev[0],  fl[0])                            # pin the anchors (>= floor) — weld safety
    E[-1] = max(elev[-1], fl[-1])
    for idx, e in zip(chain, E):
      nodes[idx].road_elev = e
  _enforce_max_grade(nodes, p.max_grade)                   # re-enforce the grade cap post-smoothing


# ---------------------------------------------------------------------------
# SPINE SMOOTHING (curvature) — the raw Dijkstra path is an 8-neighbour STAIRCASE
# (discrete 45/90 deg bends). smooth_spine replaces every degree-2 run between
# ANCHORS (root / fork / leaf / POI, all held fixed so junction welds + waypoints
# survive) with a centripetal Catmull-Rom curve resampled at station_m arc-length,
# then a curvature-limit relaxation caps the per-station turn at station/min_radius
# (a hard curvature floor). Anchors stay put so RoadMesh still sees the fork nodes
# (>= 2 children) and welds the aprons; the grade profile is RE-limited afterwards so
# max_grade survives. Mirrored OPERATION-FOR-OPERATION by hmdflow_module_routespine.cpp.
# ---------------------------------------------------------------------------
_CR_SUBSTEPS   = 16      # dense samples per Catmull-Rom span (curve fidelity before resample)
_CURV_ITERS    = 64      # curvature-limit relaxation passes (fixed -> deterministic)
_CURV_RELAX    = 0.5     # Jacobi relaxation factor toward the neighbour midpoint at tight stations


def _cr_point(p0, p1, p2, p3, t, t0, t1, t2, t3):
  """Centripetal Catmull-Rom interpolation at global knot value t in [t1,t2]."""
  def _lerp(a, b, ta, tb):
    if tb - ta < 1e-12:
      return a
    w = (t - ta) / (tb - ta)
    return (a[0] + (b[0] - a[0]) * w, a[1] + (b[1] - a[1]) * w)
  A1 = _lerp(p0, p1, t0, t1)
  A2 = _lerp(p1, p2, t1, t2)
  A3 = _lerp(p2, p3, t2, t3)
  B1 = (A1[0] + (A2[0] - A1[0]) * ((t - t0) / max(1e-12, t2 - t0)),
        A1[1] + (A2[1] - A1[1]) * ((t - t0) / max(1e-12, t2 - t0)))
  B2 = (A2[0] + (A3[0] - A2[0]) * ((t - t1) / max(1e-12, t3 - t1)),
        A2[1] + (A3[1] - A2[1]) * ((t - t1) / max(1e-12, t3 - t1)))
  w = (t - t1) / max(1e-12, t2 - t1)
  return (B1[0] + (B2[0] - B1[0]) * w, B1[1] + (B2[1] - B1[1]) * w)


def _cr_dense(waypoints):
  """Dense polyline of a centripetal Catmull-Rom curve through waypoints (>= 2 XZ
  points). Endpoints padded by reflection so the curve starts/ends AT the anchors."""
  n = len(waypoints)
  if n <= 1:
    return list(waypoints)
  if n == 2:
    a, b = waypoints[0], waypoints[1]
    return [(a[0] + (b[0] - a[0]) * (s / _CR_SUBSTEPS),
             a[1] + (b[1] - a[1]) * (s / _CR_SUBSTEPS)) for s in range(_CR_SUBSTEPS + 1)]
  pad0 = (2.0 * waypoints[0][0] - waypoints[1][0], 2.0 * waypoints[0][1] - waypoints[1][1])
  padN = (2.0 * waypoints[-1][0] - waypoints[-2][0], 2.0 * waypoints[-1][1] - waypoints[-2][1])
  P = [pad0] + list(waypoints) + [padN]
  out = []
  for i in range(1, len(P) - 2):        # span P[i]..P[i+1]
    p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
    t0 = 0.0
    t1 = t0 + math.sqrt(max(1e-9, math.hypot(p1[0] - p0[0], p1[1] - p0[1])))
    t2 = t1 + math.sqrt(max(1e-9, math.hypot(p2[0] - p1[0], p2[1] - p1[1])))
    t3 = t2 + math.sqrt(max(1e-9, math.hypot(p3[0] - p2[0], p3[1] - p2[1])))
    last = (i == len(P) - 3)
    steps = _CR_SUBSTEPS + (1 if last else 0)
    for s in range(steps):
      t = t1 + (t2 - t1) * (s / _CR_SUBSTEPS)
      out.append(_cr_point(p0, p1, p2, p3, t, t0, t1, t2, t3))
  return out


def _resample_arclen(dense, station_m):
  """Resample a dense polyline at uniform arc-length spacing. Returns >= 2 points that
  START and END exactly on dense[0]/dense[-1] (the anchors), spacing ~= station_m."""
  if len(dense) < 2:
    return list(dense)
  clen = [0.0]
  for k in range(1, len(dense)):
    clen.append(clen[-1] + math.hypot(dense[k][0] - dense[k - 1][0], dense[k][1] - dense[k - 1][1]))
  total = clen[-1]
  if total < 1e-9:
    return [dense[0], dense[-1]]
  # round-half-away (== C++ std::lround) so the station count matches the C++ mirror EXACTLY.
  nseg = max(1, int(math.floor(total / max(1e-6, station_m) + 0.5)))
  out = []
  seg = 0
  for s in range(nseg + 1):
    target = total * (s / nseg)
    while seg < len(clen) - 2 and clen[seg + 1] < target:
      seg += 1
    span = clen[seg + 1] - clen[seg]
    w = 0.0 if span < 1e-12 else (target - clen[seg]) / span
    a, b = dense[seg], dense[seg + 1]
    out.append((a[0] + (b[0] - a[0]) * w, a[1] + (b[1] - a[1]) * w))
  return out


def _curvature_limit(pts, station_m, min_radius_m):
  """Jacobi relaxation that caps the turn at every interior station to
  station/min_radius radians (endpoints fixed) — a hard curvature floor. Only tight
  stations move (toward the neighbour midpoint), so gentle valley-following sections
  are untouched. Fixed iteration count -> deterministic."""
  n = len(pts)
  if n < 3 or min_radius_m <= 1e-6:
    return list(pts)
  cap = station_m / min_radius_m
  P = list(pts)
  for _ in range(_CURV_ITERS):
    Q = list(P)
    for i in range(1, n - 1):
      v0x = P[i][0] - P[i - 1][0]; v0z = P[i][1] - P[i - 1][1]
      v1x = P[i + 1][0] - P[i][0]; v1z = P[i + 1][1] - P[i][1]
      a0 = math.atan2(v0z, v0x); a1 = math.atan2(v1z, v1x)
      dth = a1 - a0
      while dth > math.pi:  dth -= 2.0 * math.pi
      while dth < -math.pi: dth += 2.0 * math.pi
      if abs(dth) > cap:
        midx = 0.5 * (P[i - 1][0] + P[i + 1][0]); midz = 0.5 * (P[i - 1][1] + P[i + 1][1])
        Q[i] = (P[i][0] + _CURV_RELAX * (midx - P[i][0]),
                P[i][1] + _CURV_RELAX * (midz - P[i][1]))
    P = Q
  return P


def _chains_from_forest(nodes):
  """Decompose the spine forest into maximal degree-2 CHAINS between ANCHORS
  (root / fork / leaf / POI). Returns (anchors, chains): anchors = sorted raw indices
  held fixed; chains = list of raw-index paths [anchorA, interior..., anchorB]."""
  N = len(nodes)
  children = [[] for _ in range(N)]
  for i, nd in enumerate(nodes):
    if nd.parent >= 0:
      children[nd.parent].append(i)
  for c in children:
    c.sort()
  def is_anchor(i):
    nd = nodes[i]
    return nd.parent < 0 or len(children[i]) != 1 or nd.is_poi
  anchors = [i for i in range(N) if is_anchor(i)]
  chains = []
  for a in anchors:
    for c in children[a]:
      chain = [a, c]
      cur = c
      while not is_anchor(cur):
        nxt = children[cur][0]
        chain.append(nxt)
        cur = nxt
      chains.append(chain)
  return anchors, chains


def smooth_spine(cg, nodes):
  """Curvature-smooth the raw Dijkstra spine (§ curvature). Anchors (root/fork/leaf/POI)
  stay fixed; each degree-2 chain becomes a resampled centripetal Catmull-Rom curve with
  its per-station turn capped at station/min_radius. Rebuilds a fresh SpineNode forest
  (BFS order, parent<index), then _apply_vertical_profile gives road_elev C1 vertical curves
  + a no-burial clearance floor + the re-enforced grade cap, and re-asserts the forest."""
  p = cg.p
  if len(nodes) < 2:
    return list(nodes)
  anchors, chains = _chains_from_forest(nodes)
  # smoothed XZ points per chain (parallel to `chains`)
  chain_pts = []
  for chain in chains:
    wp = [(nodes[i].wx, nodes[i].wz) for i in chain]
    dense = _cr_dense(wp)
    res = _resample_arclen(dense, p.station_m)
    total = 0.0
    for k in range(1, len(res)):
      total += math.hypot(res[k][0] - res[k - 1][0], res[k][1] - res[k - 1][1])
    spacing = total / max(1, len(res) - 1)
    res = _curvature_limit(res, spacing, p.min_radius_m)
    res[0] = wp[0]; res[-1] = wp[-1]            # pin the anchors EXACTLY (weld safety)
    chain_pts.append(res)
  # BFS reassembly: index anchors as encountered so parent < index everywhere.
  outgoing = {}
  for ci, chain in enumerate(chains):
    outgoing.setdefault(chain[0], []).append(ci)
  root_raw = next(i for i in range(len(nodes)) if nodes[i].parent < 0)
  # FORK STUB: keep a straight-ish approach edge long enough for RoadMesh's apron to fit
  # (dense stations right at a fork would starve the junction setback). Interior stations
  # within stub_m of a FORK anchor are dropped so the fork's incident edge is a clean stub.
  stub_m = 3.0 * p.width_m
  def _is_fork(a):
    return len(outgoing.get(a, [])) >= 2
  new_nodes = []
  anchor_new = {}
  def _mk(wx, wz, is_poi, parent_new):
    L = cg.ldim
    n = SpineNode(cg.cell_of_world(wx, wz), wx, wz, cg.elev[cg.cell_of_world(wx, wz)], p.width_m)
    n.parent = parent_new
    n.is_poi = bool(is_poi)
    new_nodes.append(n)
    return len(new_nodes) - 1
  anchor_new[root_raw] = _mk(nodes[root_raw].wx, nodes[root_raw].wz, nodes[root_raw].is_poi, -1)
  from collections import deque
  q = deque([root_raw])
  while q:
    a = q.popleft()
    ai = anchor_new[a]
    for ci in sorted(outgoing.get(a, []), key=lambda ci: chains[ci][-1]):
      chain = chains[ci]
      pts = chain_pts[ci]
      b = chain[-1]
      start_fork = _is_fork(a); end_fork = _is_fork(b)
      arc = [0.0]
      for k in range(1, len(pts)):
        arc.append(arc[-1] + math.hypot(pts[k][0] - pts[k - 1][0], pts[k][1] - pts[k - 1][1]))
      total = arc[-1]
      prev = ai
      for k in range(1, len(pts) - 1):
        if start_fork and arc[k] < stub_m:
          continue
        if end_fork and (total - arc[k]) < stub_m:
          continue
        prev = _mk(pts[k][0], pts[k][1], False, prev)
      anchor_new[b] = _mk(pts[-1][0], pts[-1][1], nodes[b].is_poi, prev)
      q.append(b)
  # frames + arc-length along the smoothed sequence
  for ni, n in enumerate(new_nodes):
    if n.parent >= 0:
      pn = new_nodes[n.parent]
      hx = n.wx - pn.wx; hz = n.wz - pn.wz
      m = math.hypot(hx, hz)
      if m > 1e-9:
        n.hx = hx / m; n.hz = hz / m
      n.arclen = pn.arclen + m
  # root heading toward its first child
  if len(new_nodes) > 1:
    kids = [i for i in range(len(new_nodes)) if new_nodes[i].parent == 0]
    if kids:
      k0 = new_nodes[min(kids)]
      hx = k0.wx - new_nodes[0].wx; hz = k0.wz - new_nodes[0].wz
      m = math.hypot(hx, hz)
      if m > 1e-9:
        new_nodes[0].hx = hx / m; new_nodes[0].hz = hz / m
  _apply_vertical_profile(cg, new_nodes)   # C1 vertical curves + no-burial clearance + grade cap
  assert_forest(new_nodes)
  return new_nodes


# ---------------------------------------------------------------------------
# forest invariants (Q2)
# ---------------------------------------------------------------------------
def assert_forest(nodes):
  """Refuse loudly on a cycle or a multi-parent node (v1 is single-parent)."""
  n = len(nodes)
  for i, nd in enumerate(nodes):
    assert nd.parent < i, ("RouteSpine forest violation: node %d parent %d is not an ancestor "
                           "(BFS order guarantees parent<child); non-tree edge => a CYCLE, which is "
                           "the v2 currency — refuse" % (i, nd.parent))
  # single-parent is structural (each SpineNode has exactly one _parent field);
  # acyclicity follows from parent<index over the BFS order above.
  return True


def has_cycle(parent):
  """Generic cycle check over a parent array (parent[root]=-1). True if any cycle."""
  n = len(parent)
  color = [0] * n
  for s in range(n):
    if color[s] != 0:
      continue
    stack = []
    c = s
    while c != -1 and color[c] == 0:
      color[c] = 1; stack.append(c); c = parent[c]
    if c != -1 and color[c] == 1:
      return True
    for x in stack:
      color[x] = 2
  return False
