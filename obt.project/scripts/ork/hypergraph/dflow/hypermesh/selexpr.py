###############################################################################
# Hypermesh selection DSL — SelExpr (a per-element predicate expression that traces to GLSL) + MaskOp
# (the two-triple bit transform a Select writes). One atom namespace `S.*`; the Select op fixes the
# DOMAIN (POINT/POLY/LINE) and each atom compiles for it (and errors on a bad match, e.g. points have
# no area). Predicates compose with & | ^ ~ (boolean over 0/1 weights) + arithmetic/comparison.
#
#   expr = sel_normal_dir(n=vec3(0,1,0), t=0.55, soft=0.05) & (S.area > 0.02)
#   m.select(sph, expr, domain=POLY, op=replace(group(0)))
###############################################################################
import math

POINT, POLY, LINE = "point", "poly", "line"

# atom -> (valid domains, per-domain GLSL local). The Select shell pre-computes these locals.
_ATOMS = {
  "P":   ({POINT, POLY, LINE}, {POINT: "vP",    POLY: "fP",    LINE: "lP"}),       # vec3 position
  "N":   ({POINT, POLY},        {POINT: "vN",    POLY: "fN"}),                      # vec3 normal
  "uv":  ({POLY},  {POLY: "fUV"}),                                                  # vec3 per-face UV centroid (.xy=uv); extrude field exprs
  "id":  ({POINT, POLY, LINE}, {POINT: "float(_eid)", POLY: "float(_eid)", LINE: "float(_eid)"}),
  "area":   ({POLY},  {POLY: "fArea"}),                                            # float (POLY only)
  "length": ({LINE},  {LINE: "lLen"}),                                             # float (LINE only)
  "angle":  ({LINE},  {LINE: "lDihedral"}),  # float RADIANS — dihedral between the edge's two faces; boundary=0
  # multi-segment EXTRUDE field exprs only: S.t = ring parameter r/segments (0..1], S.seg = ring index r.
  # In a single-segment extrude (or any other field) both degrade to 1.0 (the cap = the end of the lift).
  "t":   ({POLY},  {POLY: "_segt"}),                                               # float 0..1 ring parameter
  "seg": ({POLY},  {POLY: "_segi"}),                                               # float ring index
}


class SelExpr:
  """A selection expression NODE in an operation tree. `gt` is "float" or "vec3". A node is either a LEAF
  (`_children` empty; `_emit(domain) -> glsl str`, e.g. an atom local or a baked constant) or an INTERNAL
  op (`_emit(child_refs) -> glsl str`, one GLSL op over its already-emitted child temps).

  emit_block(domain) lowers the whole tree to FLAT SSA — a list of `T _seN = <one op>;` statements plus the
  final result var — so each emitted statement is at most one operation deep. This is deliberate: shadlang's
  PEG parser recurses ~per nesting level (with backtracking), so a single deeply-nested expression
  (e.g. a 10-wide `|`-chain -> `max(max(max(...)))`) makes shader compilation blow up. Flattening keeps the
  parser shallow for ANY predicate. glsl(domain) still returns the nested form (debug/repr only)."""
  def __init__(self, gt, emit, children=(), meta=None):
    self._gt       = gt
    self._emit     = emit             # leaf: fn(domain)->str ; internal: fn(child_refs)->str
    self._children = tuple(children)
    # E2.5 ExprIR CAPTURE tag — the CANONICAL symbolic op/leaf spelling (Q1: tag at construction,
    # never reverse-map the GLSL `_emit`). exprir_selexpr.capture() walks _meta + _children to
    # build a hypermesh.selexpr ExprIR tree; the GLSL path (_emit/emit_block) is untouched so a
    # tagged node emits BYTE-IDENTICAL shader text. Forms (a small tuple):
    #   ("op",   name)                 an internal op over _children (Call name)
    #   ("atom", name)                 an atom leaf (ParamRef kind="atom")
    #   ("const", value)               a baked scalar constant (Const)
    #   ("vconst", (x, y, z))          a baked vec3 constant (Call "vec3" of 3 Consts)
    #   None                           UNTAGGED -> capture() fails loud (no silent hole)
    self._meta     = meta

  def _glty(self): return "vec3" if self._gt == "vec3" else "float"

  def emit_block(self, domain, prefix="_se"):
    """lower to flat SSA. returns (stmts:list[str], result_var:str). common subtrees (same object) dedup.
    `prefix` names the temps — pass a DISTINCT prefix when several blocks are spliced into one shader body
    (e.g. extrude's dist/inset/dir fields) so their temps don't redeclare each other."""
    stmts = []; memo = {}; ctr = [0]
    def visit(n):
      key = id(n)
      if key in memo: return memo[key]
      ref = n._emit([visit(c) for c in n._children]) if n._children else n._emit(domain)
      var = "%s%d" % (prefix, ctr[0]); ctr[0] += 1
      stmts.append("  %s %s = %s;" % (n._glty(), var, ref))
      memo[key] = var
      return var
    res = visit(self)
    return stmts, res

  def glsl(self, domain):  # nested form (debug/repr); the hot path uses emit_block (flat SSA)
    if self._children:
      return self._emit([c.glsl(domain) for c in self._children])
    return self._emit(domain)

  # ---- boolean composition (0/1 weights) ----  (IR tags mirror the GLSL op computed, Q1)
  def __and__(self, o):  return _node("float", lambda r: "min(%s, %s)" % (r[0], r[1]), (self, _coerce(o)), meta=("op", "min"))
  def __or__(self, o):   return _node("float", lambda r: "max(%s, %s)" % (r[0], r[1]), (self, _coerce(o)), meta=("op", "max"))
  def __xor__(self, o):  return _node("float", lambda r: "abs(%s - %s)" % (r[0], r[1]), (self, _coerce(o)), meta=("op", "xor01"))  # 0/1 xor
  def __invert__(self):  return _node("float", lambda r: "(1.0 - (%s))" % r[0], (self,), meta=("op", "not01"))
  def __sub__(self, o):  return _bin(self._gt, self, o, "-", "sub")    # set-subtract for weights, vec/float math
  def __rsub__(self, o): return _bin(self._gt, _coerce(o), self, "-", "sub")
  def __add__(self, o):  return _bin(self._gt, self, o, "+", "add")
  __radd__ = __add__                                           # a + b == b + a (const on the left)
  def __mul__(self, o):                                        # vec3 if EITHER side is vec3 (scalar*vec broadcast)
    oc = _coerce(o)
    return _node("vec3" if "vec3" in (self._gt, oc._gt) else "float", lambda r: "(%s * %s)" % (r[0], r[1]), (self, oc), meta=("op", "mul"))
  __rmul__ = __mul__
  def __truediv__(self, o):  return _bin(self._gt, self, o, "/", "div")
  def __rtruediv__(self, o): return _bin(self._gt, _coerce(o), self, "/", "div")
  def __neg__(self):         return _node(self._gt, lambda r: "(-(%s))" % r[0], (self,), meta=("op", "neg"))
  def __pow__(self, o):      return _node(self._gt, lambda r: "pow(%s, %s)" % (r[0], r[1]), (self, _coerce(o)), meta=("op", "pow"))  # x ** y
  def __mod__(self, o):      return _node(self._gt, lambda r: "mod(%s, %s)" % (r[0], r[1]), (self, _coerce(o)), meta=("op", "mod"))  # x % y (GLSL mod; e.g. S.seg % 2 = parity)
  # ---- comparisons -> 0/1 float ---- (step(edge, x) == (x >= edge)); IR captures the literal step op.
  def __gt__(self, o): return _node("float", lambda r: "step(%s, %s)" % (r[0], r[1]), (_coerce(o), self), meta=("op", "step"))  # self > o
  def __lt__(self, o): return _node("float", lambda r: "step(%s, %s)" % (r[0], r[1]), (self, _coerce(o)), meta=("op", "step"))  # self < o
  def __ge__(self, o): return self.__gt__(o)
  def __le__(self, o): return self.__lt__(o)
  # ---- vec ops ----
  def dot(self, o):    return _node("float", lambda r: "dot(%s, %s)" % (r[0], r[1]), (self, _coerce(o)), meta=("op", "dot"))
  def length(self):    return _node("float", lambda r: "length(%s)" % r[0], (self,), meta=("op", "length_of"))
  @property
  def x(self): return _node("float", lambda r: "(%s).x" % r[0], (self,), meta=("op", "swz_x"))
  @property
  def y(self): return _node("float", lambda r: "(%s).y" % r[0], (self,), meta=("op", "swz_y"))
  @property
  def z(self): return _node("float", lambda r: "(%s).z" % r[0], (self,), meta=("op", "swz_z"))


def _f(emit, meta=None):  return SelExpr("float", emit, meta=meta)          # LEAF float (emit: domain->str)
def _v(emit, meta=None):  return SelExpr("vec3", emit, meta=meta)           # LEAF vec3  (emit: domain->str)
def _node(gt, emit, children, meta=None): return SelExpr(gt, emit, children, meta=meta)   # INTERNAL op

def _coerce(x):
  if isinstance(x, SelExpr): return x
  if hasattr(x, "x") and hasattr(x, "y") and hasattr(x, "z"):     # a vec3 -> baked constant leaf
    return _v(lambda d, X=x: "vec3(%r, %r, %r)" % (float(X.x), float(X.y), float(X.z)),
              meta=("vconst", (float(x.x), float(x.y), float(x.z))))
  return _f(lambda d, X=x: "%r" % float(X), meta=("const", float(x)))       # a scalar -> baked constant leaf

def _bin(gt, a, b, op, tag):
  return _node(gt, lambda r: "(%s %s %s)" % (r[0], op, r[1]), (_coerce(a), _coerce(b)), meta=("op", tag))


# the Hypermesh asset currently being traced (set by Hypermesh.__init__). S.time attaches a single shared
# time param to it; the base class then drives that param each frame from updinfo.absolutetime.
_build_ctx = {"asset": None}
def _bind_build_asset(a):  _build_ctx["asset"] = a


class _AtomNS:
  """`S` — the atom namespace. S.P / S.N / S.id / S.area / S.length / S.tag(bit)."""
  def _atom(self, name):
    valid, locals_ = _ATOMS[name]
    gt = "vec3" if name in ("P", "N", "uv") else "float"
    def emit(d):
      if d not in valid:
        raise TypeError("selection atom S.%s is not valid for domain '%s' (valid: %s)"
                        % (name, d, ", ".join(sorted(valid))))
      return locals_[d]
    return SelExpr(gt, emit, meta=("atom", name))
  P      = property(lambda s: s._atom("P"))
  N      = property(lambda s: s._atom("N"))
  uv     = property(lambda s: s._atom("uv"))       # per-face UV centroid (vec3, .xy=uv) — extrude field exprs
  id     = property(lambda s: s._atom("id"))
  area   = property(lambda s: s._atom("area"))
  length = property(lambda s: s._atom("length"))
  angle  = property(lambda s: s._atom("angle"))      # LINE: dihedral in radians (boundary edge = 0)
  t      = property(lambda s: s._atom("t"))          # multi-segment extrude: ring parameter r/segments (0..1]
  seg    = property(lambda s: s._atom("seg"))        # multi-segment extrude: ring index r (0..segments)
  i      = property(lambda s: s._atom("seg"))        # alias of S.seg (the iteration/ring index)
  @property
  def time(self):
    """absolute time in SECONDS as a runtime value — a single shared param auto-created per asset; the
    consuming extrude marks its slot (time_slot) and the C++ CLOCK feeds it every live frame (B.4: zero
    per-frame Python; serializes with the asset; .y of the slot carries dt). BAKE = the deterministic t=0
    snapshot. Scope = same as param(): extrude_faces field expressions. e.g. S.sin(S.time*2.0)."""
    a = _build_ctx["asset"]
    if a is None:
      raise RuntimeError("S.time is only valid while a Hypermesh asset is being built (inside __init__)")
    tp = getattr(a, "_time_param", None)
    if tp is None:
      tp = _ParamLeaf("__time", 0.0)                 # ONE shared leaf per asset -> consistent EXPRP slot per op
      a._time_param = tp
    return tp
  def tag(self, bit):                                            # read this element's existing tag bit
    return _f(lambda d: "float((_tags >> %du) & 1u)" % (int(bit) & 31), meta=("tagbit", int(bit) & 31))
  @property
  def gid(self):
    """This face's persistent gid (the LOCKED top 12 tag bits [20:32) — material/semantic class,
    0..4095; 0 until assign_gid sets it). Read-only here; the ONLY write verb is assign_gid."""
    return _f(lambda d: "float((_tags >> 20u) & 0xFFFu)", meta=("atom", "gid"))
  def ftag(self, bit):                                           # LINE only: 1 if EITHER adjacent FACE has group `bit`
    # lets an edge predicate target the boundary of a FACE group (e.g. the inset/extrude partitions the exhaust
    # carries) — `~(S.ftag(2) | S.ftag(3))` = "edges not touching the exhaust faces". Reads the OR of the edge's
    # two faces' __tags (boundary edge: just f0). Requires the upstream mesh to carry FACE __tags (it does after
    # any face Select / topology op that tags partitions).
    def emit(d):
      if d != LINE:
        raise TypeError("selection atom S.ftag(bit) is LINE-only (it reads ADJACENT FACE group tags)")
      return "float((_ftags >> %du) & 1u)" % (int(bit) & 31)
    return _f(emit, meta=("ftagbit", int(bit) & 31))
  # ---- math (ergonomic S.* aliases of the free sl_* functions; e.g. S.pow(S.t, 2.0), S.sin(...)). NOTE:
  #      S.length is the LINE edge-length ATOM, so `length` is intentionally NOT a math method here. ----
  def sin(self, x):                return sl_sin(x)
  def cos(self, x):                return sl_cos(x)
  def fract(self, x):              return sl_fract(x)
  def sqrt(self, x):               return sl_sqrt(x)
  def abs(self, x):                return sl_abs(x)
  def pow(self, x, y):             return sl_pow(x, y)
  def min(self, a, b):             return sl_min(a, b)
  def max(self, a, b):             return sl_max(a, b)
  def clamp(self, x, a, b):        return sl_clamp(x, a, b)
  def step(self, e, x):            return sl_step(e, x)
  def smoothstep(self, e0, e1, x): return sl_smoothstep(e0, e1, x)
  def mix(self, a, b, t):          return sl_mix(a, b, t)
  def select(self, cond, a, b):    return sl_select(cond, a, b)   # cond!=0 ? a : b  (e.g. S.select(S.seg % 2, A, B))

S = _AtomNS()

# ---- helpers (band-limited / clamps) — INTERNAL nodes (operands become child temps; stay parser-shallow) ----
def sl_smoothstep(e0, e1, x): return _node("float", lambda r: "smoothstep(%s, %s, %s)" % (r[0], r[1], r[2]), (_coerce(e0), _coerce(e1), _coerce(x)), meta=("op", "smoothstep"))
def sl_step(e, x):            return _node("float", lambda r: "step(%s, %s)" % (r[0], r[1]), (_coerce(e), _coerce(x)), meta=("op", "step"))
def sl_clamp(x, a, b):        return _node("float", lambda r: "clamp(%s, %s, %s)" % (r[0], r[1], r[2]), (_coerce(x), _coerce(a), _coerce(b)), meta=("op", "clamp"))
def sl_min(a, b):             return _node("float", lambda r: "min(%s, %s)" % (r[0], r[1]), (_coerce(a), _coerce(b)), meta=("op", "min"))
def sl_max(a, b):             return _node("float", lambda r: "max(%s, %s)" % (r[0], r[1]), (_coerce(a), _coerce(b)), meta=("op", "max"))
def sl_sin(x):  x = _coerce(x); return _node(x._gt, lambda r: "sin(%s)" % r[0], (x,), meta=("op", "sin"))   # component-wise for vec3
def sl_cos(x):  x = _coerce(x); return _node(x._gt, lambda r: "cos(%s)" % r[0], (x,), meta=("op", "cos"))
def sl_fract(x):x = _coerce(x); return _node(x._gt, lambda r: "fract(%s)" % r[0], (x,), meta=("op", "fract"))
def sl_sqrt(x): x = _coerce(x); return _node(x._gt, lambda r: "sqrt(%s)" % r[0], (x,), meta=("op", "sqrt"))
def sl_abs(x):  x = _coerce(x); return _node(x._gt, lambda r: "abs(%s)" % r[0], (x,), meta=("op", "abs"))
def sl_pow(x, y):                                                        # x**y (GLSL pow; x>=0 for fractional y)
  x = _coerce(x); return _node(x._gt, lambda r: "pow(%s, %s)" % (r[0], r[1]), (x, _coerce(y)), meta=("op", "pow"))
def sl_mix(a, b, t):                                                     # lerp a->b by t (component-wise for vec3)
  a = _coerce(a); return _node(a._gt, lambda r: "mix(%s, %s, %s)" % (r[0], r[1], r[2]), (a, _coerce(b), _coerce(t)), meta=("op", "mix"))
def sl_select(cond, a, b):                                              # cond!=0 ? a : b  (GLSL ternary; the `?:` form)
  a = _coerce(a)
  return _node(a._gt, lambda r: "((%s) != 0.0 ? (%s) : (%s))" % (r[0], r[1], r[2]), (_coerce(cond), a, _coerce(b)), meta=("op", "select01"))
def vexpr(x, y, z):           # build a vec3 expression from scalar (SelExpr|float) components — e.g. vexpr(0,-droop,0)
  return _node("vec3", lambda r: "vec3(%s, %s, %s)" % (r[0], r[1], r[2]), (_coerce(x), _coerce(y), _coerce(z)), meta=("op", "vec3"))

###############################################################################
# Runtime params — a GENERIC named uniform a DSL expression can reference and the asset rebinds LIVE.
# extrude_faces collects the params used across its dist/inset/dir exprs, assigns each a vec4 slot in the
# extrude's `EXPRP` SSBO, and seeds the default; the engine knows nothing of the names. Hold the object
# and call `.set(value)` (e.g. in onUpdate) to push a new value to every extrude that consumed it.
###############################################################################

class _ParamLeaf(SelExpr):
  """A named runtime param leaf — emits `EXPRP[slot].x` (float) or `.xyz` (vec3); the slot is assigned per
  extrude during collection (and frozen into that extrude's emitted GLSL). `.set(...)` rebinds it live."""
  def __init__(self, name, default):
    is_vec = hasattr(default, "x") and hasattr(default, "y") and hasattr(default, "z")
    super().__init__("vec3" if is_vec else "float", self._emit_param, meta=("plug", name))
    self._pname = name; self._is_vec = is_vec; self._default = default
    self._slot = 0; self._bindings = []                   # (module, slot) for every extrude that used it
  def _emit_param(self, domain):
    return "EXPRP[%d].%s" % (self._slot, "xyz" if self._is_vec else "x")
  def _default4(self):
    if self._is_vec: return [float(self._default.x), float(self._default.y), float(self._default.z), 0.0]
    return [float(self._default), 0.0, 0.0, 0.0]
  def set(self, *vals):
    """rebind live. float param: p.set(x). vec3 param: p.set(v3) or p.set(x, y, z)."""
    if len(vals) == 1 and hasattr(vals[0], "x"):
      v = vals[0]; x, y, z, w = float(v.x), float(v.y), float(v.z), 0.0
    elif len(vals) == 1:
      x, y, z, w = float(vals[0]), 0.0, 0.0, 0.0
    else:
      q = list(vals) + [0.0, 0.0, 0.0]; x, y, z, w = float(q[0]), float(q[1]), float(q[2]), 0.0
    for (m, slot) in self._bindings:
      m.set_expr_param4(slot, x, y, z, w)

def param(name, default=0.0):
  """declare a RUNTIME param for extrude_faces distance/inset/direction expressions (float or vec3 default).
  Hold the returned object and call `.set(value)` to rebind live — e.g. drive a phase from absolutetime."""
  return _ParamLeaf(name, default)

def collect_params(*exprs):
  """unique _ParamLeaf objects referenced by these SelExprs (dedup by name, first-seen order)."""
  seen = {}; out = []
  def walk(n):
    if isinstance(n, _ParamLeaf) and n._pname not in seen:
      seen[n._pname] = n; out.append(n)
    for c in n._children: walk(c)
  for e in exprs:
    if isinstance(e, SelExpr): walk(e)
  return out

###############################################################################
# predicate builders (return SelExpr) — thin wrappers over the atoms + helpers.
###############################################################################

def sel_normal_dir(n, t=0.5, soft=1e-3):
  """elements whose normal points toward `n` past threshold t (soft = falloff). POINT/POLY only."""
  L = math.sqrt(n.x * n.x + n.y * n.y + n.z * n.z) or 1.0
  nn = _v(lambda d: "vec3(%r, %r, %r)" % (n.x / L, n.y / L, n.z / L),
          meta=("vconst", (n.x / L, n.y / L, n.z / L)))
  return sl_smoothstep(t - max(soft, 1e-4), t + max(soft, 1e-4), S.N.dot(nn))

def sel_id_range(lo, hi):
  """elements with id in [lo,hi]."""
  return (S.id >= float(lo)) & (S.id <= float(hi))

def sel_area_gt(t):
  """POLY faces with area > t."""
  return S.area > float(t)

def sel_dihedral_gt(deg, soft=2.0):
  """LINE edges whose dihedral angle exceeds `deg` degrees (a soft band of `soft` deg). The bevel/chamfer
  selector: sharp creases match, flat/coplanar edges (and boundaries, dihedral=0) don't."""
  r  = math.radians(float(deg)); sr = math.radians(max(float(soft), 0.5))
  return sl_smoothstep(r - sr, r + sr, S.angle)

def sel_length_gt(t):
  """LINE edges longer than t."""
  return S.length > float(t)

def sel_dist_point(c, r, soft=1e-3):
  """elements whose position is within radius r of point c."""
  return sl_smoothstep(r + max(soft, 1e-4), r - max(soft, 1e-4), (S.P - c).length())

def sel_height_band(lo, hi, axis=None, soft=1e-3):
  """elements whose position along `axis` (default +Y) is in [lo,hi]."""
  from orkengine.core import vec3
  ax = axis if axis is not None else vec3(0, 1, 0)
  h  = S.P.dot(ax)
  s  = max(soft, 1e-4)
  return sl_smoothstep(lo - s, lo + s, h) * sl_smoothstep(hi + s, hi - s, h)

###############################################################################
# MaskOp — the two-triple (matched / unmatched) bit transform a Select writes.
#   matched:   tags = ((tags & sel_and)   | sel_or)   ^ sel_xor
#   unmatched: tags = ((tags & unsel_and) | unsel_or) ^ unsel_xor
# identity defaults; the factories below are named constructors.
###############################################################################

_U32 = 0xFFFFFFFF

class MaskOp:
  __slots__ = ("sel_and", "sel_or", "sel_xor", "unsel_and", "unsel_or", "unsel_xor")
  def __init__(self, sel_and=_U32, sel_or=0, sel_xor=0, unsel_and=_U32, unsel_or=0, unsel_xor=0):
    self.sel_and = sel_and & _U32; self.sel_or = sel_or & _U32; self.sel_xor = sel_xor & _U32
    self.unsel_and = unsel_and & _U32; self.unsel_or = unsel_or & _U32; self.unsel_xor = unsel_xor & _U32
  def as_list(self):
    return [self.sel_and, self.sel_or, self.sel_xor, self.unsel_and, self.unsel_or, self.unsel_xor]
  def triple(self):
    # the MATCHED triple [and, or, xor] — what a generator/modifier applies to a face PARTITION (the
    # unmatched triple is moot when the whole partition is the set being transformed).
    return [self.sel_and, self.sel_or, self.sel_xor]

def group(n):           return 1 << (int(n) & 31)            # bit mask for one group
def groups(*ns):        return _orall(group(n) for n in ns)  # several groups OR'd
def _orall(it):
  v = 0
  for x in it: v |= x
  return v

def add(m):     return MaskOp(sel_or=m)                        # matched: |= m
def remove(m):  return MaskOp(sel_and=~m)                      # matched: clear m
def toggle(m):  return MaskOp(sel_xor=m)                       # matched: flip m
def isolate(m): return MaskOp(sel_and=0, sel_or=m)             # matched: clear all, set m
def replace(m): return MaskOp(sel_or=m, unsel_and=~m)          # group = exactly the matched set

__all__ = ["POINT", "POLY", "LINE", "SelExpr", "S",
           "sl_smoothstep", "sl_step", "sl_clamp", "sl_min", "sl_max", "sl_sin", "sl_cos", "sl_fract",
           "sl_sqrt", "sl_abs", "sl_pow", "sl_mix", "sl_select", "vexpr",
           "param", "collect_params",
           "sel_normal_dir", "sel_id_range", "sel_area_gt", "sel_dihedral_gt", "sel_length_gt",
           "sel_dist_point", "sel_height_band",
           "MaskOp", "group", "groups", "add", "remove", "toggle", "isolate", "replace"]
