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


class TexSample(SurfNode):
  """A render-time 2D TEXTURE sample — `texture(<sname>, <uv>)`, a vec4. `sname` becomes a
  real `sampler2D` uniform in the generated FXV2 (a bindable, NOT baked like ctx.param), so a
  Texture can be attached at runtime via `material.bindParam(sname, tex)`. In a compute BAKE
  (no runtime sampler) it degrades to a constant `_default`. Swizzle (.r/.x) for a 1-channel map."""
  __slots__ = ("_sname", "_uv", "_default")

  def __init__(self, sname, uv, default=0.0):
    super().__init__("vec4")
    self._sname = sname
    self._uv = uv
    self._default = default

  def _make_key(self): return ("tex", self._sname, self._uv.key())


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

_FBM_AA_SRC = """
float _ptex_fbm_aa(vec3 p, int octaves, float aabias) {   // footprint-band-limited fbm (a procedural mip)
  // footprint of THIS coord (p-units per pixel, MAJOR axis) — the same metric texture
  // hardware uses to pick a mip LOD: distance + glancing anisotropy grow it; camera
  // orientation does not. Each octave's spatial frequency is `fr`, so fr*foot is its
  // cycles-per-pixel; taper it out across Nyquist (~0.5) so a sub-pixel octave fades to
  // its mean instead of aliasing. Identical to _ptex_fbm when foot->0 (every bl==1).
  // `aabias` scales the footprint: >1 fades octaves earlier (more aggressive AA / softer),
  // <1 keeps them longer (sharper, more alias risk). 1.0 == neutral.
  float foot = max(length(dFdx(p)), length(dFdy(p))) * aabias;
  float v = 0.0, amp = 0.5, fr = 1.0;
  for (int i = 0; i < octaves; i++) {
    float bl = 1.0 - smoothstep(0.30, 0.70, fr * foot);   // <-- fbm band-limit window
    // fade this octave toward its MEAN (0.5), not toward zero, so the overall LEVEL is
    // preserved as sub-pixel octaves drop out — a faded octave should contribute its
    // average, exactly what a mip texel holds. (== _ptex_fbm when every bl==1.)
    v += amp * (0.5 + bl * (noise(p * fr) - 0.5)); amp *= 0.5; fr *= 2.0;
  }
  return v;
}
"""

# Reusable 2D hexagonal grid (tile any uv chart). Returns a vec4:
#   .x = edge distance (0 at a cell border, ~0.5 at the centre) -> seams
#   .y = per-cell id hash in [0,1] -> colour / variation per cell
#   .zw = local offset from the cell centre (in-cell patterns)
_HEXGRID_SRC = """
float _ptex_hash21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec4 _ptex_hexgrid(vec2 uv) {
  vec2 r = vec2(1.0, 1.7320508), hh = r * 0.5;
  vec2 a = mod(uv, r) - hh;
  vec2 b = mod(uv - hh, r) - hh;
  vec2 gv;
  if (dot(a, a) < dot(b, b)) gv = a; else gv = b;
  vec2 id = uv - gv;
  vec2 q  = abs(gv);
  float dc = max(dot(q, vec2(0.5, 0.8660254)), q.x);   // dist from centre, 0.5 at the border
  return vec4(0.5 - dc, _ptex_hash21(id), gv);
}
"""

# IMPLICIT uniform sphere tiling — the spherical Fibonacci lattice (Keinert et al.
# 2015, "Spherical Fibonacci Mapping"). Closed-form inverse: given a direction +
# point count n it finds the nearest 2 lattice points (4 candidates) — no stored
# points, no UV chart, no pole singularity, uniform density, n is just a scalar.
_SF_SRC = """
float _sf_madfrac(float a, float b) { return a*b - floor(a*b); }
vec4 _ptex_spherecells(vec3 p, float n) {   // .x=id hash, .y=F2-F1 gap (~0 at seam), .z=F1 dist
  vec3 q = normalize(p);
  float kpi = 3.14159265359;
  float kph = 1.61803398875;
  float ang = min(atan(q.z, q.x), kpi);
  float cosT = q.y;
  float k = max(2.0, floor(log(n * kpi * sqrt(5.0) * max(0.0001, 1.0 - cosT*cosT)) / log(kph*kph)));
  float Fk = pow(kph, k) / sqrt(5.0);
  float F0 = floor(0.5 + Fk);
  float F1 = floor(0.5 + Fk * kph);
  float b00 = 2.0*kpi*_sf_madfrac(F0+1.0, kph-1.0) - 2.0*kpi*(kph-1.0);
  float b01 = 2.0*kpi*_sf_madfrac(F1+1.0, kph-1.0) - 2.0*kpi*(kph-1.0);
  float b10 = -2.0*F0/n;
  float b11 = -2.0*F1/n;
  float det = b00*b11 - b01*b10;
  float rx = ang;
  float ry = cosT - (1.0 - 1.0/n);
  float c0 = floor(( b11*rx - b01*ry) / det);
  float c1 = floor((-b10*rx + b00*ry) / det);
  float d1 = 8.0;
  float d2 = 8.0;
  float jb = 0.0;
  for (int s = 0; s < 4; s++) {
    float su = float(s - 2*(s/2));
    float sv = float(s/2);
    float ct = b10*(su+c0) + b11*(sv+c1) + (1.0 - 1.0/n);
    ct = clamp(ct, -1.0, 1.0)*2.0 - ct;
    float ii = floor(n*0.5 - ct*n*0.5);
    float ph = 2.0*kpi*_sf_madfrac(ii, kph-1.0);
    ct = 1.0 - (2.0*ii + 1.0)/n;
    float st = sqrt(max(0.0, 1.0 - ct*ct));
    vec3 pt = vec3(cos(ph)*st, ct, sin(ph)*st);
    float sd = dot(pt - q, pt - q);
    if (sd < d1) { d2 = d1; d1 = sd; jb = ii; }
    else if (sd < d2) { d2 = sd; }
  }
  return vec4(fract(sin(jb*12.9898)*43758.5453), sqrt(d2) - sqrt(d1), sqrt(d1), 0.0);
}
"""



###############################################################################
# P — the op namespace
###############################################################################

class _Ops:
  # ---- external extension hook (ork.hypergraph.registry) ----
  # Any op name NOT defined natively below falls back to the shared DSL op registry, so external
  # packages (e.g. orkflow) can add ptex3d surface ops via `@op("ptex3d","name")` and call them as
  # P.<name>(...) with NO edit to this class. Native methods always win (this runs only when normal
  # attribute lookup misses). Dunders are never intercepted (pickling/copy probe them).
  def __getattr__(self, name):
    if name.startswith("__"):
      raise AttributeError(name)
    from ork.hypergraph.registry import get_op
    fn = get_op("ptex3d", name)
    if fn is None:
      raise AttributeError(
          f"ptex3d DSL has no op '{name}' — define it natively, or register it from any package: "
          f"`from ork.hypergraph.registry import op; @op('ptex3d','{name}')`")
    return fn

  def register(self, name, fn=None):
    """Register a ptex3d surface op -> callable as P.<name>(...). Decorator or direct:
       P.register('marble')(fn)   |   P.register('marble', fn)."""
    from ork.hypergraph.registry import register_op
    if fn is None:
      return lambda f: register_op("ptex3d", name, f)
    return register_op("ptex3d", name, fn)

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
  def atan(self, x):  return self._cw1("atan", x)
  def abs(self, x):   return self._cw1("abs", x)

  def atan2(self, y, x):   # GLSL atan(y, x) == atan2; -> angle in [-pi, pi]
    return Op("atan({0}, {1})", [_wrap(y), _wrap(x)], "float")
  def floor(self, x): return self._cw1("floor", x)
  def fract(self, x): return self._cw1("fract", x)
  def sqrt(self, x):  return self._cw1("sqrt", x)
  def fwidth(self, x): return self._cw1("fwidth", x)   # screen-space |dFdx|+|dFdy|
  def dFdx(self, x):   return self._cw1("dFdx", x)     # screen-space d/dx (per-pixel, screen-x)
  def dFdy(self, x):   return self._cw1("dFdy", x)     # screen-space d/dy (per-pixel, screen-y)
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
  def mod(self, a, b):
    a = _wrap(a); return Op("mod({0}, {1})", [a, _wrap(b)], a._type)
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
  def fbm_aa(self, p, octaves=4, aa=1.0):   # footprint-band-limited fbm: fades sub-pixel octaves
    # `aa` scales the footprint (the AA-aggressiveness knob): >1 fades sooner, <1 sharper.
    return Op("_ptex_fbm_aa({0}, %d, {1})" % int(octaves), [_wrap(p), _wrap(aa)], "float",
              libsrc=_FBM_AA_SRC, inherits=("lib_mmnoise",))
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

  def hexgrid(self, uv):
    """Reusable 2D hexagonal grid over a vec2 (tile any uv chart — sphere via a
    projection, a flat surface, etc.). Bundle:
      .edge = distance to the nearest cell border (0 at border) — seams / tile gap
              of any width: `P.smoothstep(0, tile_width, hex.edge)`
      .id   = per-cell hash in [0,1] — colour / roughness / variation per cell"""
    node = Op("_ptex_hexgrid({0})", [_wrap(uv)], "vec4", libsrc=_HEXGRID_SRC)
    return Bundle(node, {"edge": "x", "id": "y"})

  def spherecells(self, p, n):
    """IMPLICIT uniform sphere tiling — the spherical Fibonacci lattice. Given an
    object-space direction `p` and an (approx) tile count `n` (a runtime scalar —
    no recompile, no stored points, no UV chart, no pole singularity), returns a
    bundle:
      .id   = per-cell hash in [0,1] — colour / variation per cell
      .edge = the F2-F1 gap (~0 at a cell seam) — seams: smoothstep(0, w, hex.edge)
      .f1   = chord distance to the cell centre
    A sphere can't be all-hexagons (Euler -> 12 pentagons), but this is uniform +
    seamless. For 2D / UV-mapped surfaces use `hexgrid` instead."""
    node = Op("_ptex_spherecells({0}, {1})", [_wrap(p), _wrap(n)], "vec4", libsrc=_SF_SRC)
    return Bundle(node, {"id": "x", "edge": "y", "f1": "z"})

  def func(self, tmpl, args, *, rtype="float", libsrc=None, inherits=(), imports=()):
    """GLSL escape hatch — inject a custom function call for procedural ops that
    don't fit the core op set (e.g. a hardcoded-point pattern). `tmpl` is a format
    string over the arg exprs (e.g. "_soccer({0})"); `libsrc` its GLSL definition;
    `rtype` the result type (swizzle the returned node for multi-output). NOTE:
    GLSL-specific — won't lower to other backends until the IR op-abstraction
    refactor (#3); prefer composing core ops where practical."""
    return Op(tmpl, [_wrap(a) for a in args], rtype,
              libsrc=libsrc, inherits=tuple(inherits), imports=tuple(imports))


P = _Ops()


def rgb(*a):
  return P.vec3(*a)


###############################################################################
# SurfaceCtx — coordinate / input atoms (the generated ptex_surface() locals)
###############################################################################

class SurfaceCtx:
  P        = property(lambda self: CtxRef("wpos", "vec3"))   # world position
  P_object = property(lambda self: CtxRef("opos", "vec3"))   # object-space position
  N        = property(lambda self: CtxRef("wnrm", "vec3"))   # WORLD normal
  NW       = property(lambda self: CtxRef("wnrm", "vec3"))   # WORLD normal (explicit alias of N)
  N_object = property(lambda self: CtxRef("onrm", "vec3"))   # OBJECT-space normal (triplanar weights)
  NO       = property(lambda self: CtxRef("onrm", "vec3"))   # OBJECT normal (explicit alias of N_object)
  NV       = property(lambda self: CtxRef("vnrm", "vec3"))   # VIEW-space normal — opt-in: referencing it emits the per-vertex transform
  uv       = property(lambda self: CtxRef("uv",   "vec2"))   # free-range uv
  Cd       = property(lambda self: CtxRef("cd",   "vec4"))   # 4D per-vertex selector
  eye      = property(lambda self: CtxRef("eye",  "vec3"))   # camera world position
  footprint = property(lambda self: CtxRef("footprint", "float"))  # per-texel/per-pixel footprint —
  #   target-polymorphic: fragment ~ length(fwidth(p)); bake = texel size extent_m/dim. The portable
  #   substitute for screen derivatives so AA logic (fbm_aa/aa_ramp) can run in a compute bake.
  extent_m  = property(lambda self: CtxRef("extent_m", "float"))   # BAKE: XZ world span (meters)

  def input(self, k):
    """BAKE-only: value of image input In{k} at this texel (hfdisplacement/multi-input
    ExprModule). Input 0 is the CURRENT HEIGHT in METERS (also folded into
    ctx.P_object.y = in0, so the same strata(ctx) shades AND displaces); extra fields
    wire to In1.. ."""
    return CtxRef("in%d" % int(k), "float")

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

  def tex(self, name, uv=None, default=0.0):
    """Sample a bound 2D texture `name` (a real sampler2D uniform) at `uv` (default ctx.uv).
    Returns a vec4 — swizzle .r/.x for a single-channel (R32F) map. Attach a Texture at runtime
    via material.bindParam(name, texture). `default` is the constant used in a compute bake (which
    has no sampler). E.g.:  d = ctx.tex("FlowMap").x ;  albedo = albedo * (0.5 + d).

    `name` may also be a CaptureRef (from self.capture(...)): then it resolves to sampler <target>
    swizzled to the ref's slot — so ctx.tex(c_alb) is a vec3, ctx.tex(c_ao) is a float, etc."""
    u = self.uv if uv is None else _wrap(uv)
    if isinstance(name, CaptureRef):
      return getattr(TexSample(name.target, u, default), name.slot)
    return TexSample(name, u, default)


###############################################################################
# Ptex3d base + materialize
###############################################################################

_FIELD_TYPE = {"albedo": "vec3", "normal": "vec3", "emissive": "vec3",
               "metallic": "float", "roughness": "float", "ao": "float", "opacity": "float"}

_WIDTH = {"float": 1, "vec2": 2, "vec3": 3, "vec4": 4}


class CaptureRef:
  """Handle returned by self.capture(target, expr, slot) — the explicit connection between a baked
  image and the reconstruction (visible in the user's file, not hidden in C++). Carries the physical
  texture name (`target` — one cached file <target>.png, one sampler <target>, one bake MRT), the
  packed component slot (`slot`, e.g. 'xyz'/'w'/'zw'), and the value width. `ctx.tex(ref)` resolves it:
  samples sampler `target`, swizzles `slot` -> a node of the natural width."""
  __slots__ = ("target", "slot", "width")

  def __init__(self, target, slot, width):
    self.target = target
    self.slot   = slot
    self.width  = width

  def __repr__(self):
    return "CaptureRef(%r.%s)" % (self.target, self.slot)


# short aliases -> canonical glTF lobe field names (accepted in surface(**lobes)
# and on the Ptex3d asset). The dict (nested) form is left untouched.
LOBE_ALIASES = {
  "transmission":         "transmission_factor",
  "clearcoat":            "clearcoat_factor",
  "sheen":                "sheen_factor",
  "subsurface":           "subsurface_factor",
  "specular":             "specular_factor",
  "iridescence":          "iridescence_factor",
  "diffuse_transmission": "diffuse_transmission_factor",
  "volume":               "volume_thickness_factor",
}


class Ptex3d:
  """Author surface base. Subclass and, in __init__(self, ctx, **params), build
  expressions and call self.surface(albedo=..., metallic=..., ...)."""

  # baked-channel samplers this material reads via ctx.tex(<sampler>):
  #   sampler uniform  <-  bake capture channel (an image in the asset cache).
  # Subclasses that sample baked textures OVERRIDE this; bind_textures() consumes it.
  SAMPLER_CHANNELS = {}

  @classmethod
  def bind_textures(cls, gmtl, artifacts, ctx):
    """Upload + bind this material's baked-channel samplers (SAMPLER_CHANNELS) onto the
    live PBRMaterial `gmtl`. `artifacts` is the bake-result dict (channel -> image path);
    `ctx` is the gfx context. Returns the Texture objects so the caller can keep them
    alive. lev2 is imported lazily so importing this module for graph tracing /
    validation needs no GPU. (single-channel R32F, no mips.) No-op when SAMPLER_CHANNELS
    is empty, so it's safe to call on any ptex3d material."""
    from orkengine.lev2 import Image, Texture
    keep = []
    for sampler, channel in cls.SAMPLER_CHANNELS.items():
      path = artifacts.get(channel)
      if not path:
        print(f"{cls.__name__}: no baked '{channel}' for sampler {sampler} — skipped", flush=True)
        continue
      img = Image.createFromFile(str(path))
      tex = Texture(sampler)
      ctx.TXI.updateTexture(tex, img, False)
      gmtl.bindParam(sampler, tex)
      keep.append(tex)
      print(f"{cls.__name__}: bound {channel} -> sampler {sampler}", flush=True)
    return keep

  @staticmethod
  def aa_ramp(x, e0, e1):
    """Antialiased crossover mask: 0 below e0, 1 above e1, smoothstep between — but the edge
    is never narrower than one footprint (P.fwidth), so feature masks (height/slope bands)
    stay crisp yet never alias. Geometric AA, no texture. x/e0/e1 may be SurfNodes or floats."""
    c  = (e0 + e1) * 0.5
    hw = P.max((e1 - e0) * 0.5, P.fwidth(x) + 1e-4)
    return P.smoothstep(c - hw, c + hw, x)

  @staticmethod
  def aa_band(x, lo, hi, soft=0.0):
    """Weight of the SINGLE band lo <= x <= hi: ~1 inside, ~0 outside, with AA edges (never
    narrower than one footprint; widened to `soft`). It's the difference of rising steps at lo
    and hi, so two ADJACENT bands sum to 1 across their shared interior. NOTE: an isolated band
    ramps at BOTH edges, so a pair like aa_band(x,0,0.5)+aa_band(x,0.5,1) is 1 in the interior
    but dips at the literal 0/1 ends (both outer edges ramp). For a guaranteed sum==1 over the
    whole range, use aa_bands()."""
    hw = P.max(soft, P.fwidth(x) + 1e-4)
    return P.smoothstep(lo - hw, lo + hw, x) - P.smoothstep(hi - hw, hi + hw, x)

  @staticmethod
  def aa_bands(x, *edges, soft=0.0):
    """Partition x into the intervals between successive `edges` (N edges -> N-1 bands).
    Returns a tuple of per-band masks that ALWAYS SUM TO 1 (partition of unity): one AA rising
    step per INTERIOR edge, differenced so they telescope. The outer edges (edges[0], edges[-1])
    are HARD domain bounds — band 0 is 1 below edges[1], the last band is 1 above edges[-2] — so
    the sum is exactly 1 everywhere, including the ends. `soft` widens the edge transitions.
        lo, hi      = self.aa_bands(h01, 0.0, 0.5, 1.0)        # lo + hi == 1
        a, b, c     = self.aa_bands(h01, 0.0, 0.4, 0.7, 1.0)   # a + b + c == 1 (3 bands)"""
    n = len(edges) - 1
    if n < 1:
      raise ValueError("aa_bands needs >= 2 edges (>= 1 band)")
    if n == 1:
      return (1.0,)                                            # single band = everything
    hw = P.max(soft, P.fwidth(x) + 1e-4)
    S  = [P.smoothstep(edges[j] - hw, edges[j] + hw, x) for j in range(1, n)]  # interior edges only
    bands = [1.0 - S[0]]                                       # band 0: below first interior edge
    for k in range(1, n - 1):
      bands.append(S[k - 1] - S[k])                            # middle bands: between interior edges
    bands.append(S[-1])                                        # last band: above last interior edge
    return tuple(bands)

  @staticmethod
  def mix_layers(albedo, rough, layers):
    """OVER-PAINT a stack over a base (albedo, rough): `layers` = iterable of (select, color,
    layer_rough); each is painted on by its 0..1 `select` mask (e.g. self.aa_ramp(...)), IN ORDER
    — later layers sit on top, order-dependent. Good for RISING masks + overlays (an elevation
    ramp, then a slope-based cliff over everything). Returns (albedo, rough). NOTE: this is NOT a
    weighted sum — over-painting aa_bands PARTITION weights leaks the base at transitions; for
    partitions use mix_bands() instead."""
    for select, color, layer_rough in layers:
      albedo = P.mix(albedo, color,       select)
      rough  = P.mix(rough,  layer_rough, select)
    return albedo, rough

  @staticmethod
  def mix_bands(albedo, rough, layers):
    """WEIGHTED-SUM blend of PARTITION `layers` = (weight, color, layer_rough), whose weights sum
    to 1 (from self.aa_bands). Result = Σ weightᵢ·colorᵢ  (+ base·(1-Σweight) only where the
    partition is incomplete). So at each boundary it's a clean crossfade between the two ADJACENT
    band colors — weights sum to 1, NO base/third colour bleeds in. The transition WIDTH is the
    `soft` you pass to aa_bands (i.e. aa_bands(coord, *edges, soft=width)). Use this with aa_bands;
    use mix_layers for ordered over-paint of rising masks."""
    a_acc = r_acc = wsum = None
    for weight, color, layer_rough in layers:
      a_acc = weight * color       if a_acc is None else a_acc + weight * color
      r_acc = weight * layer_rough if r_acc is None else r_acc + weight * layer_rough
      wsum  = weight               if wsum  is None else wsum + weight
    if a_acc is None:
      return albedo, rough
    base_w = P.saturate(1.0 - wsum)                       # base shows only where Σweight < 1
    return a_acc + albedo * base_w, r_acc + rough * base_w

  @staticmethod
  def _cracked_mud_fields(ctx, *, cell_scale=5.0, base_color=(0.50, 0.33, 0.19),
                          crack=0.05, warp=0.30, bump_scale=0.025,
                          crack_color=(0.05, 0.035, 0.022), aa=0.5):
    """Cracked-mud / dried-playa surface fields: domain-warped voronoi plates with recessed
    cracks, earthy per-plate tone + dirt grain. Returns a dict(albedo, cell, vcoord, crack,
    bumps, plate) — compose `plate`/`cell.fwedge` into displacement, or just use `albedo`.
    Every knob is a ctx.param (runtime uniform, no recompile). Built-in (was a free fn in
    materials/cracked_mud.py); call as self._cracked_mud_fields(ctx, cell_scale=..., ...)."""
    cell_scale = ctx.param("cell_scale", cell_scale)         # cell density
    base   = ctx.param("base_color",  base_color)            # mud tone
    crack  = ctx.param("crack",       crack)                 # crack width, cell units
    warp   = ctx.param("warp",        warp)                  # plate irregularity
    bumps  = ctx.param("bump_scale",  bump_scale)            # relief strength
    crackc = ctx.param("crack_color", crack_color)           # deep-crack tint
    pc     = ctx.P_object * cell_scale
    wv     = P.vec3(P.fbm_aa(pc * 0.6, 2, aa=aa), P.fbm_aa(pc * 0.6 + 17.0, 2, aa=aa), P.fbm_aa(pc * 0.6 + 41.0, 2, aa=aa))
    vcoord = pc + warp * wv
    cell   = P.voronoi(vcoord)
    # ── band-limit the cellular crack + tint (warp + grain already use fbm_aa with the same `aa`) ──
    # fw = SMOOTH coordinate footprint = cell-units crossed per pixel (length(fwidth(vcoord))) × aa.
    # MUST use the COORD footprint, NOT fwidth(cell.fwedge): the wedge derivative SPIKES at the
    # cracks (F1/F2 swap), which faded cracks even when fully resolved (so `aa` "did nothing").
    # crack/fw = crack width in pixels; aa scales how aggressively cracks/tint fade with distance.
    fw     = (P.length(P.fwidth(vcoord)) + 1e-5) * aa
    fade   = P.saturate(crack / fw)                          # 1 = crack resolved; -> 0 = sub-pixel
    plate  = P.mix(1.0, P.smoothstep(0.0, P.max(crack, fw), cell.fwedge), fade)   # 0 in crack, 1 on plate
    warm   = P.mix(rgb(0.70, 0.55, 0.34), rgb(1.05, 0.95, 0.74), cell.cell)
    cfade  = P.saturate(1.0 - fw * 2.0)                      # per-cell tint -> mid as cells go sub-pixel
    tint   = P.mix(rgb(0.875, 0.75, 0.54), warm, cfade)      # rgb(...,0.5) = the cell-tint average
    mud    = base * tint
    grain  = P.fbm_aa(ctx.P_object * cell_scale * 5.0, 4, aa=aa)
    mud    = mud * P.mix(0.82, 1.15, grain)
    lip    = P.mix(1.0, P.smoothstep(0.0, P.max(crack * 3.0, fw), cell.fwedge), fade)  # band-limited
    mud    = mud * P.mix(0.50, 1.0, lip)                     # darken crack lips
    albedo = P.mix(crackc, mud, plate)                       # deep crack -> earthy plate
    return dict(albedo=albedo, cell=cell, vcoord=vcoord, crack=crack, bumps=bumps, plate=plate)

  @staticmethod
  def cracked_mud(ctx, **kw):
    """Cracked-mud / dried-playa ALBEDO — the easy one-call form: returns a usable vec3 colour
    (NOT a dict), so `albedo = self.cracked_mud(ctx)` or `C * self.cracked_mud(ctx)` just works.
    Defaults are sensible; same knobs as _cracked_mud_fields (cell_scale, crack, warp, base_color,
    crack_color, bump_scale). Use _cracked_mud_fields(ctx, ...) when you also need cell/plate/bumps
    (e.g. to drive self.displace for real crack relief)."""
    return Ptex3d._cracked_mud_fields(ctx, **kw)["albedo"]

  def surface(self, *, albedo=None, metallic=None, roughness=None,
              normal=None, emissive=None, ao=None, opacity=None,
              blend="off", depth_test="leq", depth_write=True, cull="front",
              alpha_to_coverage=False, **lobes):
    """LIT PBR surface. The TEXTURED channels (albedo/metallic/roughness/normal/emissive/ao + the
    optional per-pixel `opacity`) take SurfNode expressions. Extra kwargs are glTF PBR LOBES
    (transmission/ior/clearcoat/sheen/subsurface/...) — material-level CONSTANT uniforms.
    Rasterstate (default = opaque, byte-identical to before): `blend` off/alpha/additive,
    `depth_test` leq/less/always/off, `depth_write`, `cull` front/back/off. Set blend!="off" +
    `opacity=` for TRANSPARENT lit PBR (e.g. RELIGHTABLE gaussians: per-splat BRDF + gaussian
    alpha). For an UNLIT emissive surface (standard 3DGS / skybox / FX), use self.unlit(...)."""
    chans = {}
    for name, val in (("albedo", albedo), ("metallic", metallic), ("roughness", roughness),
                      ("normal", normal), ("emissive", emissive), ("ao", ao), ("opacity", opacity)):
      if val is not None:
        chans[name] = _wrap(val)
    self._channels = chans
    self._surface_mode = "lit"
    # A2C (order-independent foliage): fragment alpha -> MSAA coverage; needs an MSAA RTG + per-pixel opacity.
    self._raster = dict(blend=blend, depth_test=depth_test, depth_write=depth_write, cull=cull,
                        alpha_to_coverage=alpha_to_coverage)
    if lobes:
      norm = dict(getattr(self, "_lobes", {}))
      for k, v in lobes.items():
        if isinstance(v, SurfNode):
          raise TypeError(
            "surface(): PBR lobe %r must be a constant (number / vec / color), not a "
            "per-pixel expression — lobes are material-level uniforms, not textured. "
            "The textured channels are albedo/metallic/roughness/normal/emissive/ao." % k)
        if not isinstance(v, dict):
          k = LOBE_ALIASES.get(k, k)
        norm[k] = v
      self._lobes = norm

  def capture(self, target, expr, slot=None):
    """Declare a NAMED capture of a specific DSL output for the proctex texture-bake. Captures that
    share a `target` PACK into one RGBA texture (one cached file `<target>.png`, one sampler `<target>`,
    one bake MRT). `slot` ('x'/'xy'/'xyz'/'w'/'zw'/...) places `expr` in the target's components; omit
    it to auto-assign the next free components by the expr's width. Returns a CaptureRef the
    reconstruction (surface_stored) samples via ctx.tex(ref). See the design report §5.1."""
    e = _wrap(expr)
    w = _WIDTH.get(e._type)
    if w is None:
      raise TypeError("capture(%r): expr type %r not bakeable (need float/vec2/vec3/vec4)" % (target, e._type))
    used = self.__dict__.setdefault("_capture_used", {})   # target -> list of used component indices
    cur  = used.setdefault(target, [])
    if slot is None:
      free = [i for i in range(4) if i not in cur]
      if len(free) < w:
        raise ValueError("capture target %r overflow: needs %d comps, %d free" % (target, w, len(free)))
      comps = free[:w]
      slot  = "".join("xyzw"[i] for i in comps)
    else:
      if any(c not in "xyzw" for c in slot):
        raise ValueError("capture(%r): bad slot %r (use x/y/z/w)" % (target, slot))
      comps = ["xyzw".index(c) for c in slot]
      if len(comps) != w:
        raise ValueError("capture(%r): slot %r has %d comps but expr is width %d" % (target, slot, len(comps), w))
      for i in comps:
        if i in cur:
          raise ValueError("capture(%r): component %r already packed" % (target, "xyzw"[i]))
    cur.extend(comps)
    ref = CaptureRef(target, slot, w)
    self.__dict__.setdefault("_captures", []).append((target, slot, e, ref))
    return ref

  def surface_stored(self, *, albedo=None, metallic=None, roughness=None,
                     normal=None, emissive=None, ao=None, opacity=None):
    """The STORED reconstruction surface — sampled from the captures (ctx.tex(ref)) instead of the
    live proc. A FULL DSL expression context: combine sampled captures with cheap live detail
    (the hybrid path). Compiled as ptex_surface when the material materializes in mode='stored';
    ignored in mode='proc' (which compiles surface())."""
    chans = {}
    for nm, val in (("albedo", albedo), ("metallic", metallic), ("roughness", roughness),
                    ("normal", normal), ("emissive", emissive), ("ao", ao), ("opacity", opacity)):
      if val is not None:
        chans[nm] = _wrap(val)
    self._stored_channels = chans

  def unlit(self, color, opacity=1.0, *, blend="alpha", depth_test="leq", depth_write=False, cull="off",
            alpha_to_coverage=False):
    """UNLIT emissive surface: out_clr = (color, opacity), NO scene lighting. For standard 3DGS
    splats (radiance baked into the color), skyboxes, FX overlays, emissive UI. `color` (vec3) and
    `opacity` (float) are SurfNode expressions or constants. Rasterstate defaults to TRANSPARENT
    (alpha blend, depth-test-on, depth-WRITE-off, no cull) — unlit's common use; override via
    blend/depth_test/depth_write/cull. (LIT PBR + alpha — e.g. RELIGHTABLE gaussians, which carry
    a per-splat BRDF — uses surface(..., blend="alpha", opacity=...) instead.)"""
    chans = {"emissive": _wrap(color)}
    if opacity is not None:
      chans["opacity"] = _wrap(opacity)
    self._channels = chans
    self._surface_mode = "unlit"
    self._raster = dict(blend=blend, depth_test=depth_test, depth_write=depth_write, cull=cull,
                        alpha_to_coverage=alpha_to_coverage)

  def fragment_storage(self, storage_decl, *, inherits=(), append=""):
    """Declare a FRAGMENT-side storage_interface the surface reads + raw GLSL appended AFTER the DSL
    surface body. The storage is declared top-level and inherited by lib_ptex_surface; the appended
    GLSL runs last in ptex_surface(), so it can index that storage + FS builtins (e.g. gl_PrimitiveID)
    and override o.albedo/o.emissive/etc. Used by the hypermesh TopoView material (per-triangle
    face-id buffer). Read back in _build_ptex3d -> generate_surface_fxv2(surf_storage/...)."""
    self._surf_storage = storage_decl
    self._surf_storage_inherits = tuple(inherits)
    self._surf_body_append = append

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


def _bake_literal(v, gtype):
  """Format a ctx.param default as a GLSL literal for a one-shot bake (no UBO)."""
  if gtype == "float":
    return repr(float(v))
  n = {"vec2": 2, "vec3": 3, "vec4": 4}.get(gtype)
  if n is not None:
    if isinstance(v, (tuple, list)):
      comps = [float(c) for c in v][:n]
    else:
      comps = [float(getattr(v, a)) for a in ("x", "y", "z", "w")[:n]]
    return "%s(%s)" % (gtype, ", ".join(repr(c) for c in comps))
  raise TypeError("cannot bake ctx.param default %r as %s literal" % (v, gtype))


class _Emitter:
  def __init__(self, subst=None, bake_params=False):
    self.lines = []
    self.cache = {}        # node.key() -> ssa varname
    self.n = 0
    self.libsrcs = []      # ordered-unique GLSL helper sources
    self.inherits = set()
    self.imports = set()
    self.params = {}       # insertion-ordered: pname -> (gtype, default)
    self.samplers = []     # insertion-ordered-unique sampler2D names (ctx.tex) -> emitted as a sampler_set
    self.atoms = set()     # CtxRef glsl names referenced (for the bake portability gate)
    # atom-name remap (e.g. {"opos": "coord"}) — lets the height field be emitted
    # as a function of a marched/offset coordinate rather than the fixed varying.
    self.subst = subst or {}
    # bake mode: emit ctx.param() as its DEFAULT LITERAL (a one-shot compute bake has
    # no UBO; the cook hash captures the value via the generated shader text).
    self.bake_params = bake_params

  def expr(self, node):
    if isinstance(node, Const):
      return node._glsl
    if isinstance(node, CtxRef):
      self.atoms.add(node._glsl)
      return self.subst.get(node._glsl, node._glsl)
    if isinstance(node, Param):
      if self.bake_params:
        return _bake_literal(node._default, node._type)
      if node._pname not in self.params:
        self.params[node._pname] = (node._type, node._default)
      # shadlang references uniform_block members by BARE name (like ModColor),
      # not block.member — read the vec4-padded slot swizzled to logical type.
      return "%s%s" % (node._pname, _PARAM_SWIZ[node._type])
    if isinstance(node, Swizzle):
      return "%s.%s" % (self.expr(node._src), node._comp)
    if isinstance(node, TexSample):
      k = node.key()
      hit = self.cache.get(k)
      if hit is not None:
        return hit
      var = "t%d" % self.n
      self.n += 1
      if self.bake_params:
        # a compute bake has no runtime sampler -> constant fallback
        self.lines.append("vec4 %s = vec4(%s);" % (var, _bake_literal(node._default, "float")))
      else:
        if node._sname not in self.samplers:
          self.samplers.append(node._sname)
        uvexpr = self.expr(node._uv)
        self.lines.append("vec4 %s = texture(%s, %s);" % (var, node._sname, uvexpr))
      self.cache[k] = var
      return var
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
  return body, libsrcs, sorted(inherits), sorted(imports), params, list(em.samplers)


def emit_captures(captures):
  """captures: [(target, slot, node, ref), ...] (captures sharing a `target` PACK into one RGBA) ->
     (capture_body, libsrcs, lib_inherits, imports, param_specs, samplers, targets).
  capture_body assembles each target's vec4 from its packed exprs (default 0.0 in unfilled
  components) as `c.<target> = vec4(c0,c1,c2,c3);`, run inside the generated ptex_capture() (same
  signature/env/helpers as ptex_surface, so the capture exprs evaluate identically to the proc)."""
  em = _Emitter()
  targets  = []                 # ordered-unique target names (one MRT location each)
  bytarget = {}                 # target -> [(slot, gtype, glsl_var), ...]
  for (target, slot, node, ref) in captures:
    if target not in bytarget:
      bytarget[target] = []
      targets.append(target)
    glsl = em.expr(node)        # SSA (CSE shared across all captures)
    bytarget[target].append((slot, node._type, glsl))
  assigns = []
  for target in targets:
    comps = ["0.0", "0.0", "0.0", "0.0"]
    for (slot, gtype, glsl) in bytarget[target]:
      w = _WIDTH[gtype]
      for k, ch in enumerate(slot):
        idx = "xyzw".index(ch)
        comps[idx] = glsl if w == 1 else "%s.%s" % (glsl, "xyzw"[k])
    assigns.append("c.%s = vec4(%s);" % (target, ", ".join(comps)))
  body = "\n".join(em.lines + assigns)
  libsrcs, inherits, imports, params = _emitter_deps(em)
  return body, libsrcs, sorted(inherits), sorted(imports), params, list(em.samplers), targets


def emit_height(node, coord="coord"):
  """Emit a displacement-height expression as the body of
  `float ptex_height(vec3 coord, vec3 wpos, vec3 wnrm, vec3 onrm, vec2 uv, vec4 cd,
  vec3 eye)` — only the object-position atom (`opos`) is rebound to `coord`, so the
  function is re-evaluable at offset/marched coordinates (analytic bump now, parallax
  later). wpos/wnrm/uv/cd/eye are passed in CONSTANT (the fragment's values), so the
  height may use any surface atom (ctx.P/ctx.N/ctx.uv/ctx.tex as MASKS — they don't
  vary across the finite-diff) — same atom set as the surface body. Returns
  (lines, final_expr, libsrcs_list, lib_inherits, extra_imports, param_specs)."""
  em = _Emitter(subst={"opos": coord})
  final = _coerce(em.expr(node), node._type, "float")
  libsrcs, inherits, imports, params = _emitter_deps(em)
  return em.lines, final, libsrcs, sorted(inherits), sorted(imports), params


def emit_compute_field(node):
  """Emit a SurfNode as a SCALAR compute-field body for a terrain bake (the unified
  substrate: self.hfbake / hfmask / hfdisplacement). The world-position atoms
  (opos/wpos), object normal (onrm) and `footprint` are provided by the compute SHELL
  (compute_template.build_field_shader), so NO atom subst is needed — the body just
  references them. ctx.param() is BAKED to its default literal (a one-shot bake has no
  UBO; the cook hash captures it via the generated text). View-dependent atoms
  (NV/eye/Cd) are rejected here with a legible error. Returns
  (lines, final_expr, libsrcs_list, lib_inherits, extra_imports, param_specs, input_indices)
  where input_indices are the ctx.input(k) referenced (caller validates vs connected count)."""
  em = _Emitter(bake_params=True)
  final = _coerce(em.expr(node), node._type, "float")
  # portable-core gate: view-dependent atoms have no bake form. Reject with a clear
  # message at trace time (Phase 3 will add call-site provenance to the diagnostic).
  _forbidden = {"vnrm": "ctx.NV (view-space normal)", "eye": "ctx.eye (camera position)",
                "cd": "ctx.Cd (per-vertex color)"}
  bad = [msg for atom, msg in _forbidden.items() if atom in em.atoms]
  if bad:
    raise TypeError(
        "bake expression references view-dependent atom(s) with no bake form: "
        + ", ".join(bad) + ". A terrain bake (hfbake/hfmask/hfdisplacement) is a "
        "function of position only — use ctx.P_object / ctx.P / ctx.N_object / "
        "ctx.footprint / ctx.input(k) (and noise/fbm), not view/camera atoms.")
  # which image inputs (ctx.input(k) -> "in{k}") the body references — the caller
  # (expr_field) validates these against the number of connected inputs.
  in_idx = sorted(int(a[2:]) for a in em.atoms if a.startswith("in") and a[2:].isdigit())
  libsrcs, inherits, imports, params = _emitter_deps(em)
  return em.lines, final, libsrcs, sorted(inherits), sorted(imports), params, in_idx


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


def _build_ptex3d(dsl_class, name_hint=None, vertex_source=None, mode="proc", **params):
  # impostor (geometry-LOD bake source, whole-surface fixed MRT) and the terrain texture-bake both
  # emit a capture technique. Pop BOTH unconditionally so neither leaks into the surface DSL.
  _wants_impostor = bool(params.pop("impostor", False))
  _wants_capture  = bool(params.pop("capture", False))
  inst = dsl_class(SurfaceCtx(), **params)
  caps         = getattr(inst, "_captures", None)
  stored_chans = getattr(inst, "_stored_channels", None)
  capture_kwargs = {}
  # EXPLICIT-CAPTURE STORED mode: render surface_stored(); bake the named/packed captures (§5).
  if mode == "stored" and caps:
    if not stored_chans:
      raise RuntimeError("%s: mode='stored' declares captures but no surface_stored()" % dsl_class.__name__)
    body, libsrcs, inherits, imports, pspecs, samplers = emit_surface(stored_chans)
    cap_body, c_libs, c_inh, c_imp, c_params, c_samps, targets = emit_captures(caps)
    libsrcs  = _union_ordered(libsrcs, c_libs)          # union helpers (capture exprs need their own)
    inherits = sorted(set(inherits) | set(c_inh))
    imports  = sorted(set(imports) | set(c_imp))
    pspecs   = _merge_param_specs(pspecs, c_params)
    # SPLIT the sampler sets: the FORWARD declares only surface_stored's samplers (`samplers`); the capture
    # exprs' samplers go to a capture-only set (lib_ptex_capture). Metal caps 16 samplers/stage, so the
    # forward must NOT carry the channel samplers (FlowMetrics/...) it never uses (else MSL out-of-bounds).
    cap_only_samplers = [s for s in c_samps if s not in samplers]
    wants_capture  = True
    capture_kwargs = dict(capture_body=cap_body, capture_targets=tuple(targets),
                          capture_samplers=tuple(cap_only_samplers))
  else:
    # mode='proc' (live) OR impostor (whole-surface fixed-MRT capture): render surface().
    if not getattr(inst, "_channels", None):
      raise RuntimeError("%s built no surface() channels" % dsl_class.__name__)
    body, libsrcs, inherits, imports, pspecs, samplers = emit_surface(inst._channels)
    wants_capture = _wants_impostor or _wants_capture

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
  # FWD_SSBO_CUSTOM delegation: a vertex_source (e.g. the terrain GPU-chunk provider) supplies the
  # SSBO-pull vertex side (layout/lib/vs_body/compute) which the codegen splices in alongside the
  # surface fragment. The surface DSL stays vertex-agnostic; the consumer owns the geometry contract.
  vskw = {}
  if vertex_source is not None:
    vskw = (vertex_source.as_material_kwargs()
            if hasattr(vertex_source, "as_material_kwargs") else dict(vertex_source))
    # a vertex_source may contribute bindable params (e.g. a VertexDisplace's WindDir/Time) — merge them
    # into ublk_ptex_params so they get pipeline block-state + bind by name (its VS inherits the block).
    # A provider-token default (Time = RCFD_TIME) survives as a crcstring shader_param: the engine binds
    # its per-frame value via fx_pipeline's named-param providers (no per-frame host code).
    if hasattr(vertex_source, "displace_params"):
      dp = vertex_source.displace_params()
      if dp:
        pspecs = _merge_param_specs(pspecs, dp)
  # a material may declare a FRAGMENT-side storage block it reads + a raw surface-body append (e.g.
  # the hypermesh TopoView material reads a per-triangle face-id buffer via gl_PrimitiveID). Declared
  # on the instance by self.fragment_storage(...) — see Ptex3d.fragment_storage.
  matkw = {}
  if getattr(inst, "_surf_storage", None):
    matkw["surf_storage"] = inst._surf_storage
    matkw["surf_storage_inherits"] = getattr(inst, "_surf_storage_inherits", ())
  if getattr(inst, "_surf_body_append", None):
    matkw["surf_body_append"] = inst._surf_body_append
  # Owner policy (2026-06-12): generated shaders live in the per-family staging
  # cache (<staging>/dslshadercache/ptex3d), referenced by the relocatable
  # <staging> token — never persisted in the source tree (B.5c .shaders/ RETIRED).
  # unlit/blend: the sink (surface()/unlit()) records the FS mode + the rasterstate; thread them
  # to the codegen. Defaults (lit / opaque) keep every existing material byte-identical.
  _mode   = getattr(inst, "_surface_mode", "lit")
  _raster = getattr(inst, "_raster", {})
  path = materialize_surface_fxv2(body, libblock=libblock, lib_inherits=inherits,
                                  extra_imports=imports, params=pspecs, samplers=samplers,
                                  name_hint=name_hint or dsl_class.__name__.lower(),
                                  surface_mode=_mode, wants_capture=wants_capture, **_raster,
                                  **height_kwargs, **vskw, **matkw, **capture_kwargs)
  lobes = dict(getattr(inst, "_lobes", None) or {})   # class-declared PBR lobes
  return path, pspecs, lobes, capture_kwargs.get("capture_targets", ())


def materialize_ptex3d(dsl_class, *, name_hint=None, vertex_source=None, **params):
  """Instantiate a Ptex3d subclass, emit its surface, and bake the .fxv2.
  Returns the cached .fxv2 path."""
  path = _build_ptex3d(dsl_class, name_hint=name_hint, vertex_source=vertex_source, **params)[0]
  return path


def materialize_ptex3d_full(dsl_class, *, name_hint=None, vertex_source=None, **params):
  """Like materialize_ptex3d but also returns the bindable-param specs
  [(name, gtype, default), ...] (the asset layer pre-binds these defaults and
  round-trips them in PbrMaterialGenData.shader_params) and the class-declared PBR
  lobes {field: value} from surface(**lobes). Returns (path, specs, lobes).

  vertex_source: optional FWD_SSBO_CUSTOM vertex-side provider (see ptex3d codegen) — the
  consumer (e.g. terrain GPU-chunk renderer) delegates the SSBO-pull vertex layout + compute."""
  return _build_ptex3d(dsl_class, name_hint=name_hint, vertex_source=vertex_source, **params)
