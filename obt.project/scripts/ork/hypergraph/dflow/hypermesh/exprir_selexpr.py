###############################################################################
# ork.hypergraph.dflow.hypermesh.exprir_selexpr — the hypermesh SelExpr adoption of
# the shared ExprIR (JUL13 DFLOW E2.5, adjudicated 2026-07-18; Q6 = the SelExpr tree is
# CAPTURED as a canonical ExprIR TREE, replacing the throw-away-the-tree behaviour where
# only the flattened GLSL survived). The GLSL (`_predicate` / `_dist_pred` / ...) stays the
# EVAL form the op shader compiles; this module captures the SAME expression as an ExprIR
# tree — the STORAGE-form IDENTITY (the reflected `*_tree` fields on SelectData /
# ExtrudeFacesData) so an editor can re-open the author's expression and the cook hash keys
# off the stable tree instead of the codegen-churny GLSL.
#
# This mirrors ptex3d/exprir_surface.py exactly: a tagged-op table auto-derives one
# ExprContext ("hypermesh.selexpr"); capture() lowers a selexpr.SelExpr (whose nodes now
# carry a symbolic `_meta` tag — Q1, tag at CONSTRUCTION, never reverse-map the GLSL emit)
# into a validated ExprIR tree; capture_json() is the byte-stable string the C++ `*_tree`
# reflected field stores. A v1 SelExpr text pretty-printer / re-parser is DEFERRED (the
# S.* / &|^~ author syntax is richer than the generic printer, as ptex3d notes) — the S6
# contract is the JSON tree round-trip + identity, not text round-trip.
###############################################################################

from ... import exprir as _x
from . import selexpr as _se


class SelExprIRError(_x.ExprIRError):
    """A SelExpr could not be captured into the hypermesh.selexpr ExprIR — an untagged node
    (no symbolic `_meta`), or an op with no spec row. Loud by design (never a silent hole)."""


###############################################################################
# op-spec table — THE tagged-op registry the ExprContext is auto-derived from. Every entry
# is a canonical op spelling a selexpr node's `_meta = ("op", name)` produces. `kind` is the
# author-syntax rendering hint (infix/unary/call) for a future pretty-printer; v1 validates
# + JSON-round-trips only.
###############################################################################

def _specs():
    out = {}

    def add(name, kind, arity, symbol=None):
        out[name] = (name, kind, arity, symbol)

    add("add", "infix", 2, "+")
    add("sub", "infix", 2, "-")
    add("mul", "infix", 2, "*")
    add("div", "infix", 2, "/")
    add("neg", "unary", 1, "-")
    add("not01", "call", 1)      # boolean NOT  (1.0 - x)
    add("xor01", "call", 2)      # boolean XOR  abs(a - b) over 0/1 weights
    for nm in ("sin", "cos", "fract", "sqrt", "abs", "length_of",
               "swz_x", "swz_y", "swz_z"):
        add(nm, "call", 1)
    for nm in ("min", "max", "step", "pow", "mod", "dot"):
        add(nm, "call", 2)
    for nm in ("smoothstep", "clamp", "mix", "select01"):
        add(nm, "call", 3)
    add("vec3", "call", 3)       # vec3(x, y, z) — vexpr / baked vec3 constant
    add("tag", "call", 1)        # S.tag(bit)  -> Call("tag", Const(bit))
    add("ftag", "call", 1)       # S.ftag(bit) -> Call("ftag", Const(bit))
    return out


_OP_SPECS = _specs()

# the ParamRef leaf kinds this context declares (Const is universal). "atom" = a per-element
# selection atom (S.P / S.N / S.area / S.id / ... / S.gid) whose `name` is the atom spelling;
# "plug" = a runtime `param()` (_ParamLeaf) rebound live per frame (the risks-section
# live-poke leaf, PARAM_PLUG).
_ATOM_LEAF = "atom"


def _build_context():
    funcs = []
    for (name, kind, arity, symbol) in _OP_SPECS.values():
        if kind == "infix":
            funcs.append(_x.infix(name, symbol))
        elif kind == "unary":
            funcs.append(_x.unary(name, symbol))
        else:
            funcs.append(_x.fn(name, arity))
    return _x.register_context(_x.ExprContext(
        "hypermesh.selexpr", functions=funcs,
        leaves=[_x.LeafSpec(_ATOM_LEAF),                    # open: any atom spelling -> its name
                _x.LeafSpec(_x.PARAM_PLUG, "param")],       # runtime param() live-plug rebind
        doc="hypermesh per-element selection / extrude-field scalar+vec3 expression vocabulary"))


SELEXPR_CTX = _build_context()


###############################################################################
# capture: selexpr.SelExpr -> ExprIR tree
###############################################################################

def capture(expr):
    """Lower a selexpr.SelExpr into a validated hypermesh.selexpr ExprIR tree. Loud
    (SelExprIRError) on an untagged node. A plain scalar / vec3 (not a SelExpr) captures as a
    Const / vec3-Call, matching selexpr._coerce (extrude fields accept bare constants)."""
    tree = _capture(_se._coerce(expr) if not isinstance(expr, _se.SelExpr) else expr)
    SELEXPR_CTX.validate(tree)          # belt-and-suspenders: prove the vocabulary
    return tree


def capture_json(expr):
    """capture(expr) serialized to the canonical (sorted, byte-stable) ExprIR JSON — the
    string the SelectData/ExtrudeFacesData `*_tree` reflected fields store."""
    return _x.encode_json(capture(expr))


def _capture(node):
    meta = getattr(node, "_meta", None)
    if meta is None:
        raise SelExprIRError(
            "hypermesh.selexpr: a SelExpr node carries no symbolic tag (_meta) — it cannot be "
            "captured into ExprIR. Every selexpr construction site must tag its node at "
            "construction (Q1); reverse-mapping the GLSL emit is forbidden.")
    kind = meta[0]
    if kind == "const":
        return _x.Const(float(meta[1]))
    if kind == "vconst":
        x, y, z = meta[1]
        return _x.Call("vec3", _x.Const(float(x)), _x.Const(float(y)), _x.Const(float(z)))
    if kind == "atom":
        return _x.ParamRef(_ATOM_LEAF, meta[1])
    if kind == "plug":
        return _x.ParamRef(_x.PARAM_PLUG, meta[1])
    if kind == "tagbit":
        return _x.Call("tag", _x.Const(int(meta[1])))
    if kind == "ftagbit":
        return _x.Call("ftag", _x.Const(int(meta[1])))
    if kind == "op":
        name = meta[1]
        if name not in _OP_SPECS:
            raise SelExprIRError(
                "hypermesh.selexpr: op %r is tagged but has no spec row — add it to _OP_SPECS." % (name,))
        return _x.Call(name, *[_capture(c) for c in node._children])
    raise SelExprIRError("hypermesh.selexpr: unknown selexpr _meta tag %r" % (kind,))
