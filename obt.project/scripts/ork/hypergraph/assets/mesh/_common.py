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


def binormals_from_normals(norms):
  """Generic per-vertex binormals for an arbitrary normal field: cross(N, up), falling back
  to cross(N, +X) where N is ~parallel to up. (Sphere meshes use sphere_normals_binormals;
  this is for cones / general meshes.)"""
  n     = np.asarray(norms, dtype=np.float32)
  up    = np.array([0.0, 1.0, 0.0], dtype=np.float32)
  bins  = np.cross(n, up)
  degen = np.linalg.norm(bins, axis=1) < 1e-6
  bins[degen] = np.cross(n[degen], np.array([1.0, 0.0, 0.0], dtype=np.float32))
  lens  = np.linalg.norm(bins, axis=1, keepdims=True)
  return (bins / np.where(lens < 1e-10, 1.0, lens)).astype(np.float32)


def compose_xf(translation=None, orientation=None, scale=1.0):
  """Compose a mtx4 from (translation: vec3, orientation: quat, scale: float). Returns None when
  all are identity (translation None/zero-implied, orientation None, scale 1.0) -> a no-op, so
  callers can pass it straight to build_geometry(transform=...) without paying for the transform."""
  from orkengine.core import vec3, quat, mtx4
  if translation is None and orientation is None and scale == 1.0:
    return None
  return mtx4.composed(translation if translation is not None else vec3(0.0, 0.0, 0.0),
                       orientation if orientation is not None else quat(),
                       scale)


def _pretransform(verts, normals, binormals, xf):
  """Bake a local mtx4 `xf` into mesh attrs (place/orient/size the model in its OWN space):
  verts get the full affine (v' = M·v); binormals (tangents) the linear 3x3; normals (covectors)
  the inverse-transpose 3x3. np.array(mtx4) is glm COLUMN-MAJOR ([col][row]) — verified by the
  ScatterSet xform round-trip — so glm M·v == (v4 @ Mnp)."""
  Mnp = np.array(xf).astype(np.float64)                 # (4,4), [col][row]
  M3  = Mnp[:3, :3]
  v   = np.asarray(verts, dtype=np.float64)
  vh  = np.column_stack([v, np.ones(len(v))])
  out_v = (vh @ Mnp)[:, :3].astype(np.float32)          # full affine
  def _renorm(a):
    l = np.linalg.norm(a, axis=1, keepdims=True)
    return (a / np.where(l < 1e-12, 1.0, l)).astype(np.float32)
  out_n = normals   if normals   is None else _renorm(np.asarray(normals,   np.float64) @ np.linalg.inv(M3).T)
  out_b = binormals if binormals is None else _renorm(np.asarray(binormals, np.float64) @ M3)
  return out_v, out_n, out_b


def build_geometry(verts, tris, normals=None, binormals=None, transform=None, flip_winding=False):
  """Author a triangle-mesh Geometry from vert/tri arrays (+ optional per-point normals /
  binormals). `transform` (a mtx4, or None) is a LOCAL transform PRE-APPLIED to every vert
  (and normals/binormals) at build time — to place/orient/size the model relative to its
  origin without touching anything downstream (e.g. lift a centered sphere so its base is y=0).
  `flip_winding` REVERSES each triangle's vertex order (swaps the front face) — NORMALS are
  unchanged (lighting unaffected), so it only flips backface culling: use it when a mesh reads
  inside-out under a given pipeline's cull convention."""
  if transform is not None:
    verts, normals, binormals = _pretransform(verts, normals, binormals, transform)
  geo = _lev2.Geometry()
  geo.point["P"] = np.asarray(verts, dtype=np.float32)
  if normals is not None:
    geo.point["N"] = np.asarray(normals, dtype=np.float32)
  if binormals is not None:
    geo.point["binormal"] = np.asarray(binormals, dtype=np.float32)
  tri = np.asarray(tris, dtype=np.int32).reshape(-1, 3)
  if flip_winding:
    tri = tri[:, ::-1]                       # reverse winding (front face flips; normals kept)
  geo.addPolys(tri.reshape(-1), sides=3)
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

  def rigid_primitive(self, ctx):
    """Build a RigidPrimitive from this asset's geometry — for INSTANCED rendering
    (RigidPrimitive.createInstancedNode), where the material is supplied at the draw site, so
    a material on the asset is optional (geometry-only). Distinct from build(), which bakes a
    single NON-instanced drawable. `ctx` is the live render Context the prim is GPU-built on."""
    geo = self._geo if self._geo is not None else _lev2.Geometry.read(self.gendata.geometry_path)
    if geo is None:
      raise RuntimeError(f"MeshAsset.rigid_primitive: no geometry ({self.gendata.geometry_path!r})")
    prim = _lev2.RigidPrimitive()
    prim.updateWithMicroMesh(geo.toMicroMesh(), ctx, _tokens.TRIANGLES)
    return prim


__all__ = ["MeshAsset", "build_geometry", "write_geometry", "compose_xf",
           "sphere_normals_binormals", "binormals_from_normals"]
