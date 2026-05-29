###############################################################################
# Shared mesh-asset plumbing.
#
# Authoring wrappers (IcoSphere, UvSphere) build a lev2.Geometry (the
# attribute-based geometry container), which is baked to an .ogeo chunkfile under
# ~/.staging/geocache and referenced BY PATH from a reflected MeshGenData
# (geometry is never inlined into scene JSON). On the JSON load path, the
# chunkfile is read back into a Geometry -> MicroMesh -> RigidPrimitive
# drawable, so mesh drawables survive the viewer's serialize→deserialize→
# render round-trip.
###############################################################################

import os
import numpy as np

from orkengine import lev2 as _lev2
from orkengine.core import Path as _Path, CrcStringProxy as _Crc

_tokens   = _Crc()
_geocache = None


def _geocache_dir():
  global _geocache
  if _geocache is None:
    d = _Path.expandPathString("<staging>/geocache")
    os.makedirs(d, exist_ok=True)
    _geocache = d
  return _geocache


def write_geometry(geo, asset_name):
  """Bake a Geometry to <staging>/geocache/<asset_name>.ogeo, return the path."""
  path = os.path.join(_geocache_dir(), f"{asset_name}.ogeo")
  geo.write(path)
  return path


def sphere_normals_binormals(verts, radius):
  """Outward per-vertex normals for an origin-centered sphere + derived
  binormals (matches the canonical procedural_pbr test helper)."""
  inv_r = 1.0 / radius
  norms = np.asarray(verts, dtype=np.float32) * inv_r
  up    = np.array([0.0, 1.0, 0.0], dtype=np.float32)
  bins  = np.cross(norms, up)
  degen = np.linalg.norm(bins, axis=1) < 1e-6
  bins[degen] = np.cross(norms[degen], np.array([1.0, 0.0, 0.0], dtype=np.float32))
  lens  = np.linalg.norm(bins, axis=1, keepdims=True)
  bins  = bins / np.where(lens < 1e-10, 1.0, lens)
  return norms.astype(np.float32), bins.astype(np.float32)


def build_geometry(verts, tris, normals=None, binormals=None):
  """Author a triangle-mesh Geometry from vert/tri arrays (+ optional
  per-point normals / binormals)."""
  geo = _lev2.Geometry()
  geo.point["P"] = np.asarray(verts, dtype=np.float32)
  if normals is not None:
    geo.point["N"] = np.asarray(normals, dtype=np.float32)
  if binormals is not None:
    geo.point["binormal"] = np.asarray(binormals, dtype=np.float32)
  geo.addPolys(np.asarray(tris, dtype=np.int32).reshape(-1), sides=3)
  return geo


def _drawable_from_geometry(geo, material):
  # `material` is a live gfx material (lev2.PBRMaterial) by this point.
  if material is None:
    raise RuntimeError("mesh drawable: a material is required")
  ctx  = _lev2.GfxEnv.ref.loadingContext()
  mesh = geo.toMicroMesh()
  prim = _lev2.RigidPrimitive()
  prim.updateWithMicroMesh(mesh, ctx, _tokens.TRIANGLES)
  # Attach via the material (not a baked pipeline) so the drawable does
  # per-pass technique selection (depth-prepass vs forward); a baked
  # FORWARD_PBR pipeline asserts on unbound PBR_COMMON during the prepass.
  return prim.createDrawableData(material)


class MeshAsset:
  """Baked-geometry mesh drawable. Authoring subclasses (IcoSphere, UvSphere)
  build a Geometry in __init__ and call self._init(geo, material). On the JSON
  load path, materialize_from_scenedata uses from_gendata(gendata, material)
  and rebuilds by reading the .ogeo chunkfile referenced by the gendata.

  Also the wrapper materialize resolves MeshGenData to (see
  ork.hypergraph.ecs.scene.assets.materialize_from_scenedata)."""

  __slots__ = ("gendata", "material", "_geo", "built")

  def _init(self, geo, material):
    # `material` is a PbrMaterial asset wrapper. Capture its asset name (for
    # the round-trip material reference) and resolve the live gfx material via
    # the read-only as_gfx_material property.
    mat_name = ""
    if material is not None and hasattr(material, "gendata"):
      mat_name = material.gendata.asset_name or ""
    self.gendata  = _lev2.MeshGenData(material_asset_name=mat_name)
    self.material = material.as_gfx_material if material is not None else None
    self._geo     = geo
    self.built    = None

  @classmethod
  def from_gendata(cls, gendata, material=None):
    inst          = MeshAsset.__new__(MeshAsset)
    inst.gendata  = gendata
    inst.material = material
    inst._geo     = None
    inst.built    = None
    return inst

  def build(self):
    if self.built is not None:
      return self.built
    if self._geo is not None:
      # author path: bake the geometry chunkfile so it round-trips, then
      # build the live drawable directly from the in-memory Geometry.
      name = self.gendata.asset_name or "_anon_mesh"
      self.gendata.geometry_path = write_geometry(self._geo, name)
      self.built = _drawable_from_geometry(self._geo, self.material)
    else:
      # load path: read the baked geometry back from disk.
      geo = _lev2.Geometry.read(self.gendata.geometry_path)
      if geo is None:
        raise RuntimeError(
          f"MeshAsset: geometry chunkfile missing: {self.gendata.geometry_path!r}")
      self.built = _drawable_from_geometry(geo, self.material)
    return self.built


__all__ = ["MeshAsset", "build_geometry", "write_geometry", "sphere_normals_binormals"]
