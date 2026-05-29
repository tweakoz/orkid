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

from ork.hypergraph.ptex3d.fxv2_template import materialize_surface_fxv2

_VEC = {1: "float", 2: "vec2", 3: "vec3", 4: "vec4"}
_SWIZ = set("xyzw") | set("rgba")


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
  if isinstance(x, (tuple, list)):
    n = len(x)
    if n in (2, 3, 4):
      return Const("%s(%s)" % (_VEC[n], ", ".join(_fmt_float(v) for v in x)), _VEC[n])
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
vec4 _ptex_voronoi(vec3 x) {              // x=F1, y=F2, z=cell-hash, w=cell-hash2
  vec3 ip = floor(x), fp = fract(x);
  float f1 = 1e9, f2 = 1e9, idA = 0.0, idB = 0.0;
  for (int k=-1;k<=1;k++) for (int j=-1;j<=1;j++) for (int i=-1;i<=1;i++) {
    vec3 g = vec3(float(i), float(j), float(k));
    vec3 r = g + _ptex_vhash3(ip+g) - fp;
    float d = dot(r, r);
    if (d < f1)      { f2 = f1; f1 = d; idA = _ptex_shash3(ip+g); idB = _ptex_shash3(ip+g+vec3(31.7)); }
    else if (d < f2) { f2 = d; }
  }
  return vec4(sqrt(f1), sqrt(f2), idA, idB);
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
    node = Op("_ptex_voronoi({0})", [_wrap(p)], "vec4", libsrc=_VORONOI_SRC)
    return Bundle(node, {"f1": "x", "f2": "y", "cell": "z", "cell2": "w"})


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


class _Emitter:
  def __init__(self):
    self.lines = []
    self.cache = {}        # node.key() -> ssa varname
    self.n = 0
    self.libsrcs = []      # ordered-unique GLSL helper sources
    self.inherits = set()
    self.imports = set()

  def expr(self, node):
    if isinstance(node, Const):
      return node._glsl
    if isinstance(node, CtxRef):
      return node._glsl
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


def emit_surface(channels):
  """channels: {field -> SurfNode} -> (surface_body, libblock, lib_inherits, extra_imports)."""
  em = _Emitter()
  assigns = []
  for field, node in channels.items():
    e = _coerce(em.expr(node), node._type, _FIELD_TYPE[field])
    assigns.append("o.%s = %s;" % (field, e))
  body = "\n".join(em.lines + assigns)
  libblock = "\n".join(s.strip() for s in em.libsrcs)
  # imports for the inherited libblocks the body needs
  imports = set(em.imports)
  if "lib_mmnoise" in em.inherits:
    imports.add("orkshader://misctools.i2")
  return body, libblock, sorted(em.inherits), sorted(imports)


def materialize_ptex3d(dsl_class, *, name_hint=None, **params):
  """Instantiate a Ptex3d subclass, emit its surface, and bake the .fxv2."""
  inst = dsl_class(SurfaceCtx(), **params)
  if not getattr(inst, "_channels", None):
    raise RuntimeError("%s built no surface() channels" % dsl_class.__name__)
  body, libblock, inherits, imports = emit_surface(inst._channels)
  return materialize_surface_fxv2(body, libblock=libblock, lib_inherits=inherits,
                                  extra_imports=imports,
                                  name_hint=name_hint or dsl_class.__name__.lower())
