###############################################################################
# StarCatalogMesh — the REAL night sky as a splat mesh: one camera-agnostic
# quad per Bright Star Catalogue star, baked in the star dome's object space so
# the SAME entity orientation that turns the procedural dome
# (assets/mesh/stardome.py + _star_dome.py) turns this field.
#
# Sibling of StarDomeMesh in every structural respect — a lev2.Geometry baked to
# an .ogeo chunkfile, referenced by path from a reflected MeshGenData, authored
# for viewing FROM THE INSIDE (build_geometry(flip_winding=True)). What differs
# is that nothing here is procedural: positions, brightnesses and colors all
# come from the committed catalog through _bsc5.py, which owns the parse, the
# dome-space derivation and the photometric chain.
#
# PER-STAR DATA, and where it rides in the vertex format. The baked
# SVtxV12N12B12T8C4 vertex has three float3 slots, one float2 and a
# BYTE-QUANTIZED color; a star's luminance spans four decades (Sirius 3.84 down
# to 6.5e-4 at V=7.96), so it cannot live in the byte color. The four verts of a
# star therefore all carry:
#
#   P        — the quad corner, on the tangent plane at `radius` (below).
#   N        — the star's UNIT DIRECTION in dome object space (outward, toward
#              the star). Not a shading normal: the star path is unlit.
#   binormal — LINEAR RADIANCE = peak-normalized sRGB tint * Pogson luminance.
#              NOT A TANGENT. Nothing in the star path does tangent-space
#              shading, and this is the only remaining full-precision float3.
#              Because the tint is normalized to peak 1, the scalar luminance is
#              recoverable EXACTLY as max(radiance.r, radiance.g, radiance.b).
#   uv       — the corner's quad coordinate, (+-1, +-1), for the material's
#              radial profile (cheaper than rebuilding the tangent basis in the
#              shader to recover the offset from P and N).
#
# QUAD SIZE is a fixed ANGULAR half-size (QUAD_HALF_ANGLE_DEG): the ENVELOPE the
# material shades inside. The material owns the real per-star size, which depends
# on the runtime pixel footprint (viewport, fov, headset render scale) this bake
# cannot see — and the COARSEST supported configuration is the one that demands
# the largest angular footprint. So the envelope is DERIVED from that worst case
# by _splat_sizing.py, which both this bake and the material read; too small
# clips a star's glow (square-cornered blob, silently lost energy) and cannot be
# fixed downstream, too large costs overdraw on quads that shade to ~zero.
###############################################################################

import math

import numpy as np

from ork.hypergraph.asset_core import _register
from ork.hypergraph.assets.mesh._common import MeshAsset, build_geometry
from ork.hypergraph.assets.mesh import _bsc5
from ork.hypergraph.assets.mesh._splat_sizing import QUAD_HALF_ANGLE_DEG

# Local shell radius — StarDomeMesh's default and _stars.LOCAL_RADIUS, so the
# splats sit ON the procedural dome's shell and share its entity scale.
LOCAL_RADIUS = 200.0

# The quad's corners in (tangent, binormal) units, counter-clockwise seen from
# OUTSIDE the shell (build_geometry flips the winding for the inside view).
_CORNERS = ((-1.0, -1.0), (1.0, -1.0), (1.0, 1.0), (-1.0, 1.0))


def _tangent_basis(n):
  """Per-star (tangent, bitangent) with t x b == n, from world up, falling back
  to +X for the stars within a hair of the celestial poles."""
  up = np.array([0.0, 1.0, 0.0])
  t  = np.cross(np.broadcast_to(up, n.shape), n)
  bad = np.linalg.norm(t, axis=1) < 1.0e-6
  if bad.any():
    t[bad] = np.cross(np.array([1.0, 0.0, 0.0]), n[bad])
  t /= np.linalg.norm(t, axis=1, keepdims=True)
  return t, np.cross(n, t)


def _splat_geometry(stars, radius, half_angle_deg):
  """4 verts + 2 tris per star. Returns (verts, tris, normals, radiance, uvs),
  all float32 except the int32 triangle list."""
  n_star = len(stars)
  n      = np.array([s.direction for s in stars], dtype=np.float64)
  rad    = np.array([s.radiance  for s in stars], dtype=np.float64)
  t, b   = _tangent_basis(n)
  # Corners on the TANGENT PLANE: the quad subtends exactly 2*half_angle across
  # its axes (and 2*atan(sqrt(2) tan(half_angle)) on the diagonal).
  scale  = math.tan(math.radians(half_angle_deg))

  verts = np.empty((n_star * 4, 3), dtype=np.float64)
  uvs   = np.empty((n_star * 4, 2), dtype=np.float64)
  for k, (u, v) in enumerate(_CORNERS):
    verts[k::4] = radius * (n + scale * (u * t + v * b))
    uvs[k::4]   = (u, v)

  base = np.arange(n_star, dtype=np.int32) * 4
  tris = np.empty((n_star * 2, 3), dtype=np.int32)
  tris[0::2] = np.column_stack([base, base + 1, base + 2])
  tris[1::2] = np.column_stack([base, base + 2, base + 3])

  return (verts.astype(np.float32),
          tris,
          np.repeat(n, 4, axis=0).astype(np.float32),
          np.repeat(rad, 4, axis=0).astype(np.float32),
          uvs.astype(np.float32))


@_register
class StarCatalogMesh(MeshAsset):
  """Bright Star Catalogue splat field for the night sky.

  Usage::

      stars = self.asset.StarCatalogMesh("stars_catalog",
                                         radius   = 200.0,   # LOCAL units
                                         material = star_material)

  9096 stars -> 36384 verts / 18192 triangles. The 14 non-stellar BSC records
  are filtered by _bsc5 (they carry no coordinates and no magnitude)."""

  __slots__ = ()

  def __init__(self, *, radius=LOCAL_RADIUS,
               quad_half_angle_deg=QUAD_HALF_ANGLE_DEG, material=None):
    if material is None:
      raise ValueError(
        "StarCatalogMesh: material= is required (the mesh + material are baked "
        "into a single drawable, mirroring the other mesh assets).")
    stars, stats = _bsc5.load_catalog()
    verts, tris, norms, radiance, uvs = _splat_geometry(
      stars, float(radius), float(quad_half_angle_deg))
    geo = build_geometry(verts, tris, norms, radiance, flip_winding=True)
    geo.point["uv"] = uvs
    print("BSC5-SPLAT: %d stars, %d verts, %d tris, quad half-angle %.3f deg, "
          "radius %.1f" % (stats["stars"], len(verts), len(tris),
                           float(quad_half_angle_deg), float(radius)),
          flush=True)
    self._init(geo, material)


__all__ = ["StarCatalogMesh", "LOCAL_RADIUS", "QUAD_HALF_ANGLE_DEG"]
