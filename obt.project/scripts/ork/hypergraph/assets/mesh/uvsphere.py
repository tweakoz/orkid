###############################################################################
# UvSphere — latitude/longitude sphere primitive.
#
# Direct triangle-mesh sphere via the classic UV parameterization. Vertices
# concentrate at the poles. Use IcoSphere for uniform vertex distribution;
# use UvSphere when pole-aware UV mapping matters (e.g., latitude-aligned
# textures, world / equatorial decals).
#
# Tessellation:
#   segments_u — longitude divisions (slices around the equator, >= 3)
#   segments_v — latitude divisions  (stacks from pole to pole, >= 3)
#
# Triangle count ≈ 2 * segments_u * (segments_v - 1)
# Default segments_u=48, segments_v=32  →  ~2976 triangles, ~1538 verts.
#
# Wired through the canonical test-proven path (shared with IcoSphere).
###############################################################################

import math

from ork.hypergraph.asset_core import _register
from ork.hypergraph.assets.mesh._common import (
    MeshAsset, build_geometry, sphere_normals_binormals)


def _uvsphere_verts_tris(radius, segments_u, segments_v):
  if segments_u < 3:
    raise ValueError(f"UvSphere: segments_u must be >= 3; got {segments_u}")
  if segments_v < 3:
    raise ValueError(f"UvSphere: segments_v must be >= 3; got {segments_v}")

  verts = []
  tris  = []

  # North pole
  north_idx = len(verts)
  verts.append([0.0, radius, 0.0])

  # Latitude rings, excluding poles
  ring_first = []
  for v in range(1, segments_v):
    phi = math.pi * v / segments_v
    y   = radius * math.cos(phi)
    r   = radius * math.sin(phi)
    ring_first.append(len(verts))
    for u in range(segments_u):
      theta = 2.0 * math.pi * u / segments_u
      verts.append([r * math.cos(theta), y, r * math.sin(theta)])

  # South pole
  south_idx = len(verts)
  verts.append([0.0, -radius, 0.0])

  # North cap
  ring0 = ring_first[0]
  for u in range(segments_u):
    a = ring0 + u
    b = ring0 + (u + 1) % segments_u
    tris.append([north_idx, b, a])

  # Middle rings — quads split into two triangles. Wound a,b,c / a,c,d so the
  # shared ring edges run OPPOSITE to the pole fans (outward-facing under
  # PASS_FRONT cull); the reverse split renders the body inside-out.
  for v in range(len(ring_first) - 1):
    top = ring_first[v]
    bot = ring_first[v + 1]
    for u in range(segments_u):
      a = top + u
      b = top + (u + 1) % segments_u
      c = bot + (u + 1) % segments_u
      d = bot + u
      tris.append([a, b, c])
      tris.append([a, c, d])

  # South cap
  ringN = ring_first[-1]
  for u in range(segments_u):
    a = ringN + u
    b = ringN + (u + 1) % segments_u
    tris.append([south_idx, a, b])

  return verts, tris


@_register
class UvSphere(MeshAsset):
  """Latitude / longitude sphere mesh.

  Usage::

      ball = self.asset.UvSphere("ball",
                                 radius=0.6,
                                 segments_u=48,    # longitude
                                 segments_v=32,    # latitude
                                 material=some_pbr_material)

  Comparable smoothness budget to IcoSphere at subdivisions=3-4; default
  (48, 32) → ~3000 tris. Authored as a lev2.Geometry baked to an .ogeo
  chunkfile and referenced by path from the reflected MeshGenData."""

  __slots__ = ()

  # Default longitude:latitude aspect ratio (48 / 32), preserved when sizing
  # a tessellation to a target poly budget.
  DEFAULT_ASPECT = 1.5

  @staticmethod
  def poly_count(segments_u, segments_v):
    """Triangle count for the given tessellation: 2 * u * (v - 1)
    (segments_u longitude slices, segments_v latitude stacks)."""
    return 2 * int(segments_u) * (int(segments_v) - 1)

  @staticmethod
  def vert_count(segments_u, segments_v):
    """Vertex count: 2 poles + (segments_v - 1) latitude rings of segments_u."""
    return 2 + (int(segments_v) - 1) * int(segments_u)

  @classmethod
  def segments_for_polys(cls, requested_polys, *, aspect=None,
                         min_segments=3, max_segments=512):
    """(segments_u, segments_v) whose triangle count is >= requested_polys
    (the closest match at or above the request), keeping the longitude:latitude
    aspect ratio (default ~3:2, matching the 48x32 default). Smallest such
    tessellation; clamped to [min_segments, max_segments]. Compose with the
    ctor::

        u, v = UvSphere.segments_for_polys(5000)
        UvSphere(segments_u=u, segments_v=v, material=mat)
    """
    a    = cls.DEFAULT_ASPECT if aspect is None else float(aspect)
    mins = int(min_segments)
    maxs = int(max_segments)
    v    = mins
    while v <= maxs:
      u = min(maxs, max(mins, int(round(a * v))))
      if cls.poly_count(u, v) >= requested_polys:
        return u, v
      v += 1
    return min(maxs, max(mins, int(round(a * maxs)))), maxs

  def __init__(self, *, radius=0.5, segments_u=48, segments_v=32, material=None):
    if material is None:
      raise ValueError(
        "UvSphere: material= is required (the mesh + material are baked "
        "into a single drawable, mirroring VdbGridToDrawable's shape).")
    verts, tris        = _uvsphere_verts_tris(float(radius), int(segments_u), int(segments_v))
    normals, binormals = sphere_normals_binormals(verts, float(radius))
    geo                = build_geometry(verts, tris, normals=normals, binormals=binormals)
    self._init(geo, material)


__all__ = ["UvSphere"]
