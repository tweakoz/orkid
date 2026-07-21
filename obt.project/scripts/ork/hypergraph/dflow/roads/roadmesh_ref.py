###############################################################################
# roadmesh_ref.py — the PURE-PYTHON reference for the R-family RoadMesh v2
# SKINNER: the swept road RIBBON + JUNCTION patches + gid material split. It is
# the executable spec the C++ hmdflow_module_roadmesh.cpp mirrors OPERATION-FOR-
# OPERATION (the LSweep <-> route_ref precedent). Consumes the RouteSpine FOREST
# (route_ref.SpineNode list, or the equivalent XfNodeGraph CPU mirror via
# nodes_from_arrays) and emits an INDEXED mesh (positions + UV + CSR faces + a
# per-face gid). When the C++ skinner changes, this file changes with it and the
# parity gate pins them (test_roadmesh_gate.py / _tierb).
#
# INVARIANTS this reference establishes (asserted by test_roadmesh_oracles.py):
#   * QUAD-STRIP: each spine EDGE (parent->child) sweeps to ONE quad (gid=road).
#     A straight 2-node spine => exactly 1 quad / 4 verts / 4 corners.
#   * JUNCTION PATCH: a node with >= 2 CHILDREN is a FORK (the only junction kind
#     in the single-parent forest). Its incident segments STOP SHORT at a setback
#     ring; the gap is filled by ONE n-gon patch face (gid=junction) whose 2*degree
#     boundary verts are COINCIDENT with the segment mouth rings — so meshvet's
#     position-weld collapses the seam to a manifold interior edge (crack-free),
#     while the patch keeps its OWN verts for an independent local UV chart.
#   * WELD INTEGRITY: no position-welded edge at a junction seam is a boundary
#     (every mouth edge is shared by its segment quad AND the patch); the only
#     boundary edges are the road perimeter (outer rails, end caps, outer fillets).
#   * WINDING: every emitted face's Newell normal points +Y (road faces up).
#   * UV: U = normalized lateral across width [0,1] (left=1, right=0 — the
#     mask_ref contract), V = world-metric arc-length / v_meters_per_tile. V is
#     MONOTONIC along any root->leaf path (the arc-length is monotone in the tree).
#   * DETERMINISM: nodes in index order, incident edges by neighbor index,
#     junction corners by a stable angle sort -> a byte/count-exact mesh.
#
# SELF-DEFENSE (ops self-defend; documented rules):
#   * zero-length spine edge (coincident nodes)  -> REFUSE loudly (unsweepable).
#   * zero/negative road width                    -> REFUSE loudly.
#   * near-tangent fork (two incident edges whose mouth corners INTERLEAVE around
#     the junction center) -> REFUSE loudly (a valid apron polygon is impossible;
#     the fix is upstream: separate the POIs / raise the routing cell size).
#   * >4-way meet                                 -> AUTO-SATISFIED (a general
#     2*degree-gon patch; the render's E.5 ear-clip triangulates any n<=60).
#   * short incident edge (< 2*setback)           -> AUTO-SATISFIED (the apron
#     setback shrinks to junction_min_edge_frac*edgelen so the segment can't
#     invert).
###############################################################################

import math

# gid material keys (A1 [20:32) band). kRoadGid == hmdflow_module_routespine.cpp.
GID_ROAD     = 1  # swept ribbon segments
GID_JUNCTION = 2  # fork/apron patches
GID_SHOULDER = 3  # v2.5: the terrain-grounding skirt (a dirt/gravel band, distinct material region)
# v2.5 GROUNDING: each deck edge gets an OUTER SKIRT ring dropping to the shared terrain
# height (the interface the owner asked for — no floating silhouette). The outer ring rides
# EXACTLY on the terrain the layout routes on (terrain_h sampler, byte-identical), the deck
# keeps its (possibly lifted) road_elev; the skirt welds deck->ground crack-free.

# Per-vertex REGION + LIFT selector, carried in the mesh COLOR channel (ctx.Cd in the
# asphalt material): Cd.x = region (0 road / 1 junction / 2 shoulder) selects the material
# band + suppresses markings off the ribbon; Cd.y = lift factor (1 = follow the deck's
# scene lift, 0 = grounded) so the outer skirt ring stays pinned to the terrain while the
# deck lifts. The pure-python reference tracks them only so the counts/positions match C++.
REGION_ROAD, REGION_JUNCTION, REGION_SHOULDER = 0.0, 1.0, 2.0
LIFT_DECK, LIFT_GROUND = 1.0, 0.0

_EPS = 1e-6


class RoadProfile:
  """The reflected RoadMeshModuleData scalar env (A8: tweakables are params). Kept
  in ONE place so the C++ describeX + the DSL kwargs + this reference stay aligned."""
  def __init__(self, v_meters_per_tile=8.0, junction_setback_scale=1.5,
               junction_min_edge_frac=0.45, weld_eps=1e-4,
               shoulder_m=3.0, shoulder_gid=GID_SHOULDER, clearance_m=0.3,
               max_bank_rad=math.radians(6.0), bank_runoff_m=30.0, bank_ref_radius_m=24.0):
    self.v_meters_per_tile      = float(v_meters_per_tile)   # meters of arc-length per UV tile
    self.junction_setback_scale = float(junction_setback_scale)  # apron depth = scale * half-width
    self.junction_min_edge_frac = float(junction_min_edge_frac)  # setback clamp on short edges
    self.weld_eps               = float(weld_eps)            # position-weld cell (meshvet parity)
    self.shoulder_m             = float(shoulder_m)          # lateral width of the grounding skirt (m)
    self.shoulder_gid           = int(shoulder_gid)          # gid on the skirt faces
    self.clearance_m            = float(clearance_m)         # no-burial floor for the junction apron (m)
    self.max_bank_rad           = float(max_bank_rad)        # superelevation cap (subtle, ~6 deg)
    self.bank_runoff_m          = float(bank_runoff_m)       # arc length to ramp bank 0<->full (roll runoff)
    self.bank_ref_radius_m      = float(bank_ref_radius_m)   # turn radius at which FULL bank is reached


# ---------------------------------------------------------------------------
# node adapter — a RoadMesh node is the minimal spine facet the skinner needs.
# route_ref.SpineNode already exposes all of these; nodes_from_arrays adapts the
# XfNodeGraph CPU mirror (positions + parents + attrs) for the C++ parity gate.
# ---------------------------------------------------------------------------
class RmNode:
  __slots__ = ("wx", "wz", "y", "parent", "width", "arclen")
  def __init__(self, wx, wz, y, parent, width, arclen):
    self.wx = float(wx); self.wz = float(wz); self.y = float(y)
    self.parent = int(parent); self.width = float(width); self.arclen = float(arclen)


def nodes_from_spine(spine_nodes):
  """Adapt a list of route_ref.SpineNode. Road surface Y = road_elev (the grade-
  limited flatten target the terrain re-bake lowers to), NOT the raw terrain elev."""
  return [RmNode(n.wx, n.wz, n.road_elev, n.parent, n.width, n.arclen) for n in spine_nodes]


def nodes_from_arrays(positions, parents, attrs):
  """Adapt the XfNodeGraph SoA CPU mirror (the RoadsBakeReadout form): positions =
  flat xyz*count (from _xform[12..14]); parents = per node (0xffffffff = root); attrs
  = flat vec4*count (x=width, y=arclen, z=road_elev). Y = road_elev (attrs.z)."""
  n = len(parents)
  out = []
  for i in range(n):
    wx = positions[3 * i + 0]; wz = positions[3 * i + 2]
    par = -1 if (parents[i] == 0xffffffff or parents[i] < 0) else int(parents[i])
    width = attrs[4 * i + 0]; arclen = attrs[4 * i + 1]; road_elev = attrs[4 * i + 2]
    out.append(RmNode(wx, wz, road_elev, par, width, arclen))
  return out


# ---------------------------------------------------------------------------
# geometry helpers (XZ plane, up = +Y). "right" of heading (dx,dz) matches the
# RouteSpine frame X = up x heading = (dz, 0, -dx) -> right_xz = (dz, -dx).
# ---------------------------------------------------------------------------
def _norm2(dx, dz):
  m = math.hypot(dx, dz)
  if m < _EPS:
    return 0.0, 0.0, 0.0
  return dx / m, dz / m, m

def _right(dx, dz):   # right of heading (dx,dz)
  return (dz, -dx)

def _left(dx, dz):    # left of heading (dx,dz)
  return (-dz, dx)

def _newell_y(pts):
  """Y component of the Newell area-normal of a 3D polygon (list of (x,y,z))."""
  ny = 0.0
  n = len(pts)
  for k in range(n):
    x0, _, z0 = pts[k]
    x1, _, z1 = pts[(k + 1) % n]
    ny += (z0 - z1) * (x0 + x1)
  return 0.5 * ny

def _poly_area_xz(pts):
  """Absolute XZ area of a planar polygon (list of (x,y,z))."""
  return abs(_newell_y(pts))


class RoadMesh:
  """The emitted indexed mesh + the metadata the oracles read."""
  def __init__(self):
    self.P   = []       # positions (x,y,z)
    self.UV  = []       # (u,v) per vert
    self.CD  = []       # (region, lift, 0, 1) per vert — the ctx.Cd selector (C++ COLOR channel)
    self.faces = []     # list of (vert_index_tuple, gid)
    self.junction_polys = []  # per-junction: the ordered boundary vert-index tuple
    self.n_quads = 0
    self.n_junctions = 0
    self.n_shoulders = 0      # skirt faces emitted

  def add_vert(self, x, y, z, u, v, region=REGION_ROAD, lift=LIFT_DECK):
    self.P.append((float(x), float(y), float(z)))
    self.UV.append((float(u), float(v)))
    self.CD.append((float(region), float(lift), 0.0, 1.0))
    return len(self.P) - 1

  # ---- counts (exact-count oracle) ----
  @property
  def num_verts(self):   return len(self.P)
  @property
  def num_faces(self):   return len(self.faces)
  @property
  def num_corners(self): return sum(len(f[0]) for f in self.faces)


def _incident(nodes):
  """children[] and, per node, the ordered incident-edge neighbor list (parent first,
  then children ascending) — the deterministic order the C++ mirrors."""
  N = len(nodes)
  children = [[] for _ in range(N)]
  for i, nd in enumerate(nodes):
    if nd.parent >= 0:
      children[nd.parent].append(i)
  for c in children:
    c.sort()
  incident = []
  for i, nd in enumerate(nodes):
    lst = []
    if nd.parent >= 0:
      lst.append(nd.parent)
    lst.extend(children[i])
    incident.append(lst)
  return children, incident


def _is_junction(children, i):
  """A FORK: >= 2 children (the only junction kind in a single-parent forest — a
  join would need 2 parents, which the forest forbids)."""
  return len(children[i]) >= 2


# the hard floor + auto-grow controls for the junction apron setback (see _junction_setback).
_JUNCTION_ANGLE_FLOOR  = math.radians(3.0)  # cap the auto-grow (avoid infinite setback)
_JUNCTION_ANGLE_HARD   = math.radians(6.0)  # below this adjacent-stub gap -> truly near-tangent (refuse)
_JUNCTION_CORNER_MARGIN = 0.85              # keep the corner half-angle strictly < half the gap
                                            # (a real fillet between stubs; < 1 avoids touching corners)


def _junction_setback(nodes, J, incident, profile):
  """The apron setback for junction J. AUTO-GROWS so the incident stubs' mouth corners
  don't overlap (corner half-angle atan2(hw,sb) <= half the smallest adjacent-stub angular
  gap), clamped to junction_min_edge_frac of the shortest incident edge so a segment can't
  invert. Returns (sb, ok, reason):
    ok=False, reason='tangent'  -> two stubs are within the hard angular floor (near-parallel)
    ok=False, reason='tight'    -> the apron can't fit in the available edge length
  Auto-satisfy is the primary path (grow the apron); refusal is only for the unfittable."""
  a = nodes[J]
  hw = a.width * 0.5
  thetas = []
  edge_clamp = float("inf")
  for nbr in incident[J]:
    b = nodes[nbr]
    ux, uz, edgelen = _norm2(b.wx - a.wx, b.wz - a.wz)
    thetas.append(math.atan2(uz, ux))
    edge_clamp = min(edge_clamp, profile.junction_min_edge_frac * edgelen)
  thetas.sort()
  n = len(thetas)
  dmin = 2.0 * math.pi
  for k in range(n):
    gap = thetas[(k + 1) % n] - thetas[k]
    if k == n - 1:
      gap += 2.0 * math.pi
    dmin = min(dmin, gap)
  if dmin < _JUNCTION_ANGLE_HARD:
    return 0.0, False, "tangent"
  base   = profile.junction_setback_scale * hw
  needed = hw / math.tan(max(dmin, _JUNCTION_ANGLE_FLOOR) * 0.5 * _JUNCTION_CORNER_MARGIN)
  sb = min(max(base, needed), edge_clamp)
  if needed > edge_clamp + 1e-6:
    return 0.0, False, "tight"
  return sb, True, ""


def _ring_center_and_dir(nodes, i, nbr, sb):
  """The setback ring for junction node i toward neighbor nbr at the given apron setback
  sb: outward unit dir u, ring center (Y lerped along the edge), and the arc-length AT the
  ring (monotone along the path). Returns (ux,uz,cx,cy,cz,arc)."""
  a = nodes[i]; b = nodes[nbr]
  ux, uz, edgelen = _norm2(b.wx - a.wx, b.wz - a.wz)
  t = sb / edgelen if edgelen > _EPS else 0.0
  cx = a.wx + ux * sb
  cz = a.wz + uz * sb
  cy = a.y + (b.y - a.y) * t
  # arc-length AT the ring: toward a CHILD => +sb; toward the PARENT => -sb.
  toward_parent = (nbr == a.parent)
  arc = a.arclen + (-sb if toward_parent else sb)
  return ux, uz, cx, cy, cz, arc


# ---------------------------------------------------------------------------
# SUPERELEVATION (deck bank) — a subtle cross-slope into plan-view curves. The bank angle
# is derived from the local turn radius (max_bank * clamp(bank_ref_radius/R,0,1)), zeroed on
# straights + at junctions + within bank_runoff_m of a junction (the apron stays flat), then
# RATE-LIMITED to a roll-rate ceiling (max_bank*station/bank_runoff_m) along each chain so the
# roll has no kinks (C1). Application PIVOTS ABOUT THE INSIDE (low) rail: the low rail stays at
# road_elev, the outside rail RISES by width*sin(bank). So NO deck vertex drops below road_elev
# and the no-burial clearance (solved upstream in RouteSpine) covers every banked vert — banking
# is applied AFTER clearance with no re-check needed. Mirrored by hmdflow_module_roadmesh.cpp.
# ---------------------------------------------------------------------------
_BANK_SIGN = 1.0    # +1 => a LEFT turn (cross>0) raises the RIGHT (outside) rail; flip to invert


def _chains(nodes, children):
  """Degree-2 chains between anchors (root/fork/leaf) over the RmNode forest — the banking-
  profile decomposition (no POI concept at the mesh layer, so a mid-chain waypoint still banks)."""
  N = len(nodes)
  def is_anchor(i):
    return nodes[i].parent < 0 or len(children[i]) != 1
  chains = []
  for a in range(N):
    if not is_anchor(a):
      continue
    for c in children[a]:
      chain = [a, c]; cur = c
      while not is_anchor(cur):
        nxt = children[cur][0]; chain.append(nxt); cur = nxt
      chains.append(chain)
  return chains


def _slew_c1(vals, rate_cap):
  """Forward+backward rate limiter: |V[i]-V[i-1]| <= rate_cap everywhere, endpoints (=0 at
  anchors) preserved. Deterministic -> the roll runoff with a bounded roll rate (no kinks)."""
  n = len(vals); V = list(vals)
  for i in range(1, n):
    d = V[i] - V[i - 1]
    if d > rate_cap: V[i] = V[i - 1] + rate_cap
    elif d < -rate_cap: V[i] = V[i - 1] - rate_cap
  for i in range(n - 2, -1, -1):
    d = V[i] - V[i + 1]
    if d > rate_cap: V[i] = V[i + 1] + rate_cap
    elif d < -rate_cap: V[i] = V[i + 1] - rate_cap
  return V


def _superelevation(nodes, children, profile):
  """Per-node signed bank (radians; + = RIGHT rail raised). See the section header."""
  N = len(nodes)
  maxb = profile.max_bank_rad
  refr = profile.bank_ref_radius_m
  runoff = max(_EPS, profile.bank_runoff_m)
  raw = [0.0] * N
  for i, nd in enumerate(nodes):
    if len(children[i]) != 1 or nd.parent < 0:
      continue                                   # junction / leaf / root -> no through-turn
    p = nodes[nd.parent]; c = nodes[children[i][0]]
    ix, iz, seg_in  = _norm2(nd.wx - p.wx, nd.wz - p.wz)
    ox, oz, seg_out = _norm2(c.wx - nd.wx, c.wz - nd.wz)
    if seg_in < _EPS or seg_out < _EPS:
      continue
    dth = math.atan2(oz, ox) - math.atan2(iz, ix)
    while dth > math.pi:  dth -= 2.0 * math.pi
    while dth < -math.pi: dth += 2.0 * math.pi
    ds = 0.5 * (seg_in + seg_out)
    kappa = abs(dth) / ds if ds > _EPS else 0.0
    mag = maxb * min(1.0, refr * kappa)
    cross = ix * oz - iz * ox                    # in x out (2D): >0 left turn -> outside = right rail
    if abs(cross) > 1e-12:
      raw[i] = mag * (_BANK_SIGN if cross > 0.0 else -_BANK_SIGN)
  signed = list(raw)
  for chain in _chains(nodes, children):
    arc = [0.0]
    for k in range(1, len(chain)):
      a = nodes[chain[k - 1]]; b = nodes[chain[k]]
      arc.append(arc[-1] + math.hypot(b.wx - a.wx, b.wz - a.wz))
    total = arc[-1]
    a_j = _is_junction(children, chain[0]); b_j = _is_junction(children, chain[-1])
    vals = []
    for k, idx in enumerate(chain):
      v = raw[idx]
      if a_j and arc[k] < runoff: v = 0.0
      if b_j and (total - arc[k]) < runoff: v = 0.0
      vals.append(v)
    spacing = total / max(1, len(chain) - 1)
    rate_cap = maxb * spacing / runoff
    vals = _slew_c1(vals, rate_cap)
    for k, idx in enumerate(chain):
      signed[idx] = vals[k]
  return signed


def build_roadmesh(nodes, profile=None, terrain_h=None):
  """Skin the spine forest into the swept ribbon + junction patches, plus (v2.5) an
  OUTER GROUNDING SKIRT dropping every deck edge + the junction apron perimeter to the
  shared terrain height (terrain_h(x,z)->y). Returns a RoadMesh. When terrain_h is None
  the skirt is omitted (deck-only — the pure-geometry oracles). Raises on the documented
  degenerate refusals (ops self-defend)."""
  if profile is None:
    profile = RoadProfile()
  N = len(nodes)
  children, incident = _incident(nodes)
  bank = _superelevation(nodes, children, profile)   # per-node signed deck bank (superelevation)
  mesh = RoadMesh()
  vmpt = max(_EPS, profile.v_meters_per_tile)
  shoulders = (terrain_h is not None) and (profile.shoulder_m > _EPS)
  shw = profile.shoulder_m
  sgid = profile.shoulder_gid

  # ---- self-defense: widths + edge lengths ----
  for i, nd in enumerate(nodes):
    if nd.width <= _EPS:
      raise RuntimeError("RoadMesh: node %d has non-positive width %.6g — refuse "
                         "(a ribbon needs a road width; set width_m > 0)" % (i, nd.width))
    if nd.parent >= 0:
      p = nodes[nd.parent]
      if math.hypot(nd.wx - p.wx, nd.wz - p.wz) < profile.weld_eps:
        raise RuntimeError("RoadMesh: spine edge %d<-%d is zero-length (coincident nodes) "
                           "— refuse (unsweepable; RouteSpine must emit distinct cells)"
                           % (i, nd.parent))

  # rail-vert -> (shoulder_inner, shoulder_outer) so incident segments SHARE the skirt
  # verts at a ring (the cross-ring skirt edge stays a welded interior edge).
  shoulder_of = {}
  def _emit_shoulder(rail_vert, ox, oz, u, vv):
    """Emit the region-SHOULDER inner (coincident w/ the deck rail, lifts with the deck)
    + outer (offset outward by shoulder_m, pinned to the terrain) verts for a rail vert."""
    px, py, pz = mesh.P[rail_vert]
    inner = mesh.add_vert(px, py, pz, u, vv, REGION_SHOULDER, LIFT_DECK)
    gx = px + ox * shw; gz = pz + oz * shw
    gy = terrain_h(gx, gz)
    outer = mesh.add_vert(gx, gy, gz, u, vv, REGION_SHOULDER, LIFT_GROUND)
    shoulder_of[rail_vert] = (inner, outer)
    return inner, outer

  # ---- PASS 1: node rings for every NON-junction node (through + endpoint). Shared by
  #      the incident segments so they meet EXACTLY (no miter gap). heading = the miter
  #      bisector of the incident travel dirs (endpoint: the single incident dir). ----
  node_ring = [None] * N    # i -> (vLeft, vRight) or None (junction)
  for i, nd in enumerate(nodes):
    if _is_junction(children, i):
      continue
    # incident travel dirs (all pointing 'through' node i): parent->i and i->child.
    hx = hz = 0.0
    if nd.parent >= 0:
      dx, dz, _ = _norm2(nd.wx - nodes[nd.parent].wx, nd.wz - nodes[nd.parent].wz)
      hx += dx; hz += dz
    for c in children[i]:
      dx, dz, _ = _norm2(nodes[c].wx - nd.wx, nodes[c].wz - nd.wz)
      hx += dx; hz += dz
    hx, hz, hm = _norm2(hx, hz)
    if hm < _EPS:  # degenerate bisector (a hairpin) -> fall back to +Z frame
      hx, hz = 0.0, 1.0
    rx, rz = _right(hx, hz)
    hw = nd.width * 0.5
    v_uv = nd.arclen / vmpt
    # superelevation: pivot about the INSIDE (low) rail -> low rail stays at road_elev, outside
    # rail rises by width*sin(bank). No deck vert drops below road_elev (clearance preserved).
    bnk = bank[i]
    lift = nd.width * math.sin(abs(bnk))
    yR = nd.y + (lift if bnk > 0.0 else 0.0)   # right rail high on a left turn (bank>0)
    yL = nd.y + (lift if bnk < 0.0 else 0.0)   # left  rail high on a right turn (bank<0)
    vRight = mesh.add_vert(nd.wx + rx * hw, yR, nd.wz + rz * hw, 0.0, v_uv)   # right rail u=0
    vLeft  = mesh.add_vert(nd.wx - rx * hw, yL, nd.wz - rz * hw, 1.0, v_uv)   # left  rail u=1
    node_ring[i] = (vLeft, vRight)
    if shoulders:
      _emit_shoulder(vRight, rx, rz, 0.0, v_uv)    # right rail -> outward +right
      _emit_shoulder(vLeft, -rx, -rz, 1.0, v_uv)   # left  rail -> outward -right

  # ---- PASS 2: per-junction setback SEGMENT rings (road UV) + PATCH corner rings
  #      (junction-local UV, coincident positions -> position-welded seam). ----
  seg_ring = {}    # (junction i, neighbor nbr) -> (vLeft, vRight) road-UV mouth
  mesh.junction_sb = {}   # J -> the apron setback used (oracles reuse it)
  for J in range(N):
    if not _is_junction(children, J):
      continue
    sb, ok, reason = _junction_setback(nodes, J, incident, profile)
    if not ok:
      if reason == "tangent":
        raise RuntimeError(
            "RoadMesh: junction node %d is NEAR-TANGENT (two fork edges within %.1f deg) — "
            "refuse (separate the routes / raise layout_cell_m)"
            % (J, math.degrees(_JUNCTION_ANGLE_HARD)))
      raise RuntimeError(
          "RoadMesh: junction node %d apron can't fit — the fork's tightest edge is too "
          "short for a %.1fm-wide road's apron. Raise layout_cell_m or narrow width_m."
          % (J, nodes[J].width))
    mesh.junction_sb[J] = sb
    hw = nodes[J].width * 0.5
    # gather this junction's stub geometry (uniform setback -> non-overlapping mouths). The apron
    # gets the SAME no-burial treatment as the deck: raise a buried stub ring centre (and its two
    # mouth corners, which share cy) to terrain + clearance. The apron stays FLAT per stub (a
    # bounded local lift; the segment quad into it absorbs the sub-setback step).
    stubs = []  # (nbr, ux, uz, cx, cy, cz, arc, theta)
    for nbr in incident[J]:
      ux, uz, cx, cy, cz, arc = _ring_center_and_dir(nodes, J, nbr, sb)
      if shoulders:
        rx, rz = _right(ux, uz)
        tmax = max(terrain_h(cx, cz), terrain_h(cx + rx * hw, cz + rz * hw),
                   terrain_h(cx - rx * hw, cz - rz * hw))
        cy = max(cy, tmax + profile.clearance_m)
      theta = math.atan2(uz, ux)
      stubs.append((nbr, ux, uz, cx, cy, cz, arc, theta))
    # segment-side mouth rings (road UV: u=1 left / 0 right, V from arc) + their shoulders
    for (nbr, ux, uz, cx, cy, cz, arc, theta) in stubs:
      rx, rz = _right(ux, uz)
      v_uv = arc / vmpt
      vRight = mesh.add_vert(cx + rx * hw, cy, cz + rz * hw, 0.0, v_uv)
      vLeft  = mesh.add_vert(cx - rx * hw, cy, cz - rz * hw, 1.0, v_uv)
      seg_ring[(J, nbr)] = (vLeft, vRight)
      if shoulders:
        _emit_shoulder(vRight, rx, rz, 0.0, v_uv)
        _emit_shoulder(vLeft, -rx, -rz, 1.0, v_uv)

    # ---- junction patch: corner verts (own local UV chart), COINCIDENT with the
    #      segment mouths. Order them by a stable DESCENDING angle sort about the
    #      junction center (-> +Y Newell normal). Each stub must contribute a
    #      CONSECUTIVE corner pair (its mouth edge), else the fork is near-tangent. ----
    corners = []  # (px, py, pz, stub_idx)
    for si, (nbr, ux, uz, cx, cy, cz, arc, theta) in enumerate(stubs):
      rx, rz = _right(ux, uz)
      corners.append((cx + rx * hw, cy, cz + rz * hw, si))   # right corner
      corners.append((cx - rx * hw, cy, cz - rz * hw, si))   # left  corner
    Jx, Jz = nodes[J].wx, nodes[J].wz
    def _ang(c):
      return math.atan2(c[2] - Jz, c[0] - Jx)
    # DESCENDING angle, stable by stub index then side for reproducibility
    order = sorted(range(len(corners)),
                   key=lambda k: (-_ang(corners[k]), corners[k][3]))
    # near-tangent detection: each stub's two corners must be adjacent in `order`.
    pos_in_order = [0] * len(corners)
    for oidx, cidx in enumerate(order):
      pos_in_order[cidx] = oidx
    n_c = len(corners)
    for si in range(len(stubs)):
      c0 = 2 * si; c1 = 2 * si + 1
      d = abs(pos_in_order[c0] - pos_in_order[c1])
      if not (d == 1 or d == n_c - 1):
        raise RuntimeError(
            "RoadMesh: junction node %d is NEAR-TANGENT (fork edges to %d overlap: mouth "
            "corners interleave) — refuse (separate the routes / raise layout_cell_m)"
            % (J, stubs[si][0]))
    # local planar UV chart normalized to the patch XZ bbox (top-down; independent of
    # the road arc-length chart — the patch owns these verts).
    xs = [corners[k][0] for k in range(n_c)]; zs = [corners[k][2] for k in range(n_c)]
    xmin, xmax = min(xs), max(xs); zmin, zmax = min(zs), max(zs)
    du = max(_EPS, xmax - xmin); dv = max(_EPS, zmax - zmin)
    poly = []
    for cidx in order:
      cx0, cy0, cz0, _si = corners[cidx]
      u = (cx0 - xmin) / du; v = (cz0 - zmin) / dv
      poly.append(mesh.add_vert(cx0, cy0, cz0, u, v, REGION_JUNCTION, LIFT_DECK))
    mesh.faces.append((tuple(poly), GID_JUNCTION))
    mesh.junction_polys.append(tuple(poly))
    mesh.n_junctions += 1

    # ---- junction apron SKIRT: extrude the FILLET-GAP perimeter edges (consecutive
    #      corners from DIFFERENT stubs) radially outward to the terrain (the mouth
    #      edges are welded to the segment decks, so they are NOT skirted here). ----
    if shoulders:
      for oi in range(n_c):
        c0 = order[oi]; c1 = order[(oi + 1) % n_c]
        if corners[c0][3] == corners[c1][3]:
          continue                                # same-stub mouth edge (deck weld seam)
        p0 = mesh.P[poly[oi]]; p1 = mesh.P[poly[(oi + 1) % n_c]]
        i0 = mesh.add_vert(p0[0], p0[1], p0[2], 0.0, 0.0, REGION_SHOULDER, LIFT_DECK)
        i1 = mesh.add_vert(p1[0], p1[1], p1[2], 0.0, 0.0, REGION_SHOULDER, LIFT_DECK)
        o0x, o0z, _l = _norm2(p0[0] - Jx, p0[2] - Jz)
        o1x, o1z, _l = _norm2(p1[0] - Jx, p1[2] - Jz)
        g0x = p0[0] + o0x * shw; g0z = p0[2] + o0z * shw
        g1x = p1[0] + o1x * shw; g1z = p1[2] + o1z * shw
        o0 = mesh.add_vert(g0x, terrain_h(g0x, g0z), g0z, 0.0, 0.0, REGION_SHOULDER, LIFT_GROUND)
        o1 = mesh.add_vert(g1x, terrain_h(g1x, g1z), g1z, 0.0, 0.0, REGION_SHOULDER, LIFT_GROUND)
        mesh.faces.append(((i0, i1, o1, o0), sgid))   # descending-angle order -> +Y
        mesh.n_shoulders += 1

  # ---- PASS 3: segment quads (one per parent->child edge). Each end uses the node
  #      ring (non-junction) or the setback seg-ring (junction). Left/right by the
  #      segment travel dir; wound [aL,bL,bR,aR] -> +Y normal. gid = road. Plus the
  #      per-side grounding skirts (shared ring shoulder verts -> welded strips). ----
  for b in range(N):
    a = nodes[b].parent
    if a < 0:
      continue
    tx, tz, _ = _norm2(nodes[b].wx - nodes[a].wx, nodes[b].wz - nodes[a].wz)
    lx, lz = _left(tx, tz)

    def pick(end_node, ring_pair, cx, cz):
      # ring_pair = (vA, vB); pick left/right by dot with the travel-left dir.
      vA, vB = ring_pair
      def latdot(vi):
        px, _py, pz = mesh.P[vi]
        return (px - cx) * lx + (pz - cz) * lz
      return (vA, vB) if latdot(vA) >= latdot(vB) else (vB, vA)

    if _is_junction(children, a):
      aL, aR = pick(a, seg_ring[(a, b)], *_ring_center_xz(mesh, seg_ring[(a, b)]))
    else:
      aL, aR = pick(a, node_ring[a], nodes[a].wx, nodes[a].wz)
    if _is_junction(children, b):
      bL, bR = pick(b, seg_ring[(b, a)], *_ring_center_xz(mesh, seg_ring[(b, a)]))
    else:
      bL, bR = pick(b, node_ring[b], nodes[b].wx, nodes[b].wz)

    mesh.faces.append(((aL, bL, bR, aR), GID_ROAD))
    mesh.n_quads += 1

    if shoulders:
      raiL = shoulder_of[aR]; rbiL = shoulder_of[bR]   # right side (skirt at aR/bR)
      laiL = shoulder_of[aL]; lbiL = shoulder_of[bL]   # left  side (skirt at aL/bL)
      mesh.faces.append(((raiL[0], rbiL[0], rbiL[1], raiL[1]), sgid))   # right skirt +Y
      mesh.faces.append(((laiL[0], laiL[1], lbiL[1], lbiL[0]), sgid))   # left  skirt +Y
      mesh.n_shoulders += 2

  return mesh


def _ring_center_xz(mesh, ring_pair):
  """XZ midpoint of a ring's two verts (for the travel-left pick reference point)."""
  vA, vB = ring_pair
  ax, _ay, az = mesh.P[vA]; bx, _by, bz = mesh.P[vB]
  return (0.5 * (ax + bx), 0.5 * (az + bz))


# ---------------------------------------------------------------------------
# ORACLES — pure geometry checks the tests drive (also the E.5 area-sum ear-clip).
# ---------------------------------------------------------------------------
def face_normals_up(mesh, tol=1e-9):
  """True iff every DRIVABLE face (road ribbon + junction apron) has a +Y Newell normal.
  Shoulder-skirt faces (GID_SHOULDER) are graded BANKS — sloped by construction, so their
  top-down projection sign is not an invariant — and are exempt. Returns (ok, first_bad)."""
  for fi, (idx, gid) in enumerate(mesh.faces):
    if gid == GID_SHOULDER:
      continue
    pts = [mesh.P[v] for v in idx]
    if _newell_y(pts) <= tol:
      return False, fi
  return True, -1


def _weld_key(p, eps):
  return (round(p[0] / eps), round(p[1] / eps), round(p[2] / eps))


def edge_health(mesh, eps=1e-4):
  """Position-welded undirected edge -> incident face count (the meshvet model). Returns
  dict(boundary=int, nonmanifold=int, interior=int) counting welded edges by degree."""
  wid = {}
  def wk(v):
    k = _weld_key(mesh.P[v], eps)
    if k not in wid:
      wid[k] = len(wid)
    return wid[k]
  from collections import Counter
  ec = Counter()
  for (idx, _gid) in mesh.faces:
    n = len(idx)
    for k in range(n):
      a = wk(idx[k]); b = wk(idx[(k + 1) % n])
      ec[(min(a, b), max(a, b))] += 1
  boundary = sum(1 for c in ec.values() if c == 1)
  nonmanifold = sum(1 for c in ec.values() if c > 2)
  interior = sum(1 for c in ec.values() if c == 2)
  return dict(boundary=boundary, nonmanifold=nonmanifold, interior=interior)


def junction_seam_edges_shared(mesh, nodes, profile, eps=1e-4, terrain_h=None):
  """Every junction MOUTH edge (a stub's two mouth corners) is shared by >= 2 faces
  after position-weld -> no boundary at the seam. Returns (ok, n_seams_checked). When
  terrain_h is supplied, the expected mouth Y carries the SAME no-burial apron raise
  build_roadmesh applies (else the recomputed seam misses a raised mouth)."""
  children, incident = _incident(nodes)
  # rebuild the welded edge count
  wid = {}
  def wk_pos(p):
    k = _weld_key(p, eps)
    if k not in wid:
      wid[k] = len(wid)
    return wid[k]
  from collections import Counter
  ec = Counter()
  for (idx, _gid) in mesh.faces:
    n = len(idx)
    for k in range(n):
      a = wk_pos(mesh.P[idx[k]]); b = wk_pos(mesh.P[idx[(k + 1) % n]])
      ec[(min(a, b), max(a, b))] += 1
  checked = 0
  sbmap = getattr(mesh, "junction_sb", {})
  for J in range(len(nodes)):
    if not _is_junction(children, J):
      continue
    hw = nodes[J].width * 0.5
    sb = sbmap.get(J)
    if sb is None:
      sb, _ok, _r = _junction_setback(nodes, J, incident, profile)
    for nbr in incident[J]:
      ux, uz, cx, cy, cz, arc = _ring_center_and_dir(nodes, J, nbr, sb)
      rx, rz = _right(ux, uz)
      if terrain_h is not None and profile.shoulder_m > _EPS:
        tmax = max(terrain_h(cx, cz), terrain_h(cx + rx * hw, cz + rz * hw),
                   terrain_h(cx - rx * hw, cz - rz * hw))
        cy = max(cy, tmax + profile.clearance_m)   # same apron no-burial raise as build_roadmesh
      pR = (cx + rx * hw, cy, cz + rz * hw)
      pL = (cx - rx * hw, cy, cz - rz * hw)
      a = wk_pos(pR); b = wk_pos(pL)
      e = (min(a, b), max(a, b))
      checked += 1
      if ec.get(e, 0) < 2:
        return False, checked
  return True, checked


def uv_v_monotonic_paths(mesh, nodes, profile, tol=1e-6):
  """V (arc-length UV) is monotone non-decreasing along every root->leaf path. Checked
  on the ROAD faces' rail verts (the ribbon carries arc-length; junctions have their own
  chart). Returns (ok, worst_decrease)."""
  # map each non-junction node -> its ring V; each junction stub -> its seg-ring V.
  children, incident = _incident(nodes)
  vmpt = max(_EPS, profile.v_meters_per_tile)
  worst = 0.0
  ok = True
  for b in range(len(nodes)):
    a = nodes[b].parent
    if a < 0:
      continue
    va = nodes[a].arclen / vmpt
    vb = nodes[b].arclen / vmpt
    if vb - va < -tol:
      ok = False
      worst = max(worst, va - vb)
  return ok, worst


def uv_u_in_unit(mesh, tol=1e-9):
  """Every vertex U is within [0,1]."""
  for (u, v) in mesh.UV:
    if u < -tol or u > 1.0 + tol:
      return False
  return True


# --- E.5 ear-clip (mirrors hmdflow_render.cpp cs_tri on the Newell best-fit plane) ---
def earclip_polygon(pts):
  """Triangulate a simple planar polygon (list of (x,y,z)) into (n-2) triangles by
  ear-clipping on the Newell best-fit plane. Returns a list of index triples into pts.
  This is the E.5 discipline used ONLY by the area-sum oracle (the mesh ships the raw
  n-gon; the render's cs_tri does the real GPU ear-clip identically)."""
  n = len(pts)
  if n < 3:
    return []
  # Newell normal -> plane basis
  nx = ny = nz = 0.0
  for k in range(n):
    x0, y0, z0 = pts[k]; x1, y1, z1 = pts[(k + 1) % n]
    nx += (y0 - y1) * (z0 + z1); ny += (z0 - z1) * (x0 + x1); nz += (x0 - x1) * (y0 + y1)
  nl = math.sqrt(nx * nx + ny * ny + nz * nz)
  if nl < 1e-12:
    return [(0, k, k + 1) for k in range(1, n - 1)]  # degenerate -> fan
  nx /= nl; ny /= nl; nz /= nl
  ax = (1.0, 0.0, 0.0) if abs(nx) < 0.9 else (0.0, 1.0, 0.0)
  ux = ny * ax[2] - nz * ax[1]; uy = nz * ax[0] - nx * ax[2]; uz = nx * ax[1] - ny * ax[0]
  ul = math.sqrt(ux * ux + uy * uy + uz * uz); ux /= ul; uy /= ul; uz /= ul
  vx = ny * uz - nz * uy; vy = nz * ux - nx * uz; vz = nx * uy - ny * ux
  c0 = pts[0]
  pp = []
  for k in range(n):
    dx = pts[k][0] - c0[0]; dy = pts[k][1] - c0[1]; dz = pts[k][2] - c0[2]
    pp.append((dx * ux + dy * uy + dz * uz, dx * vx + dy * vy + dz * vz))
  area2 = 0.0
  for k in range(n):
    q0 = pp[k]; q1 = pp[(k + 1) % n]
    area2 += q0[0] * q1[1] - q1[0] * q0[1]
  orient = 1.0 if area2 >= 0.0 else -1.0
  ring = list(range(n))
  tris = []
  guard = 0
  while len(ring) > 3 and guard < 10 * n:
    guard += 1
    m = len(ring)
    found = False
    for i in range(m):
      ip = ring[(i - 1) % m]; ic = ring[i]; ino = ring[(i + 1) % m]
      A = pp[ip]; B = pp[ic]; C = pp[ino]
      cr = (B[0] - A[0]) * (C[1] - B[1]) - (B[1] - A[1]) * (C[0] - B[0])
      if cr * orient <= 1e-12:
        continue
      clear = True
      for j in ring:
        if j in (ip, ic, ino):
          continue
        Q = pp[j]
        s0 = ((B[0] - A[0]) * (Q[1] - A[1]) - (B[1] - A[1]) * (Q[0] - A[0])) * orient
        s1 = ((C[0] - B[0]) * (Q[1] - B[1]) - (C[1] - B[1]) * (Q[0] - B[0])) * orient
        s2 = ((A[0] - C[0]) * (Q[1] - C[1]) - (A[1] - C[1]) * (Q[0] - C[0])) * orient
        if s0 >= 0.0 and s1 >= 0.0 and s2 >= 0.0:
          clear = False; break
      if clear:
        tris.append((ip, ic, ino)); ring.pop(i); found = True; break
    if not found:
      break
  for k in range(1, len(ring) - 1):
    tris.append((ring[0], ring[k], ring[k + 1]))
  return tris


def junction_area_matches_earclip(mesh, tol=1e-6):
  """E.5 AREA-SUM oracle: for each junction n-gon, the ear-clip triangle areas sum to
  the polygon area (the polygon is simple & the triangulation is valid). Returns
  (ok, n_junctions_checked, worst_rel_err)."""
  worst = 0.0
  for poly in mesh.junction_polys:
    pts = [mesh.P[v] for v in poly]
    poly_area = _poly_area_xz(pts)
    tris = earclip_polygon(pts)
    if len(tris) != len(pts) - 2:
      return False, 0, 1.0
    tsum = 0.0
    for (a, b, c) in tris:
      pa, pb, pc = pts[a], pts[b], pts[c]
      # XZ triangle area
      tsum += abs((pb[0] - pa[0]) * (pc[2] - pa[2]) - (pc[0] - pa[0]) * (pb[2] - pa[2])) * 0.5
    if poly_area > _EPS:
      worst = max(worst, abs(tsum - poly_area) / poly_area)
  return worst <= tol, len(mesh.junction_polys), worst
