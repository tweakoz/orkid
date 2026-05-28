###############################################################################
# ork.dflow._expr — symbolic expression builder for HyperSyn DSL bindings.
#
# Authored at DSL time, lowered at generatedflow() time. The Expr tree captures
# what a binding should compute (e.g. 0.6 + sin(time*4) * 0.2) without committing
# to a runtime representation; the lowerer picks between:
#   (a) a floatxf chain on the bound plug + a connection from a Globals/Params
#       source — when the expression fits the chain's unary-on-single-input shape
#   (b) a FloatExprModule node with serialized bytecode — for multi-input or
#       chain-unfriendly expressions
#
# Author surface lives on the `Expr` singleton:
#   Expr.time, Expr.dt, Expr.param(name), Expr.const(x)
#   Expr.sin/cos/abs/sqrt/pow/clamp/min/max/lerp
#   Expr.vec3(x[, y, z])
# Plus Python-native arithmetic operators on any Expr (+, -, *, /, unary -).
#
# Constants fold at construction time, so Const(0.6) + Const(0.4) collapses to
# Const(1.0) before the lowerer ever sees it.
###############################################################################

import math


# Note: the base class is named ExprNode (not Expr) because `Expr` is bound at
# module bottom to the _ExprFactory singleton — the public author surface. The
# class itself is internal: tests and the lowerer pattern-match on specific
# subclasses (Const, BinOp, ...) directly, never on the base.
class ExprNode:
    """Base symbolic node. Subclasses are leaves (Const/TimeRef/ParamRef) or
    composites (BinOp/UnaryFn/Vec3Expr and the multi-arg helpers). Arithmetic
    operators on any node produce a new node; Python numbers are auto-wrapped
    via _wrap."""

    __slots__ = ()

    # --- arithmetic operator overloads --------------------------------------

    def __add__(self, other):       return _make_binop("+", self, _wrap(other))
    def __radd__(self, other):      return _make_binop("+", _wrap(other), self)
    def __sub__(self, other):       return _make_binop("-", self, _wrap(other))
    def __rsub__(self, other):      return _make_binop("-", _wrap(other), self)
    def __mul__(self, other):       return _make_binop("*", self, _wrap(other))
    def __rmul__(self, other):      return _make_binop("*", _wrap(other), self)
    def __truediv__(self, other):   return _make_binop("/", self, _wrap(other))
    def __rtruediv__(self, other):  return _make_binop("/", _wrap(other), self)
    def __neg__(self):              return _make_binop("*", Const(-1.0), self)


# ---- leaves ---------------------------------------------------------------

class Const(ExprNode):
    """Numeric literal. Always a single concrete float."""
    __slots__ = ("value",)

    def __init__(self, value):
        self.value = float(value)

    def __repr__(self):
        return f"Const({self.value!r})"


class ContextRef(ExprNode):
    """Reference to a registered context variable (per the C++ side's
    ContextVariableRegistry — Expr.time, Expr.dt, Expr.ptc.unit_age, ...).
    The lowerer resolves these at generatedflow() time via the C++ registry
    + Python class mirror (ork.dflow._context_classes).

    dsl_name is the dotted name as declared in the registry, e.g.
    'time', 'dt', 'ptc.unit_age'."""
    __slots__ = ("dsl_name",)

    def __init__(self, dsl_name):
        self.dsl_name = dsl_name

    def __repr__(self):
        return f"ContextRef({self.dsl_name!r})"


class EntityRef(ExprNode):
    """Reference to one SRT component of a published ECS entity's
    transform. Carries the published-xf key (e.g. 'saddle_ptc0') plus a
    component selector ('pos' / 'quat' / 'scale'). The lowerer
    materializes a particle EntityRef module per unique key in the graph
    and connects the matching output plug into the consumer's chain.

    Vec3 / quat — pos and scale lower to vec3 outputs; quat lowers to a
    quat output. The downstream binding's expected plug type is
    enforced when the chain is emitted.

    Standalone (non-ECS) graphs lookup against an empty resolver and
    see identity defaults — graphs using Expr.entity(...) still run."""
    __slots__ = ("entity_name", "component")

    def __init__(self, entity_name, component):
        self.entity_name = entity_name
        self.component   = component

    def __repr__(self):
        return f"EntityRef({self.entity_name!r}, {self.component!r})"


class EntityTransform(ExprNode):
    """Apply a published entity's transform to a vec3 input.

    mode='point' → full SRT applied (includes translation):
        world_point = host_xf * local
    mode='dir'   → 3x3 (rotation + scale) only:
        world_dir = (host_xf 3x3) * local

    `local` is an ExprNode (typically a Vec3Expr or a wrapped fvec3) —
    the lowerer materializes a TransformPoint / TransformDir particle
    module per (entity_name, mode) pair and connects this local input
    through the vec3 binding path. Standalone graphs (no resolver) or
    unresolved names → output equals input (identity)."""
    __slots__ = ("entity_name", "mode", "local")

    def __init__(self, entity_name, mode, local):
        if mode not in ("point", "dir"):
            raise ValueError(
                f"EntityTransform mode must be 'point' or 'dir', got {mode!r}")
        self.entity_name = entity_name
        self.mode        = mode
        self.local       = _wrap(local)

    def __repr__(self):
        return f"EntityTransform({self.entity_name!r}, {self.mode!r}, {self.local!r})"


class _EntityProxy:
    """Component-accessor returned by Expr.entity('name'). Attribute
    access (.pos / .quat / .scale) yields an EntityRef leaf. The
    transformPoint(local) / transformDir(local) methods produce
    EntityTransform nodes wrapping a vec3 local input."""
    __slots__ = ("_entity_name",)

    def __init__(self, entity_name):
        self._entity_name = entity_name

    @property
    def pos(self):
        return EntityRef(self._entity_name, "pos")

    @property
    def quat(self):
        return EntityRef(self._entity_name, "quat")

    @property
    def scale(self):
        return EntityRef(self._entity_name, "scale")

    @property
    def scaleU(self):
        """Scalar uniform scale — equivalent to .scale.x but typed as a
        SCALAR Expr. Use when binding to a float plug, e.g.
        EmissionRadius = base_radius * Expr.entity('X').scaleU."""
        return EntityRef(self._entity_name, "scale_uniform")

    def transformPoint(self, local):
        """Apply the host's full SRT to `local` (point semantics —
        includes translation). `local` may be a Vec3Expr, an fvec3
        literal, or another vec3-typed ExprNode."""
        return EntityTransform(self._entity_name, "point", local)

    def transformDir(self, local):
        """Apply the host's rotation+scale (3x3) to `local` (direction
        semantics — no translation)."""
        return EntityTransform(self._entity_name, "dir", local)

    def __repr__(self):
        return f"_EntityProxy({self._entity_name!r})"


class _NamespaceProxy:
    """Dotted-prefix accessor for the Expr factory. Lets authors write
    `Expr.ptc.unit_age` — first attribute returns this proxy (with prefix
    'ptc'), second attribute returns ContextRef('ptc.unit_age'). Lookups
    are not validated against the registry at attribute-access time — the
    lowerer raises a clear error if the name is unknown at emission time."""
    __slots__ = ("_prefix",)

    def __init__(self, prefix):
        self._prefix = prefix

    def __getattr__(self, name):
        return ContextRef(f"{self._prefix}.{name}")

    def __repr__(self):
        return f"_NamespaceProxy({self._prefix!r})"


class _ParamProxy:
    """Author-facing accessor for exposed parameters. Supports BOTH the
    call form `Expr.param("Name")` and the attribute form `Expr.param.Name`
    — purely syntactic, both produce identical ParamRef nodes. Attribute
    form is the preferred style; the call form stays for parameter names
    that aren't valid Python identifiers (rare)."""
    __slots__ = ()

    def __call__(self, name):
        return ParamRef(name)

    def __getattr__(self, name):
        # dunders should follow normal attribute resolution.
        if name.startswith("__") and name.endswith("__"):
            raise AttributeError(name)
        return ParamRef(name)

    def __repr__(self):
        return "_ParamProxy()"


_PARAM_PROXY = _ParamProxy()


class ParamRef(ExprNode):
    """Reference to an exposed runtime-mutable parameter, declared via
    self.expose(name, default=...) in the DSL __init__. Lowers to a connection
    from the (lazily-added) ParametersModule's output of the same name."""
    __slots__ = ("name",)

    def __init__(self, name):
        self.name = name

    def __repr__(self):
        return f"ParamRef({self.name!r})"


class PromotedSource(ExprNode):
    """Synthetic leaf produced by the lowerer when a typed-module subtree
    (e.g. MaxExpr inside a larger chain expression) has been materialized
    into its own real graph module. Holds a direct reference to that module's
    output plug so the outer chain can connect to it as if it were any other
    source (ContextRef/ParamRef).

    Never authored by DSL users — only inserted by _bindings._promote during
    tree rewriting before lower_to_chain runs."""
    __slots__ = ("output_plug",)

    def __init__(self, output_plug):
        self.output_plug = output_plug

    def __repr__(self):
        return f"PromotedSource(<plug>)"


# ---- composites -----------------------------------------------------------

class BinOp(ExprNode):
    """+ - * /. Construct via _make_binop so Const+Const folds to Const."""
    __slots__ = ("op", "lhs", "rhs")

    def __init__(self, op, lhs, rhs):
        self.op = op
        self.lhs = lhs
        self.rhs = rhs

    def __repr__(self):
        return f"BinOp({self.op!r}, {self.lhs!r}, {self.rhs!r})"


class UnaryFn(ExprNode):
    """Single-argument function (sin, cos, abs, sqrt). Construct via
    _make_unary so Const folds when the fn is in _UNARY_TABLE."""
    __slots__ = ("fn", "arg")

    def __init__(self, fn, arg):
        self.fn = fn
        self.arg = arg

    def __repr__(self):
        return f"UnaryFn({self.fn!r}, {self.arg!r})"


class PowExpr(ExprNode):
    """pow(x, k). Chain-lowerable when k is Const (→ floatxfpowdata)."""
    __slots__ = ("x", "k")

    def __init__(self, x, k):
        self.x = x
        self.k = k

    def __repr__(self):
        return f"Pow({self.x!r}, {self.k!r})"


class ClampExpr(ExprNode):
    """clamp(x, lo, hi). Always FloatExprModule (no chain stage today)."""
    __slots__ = ("x", "lo", "hi")

    def __init__(self, x, lo, hi):
        self.x = x
        self.lo = lo
        self.hi = hi

    def __repr__(self):
        return f"Clamp({self.x!r}, {self.lo!r}, {self.hi!r})"


class MinMaxExpr(ExprNode):
    """min(a,b) / max(a,b). Always FloatExprModule."""
    __slots__ = ("op", "a", "b")

    def __init__(self, op, a, b):
        self.op = op
        self.a = a
        self.b = b

    def __repr__(self):
        return f"{self.op}({self.a!r}, {self.b!r})"


class LerpExpr(ExprNode):
    """lerp(a, b, t) = a*(1-t) + b*t. Always FloatExprModule."""
    __slots__ = ("a", "b", "t")

    def __init__(self, a, b, t):
        self.a = a
        self.b = b
        self.t = t

    def __repr__(self):
        return f"Lerp({self.a!r}, {self.b!r}, {self.t!r})"


class FmodExpr(ExprNode):
    """fmod(x, k) — C-style fmod, preserves sign. Chain-lowerable when k is
    Const → floatxfmoddata."""
    __slots__ = ("x", "k")

    def __init__(self, x, k):
        self.x = x
        self.k = k

    def __repr__(self):
        return f"Fmod({self.x!r}, {self.k!r})"


class SmoothstepExpr(ExprNode):
    """smoothstep(x, edge0, edge1) — CLAMPED LINEAR remap to [0,1], NOT
    cubic hermite. Returns 0 below edge0, 1 above edge1, linear between.
    Chain-lowerable when edges are Const → floatxfsmoothstepdata."""
    __slots__ = ("x", "edge0", "edge1")

    def __init__(self, x, edge0, edge1):
        self.x = x
        self.edge0 = edge0
        self.edge1 = edge1

    def __repr__(self):
        return f"Smoothstep({self.x!r}, {self.edge0!r}, {self.edge1!r})"


class QuantizeExpr(ExprNode):
    """quantize(x, step). Matches C++ floatxfquantizedata semantics exactly:
    inumsteps = int(1/step) + 1; result = int(x*inumsteps) / inumsteps.
    Chain-lowerable when step is Const → floatxfquantizedata."""
    __slots__ = ("x", "step")

    def __init__(self, x, step):
        self.x = x
        self.step = step

    def __repr__(self):
        return f"Quantize({self.x!r}, {self.step!r})"


class CurveExpr(ExprNode):
    """curve(x, curve_data) — samples a MultiCurve1D at x. Always chain-
    lowerable → floatxfmulticurvedata. curve_data is held opaquely; the
    lowerer materializes it (a list of (t,v) tuples becomes a real
    MultiCurve1D at lower time; an existing MultiCurve1D passes through)."""
    __slots__ = ("x", "curve")

    def __init__(self, x, curve):
        self.x = x
        self.curve = curve

    def __repr__(self):
        return f"Curve({self.x!r}, <curve>)"


class Vec3Expr(ExprNode):
    """vec3 expression — three scalar Expr children. Single-arg constructor
    broadcasts: Vec3Expr(s) means vec3(s, s, s). At lower time each axis
    lowers independently and the three results combine via a Vec3CombineModule
    (one input per axis, one vec3 output)."""
    __slots__ = ("x", "y", "z")

    def __init__(self, x, y=None, z=None):
        if y is None and z is None:
            wx = _wrap(x)
            self.x = wx
            self.y = wx
            self.z = wx
        else:
            self.x = _wrap(x)
            self.y = _wrap(y)
            self.z = _wrap(z)

    def __repr__(self):
        return f"Vec3Expr({self.x!r}, {self.y!r}, {self.z!r})"


# ---- constant-folding constructors ----------------------------------------

_BINOP_TABLE = {
    "+": lambda a, b: a + b,
    "-": lambda a, b: a - b,
    "*": lambda a, b: a * b,
    "/": lambda a, b: a / b,
}

_UNARY_TABLE = {
    "sin": math.sin,
    "cos": math.cos,
    "abs": abs,
    "sqrt": math.sqrt,
}


def _make_binop(op, lhs, rhs):
    """Build a BinOp; fold to Const when both operands are Const."""
    if isinstance(lhs, Const) and isinstance(rhs, Const):
        return Const(_BINOP_TABLE[op](lhs.value, rhs.value))
    return BinOp(op, lhs, rhs)


def _make_unary(fn, arg):
    """Build a UnaryFn; fold to Const when arg is Const and fn is foldable."""
    wrapped = _wrap(arg)
    if isinstance(wrapped, Const) and fn in _UNARY_TABLE:
        return Const(_UNARY_TABLE[fn](wrapped.value))
    return UnaryFn(fn, wrapped)


def _wrap(value):
    """Pass ExprNode through; wrap Python numbers as Const; wrap an
    fvec3 as a Vec3Expr of three Const axes (so vec3 arithmetic can mix
    Expr nodes and real literals). Anything else raises so DSL-author
    bugs surface at construction rather than at lower time."""
    if isinstance(value, ExprNode):
        return value
    if isinstance(value, (int, float)):
        return Const(value)
    # fvec3 / vec3-like — duck-typed via .x/.y/.z so we don't take a
    # hard import dep on the core math module just for this check. The
    # type check matches our existing pattern (the codebase already
    # treats anything with x/y/z as a vec3 in several places).
    if hasattr(value, "x") and hasattr(value, "y") and hasattr(value, "z"):
        return Vec3Expr(Const(float(value.x)),
                        Const(float(value.y)),
                        Const(float(value.z)))
    raise TypeError(
        f"cannot wrap {type(value).__name__!r} as Expr (got {value!r})")


# ---- author-facing factory (singleton, imported as `Expr`) ----------------

class _ExprFactory:
    """Author surface: imported as Expr from ork.dflow. Each attribute or
    method returns an Expr tree node ready to feed into bind()."""

    # --- source references ---

    @property
    def time(self):
        """Per-graphinst time-since-first-compute, in seconds. Lowers to a
        Globals.RelTime connection via the context-variable registry."""
        return ContextRef("time")

    @property
    def dt(self):
        """Per-frame delta-time. Requires the Globals.DeltaTime output (task
        #24); the lowerer raises a clear error if the registration is
        missing."""
        return ContextRef("dt")

    @property
    def ptc(self):
        """Per-particle context: Expr.ptc.unit_age, Expr.ptc.random, etc.
        Returns a namespace proxy that builds ContextRef on attribute access."""
        return _NamespaceProxy("ptc")

    def entity(self, name):
        """Reference an ECS-published entity's live world transform.

        Returns an _EntityProxy whose .pos / .quat / .scale attributes
        yield EntityRef leaves that compose into binding chains. The
        lowerer creates one shared EntityRef particle module per unique
        name in the graph and wires the requested output plug. Common
        idiom — track the entity owning the particle component:

            emit = P.RingEmitter(..., Offset=Expr.entity("saddle_ptc0").pos)

        The string is the published-xf key as registered by the
        Simulation (publish_xf base + monotonic suffix; see
        SpawnData::publishxf_name)."""
        return _EntityProxy(name)

    @property
    def param(self):
        """Reference an exposed runtime-mutable parameter. The parameter must
        be declared via self.expose(name, default=...) in the DSL __init__.

        Two equivalent syntaxes:
            Expr.param.Name       # attribute style (preferred)
            Expr.param("Name")    # call style (for names that aren't valid
                                  # Python identifiers — rare).
        """
        return _PARAM_PROXY

    def const(self, value):
        """Wrap a numeric literal explicitly. Arithmetic auto-wraps; this is
        for stylistic emphasis or when a chain must start with a literal."""
        return Const(value)

    # --- single-argument functions ---

    def sin(self, x):  return _make_unary("sin", x)
    def cos(self, x):  return _make_unary("cos", x)
    def abs(self, x):  return _make_unary("abs", x)
    def sqrt(self, x): return _make_unary("sqrt", x)

    # --- multi-argument functions ---

    def pow(self, x, k):
        wx, wk = _wrap(x), _wrap(k)
        if isinstance(wx, Const) and isinstance(wk, Const):
            return Const(wx.value ** wk.value)
        return PowExpr(wx, wk)

    def clamp(self, x, lo, hi):
        wx, wlo, whi = _wrap(x), _wrap(lo), _wrap(hi)
        if isinstance(wx, Const) and isinstance(wlo, Const) and isinstance(whi, Const):
            return Const(max(wlo.value, min(whi.value, wx.value)))
        return ClampExpr(wx, wlo, whi)

    def min(self, a, b):
        wa, wb = _wrap(a), _wrap(b)
        if isinstance(wa, Const) and isinstance(wb, Const):
            return Const(min(wa.value, wb.value))
        return MinMaxExpr("min", wa, wb)

    def max(self, a, b):
        wa, wb = _wrap(a), _wrap(b)
        if isinstance(wa, Const) and isinstance(wb, Const):
            return Const(max(wa.value, wb.value))
        return MinMaxExpr("max", wa, wb)

    def lerp(self, a, b, t):
        wa, wb, wt = _wrap(a), _wrap(b), _wrap(t)
        if isinstance(wa, Const) and isinstance(wb, Const) and isinstance(wt, Const):
            return Const(wa.value * (1.0 - wt.value) + wb.value * wt.value)
        return LerpExpr(wa, wb, wt)

    # --- chain-targetable primitives (each maps 1:1 to an existing xf stage) ---

    def fmod(self, x, k):
        wx, wk = _wrap(x), _wrap(k)
        if isinstance(wx, Const) and isinstance(wk, Const):
            return Const(math.fmod(wx.value, wk.value))
        return FmodExpr(wx, wk)

    def smoothstep(self, x, edge0, edge1):
        wx, we0, we1 = _wrap(x), _wrap(edge0), _wrap(edge1)
        if isinstance(we0, Const) and isinstance(we1, Const) and we0.value == we1.value:
            raise ValueError(
                f"smoothstep edges must differ (got edge0=edge1={we0.value!r})")
        if isinstance(wx, Const) and isinstance(we0, Const) and isinstance(we1, Const):
            t = (wx.value - we0.value) / (we1.value - we0.value)
            return Const(max(0.0, min(1.0, t)))
        return SmoothstepExpr(wx, we0, we1)

    def quantize(self, x, step):
        wx, ws = _wrap(x), _wrap(step)
        if isinstance(wx, Const) and isinstance(ws, Const):
            # match C++ pass-through when step <= 0
            if ws.value <= 0.0:
                return Const(wx.value)
            inumsteps = int(1.0 / ws.value) + 1
            ib = int(wx.value * float(inumsteps))
            return Const(float(ib) / float(inumsteps))
        return QuantizeExpr(wx, ws)

    def curve(self, x, curve_data):
        """Sample curve_data at x. curve_data accepts an existing MultiCurve1D
        or a list of (t, v) control-point tuples — the lowerer materializes
        the latter into a real curve at graph-build time."""
        return CurveExpr(_wrap(x), curve_data)

    def rand_range(self, low, high):
        """Per-particle uniform random in [low, high]. Pure sugar:
        desugars to lerp(low, high, ptc.random). Each particle gets its
        own random sample (driven by particle.mfRandom at emit time);
        low/high are uniform per emit cohort (or whatever expressions
        produce). Aux is the primary consumer:
            emitter.Aux.x = Expr.rand_range(0.1, 0.9)
            emitter.Aux.x = Expr.rand_range(
                Expr.sin(Expr.time)*0.4 + 0.1,
                Expr.sin(Expr.time)*0.4 + 0.5,
            )
        """
        return self.lerp(low, high, self.ptc.random)

    def remap(self, x, in_lo, in_hi, out_lo, out_hi):
        """Linearly remap x from [in_lo, in_hi] to [out_lo, out_hi]. Pure
        sugar — desugars to (x - in_lo) / (in_hi - in_lo) * (out_hi - out_lo)
        + out_lo, so constant-folding and chain lowering happen for free via
        the BinOp path (bias, scale, scale, bias when all ranges are Const)."""
        wx = _wrap(x)
        in_lo_e, in_hi_e = _wrap(in_lo), _wrap(in_hi)
        out_lo_e, out_hi_e = _wrap(out_lo), _wrap(out_hi)
        return (wx - in_lo_e) / (in_hi_e - in_lo_e) * (out_hi_e - out_lo_e) + out_lo_e

    # --- vec3 ---

    def vec3(self, x, y=None, z=None):
        return Vec3Expr(x, y, z)


# the singleton authors import.
Expr = _ExprFactory()
