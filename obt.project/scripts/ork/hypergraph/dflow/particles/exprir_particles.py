###############################################################################
# ork.hypergraph.dflow.particles.exprir_particles — the particle-family adoption of the
# shared ExprIR (JUL13 DFLOW E2.5, S8 + S9). The adjudication supersedes Q7 with the CONTEXT
# model: a family declares one ExprContext per author-time expression ENVIRONMENT it exposes.
# Particles expose TWO, with vastly different vocabularies + evaluation sites:
#
#   "particles.force"  (S8) — a per-particle FORCE (acceleration) expression, evaluated on the
#       CPU per particle per frame by the C++ ExprForce module. Symbols evaluate honestly from
#       the live particle: unit_age / age / random / pos.{x,y,z} / vel.{x,y,z} / speed.
#
#   "particles.color"  (S9) — a RENDERER color-ramp expression; lives in the sibling module
#       gradient_expr.py (which REUSES the shared vocabulary + builder + evaluator here).
#
# The force context keys off the shared node set (Const/ParamRef/Call) + the family-neutral
# JSON (exprir.encode_json) — the C++ ExprForce stores the force JSON in its reflected
# force_{x,y,z} fields and evaluates the tree per particle on the CPU.
###############################################################################

from ... import exprir as _x
from ...exprir import Const, ParamRef, Call


###############################################################################
# vocabulary shared by both contexts (matches the C++ ExprForce evaluator op set).
###############################################################################

def _math_functions():
    fns = [_x.infix("add", "+"), _x.infix("sub", "-"), _x.infix("mul", "*"),
           _x.infix("div", "/"), _x.unary("neg", "-")]
    for nm in ("sin", "cos", "abs", "sqrt", "fract"):
        fns.append(_x.fn(nm, 1))
    for nm in ("min", "max", "step", "pow", "mod"):
        fns.append(_x.fn(nm, 2))
    for nm in ("clamp", "mix", "smoothstep"):
        fns.append(_x.fn(nm, 3))
    return fns


# honest per-particle FORCE symbols (ParamRef kind "ptc"); the C++ evaluator resolves these
# names. Keep in lockstep with _resolveSym() in modules_force_expr.cpp.
_FORCE_SYMBOLS = ("unit_age", "age", "random",
                  "pos_x", "pos_y", "pos_z", "vel_x", "vel_y", "vel_z", "speed")

PTC_LEAF = "ptc"

FORCE_CTX = _x.register_context(_x.ExprContext(
    "particles.force", functions=_math_functions(),
    leaves=[_x.LeafSpec(PTC_LEAF)],
    doc="per-particle force (acceleration) scalar expression; CPU-evaluated per particle"))


class ParticleExprError(_x.ExprIRError):
    """A particle ExprIR was authored with a symbol/op outside its context vocabulary, or the
    honest per-context symbol whitelist (loud — never a silent zero force / black ramp)."""


def _check_symbols(node, allowed, ctx_name):
    """Walk a tree; every ptc leaf name must be in `allowed` (the context validates the leaf
    KIND, this pins the honest NAME set). Loud on an out-of-vocabulary symbol."""
    if isinstance(node, ParamRef):
        if node.kind == PTC_LEAF and node.name not in allowed:
            raise ParticleExprError(
                "%s: symbol %r is not honestly available here; vocabulary = %s"
                % (ctx_name, node.name, sorted(allowed)))
    elif isinstance(node, Call):
        for a in node.args:
            _check_symbols(a, allowed, ctx_name)


###############################################################################
# author surface — a thin builder producing validated ExprIR trees. Operators + a symbol
# namespace, mirroring the established E.* ergonomics (Python-arithmetic on any node).
###############################################################################

class _EB:
    """One expression-builder node wrapping an ExprIR tree; arithmetic/ops compose new ones."""
    __slots__ = ("node",)

    def __init__(self, node):
        self.node = node

    def __add__(s, o):      return _EB(Call("add", s.node, _wrap(o).node))
    def __radd__(s, o):     return _EB(Call("add", _wrap(o).node, s.node))
    def __sub__(s, o):      return _EB(Call("sub", s.node, _wrap(o).node))
    def __rsub__(s, o):     return _EB(Call("sub", _wrap(o).node, s.node))
    def __mul__(s, o):      return _EB(Call("mul", s.node, _wrap(o).node))
    def __rmul__(s, o):     return _EB(Call("mul", _wrap(o).node, s.node))
    def __truediv__(s, o):  return _EB(Call("div", s.node, _wrap(o).node))
    def __rtruediv__(s, o): return _EB(Call("div", _wrap(o).node, s.node))
    def __neg__(s):         return _EB(Call("neg", s.node))
    def __pow__(s, o):      return _EB(Call("pow", s.node, _wrap(o).node))


def _wrap(x):
    if isinstance(x, _EB):
        return x
    return _EB(Const(float(x)))


class _SymbolNS:
    """A per-context symbol + math namespace. Attribute access yields a leaf builder; math
    methods build Call builders. Unknown symbols raise (fail-loud author surface)."""

    def __init__(self, symbols):
        self._symbols = frozenset(symbols)

    def __getattr__(self, name):
        if name in object.__getattribute__(self, "_symbols"):
            return _EB(ParamRef(PTC_LEAF, name))
        raise AttributeError(
            "no particle expression symbol %r; available = %s"
            % (name, sorted(object.__getattribute__(self, "_symbols"))))

    # ---- math (each returns an _EB) ----
    def const(self, v):              return _EB(Const(float(v)))
    def sin(self, x):                return _EB(Call("sin", _wrap(x).node))
    def cos(self, x):                return _EB(Call("cos", _wrap(x).node))
    def abs(self, x):                return _EB(Call("abs", _wrap(x).node))
    def sqrt(self, x):               return _EB(Call("sqrt", _wrap(x).node))
    def fract(self, x):              return _EB(Call("fract", _wrap(x).node))
    def min(self, a, b):             return _EB(Call("min", _wrap(a).node, _wrap(b).node))
    def max(self, a, b):             return _EB(Call("max", _wrap(a).node, _wrap(b).node))
    def step(self, e, x):            return _EB(Call("step", _wrap(e).node, _wrap(x).node))
    def pow(self, x, y):             return _EB(Call("pow", _wrap(x).node, _wrap(y).node))
    def mod(self, x, y):             return _EB(Call("mod", _wrap(x).node, _wrap(y).node))
    def clamp(self, x, a, b):        return _EB(Call("clamp", _wrap(x).node, _wrap(a).node, _wrap(b).node))
    def mix(self, a, b, t):          return _EB(Call("mix", _wrap(a).node, _wrap(b).node, _wrap(t).node))
    def smoothstep(self, e0, e1, x): return _EB(Call("smoothstep", _wrap(e0).node, _wrap(e1).node, _wrap(x).node))


# PF — the FORCE author namespace (PF.unit_age, PF.speed, PF.sin(...), ...).
PF = _SymbolNS(_FORCE_SYMBOLS)


###############################################################################
# capture: builder / number -> validated ExprIR JSON for the reflected field.
###############################################################################

def capture_force(x):
    """A force-channel expression (an _EB builder or a plain number) -> canonical
    particles.force ExprIR JSON. Loud on an out-of-vocabulary symbol/op."""
    node = _wrap(x).node
    FORCE_CTX.validate(node)
    _check_symbols(node, _FORCE_SYMBOLS, "particles.force")
    return _x.encode_json(node)


def parse_force(source):
    """Editor path (S7): particles.force author SOURCE -> validated canonical ExprIR JSON.
    The inverse of pretty_print for this context — ast-whitelist parse + context validate +
    the honest per-particle symbol check, so a syntax error, an out-of-vocabulary op, or a
    DISHONEST symbol (an open-leaf identifier that is not a real per-particle symbol) fails
    LOUDLY here and never reaches the C++ lowering (the #86 hard-assert hazard)."""
    node = _x.parse(source, FORCE_CTX)
    _check_symbols(node, _FORCE_SYMBOLS, "particles.force")
    return _x.encode_json(node)


def print_force(json_str):
    """The stored particles.force ExprIR JSON -> author SOURCE (pretty_print). Empty stored
    value (an unset axis) -> empty source."""
    if not json_str:
        return ""
    return _x.pretty_print(_x.decode_json(json_str), FORCE_CTX)


###############################################################################
# evaluator — the family owns its ExprIR evaluator (the IR is dumb data). Numeric eval over a
# symbol dict; the Python mirror of the C++ ExprForce evaluator (also reused by the S9 color
# LUT bake in the sibling gradient_expr.py).
###############################################################################

import math as _math

_UNARY = {"sin": _math.sin, "cos": _math.cos, "abs": abs,
          "sqrt": lambda v: _math.sqrt(v) if v > 0.0 else 0.0,
          "fract": lambda v: v - _math.floor(v), "neg": lambda v: -v}


def eval_expr(node, syms):
    """Deterministically evaluate an ExprIR tree over a {symbol_name: float} dict."""
    if isinstance(node, Const):
        return float(node.value)
    if isinstance(node, ParamRef):
        if node.name not in syms:
            raise ParticleExprError("eval: unbound symbol %r (have %s)" % (node.name, sorted(syms)))
        return float(syms[node.name])
    if isinstance(node, Call):
        a = [eval_expr(x, syms) for x in node.args]
        n = node.name
        if n in _UNARY:
            return _UNARY[n](a[0])
        if n == "add": return a[0] + a[1]
        if n == "sub": return a[0] - a[1]
        if n == "mul": return a[0] * a[1]
        if n == "div": return a[0] / a[1] if a[1] != 0.0 else 0.0
        if n == "min": return min(a[0], a[1])
        if n == "max": return max(a[0], a[1])
        if n == "step": return 1.0 if a[1] >= a[0] else 0.0
        if n == "pow": return _math.pow(a[0], a[1])
        if n == "mod": return a[0] - a[1] * _math.floor(a[0] / a[1]) if a[1] != 0.0 else 0.0
        if n == "clamp": return min(max(a[0], a[1]), a[2])
        if n == "mix": return a[0] + (a[1] - a[0]) * a[2]
        if n == "smoothstep":
            e0, e1, x = a
            t = (x - e0) / (e1 - e0) if e1 != e0 else 0.0
            t = min(max(t, 0.0), 1.0)
            return t * t * (3.0 - 2.0 * t)
        raise ParticleExprError("eval: unknown op %r" % (n,))
    raise ParticleExprError("eval: not an ExprIR node: %r" % (node,))
