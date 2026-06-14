###############################################################################
# IcoSphere — subdivided-icosahedron sphere primitive.
#
# Direct triangle-mesh sphere, NO SDF / marching-cubes intermediary. Vertex
# normals point exactly outward from center (smooth per-vertex normals on
# the analytic sphere — no voxel-quantization bumpiness).
#
# Tessellation count (subdivisions kwarg):
#   subdivisions=0    20 triangles,   12 verts  (icosahedron — visible facets)
#   subdivisions=1    80 triangles,   42 verts
#   subdivisions=2   320 triangles,  162 verts
#   subdivisions=3  1280 triangles,  642 verts
#   subdivisions=4  5120 triangles, 2562 verts  ← recommended sweet spot
#   subdivisions=5 20480 triangles,10242 verts
#
# Authored as a lev2.Geometry (attribute container) -> baked to an .ogeo
# chunkfile -> rebuilt at load as Geometry -> MicroMesh ->
# RigidPrimitive.updateWithMicroMesh -> createDrawableData(material).
###############################################################################

import math

from ork.hypergraph.asset_core import _register
from ork.hypergraph.assets.mesh._common import (
    MeshAsset, build_geometry, sphere_normals_binormals, compose_xf)


def _icosphere_verts_tris(radius, subdivisions):
  t = (1.0 + math.sqrt(5.0)) / 2.0
  verts = [
    [-1.0,   t, 0.0], [ 1.0,   t, 0.0], [-1.0,  -t, 0.0], [ 1.0,  -t, 0.0],
    [0.0, -1.0,   t], [0.0,  1.0,   t], [0.0, -1.0,  -t], [0.0,  1.0,  -t],
    [   t, 0.0,-1.0], [   t, 0.0, 1.0], [  -t, 0.0,-1.0], [  -t, 0.0, 1.0],
  ]

  def push_to_sphere(v):
    n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    s = radius / n
    return [v[0] * s, v[1] * s, v[2] * s]

  verts = [push_to_sphere(v) for v in verts]
  tris = [
    [ 0, 11,  5], [ 0,  5,  1], [ 0,  1,  7], [ 0,  7, 10], [ 0, 10, 11],
    [ 1,  5,  9], [ 5, 11,  4], [11, 10,  2], [10,  7,  6], [ 7,  1,  8],
    [ 3,  9,  4], [ 3,  4,  2], [ 3,  2,  6], [ 3,  6,  8], [ 3,  8,  9],
    [ 4,  9,  5], [ 2,  4, 11], [ 6,  2, 10], [ 8,  6,  7], [ 9,  8,  1],
  ]

  midpoint_cache = {}

  def midpoint_idx(a, b):
    key = (a, b) if a < b else (b, a)
    cached = midpoint_cache.get(key)
    if cached is not None:
      return cached
    va, vb = verts[a], verts[b]
    mid = push_to_sphere([(va[0] + vb[0]) * 0.5,
                          (va[1] + vb[1]) * 0.5,
                          (va[2] + vb[2]) * 0.5])
    idx = len(verts)
    verts.append(mid)
    midpoint_cache[key] = idx
    return idx

  for _ in range(subdivisions):
    new_tris = []
    for (a, b, c) in tris:
      ab = midpoint_idx(a, b)
      bc = midpoint_idx(b, c)
      ca = midpoint_idx(c, a)
      new_tris.append([a,  ab, ca])
      new_tris.append([b,  bc, ab])
      new_tris.append([c,  ca, bc])
      new_tris.append([ab, bc, ca])
    tris = new_tris

  return verts, tris


@_register
class IcoSphere(MeshAsset):
  """Subdivided-icosahedron sphere mesh — smooth per-vertex normals.

  Usage::

      ball = self.asset.IcoSphere("ball",
                                  radius=0.6,
                                  subdivisions=4,
                                  material=some_pbr_material)
      self.entity("ent_name",
          transform={"translation": vec3(0, 0, 0)},
          components=[SG.component(nodes={"n": {"drawable": ball}})])

  Tessellation via `subdivisions` kwarg (range [0, 6]). subdivisions=4 →
  5120 tris / 2562 verts is the recommended sweet spot. The mesh is authored
  as a lev2.Geometry, baked to an .ogeo chunkfile, and referenced by path
  from the reflected MeshGenData (so it round-trips through scene JSON)."""

  __slots__ = ()

  # Each subdivision quadruples the icosahedron's 20 faces.
  MAX_SUBDIVISIONS = 6

  @staticmethod
  def poly_count(subdivisions):
    """Triangle count for a subdivision level: 20 * 4**subdivisions."""
    return 20 * (4 ** int(subdivisions))

  @staticmethod
  def vert_count(subdivisions):
    """Vertex count for a subdivision level: 10 * 4**subdivisions + 2."""
    return 10 * (4 ** int(subdivisions)) + 2

  @classmethod
  def subdivisions_for_polys(cls, requested_polys, *, max_subdivisions=None):
    """Smallest subdivision level whose triangle count is >= requested_polys
    (the closest match at or above the request), clamped to
    [0, max_subdivisions]. Compose with the ctor::

        n = IcoSphere.subdivisions_for_polys(5000)   # -> 4 (5120 tris)
        IcoSphere(subdivisions=n, material=mat)
    """
    hi = cls.MAX_SUBDIVISIONS if max_subdivisions is None else int(max_subdivisions)
    for s in range(0, hi + 1):
      if cls.poly_count(s) >= requested_polys:
        return s
    return hi

  def __init__(self, *, radius=0.5, subdivisions=4,
               translation=None, orientation=None, scale=1.0, flip_winding=False, material=None):
    if not (0 <= int(subdivisions) <= 6):
      raise ValueError(
        f"IcoSphere: subdivisions must be in [0, 6]; got {subdivisions}")
    # material is REQUIRED for build() (mesh+material baked into one drawable), but OPTIONAL
    # for geometry-only use (material=None) — e.g. INSTANCED rendering via rigid_primitive(),
    # where the material is supplied at the draw site (build() then errors clearly).
    verts, tris        = _icosphere_verts_tris(float(radius), int(subdivisions))
    normals, binormals = sphere_normals_binormals(verts, float(radius))
    # local transform baked into the verts (e.g. translation=(0,radius,0) lifts the centered
    # sphere so its base sits at y=0); identity by default. flip_winding reverses the front
    # face (normals kept) for pipelines whose cull is opposite the default scene cull.
    geo                = build_geometry(verts, tris, normals=normals, binormals=binormals,
                                        transform=compose_xf(translation, orientation, scale),
                                        flip_winding=flip_winding)
    self._init(geo, material)


__all__ = ["IcoSphere"]
