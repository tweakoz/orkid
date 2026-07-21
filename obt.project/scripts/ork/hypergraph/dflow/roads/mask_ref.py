###############################################################################
# mask_ref.py — PURE-PYTHON reference for the R-family field/placement stages
# that ride on top of the RouteSpine (route_ref.py): RoadbedMask + road_elev_m
# + road UV field, KeepoutMask, Parcelize, BuildingSeeds. Mirrors the C++
# hmdflow_module_roadmask / _parcelize / _buildingseeds operation-for-operation
# (the scatter.py <-> hfdflow_scatter.cpp precedent), at the DECLARED layout
# resolution (dim-independent, Q8). Consumed by test_route_oracles.py for the
# coupling oracles (keepout-zero-count, flatten-match) without the built C++.
###############################################################################

import math
from . import route_ref as R


# ---------------------------------------------------------------------------
# geometry helpers
# ---------------------------------------------------------------------------
def _nearest_on_segment(px, pz, ax, az, bx, bz):
  """Return (dist, t, qx, qz) — closest point q=a+t*(b-a), t in [0,1], to p."""
  dx = bx - ax; dz = bz - az
  L2 = dx * dx + dz * dz
  if L2 < 1e-12:
    return math.hypot(px - ax, pz - az), 0.0, ax, az
  t = ((px - ax) * dx + (pz - az) * dz) / L2
  if t < 0.0: t = 0.0
  elif t > 1.0: t = 1.0
  qx = ax + t * dx; qz = az + t * dz
  return math.hypot(px - qx, pz - qz), t, qx, qz


def _signed_lateral(px, pz, ax, az, bx, bz):
  """Signed lateral offset of p across the a->b travel direction (left +)."""
  dx = bx - ax; dz = bz - az
  m = math.hypot(dx, dz)
  if m < 1e-12:
    return 0.0
  # left-normal of travel dir (dx,dz) in XZ is (-dz, dx)
  return ((px - ax) * (-dz) + (pz - az) * (dx)) / m


class RoadProfile:
  def __init__(self, width_m=6.0, shoulder_m=2.0, v_meters_per_tile=8.0):
    self.width_m = float(width_m)
    self.shoulder_m = float(shoulder_m)          # falloff band beyond the half-width
    self.v_meters_per_tile = float(v_meters_per_tile)


def rasterize_roadbed(cg, nodes, profile):
  """Rasterize the spine polyline into LAYOUT-resolution fields (dim-independent):
    roadbed_mask [0,1]  — coverage (1 inside half-width, shoulder falloff to 0)
    road_elev_m         — grade-limited road elevation at the nearest spine point
    uv_u, uv_v          — road UV: U = normalized lateral [0,1] across width,
                          V = world-metric arc-length / v_meters_per_tile
  Returns dict of flat L*L float arrays. NEAREST-SEGMENT parameterization; a
  fork's overlapping approach-band is resolved to the closest segment (documented
  junction seam — the texture-only v1 behavior; RoadMesh junction geometry is the
  next slice)."""
  L = cg.ldim
  N = L * L
  hw = profile.width_m * 0.5
  band = hw + profile.shoulder_m
  segs = [(nodes[nd.parent], nd) for nd in nodes if nd.parent >= 0]
  roadbed = [0.0] * N
  road_elev = [0.0] * N
  uv_u = [0.0] * N
  uv_v = [0.0] * N
  for j in range(L):
    for i in range(L):
      c = j * L + i
      px, pz = R.cell_center_world(i, j, L, cg.p.extent_m)
      best = None
      for (a, b) in segs:
        d, t, qx, qz = _nearest_on_segment(px, pz, a.wx, a.wz, b.wx, b.wz)
        if best is None or d < best[0]:
          best = (d, t, a, b)
      if best is None:
        continue
      d, t, a, b = best
      if d > band:
        continue
      cov = 1.0 if d <= hw else max(0.0, 1.0 - (d - hw) / max(1e-6, profile.shoulder_m))
      roadbed[c] = cov
      relev = a.road_elev + t * (b.road_elev - a.road_elev)
      road_elev[c] = relev
      lat = _signed_lateral(px, pz, a.wx, a.wz, b.wx, b.wz)
      uv_u[c] = min(1.0, max(0.0, 0.5 + 0.5 * (lat / max(1e-6, hw))))
      arc = a.arclen + t * (b.arclen - a.arclen)
      uv_v[c] = arc / max(1e-6, profile.v_meters_per_tile)
  return dict(roadbed=roadbed, road_elev=road_elev, uv_u=uv_u, uv_v=uv_v, ldim=L)


def dilate(mask, L, radius_cells):
  """Chebyshev dilation of a binary-ish mask (values>0.5) by radius cells."""
  out = list(mask)
  r = int(radius_cells)
  if r <= 0:
    return out
  src = [1 if v > 0.5 else 0 for v in mask]
  for j in range(L):
    for i in range(L):
      if src[j * L + i]:
        continue
      hit = False
      for dj in range(-r, r + 1):
        for di in range(-r, r + 1):
          ni = i + di; nj = j + dj
          if 0 <= ni < L and 0 <= nj < L and src[nj * L + ni]:
            hit = True; break
        if hit: break
      if hit:
        out[j * L + i] = 1.0
  return out


def keepout_mask(rb, parcels_cells, L, keepout_radius_cells=1):
  """keepout = dilate(roadbed>0) UNION parcel footprints. Fed to scatter sinks
  INVERTED (no trees on roads / in parcels) — spec §0 BACKWARD coupling."""
  ko = dilate(rb["roadbed"], L, keepout_radius_cells)
  for c in parcels_cells:
    ko[c] = 1.0
  return ko


# ---------------------------------------------------------------------------
# Parcelize — frontage strips along each spine edge (v1). Walk each lane edge,
# counter-hash-jittered spacing; reject on suitability<cutoff. Returns OBB attrs.
# ---------------------------------------------------------------------------
class Parcel:
  __slots__ = ("cx", "cz", "hx", "hz", "frontage", "depth", "cell")
  def __init__(self, cx, cz, hx, hz, frontage, depth, cell):
    self.cx = cx; self.cz = cz; self.hx = hx; self.hz = hz
    self.frontage = frontage; self.depth = depth; self.cell = cell


def parcelize(cg, nodes, frontage_m=12.0, depth_m=16.0, spacing_m=2.0,
              jitter=0.4, side_offset_m=None, seed=1):
  """Deterministic frontage parcels along the spine. Each edge is walked in
  frontage-sized steps (+jittered spacing); a parcel sits offset laterally by
  half-width+depth/2 on BOTH sides. Reference for hmdflow_module_parcelize.cpp."""
  parcels = []
  if side_offset_m is None:
    side_offset_m = cg.p.width_m * 0.5
  stream = 0
  for nd in nodes:
    if nd.parent < 0:
      continue
    a = nodes[nd.parent]; b = nd
    ex = b.wx - a.wx; ez = b.wz - a.wz
    seglen = math.hypot(ex, ez)
    if seglen < 1e-6:
      continue
    dx = ex / seglen; dz = ez / seglen
    nx = -dz; nz = dx                             # left normal
    step = frontage_m + spacing_m
    s = frontage_m * 0.5
    while s < seglen:
      jit = (R.route_u01(seed, nd.cell, stream) - 0.5) * jitter * spacing_m
      stream += 1
      sc = s + jit
      if sc < 0.0 or sc > seglen:
        s += step; continue
      mx = a.wx + dx * sc; mz = a.wz + dz * sc
      for side in (+1.0, -1.0):
        off = side * (side_offset_m + depth_m * 0.5)
        cx = mx + nx * off; cz = mz + nz * off
        cell = cg.cell_of_world(cx, cz)
        parcels.append(Parcel(cx, cz, dx, dz, frontage_m, depth_m, cell))
      s += step
  return parcels


# ---------------------------------------------------------------------------
# BuildingSeeds — scatter-sink-compatible InstanceSet PLUS freeform named SoA
# channels (owner Q2: augmentable-extensibly). Consumers read known channels and
# IGNORE unknown ones; adding a channel is data-side only. Mirrors the C++
# emit adapter default (scatter-sink-compatible form).
#
# CHANNEL SCHEMA (v1 — documented here AND in the C++ module header):
#   xform          mat4   per-instance placement (col-major; ScatterSet layout)
#   type_id        int    building archetype id (freeform SoA)
#   variant_seed   int    per-item variant hash (top-30 bits, cross-lang exact)
#   frontage_m     float  owning parcel frontage (SoA)
#   depth_m        float  owning parcel depth (SoA)
#   orient_rad     float  facing (yaw about +Y, toward the spine) (SoA)
#   height_seed    float  [0,1] per-item storey/height jitter seed (SoA)
#   style_seed     float  [0,1] per-item style seed (SoA)
# ---------------------------------------------------------------------------
class BuildingSeed:
  __slots__ = ("wx", "wz", "orient", "type_id", "variant_seed",
               "frontage", "depth", "height_seed", "style_seed", "cell")
  def __init__(self, **kw):
    for k in self.__slots__:
      setattr(self, k, kw.get(k, 0))


def building_seeds(cg, parcels, type_weights=None, seed=1):
  """One seed per accepted parcel (v1: frontage parcels ARE the building sites).
  type_id is a counter-hash weighted pick; per-item SoA carries parcel geometry
  + variant seeds. Deterministic; consumers place buildings exactly like trees."""
  if type_weights is None:
    type_weights = [1.0]
  tw_sum = sum(type_weights) if sum(type_weights) > 0 else 1.0
  seeds = []
  for idx, pc in enumerate(parcels):
    # facing = toward the spine = -lateral normal (parcel was offset by +normal*side)
    orient = math.atan2(-pc.hx, pc.hz)            # yaw toward travel-left/right frontage
    r = R.route_u01(seed, pc.cell, 0x5EED0001 + idx) * tw_sum
    acc = 0.0; tid = 0
    for t, w in enumerate(type_weights):
      acc += w
      if r < acc:
        tid = t; break
    vseed = int((R.route_hash(seed, pc.cell, 0x5EED0002 + idx) >> 34) & 0x3FFFFFFF)
    seeds.append(BuildingSeed(
      wx=pc.cx, wz=pc.cz, orient=orient, type_id=tid, variant_seed=vseed,
      frontage=pc.frontage, depth=pc.depth,
      height_seed=R.route_u01(seed, pc.cell, 0x5EED0003 + idx),
      style_seed=R.route_u01(seed, pc.cell, 0x5EED0004 + idx),
      cell=pc.cell))
  return seeds


# ---------------------------------------------------------------------------
# scatter-keepout reference — the terrain scatter sink with keepout INVERTED
# into type weights. Used by the keepout-zero-count oracle.
# ---------------------------------------------------------------------------
SS_JITTER_X, SS_JITTER_Z, SS_KEEP = 1, 2, 3

def scatter_with_keepout(cg, suitability, keepout, density=0.01, cutoff=0.05,
                         jitter=1.0, seed=1):
  """Jittered-grid scatter (cells in METERS -> resolution-independent, mirrors
  hfdflow_scatter.cpp) whose per-point weight = suitability*(1-keepout). Returns
  the list of accepted world XZ. The keepout-zero oracle asserts NONE land where
  keepout>0.5."""
  p = cg.p
  L = cg.ldim
  extent = p.extent_m
  ncx = max(1, int(round(extent * math.sqrt(density))))
  cell_m = extent / ncx
  pts = []
  for k in range(ncx * ncx):
    i = k % ncx; j = k // ncx
    wx = (i + 0.5 + (R.route_u01(seed, k, SS_JITTER_X) - 0.5) * jitter) * cell_m - extent * 0.5
    wz = (j + 0.5 + (R.route_u01(seed, k, SS_JITTER_Z) - 0.5) * jitter) * cell_m - extent * 0.5
    u = wx / extent + 0.5; v = wz / extent + 0.5
    suit = R.sample_nearest(suitability, L, L, u, v) if suitability else 1.0
    ko = R.sample_nearest(keepout, L, L, u, v) if keepout else 0.0
    w = max(0.0, suit) * (1.0 - min(1.0, max(0.0, ko)))
    if w < cutoff:
      continue
    keep_p = min(max(w, 0.0), 1.0)
    if R.route_u01(seed, k, SS_KEEP) < keep_p:
      pts.append((wx, wz))
  return pts
