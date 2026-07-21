###############################################################################
# ork.hypergraph.exprir — the shared EXPRESSION IR (JUL13 DFLOW E2.5, adjudicated
# 2026-07-18). ONE tree shape + ONE vocabulary mechanism for every dataflow family's
# author-time scalar/predicate expressions, replacing the per-family bespoke trees
# (terrain _ParamExpr / _IterExpr today; SelExpr / ptex3d / particles as they adopt).
#
# THREE node kinds (a sealed enumeration — Q3):
#   Const(value, unit=None)   a numeric literal. `unit` is an E0 typed-literal tag
#                             (units.UNIT_TAGS) whose AUTHOR form is `meters(2000)`.
#   ParamRef(kind, name)      a reference to an external binding. `kind` selects the
#                             binding NAMESPACE (the risks-section leaf distinction):
#                               PARAM_DOC  — a document-level param symbol resolved at
#                                            elaborate time (E0 _ParamExpr param).
#                               PARAM_PLUG — a runtime live-plug rebind (SelExpr param()).
#                             A family MAY declare additional leaf kinds via its context
#                             (terrain declares "index" for the L.i loop-index leaf, whose
#                             `name` carries the loop reference rather than a symbol name).
#   Call(name, *args)         a symbolic op-tag applied to child nodes (Q1 — symbolic
#                             tags, never a raw template string). `name` is the CANONICAL
#                             op spelling; `args` are ExprIR nodes.
#
# VOCABULARY keys off NAMED CONTEXTS (adjudication supersedes Q7): a family declares one
# ExprContext per author-time expression ENVIRONMENT it exposes (e.g. particles will
# declare a color-expression context and a force-expression context — same family, different
# vocabularies + type environments). A context whitelists the legal Call names (functions)
# and ParamRef kinds (leaves); Const is universal. Validation, the canonical pretty-printer
# (tree -> author text) and the whitelisted-ast text parser (S2) all key off a context, so an
# unknown Call / bad arity / undeclared leaf FAILS LOUDLY naming the offending context.
#
# The IR itself is family-neutral and engine-free (imports only the sibling `units` module,
# also pure-python). A family owns its own EVALUATOR (the IR is dumb data — resolving a
# ParamRef needs the family's params table / plug environment) and MAY keep its own wire
# format (terrain's _enc_param_expr / _enc_iter stay byte-identical until the coordinated
# salt bump); `encode`/`decode` below are the family-neutral JSON form for new consumers.
###############################################################################

import ast as _ast
import json as _json

from . import units as _units


###############################################################################
# errors
###############################################################################

class ExprIRError(ValueError):
    """Base for every ExprIR failure. Loud by design (ops self-defend) — a malformed
    tree, an unknown context, or an out-of-vocabulary construct never passes silently."""


class ExprSignatureError(ExprIRError):
    """A tree violated a CONTEXT: an unknown Call name, a wrong arity, or an undeclared
    ParamRef leaf kind. The message names the context (adjudication: contexts are the unit
    of vocabulary, so a validation error is reported against its context, not a family)."""


class ExprParseError(ExprIRError):
    """The whitelisted-ast text parser (S2) rejected source: a syntax error, a construct
    outside the whitelist (attribute chains, comprehensions, lambdas, subscripts, dunder
    names), or a token outside the context vocabulary. Positioned + names the context."""


###############################################################################
# canonical ParamRef leaf-kind constants. "doc" vs "plug" is the risks-section
# distinction (an elaborate-time document symbol vs a runtime live-plug rebind) — the two
# must never conflate (re-elaborate vs live-poke). Families may declare MORE kinds.
###############################################################################

PARAM_DOC = "doc"      # document-level param symbol (E0 _ParamExpr param; elaborate-time)
PARAM_PLUG = "plug"    # runtime live-plug rebind (SelExpr param()/_ParamLeaf; per-frame)


###############################################################################
# nodes
###############################################################################

class ExprNode:
    """Marker base for the three node kinds (isinstance dispatch + a shared repr contract).
    Sealed: no family subclasses a node — a family extends the VOCABULARY (a context), never
    the node set."""
    __slots__ = ()


class Const(ExprNode):
    __slots__ = ("value", "unit")

    def __init__(self, value, unit=None):
        if unit is not None and unit not in _units.UNIT_TAGS:
            raise ExprIRError(
                "Const unit tag %r outside unit vocabulary v1 = %s"
                % (unit, sorted(_units.UNIT_TAGS)))
        self.value = value
        self.unit = unit

    def __eq__(self, o):
        return isinstance(o, Const) and o.value == self.value and o.unit == self.unit

    def __hash__(self):
        return hash(("const", self.value, self.unit))

    def __repr__(self):
        return "Const(%r, %r)" % (self.value, self.unit) if self.unit else "Const(%r)" % (self.value,)


class ParamRef(ExprNode):
    __slots__ = ("kind", "name")

    def __init__(self, kind, name):
        self.kind = kind
        self.name = name

    def __eq__(self, o):
        return isinstance(o, ParamRef) and o.kind == self.kind and o.name == self.name

    def __hash__(self):
        return hash(("ref", self.kind, self.name))

    def __repr__(self):
        return "ParamRef(%r, %r)" % (self.kind, self.name)


class Call(ExprNode):
    __slots__ = ("name", "args")

    def __init__(self, name, *args):
        self.name = name
        self.args = tuple(args)

    def __eq__(self, o):
        return isinstance(o, Call) and o.name == self.name and o.args == self.args

    def __hash__(self):
        return hash(("call", self.name, self.args))

    def __repr__(self):
        return "Call(%r%s)" % (self.name, "".join(", " + repr(a) for a in self.args))


###############################################################################
# context vocabulary specs
###############################################################################

class FuncSpec:
    """One whitelisted Call name in a context: its CANONICAL op spelling, arity, and
    author-syntax rendering. `syntax`:
      "infix" — a binary operator; `symbol` is the spelling ('+' etc.) -> `(a sym b)`.
      "unary" — a prefix operator; `symbol` is the spelling ('-') -> `sym a`.
      "call"  — function-call syntax -> `name(a, b, ...)`.
    `arity` is fixed; -1 = variadic (call-syntax only)."""
    __slots__ = ("name", "arity", "syntax", "symbol", "doc")

    def __init__(self, name, arity, syntax="call", symbol=None, doc=""):
        self.name = name
        self.arity = arity
        self.syntax = syntax
        self.symbol = symbol
        self.doc = doc


def infix(name, symbol, doc=""):
    return FuncSpec(name, 2, "infix", symbol, doc)


def unary(name, symbol, doc=""):
    return FuncSpec(name, 1, "unary", symbol, doc)


def fn(name, arity, doc=""):
    return FuncSpec(name, arity, "call", None, doc)


class LeafSpec:
    """One whitelisted ParamRef kind in a context. `spelling`:
      None — an OPEN leaf: ANY bare identifier maps to this kind, its `name` = the identifier
             (terrain's document params). At most one open leaf per context.
      str  — a FIXED leaf: only that exact identifier maps to this kind, and the ParamRef
             `name` is supplied by the FAMILY at build time (terrain's 'i' -> the loop object);
             a text parse yields `name=None` (the binding is external to the source text)."""
    __slots__ = ("kind", "spelling", "doc")

    def __init__(self, kind, spelling=None, doc=""):
        self.kind = kind
        self.spelling = spelling
        self.doc = doc


class ExprContext:
    """A NAMED author-time expression environment (the unit of vocabulary). A family
    registers one per expression surface it exposes; validation / pretty-print / parse all
    take a context so a construct outside its whitelist fails loudly against it.

    functions : iterable of FuncSpec — the whitelisted Call names.
    leaves    : iterable of LeafSpec — the whitelisted ParamRef kinds (Const is universal).
    allow_*   : opt-ins for the richer constructs a future context needs (ptex3d `P.*`
                namespaces, particle subscripts). v1 contexts leave them off, so the parser
                rejects attribute/subscript/comprehension by default."""

    def __init__(self, name, functions=(), leaves=(), doc="",
                 allow_attribute=False, allow_subscript=False, allow_comprehension=False):
        self.name = name
        self.doc = doc
        self.allow_attribute = allow_attribute
        self.allow_subscript = allow_subscript
        self.allow_comprehension = allow_comprehension

        self._funcs = {}
        for f in functions:
            if f.name in self._funcs:
                raise ExprIRError("context %r: duplicate function %r" % (name, f.name))
            self._funcs[f.name] = f

        self._leaves = {}          # kind -> LeafSpec
        self._fixed_leaves = {}    # spelling -> LeafSpec
        self._open_leaf = None
        for lf in leaves:
            if lf.kind in self._leaves:
                raise ExprIRError("context %r: duplicate leaf kind %r" % (name, lf.kind))
            self._leaves[lf.kind] = lf
            if lf.spelling is None:
                if self._open_leaf is not None:
                    raise ExprIRError(
                        "context %r: two OPEN leaves (%r, %r) — at most one identifier-catch-all"
                        % (name, self._open_leaf.kind, lf.kind))
                self._open_leaf = lf
            else:
                self._fixed_leaves[lf.spelling] = lf

        self._infix = {f.symbol: f for f in self._funcs.values() if f.syntax == "infix"}
        self._unary = {f.symbol: f for f in self._funcs.values() if f.syntax == "unary"}

    # ---- introspection --------------------------------------------------------

    def func(self, name):
        f = self._funcs.get(name)
        if f is None:
            raise ExprSignatureError(
                "context %r: unknown function %r; whitelist = %s"
                % (self.name, name, sorted(self._funcs)))
        return f

    def has_leaf_kind(self, kind):
        return kind in self._leaves

    def functions(self):
        return dict(self._funcs)

    def leaves(self):
        return dict(self._leaves)

    # ---- pretty-print helper --------------------------------------------------

    def render_leaf(self, node):
        """Author spelling of a ParamRef in this context: a fixed leaf renders its declared
        spelling ('i'); an open leaf renders its `name` (the param identifier)."""
        lf = self._leaves.get(node.kind)
        if lf is None:
            raise ExprSignatureError(
                "context %r: undeclared leaf kind %r; declared = %s"
                % (self.name, node.kind, sorted(self._leaves)))
        return lf.spelling if lf.spelling is not None else str(node.name)

    # ---- parse helper ---------------------------------------------------------

    def leaf_for_identifier(self, ident):
        """The ParamRef a bare identifier maps to in this context, or None (caller raises a
        positioned error). A fixed-leaf match yields name=None (external binding); the open
        leaf catches everything else."""
        lf = self._fixed_leaves.get(ident)
        if lf is not None:
            return ParamRef(lf.kind, None)
        if self._open_leaf is not None:
            return ParamRef(self._open_leaf.kind, ident)
        return None

    # ---- validation -----------------------------------------------------------

    def validate(self, node):
        """Raise ExprSignatureError (naming this context) if `node` uses a Call name / arity
        or ParamRef leaf kind outside this context's vocabulary. Const is always legal."""
        _validate(node, self)


def _validate(node, ctx):
    if isinstance(node, Const):
        if node.unit is not None and node.unit not in _units.UNIT_TAGS:
            raise ExprSignatureError(
                "context %r: Const unit %r outside vocabulary v1" % (ctx.name, node.unit))
        return
    if isinstance(node, ParamRef):
        if not ctx.has_leaf_kind(node.kind):
            raise ExprSignatureError(
                "context %r: leaf kind %r not declared; declared = %s"
                % (ctx.name, node.kind, sorted(ctx.leaves())))
        return
    if isinstance(node, Call):
        f = ctx.func(node.name)
        if f.arity >= 0 and len(node.args) != f.arity:
            raise ExprSignatureError(
                "context %r: %r expects %d args, got %d"
                % (ctx.name, node.name, f.arity, len(node.args)))
        for a in node.args:
            _validate(a, ctx)
        return
    raise ExprSignatureError("context %r: not an ExprIR node: %r" % (ctx.name, node))


###############################################################################
# context registry — the base document layer participates generically (context-keyed):
# any family registers its contexts here; a consumer looks one up by name.
###############################################################################

_CONTEXTS = {}


def register_context(ctx):
    """Register a family's ExprContext. Loud on a duplicate name (fail-loud, no silent
    shadowing). Returns the context so a module can `X_CTX = register_context(ExprContext(...))`."""
    if ctx.name in _CONTEXTS:
        raise ExprIRError("expression context %r already registered" % ctx.name)
    _CONTEXTS[ctx.name] = ctx
    return ctx


def get_context(name):
    ctx = _CONTEXTS.get(name)
    if ctx is None:
        raise ExprIRError(
            "no expression context named %r; registered = %s" % (name, sorted(_CONTEXTS)))
    return ctx


def registered_contexts():
    return dict(_CONTEXTS)


###############################################################################
# family-neutral JSON form (stable, sorted, round-trip idempotent). Self-describing
# ("k" tag) so a generic decoder is unambiguous. A family MAY instead keep a bespoke wire
# format (terrain does, byte-identically) — this is the shared form for new consumers.
###############################################################################

def encode(node):
    if isinstance(node, Const):
        d = {"k": "const", "v": node.value}
        if node.unit is not None:
            d["unit"] = node.unit
        return d
    if isinstance(node, ParamRef):
        return {"k": "ref", "kind": node.kind, "name": node.name}
    if isinstance(node, Call):
        return {"k": "call", "name": node.name, "args": [encode(a) for a in node.args]}
    raise ExprIRError("cannot encode non-ExprIR node %r" % (node,))


def decode(d):
    k = d["k"]
    if k == "const":
        return Const(d["v"], d.get("unit"))
    if k == "ref":
        return ParamRef(d["kind"], d["name"])
    if k == "call":
        return Call(d["name"], *[decode(a) for a in d["args"]])
    raise ExprIRError("bad ExprIR node tag %r" % (k,))


def encode_json(node):
    """Canonical JSON text: sorted keys, no incidental whitespace -> byte-stable, so
    encode_json(decode_json(encode_json(x))) is byte-equal."""
    return _json.dumps(encode(node), sort_keys=True, separators=(",", ":"))


def decode_json(s):
    return decode(_json.loads(s))


###############################################################################
# canonical pretty-printer — tree -> the author-syntax text form (the inverse target of the
# S2 parser). Fully parenthesized (every infix wraps) so the text re-parses without
# precedence ambiguity. Numeric formatting matches units._fmt_number (integral floats render
# bare) so a family whose display strings must stay character-identical routes through here.
###############################################################################

def pretty_print(node, ctx):
    if isinstance(node, Const):
        return _fmt_const(node)
    if isinstance(node, ParamRef):
        return ctx.render_leaf(node)
    if isinstance(node, Call):
        f = ctx.func(node.name)
        if f.syntax == "infix":
            if len(node.args) != 2:
                raise ExprSignatureError(
                    "context %r: infix %r needs 2 args, got %d"
                    % (ctx.name, node.name, len(node.args)))
            return "(%s %s %s)" % (pretty_print(node.args[0], ctx), f.symbol,
                                   pretty_print(node.args[1], ctx))
        if f.syntax == "unary":
            if len(node.args) != 1:
                raise ExprSignatureError(
                    "context %r: unary %r needs 1 arg, got %d"
                    % (ctx.name, node.name, len(node.args)))
            return f.symbol + pretty_print(node.args[0], ctx)
        return "%s(%s)" % (node.name, ", ".join(pretty_print(a, ctx) for a in node.args))
    raise ExprIRError("cannot pretty-print non-ExprIR node %r" % (node,))


def _fmt_const(node):
    if node.unit is not None:
        return "%s(%s)" % (node.unit, _units._fmt_number(node.value))
    return _units._fmt_number(node.value)


###############################################################################
# S2 — the whitelisted-ast text parser. ast.parse(text, mode="eval") + a walk that admits
# ONLY the constructs a context declares: numeric literals, the context's leaf identifiers,
# its infix/unary operators, and bare-name Call whitelist (plus the universal E0 unit
# constructors -> Const-with-unit). Everything else — attribute chains, subscripts,
# comprehensions, lambdas, starred/keyword args, dunder names — is a loud, positioned reject.
###############################################################################

_BINOP_AST = {_ast.Add: "+", _ast.Sub: "-", _ast.Mult: "*", _ast.Div: "/"}
_UNARY_AST = {_ast.USub: "-", _ast.UAdd: "+"}


def parse(text, ctx):
    """Parse author text into an ExprIR tree validated against `ctx`. Loud (ExprParseError,
    positioned + naming the context) on any construct outside the context's vocabulary."""
    try:
        mod = _ast.parse(text, mode="eval")
    except SyntaxError as e:
        raise ExprParseError(
            "context %r: cannot parse %r: %s (col %s)"
            % (ctx.name, text, e.msg, e.offset))
    node = _parse_node(mod.body, ctx, text)
    ctx.validate(node)      # belt-and-suspenders: the walk is whitelist-only, but prove it
    return node


def _pos(a):
    return "line %s col %s" % (getattr(a, "lineno", 1), getattr(a, "col_offset", 0))


def _reject(a, ctx, text, why):
    raise ExprParseError(
        "context %r: %s (%s) in %r" % (ctx.name, why, _pos(a), text))


def _parse_node(a, ctx, text):
    if isinstance(a, _ast.Constant):
        v = a.value
        if isinstance(v, bool) or not isinstance(v, (int, float)):
            _reject(a, ctx, text, "non-numeric literal %r" % (v,))
        return Const(float(v))
    if isinstance(a, _ast.Name):
        if a.id.startswith("__"):
            _reject(a, ctx, text, "dunder identifier %r is not allowed" % (a.id,))
        leaf = ctx.leaf_for_identifier(a.id)
        if leaf is None:
            _reject(a, ctx, text, "unknown identifier %r (no matching leaf)" % (a.id,))
        return leaf
    if isinstance(a, _ast.UnaryOp):
        sym = _UNARY_AST.get(type(a.op))
        if sym is None:
            _reject(a, ctx, text, "unsupported unary operator %s" % type(a.op).__name__)
        f = ctx._unary.get(sym)
        if f is None:
            _reject(a, ctx, text, "unary %r not in this context" % (sym,))
        return Call(f.name, _parse_node(a.operand, ctx, text))
    if isinstance(a, _ast.BinOp):
        sym = _BINOP_AST.get(type(a.op))
        if sym is None:
            _reject(a, ctx, text, "unsupported binary operator %s" % type(a.op).__name__)
        f = ctx._infix.get(sym)
        if f is None:
            _reject(a, ctx, text, "operator %r not in this context" % (sym,))
        return Call(f.name, _parse_node(a.left, ctx, text), _parse_node(a.right, ctx, text))
    if isinstance(a, _ast.Call):
        return _parse_call(a, ctx, text)
    if isinstance(a, _ast.Attribute):
        if not ctx.allow_attribute:
            _reject(a, ctx, text, "attribute access is not allowed in this context")
        _reject(a, ctx, text, "attribute-namespace leaves are not implemented (v1)")
    if isinstance(a, _ast.Subscript):
        _reject(a, ctx, text, "subscript is not allowed in this context")
    if isinstance(a, (_ast.ListComp, _ast.SetComp, _ast.DictComp, _ast.GeneratorExp)):
        _reject(a, ctx, text, "comprehensions are not allowed in this context")
    if isinstance(a, _ast.Lambda):
        _reject(a, ctx, text, "lambda is not allowed in this context")
    _reject(a, ctx, text, "unsupported expression construct %s" % type(a).__name__)


def _parse_call(a, ctx, text):
    if not isinstance(a.func, _ast.Name):
        _reject(a, ctx, text,
                "only bare-name calls are allowed (no attribute/callable chains)")
    if a.keywords:
        _reject(a, ctx, text, "keyword arguments are not allowed in an expression call")
    if any(isinstance(x, _ast.Starred) for x in a.args):
        _reject(a, ctx, text, "starred arguments are not allowed")
    name = a.func.id
    if name.startswith("__"):
        _reject(a, ctx, text, "dunder call %r is not allowed" % (name,))
    # the universal E0 typed-literal constructors -> Const-with-unit, in ANY context
    if name in _units.UNIT_TAGS:
        if len(a.args) != 1:
            _reject(a, ctx, text, "unit constructor %r takes exactly 1 argument" % (name,))
        return Const(_const_number(a.args[0], ctx, text), unit=name)
    f = ctx._funcs.get(name)
    if f is None:
        _reject(a, ctx, text,
                "unknown function %r; whitelist = %s" % (name, sorted(ctx._funcs)))
    if f.arity >= 0 and len(a.args) != f.arity:
        _reject(a, ctx, text, "%r expects %d args, got %d" % (name, f.arity, len(a.args)))
    return Call(f.name, *[_parse_node(x, ctx, text) for x in a.args])


def _const_number(a, ctx, text):
    """A signed numeric literal (a unit-constructor argument), e.g. `2000` or `-5`."""
    if isinstance(a, _ast.Constant) and isinstance(a.value, (int, float)) \
            and not isinstance(a.value, bool):
        return float(a.value)
    if isinstance(a, _ast.UnaryOp) and isinstance(a.op, _ast.USub):
        return -_const_number(a.operand, ctx, text)
    _reject(a, ctx, text, "unit constructor argument must be a numeric literal")
