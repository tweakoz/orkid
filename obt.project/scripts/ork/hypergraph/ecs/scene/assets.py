###############################################################################
# ork.ecs.scene.assets — HYPERECS M2a asset gens (Python-eager).
#
# Each asset gen is a plain Python class with a kwargs constructor and a
# .build() instance method. .build() does the actual work eagerly and
# returns the produced artifact (mesh tuple, vdb FloatGrid, drawable, ...).
#
# Two call sites:
#
#   1. Tier 1/2 standalone (any orkid Python script):
#        sdf = MeshToSdf(input_mesh=HollowFunnelMesh(...), voxel_size=0.08).build()
#
#   2. Tier 3 Scene composite (M2a — eager):
#        funnel = self.asset.HollowFunnelMesh("funnel", top_outer=8.0, ...)
#        sdf    = self.asset.MeshToSdf("sdf", input_mesh=funnel, voxel_size=0.08)
#        drw    = self.asset.VdbGridToDrawable("drw", grid=sdf, material=mat)
#      The Scene-side namespace caches by name (uniqueness enforced in Pass 1)
#      and returns the BUILT artifact, not the gen instance. Downstream
#      consumers therefore see real objects (verts/tris tuple, FloatGrid,
#      Drawable) regardless of which call site produced them.
#
# Where inputs may be either (a) a pre-built artifact or (b) another asset
# gen instance, _materialize() handles both — calling .build() if needed.
# Standalone chains work without any Scene; Scene-mediated chains pass the
# already-built artifacts.
#
# M2b (future) will replace these with reflected C++ wrappers (AssetGenData
# subclasses) so the gen graph round-trips through JSON. The author surface
# (class names, kwargs, .build()) is intended to remain stable across that
# transition.
###############################################################################

import math
from typing import TypedDict

from orkengine.core import vec2, vec3, vec4, CrcStringProxy

# Imported lazily inside methods to avoid an __init__→assets→__init__ cycle:
#   ork.ecs.scene.__init__ imports assets at module-load time; assets
#   reaches back for _Hsv only at PbrMaterial-construction time.
from orkengine.lev2 import vdb
from orkengine import lev2 as _lev2
from orkengine.lev2 import (ImplicitSdfGenData,
                            VdbGridToDrawableGenData,
                            PbrMaterialGenData,
                            FreestyleMaterialGenData,
                            ParticleSystemGenData,
                            HeightFieldGenData,
                            HypermeshGenData,
                            HdriToXirGenData,
                            VdbFileSdfGenData,
                            MeshSdfGenData,
                            ParticlesDrawableData,
                            HypermeshDrawableData,
                            ScatterSinkData)

_tokens = CrcStringProxy()


###############################################################################
# Asset registry — populated by @_register decorator below. Used by the
# Scene.asset.<GenName> namespace to resolve attribute access into the
# corresponding class.
###############################################################################

_ASSET_REGISTRY = {}


def _register(cls):
  _ASSET_REGISTRY[cls.__name__] = cls
  return cls


def _materialize(x):
  """Return the built artifact for x. Three input forms:
    - asset wrapper with cached .built (factory-built; common case)
    - asset wrapper with .build() but no cache (standalone Tier 1/2)
    - already-built artifact (FloatGrid, material, drawable_data, ...)

  Wrappers can't be naive `.build()`-each-call: build() is non-
  idempotent (uploads GPU buffers, compiles shaders), so a hot path
  that materializes the same input twice would burn GPU work. Factories
  stash the artifact on `wrap._built` for callers; this helper prefers
  that cache and only falls back to .build() for standalone use."""
  if hasattr(x, "built") and x.built is not None:
    return x.built
  if hasattr(x, "build") and callable(x.build):
    return x.build()
  return x


###############################################################################
# HollowFunnelMesh — closed triangle mesh of a hollow truncated cone.
#
# Produces ((verts, tris)) — both as plain Python lists. The mesh is closed
# (outer wall + inner wall + top annular cap + bottom annular cap, all
# CCW-from-outside), suitable for meshToLevelSet downstream. Identical
# topology + windings to the original col_vdb.py _build_funnel_grid path.
#
# Tuning:
#   top_outer/top_inner — mouth radii (outer wall vs. opening at top)
#   bot_outer/bot_inner — spout radii (bot_inner = particle exit hole)
#   top_y, bot_y         — funnel vertical extent
#   segments             — radial tessellation; 48 = visually smooth
###############################################################################

@_register
class HollowFunnelMesh:
  """Closed triangle mesh of a hollow truncated cone (Python-only).

  No reflected gendata: this class outputs a (verts, tris) tuple, not a
  grid, and round-trip support for the mesh path is deferred (M2b
  reflects only SDF assets via ImplicitSdfGenData). For round-trip
  scenes, prefer an ImplicitSdf-based shape (e.g. ThickSaddleSdf) or
  pre-bake to a .vdb file via a future VdbFile gen.
  """

  def __init__(self, *,
               top_outer=8.0, top_inner=3.6,
               bot_outer=0.8, bot_inner=0.4,
               top_y=3.5,     bot_y=0.0,
               segments=48):
    self.top_outer = top_outer
    self.top_inner = top_inner
    self.bot_outer = bot_outer
    self.bot_inner = bot_inner
    self.top_y     = top_y
    self.bot_y     = bot_y
    self.segments  = segments

  def build(self):
    seg = self.segments
    verts = []
    tris  = []

    R0, R1, R2, R3 = 0, seg, 2*seg, 3*seg

    for radius, y in [(self.top_outer, self.top_y),
                      (self.top_inner, self.top_y),
                      (self.bot_outer, self.bot_y),
                      (self.bot_inner, self.bot_y)]:
      for s in range(seg):
        a = 2.0 * math.pi * s / seg
        verts.append(vec3(radius * math.cos(a), y, radius * math.sin(a)))

    def quad(a0, a1, b1, b0):
      tris.append((a0, a1, b1))
      tris.append((a0, b1, b0))

    for s in range(seg):
      sn = (s + 1) % seg
      # Outer wall (top → bot, outward-facing)
      quad(R0 + s,  R0 + sn,  R2 + sn,  R2 + s)
      # Inner wall (bot → top, inward-facing — into the hollow)
      quad(R3 + s,  R3 + sn,  R1 + sn,  R1 + s)
      # Top annular cap (faces +Y)
      quad(R0 + sn, R0 + s,   R1 + s,   R1 + sn)
      # Bottom annular cap (faces −Y)
      quad(R2 + s,  R2 + sn,  R3 + sn,  R3 + s)

    return (verts, tris)


def _write_obj(path, verts, tris):
  """Minimal Wavefront OBJ writer — verts as `v x y z`, tris as 1-based `f a b c`.
  Used by mesh-producing wrappers (HollowFunnelMesh, ...) to materialize their
  output to disk so MeshSdfGenData has a stable path it can re-read at load time."""
  with open(path, "w") as f:
    f.write("# orkid asset-gen mesh\n")
    for v in verts:
      f.write(f"v {v.x} {v.y} {v.z}\n")
    for t in tris:
      f.write(f"f {t[0]+1} {t[1]+1} {t[2]+1}\n")


def _read_obj(path):
  """Minimal Wavefront OBJ reader — pairs with _write_obj. Skips non-tri faces;
  v/f only (no vn/vt/groups). Returns (verts, tris)."""
  verts = []
  tris  = []
  with open(path, "r") as f:
    for line in f:
      tok = line.split()
      if not tok:
        continue
      if tok[0] == "v":
        verts.append(vec3(float(tok[1]), float(tok[2]), float(tok[3])))
      elif tok[0] == "f":
        # OBJ face indices are 1-based and may carry /vt/vn — strip those.
        def _idx(s):
          return int(s.split("/")[0]) - 1
        ids = [_idx(s) for s in tok[1:]]
        if len(ids) == 3:
          tris.append(tuple(ids))
        elif len(ids) == 4:
          tris.append((ids[0], ids[1], ids[2]))
          tris.append((ids[0], ids[2], ids[3]))
  return verts, tris


###############################################################################
# MeshToSdf — rasterize a closed triangle mesh into a narrow-band SDF.
#
# input_mesh:  either a (verts, tris) tuple OR another asset gen whose
#              .build() returns that tuple (e.g. HollowFunnelMesh).
# Output:      vdb_floatgrid_ptr_t (GRID_LEVEL_SET), ready for VdbCollider
#              and/or VdbGridToDrawable downstream.
#
# Wraps lev2.vdb.meshToLevelSet. The input mesh MUST be closed — open
# surfaces produce garbage inside/outside in the level-set rasterization.
###############################################################################

@_register
class MeshToSdf:

  def __init__(self, *, input_mesh, voxel_size=0.08, half_width=3.0):
    self.input_mesh = input_mesh
    self.voxel_size = voxel_size
    self.half_width = half_width

  def build(self):
    verts, tris = _materialize(self.input_mesh)
    return vdb.meshToLevelSet(
        verts, tris,
        voxel_size=self.voxel_size,
        half_width=self.half_width)


###############################################################################
# VdbFileSdf — load a pre-baked .vdb file. Reflected, round-trippable.
#
# Use when the SDF is already on disk (heavy mesh that's slow to revoxelize,
# Houdini export, or a previous bake we want to cache). The recipe is just
# the file path + grid name; the .vdb itself carries voxel size / transform.
###############################################################################

@_register
class VdbFileSdf:
  """Wraps VdbFileSdfGenData. .build() opens the .vdb and returns the FloatGrid."""

  def __init__(self, *, vdb_path=None, grid_name="sdf", gendata=None):
    if gendata is not None:
      self.gendata = gendata
      return
    if not vdb_path:
      raise ValueError("VdbFileSdf requires vdb_path= (path to a .vdb file)")
    import os as _os
    expanded = _os.path.expandvars(_os.path.expanduser(str(vdb_path)))
    self.gendata = VdbFileSdfGenData(
        vdb_path  = expanded,
        grid_name = grid_name)

  @classmethod
  def from_gendata(cls, gendata):
    return cls(gendata=gendata)

  def build(self):
    d = self.gendata
    return vdb.FloatGrid.loadFromVDB(d.grid_name, d.vdb_path)


###############################################################################
# MeshSdf — voxelize a triangle mesh loaded from disk into a narrow-band SDF.
#
# Two construction modes (mutually exclusive):
#   mesh_path=...    → use a .obj already on disk verbatim.
#   input_mesh=wrap  → call wrap.build() to produce (verts, tris), write
#                       them to a deterministic cache .obj, and store that
#                       path on the gendata.
#
# The cache .obj lands at <assetcache>/meshtemp/<asset_name>.obj. The
# materialize step at load time only needs the reflected mesh_path —
# it does not re-run the mesh recipe.
###############################################################################

@_register
class MeshSdf:
  """Wraps MeshSdfGenData. .build() reads the .obj at mesh_path and voxelizes.

  Two construction modes (mutually exclusive):
    mesh_path=...    → use a .obj already on disk verbatim. Path is stored
                       on the reflected gendata and round-trips as-is.
    input_mesh=wrap  → mesh is produced at build() time and written to a
                       deterministic cache .obj keyed by the gendata's
                       asset_name. The Scene-side factory sets asset_name
                       AFTER __init__ runs, so .obj writing has to happen
                       in build(), not __init__.
  """

  def __init__(self, *,
               mesh_path=None,
               input_mesh=None,
               voxel_size=0.08,
               half_width=3.0,
               grid_name="sdf",
               gendata=None):
    if gendata is not None:
      self.gendata = gendata
      self._input_mesh = None
      return
    if mesh_path is not None and input_mesh is not None:
      raise ValueError("MeshSdf: pass either mesh_path= or input_mesh=, not both")
    if mesh_path is None and input_mesh is None:
      raise ValueError("MeshSdf: need mesh_path= or input_mesh=")
    resolved = ""
    if mesh_path is not None:
      import os as _os
      resolved = _os.path.expandvars(_os.path.expanduser(str(mesh_path)))
    self.gendata = MeshSdfGenData(
        mesh_path  = resolved,
        voxel_size = float(voxel_size),
        half_width = float(half_width),
        grid_name  = grid_name)
    self._input_mesh = input_mesh  # wrapper or (verts, tris)

  @classmethod
  def from_gendata(cls, gendata):
    return cls(gendata=gendata)

  def build(self):
    d = self.gendata
    if not d.mesh_path:
      # input_mesh path: bake the .obj into <assetcache>/meshtemp/<asset_name>.obj
      # and stash the path on the gendata so it round-trips.
      if self._input_mesh is None:
        raise RuntimeError(
          "MeshSdf.build: gendata has no mesh_path and no input_mesh "
          "wrapper was supplied; deserialized data must carry a mesh_path")
      cache_key = d.asset_name
      if not cache_key:
        raise RuntimeError(
          "MeshSdf.build: gendata.asset_name is empty; the Scene factory "
          "must set asset_name before build() runs")
      import os as _os
      from orkengine.core import Path as _Path
      cache_root = _Path.expandPathString("<assetcache>/meshtemp")
      _os.makedirs(cache_root, exist_ok=True)
      verts, tris = _materialize(self._input_mesh)
      cache_path = _os.path.join(cache_root, f"{cache_key}.obj")
      _write_obj(cache_path, verts, tris)
      d.mesh_path = cache_path
    verts, tris = _read_obj(d.mesh_path)
    return vdb.meshToLevelSet(
        verts, tris,
        voxel_size=d.voxel_size,
        half_width=d.half_width)


###############################################################################
# VdbGridToDrawable — one-shot marching-cubes of an SDF/level-set into a
# rendered Drawable. Wraps lev2.vdb.gridToDrawable (one-shot helper that
# shares the renderer's mesh-extraction algorithm).
#
# grid:     either a vdb_floatgrid_ptr_t OR another asset gen whose
#           .build() returns that grid (e.g. MeshToSdf).
# material: a lev2 material (e.g. shaders.createPbrMaterialWithColor).
# iso:      level-set iso value to extract (default 0 — the surface).
#
# Returns a drawable_ptr_t suitable as drawable= in
# SceneGraphComponent.declareNodeOnLayer.
###############################################################################

@_register
class VdbGridToDrawable:
  """Python wrapper around VdbGridToDrawableGenData (M2b.3).

  Reflected fields (iso, adaptivity, flip_windings, grid_asset_name)
  live on `self.gendata` and survive JSON round-trip. The grid input
  is captured BY NAME when it's an asset gen instance (the gen's
  gendata.asset_name → gendata.grid_asset_name) — that's the cross-
  asset reference convention. Material is held outside the reflected
  data (still Python-only).

  At build() time, the grid is sourced from whichever path is live:
    - if a built grid was passed directly, use it (eager / standalone)
    - if a gen instance was passed, materialize it (chains)
    - name-based resolution via a Scene asset registry is M2b.4.
  """

  def __init__(self, *, grid=None, material=None,
               iso=0.0, adaptivity=0.0, flip_windings=False,
               gendata=None):
    if gendata is not None:
      # Wrap an existing reflected data object (deserialized path).
      self.gendata  = gendata
      self.material = material
      self._grid    = None     # name-resolution path
      return

    # Capture cross-asset names if the inputs are wrappers (have
    # .gendata.asset_name). Plain built artifacts → name stays empty.
    grid_asset_name     = ""
    material_asset_name = ""
    if grid is not None and hasattr(grid, "gendata"):
      grid_asset_name = grid.gendata.asset_name or ""
    if material is not None and hasattr(material, "gendata"):
      material_asset_name = material.gendata.asset_name or ""

    self.gendata = VdbGridToDrawableGenData(
        grid_asset_name     = grid_asset_name,
        material_asset_name = material_asset_name,
        iso                 = iso,
        adaptivity          = adaptivity,
        flip_windings       = flip_windings)
    # Stash the live inputs for the eager build() path. Unwrap material
    # immediately (build() passes it straight to vdb.gridToDrawable
    # which expects material_ptr_t, not a wrapper); _grid stays as-is
    # since build() runs it through _materialize anyway.
    self._grid    = grid
    self.material = _materialize(material) if material is not None else None

  @classmethod
  def from_gendata(cls, gendata, material=None, grid=None):
    """Wrap a deserialized VdbGridToDrawableGenData. `grid` should be
    the already-built FloatGrid referenced by gendata.grid_asset_name
    (resolved by the caller, e.g. materialize_from_scenedata); pass it
    explicitly so build() doesn't need a scene registry lookup."""
    inst = cls(gendata=gendata, material=material)
    inst._grid = grid
    return inst

  def build(self):
    d = self.gendata
    g = _materialize(self._grid)
    if g is None:
      raise RuntimeError(
        f"VdbGridToDrawable.build: no grid available (grid_asset_name="
        f"{d.grid_asset_name!r}); name resolution lands in M2b.4")
    if self.material is None:
      raise RuntimeError(
        "VdbGridToDrawable.build: material is required (kept out of the "
        "reflected gendata; supply it via __init__ or from_gendata)")
    return vdb.gridToDrawable(
        g, self.material,
        iso           = d.iso,
        adaptivity    = d.adaptivity,
        flip_windings = d.flip_windings)


###############################################################################
# ImplicitSdf — voxelize an OpenVDB AX volume shader into a FloatGrid.
#
# AX is OpenVDB's JIT-compiled shading language. The shader runs once per
# active voxel; the user code writes the implicit-function value into the
# grid's value attribute (`f@<grid_name>`). Read world-space position via
# `vec3f@pos = getvoxelpws();`. CustomData scalars are accessible as
# `f$<name>` (float) / `i$<name>` (int). See
# ork.lev2/pyext/tests/renderer/datasources/vdb_ax_*.py for runnable AX
# examples; AX builtins include abs, max, min, sqrt, cos, sin, pow, ...
#
# Build path:
#   1. Create a named FloatGrid (the name is what the shader writes to —
#      e.g. grid_name="sdf" ⇒ shader writes `f@sdf = ...`). Background is
#      a positive "very far outside" value so any voxel outside the bbox
#      reads as exterior to a downstream marching-cubes / collider.
#   2. fillBBox to ACTIVATE every voxel inside the requested world bbox.
#      AX only iterates active voxels, so the bbox here is the eval
#      domain. Cost is O(voxels) = bbox_volume / voxel_size**3.
#   3. Compile the AX shader + populate CustomData.
#   4. ve.executeOnGrid(grid) — fills every active voxel via the shader.
#
# Notes / caveats:
#   - The output is a TRUE level set only if the shader writes true signed
#     distance. For collision (VdbCollider) and marching-cubes extraction,
#     any signed implicit value with the right sign pattern works — the
#     zero crossings define the surface correctly. Magnitudes only affect
#     collision pushback distance + narrow-band thickness.
#   - The eval bbox is dense — fully activated. Pick voxel_size so total
#     voxel count stays sane (1M voxels at 0.1 = ~10 m^3 of coverage).
###############################################################################

@_register
class ImplicitSdf:
  """Python wrapper around the reflected ImplicitSdfGenData (M2b).

  __init__ kwargs lower 1:1 into reflected fields on `self.gendata`.
  `.build()` reads fields off the gendata and runs the AX voxelizer.
  Round-trip path:
    js   = isdf.gendata.serializeJson()
    gd2  = Object.deserializeJson(js)
    isdf2 = ImplicitSdf.from_gendata(gd2)
    isdf2.build() == isdf.build()  (semantically — both produce the same SDF)

  The background value is resolved at __init__ time (not at build time)
  so the value the reflected data carries is concrete. If the caller
  passes background=None, the wrapper computes the bbox-diagonal length
  and stores that on the gendata.
  """

  def __init__(self, *,
               shader=None,
               bbox_min=None, bbox_max=None,
               voxel_size=0.1,
               background=None,
               grid_name="sdf",
               params=None,
               gendata=None):
    if gendata is not None:
      # Wrap an existing reflected data object (deserialized path).
      self.gendata = gendata
      return
    # Resolve auto-background here so the reflected data has a concrete
    # value (deserialized objects skip __init__ → no chance to resolve).
    bg = background
    if bg is None:
      d  = bbox_max - bbox_min
      bg = math.sqrt(d.x*d.x + d.y*d.y + d.z*d.z)
    self.gendata = ImplicitSdfGenData(
        shader     = shader or "",
        bbox_min   = bbox_min,
        bbox_max   = bbox_max,
        voxel_size = voxel_size,
        background = bg,
        grid_name  = grid_name,
        params     = params or {})

  @classmethod
  def from_gendata(cls, gendata):
    """Wrap an ImplicitSdfGenData (e.g. from Object.deserializeJson)."""
    return cls(gendata=gendata)

  def build(self):
    d = self.gendata
    xf = vdb.Transform.create(d.voxel_size)
    grid = vdb.FloatGrid.create(d.grid_name, xf, d.background)
    grid.fillBBox(d.bbox_min, d.bbox_max, d.background)

    cdata = vdb.ax.CustomData()
    # d.params is a VarMap (from the reflected ImplicitSdfGenData).
    # Iterate via keys() + __getitem__ — the VarMap pyext binding
    # encodes each value to the matching Python scalar (float/int).
    params = d.params
    for k in params.keys():
      cdata.set(k, params[k])
    ve = vdb.ax.VolumeExecutable.compile(d.shader, cdata)
    ve.executeOnGrid(grid)
    return grid


###############################################################################
# ThickSaddleSdf — hyperbolic-paraboloid slab via ImplicitSdf.
#
# Surface: y = saddle_coef * x * z (Y-up, matches the funnel scene's
# vertical-axis convention). Thickened to a slab of uniform thickness
# along the surface normal, then clipped to a finite x,z extent via box
# intersection. The half_extent_y of the activated voxel region is
# auto-sized to contain the saddle's max excursion plus the slab
# thickness plus a small margin.
#
# Distance approximation: the EXACT distance to y = a*x*z requires
# solving a quartic. Instead we use the first-order tangent-plane
# distance |F(p)| / |∇F(p)| with F(p) = y - a*x*z, ∇F = (-a*z, 1, -a*x).
# This is correct AT the surface (where it matters for marching cubes
# and collision), and slightly off in magnitude farther away — fine for
# rendering and SDF collision pushback.
#
# Output is a vdb_floatgrid_ptr_t (NOT a mesh, despite the parallel to
# HollowFunnelMesh) — plug directly into VdbGridToDrawable to render and/or
# pass to ColVdbSystem(funnel_sdf=...) as a collider.
###############################################################################

_SADDLE_SHADER = """
// Per-voxel world-space position.
vec3f@pp = getvoxelpws();
float@xx = vec3f@pp.x;
float@yy = vec3f@pp.y;
float@zz = vec3f@pp.z;

// Signed gap to the saddle surface y = a*x*z.
float@sgn = float@yy - f$a * float@xx * float@zz;

// Tangent-plane distance: |F| / |grad F|. grad(y - a*x*z) = (-a*z, 1, -a*x).
float@gx = f$a * float@zz;
float@gz = f$a * float@xx;
float@gm = sqrt(1.0 + float@gx*float@gx + float@gz*float@gz);
float@slab = abs(float@sgn / float@gm) - f$t * 0.5;

// Clip the saddle to a finite (x, z) extent — box intersection.
float@bx = abs(float@xx) - f$ex;
float@bz = abs(float@zz) - f$ez;
float@box = max(float@bx, float@bz);

// SDF = max(slab, box). Negative inside both → solid; positive elsewhere.
f@sdf = max(float@slab, float@box);
"""


@_register
class ThickSaddleSdf:
  """Saddle slab as a thin factory over ImplicitSdf.

  No reflected C++ type of its own — at construction time it composes
  an ImplicitSdf with the saddle shader + derived bbox + params, and
  holds that ImplicitSdf. `self.gendata` exposes the underlying
  ImplicitSdfGenData (the saddle's shape encoded as shader+params), so
  round-trip works via the SAME path as any other ImplicitSdf asset.
  """

  def __init__(self, *,
               half_extent_x = 4.0,
               half_extent_z = 4.0,
               saddle_coef   = 0.3,
               thickness     = 0.4,
               half_extent_y = None,
               voxel_size    = 0.08):
    if half_extent_y is None:
      # The saddle's max |y| inside the (x,z) box is at the corners:
      # |a| * half_extent_x * half_extent_z. Add half the slab thickness
      # plus a margin to keep the surface inside the activated region.
      half_extent_y = (abs(saddle_coef) * half_extent_x * half_extent_z
                       + thickness * 0.5 + 1.0)
    self._implicit = ImplicitSdf(
      shader     = _SADDLE_SHADER,
      bbox_min   = vec3(-half_extent_x, -half_extent_y, -half_extent_z),
      bbox_max   = vec3(+half_extent_x, +half_extent_y, +half_extent_z),
      voxel_size = voxel_size,
      params = {
        "a":  float(saddle_coef),
        "t":  float(thickness),
        "ex": float(half_extent_x),
        "ez": float(half_extent_z),
      })

  @property
  def gendata(self):
    """The reflected ImplicitSdfGenData this saddle resolved to. JSON
    round-tripping this is the round-trip path for the saddle."""
    return self._implicit.gendata

  def build(self):
    return self._implicit.build()


###############################################################################
# SphereSdf — sphere SDF as a thin factory over ImplicitSdf. Same pattern
# as ThickSaddleSdf: composes an ImplicitSdf at construct time, exposes
# the underlying gendata for round-trip. Useful for clean-curvature
# reflection / refraction test scenes where saddle geometry would confuse
# the visual diagnostic.
###############################################################################

_SPHERE_SHADER = """
vec3f@pp = getvoxelpws();
float@dx = vec3f@pp.x;
float@dy = vec3f@pp.y;
float@dz = vec3f@pp.z;
f@sdf = sqrt(float@dx*float@dx + float@dy*float@dy + float@dz*float@dz) - f$radius;
"""


@_register
class SphereSdf:
  """Sphere at origin. Wraps ImplicitSdf with a radius parameter.

  Usage:
      sph = self.asset.SphereSdf("ball", radius=2.0)
  """

  def __init__(self, *, radius=1.0, voxel_size=0.04, margin=0.2):
    self._implicit = ImplicitSdf(
      shader     = _SPHERE_SHADER,
      bbox_min   = vec3(-(radius + margin), -(radius + margin), -(radius + margin)),
      bbox_max   = vec3(+(radius + margin), +(radius + margin), +(radius + margin)),
      voxel_size = voxel_size,
      params     = {"radius": float(radius)})

  @property
  def gendata(self):
    return self._implicit.gendata

  def build(self):
    return self._implicit.build()


###############################################################################
# PbrMaterial — wraps PbrMaterialGenData. Build() consumes a Context
# (defaults to GfxEnv.ref.loadingContext()) and produces a live
# PBRMaterial. Mirrors lev2utils.shaders.createPbrMaterialWithColor
# but driven by the reflected recipe so round-trip is automatic.
###############################################################################

# PBR2 — TypedDict declarations per lobe. Used to give IDE / type-checker
# (and LLM-prompts that include these) a closed vocabulary for the nested
# authoring form. `total=False` so every key is optional.
#
# Naming convention inside a lobe: drop the lobe-name prefix that the flat
# form requires. E.g. flat `subsurface_factor` → nested `subsurface={"factor": ...}`.

class BaseLobe(TypedDict, total=False):
  color: vec4
  metallic: float
  roughness: float
  color_path: str
  normal_path: str
  mtlruf_path: str

class TransmissionLobe(TypedDict, total=False):
  factor: float
  roughness: float            # P3.D — separate transmission roughness

class VolumeLobe(TypedDict, total=False):
  thickness_factor: float
  attenuation_color: vec3
  attenuation_distance: float

class DiffuseTransmissionLobe(TypedDict, total=False):
  factor: float
  color: vec3

class SpecularLobe(TypedDict, total=False):
  factor: float
  color: vec3

class ClearcoatLobe(TypedDict, total=False):
  factor: float
  roughness: float

class SheenLobe(TypedDict, total=False):
  factor: float
  color: vec3
  roughness: float

class IridescenceLobe(TypedDict, total=False):
  factor: float

class SubsurfaceLobe(TypedDict, total=False):
  factor: float
  color: vec3
  radius: vec3

# Mapping from nested-form kwarg name → (lobe-name prefix, allowed inner keys).
# Each inner key maps to a flat name: `flat = prefix + "_" + inner` unless
# explicitly remapped (the "remap" dict overrides).
_NESTED_LOBES = {
  "base": {
    "remap": {"color": "base_color"},
    "passthrough": {"metallic", "roughness",
                    "color_path", "normal_path", "mtlruf_path"},
  },
  "transmission":         {"prefix": "transmission",         "keys": {"factor", "roughness"}},
  "volume":               {"prefix": "volume",               "keys": {"thickness_factor"},
                           "remap": {"attenuation_color":    "attenuation_color",
                                     "attenuation_distance": "attenuation_distance"}},
  "diffuse_transmission": {"prefix": "diffuse_transmission", "keys": {"factor", "color"}},
  "specular":             {"prefix": "specular",             "keys": {"factor", "color"}},
  "clearcoat":            {"prefix": "clearcoat",            "keys": {"factor", "roughness"}},
  "sheen":                {"prefix": "sheen",                "keys": {"factor", "color", "roughness"}},
  "iridescence":          {"prefix": "iridescence",          "keys": {"factor"}},
  "subsurface":           {"prefix": "subsurface",           "keys": {"factor", "color", "radius"}},
}


@_register
class PbrMaterial:

  # PBR2 Phase 2 — glTF KHR-extension lobe kwargs (all optional).
  # Default OFF: passing any <lobe>_factor or <lobe>_color implicitly
  # enables the corresponding has_<lobe> flag on the GenData ctor;
  # pass `has_<lobe>=False` to force-disable.
  _LOBE_KWARGS = (
    "has_transmission", "transmission_factor",
    "has_transmission_roughness", "transmission_roughness",
    "has_ior", "ior",
    "has_volume", "volume_thickness_factor",
    "has_diffuse_transmission", "diffuse_transmission_factor",
    "has_specular", "specular_factor",
    "has_clearcoat", "clearcoat_factor",
    "has_sheen", "sheen_factor",
    "has_iridescence", "iridescence_factor",
    "sheen_color", "specular_color",
    "attenuation_color", "diffuse_transmission_color",
    "clearcoat_roughness", "sheen_roughness", "attenuation_distance",
    # PBR2 Phase 3 (P3.D) — subsurface scattering.
    "has_subsurface", "subsurface_color", "subsurface_radius",
    "subsurface_factor",
  )

  # Set of top-level kwargs the dispatcher recognizes as "nested-form" blocks.
  # If any of these appear, the call is treated as nested form; mixing with
  # the corresponding flat kwarg names is rejected.
  _NESTED_KEYS = frozenset(_NESTED_LOBES.keys())

  @classmethod
  def _flatten_nested(cls, lobe_kwargs):
    """Translate any top-level nested-form blocks into the flat _LOBE_KWARGS
    surface. Raises on mixed form or unknown inner keys. Returns the merged
    flat-form dict."""
    nested_seen = {k: lobe_kwargs.pop(k) for k in list(lobe_kwargs)
                   if k in cls._NESTED_KEYS}
    out = dict(lobe_kwargs)  # flat fields stay as-is
    for lobe_name, block in nested_seen.items():
      if not isinstance(block, dict):
        raise TypeError(
          f"PbrMaterial: {lobe_name}= must be a dict (got {type(block).__name__})")
      spec = _NESTED_LOBES[lobe_name]
      prefix      = spec.get("prefix", lobe_name)
      allowed     = set(spec.get("keys", ()))
      passthrough = set(spec.get("passthrough", ()))
      remap       = dict(spec.get("remap", {}))
      for k, v in block.items():
        if k in remap:
          flat_name = remap[k]
        elif k in passthrough:
          flat_name = k
        elif k in allowed:
          flat_name = f"{prefix}_{k}"
        else:
          raise TypeError(
            f"PbrMaterial: unknown key {lobe_name}[{k!r}]; "
            f"valid keys for {lobe_name} are "
            f"{sorted(allowed | passthrough | set(remap.keys()))}")
        if flat_name in out:
          raise TypeError(
            f"PbrMaterial: mixed form — {lobe_name}[{k!r}] resolves to "
            f"flat kwarg {flat_name!r} which is already provided. "
            f"Use one form, not both.")
        out[flat_name] = v
    return out

  @staticmethod
  def _coerce_hsv(flat_kwargs, base_color):
    """In-place coerce color values to the slot's component count. Handles
    BOTH the lazy palette colors (colors.red — _LazyColor, auto-coerce) and
    the raw vec3/vec4 that hsv()/wavelength()/colortemp() now return (vec3
    when alpha omitted): vec4 slots upgrade vec3 → vec4(rgb, 1); vec3 slots
    downgrade an opaque vec4 and reject one carrying real alpha. Returns
    (flat_kwargs, base_color). The base_color positional kwarg is
    special-cased because it isn't in flat_kwargs."""
    from orkengine.core import vec3, vec4
    from ork.hypergraph.colors import _LazyColor, _PBR_VEC3_FIELDS, _PBR_VEC4_FIELDS
    if isinstance(base_color, _LazyColor):
      base_color = base_color.to_vec4()
    elif isinstance(base_color, vec3):
      base_color = vec4(base_color.x, base_color.y, base_color.z, 1.0)
    for k, v in list(flat_kwargs.items()):
      if isinstance(v, _LazyColor):
        if k in _PBR_VEC4_FIELDS:
          flat_kwargs[k] = v.to_vec4()
        else:
          # Known vec3 slot, or unknown color slot — default to vec3 +
          # alpha assertion. Covers future glTF lobe additions that take
          # vec3 colors by convention.
          flat_kwargs[k] = v.to_vec3(field_name=k)
      elif isinstance(v, vec3) and k in _PBR_VEC4_FIELDS:
        flat_kwargs[k] = vec4(v.x, v.y, v.z, 1.0)
      elif isinstance(v, vec4) and k in _PBR_VEC3_FIELDS:
        if v.w != 1.0:
          raise ValueError(
            f"{k!r} is a 3-component color slot with no alpha channel; "
            f"got vec4 with a={v.w}. Drop the a= argument.")
        flat_kwargs[k] = vec3(v.x, v.y, v.z)
    return flat_kwargs, base_color

  def __init__(self, *,
               base_color=None, metallic=0.0, roughness=1.0,
               color_path="", normal_path="", mtlruf_path="",
               gendata=None,
               **lobe_kwargs):
    """PbrMaterial author surface. Two equivalent forms:

      Flat (LLM-friendly, glTF-aligned):
        PbrMaterial("foo", subsurface_color=..., subsurface_factor=..., ...)

      Nested (human-friendly, mirrors glTF JSON structure):
        PbrMaterial("foo", subsurface={"color": ..., "factor": ...}, ...)

    Forms can be mixed across DIFFERENT lobes but not within one lobe —
    e.g. transmission={"factor": 0.5} + sheen_color=... is fine, but
    transmission={"factor": 0.5} + transmission_factor=... is rejected.

    Colors authored via hsv(h, s, v, a=1) auto-convert to vec3 or vec4
    based on the target slot (vec3 slots assert alpha == 1.0).

    The `base` dict can override metallic/roughness/etc. via the nested form;
    if neither `base` nor `base_color` is given, defaults apply.
    See `_NESTED_LOBES` for the full inner-key vocabulary per lobe and
    `TransmissionLobe` / `SubsurfaceLobe` / etc. TypedDict declarations
    for IDE-side autocompletion."""
    if gendata is not None:
      self.gendata = gendata
      return
    # Flatten any nested-form lobe blocks. Errors on unknown keys or mixed form.
    lobe_kwargs = self._flatten_nested(lobe_kwargs)
    # Auto-coerce any hsv() values to vec3/vec4 based on field name.
    lobe_kwargs, base_color = self._coerce_hsv(lobe_kwargs, base_color)
    # Pull base-block overrides out before the kwargs reach _LOBE_KWARGS.
    if "base_color"  in lobe_kwargs and base_color is None:
      base_color  = lobe_kwargs.pop("base_color")
    if "metallic"    in lobe_kwargs:
      metallic    = lobe_kwargs.pop("metallic")
    if "roughness"   in lobe_kwargs:
      roughness   = lobe_kwargs.pop("roughness")
    if "color_path"  in lobe_kwargs:
      color_path  = lobe_kwargs.pop("color_path")
    if "normal_path" in lobe_kwargs:
      normal_path = lobe_kwargs.pop("normal_path")
    if "mtlruf_path" in lobe_kwargs:
      mtlruf_path = lobe_kwargs.pop("mtlruf_path")
    extras = {}
    for k in self._LOBE_KWARGS:
      if k in lobe_kwargs:
        extras[k] = lobe_kwargs.pop(k)
    if lobe_kwargs:
      raise TypeError(f"PbrMaterial: unexpected kwargs {sorted(lobe_kwargs)}")
    self.gendata = PbrMaterialGenData(
        base_color  = base_color if base_color is not None else vec4(1, 1, 1, 1),
        metallic    = float(metallic),
        roughness   = float(roughness),
        color_path  = color_path,
        normal_path = normal_path,
        mtlruf_path = mtlruf_path,
        **extras)
    self._ctx = None

  @classmethod
  def from_gendata(cls, gendata, ctx=None):
    inst = cls(gendata=gendata)
    inst._ctx = ctx
    return inst

  @property
  def as_gfx_material(self):
    """The live PBRMaterial (material_ptr_t), built on first access.

    Read-only property form of _materialize: prefers the factory-cached
    `.built`, otherwise builds once and caches. build() is non-idempotent
    (gpuInit uploads GPU buffers), so it must not run twice — hence the cache.

        mat = self.asset.PbrMaterial("m", base_color=..., roughness=0.4)
        gfxmat = mat.as_gfx_material      # -> live lev2.PBRMaterial
    """
    built = getattr(self, "built", None)
    if built is not None:
      return built
    self.built = self.build()
    return self.built

  def build(self):
    d   = self.gendata
    ctx = getattr(self, "_ctx", None) or _lev2.GfxEnv.ref.loadingContext()
    # D.1: the materialize step is C++ (PbrMaterialGenData::materialize — a 1:1 port of the body
    # below). The Python body is RETAINED as the reference implementation during the transition
    # (ORK_HM_PYMAT=1 selects it; removed only at proven parity, per the plan's no-regression rule).
    import os as _os
    if not _os.environ.get("ORK_HM_PYMAT"):
      return d.materialize(ctx)
    mat = _lev2.PBRMaterial()
    mat.name = d.asset_name or "pbr"
    # Color / normal / metallic-roughness: load from path if specified,
    # otherwise use the procedural solid-color defaults that
    # shaders.createPbrMaterialWithColor uses.
    img_color  = (_lev2.Image.createFromFile(d.color_path)
                  if d.color_path  else _lev2.Image.createRGB8FromColor(64, 64, vec3(1.0)))
    img_normal = (_lev2.Image.createFromFile(d.normal_path)
                  if d.normal_path else _lev2.Image.createRGB8FromColor(64, 64, vec3(0.5, 1.0, 0.5)))
    img_mtlruf = (_lev2.Image.createFromFile(d.mtlruf_path)
                  if d.mtlruf_path else _lev2.Image.createRGB8FromColor(64, 64, vec3(1.0)))
    mat.assignImages(ctx,
                     color=img_color, normal=img_normal,
                     mtlruf=img_mtlruf, doConform=True)
    mat.metallicFactor  = d.metallic
    mat.roughnessFactor = d.roughness
    mat.baseColor       = d.base_color
    # PBR2 Phase 2 — 8 glTF KHR-extension lobes. Mirror GenData fields
    # onto the live PBRMaterial; the pyext bindings on PBRMaterial use
    # the same snake_case names as the GenData.
    for _k in self._LOBE_KWARGS:
      setattr(mat, _k, getattr(d, _k))
    # GEOV2 Phase 3 — procedural shaderpath override (ptex3d). The generated
    # .fxv2 must be set BEFORE gpuInit (the internal FreestyleMaterial loads it
    # there); the bindable uniform_block params only resolve AFTER, so pre-bind
    # their round-tripped defaults post-gpuInit (they propagate into pipelines
    # at pipeline-creation, before first draw). See [[project-geometry-type]].
    shaderpath = getattr(d, "shaderpath", "") or ""
    if shaderpath:
      mat.shaderpath = shaderpath
    mat.gpuInit(ctx)
    if shaderpath:
      sp = getattr(d, "shader_params", None)
      if sp is not None:
        for k in sp.keys():
          mat.bindParam(k, sp[k])
    # D.4: sampler→texture bindings (parity with the C++ materializer)
    from orkengine.core import Path as _Path
    for sampler, tpath in (d.sampler_textures or {}).items():
      tpath = _Path.expandPathString(tpath)
      if not _os.path.isfile(tpath):
        print(f"PbrMaterial {d.asset_name!r}: sampler {sampler!r} texture {tpath!r} MISSING — skipped", flush=True)
        continue
      img = _lev2.Image.createFromFile(tpath)
      tex = _lev2.Texture(sampler)
      ctx.TXI.updateTexture(tex, img, False)
      mat.bindParam(sampler, tex)
    return mat


###############################################################################
# Ptex3d — procedural-surface material authored as a ptex3d DSL class
# (GEOV2 Phase 3). Materializes the DSL to a cached .fxv2, drives a PBRMaterial
# via shaderpath, and round-trips through the scene as a PbrMaterialGenData
# carrying shaderpath + the bindable-param defaults (shader_params). No new
# reflected class — it deserializes straight back to PbrMaterial.
#
#   from ork.hypergraph.ptex3d import Ptex3d as Ptex3dBase, P, rgb
#   class CastMetal(Ptex3dBase):
#     def __init__(self, ctx, *, cell_scale=4.0):
#       base = ctx.param("base_color", (0.35, 0.37, 0.40))   # bindable uniform
#       cell = P.voronoi(ctx.P_object * cell_scale)
#       self.surface(albedo=base * P.mix(0.45, 1.0, cell.cell),
#                    metallic=1.0, roughness=P.mix(0.0, 0.9, cell.cell2))
#
#   mat = self.asset.Ptex3d("steel", dsl_class=CastMetal, cell_scale=6.0)
#   mat.as_gfx_material.bindParam("base_color", vec4(0.8, 0.2, 0.1, 1))  # live
###############################################################################

def _ptex_param_defaults(pspecs):
  """[(name, gtype, default), ...] -> {name: vec/float} for the shader_params
  varmap (the pre-bound uniform defaults that round-trip in the GenData)."""
  from ork.hypergraph.colors import _LazyColor
  from orkengine.core import CrcString
  out = {}
  for (name, gtype, default) in pspecs:
    if isinstance(default, CrcString):
      # a named fx-pipeline PROVIDER token (e.g. tokens.RCFD_TIME). The value is supplied per-frame
      # by the engine, NOT a constant — bind the crcstring as-is (fx_pipeline resolves it); gtype is
      # only the uniform-slot type. Round-trips through the shader_params varmap codec by hash.
      out[name] = default
      continue
    if isinstance(default, _LazyColor):
      default = default.to_vec4() if gtype == "vec4" else default.to_vec3()
    # hsv()/wavelength()/colortemp() return raw vec3 (no alpha) — upgrade
    # opaque into a vec4 slot; mirror the downgrade for an opaque vec4.
    elif isinstance(default, vec3) and gtype == "vec4":
      default = vec4(default.x, default.y, default.z, 1.0)
    elif isinstance(default, vec4) and gtype == "vec3":
      default = vec3(default.x, default.y, default.z)
    if gtype == "float":
      out[name] = float(default)
    elif gtype == "vec2":
      out[name] = default if isinstance(default, vec2) else vec2(*default)
    elif gtype == "vec3":
      out[name] = default if isinstance(default, vec3) else vec3(*default)
    elif gtype == "vec4":
      out[name] = default if isinstance(default, vec4) else vec4(*default)
  return out


@_register
class Ptex3d:

  def __init__(self, *, dsl_class=None, gendata=None, **kwargs):
    if gendata is not None:
      self.gendata = gendata
      self._ctx = None
      return
    if dsl_class is None:
      raise TypeError("Ptex3d requires dsl_class= (a ptex3d.dsl.Ptex3d subclass)")
    # Split the glTF PBR-lobe kwargs (transmission / ior / clearcoat / sheen /
    # subsurface / specular / iridescence / volume / attenuation / diffuse-trans)
    # off from the DSL ctor params. The lobes are MATERIAL-LEVEL UBO uniforms that
    # the stock forward-PBR lighting already consumes — ptex3d's generated shader
    # routes through the same _forward_lightingX, so they need no per-pixel
    # SurfaceOut field and no shader change; they pass straight to the GenData
    # (auto-enabling each has_<lobe>). Set them per-instance, like a PbrMaterial:
    #     A.Ptex3d("glass", dsl_class=Marble, transmission_factor=0.6, ior=1.45)
    # (Per-pixel TEXTURING of the lobes isn't wired — these are constants/uniforms.)
    from ork.hypergraph.ptex3d import materialize_ptex3d_full
    from ork.hypergraph.ptex3d.dsl import LOBE_ALIASES
    # FWD_SSBO_CUSTOM: the terrain GPU-chunk renderer (or any SSBO-sourced consumer) passes a
    # vertex_source= that delegates the SSBO-pull vertex side; it is NOT a DSL/lobe param.
    vertex_source = kwargs.pop("vertex_source", None)
    # D.4: sampler→texture bindings ({sampler uniform: image path, alias-form OK}) — reflected
    # on the GenData; the materializer loads + binds. For baked terrain channels the
    # deterministic path is "<assetcache>/terrain/<hf_asset>/<channel>.exr". NOT a DSL param.
    sampler_textures = kwargs.pop("sampler_textures", None)
    # normalize short lobe aliases (transmission -> transmission_factor, ...) before
    # the split; the dict (nested) form is left for _flatten_nested.
    for short, canon in LOBE_ALIASES.items():
      if short in kwargs and not isinstance(kwargs[short], dict):
        kwargs[canon] = kwargs.pop(short)
    lobe_keys   = set(PbrMaterial._LOBE_KWARGS) | PbrMaterial._NESTED_KEYS
    lobe_kwargs = {k: kwargs.pop(k) for k in list(kwargs) if k in lobe_keys}
    path, pspecs, dsl_lobes, capture_targets = materialize_ptex3d_full(dsl_class, vertex_source=vertex_source, **kwargs)  # kwargs now = DSL params
    self.capture_targets = list(capture_targets)   # explicit-capture target names, in codegen/MRT order
    # merge: class-declared lobes (surface(**lobes)) are defaults; per-instance
    # Ptex3d(...) kwargs override. Then reuse PbrMaterial nested-form + hsv coercion.
    combined = {**dsl_lobes, **lobe_kwargs}
    combined = PbrMaterial._flatten_nested(combined)
    combined, _ = PbrMaterial._coerce_hsv(combined, None)
    extras = {}
    for k in PbrMaterial._LOBE_KWARGS:
      if k in combined:
        extras[k] = combined.pop(k)
    if combined:
      raise TypeError(f"Ptex3d: unexpected kwargs {sorted(combined)}")
    self.gendata = PbrMaterialGenData(
        shaderpath    = path,
        shader_params = _ptex_param_defaults(pspecs),
        metallic      = 1.0,
        roughness     = 1.0,
        **extras)
    if sampler_textures:
      self.gendata.sampler_textures = {str(k): str(v) for k, v in sampler_textures.items()}
    self._ctx = None

  @classmethod
  def from_gendata(cls, gendata, ctx=None):
    inst = cls(gendata=gendata)
    inst._ctx = ctx
    return inst

  @property
  def as_gfx_material(self):
    built = getattr(self, "built", None)
    if built is not None:
      return built
    self.built = self.build()
    return self.built

  def build(self):
    # Delegate to PbrMaterial.build(), which honors gendata.shaderpath +
    # shader_params (set above). Ptex3d is author-side sugar; on JSON round-trip
    # the PbrMaterialGenData deserializes straight to PbrMaterial.
    return PbrMaterial.from_gendata(self.gendata,
                                    ctx=getattr(self, "_ctx", None)).build()


###############################################################################
# FreestyleMaterial — wraps FreestyleMaterialGenData. Pipeline param
# bindings are deferred (the dataflow shader work will own them); for
# now build() returns a live FreestyleMaterial with raster state set
# from the reflected tokens, callers wire up their own pipeline.
###############################################################################

@_register
class FreestyleMaterial:

  def __init__(self, *,
               shader_text="", shader_file="orkshader://manip",
               shader_name="x", technique="std_mono_fwd",
               rendermodel="ForwardPBR",
               blending="OFF", culltest="PASS_FRONT", depthtest="LEQUALS",
               gendata=None):
    if gendata is not None:
      self.gendata = gendata
      return
    self.gendata = FreestyleMaterialGenData(
        shader_text=shader_text, shader_file=shader_file,
        shader_name=shader_name, technique=technique,
        rendermodel=rendermodel,
        blending=blending, culltest=culltest, depthtest=depthtest)
    self._ctx = None

  @classmethod
  def from_gendata(cls, gendata, ctx=None):
    inst = cls(gendata=gendata)
    inst._ctx = ctx
    return inst

  def build(self):
    d   = self.gendata
    ctx = getattr(self, "_ctx", None) or _lev2.GfxEnv.ref.loadingContext()
    mat = _lev2.FreestyleMaterial()
    if d.shader_text:
      mat.gpuInitFromShaderText(ctx, d.shader_name, d.shader_text)
    else:
      mat.gpuInit(ctx, d.shader_file)
    mat.rasterstate.setBlendingMacro(getattr(_tokens, d.blending))
    mat.rasterstate.culltest  = getattr(_tokens, d.culltest)
    mat.rasterstate.depthtest = getattr(_tokens, d.depthtest)
    return mat


###############################################################################
# ParticleSystem — round-trippable wrapper for a Python particle DSL.
#
# Construction (Scene-side):
#   ptc = self.asset.ParticleSystem("col_vdb_ptc",
#             dsl_file="col_vdb",
#             collision_sdf=saddle_sdf,    # asset-wrapper cross-ref
#             base_radius=1.0,             # scalar param (varmap)
#             pool_size=10000)
#
# Build path:
#   - Resolves the DSL file via ork.dflow.particles.resolve.resolve_dsl_file.
#   - Loads the DSL class (auto-detect, or dsl_class= for multi-class files).
#   - Calls the class ctor with **scalar_params merged with the resolved
#     cross-asset refs (an asset wrapper input becomes the underlying
#     built artifact, e.g. a FloatGrid).
#   - Calls .generatedflow() to produce the dataflow GraphData.
#   - Returns a ParticlesDrawableData carrying that graph.
#
# Round-trip: scalar kwargs land in ParticleSystemGenData._params
# (varmap). Cross-asset wrapper kwargs are recorded in
# ParticleSystemGenData._asset_kwargs as {ctor_kwarg: asset_name}; the
# materializer resolves them against the in-progress artifact dict.
###############################################################################

@_register
class ParticleSystem:
  """Asset-DSL wrapper for a Python particle DSL class.

  The DSL class must inherit from ork.hypergraph.dflow.particles.ParticleSystem and
  expose .generatedflow() returning a dataflow.GraphData. Constructor
  kwargs split into:
    * cross-asset references (values are asset wrappers with .gendata.asset_name);
      these get round-tripped via ParticleSystemGenData._asset_kwargs.
    * scalar params (anything else); routed through varmap.
  """

  def __init__(self, *,
               dsl_file=None,
               dsl_class=None,
               gendata=None,
               probe=None,
               emitter_intensity=1.0,
               emitter_radius=1.0,
               **kwargs):
    # item E — the per-system emission point light (color/position track the
    # live particles; these scale brightness + falloff). Gendata fields, NOT
    # DSL ctor kwargs — both materializers apply them to the drawable data.
    self._emitter_intensity = float(emitter_intensity)
    self._emitter_radius    = float(emitter_radius)
    if gendata is not None:
      self.gendata = gendata
      # Live-side kwargs (resolved cross-asset artifacts + scalars) are
      # supplied by from_gendata via _live_kwargs; ctor-only path leaves
      # them empty and build() will pull from gendata.
      self._live_kwargs = {}
      return

    if not dsl_file:
      raise ValueError("ParticleSystem requires dsl_file (DSL .py name or path)")

    # PBR2 Phase 0 — per-particle-system HDRI probe binding. Accepts:
    #   - HdriToXir wrapper (.gendata.asset_name) — primary case;
    #     wire_scene_data resolves to <assetcache>/xirtemp/<asset>.xir
    #     and stuffs into _environmentMapPath on the particle drawable.
    #   - _EntityHandle (.name) — reflection-probe entity (future);
    #     same XIR path convention but baked from scene rendering
    #     instead of a static HDR file.
    #   - bare string — entity/asset name directly.
    probe_entity_name = ""
    if probe is not None:
      if isinstance(probe, str):
        probe_entity_name = probe
      else:
        gd = getattr(probe, "gendata", None)
        asset_nm = getattr(gd, "asset_name", "") if gd is not None else ""
        if asset_nm:
          probe_entity_name = asset_nm
        else:
          probe_entity_name = getattr(probe, "name", None) or ""
        if not probe_entity_name:
          raise ValueError(
            "ParticleSystem(probe=...) expects an HdriToXir wrapper, an "
            "entity handle from self.probe(...), or a bare name string; "
            f"got {type(probe).__name__}")

    # Split kwargs: anything with a .gendata.asset_name is a cross-asset
    # ref; everything else is a scalar param.
    asset_kwargs   = {}
    scalar_kwargs  = {}
    live_kwargs    = {}
    for k, v in kwargs.items():
      gd = getattr(v, "gendata", None)
      asset_name = getattr(gd, "asset_name", "") if gd is not None else ""
      if asset_name:
        asset_kwargs[k] = asset_name
        live_kwargs[k]  = _materialize(v)   # build-time live artifact
      else:
        scalar_kwargs[k] = v
        live_kwargs[k]   = v
    self.gendata = ParticleSystemGenData(
        dsl_file     = dsl_file,
        dsl_class    = dsl_class or "",
        params       = scalar_kwargs,
        asset_kwargs = asset_kwargs)
    self.gendata.emitter_intensity = self._emitter_intensity
    self.gendata.emitter_radius    = self._emitter_radius
    if probe_entity_name:
      self.gendata.probe_entity_name = probe_entity_name
    self._live_kwargs = live_kwargs
    # D.2 MODEL B: run the DSL ONCE here at authoring and EMBED the graph in the gendata —
    # the serialized scene then reloads with no Python re-run (dsl_file stays as provenance).
    # Cross-asset systems (asset_kwargs) embed too: the LIVE artifacts they inject (e.g. an
    # openvdb FloatGrid into VdbCollider) don't serialize, but their ASSET NAMES do — the
    # registration window below lets op builders stamp each consuming module's reflected
    # asset-reference field (sdf_asset), and build() re-resolves the live artifact from the
    # registry post-deserialize (_resolve_asset_refs).
    from ork.hypergraph.dflow.particles.resolve import resolve_dsl_file, load_dsl_class
    from ork.hypergraph.dflow.particles import _artifact_names
    _cls = load_dsl_class(resolve_dsl_file(dsl_file), dsl_class or None)
    _artifact_names.clear()
    for _kw, _nm in asset_kwargs.items():
      _artifact_names.register(live_kwargs.get(_kw), _nm)
    try:
      self.gendata.graph = _cls(**live_kwargs).generatedflow()
    finally:
      _artifact_names.clear()

  @classmethod
  def from_gendata(cls, gendata, artifacts=None):
    """Rehydrate from a deserialized gendata. `artifacts` is the
    materialize_from_scenedata in-progress dict so cross-asset references
    can be resolved at build time."""
    inst = cls(gendata=gendata)
    live = {}
    if gendata.params is not None:
      for key in gendata.params.keys():
        live[key] = gendata.params[key]
    for kw, asset_name in gendata.asset_kwargs.items():
      if artifacts is None or asset_name not in artifacts:
        raise KeyError(
          f"ParticleSystem {gendata.asset_name!r} references unknown "
          f"asset {asset_name!r} for kwarg {kw!r}")
      live[kw] = artifacts[asset_name]
    inst._live_kwargs = live
    return inst

  def build(self):
    """Return a ParticlesDrawableData. MODEL B: the embedded graph (gendata.graph) is used
    directly — NO Python DSL re-run at load; cross-asset references (sdf_asset names) are
    re-resolved against the artifact registry. Legacy model-A gendatas (no embedded graph)
    fall back to re-running the DSL from dsl_file (kept until every scene re-serializes)."""
    d  = self.gendata
    dd = ParticlesDrawableData()
    dd.emitterIntensity = d.emitter_intensity   # existing camelCase pyext props
    dd.emitterRadius    = d.emitter_radius
    g  = d.graph
    if g is not None:
      dd.graphdata = g
      self._resolve_asset_refs(g)
      return dd
    from ork.hypergraph.dflow.particles.resolve import resolve_dsl_file, load_dsl_class
    dsl_path = resolve_dsl_file(d.dsl_file)
    cls      = load_dsl_class(dsl_path, d.dsl_class or None)
    ptc      = cls(**self._live_kwargs)
    dd.graphdata = ptc.generatedflow()
    return dd

  def _resolve_asset_refs(self, g):
    """Post-deserialize wire: fill LIVE artifacts onto modules that reference them BY
    NAME (the sdf_asset stamp). The artifacts come through _live_kwargs (from_gendata
    resolved gendata.asset_kwargs against the registry — declaration order guarantees
    they materialized first). In-process authoring is a harmless no-op re-set (the
    modules already hold their live artifacts)."""
    from orkengine.lev2 import particles as _ptc
    for kw, asset_name in (self.gendata.asset_kwargs or {}).items():
      art = self._live_kwargs.get(kw)
      if art is None:
        continue
      try:
        n = _ptc.resolve_sdf_assets(g, asset_name, art)
      except TypeError:
        continue   # not a FloatGrid-typed artifact — no collider consumes it
      if n:
        print(f"ParticleSystem {self.gendata.asset_name!r}: resolved sdf asset "
              f"{asset_name!r} -> {n} module(s)", flush=True)


###############################################################################
# Hypermesh — GPU mesh-graph asset (D.3). EMBEDS the serialized hypermesh graph
# in HypermeshGenData (model B): the DSL runs ONCE here at authoring; the graph
# round-trips in the scene JSON; reload materializes with NO Python.
#
#   mesh = self.asset.Hypermesh("racer_hull",
#              dsl_class = RacerHull,        # hypermesh DSL class (authoring only)
#              vtx_budget = 1 << 20,
#              **dsl_kwargs)
#
# build() LIVE-materializes the embedded graph (needs ctx) and returns the
# LiveHypermesh. drawable_data(material=...) builds the round-trippable
# HypermeshDrawableData for an ECS HypermeshComponent — material referenced
# BY NAME (resolved from the AssetSystem artifact registry at load).
###############################################################################

class _ImposterTier:
  """Marker for an impostor LOD tier in a drawable_data lods={dist: ...} chain (see Hypermesh.imposter()).
  `_mesh` is the fallback/placeholder Hypermesh (drawn if the bake is unavailable); grid×grid = atlas views;
  ssaa = bake supersample factor (render tile*ssaa per view), msaa = bake multisample count."""
  def __init__(self, mesh, grid, tile, ssaa, msaa):
    self._mesh = mesh
    self._grid = grid
    self._tile = tile
    self._ssaa = ssaa
    self._msaa = msaa


@_register
class Hypermesh:
  """Asset-DSL wrapper for a hypermesh DSL class (embedded-graph model B)."""

  def __init__(self, *, dsl_class=None, gendata=None, ctx=None, vtx_budget=1 << 20, **kwargs):
    self._ctx = ctx
    if gendata is not None:
      self.gendata = gendata
      self._animated = False  # unknown on rehydrate; drawable_data callers pass it explicitly
      return
    if dsl_class is None:
      raise TypeError("Hypermesh requires dsl_class= (a dflow.hypermesh.Hypermesh subclass)")
    inst = dsl_class(**kwargs)               # AUTHORING: run the DSL ONCE -> graph -> embed
    self.gendata = HypermeshGenData(
        graph      = inst.generatedflow(),
        dsl_file   = getattr(dsl_class, "__module__", "") or "",
        vtx_budget = vtx_budget)
    self._animated = bool(inst.is_animated)
    # remembered so fork()/imposter() can re-evaluate the same DSL at overridden params (LOD chain).
    self._dsl_class  = dsl_class
    self._dsl_kwargs = dict(kwargs)
    self._vtx_budget = vtx_budget

  def fork(self, **overrides):
    """A derived LOD mesh: re-run THIS hypermesh's DSL with `overrides` merged over the original
    kwargs (e.g. fork(depth=3) for a reduced tree). Returns an UNREGISTERED Hypermesh wrapper meant
    to live in a drawable_data lods={dist: mesh} chain — chainable: base.fork(...).fork(...)."""
    if not hasattr(self, "_dsl_class"):
      raise RuntimeError("fork() needs an authoring Hypermesh (built from dsl_class), not a rehydrated one.")
    return Hypermesh(dsl_class=self._dsl_class, ctx=self._ctx,
                     vtx_budget=self._vtx_budget, **{**self._dsl_kwargs, **overrides})

  def imposter(self, grid=8, tile=256, ssaa=1, msaa=2):
    """The FAR-tier impostor LOD (hemi-octahedral billboard). Returns a marker the drawable_data lods chain
    holds at its distance, e.g. lods={..., 600.0: boulders.fork(subdivisions=0).imposter()}. The TIER routes
    that distance band's instances (via the GPU cull) to ONE camera-facing quad each, sampling a PBR atlas
    baked once at materialize from the base mesh — so the base material MUST carry the capture technique
    (Ptex3d(impostor=True)). THIS hypermesh (the one .imposter() is called on) is the mesh that draws if the
    bake is unavailable, so call it on a CHEAP mesh (a coarse fork). grid×grid = the atlas view count."""
    return _ImposterTier(self, grid, tile, ssaa, msaa)

  @classmethod
  def from_gendata(cls, gendata, ctx=None, artifacts=None):
    return cls(gendata=gendata, ctx=ctx)

  def build(self):
    ctx = getattr(self, "_ctx", None)
    if ctx is None:
      # AUTHORING/wire path (scene declaration, python ecsplay wire): the live GPU
      # mesh is NOT needed here — the HypermeshComponent materializes from the
      # AssetSystem registry at load (C++), and an eager materializeLive on the
      # loading context would race the loader thread's frame pump. Tier-1/2
      # standalone callers pass ctx= explicitly and get the live mesh.
      return None
    return self.gendata.materialize(ctx)     # -> LiveHypermesh (pure C++)

  def drawable_data(self, *, material=None, materials=None, animated=None, lods=None, **viz):
    """Round-trippable HypermeshDrawableData for an ECS HypermeshComponent.
    `material` is a Ptex3d/PbrMaterial wrapper (referenced BY asset name) or a bare
    name string. `materials` (E.3) = {gid: wrapper-or-name} — faces whose __tags gid
    (set via m.assign_gid) matches draw with that material in their own indirect-draw
    bucket; unbound gids render with the default `material`. viz kwargs: face_viz /
    tag_viz / wireframe / instance_matrices / cull (E.4: per-view GPU frustum cull
    for instanced graphs; bound auto-computed) / cull_bound (vec4 override)."""
    def _name_of(m):
      return m if isinstance(m, str) else \
             getattr(getattr(m, "gendata", None), "asset_name", "") if m is not None else ""
    mtl_name = _name_of(material)
    kwargs = {}
    if materials:
      kwargs["gid_materials"] = {int(g): _name_of(m) for g, m in materials.items()}
    # Phase 3c — DISTANCE LOD: lods={dist: hypermesh} (or {dist: (hypermesh, material)} to draw that tier
    # with its OWN material — the far-LOD case). tier 0 (this asset, the base) draws near; each dist>0
    # entry is a coarser mesh drawn beyond that many meters. Parallel arrays, ascending distance.
    if lods:
      extra = sorted(((float(d), m) for d, m in lods.items() if float(d) > 0.0), key=lambda kv: kv[0])
      if extra:
        graphs, dists, lodmats, impostors = [], [], {}, []
        imp_grid, imp_tile, imp_ssaa, imp_msaa = 8, 512, 2, 4
        for idx, (d, entry) in enumerate(extra):
          # an impostor tier carries no mesh of its own — its _mesh is the bake-unavailable fallback (a
          # cheap mesh); the C++ cull routes the band to billboards (impostor_lods names this tier index).
          # grid×grid = atlas view count, tile = per-view pixels (atlas = grid*tile square) — the bake reads
          # these from imposter(grid=, tile=), NOT hardcoded.
          if isinstance(entry, _ImposterTier):
            mesh, mat = entry._mesh, None
            impostors.append(idx)
            imp_grid, imp_tile = entry._grid, entry._tile
            imp_ssaa, imp_msaa = entry._ssaa, entry._msaa
          elif isinstance(entry, tuple):
            mesh, mat = entry[0], entry[1]
          else:
            mesh, mat = entry, None
          graphs.append(mesh.gendata.graph)
          dists.append(d)
          if mat is not None:
            lodmats[idx] = _name_of(mat)
        kwargs["lod_graphs"]    = graphs
        kwargs["lod_distances"] = dists
        if lodmats:
          kwargs["lod_materials"] = lodmats
        if impostors:
          kwargs["impostor_lods"] = impostors
          kwargs["impostor_grid"] = int(imp_grid)
          kwargs["impostor_tile"] = int(imp_tile)
          kwargs["impostor_ssaa"] = int(imp_ssaa)
          kwargs["impostor_msaa"] = int(imp_msaa)
    return HypermeshDrawableData(
        graph          = self.gendata.graph,
        material_asset = mtl_name,
        animated       = self._animated if animated is None else bool(animated),
        vtx_budget     = self.gendata.vtx_budget,
        **kwargs,
        **viz)


###############################################################################
# HeightField — terrain heightfield asset. UNLIKE ParticleSystem (dsl_file +
# re-run Python at load), this EMBEDS the serialized terrain graph in
# HeightFieldGenData (model B): the DSL runs ONCE here at authoring; the graph
# round-trips in the scene JSON; reload bakes it with NO Python / NO DSL file.
#
#   hf = self.asset.HeightField("rolling_hills",
#            dsl_file  = "rolling_hills",   # terrain DSL .py (authoring only)
#            dimension = 1024,              # this heightfield's grid res = the bake/compute res
#            octaves   = 6)                  # scalar kwargs -> DSL ctor
#
# build() bakes the embedded graph (cook-cache-backed) and returns a dict:
#   { "<channel>": "<assetcache>/terrain/<asset>/<channel>.exr", ...,
#     "stats": { "<channel>": FieldStats } }
#
# NOTE: `dimension` here is THIS heightfield's grid resolution — the bake/COMPUTE res (the EXRs are
# dimension x dimension). It has no render counterpart at this layer. When wired through
# self.terrain(), it is driven from terrain()'s `bake_dimension`; terrain()'s separate
# `render_dimension` is the downsampled render-mesh grid (NOT this).
###############################################################################

@_register
class HeightField:
  """Asset-DSL wrapper for a terrain HeightField DSL class (embedded-graph model)."""

  def __init__(self, *, dsl_file=None, dsl_class=None, dimension=512,
               extent_m=4096.0, gendata=None, ctx=None,
               material=None, channel_samplers=None, **kwargs):
    # E.6/2.20 — the terrain↔material contract rides the TERRAIN asset: `material`
    # names the shading material asset; `channel_samplers` maps baked channel ->
    # sampler uniform ({"flow": "FlowMap"}). The C++ materializeAll post-pass
    # derives each channel's deterministic <assetcache> path and binds it onto
    # the resolved material — no hand-synced path strings on the material side.
    self._ctx = ctx
    if gendata is not None:
      self.gendata = gendata
      return
    if not dsl_file:
      raise ValueError("HeightField requires dsl_file (terrain DSL .py name or path)")
    # AUTHORING: resolve + run the DSL ONCE -> graph -> embed in the gendata.
    # extent_m makes the graph resolution-independent: spatial op params are in
    # meters, converted to texels per-bake (heights are TRUE METERS on the plugs).
    from ork.hypergraph.dflow.terrain.resolve import resolve_dsl_file, load_dsl_class
    dsl_path = resolve_dsl_file(dsl_file)
    cls      = load_dsl_class(dsl_path, dsl_class or None)
    inst     = cls(**kwargs)                 # scalar kwargs -> DSL ctor (parameterized terrain)
    graph    = inst.generatedflow()
    # keep the live DSL instance (legacy scatter fallback for pre-D.4 flows).
    self._dsl_inst = inst
    self.gendata = HeightFieldGenData(dimension=dimension, extent_m=extent_m,
                                      graph=graph)
    if material:
      self.gendata.material_asset = str(material)
    if channel_samplers:
      self.gendata.channel_samplers = {str(k): str(v) for k, v in channel_samplers.items()}
    # D.4: REFLECT the scatter sinks — the placement contract + type bindings ride the
    # gendata (the weight fields already ride the embedded graph as captured channels),
    # so a deserialized scene re-places scatters with NO Python DSL state.
    sinks = []
    for sname, spec in getattr(inst, "_scatters", {}).items():
      sinks.append(ScatterSinkData(
          name=sname,
          density=(0.0 if spec.density is None else float(spec.density)),
          count=(0 if spec.count is None else int(spec.count)),
          seed=spec.seed, align=spec.align,
          yaw_lo=spec.yaw[0], yaw_hi=spec.yaw[1],
          scale_lo=spec.scale[0], scale_hi=spec.scale[1],
          cutoff=spec.cutoff, jitter=spec.jitter,
          max_points=spec.max_points, lift=spec.lift,
          type_names=[t for (t, _c) in spec.types],
          type_channels=[c for (_t, c) in spec.types],
          type_assets=dict(spec.assets), type_materials=dict(spec.materials),
          type_colliders=dict(getattr(spec, 'colliders', {}) or {})))
    if sinks:
      self.gendata.scatters = sinks

  @classmethod
  def from_gendata(cls, gendata, ctx=None):
    """Rehydrate from a deserialized gendata — the graph is embedded, so NO DSL."""
    return cls(gendata=gendata, ctx=ctx)

  def build(self, ext="exr"):
    """Bake the embedded graph (cook-cache-backed) -> dict of channel image paths +
    per-channel FieldStats. Channels come from the graph's CaptureModules.
    `ext` picks the output format by the capture file extension: "exr" (RGBA32F
    float, the canonical lossless artifact) or "png" (single-channel 16-bit)."""
    import os as _os
    from orkengine.core import Path as _Path
    ext = ext.lower().lstrip(".")
    if ext not in ("exr", "png"):
      raise ValueError(f"HeightField.build: ext must be 'exr' or 'png', got {ext!r}")
    d     = self.gendata
    graph = d.graph
    if graph is None:
      raise RuntimeError(f"HeightField {d.asset_name!r}: gendata carries no embedded graph")
    if self._ctx is None:
      # AUTHORING/wire path (scene declaration): defer the bake to LOAD — the C++
      # host's materializeAll bakes (cook-cached) with a real ctx; an eager bake on
      # the loading context at tojson time is wasted work and races the loader pump.
      # Tier-1/2 standalone callers pass ctx= explicitly and get the full bake dict.
      print(f"HeightField {d.asset_name!r}: no ctx bound — bake deferred to load", flush=True)
      return None
    # D.1: paths + bake + manifest are C++ (HeightFieldGenData::materialize — a 1:1 port of the
    # body below). The result dict rebuilds from the manifest (the self-describing contract);
    # the AUTHORING-only scatter sinks stay here. ORK_HM_PYMAT=1 = the retained Python reference.
    if not _os.environ.get("ORK_HM_PYMAT"):
      manifest_path = d.materialize(self._ctx, ext)
      from ork.hypergraph.dflow.terrain.manifest import TerrainManifest
      from types import SimpleNamespace
      man    = TerrainManifest.load(manifest_path)
      result = {ch: man.channel_path(ch) for ch in man.channels}
      result["stats"] = {ch: SimpleNamespace(min=ci.min, max=ci.max, mean=ci.mean)
                         for ch, ci in man.channels.items()}
      result["manifest"] = manifest_path
      self._run_scatters(result, d)
      return result
    outdir = _Path.expandPathString(f"<assetcache>/terrain/{d.asset_name or 'unnamed'}")
    _os.makedirs(outdir, exist_ok=True)
    caps     = _lev2.terrain.capture_modules(graph)
    channels = []  # flat list of every emitted channel, in capture/declaration order
    for cap in caps:
      ch_list = [c.strip() for c in (cap.channel or "height").split(",") if c.strip()]
      if not ch_list:
        ch_list = ["height"]
      if len(ch_list) == 1:
        # single channel -> a CONCRETE path. Works with any bake build (no reliance on
        # the C++ {channel}-template substitution, which only exists in the multichannel
        # bake). This is the common case (e.g. just "height").
        cap.path = _os.path.join(outdir, f"{ch_list[0]}.{ext}")
      else:
        # multi-channel ("height,normal") -> ONE {channel} TEMPLATE; the bake emits one
        # image per channel by substitution. REQUIRES the multichannel bake build.
        cap.path = _os.path.join(outdir, "{channel}." + ext)
      channels.extend(ch_list)
    stats  = _lev2.terrain.bake_heightfield(graph, self._ctx, d.dimension,
                                            extent_m=d.extent_m)
    result = {ch: _os.path.join(outdir, f"{ch}.{ext}") for ch in channels}
    # stats return in FLUSH (topo) order; `channels` is name-sorted module order — key by the
    # self-describing .channel (index-zipping shuffles stats across channels)
    _by_ch = {st.channel: st for st in stats}
    result["stats"] = {ch: _by_ch.get(ch) for ch in channels}
    # persist the SCALE CONTRACT next to the channel images: the authoritative,
    # self-describing physical scale + per-channel stats that every consumer
    # (viewer, CPU mask sampler, physics, segmentation) reads instead of hardcoding.
    # v2 NATURAL UNITS: stored heights ARE meters (exaggeration lives in the graph).
    from ork.hypergraph.dflow.terrain.manifest import TerrainManifest
    _SEMANTIC = {"height": "height_meters", "normal": "normal_world"}
    ch_meta = {ch: {"file": result[ch], "semantic": _SEMANTIC.get(ch, ch),
                    "min": st.min, "max": st.max, "mean": st.mean}
               for ch, st in result["stats"].items()}
    manifest_path = _os.path.join(outdir, f"{d.asset_name or 'unnamed'}.terrain.json")
    TerrainManifest.write(manifest_path,
        extent_m=d.extent_m, dim=d.dimension,
        channels=ch_meta, format=ext, provenance={"asset_name": d.asset_name or ""})
    result["manifest"] = manifest_path
    # POST-BAKE: scatter sinks (mask-driven placement). MESH-AGNOSTIC — one ScatterSet .ogeo
    # per sink (per-instance matrices + attribute table); a SEPARATE consumer binds meshes.
    # Only the live/authoring path carries the specs (from_gendata scenes reference the .ogeo).
    self._run_scatters(result, d)
    return result

  def _run_scatters(self, result, d):
    """POST-BAKE scatter sinks (mask-driven placement). E.2: placement is the C++ PLACER
    (lev2.terrain.scatter_place_ogeo — ONE implementation; the C++ materializer runs the
    same placer at zero-Python loads, so re-placing here is idempotent on the identical
    artifact). The numpy scatter.place survives only as the parity-gate reference. Specs:
    the REFLECTED gendata.scatters (D.4); legacy fallback = the live DSL ._scatters."""
    import os as _os
    sinks = list(d.scatters or [])
    if not sinks:
      inst  = getattr(self, "_dsl_inst", None)
      specs = getattr(inst, "_scatters", {}) if inst is not None else {}
      for sname, spec in specs.items():
        sinks.append(ScatterSinkData(
            name=sname,
            density=(0.0 if spec.density is None else float(spec.density)),
            count=(0 if spec.count is None else int(spec.count)),
            seed=spec.seed, align=spec.align,
            yaw_lo=spec.yaw[0], yaw_hi=spec.yaw[1],
            scale_lo=spec.scale[0], scale_hi=spec.scale[1],
            cutoff=spec.cutoff, jitter=spec.jitter,
            max_points=spec.max_points, lift=spec.lift,
            type_names=[t for (t, _c) in spec.types],
            type_channels=[c for (_t, c) in spec.types],
            type_assets=dict(spec.assets), type_materials=dict(spec.materials),
          type_colliders=dict(getattr(spec, 'colliders', {}) or {})))
    if not sinks:
      return
    outdir = _os.path.dirname(result["manifest"])
    result["scatters"] = {}
    for sink in sinks:
      chans = {"height": result["height"]}
      for ch in sink.type_channels:
        chans[ch] = result[ch]
      opath = _os.path.join(outdir, f"{sink.name}.ogeo")
      n     = _lev2.terrain.scatter_place_ogeo(sink, chans, d.extent_m, opath)
      result["scatters"][sink.name] = {"path": opath, "count": n,
                                       "types": list(sink.type_names),
                                       "assets": dict(sink.type_assets),
                                       "materials": dict(sink.type_materials)}
      print(f"terrain scatter: {sink.name!r} -> {n} points -> {opath}", flush=True)


###############################################################################
# HdriToXir — wraps HdriToXirGenData. Bakes an HDR/PNG/EXR source through
# the sync envmap GPU pipeline (ork.envmap.process_envmap, same path
# ork.hdri.genxir.py main_sync uses) and writes the result to a deterministic
# .xir under <assetcache>/envmaps2/. .build() returns the resolved .xir
# path as a string — downstream consumers (SceneGraph SkyboxTexPathStr,
# per-drawable env override) expect a path.
#
# No cache for now: every build() re-bakes. A caching layer can wrap this
# later once we settle on the cache-key strategy.
###############################################################################

@_register
class HdriToXir:

  def __init__(self, *,
               source=None, levels=6, samples=128, scale=1.0, clamp=16.0,
               gendata=None):
    if gendata is not None:
      self.gendata = gendata
      self._ezapp = None
      self._ctx = None
      return
    if not source:
      raise ValueError("HdriToXir requires source= (path to HDR/EXR/PNG)")
    # Expand ~ and $ENV at construction so the reflected source_path is
    # absolute on disk regardless of where the Scene class was authored.
    import os as _os
    expanded = _os.path.expandvars(_os.path.expanduser(str(source)))
    self.gendata = HdriToXirGenData(
        source_path = expanded,
        levels      = int(levels),
        samples     = int(samples),
        scale       = float(scale),
        clamp       = float(clamp))
    self._ezapp = None
    self._ctx = None

  @classmethod
  def from_gendata(cls, gendata, ctx=None, ezapp=None):
    inst = cls(gendata=gendata)
    inst._ctx = ctx
    inst._ezapp = ezapp
    return inst

  def output_path(self):
    """Deterministic .xir path for this gen's baked artifact.

    Lands under <assetcache>/xirtemp/ — kept separate from
    <ork_envmaps2>/ which is reserved for engine-shipped pre-baked
    envmap XIRs. xirtemp is the per-scene scratch cache for probe-
    authored bakes.

    Cheap — no I/O beyond mkdir. Safe to call from the load path to
    resolve the path without triggering a bake."""
    import os
    from orkengine.core import Path as _Path
    cache_root = _Path.expandPathString("<assetcache>/xirtemp")
    os.makedirs(cache_root, exist_ok=True)
    name = self.gendata.asset_name or "hdri_probe"
    return os.path.join(cache_root, f"{name}.xir")

  def build(self):
    """Run the bake. Returns the resolved .xir path string.

    Requires an ezapp + ctx (the Python envmap pipeline drives GPU work
    inline on the main thread). For Scene-mediated builds, pass them in
    via from_gendata; for ad-hoc Tier 1/2 use, the caller is responsible
    for having lev2appinit+mainThreadBegin+bindGfxToCurrentThread already
    done before calling build().
    """
    from ork.envmap import process_envmap
    d = self.gendata
    out_path = self.output_path()
    # No file-exists short-circuit: tojson re-runs should re-bake.
    # The load-time path (materialize_from_scenedata) explicitly avoids
    # calling build() and resolves to the cached .xir via output_path()
    # so the load never triggers a bake.
    # ezapp + ctx fallback chain:
    #   1. Explicit args from from_gendata(ezapp=, ctx=) win.
    #   2. Otherwise reach for OrkEzApp.current() — set by lev2appinit and
    #      cleared in ~OrkEzApp; lets Scene.__init__'s eager
    #      self.asset.HdriToXir(...) build inline during ork.scene.tojson.py
    #      without threading ezapp through the asset namespace.
    #   3. ctx falls back to GfxEnv.ref.loadingContext() (= TLS lookup,
    #      valid on the Python thread once bindGfxToCurrentThread fired).
    ezapp = self._ezapp
    if ezapp is None:
      ezapp = _lev2.OrkEzApp.current()
    ctx   = self._ctx
    if ctx is None:
      ctx = _lev2.GfxEnv.ref.loadingContext()
    if not ctx:
      raise RuntimeError(
        "HdriToXir.build: no gfx context — ezapp.bindGfxToCurrentThread() "
        "must be called before HdriToXir.build() (headless harness setup)")
    if ezapp is None:
      raise RuntimeError(
        "HdriToXir.build: no ezapp — lev2.lev2appinit(use_subsystems=...) "
        "must be called before HdriToXir.build()")
    ok = process_envmap(d.source_path, out_path, ctx, ezapp,
                        verbose=True,
                        num_roughness_levels=d.levels,
                        specular_samples=d.samples,
                        scale=d.scale,
                        clamp=d.clamp)
    if not ok:
      raise RuntimeError(f"HdriToXir.build: process_envmap failed for {d.source_path!r}")
    return out_path


###############################################################################
# materialize_from_scenedata — M2b.5 round-trip materialization.
#
# Walks an ecs.SceneData's AssetSystemData, instantiates a Python
# wrapper for each reflected gen by gendata className, resolves any
# cross-asset references by name from the in-progress artifact dict,
# and calls .build() in declaration order. Returns {name → built}.
#
# Materials are NOT round-tripped through reflection yet (deferred).
# The caller supplies material_resolver(gen) → material for gens that
# need one (VdbGridToDrawableGenData); returning None disables those
# gens (they're skipped instead of failing the whole load).
###############################################################################

# Reflected gendata className → Python wrapper class. Wrappers must
# expose a from_gendata classmethod that accepts the gendata plus any
# resolved dependencies as kwargs.
_GENDATA_TO_WRAPPER = {
  "ImplicitSdfGenData":       ImplicitSdf,
  "PbrMaterialGenData":       PbrMaterial,
  "FreestyleMaterialGenData": FreestyleMaterial,
  "VdbGridToDrawableGenData": VdbGridToDrawable,
  "ParticleSystemGenData":    ParticleSystem,
  "HeightFieldGenData":       HeightField,
  "HypermeshGenData":         Hypermesh,
  "HdriToXirGenData":         HdriToXir,
  "VdbFileSdfGenData":        VdbFileSdf,
  "MeshSdfGenData":           MeshSdf,
}


def materialize_from_scenedata(scene_data, ctx=None, ezapp=None, material_resolver=None):
  """Materialize all reflected AssetGenData entries on `scene_data`.

  ctx: GPU Context for material gens (PBR/Freestyle need one to gpuInit).
       Defaults to GfxEnv.ref.loadingContext() when None. Pass None
       explicitly + omit material asset gens for a headless data-only
       materialize.
  ezapp: OrkEzApp instance — required for HdriToXirGenData materialization
       (the envmap bake pipeline drives GPU work inline through ezapp).
       Other gen kinds ignore it.
  material_resolver(gen) → material — optional fallback for drawable
       gens whose material_asset_name is empty (eager-build scenes that
       don't route their material through the asset graph).

  Returns {asset_name: built_artifact}. Order is the AssetSystemData's
  declaration order, so cross-asset references (a VdbGridToDrawable's
  grid_asset_name → its ImplicitSdf input; material_asset_name → its
  PbrMaterial) resolve naturally as long as scenes declare upstream
  assets first.
  """
  artifacts = {}
  asset_sys = None
  for s in scene_data.systemDatas:
    if s.className == "AssetSystemData":
      asset_sys = s
      break
  if asset_sys is None:
    return artifacts

  for gen in asset_sys.gens:
    cn       = type(gen).__name__
    # HdriToXirGenData: load-time short-circuit. The .xir is produced
    # eagerly during scene authoring (ork.scene.tojson.py runs the
    # Scene's __init__, which calls self.asset.HdriToXir(...) →
    # wrapper.build() → process_envmap → .xir on disk). At load we
    # just resolve to that path; calling .build() here would re-bake
    # every time the scene loads. To force a re-bake, re-run tojson
    # (or delete the .xir manually).
    if cn == "HdriToXirGenData":
      wrap = HdriToXir.from_gendata(gen)
      artifacts[gen.asset_name] = wrap.output_path()
      continue
    # MeshGenData: baked-geometry mesh drawable. Geometry lives in a sidecar
    # .ogeo chunkfile (gen.geometry_path); rebuild reads it back. Material is
    # resolved by name (like VdbGridToDrawable). Late import avoids the
    # assets <- mesh._common <- asset_core <- assets import cycle.
    if cn == "MeshGenData":
      from ork.hypergraph.assets.mesh._common import MeshAsset
      mat = None
      if gen.material_asset_name:
        if gen.material_asset_name not in artifacts:
          raise KeyError(
            f"MeshGenData {gen.asset_name!r} references unknown material asset "
            f"{gen.material_asset_name!r}; declare it earlier")
        mat = artifacts[gen.material_asset_name]
      elif material_resolver:
        mat = material_resolver(gen)
      if mat is None:
        continue  # caller opted out for this drawable
      wrap = MeshAsset.from_gendata(gen, material=mat)
      artifacts[gen.asset_name] = wrap.build()
      continue
    wrap_cls = _GENDATA_TO_WRAPPER.get(cn)
    if wrap_cls is None:
      raise RuntimeError(
        f"materialize_from_scenedata: no wrapper for {cn!r}; "
        f"register it in ork.ecs.scene.assets._GENDATA_TO_WRAPPER")
    deps = {}
    if cn in ("PbrMaterialGenData", "FreestyleMaterialGenData", "HeightFieldGenData"):
      deps["ctx"] = ctx
    elif cn == "ParticleSystemGenData":
      deps["artifacts"] = artifacts
    elif cn == "VdbGridToDrawableGenData":
      if gen.grid_asset_name not in artifacts:
        raise KeyError(
          f"VdbGridToDrawable {gen.asset_name!r} references unknown "
          f"grid asset {gen.grid_asset_name!r}; declare it earlier")
      deps["grid"] = artifacts[gen.grid_asset_name]
      # Material: resolve by name from earlier-materialized assets, or
      # fall back to the caller-supplied resolver, or skip the gen.
      mat = None
      if gen.material_asset_name:
        if gen.material_asset_name not in artifacts:
          raise KeyError(
            f"VdbGridToDrawable {gen.asset_name!r} references unknown "
            f"material asset {gen.material_asset_name!r}; declare it earlier")
        mat = artifacts[gen.material_asset_name]
      elif material_resolver:
        mat = material_resolver(gen)
      if mat is None:
        continue  # caller opted out for this drawable
      deps["material"] = mat
    wrap  = wrap_cls.from_gendata(gen, **deps)
    built = wrap.build()
    artifacts[gen.asset_name] = built

  return artifacts


###############################################################################
# wire_scene_data — M3.7 post-deserialize patching.
#
# After Object.deserializeJson rehydrates a SceneData from JSON, every
# DrawableData that was a reflected placeholder (e.g.
# RigidPrimitiveDrawableData with empty _primitive/_pipeline/_material)
# needs the runtime fields filled in before the scene can render. We
# carry the asset name across the round-trip via NodeDef-side
# _drawable_asset_name (reflected); on load we materialize all gens via
# materialize_from_scenedata, then walk every SceneGraphComponentData on
# every archetype and replace any node's _drawabledata whose
# drawable_asset_name resolves to a freshly materialized artifact.
#
# Caller (typically EcsRuntime.load_scene) supplies a GPU ctx for
# material gens. Returns the {name → built_artifact} dict from
# materialize_from_scenedata for downstream inspection.
###############################################################################

def wire_scene_data(scene_data, ctx=None, ezapp=None, material_resolver=None):
  """Materialize asset gens + re-attach drawables to node decls.

  Intended for the JSON load path: after Object.deserializeJson rebuilds
  the SceneData, reflected drawable placeholders need their runtime
  fields rebuilt from the asset graph before the simulation can render
  them. Side-effects mutate `scene_data` in place.

  ezapp: required for HdriToXirGenData materialization (envmap bake);
         ignored by other gen kinds.
  """
  artifacts = materialize_from_scenedata(
      scene_data, ctx=ctx, ezapp=ezapp, material_resolver=material_resolver)
  # NOTE: do NOT early-return when artifacts is empty — the skybox_path
  # resolution block below still needs to run for scenes that declare a
  # literal skybox via self.scenegraph(skybox_path=...) without any
  # self.asset.* assets (e.g. hello.py).

  for arch in scene_data.archetypes:
    for comp in arch.components:
      cn = comp.className
      if cn == "SceneGraphComponentData":
        for node_name, nid in comp.nodedatas.items():
          # Re-attach materialized drawable by name (M3.7).
          asset_name = nid.drawable_asset_name
          if asset_name:
            built = artifacts.get(asset_name)
            if built is not None:
              nid.drawabledata = built
          # PBR2 Phase 0 — resolve per-node envmap URI. SG.component
          # may have stored "asset://<name>" pointing at an
          # HdriToXirGenData; flatten to the baked .xir path so the
          # C++ side (SceneGraphSystem post-createDrawable) can hand
          # it straight to loadEnvMapOverride. Literal paths pass
          # through unchanged. Empty = no override.
          raw = nid.envmap_path or ""
          if raw.startswith("asset://"):
            env_name = raw[len("asset://"):]
            resolved = artifacts.get(env_name)
            if not resolved:
              raise KeyError(
                f"node {node_name!r}: envmap_path = {raw!r} but asset "
                f"{env_name!r} did not materialize; declare it via "
                f"self.asset.HdriToXir({env_name!r}, ...)")
            nid.envmap_path = str(resolved)
      elif cn == "ParticlesComponentData":
        asset_name = comp.particles_asset_name
        if not asset_name:
          continue
        built = artifacts.get(asset_name)
        if built is None:
          continue
        comp.drawabledata = built
        # PBR2 Phase 0 — per-particle-system probe wire. The gendata's
        # _probe_entity_name (captured by ParticleSystem(probe=...))
        # resolves to <assetcache>/xirtemp/<probe_entity_name>.xir. We
        # set this as the live drawable's _environmentMapPath so
        # ParticleDrawableData::createDrawable's _loadEnvMapOverride
        # picks it up at instantiation time. If the probe hasn't been
        # baked yet, the path is set but loadEnvMapOverride no-ops on
        # cache miss — silently falls back to scene-global skybox.
        gen = None
        for sd in scene_data.systemDatas:
          if sd.className == "AssetSystemData":
            for g in sd.gens:
              if type(g).__name__ == "ParticleSystemGenData" and g.asset_name == asset_name:
                gen = g
                break
            break
        if gen is not None:
          probe_name = getattr(gen, "probe_entity_name", "") or ""
          if probe_name:
            # Stash the probe-entity name on the live ParticlesDrawableData.
            # ParticlesGlobalSystem._onStageComponent reads this at stage
            # time, looks up the LightProbe by entity name in the sim,
            # and sets drawable->_probeOverride per slot. The live probe
            # cube is bound per-draw via fwdnode_pipeline.cpp.
            if hasattr(built, "probeEntityName"):
              built.probeEntityName = probe_name

  # PBR2 Phase 0 — resolve _skybox_path on SceneGraphSystemData.
  # Two value forms:
  #   "asset://<name>" — look up the HdriToXirGenData artifact (a .xir
  #                       path from HdriToXir.build()) and push to
  #                       _userParams["SkyboxTexPathStr"].
  #   anything else    — literal path; push through unchanged.
  # If _skybox_path is empty, leave whatever's already in _userParams
  # untouched (back-compat with bare-string SkyboxTexPathStr authors).
  for sd in scene_data.systemDatas:
    if sd.className != "SceneGraphSystemData":
      continue
    raw = getattr(sd, "skybox_path", "") or ""
    if not raw:
      continue
    if raw.startswith("asset://"):
      name = raw[len("asset://"):]
      resolved = artifacts.get(name)
      if not resolved:
        raise KeyError(
          f"scenegraph.skybox_path = {raw!r} but asset {name!r} did not "
          f"materialize; declare it via self.asset.HdriToXir({name!r}, ...)")
      sd.declareParams({"SkyboxTexPathStr": str(resolved)})
    else:
      sd.declareParams({"SkyboxTexPathStr": raw})

  return artifacts


__all__ = [
  "HollowFunnelMesh",
  "MeshToSdf",
  "VdbGridToDrawable",
  "ImplicitSdf",
  "ThickSaddleSdf",
  "PbrMaterial",
  "FreestyleMaterial",
  "VdbFileSdf",
  "MeshSdf",
  "materialize_from_scenedata",
  "wire_scene_data",
]
