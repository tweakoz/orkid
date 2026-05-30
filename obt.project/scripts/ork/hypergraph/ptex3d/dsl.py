###############################################################################
# GEOV2 Phase 2 — procedural-surface DSL (typed SurfNode IR + SSA/CSE emitter).
#
# Author a PBR surface as Python expressions over `ctx` atoms and `P.<op>`s;
# the emitter walks the output DAG, shares common subexpressions (CSE), and
# produces the (surface_body, libblock, lib_inherits, extra_imports) that the
# Phase-1 generator (fxv2_template.materialize_surface_fxv2) turns into a real
# .fxv2. Example:
#
#   class CastMetal(Ptex3d):
#     def __init__(self, ctx, *, cell_scale=4.0):
#       cell  = P.voronoi(ctx.P_object * cell_scale)     # .f1 .f2 .cell .cell2
#       seam  = P.smoothstep(0.0, 0.06, cell.f2 - cell.f1)
#       steel = P.mix(rgb(0.35), rgb(0.63), cell.cell)
#       self.surface(albedo    = steel * P.mix(0.45, 1.0, seam),
#                    metallic  = 1.0,
#                    roughness = P.mix(0.25, 0.85, cell.cell2))
#
#   path = materialize_ptex3d(CastMetal, cell_scale=6.0)  # -> cached .fxv2
#
# Design notes (~/GEOV2.md §6/§16-G): this is a NEW typed IR (a sibling of the
# particles _expr.py, not a subclass) + a NEW emitter (the particles _lower.py /
# _bindings.py target CPU dataflow and are NOT reused).
###############################################################################

from orkengine.core import vec2 as _v2, vec3 as _v3, vec4 as _v4

from ork.hypergraph.ptex3d.fxv2_template import materialize_surface_fxv2

_VEC = {1: "float", 2: "vec2", 3: "vec3", 4: "vec4"}
_SWIZ = set("xyzw") | set("rgba")

# A bindable param is stored vec4-padded in the UBO (std140-safe); read swizzled
# back down to its logical type at the use site.
_PARAM_SWIZ = {"float": ".x", "vec2": ".xy", "vec3": ".xyz", "vec4": ""}


def _fmt_float(x):
  s = repr(float(x))
  return s if ("." in s or "e" in s or "E" in s) else s + ".0"


###############################################################################
# IR nodes
###############################################################################

class SurfNode:
  """Typed GLSL expression node. `_type` is one of float/vec2/vec3/vec4(/mat3)."""
  __slots__ = ("_type", "_key_cache")

  def __init__(self, gtype):
    self._type = gtype
    self._key_cache = None

  # structural key for CSE (memoized)
  def key(self):
    if self._key_cache is None:
      self._key_cache = self._make_key()
    return self._key_cache

  def _make_key(self):
    raise NotImplementedError

  # ── operators → Op nodes ──
  def __add__(self, o):  return _binop("+", self, o)
  def __radd__(self, o): return _binop("+", o, self)
  def __sub__(self, o):  return _binop("-", self, o)
  def __rsub__(self, o): return _binop("-", o, self)
  def __mul__(self, o):  return _binop("*", self, o)
  def __rmul__(self, o): return _binop("*", o, self)
  def __truediv__(self, o):  return _binop("/", self, o)
  def __rtruediv__(self, o): return _binop("/", o, self)
  def __neg__(self):
    return Op("(-{0})", [self], self._type)

  # ── swizzle ──
  def __getattr__(self, name):
    if name.startswith("_"):
      raise AttributeError(name)
    if name and len(name) <= 4 and all(c in _SWIZ for c in name):
      return Swizzle(self, name, _VEC[len(name)])
    raise AttributeError(name)


class Const(SurfNode):
  __slots__ = ("_glsl",)

  def __init__(self, glsl, gtype):
    super().__init__(gtype)
    self._glsl = glsl

  def _make_key(self): return ("c", self._type, self._glsl)


class CtxRef(SurfNode):
  """A coordinate/input atom — emits a fixed local name from the generated
  ptex_surface() signature (wpos/opos/uv/cd/tbn/wnrm/eye)."""
  __slots__ = ("_glsl",)

  def __init__(self, glsl, gtype):
    super().__init__(gtype)
    self._glsl = glsl

  def _make_key(self): return ("x", self._glsl)


class Param(SurfNode):
  """A bindable runtime uniform — a member of the generated `ublk_ptex_params`
  block. Stored vec4-padded in the UBO (std140-safe), read swizzled to its
  logical type. `_default` (a python float / 2-4 tuple / colors.* value) seeds
  the binding the asset wrapper pre-binds; rebind live from Python or C++ via
  `material.bindParam(name, value)`."""
  __slots__ = ("_pname", "_default")

  def __init__(self, pname, gtype, default):
    super().__init__(gtype)
    self._pname = pname
    self._default = default

  def _make_key(self): return ("param", self._pname)


class Op(SurfNode):
  """A GLSL builtin/library call or operator. `tmpl` is a format string over
  the arg expressions; `libsrc`/`inherits`/`imports` carry codegen deps."""
  __slots__ = ("_tmpl", "_args", "_libsrc", "_inherits", "_imports")

  def __init__(self, tmpl, args, gtype, libsrc=None, inherits=(), imports=()):
    super().__init__(gtype)
    self._tmpl = tmpl
    self._args = args
    self._libsrc = libsrc
    self._inherits = tuple(inherits)
    self._imports = tuple(imports)

  def _make_key(self):
    return ("o", self._tmpl, self._type, tuple(a.key() for a in self._args))


class Swizzle(SurfNode):
  __slots__ = ("_src", "_comp")

  def __init__(self, src, comp, gtype):
    super().__init__(gtype)
    self._src = src
    self._comp = comp

  def _make_key(self): return ("s", self._comp, self._src.key())


class Bundle:
  """Multi-output op result (e.g. voronoi) exposing named scalar fields that
  swizzle a shared underlying vecN node — so the node emits once (CSE)."""
  def __init__(self, node, fields):
    self._node = node
    self._fields = fields

  def __getattr__(self, name):
    f = self.__dict__["_fields"]
    if name in f:
      return Swizzle(self.__dict__["_node"], f[name], "float")
    raise AttributeError(name)


###############################################################################
# wrapping + type rules
###############################################################################

def _wrap(x):
  if isinstance(x, SurfNode):
    return x
  if isinstance(x, bool):
    return Const("true" if x else "false", "bool")
  if isinstance(x, (int, float)):
    return Const(_fmt_float(x), "float")
  # orkengine vec2/vec3/vec4 objects (the preferred authoring form)
  if isinstance(x, _v2):
    comps = (x.x, x.y)
  elif isinstance(x, _v3):
    comps = (x.x, x.y, x.z)
  elif isinstance(x, _v4):
    comps = (x.x, x.y, x.z, x.w)
  elif isinstance(x, (tuple, list)):
    comps = tuple(x)
  else:
    comps = None
  if comps is not None:
    n = len(comps)
    if n in (2, 3, 4):
      return Const("%s(%s)" % (_VEC[n], ", ".join(_fmt_float(v) for v in comps)), _VEC[n])
    raise TypeError("vector literal must be len 2/3/4, got %d" % n)
  # _LazyColor (ork.hypergraph.colors) support — hsv/wavelength/colortemp/colors.*
  if hasattr(x, "to_vec3") and hasattr(x, "to_vec4"):
    v = x.to_vec3()
    return Const("vec3(%s, %s, %s)" % (_fmt_float(v.x), _fmt_float(v.y), _fmt_float(v.z)), "vec3")
  raise TypeError("cannot use %r in a ptex3d expression" % (x,))


def _binop_type(ta, tb):
  if ta == tb:
    return ta
  if ta == "float":
    return tb
  if tb == "float":
    return ta
  raise TypeError("incompatible operand types %s and %s" % (ta, tb))


def _binop(sym, a, b):
  a = _wrap(a); b = _wrap(b)
  return Op("({0} %s {1})" % sym, [a, b], _binop_type(a._type, b._type))


###############################################################################
# GLSL helper sources for ops that need a libblock (inlined into the generated
# surface libblock; disjoint so no duplicate function defs). A future refinement
# promotes these into shared proctex_*.i2 files (GEOV2 §7).
###############################################################################

_VORONOI_SRC = """
float _ptex_shash3(vec3 p) { return fract(sin(dot(p, vec3(127.1,311.7,74.7))) * 43758.5453); }
vec3  _ptex_vhash3(vec3 p) { return fract(sin(vec3(dot(p,vec3(127.1,311.7,74.7)),
                                                   dot(p,vec3(269.5,183.3,246.1)),
                                                   dot(p,vec3(113.5,271.9,124.6)))) * 43758.5453); }
// x=border coord, onrm=OBJECT-space surface normal (for the surface-aware width
// correction). Returns a ptex_voro_t struct (defined in the types typeblock):
//   .f1     = distance to the cell's feature point (smooth radial -> domes)
//   .edge   = raw 3D border distance (world/coord units, unfiltered)
//   .fwedge = the border width-corrected for how the surface slices the 3D
//             cell-wall (constant apparent width, no crease dots)
//   .cellA/.cellB = per-cell hashes
// Unused members are dead-code-eliminated by the SPIR-V compiler (e.g. a domed
// surface using only .f1/.cellA pays nothing for pass 2 / fwedge).
ptex_voro_t _ptex_voronoi(vec3 x, vec3 onrm) {
  vec3 ip = floor(x), fp = fract(x);
  // pass 1 — nearest feature point: remember its cell offset (mg) and the
  // vector to it (mr), plus the nearest cell's hashes.
  vec3  mg = vec3(0.0), mr = vec3(0.0);
  float f1 = 1e9, idA = 0.0, idB = 0.0;
  for (int k=-1;k<=1;k++) for (int j=-1;j<=1;j++) for (int i=-1;i<=1;i++) {
    vec3 g = vec3(float(i), float(j), float(k));
    vec3 r = g + _ptex_vhash3(ip+g) - fp;
    float d = dot(r, r);
    if (d < f1) { f1 = d; mr = r; mg = g; idA = _ptex_shash3(ip+g); idB = _ptex_shash3(ip+g+vec3(31.7)); }
  }
  // pass 2 — UNIFORM-WIDTH border distance (Quilez): min over the nearest
  // cell's neighbors of the distance to the bisecting plane between the
  // nearest point and that neighbor; also keep that wall's plane normal.
  float edge = 1e9;
  vec3  enrm = onrm;
  for (int k=-1;k<=1;k++) for (int j=-1;j<=1;j++) for (int i=-1;i<=1;i++) {
    vec3 g = mg + vec3(float(i), float(j), float(k));
    vec3 r = g + _ptex_vhash3(ip+g) - fp;
    vec3 diff = r - mr;
    float lensq = dot(diff, diff);
    if (lensq > 1e-5) {                      // skip the nearest point itself
      vec3  n = diff * inversesqrt(lensq);   // wall plane normal (coord space)
      float d = dot(0.5*(mr+r), n);
      if (d < edge) { edge = d; enrm = n; }
    }
  }
  // ANALYTIC width correction: a 3D wall sliced by the surface widens by
  // 1/sin(angle) at grazing. The surface-tangential gradient magnitude of the
  // distance field is |n - (n.N)N| = sin(angle); divide it out -> a width that
  // is uniform along the surface, in coordinate units, and dot-free.
  float gT = length(enrm - dot(enrm, onrm) * onrm);
  float fwedge = edge / max(gT, 1e-3);
  return ptex_voro_t(sqrt(f1), edge, fwedge, idA, idB);
}
"""

# Gradient-returning voronoi for the ANALYTIC cellular bump (GEOV2 §18 option 1).
# Returns .x = fwedge AND .yzw = the object-space gradient direction of fwedge
# (the surface-corrected cell-wall normal). One eval replaces the 3 finite-
# difference height taps — the wall normal IS the height gradient direction, so
# we get the bump analytically instead of re-marching warp+voronoi per tap.
# Relies on _ptex_vhash3 from _VORONOI_SRC (always co-emitted; dedup keeps it once).
_VORONOI_G_SRC = """
vec4 _ptex_voronoi_g(vec3 x, vec3 onrm) {   // .x=fwedge, .yzw=grad(fwedge) dir
  vec3 ip = floor(x), fp = fract(x);
  vec3 mg = vec3(0.0), mr = vec3(0.0);
  float f1 = 1e9;
  for (int k=-1;k<=1;k++) for (int j=-1;j<=1;j++) for (int i=-1;i<=1;i++) {
    vec3 g = vec3(float(i), float(j), float(k));
    vec3 r = g + _ptex_vhash3(ip+g) - fp;
    float d = dot(r, r);
    if (d < f1) { f1 = d; mr = r; mg = g; }
  }
  float edge = 1e9; vec3 enrm = onrm;
  for (int k=-1;k<=1;k++) for (int j=-1;j<=1;j++) for (int i=-1;i<=1;i++) {
    vec3 g = mg + vec3(float(i), float(j), float(k));
    vec3 r = g + _ptex_vhash3(ip+g) - fp;
    vec3 diff = r - mr;
    float lensq = dot(diff, diff);
    if (lensq > 1e-5) {
      vec3 n = diff * inversesqrt(lensq);
      float d = dot(0.5*(mr+r), n);
      if (d < edge) { edge = d; enrm = n; }
    }
  }
  float gT = max(length(enrm - dot(enrm, onrm) * onrm), 1e-3);
  return vec4(edge / gT, enrm / gT);
}
"""

_FBM_SRC = """
float _ptex_fbm(vec3 p, int octaves) {    // value-noise fbm over lib_mmnoise::noise
  float v = 0.0, amp = 0.5, fr = 1.0;
  for (int i = 0; i < octaves; i++) { v += amp * noise(p * fr); amp *= 0.5; fr *= 2.0; }
  return v;
}
"""


###############################################################################
# P — the op namespace
###############################################################################

class _Ops:
  # constructors / colors
  def vec2(self, *a): return self._vec(2, a)
  def vec3(self, *a): return self._vec(3, a)
  def vec4(self, *a): return self._vec(4, a)

  def _vec(self, n, a):
    a = [_wrap(x) for x in a]
    if len(a) == 1 and a[0]._type == "float":       # scalar broadcast
      return Op(_VEC[n] + "({0})", a, _VEC[n])
    tmpl = _VEC[n] + "(" + ", ".join("{%d}" % i for i in range(len(a))) + ")"
    return Op(tmpl, a, _VEC[n])

  # component-wise (type follows the arg)
  def _cw1(self, fn, x):
    x = _wrap(x); return Op("%s({0})" % fn, [x], x._type)
  def sin(self, x):   return self._cw1("sin", x)
  def cos(self, x):   return self._cw1("cos", x)
  def abs(self, x):   return self._cw1("abs", x)
  def floor(self, x): return self._cw1("floor", x)
  def fract(self, x): return self._cw1("fract", x)
  def sqrt(self, x):  return self._cw1("sqrt", x)
  def fwidth(self, x): return self._cw1("fwidth", x)   # screen-space |dFdx|+|dFdy|
  def normalize(self, x): return self._cw1("normalize", x)
  def saturate(self, x):  x = _wrap(x); return Op("clamp({0}, 0.0, 1.0)", [x], x._type)

  def mix(self, a, b, t):
    a = _wrap(a); b = _wrap(b); t = _wrap(t)
    return Op("mix({0}, {1}, {2})", [a, b, t], _binop_type(a._type, b._type))
  def clamp(self, x, lo, hi):
    x = _wrap(x); return Op("clamp({0}, {1}, {2})", [x, _wrap(lo), _wrap(hi)], x._type)
  def step(self, edge, x):
    x = _wrap(x); return Op("step({0}, {1})", [_wrap(edge), x], x._type)
  def smoothstep(self, e0, e1, x):
    x = _wrap(x); return Op("smoothstep({0}, {1}, {2})", [_wrap(e0), _wrap(e1), x], x._type)
  def pow(self, a, b):
    a = _wrap(a); return Op("pow({0}, {1})", [a, _wrap(b)], a._type)
  def min(self, a, b):
    a = _wrap(a); b = _wrap(b); return Op("min({0}, {1})", [a, b], _binop_type(a._type, b._type))
  def max(self, a, b):
    a = _wrap(a); b = _wrap(b); return Op("max({0}, {1})", [a, b], _binop_type(a._type, b._type))

  def dot(self, a, b):    return Op("dot({0}, {1})", [_wrap(a), _wrap(b)], "float")
  def length(self, x):    return Op("length({0})", [_wrap(x)], "float")
  def cross(self, a, b):  return Op("cross({0}, {1})", [_wrap(a), _wrap(b)], "vec3")

  # noise / cellular
  def noise(self, p):
    return Op("noise({0})", [_wrap(p)], "float", inherits=("lib_mmnoise",))
  def fbm(self, p, octaves=4):
    return Op("_ptex_fbm({0}, %d)" % int(octaves), [_wrap(p)], "float",
              libsrc=_FBM_SRC, inherits=("lib_mmnoise",))
  def voronoi(self, p):
    # Returns a bundle over a ptex_voro_t struct. Fields (unused ones are DCE'd
    # by the compiler — e.g. a .f1-only dome doesn't pay for the .fwedge pass):
    #   .f1     = distance to the cell centre/site (smooth radial -> rounded domes)
    #   .edge   = raw 3D border distance (unfiltered)
    #   .fwedge = surface-width-corrected border (constant apparent width, dot-free)
    #   .cell / .cell2 = per-cell hashes
    # (passes the object-space surface normal `onrm` for the width correction.)
    node = Op("_ptex_voronoi({0}, onrm)", [_wrap(p)], "ptex_voro_t", libsrc=_VORONOI_SRC)
    return Bundle(node, {"f1": "f1", "edge": "edge", "fwedge": "fwedge",
                         "cell": "cellA", "cell2": "cellB"})


P = _Ops()


def rgb(*a):
  return P.vec3(*a)


###############################################################################
# SurfaceCtx — coordinate / input atoms (the generated ptex_surface() locals)
###############################################################################

class SurfaceCtx:
  P        = property(lambda self: CtxRef("wpos", "vec3"))   # world position
  P_object = property(lambda self: CtxRef("opos", "vec3"))   # object-space position
  N        = property(lambda self: CtxRef("wnrm", "vec3"))   # world normal
  uv       = property(lambda self: CtxRef("uv",   "vec2"))   # free-range uv
  Cd       = property(lambda self: CtxRef("cd",   "vec4"))   # 4D per-vertex selector
  eye      = property(lambda self: CtxRef("eye",  "vec3"))   # camera world position

  def param(self, name, default):
    """Declare a bindable runtime uniform. `default` (a python scalar / 2-4
    tuple / colors.* value) sets both the logical type and the pre-bound value;
    rebind at runtime via `material.bindParam(name, value)`. The generator emits
    a `ublk_ptex_params` member (vec4-padded); the asset wrapper round-trips the
    default in `PbrMaterialGenData.shader_params`."""
    d = _wrap(default)
    if d._type not in ("float", "vec2", "vec3", "vec4"):
      raise TypeError("ctx.param(%r): type must be float/vec2/vec3/vec4, got %s"
                      % (name, d._type))
    return Param(name, d._type, default)


###############################################################################
# Ptex3d base + materialize
###############################################################################

_FIELD_TYPE = {"albedo": "vec3", "normal": "vec3", "emissive": "vec3",
               "metallic": "float", "roughness": "float", "ao": "float"}


class Ptex3d:
  """Author surface base. Subclass and, in __init__(self, ctx, **params), build
  expressions and call self.surface(albedo=..., metallic=..., ...)."""

  def surface(self, *, albedo=None, metallic=None, roughness=None,
              normal=None, emissive=None, ao=None):
    chans = {}
    for name, val in (("albedo", albedo), ("metallic", metallic), ("roughness", roughness),
                      ("normal", normal), ("emissive", emissive), ("ao", ao)):
      if val is not None:
        chans[name] = _wrap(val)
    self._channels = chans

  def displace(self, height, *, scale=0.05, parallax_steps=0, depth=0.04):
    """Declare a procedural displacement height — a scalar field of `ctx.P_object`
    (0 = recessed, 1 = raised, by convention; GEOV2 §18). Phase 4 drives ANALYTIC
    BUMP from it now (the relief catches light) and parallax-occlusion later. The
    height must be a function of `ctx.P_object` (+ params/constants) so it can be
    re-evaluated at offset/marched coordinates. `scale` sets the bump strength.
    GENERAL but costly — finite-difference (3 height taps/fragment). For a
    voronoi-edge recession use `displace_cellular` (1 eval, analytic gradient).

    `parallax_steps` > 0 adds PROCEDURAL parallax-occlusion: the surface is
    evaluated at the marched displaced coordinate (real depth + self-occlusion),
    not just bump-shaded. EXPENSIVE/TEMPORARY — re-marches the height
    `parallax_steps`× per fragment (GEOV2 §18.5; a baked-height texture replaces
    it once auto-uv lands). `parallax_steps` is a BAKE-TIME constant (the march
    loop bound); 0 (default) = bump only, no march. `depth` is the relief depth
    in object units (plain float or a `ctx.param`)."""
    self._height = _wrap(height)
    self._height_scale = _wrap(scale)
    if parallax_steps > 0:
      self._parallax = dict(steps=int(parallax_steps), depth=_wrap(depth))

  def displace_cellular(self, coord, *, width, scale=0.1):
    """ANALYTIC cellular relief (GEOV2 §18 option 1): the height is
    `smoothstep(0, width, voronoi(coord).fwedge)` — recessed cracks, raised
    plates — and the bump comes from ONE gradient-voronoi eval (the cell-wall
    normal IS the height gradient), not finite differences. ~3× cheaper than
    `displace`. `coord` is the voronoi coordinate (e.g. the same warped coord the
    surface samples); `width` the crack width; `scale` the bump strength."""
    self._cellular = dict(coord=_wrap(coord), width=_wrap(width), scale=_wrap(scale))


class _Emitter:
  def __init__(self, subst=None):
    self.lines = []
    self.cache = {}        # node.key() -> ssa varname
    self.n = 0
    self.libsrcs = []      # ordered-unique GLSL helper sources
    self.inherits = set()
    self.imports = set()
    self.params = {}       # insertion-ordered: pname -> (gtype, default)
    # atom-name remap (e.g. {"opos": "coord"}) — lets the height field be emitted
    # as a function of a marched/offset coordinate rather than the fixed varying.
    self.subst = subst or {}

  def expr(self, node):
    if isinstance(node, Const):
      return node._glsl
    if isinstance(node, CtxRef):
      return self.subst.get(node._glsl, node._glsl)
    if isinstance(node, Param):
      if node._pname not in self.params:
        self.params[node._pname] = (node._type, node._default)
      # shadlang references uniform_block members by BARE name (like ModColor),
      # not block.member — read the vec4-padded slot swizzled to logical type.
      return "%s%s" % (node._pname, _PARAM_SWIZ[node._type])
    if isinstance(node, Swizzle):
      return "%s.%s" % (self.expr(node._src), node._comp)
    if isinstance(node, Op):
      k = node.key()
      hit = self.cache.get(k)
      if hit is not None:
        return hit
      args = [self.expr(a) for a in node._args]
      if node._libsrc and node._libsrc not in self.libsrcs:
        self.libsrcs.append(node._libsrc)
      self.inherits.update(node._inherits)
      self.imports.update(node._imports)
      var = "t%d" % self.n
      self.n += 1
      self.lines.append("%s %s = %s;" % (node._type, var, node._tmpl.format(*args)))
      self.cache[k] = var
      return var
    raise TypeError("unknown node %r" % (node,))


def _coerce(expr, have, want):
  if have == want:
    return expr
  if want == "vec3" and have == "float":
    return "vec3(%s)" % expr
  raise TypeError("surface channel type mismatch: have %s, want %s" % (have, want))


def _emitter_deps(em):
  """(libsrcs_list, inherits_set, imports_set, param_specs) from an emitter."""
  imports = set(em.imports)
  if "lib_mmnoise" in em.inherits:
    imports.add("orkshader://misctools.i2")
  params = [(name, gt, dflt) for name, (gt, dflt) in em.params.items()]
  return list(em.libsrcs), set(em.inherits), imports, params


def emit_surface(channels):
  """channels: {field -> SurfNode} ->
     (surface_body, libsrcs_list, lib_inherits, extra_imports, param_specs).
  libsrcs_list is the ORDERED-UNIQUE list of GLSL helper sources (kept as a list,
  not joined, so the caller can dedup it against the height field's helpers —
  a shared helper like the voronoi must be defined exactly once). param_specs is
  the insertion-ordered (name, gtype, default) of the bindable uniforms used."""
  em = _Emitter()
  assigns = []
  for field, node in channels.items():
    e = _coerce(em.expr(node), node._type, _FIELD_TYPE[field])
    assigns.append("o.%s = %s;" % (field, e))
  body = "\n".join(em.lines + assigns)
  libsrcs, inherits, imports, params = _emitter_deps(em)
  return body, libsrcs, sorted(inherits), sorted(imports), params


def emit_height(node, coord="coord"):
  """Emit a displacement-height expression as the body of
  `float ptex_height(vec3 coord, vec3 onrm)` — the object-position atom (`opos`)
  is rebound to `coord`, so the function is re-evaluable at offset/marched
  coordinates (analytic bump now, parallax later). The height must be a function
  of `ctx.P_object` (+ params/constants). Returns
  (lines, final_expr, libsrcs_list, lib_inherits, extra_imports, param_specs)."""
  em = _Emitter(subst={"opos": coord})
  final = _coerce(em.expr(node), node._type, "float")
  libsrcs, inherits, imports, params = _emitter_deps(em)
  return em.lines, final, libsrcs, sorted(inherits), sorted(imports), params


def emit_cellular(coord_node, width_node, scale_node, coord_name="opos"):
  """Emit the cellular-bump inputs: the voronoi coordinate, the crack width, and
  the bump strength, as GLSL evaluated at the fragment's object position (`opos`
  rebound to frg_opos). width/scale may be bindable-param uniforms. Co-emits the
  gradient-voronoi helper. Returns
  (lines, coord_expr, width_expr, scale_expr, libsrcs, inherits, imports, params)."""
  em = _Emitter(subst={"opos": coord_name})
  cvar = _coerce(em.expr(coord_node), coord_node._type, "vec3")
  wvar = _coerce(em.expr(width_node), width_node._type, "float")
  svar = _coerce(em.expr(scale_node), scale_node._type, "float")
  libsrcs, inherits, imports, params = _emitter_deps(em)
  libsrcs = _union_ordered(libsrcs, [_VORONOI_SRC, _VORONOI_G_SRC])  # hashes + grad voronoi
  return em.lines, cvar, wvar, svar, libsrcs, sorted(inherits), sorted(imports), params


def _union_ordered(a, b):
  out = list(a)
  for x in b:
    if x not in out:
      out.append(x)
  return out


def _merge_param_specs(a, b):
  seen = {n for (n, _g, _d) in a}
  return a + [p for p in b if p[0] not in seen]


def _build_ptex3d(dsl_class, name_hint=None, **params):
  inst = dsl_class(SurfaceCtx(), **params)
  if not getattr(inst, "_channels", None):
    raise RuntimeError("%s built no surface() channels" % dsl_class.__name__)
  body, libsrcs, inherits, imports, pspecs = emit_surface(inst._channels)

  height_kwargs = {}
  height   = getattr(inst, "_height", None)
  cellular = getattr(inst, "_cellular", None)
  if cellular is not None:
    c_lines, c_coord, c_width, c_scale, c_libsrcs, c_inh, c_imp, c_params = emit_cellular(
        cellular["coord"], cellular["width"], cellular["scale"])
    libsrcs  = _union_ordered(libsrcs, c_libsrcs)
    inherits = sorted(set(inherits) | set(c_inh))
    imports  = sorted(set(imports) | set(c_imp))
    pspecs   = _merge_param_specs(pspecs, c_params)
    height_kwargs = dict(cellular_body="\n".join(c_lines),
                         cellular_coord=c_coord,
                         cellular_width=c_width,
                         cellular_scale=c_scale)
  elif height is not None:
    h_lines, h_final, h_libsrcs, h_inh, h_imp, h_params = emit_height(height)
    libsrcs  = _union_ordered(libsrcs, h_libsrcs)        # dedup shared helpers
    inherits = sorted(set(inherits) | set(h_inh))
    imports  = sorted(set(imports) | set(h_imp))
    pspecs   = _merge_param_specs(pspecs, h_params)
    # scale (+ parallax depth) must emit to single inline exprs (param/const,
    # no SSA lines) — they're read in the template's bump / march blocks.
    par = getattr(inst, "_parallax", None)
    dem = _Emitter(subst={"opos": "opos"})
    sexpr = _coerce(dem.expr(inst._height_scale), inst._height_scale._type, "float")
    dexpr = None
    if par is not None:
      dexpr = _coerce(dem.expr(par["depth"]), par["depth"]._type, "float")
    if dem.lines:
      raise ValueError("displace(scale=/depth=) must be params or constants, not compound expressions")
    pspecs = _merge_param_specs(pspecs, _emitter_deps(dem)[3])
    height_kwargs = dict(height_body="\n".join(h_lines),
                         height_expr=h_final,
                         displace_scale=sexpr)
    if par is not None:
      height_kwargs["parallax_steps"] = par["steps"]
      height_kwargs["parallax_depth"] = dexpr

  libblock = "\n".join(s.strip() for s in libsrcs)
  path = materialize_surface_fxv2(body, libblock=libblock, lib_inherits=inherits,
                                  extra_imports=imports, params=pspecs,
                                  name_hint=name_hint or dsl_class.__name__.lower(),
                                  **height_kwargs)
  return path, pspecs


def materialize_ptex3d(dsl_class, *, name_hint=None, **params):
  """Instantiate a Ptex3d subclass, emit its surface, and bake the .fxv2.
  Returns the cached .fxv2 path."""
  path, _ = _build_ptex3d(dsl_class, name_hint=name_hint, **params)
  return path


def materialize_ptex3d_full(dsl_class, *, name_hint=None, **params):
  """Like materialize_ptex3d but also returns the bindable-param specs
  [(name, gtype, default), ...] — the asset layer pre-binds these defaults and
  round-trips them in PbrMaterialGenData.shader_params. Returns (path, specs)."""
  return _build_ptex3d(dsl_class, name_hint=name_hint, **params)
