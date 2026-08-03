###############################################################################
# sdf — the analytic-SDF expression algebra (E.7/M0). Python composes distance
# expressions; they LOWER to a single GLSL float expression of `vec3 p` (world
# space) baked into an SdfEval kernel — the ptex3d/SelExpr trace pattern. No
# grid exists until the expression bakes into a dense brick (sdf_eval) or a
# mesh gets voxelized in (M1 mesh_to_sdf).
#
#   from ork.hypergraph.dflow import sdf
#   expr = sdf.sphere(1.0) | sdf.sphere(0.8, center=(1.5, 0, 0))    # union
#   expr = sdf.box((1, 0.5, 2)) - sdf.sphere(0.9)                   # subtract
#   expr = sdf.smooth_union(a, b, k=0.25)
#
# Operators: | union, & intersect, - subtract. All emit standard iq-style SDFs.
###############################################################################


def _f(x):
  return "%.9g" % float(x)


def _v3(v):
  x, y, z = (float(v[0]), float(v[1]), float(v[2]))
  return "vec3(%s, %s, %s)" % (_f(x), _f(y), _f(z))


class SdfExpr:
  """A GLSL float distance expression of `vec3 p`."""

  def __init__(self, glsl):
    self._glsl = str(glsl)

  def __or__(self, other):   # union
    return SdfExpr("min(%s, %s)" % (self._glsl, other._glsl))

  def __and__(self, other):  # intersection
    return SdfExpr("max(%s, %s)" % (self._glsl, other._glsl))

  def __sub__(self, other):  # subtraction (self minus other)
    return SdfExpr("max(%s, -(%s))" % (self._glsl, other._glsl))

  def offset(self, d):
    """Surface offset: positive d inflates, negative shrinks."""
    return SdfExpr("(%s) - %s" % (self._glsl, _f(d)))

  def __repr__(self):
    return "SdfExpr(%s)" % self._glsl


def sphere(radius=1.0, center=(0.0, 0.0, 0.0)):
  return SdfExpr("length(p - %s) - %s" % (_v3(center), _f(radius)))


def box(size=(1.0, 1.0, 1.0), center=(0.0, 0.0, 0.0)):
  """Axis-aligned box; `size` = HALF-extents per axis (scalar broadcasts)."""
  if not hasattr(size, "__len__"):
    size = (size, size, size)
  return SdfExpr(
      "(length(max(abs(p - %s) - %s, vec3(0.0, 0.0, 0.0)))"
      " + min(max(abs(p - %s).x - %s, max(abs(p - %s).y - %s, abs(p - %s).z - %s)), 0.0))"
      % (_v3(center), _v3(size),
         _v3(center), _f(size[0]), _v3(center), _f(size[1]), _v3(center), _f(size[2])))


def capsule(a=(0.0, -0.5, 0.0), b=(0.0, 0.5, 0.0), radius=0.5):
  return SdfExpr(
      "(length((p - %s) - (%s - %s) * clamp(dot(p - %s, %s - %s) / dot(%s - %s, %s - %s), 0.0, 1.0)) - %s)"
      % (_v3(a), _v3(b), _v3(a), _v3(a), _v3(b), _v3(a), _v3(b), _v3(a), _v3(b), _v3(a), _f(radius)))


def smooth_union(a, b, k=0.25):
  """Polynomial smooth-min (iq): blends a|b with fillet radius ~k."""
  return SdfExpr(
      "(mix(%s, %s, clamp(0.5 + 0.5 * ((%s) - (%s)) / %s, 0.0, 1.0))"
      " - %s * clamp(0.5 + 0.5 * ((%s) - (%s)) / %s, 0.0, 1.0)"
      " * (1.0 - clamp(0.5 + 0.5 * ((%s) - (%s)) / %s, 0.0, 1.0)))"
      % (b._glsl, a._glsl, b._glsl, a._glsl, _f(k),
         _f(k), b._glsl, a._glsl, _f(k),
         b._glsl, a._glsl, _f(k)))


###############################################################################
# Fluent builder — a bound SDF context (`hm.sdf(dim=, extent=)`). Shapes share
# the framing; `-` / `|` / `&` compose CSG; `.to_mesh()` marching-tetrahedra's
# the result into the hypermesh. Sugar over hm.sdf_eval / hm.csg / hm.sdf_to_mesh:
#
#   s    = self.sdf(dim=128, extent=4.0)
#   box  = s.box(size=(1, 1, 1))
#   sph  = s.sphere(0.75)
#   cut  = box - sph                 # subtract (| union, & intersect)
#   mesh = cut.to_mesh()
###############################################################################

class SdfNode:
  """Wraps a hypermesh module whose `Out` is an SdfGrid; composes CSG via operators.
  `.inputs` / `.outputs` delegate to the module (so `sph.inputs.offset = ...` animates)."""

  def __init__(self, hm, module):
    self._hm     = hm
    self._module = module

  @property
  def inputs(self):
    return self._module.inputs

  @property
  def outputs(self):
    return self._module.outputs

  def __sub__(self, other):
    return SdfNode(self._hm, self._hm.csg(self, other, op="subtract"))

  def __or__(self, other):
    return SdfNode(self._hm, self._hm.csg(self, other, op="union"))

  def __and__(self, other):
    return SdfNode(self._hm, self._hm.csg(self, other, op="intersect"))

  def smooth_union(self, other, k=0.25):
    return SdfNode(self._hm, self._hm.csg(self, other, op="smooth_union", k=k))

  def redistance(self, max_iterations=0):
    """Re-normalize to a TRUE |grad|=1 SDF (JFA), preserving the zero-set. Chain
    before .to_mesh() shading or a conform/displace consumer; smooth_union/CSG output
    is NOT a true distance field away from the surface. Returns a chainable SdfNode."""
    return SdfNode(self._hm, self._hm.redistance(self, max_iterations=max_iterations))

  def to_mesh(self, weld=True, blocky=False):
    """Extract this SDF into an INDEXED GpuMesh (the hypermesh terminal). Default = marching tetrahedra
    (smooth). `blocky=True` = CUBERILLE pure voxel-block surface (no smoothing; block size = brick voxel;
    handles disconnected/high-genus topology). `weld` is forced off for blocky (hard per-face normals)."""
    return self._hm.sdf_to_mesh(self, weld=weld, blocky=blocky)

  def to_mesh_clean(self, adaptivity=0.5, unwrap=True, isovalue=0.0, weld_tol=0.0):
    """SHAPE-AWARE clean remesh (the low-poly / good-UV terminal): openvdb curvature-adaptive
    volumeToMesh -> ~20k clean quad-dominant faces (vs marching-tets' uniform ~voxel^2 soup),
    then (unwrap=True) xatlas UV-unwrap for texture baking. `adaptivity` 0..1 (0=detail,1=flat).
    One-shot CPU bake (not for animated SDF). gids do NOT survive — re-gid by band after."""
    return self._hm.sdf_to_mesh_clean(self, adaptivity=adaptivity, unwrap=unwrap,
                                      isovalue=isovalue, weld_tol=weld_tol)


class SdfContext:
  """Bound SDF builder (see hm.sdf). Holds the brick framing shared by every shape."""

  def __init__(self, hm, dim, extent, center):
    self._hm     = hm
    self._dim    = dim
    self._extent = extent
    self._center = center

  def _eval(self, expr):
    return SdfNode(self._hm, self._hm.sdf_eval(expr, dim=self._dim, extent=self._extent, center=self._center))

  def sphere(self, radius=1.0, center=(0.0, 0.0, 0.0)):
    return self._eval(sphere(radius, center=center))

  def box(self, size=(1.0, 1.0, 1.0), center=(0.0, 0.0, 0.0)):
    return self._eval(box(size=size, center=center))

  def capsule(self, a=(0.0, -0.5, 0.0), b=(0.0, 0.5, 0.0), radius=0.5):
    return self._eval(capsule(a=a, b=b, radius=radius))

  def expr(self, e):
    """Bake an arbitrary SdfExpr (or raw GLSL of `vec3 p`) as a brick."""
    return self._eval(e)


__all__ = ["SdfExpr", "sphere", "box", "capsule", "smooth_union", "SdfNode", "SdfContext"]
