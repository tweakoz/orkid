###############################################################################
# ork.hypergraph.ptex3d.exprir_surface — the ptex3d SurfNode <-> shared ExprIR bridge
# (JUL13 DFLOW E2.5 S5). Lowers a ptex3d scalar-bake SurfNode DAG (dsl.py) into the
# family-neutral ExprIR (ork.hypergraph.exprir: sealed {Const, ParamRef, Call}) and
# renders it back to a ptex3d DSL SOURCE STRING that re-evals — in the T.expr eval
# namespace {"ctx": SurfaceCtx(), "P": P} — to a STRUCTURALLY IDENTICAL SurfNode, so
# emit_compute_field produces BYTE-IDENTICAL shadertext (the round-trip contract the
# terrain pywriter + cook-hash rely on).
#
# This REPLACES the removed compiled-shadertext-blob escape hatch: an authored hfbake /
# hfmask / hfdisplacement / expr_field node now captures its SurfNode tree here and the
# writer re-emits real DSL from it — the author's op vocabulary, never opaque GLSL.
#
# REPRESENTATION within the sealed {Const, ParamRef, Call} kinds (Q1: symbolic op tags,
# never a raw `_tmpl`):
#   float literal        -> Const(value)                         # numeric leaf
#   vecN literal (rare)  -> Call("veclit", Const, ...)           # a Python tuple arg
#   ctx atom (ctx.P_...) -> ParamRef("ctx", <glsl atom>)         # opos/onrm/uv/in{k}/...
#   swizzle  a.xyz / .f1 -> Call("swizzle", <a>, Const("<comp>"))# comp rides a string Const
#   P.<op>(...)          -> Call("<op>", <args...>[, <meta Const>])  # op._name tag
#   a + b  (operators)   -> Call("add"/"sub"/"mul"/"div", a, b)  # the binop tags
#   -a                   -> Call("neg", a)
# A ptex3d op whose octave-loop bound (a STRUCTURAL A8 int) bakes into `_tmpl`
# (fbm/fbm_aa) carries it in `op._meta`; capture appends it as a trailing Const arg
# recovered at render (octaves= keyword). The `ptex3d.surface` ExprContext (registered
# below) is AUTO-DERIVED from the op-spec table `_OP_SPECS` — a new dsl `_Ops` entry that
# adds a spec row is automatically in the vocabulary; an UNTAGGED op (the raw `P.func`
# escape hatch, `op._name is None`) fails LOUD rather than smuggling a template into the IR.
#
# ctx.param / ctx.tex leaves are NOT representable in v1 (they need a string-valued /
# sampler leaf beyond a bake's position-only vocabulary) — capture rejects them loudly;
# no corpus bake uses them (a bake is a function of position + noise only).
###############################################################################

from .. import exprir as _x
from .dsl import (
    SurfNode, Const as _SConst, CtxRef as _SCtxRef, Param as _SParam,
    Op as _SOp, Swizzle as _SSwizzle, TexSample as _STexSample, _VEC)


class Ptex3dExprError(_x.ExprIRError):
    """A ptex3d SurfNode could not be lowered to (or an ExprIR tree rendered from) the
    ptex3d.surface vocabulary — an untagged op, a non-portable leaf, or a malformed tree.
    Loud by design (ops self-defend): the writer never emits an opaque compiled blob."""


###############################################################################
# op-spec table — THE tagged-op registry the ExprContext is auto-derived from.
###############################################################################

class _OpSpec:
    __slots__ = ("name", "kind", "arity", "symbol", "meta_keys", "bundle")

    def __init__(self, name, kind, arity, symbol=None, meta_keys=(), bundle=None):
        self.name = name          # canonical DSL op tag (== dsl Op._name)
        self.kind = kind          # method | infix | unary | vec | fbm | fbm_aa | bundle
        self.arity = arity        # captured-Call arity (incl. trailing meta Consts); -1 variadic
        self.symbol = symbol      # infix/unary operator spelling
        self.meta_keys = tuple(meta_keys)   # op._meta keys appended as trailing Const args
        self.bundle = bundle      # struct-member -> Bundle-field reverse map (bundle ops)


# Bundle-field reverse maps: dsl `Bundle(node, {field: struct_member})` swizzles a struct
# MEMBER, so the SurfNode swizzle comp is the member; render maps it back to the field name.
_VORONOI_FIELDS = {"f1": "f1", "edge": "edge", "fwedge": "fwedge", "cellA": "cell", "cellB": "cell2"}
_HEXGRID_FIELDS = {"x": "edge", "y": "id"}
_SPHERECELLS_FIELDS = {"x": "id", "y": "edge", "z": "f1"}


def _specs():
    out = {}

    def add(name, kind, arity, **kw):
        out[name] = _OpSpec(name, kind, arity, **kw)

    add("add", "infix", 2, symbol="+")
    add("sub", "infix", 2, symbol="-")
    add("mul", "infix", 2, symbol="*")
    add("div", "infix", 2, symbol="/")
    add("neg", "unary", 1, symbol="-")
    for nm in ("sin", "cos", "atan", "abs", "floor", "fract", "sqrt",
               "fwidth", "dFdx", "dFdy", "normalize", "saturate", "noise", "length"):
        add(nm, "method", 1)
    for nm in ("atan2", "step", "pow", "mod", "min", "max", "dot", "cross"):
        add(nm, "method", 2)
    for nm in ("mix", "clamp", "smoothstep"):
        add(nm, "method", 3)
    for nm in ("vec2", "vec3", "vec4"):
        add(nm, "vec", -1)
    add("fbm", "fbm", 2, meta_keys=("octaves",))
    add("fbm_aa", "fbm_aa", 3, meta_keys=("octaves",))
    add("voronoi", "bundle", 1, bundle=_VORONOI_FIELDS)
    add("hexgrid", "bundle", 1, bundle=_HEXGRID_FIELDS)
    add("spherecells", "bundle", 2, bundle=_SPHERECELLS_FIELDS)
    return out


_OP_SPECS = _specs()

# ctx atom (dsl CtxRef._glsl) -> canonical author spelling. Aliases (N/NW, N_object/NO)
# collapse onto their shared glsl atom, so re-eval reconstructs the identical CtxRef.
_CTX_RENDER = {
    "opos": "ctx.P_object", "wpos": "ctx.P", "wnrm": "ctx.N", "onrm": "ctx.N_object",
    "vnrm": "ctx.NV", "uv": "ctx.uv", "cd": "ctx.Cd", "eye": "ctx.eye",
    "footprint": "ctx.footprint", "extent_m": "ctx.extent_m",
    # the ublk_sun atoms carry their swizzle in the atom name (the block member is a
    # vec4), so they render back to the vec3/float ctx accessors, not to a swizzle.
    "sun_dir.xyz": "ctx.sun_dir", "sun_dir.w": "ctx.has_sun",
    "sun_color.xyz": "ctx.sun_color", "sun_color.w": "ctx.sun_intensity",
    "sky_ambient.x": "ctx.sky_luminance",
}

_SWIZZLE_NAME = "swizzle"
_VECLIT_NAME = "veclit"
_CTX_LEAF = "ctx"


###############################################################################
# the registered ExprContext (auto-derived from _OP_SPECS). Used to VALIDATE a captured
# tree (fail loud on an out-of-vocabulary op); the ptex3d AUTHOR SYNTAX (P./ctx./postfix
# swizzle) is richer than the generic infix/call/leaf printer, so rendering lives here.
###############################################################################

def _build_context():
    funcs = []
    for spec in _OP_SPECS.values():
        if spec.kind == "infix":
            funcs.append(_x.infix(spec.name, spec.symbol))
        elif spec.kind == "unary":
            funcs.append(_x.unary(spec.name, spec.symbol))
        else:
            funcs.append(_x.fn(spec.name, spec.arity))
    funcs.append(_x.fn(_SWIZZLE_NAME, 2))
    funcs.append(_x.fn(_VECLIT_NAME, -1))
    return _x.register_context(_x.ExprContext(
        "ptex3d.surface", functions=funcs,
        leaves=[_x.LeafSpec(_CTX_LEAF)],
        doc="ptex3d procedural-surface / terrain-bake scalar expression vocabulary"))


SURFACE_CTX = _build_context()


###############################################################################
# capture: SurfNode DAG -> ExprIR tree
###############################################################################

def capture(node):
    """Lower a ptex3d SurfNode into a validated ptex3d.surface ExprIR tree. Loud
    (Ptex3dExprError) on an untagged op or a leaf with no bake vocabulary form."""
    tree = _capture(node)
    SURFACE_CTX.validate(tree)          # belt-and-suspenders: prove the vocabulary
    return tree


def capture_json(node):
    """capture(node) serialized to the canonical (sorted, byte-stable) ExprIR JSON —
    the string the ExprModuleData._expr_tree reflected field stores."""
    return _x.encode_json(capture(node))


def _capture(node):
    if isinstance(node, _SConst):
        return _capture_const(node)
    if isinstance(node, _SCtxRef):
        return _x.ParamRef(_CTX_LEAF, node._glsl)
    if isinstance(node, _SSwizzle):
        return _x.Call(_SWIZZLE_NAME, _capture(node._src), _x.Const(node._comp))
    if isinstance(node, _SParam):
        raise Ptex3dExprError(
            "ptex3d.surface: ctx.param(%r) is not representable in ExprIR v1 — a bake "
            "expression is a function of position + noise; use a plain constant." % (node._pname,))
    if isinstance(node, _STexSample):
        raise Ptex3dExprError(
            "ptex3d.surface: ctx.tex(%r) (a texture sample) is not representable in ExprIR v1 "
            "for a terrain bake." % (node._sname,))
    if isinstance(node, _SOp):
        return _capture_op(node)
    raise Ptex3dExprError("ptex3d.surface: cannot capture SurfNode %r" % (node,))


def _capture_op(node):
    nm = node._name
    if nm is None:
        raise Ptex3dExprError(
            "ptex3d.surface: an op carries no symbolic tag (raw-template P.func escape hatch, "
            "tmpl=%r) — the ExprIR requires a canonical op name; compose core P.* ops instead."
            % (node._tmpl,))
    spec = _OP_SPECS.get(nm)
    if spec is None:
        raise Ptex3dExprError(
            "ptex3d.surface: op %r is tagged but has no spec row — add it to _OP_SPECS." % (nm,))
    args = [_capture(a) for a in node._args]
    for mk in spec.meta_keys:
        if mk not in node._meta:
            raise Ptex3dExprError(
                "ptex3d.surface: op %r is missing structural meta %r." % (nm, mk))
        args.append(_x.Const(node._meta[mk]))
    return _x.Call(nm, *args)


def _capture_const(node):
    t = node._type
    if t == "float":
        return _x.Const(float(node._glsl))
    if t in ("vec2", "vec3", "vec4"):
        return _x.Call(_VECLIT_NAME, *[_x.Const(c) for c in _parse_vec_literal(node._glsl, t)])
    raise Ptex3dExprError(
        "ptex3d.surface: constant of type %r (%r) is not representable in ExprIR v1 (scalar-bake "
        "expressions are float-valued; vecN literals are supported only as op arguments)." % (t, node._glsl))


def _parse_vec_literal(glsl, gtype):
    n = _WIDTH[gtype]
    inner = glsl[glsl.index("(") + 1:glsl.rindex(")")]
    comps = [float(c) for c in inner.split(",")]
    if len(comps) != n:
        raise Ptex3dExprError("ptex3d.surface: malformed %s literal %r" % (gtype, glsl))
    return comps


_WIDTH = {"vec2": 2, "vec3": 3, "vec4": 4}


###############################################################################
# render: ExprIR tree -> ptex3d DSL source string (the inverse of capture)
###############################################################################

def render(node_or_json):
    """Render a ptex3d.surface ExprIR tree (a node OR its JSON) back to the DSL source
    string. eval(render(x), {"ctx": SurfaceCtx(), "P": P}) reconstructs a SurfNode that
    emits byte-identical shadertext to the one x was captured from."""
    node = _x.decode_json(node_or_json) if isinstance(node_or_json, str) else node_or_json
    return _render(node)


def _render(node):
    if isinstance(node, _x.Const):
        return _render_const(node)
    if isinstance(node, _x.ParamRef):
        return _render_leaf(node)
    if isinstance(node, _x.Call):
        return _render_call(node)
    raise Ptex3dExprError("ptex3d.surface: cannot render %r" % (node,))


def _render_const(node):
    v = node.value
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        raise Ptex3dExprError("ptex3d.surface: non-numeric Const %r in render position" % (v,))
    # repr(float) round-trips exactly; dsl._fmt_float(float(literal)) reproduces the original
    # GLSL literal regardless of the int/float spelling (it normalizes via float()).
    return repr(float(v))


def _render_leaf(node):
    if node.kind != _CTX_LEAF:
        raise Ptex3dExprError("ptex3d.surface: unknown leaf kind %r" % (node.kind,))
    atom = node.name
    if atom in _CTX_RENDER:
        return _CTX_RENDER[atom]
    if isinstance(atom, str) and atom.startswith("in") and atom[2:].isdigit():
        return "ctx.input(%d)" % int(atom[2:])
    raise Ptex3dExprError("ptex3d.surface: unknown ctx atom %r" % (atom,))


def _render_call(node):
    nm = node.name
    if nm == _SWIZZLE_NAME:
        return _render_swizzle(node)
    if nm == _VECLIT_NAME:
        return "(%s)" % ", ".join(_render(a) for a in node.args)
    spec = _OP_SPECS.get(nm)
    if spec is None:
        raise Ptex3dExprError("ptex3d.surface: cannot render op %r (no spec)" % (nm,))
    if spec.kind == "infix":
        return "(%s %s %s)" % (_render(node.args[0]), spec.symbol, _render(node.args[1]))
    if spec.kind == "unary":
        return "(%s%s)" % (spec.symbol, _render(node.args[0]))
    if spec.kind == "vec":
        return "P.%s(%s)" % (nm, ", ".join(_render(a) for a in node.args))
    if spec.kind == "fbm":
        p, oct_c = node.args
        return "P.fbm(%s, octaves=%d)" % (_render(p), int(oct_c.value))
    if spec.kind == "fbm_aa":
        p, aa, oct_c = node.args
        return "P.fbm_aa(%s, octaves=%d, aa=%s)" % (_render(p), int(oct_c.value), _render(aa))
    if spec.kind == "bundle":
        # a bundle op standalone yields a Bundle (not a SurfNode) — valid only under a swizzle,
        # which owns the `.field` suffix. Render the method call; the swizzle appends the field.
        return "P.%s(%s)" % (nm, ", ".join(_render(a) for a in node.args))
    # method
    return "P.%s(%s)" % (nm, ", ".join(_render(a) for a in node.args))


def _render_swizzle(node):
    src, comp_c = node.args
    comp = comp_c.value
    if isinstance(src, _x.Call):
        spec = _OP_SPECS.get(src.name)
        if spec is not None and spec.kind == "bundle":
            field = spec.bundle.get(comp)
            if field is None:
                raise Ptex3dExprError(
                    "ptex3d.surface: %r has no bundle field for swizzle %r" % (src.name, comp))
            return "%s.%s" % (_render_call(src), field)
    return "(%s).%s" % (_render(src), comp)
