###############################################################################
# ork.hypergraph.dflow.terrain._node — the expression-first terrain node.
#
# Terrain is a DAG (not a chain): an op output fans out to many consumers, and
# ops like Combine join two inputs. TerrainNode is a DslNode carrying algebra —
# operators LOWER to real dflow modules in the active trace graph:
#
#     node * k  /  k * node  /  node / k   ->  Remap (affine, no clamp)
#     node + k  /  k + node  /  node - k   ->  Remap
#     k - node                             ->  Remap(scale=-1, bias=k)
#     a * b  /  a + b  /  a - b  (nodes)   ->  Combine(MUL/ADD/SUB)
#
# One module per operator (no affine folding yet — a later peephole can collapse
# a*s+b into a single Remap). Python control flow (for/if) around these calls is
# graph metaprogramming: it runs at TRACE time and unrolls into DAG topology;
# nothing survives to bake time. Module names auto-increment per graph so loop
# iterations never collide.
###############################################################################

import itertools

from orkengine.lev2 import terrain as _terrain
from .._trace import DslNode, current_graph

# Combine op codes — MUST match enum CombineOp in hfdflow.h.
OP_ADD, OP_SUB, OP_MUL, OP_MIN, OP_MAX, OP_MIX = 0, 1, 2, 3, 4, 5

# Affine operators must NOT clip — Remap always clamps, so use bounds far outside
# any plausible (normalized) height. Explicit T.Clamp / T.Remap set real bounds.
_NO_CLAMP_LO = -1.0e9
_NO_CLAMP_HI = 1.0e9

# Per-graph anonymous-name counters, keyed (id(graph), prefix), so loops that
# call the same op repeatedly get fbm_0/fbm_1/... without collision.
_anon_counters = {}


def anon_name(prefix, graph):
    key = (id(graph), prefix)
    if key not in _anon_counters:
        _anon_counters[key] = itertools.count(0)
    return f"{prefix}_{next(_anon_counters[key])}"


def graph_or_raise(what):
    g = current_graph()
    if g is None:
        raise RuntimeError(
            f"{what} called outside a trace context — call super().__init__() "
            f"at the top of your HeightField subclass __init__.")
    return g


def make_const(level, *, name=None):
    g = graph_or_raise("Const")
    m = g.create(name or anon_name("const", g), _terrain.ConstModule)
    m.inputs.level = float(level)
    return TerrainNode(m, m.outputs.Out)


def make_remap(node, *, scale=1.0, bias=0.0, lo=_NO_CLAMP_LO, hi=_NO_CLAMP_HI, name=None):
    g = graph_or_raise("Remap")
    if not isinstance(node, TerrainNode):
        raise TypeError(f"Remap expects a terrain node; got {type(node).__name__}")
    m = g.create(name or anon_name("remap", g), _terrain.RemapModule)
    g.connect(m.inputs.In, node.output_plug)
    m.inputs.scale = float(scale)
    m.inputs.bias = float(bias)
    m.inputs.lo = float(lo)
    m.inputs.hi = float(hi)
    return TerrainNode(m, m.outputs.Out)


def _coerce(x):
    """A binary-op operand: pass TerrainNode through; wrap a scalar as a Const
    field (so T.Max(h, 0.0) / T.Mix(rock, 0.2, t) read naturally)."""
    return x if isinstance(x, TerrainNode) else make_const(float(x))


def make_combine(a, b, op, *, t=0.5, name=None):
    g = graph_or_raise("Combine")
    a = _coerce(a)
    b = _coerce(b)
    m = g.create(name or anon_name("comb", g), _terrain.CombineModule)
    m.op = int(op)
    g.connect(m.inputs.A, a.output_plug)
    g.connect(m.inputs.B, b.output_plug)
    m.inputs.t = float(t)
    return TerrainNode(m, m.outputs.Out)


def make_maskblend(a, b, mask, *, name=None):
    """Per-texel blend: out = mix(a, b, mask). `mask` is a FIELD (the masking
    primitive — b shows through where mask is high). Unlike Combine(MIX), whose
    `t` is a uniform scalar, this blends by a per-texel field."""
    g = graph_or_raise("MaskBlend")
    a = _coerce(a)
    b = _coerce(b)
    if not isinstance(mask, TerrainNode):
        raise TypeError(
            f"MaskBlend mask must be a terrain field (TerrainNode); got "
            f"{type(mask).__name__}. For a uniform blend use mix(a, b, t=<scalar>).")
    m = g.create(name or anon_name("mask", g), _terrain.MaskBlendModule)
    g.connect(m.inputs.A, a.output_plug)
    g.connect(m.inputs.B, b.output_plug)
    g.connect(m.inputs.M, mask.output_plug)
    return TerrainNode(m, m.outputs.Out)


class TerrainNode(DslNode):
    """A DslNode with terrain algebra. Operators lower to Remap (scalar operand,
    affine) or Combine (node operand) modules in the active trace graph."""

    __slots__ = ()

    # multiply: node*k / k*node -> Remap(scale) ; node*node -> Combine(MUL)
    def __mul__(self, other):
        if isinstance(other, TerrainNode):
            return make_combine(self, other, OP_MUL)
        return make_remap(self, scale=float(other))

    __rmul__ = __mul__

    # add: node+k / k+node -> Remap(bias) ; node+node -> Combine(ADD)
    def __add__(self, other):
        if isinstance(other, TerrainNode):
            return make_combine(self, other, OP_ADD)
        return make_remap(self, bias=float(other))

    __radd__ = __add__

    # subtract (non-commutative)
    def __sub__(self, other):
        if isinstance(other, TerrainNode):
            return make_combine(self, other, OP_SUB)
        return make_remap(self, bias=-float(other))

    def __rsub__(self, other):  # other(scalar) - self  ==  -self + other
        return make_remap(self, scale=-1.0, bias=float(other))

    # divide by a scalar only (no Combine DIV op)
    def __truediv__(self, other):
        if isinstance(other, TerrainNode):
            raise TypeError(
                "node / node is unsupported (no Combine DIV op) — invert and multiply, "
                "or add a DIV op to CombineModule")
        return make_remap(self, scale=1.0 / float(other))

    def __neg__(self):
        return make_remap(self, scale=-1.0)

    # masking sugar: `base.masked_by(other, mask)` == mix(base, other, mask) ==
    # Houdini's lerp(input, op(input), mask). `other` shows through where the
    # [0,1] mask field is high; self elsewhere. e.g.
    #   steep = T.slope(h)
    #   h = h.masked_by(h + detail, steep)   # add detail only on steep slopes
    def masked_by(self, other, mask):
        return make_maskblend(self, other, mask)

    def __repr__(self):
        return f"TerrainNode(module={self._module._name!r})"
