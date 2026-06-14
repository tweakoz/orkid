###############################################################################
# ork.dflow._lower — Expr → (source_descriptor, chain_stages) lowering.
#
# Pure Python; touches no C++ runtime. Given an Expr tree, returns either:
#   ((kind, ...), [ChainStage, ...])     — chain-lowerable
#   None                                  — chain cannot express; caller must
#                                           fall back to FloatExprModule (#27)
#
# A chain-lowerable expression has exactly ONE non-Const leaf (the source) and
# every internal node is unary-equivalent — either a UnaryFn, a BinOp where
# the non-spine side is Const, or one of the chain-stage helpers (FmodExpr,
# SmoothstepExpr, QuantizeExpr, CurveExpr, PowExpr) whose non-spine args are
# all Const. Vec3Expr, ClampExpr, MinMaxExpr, LerpExpr never lower to a chain.
###############################################################################

import math

from ._expr import (
    Const, ContextRef, EntityRef, ParamRef, PromotedSource, BinOp, UnaryFn,
    PowExpr, ClampExpr, MinMaxExpr, LerpExpr, Vec3Expr,
    FmodExpr, SmoothstepExpr, QuantizeExpr, CurveExpr,
)


class ChainStage:
    """A recorded floatxf stage. `kind` is the stage name; `args` holds the
    constants captured from the source-tree (e.g. {'scale': 4.0})."""
    __slots__ = ("kind", "args")

    def __init__(self, kind, args=None):
        self.kind = kind
        self.args = args if args is not None else {}

    def __repr__(self):
        return f"ChainStage({self.kind!r}, {self.args!r})"


# Source descriptor shapes returned alongside the chain:
#   ("const", float)              — all-constant expression; emitter sets plug value
#   ("ctx", "<dsl_name>")         — registered context variable (e.g. "time",
#                                    "dt", "ptc.unit_age") — emitter resolves
#                                    via the C++ ContextVariableRegistry +
#                                    Python class mirror.
#   ("param", "<name>")           — connect from ParametersModule.<name>
#                                    (deferred; raises in emitter until #26)
#   ("promoted", <output_plug>)   — connect from an already-created typed
#                                    module's output plug (the emitter
#                                    promoted a chain-breaking subtree to its
#                                    own module before re-attempting chain
#                                    lowering on the outer expression)


def lower_to_chain(expr):
    """Attempt to lower `expr` to (source_descriptor, [ChainStage]).
    Returns None if the expression cannot be a single-input chain."""

    # Pre-walk rewrites — produce a chain-friendlier tree before the spine
    # analysis runs. cos becomes sin(x + π/2), so it inherits chain-lowering
    # for free (bias stage + sine stage already exist).
    expr = _rewrite(expr)

    # All-constant: trivial. The emitter writes the value directly to the plug.
    if isinstance(expr, Const):
        return (("const", expr.value), [])

    leaves = _collect_non_const_leaves(expr)
    if len(leaves) != 1:
        return None   # multi-source — fallback path

    source = leaves[0]
    if isinstance(source, ContextRef):
        descriptor = ("ctx", source.dsl_name)
    elif isinstance(source, EntityRef):
        # Parametric counterpart: identifies which EntityRef module to
        # use (by entity_name) AND which output plug to wire (the
        # SRT component). Bindings emitter resolves it the same way
        # ("ctx", ...) is resolved but routes through a name-keyed
        # singleton instead of the global ContextVariableRegistry.
        descriptor = ("entity", source.entity_name, source.component)
    elif isinstance(source, ParamRef):
        descriptor = ("param", source.name)
    elif isinstance(source, PromotedSource):
        descriptor = ("promoted", source.output_plug)
    else:
        return None   # unknown leaf kind

    stages = _walk_stages(expr, source)
    if stages is None:
        return None

    return (descriptor, stages)


# ---- pre-walk rewrites ----------------------------------------------------

def _rewrite(node):
    """Apply tree-level rewrites that don't change semantics but produce a
    chain-friendlier shape. Idempotent and side-effect-free; runs once at
    lower-time before any spine analysis. Currently:

      cos(x) → sin(x + π/2)

    New rewrites belong here (e.g. tan, exp via curve lookup, etc.)."""

    if isinstance(node, UnaryFn):
        if node.fn == "cos":
            return UnaryFn("sin", _rewrite(BinOp("+", node.arg, Const(math.pi / 2.0))))
        return UnaryFn(node.fn, _rewrite(node.arg))

    if isinstance(node, BinOp):
        return BinOp(node.op, _rewrite(node.lhs), _rewrite(node.rhs))
    if isinstance(node, PowExpr):
        return PowExpr(_rewrite(node.x), _rewrite(node.k))
    if isinstance(node, ClampExpr):
        return ClampExpr(_rewrite(node.x), _rewrite(node.lo), _rewrite(node.hi))
    if isinstance(node, MinMaxExpr):
        return MinMaxExpr(node.op, _rewrite(node.a), _rewrite(node.b))
    if isinstance(node, LerpExpr):
        return LerpExpr(_rewrite(node.a), _rewrite(node.b), _rewrite(node.t))
    if isinstance(node, Vec3Expr):
        out = Vec3Expr.__new__(Vec3Expr)
        out.x = _rewrite(node.x)
        out.y = _rewrite(node.y)
        out.z = _rewrite(node.z)
        return out
    if isinstance(node, FmodExpr):
        return FmodExpr(_rewrite(node.x), _rewrite(node.k))
    if isinstance(node, SmoothstepExpr):
        return SmoothstepExpr(_rewrite(node.x), _rewrite(node.edge0), _rewrite(node.edge1))
    if isinstance(node, QuantizeExpr):
        return QuantizeExpr(_rewrite(node.x), _rewrite(node.step))
    if isinstance(node, CurveExpr):
        return CurveExpr(_rewrite(node.x), node.curve)

    # Leaves (Const, ContextRef, ParamRef) and anything else — return as-is.
    return node


# ---- traversal helpers ----------------------------------------------------

def _children(node):
    """Return immediate children of a composite Expr node."""
    if isinstance(node, BinOp):       return [node.lhs, node.rhs]
    if isinstance(node, UnaryFn):     return [node.arg]
    if isinstance(node, PowExpr):     return [node.x, node.k]
    if isinstance(node, ClampExpr):   return [node.x, node.lo, node.hi]
    if isinstance(node, MinMaxExpr):  return [node.a, node.b]
    if isinstance(node, LerpExpr):    return [node.a, node.b, node.t]
    if isinstance(node, Vec3Expr):    return [node.x, node.y, node.z]
    if isinstance(node, FmodExpr):    return [node.x, node.k]
    if isinstance(node, SmoothstepExpr): return [node.x, node.edge0, node.edge1]
    if isinstance(node, QuantizeExpr):   return [node.x, node.step]
    if isinstance(node, CurveExpr):   return [node.x]
    return []


def _collect_non_const_leaves(node):
    """Recursively gather all non-Const leaves (ContextRef, ParamRef,
    PromotedSource, …)."""
    if isinstance(node, Const):
        return []
    if isinstance(node, (ContextRef, EntityRef, ParamRef, PromotedSource)):
        return [node]
    leaves = []
    for child in _children(node):
        leaves.extend(_collect_non_const_leaves(child))
    return leaves


def _contains(node, target):
    """Does the subtree rooted at `node` contain `target` (identity match)?"""
    if node is target:
        return True
    for child in _children(node):
        if _contains(child, target):
            return True
    return False


def _walk_stages(root, source_leaf):
    """Walk from `root` down to `source_leaf`, collecting chain stages in
    execution order (source-side first, consumer-side last)."""

    # Identify the spine: at each composite, exactly one child must contain
    # the source leaf. The other children must be Const.
    spine = []           # ordered top → bottom
    current = root
    while current is not source_leaf:
        children = _children(current)
        spine_child = None
        for child in children:
            if _contains(child, source_leaf):
                if spine_child is not None:
                    return None   # source reachable via two paths — multi-input
                spine_child = child
        if spine_child is None:
            return None
        spine.append((current, spine_child))
        current = spine_child

    # Emit stages bottom-up so the resulting list runs source → consumer.
    # Each node may emit one OR MORE stages (e.g. `k - spine` needs two).
    stages = []
    for node, spine_child in reversed(spine):
        emitted = _node_to_stages(node, spine_child)
        if emitted is None:
            return None
        stages.extend(emitted)
    return stages


def _node_to_stages(node, spine_child):
    """Convert a single tree node to its chain-stage equivalent(s). Returns a
    list of ChainStage in source→consumer order, or None if the node has no
    chain expression. `spine_child` is the child carrying the source value;
    other children must be Const."""

    if isinstance(node, BinOp):
        # The non-spine operand is the constant. Commutative ops accept either
        # side; the asymmetric cases (`k - spine`) get explicit handling.
        if node.lhs is spine_child:
            const_side = node.rhs
            spine_is_left = True
        else:
            const_side = node.lhs
            spine_is_left = False

        if not isinstance(const_side, Const):
            return None
        k = const_side.value

        if node.op == "+":
            return [ChainStage("bias", {"bias": k})]
        if node.op == "*":
            return [ChainStage("scale", {"scale": k})]
        if node.op == "-":
            if spine_is_left:
                # spine - k  →  bias(-k)
                return [ChainStage("bias", {"bias": -k})]
            # k - spine  →  -spine + k  →  scale(-1) then bias(k)
            return [ChainStage("scale", {"scale": -1.0}),
                    ChainStage("bias",  {"bias":  k})]
        if node.op == "/":
            if spine_is_left:
                if k == 0.0:
                    return None
                # spine / k  →  scale(1/k)
                return [ChainStage("scale", {"scale": 1.0 / k})]
            return None   # k / spine has no chain (no reciprocal stage)
        return None

    if isinstance(node, UnaryFn):
        if node.fn == "sin":  return [ChainStage("sine")]
        if node.fn == "abs":  return [ChainStage("abs")]
        # cos is rewritten to sin(x + π/2) by _rewrite() before this runs,
        # so we never see UnaryFn("cos") here. sqrt has no chain stage —
        # falls through to None (handled by FloatExprModule fallback).
        return None

    if isinstance(node, PowExpr):
        if node.x is spine_child and isinstance(node.k, Const):
            return [ChainStage("power", {"power": node.k.value})]
        return None

    if isinstance(node, FmodExpr):
        if node.x is spine_child and isinstance(node.k, Const):
            return [ChainStage("mod", {"mod": node.k.value})]
        return None

    if isinstance(node, SmoothstepExpr):
        if (node.x is spine_child
                and isinstance(node.edge0, Const)
                and isinstance(node.edge1, Const)):
            return [ChainStage("smoothstep",
                               {"edge0": node.edge0.value, "edge1": node.edge1.value})]
        return None

    if isinstance(node, QuantizeExpr):
        if node.x is spine_child and isinstance(node.step, Const):
            return [ChainStage("quantize", {"step": node.step.value})]
        return None

    if isinstance(node, CurveExpr):
        if node.x is spine_child:
            return [ChainStage("curve", {"curve": node.curve})]
        return None

    # Vec3Expr / ClampExpr / MinMaxExpr / LerpExpr — never a single chain stage.
    return None
