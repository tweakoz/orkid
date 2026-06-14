###############################################################################
# Cone — apex-up cone primitive (apex at +Y=height, base ring at y=0), with a
# base cap. Faceted sides (per-face normals -> crisp facets). Authored as a
# lev2.Geometry -> .ogeo chunkfile, exactly like IcoSphere; usable as a single
# baked drawable (build()) or — material-less — as instanced geometry via
# MeshAsset.rigid_primitive(ctx).
###############################################################################
import math
import numpy as np

from ork.hypergraph.asset_core import _register
from ork.hypergraph.assets.mesh._common import (
    MeshAsset, build_geometry, binormals_from_normals, compose_xf)


def _cone_verts_tris_norms(radius, height, segments, cap=True):
  """Faceted cone: `segments` side triangles (apex, p0, p1) + (if `cap`) a down-facing base
  cap. Base at y=0, apex at y=height -> sits ON the ground when placed at a surface point;
  the cap is hidden against the ground, so cap=False drops it (fewer tris, open bottom)."""
  verts, norms, tris = [], [], []
  apex = (0.0, height, 0.0)
  sl   = math.hypot(radius, height) or 1.0
  for j in range(segments):
    a0 = math.tau * j / segments
    a1 = math.tau * (j + 1) / segments
    p0 = (math.cos(a0) * radius, 0.0, math.sin(a0) * radius)
    p1 = (math.cos(a1) * radius, 0.0, math.sin(a1) * radius)
    am = (a0 + a1) * 0.5
    nrm = (math.cos(am) * height / sl, radius / sl, math.sin(am) * height / sl)  # outward + up
    base = len(verts)
    verts += [apex, p0, p1]
    norms += [nrm, nrm, nrm]
    tris.append([base, base + 1, base + 2])
  if cap:
    # base cap (down-facing), fan from center
    c = len(verts); verts.append((0.0, 0.0, 0.0)); norms.append((0.0, -1.0, 0.0))
    ring = []
    for j in range(segments):
      a = math.tau * j / segments
      ring.append(len(verts))
      verts.append((math.cos(a) * radius, 0.0, math.sin(a) * radius)); norms.append((0.0, -1.0, 0.0))
    for j in range(segments):
      # reversed vs the ring order so the cap's shared base edges run OPPOSITE the side faces'
      # (consistent outward orientation — the cap winds the same handedness as the sides).
      tris.append([c, ring[(j + 1) % segments], ring[j]])
  return verts, tris, norms


@_register
class Cone(MeshAsset):
  """Cone mesh (apex at +Y, base on y=0). `material=` is REQUIRED for build() (a single baked
  drawable) but OPTIONAL for geometry-only / INSTANCED use via rigid_primitive()."""

  __slots__ = ()

  def __init__(self, *, radius=0.5, height=1.0, segments=12, cap=True,
               translation=None, orientation=None, scale=1.0, flip_winding=False, material=None):
    if int(segments) < 3:
      raise ValueError(f"Cone: segments must be >= 3; got {segments}")
    verts, tris, norms = _cone_verts_tris_norms(float(radius), float(height), int(segments), cap=bool(cap))
    norms = np.asarray(norms, dtype=np.float32)
    # local transform baked into the verts (place/orient/size the model; identity by default).
    geo = build_geometry(verts, tris, normals=norms, binormals=binormals_from_normals(norms),
                         transform=compose_xf(translation, orientation, scale), flip_winding=flip_winding)
    self._init(geo, material)


__all__ = ["Cone"]
