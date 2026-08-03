###############################################################################
# StarDomeMesh — the CELESTIAL SPHERE shell the night star field renders on.
#
# A lat/long sphere around the origin authored to be viewed FROM THE INSIDE:
# normals point inward and the winding is reversed (build_geometry(flip_winding))
# so the shell reads right-side-out under PASS_FRONT cull. The star material
# shades from the OBJECT-space direction, so the entity ORIENTATION is the
# sidereal clock — rotating the entity rotates the sky.
#
# WHY A FULL SPHERE by default (cap_deg=180) and not a hemisphere: the dome
# entity is tilted by (latitude - 90) degrees to put its local +Y on the north
# celestial pole, so a hemisphere's rim tilts with it and leaves an unstarred
# wedge of the visible sky — 90 degrees of it at the equator. The below-horizon
# half costs ~half the (trivial) triangle budget and the material fades it out
# at the horizon, so it is never seen. cap_deg < 180 authors an OPEN spherical
# cap for a scene that knows its latitude keeps the rim buried.
#
# Local radius stays in the hundreds of units — the mesh pipeline mangles
# multi-km vertex coordinates (see scn_cloudgauge's cloud shell) — and ENTITY
# SCALE blows the shell up to the world sky radius (Scene.stars(radius_m=...)).
###############################################################################

import math

import numpy as np

from ork.hypergraph.asset_core import _register
from ork.hypergraph.assets.mesh._common import (
    MeshAsset, build_geometry, binormals_from_normals)

# cap_deg at/above this is treated as CLOSED (the last ring collapses onto the
# antipodal pole instead of leaving a rim).
_CLOSED_CAP_DEG = 179.99


def _cap_verts_tris(radius, segments_u, segments_v, cap_deg):
  """Spherical cap from local +Y down to polar angle `cap_deg`, OUTWARD winding
  (build_geometry flips it for the inside view). Rings are uniform in polar
  angle; the apex (and, for a closed cap, the antipode) is a single vertex."""
  if segments_u < 3:
    raise ValueError(f"StarDomeMesh: segments_u must be >= 3; got {segments_u}")
  if segments_v < 2:
    raise ValueError(f"StarDomeMesh: segments_v must be >= 2; got {segments_v}")
  if not (0.0 < cap_deg <= 180.0):
    raise ValueError(f"StarDomeMesh: cap_deg must be in (0,180]; got {cap_deg}")

  closed = cap_deg >= _CLOSED_CAP_DEG
  cap    = math.radians(cap_deg)
  verts  = [[0.0, radius, 0.0]]                  # apex (local +Y = the pole)
  apex   = 0
  rings  = []
  last   = segments_v - 1 if closed else segments_v
  for i in range(1, last + 1):
    phi = cap * i / segments_v
    y   = radius * math.cos(phi)
    r   = radius * math.sin(phi)
    rings.append(len(verts))
    for u in range(segments_u):
      theta = 2.0 * math.pi * u / segments_u
      verts.append([r * math.cos(theta), y, r * math.sin(theta)])

  tris = []
  ring0 = rings[0]
  for u in range(segments_u):                    # apex fan
    tris.append([apex, ring0 + (u + 1) % segments_u, ring0 + u])
  for v in range(len(rings) - 1):                # ring quads
    top = rings[v]
    bot = rings[v + 1]
    for u in range(segments_u):
      a = top + u
      b = top + (u + 1) % segments_u
      c = bot + (u + 1) % segments_u
      d = bot + u
      tris.append([a, b, c])
      tris.append([a, c, d])
  if closed:
    antipode = len(verts)
    verts.append([0.0, -radius, 0.0])
    ringN = rings[-1]
    for u in range(segments_u):
      tris.append([antipode, ringN + u, ringN + (u + 1) % segments_u])
  return verts, tris


@_register
class StarDomeMesh(MeshAsset):
  """Inward-facing celestial-sphere shell for the night star field.

  Usage::

      dome = self.asset.StarDomeMesh("stars_mesh",
                                     radius     = 200.0,   # LOCAL units
                                     segments_u = 64,
                                     segments_v = 48,
                                     material   = star_material)

  Default (64, 48) -> ~6000 triangles. Authored as a lev2.Geometry baked to an
  .ogeo chunkfile and referenced by path from the reflected MeshGenData, like
  every other mesh asset."""

  __slots__ = ()

  def __init__(self, *, radius=200.0, segments_u=64, segments_v=48,
               cap_deg=180.0, material=None):
    if material is None:
      raise ValueError(
        "StarDomeMesh: material= is required (the mesh + material are baked "
        "into a single drawable, mirroring the other mesh assets).")
    verts, tris = _cap_verts_tris(float(radius), int(segments_u),
                                 int(segments_v), float(cap_deg))
    v     = np.asarray(verts, dtype=np.float32)
    norms = (-v / float(radius)).astype(np.float32)   # INWARD (viewed from inside)
    bins  = binormals_from_normals(norms)
    self._init(build_geometry(v, tris, norms, bins, flip_winding=True), material)


__all__ = ["StarDomeMesh"]
