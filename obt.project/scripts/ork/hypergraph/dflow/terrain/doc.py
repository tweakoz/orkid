###############################################################################
# ork.hypergraph.dflow.terrain.doc — the structured terrain DOCUMENT (Slice 0).
#
# The document is the un-unrolled program: nodes, params, connections, captures,
# and GROUPS (loop / encapsulate / switch). It is the single source of truth
# (owner law L2). The trace no longer builds a dflow.GraphData directly; instead
# the DSL ops record into a document, and a separate elaborate() DERIVES a fresh
# dflow.GraphData from it. Nothing outside elaborate() constructs GraphData.
#
# How the trace still targets "a graph" unchanged: current_graph() returns a
# _DocGraph RECORDER that duck-types dflow.GraphData's create()/connect() surface
# and hands back _DocModuleProxy objects that duck-type DgModuleData (inputs /
# outputs proxies + reflected scalar attrs). So ops.py / _node.py are untouched —
# they run against the recorder identically whether the trace targets a real
# GraphData (hypermesh cross-family composition) or this document (a HeightField).
# Each _DocModuleProxy wraps ONE real, UNATTACHED module instance purely so
# _apply_packs' schema/baked-scalar reflection stays faithful; that scratch module
# is never added to any GraphData (L2 holds).
#
# Explicit constructs (owner sign-off 2026-07-09, L-A/V-A/G-A/S-B):
#   with T.loop(N, carry=init) as L:   body traces ONCE; L.<carry> read/written;
#                                      L.i is a per-iteration index for scalar
#                                      param arithmetic; elaborate emits a nested
#                                      LoopModule (composite) — NOT a flat unroll.
#   @T.group def f(a, k=..): ...       each call expands independently.
#   T.switch(sel, name_a=.., name_b=..) eager: all branches trace; select one.
###############################################################################

import re as _re
import threading

from orkengine.core import dataflow as _dflow
from orkengine.core import vec2 as _vec2
from orkengine.lev2 import terrain as _terrain
from .._trace import current_graph, enter_trace, leave_trace, DslNode
from ._node import TerrainNode
from ... import units as _units
from ... import exprir as _exprir
from ..document import GraphDocument, GraphDocumentError, ParamTable as _BaseParamTable


class TerrainDocParamError(GraphDocumentError):
    """Raised on an illegal editor document mutation (S1): setting an unknown param
    on a node, or attempting to edit a read-only iteration (L.i) param. Loud by
    design (ops-self-defend) — the editor never silently drops an edit. Subclasses
    the family-neutral GraphDocumentError (E2) so the base document machinery's
    raises still surface as a terrain param error to existing `except` sites."""


# --- L.i per-iteration scalar expression (V-A) -------------------------------
# A tiny symbolic scalar tree over loop indices. Arithmetic (L.i*0.02, 0.8 - ...)
# builds the tree WITHOUT collapsing; the op's float()/setattr triggers __float__,
# which stashes the tree in a thread-local so the recorder captures it symbolically
# (evaluating to i=0 for the wrapped scratch module) and re-evaluates it per
# iteration at unroll. A8: the plug stays a plug — L.i only sets that iteration's
# stored plug VALUE, never inlined into generated shader text.

_ti_local = threading.local()

# L.i is direct-plug-scalar-only. It survives an op's `m.inputs.X = float(expr)` by
# stashing (expr, the-exact-float-object-returned) here; the recorder claims it ONLY
# when the immediately-following plug set receives THAT SAME float object (identity —
# proving the coerced expr flowed straight into a plug, untransformed). Every other
# fate is a LOUD, named error (ops-self-defend): a second coercion before the first is
# claimed (stash-over-stash), a coercion whose value was transformed (math.*, arithmetic
# after float, spread across params), a leftover pending at loop-exit / trace-end, or an
# L.i fed through the ParamPack path. Silent collapse to the i=0 constant is forbidden.
_LI_HELP = ("L.i is usable ONLY as a single direct scalar plug param, e.g. "
            "`T.terrace(L.h, sharpness=2.0 + L.i*0.5)` — not nested in math.*, "
            "int()/comparisons, ParamPack args, or multi-component values.")


def _set_pending_iter(expr, ret):
    prev = getattr(_ti_local, "pending", None)
    if prev is not None:
        _ti_local.pending = None
        raise RuntimeError(
            "an L.i expression was coerced to a float while a prior L.i coercion was "
            "still unclaimed (stash-over-stash). " + _LI_HELP)
    _ti_local.pending = (expr, ret)


def _take_pending_iter():
    p = getattr(_ti_local, "pending", None)
    _ti_local.pending = None
    return p


def assert_no_pending_iter(where):
    """LOUD guard: an outstanding L.i coercion that no plug set claimed is an error
    (never silently collapsed to its i=0 constant). Called at loop-exit + trace-end."""
    p = getattr(_ti_local, "pending", None)
    if p is not None:
        _ti_local.pending = None
        raise RuntimeError(
            f"an L.i expression was coerced to a float but never bound to a scalar "
            f"plug ({where}). " + _LI_HELP)


# --- terrain expression CONTEXTS (E2.5) --------------------------------------
# Terrain declares two named ExprIR contexts — the shared IR's per-family vocabulary unit
# (adjudication: expression vocabularies key off named CONTEXTS, a family declares several):
#   terrain.params — document-parameter (ctor-kwarg) arithmetic: an open doc-param leaf +
#                    const + add/sub/mul/div/neg. The E0 _ParamExpr replacement.
#   terrain.iter   — L.i per-iteration arithmetic: the loop-INDEX leaf (spelled `i`, its
#                    ParamRef.name carrying the loop OBJECT) + const + the same arithmetic.
#                    Disjoint from params (an iter tree never carries a doc-param leaf — mixing
#                    L.i with a doc-param folds the param to a const at trace, as it always has).
# The vocabularies match the old bespoke _IterExpr/_ParamExpr trees bit-for-bit, so
# param_expr_string / iter_expr_string stay character-identical and the wire encoders below
# (_enc_param_expr / _enc_iter) stay byte-identical.

_ARITH_FUNCS = [
    _exprir.infix("add", "+"), _exprir.infix("sub", "-"),
    _exprir.infix("mul", "*"), _exprir.infix("div", "/"),
    _exprir.unary("neg", "-"),
]

PARAMS_CTX = _exprir.register_context(_exprir.ExprContext(
    "terrain.params", functions=_ARITH_FUNCS,
    leaves=[_exprir.LeafSpec(_exprir.PARAM_DOC)],
    doc="terrain document-parameter (ctor-kwarg) arithmetic"))

ITER_CTX = _exprir.register_context(_exprir.ExprContext(
    "terrain.iter", functions=_ARITH_FUNCS,
    leaves=[_exprir.LeafSpec("index", spelling="i")],
    doc="terrain L.i per-iteration loop-index arithmetic"))


def _eval_iter_node(node, env):
    """Evaluate an ExprIR iter tree. The index leaf carries the LOOP OBJECT in ParamRef.name;
    `env` maps id(loop) -> the current iteration (default 0, the trace-time i=0 constant)."""
    if isinstance(node, _exprir.ParamRef):
        return float(env.get(id(node.name), 0))
    if isinstance(node, _exprir.Const):
        return float(node.value)
    if node.name == "neg":
        return -_eval_iter_node(node.args[0], env)
    return _PARAM_OPS[node.name](_eval_iter_node(node.args[0], env),
                                 _eval_iter_node(node.args[1], env))


class _IterCapture:
    """Trace-time capture handle for `L.i` arithmetic. NOT a float (like the old _IterExpr):
    operator overloads build a shared-ExprIR tree (`self._node`); __float__ mints an i=0 claim
    token the recorder identity-matches. The STORED / serialized form is the bare IR node —
    this handle is transient (it exists only during the trace)."""
    __slots__ = ("_node",)

    def __init__(self, node):
        self._node = node

    @staticmethod
    def index(loop):
        return _IterCapture(_exprir.ParamRef("index", loop))

    def __float__(self):
        r = _eval_iter_node(self._node, {})   # i=0 constant; the exact object is the claim token
        _set_pending_iter(self._node, r)
        return r

    # ParamPack coerces its values via _canon(), which probes to_vec4/to_vec3 (and never
    # calls __float__ on a non-float). Intercept there so an L.i fed through a pack raises
    # the direct-plug-scalar-only guidance instead of a bare "unsupported type".
    def to_vec4(self):
        raise TypeError("L.i cannot be used in a ParamPack. " + _LI_HELP)

    to_vec3 = to_vec4

    def __add__(self, o):
        return _IterCapture(_exprir.Call("add", self._node, _wrap_iter(o)._node))

    def __radd__(self, o):
        return _IterCapture(_exprir.Call("add", _wrap_iter(o)._node, self._node))

    def __sub__(self, o):
        return _IterCapture(_exprir.Call("sub", self._node, _wrap_iter(o)._node))

    def __rsub__(self, o):
        return _IterCapture(_exprir.Call("sub", _wrap_iter(o)._node, self._node))

    def __mul__(self, o):
        return _IterCapture(_exprir.Call("mul", self._node, _wrap_iter(o)._node))

    def __rmul__(self, o):
        return _IterCapture(_exprir.Call("mul", _wrap_iter(o)._node, self._node))

    def __truediv__(self, o):
        return _IterCapture(_exprir.Call("div", self._node, _wrap_iter(o)._node))

    def __rtruediv__(self, o):
        return _IterCapture(_exprir.Call("div", _wrap_iter(o)._node, self._node))

    def __neg__(self):
        return _IterCapture(_exprir.Call("neg", self._node))


def _wrap_iter(o):
    return o if isinstance(o, _IterCapture) else _IterCapture(_exprir.Const(float(o)))


def iter_expr_string(expr):
    """Human-readable source-ish string for an L.i iteration expression (editor read-only
    display, e.g. '(2 + (i * 0.5))'). `i` is the loop index. Delegates to the shared canonical
    pretty-printer — character-identical to the pre-E2.5 bespoke renderer."""
    return _exprir.pretty_print(expr, ITER_CTX)


# --- document parameters (E0): symbolic scalars over a doc-level params table ----
# _ParamCapture is the L.i capture protocol generalized to DOCUMENT PARAMETERS (DSL ctor kwargs
# promoted to first-class document data). It is a FLOAT SUBCLASS so it folds cleanly to its
# CURRENT value in any numeric context (ptex3d _wrap/_fmt_float, ParamPack _canon, plain
# arithmetic with a non-param operand) while operator overloads build a shared-ExprIR TREE.
# elaborate RE-EVALUATES that tree against the params table on a param edit — no re-trace, so
# topology
# edits survive by construction (the P0 fix). float(expr) mints a PER-USE claim token (one
# kwarg legitimately feeds many plugs) the recorder binds when the value lands on a plug or
# reflected prop — L.i's identity protocol, generalized to an id-keyed MAP so a coercion at
# an op's CALL SITE survives the intervening plug records of that op's other kwargs. int()/
# __index__/__bool__ are FORCED concretization (loop counts, range(), int props): they flag
# the referenced param(s) structural (edited via the guarded re-trace path, never captured).

_pe_local = threading.local()


def _param_pending():
    m = getattr(_pe_local, "pending", None)
    if m is None:
        m = {}
        _pe_local.pending = m
    return m


def _register_param_float(ret, expr):
    # id-keyed, holding the float object so its id cannot be reused: a float() coerced at an
    # op's call site (e.g. amplitude=float(amplitude_m)) survives the intervening plug records
    # of that op's other kwargs (a single-slot identity, as L.i uses, would be clobbered).
    _param_pending()[id(ret)] = (ret, expr)


def _claim_param_float(value):
    m = _param_pending()
    entry = m.get(id(value))
    if entry is not None and entry[0] is value:
        del m[id(value)]
        return entry[1]
    return None


def _clear_param_pending():
    """Drop unclaimed param-float tokens at a trace boundary. A leftover is a folded use
    (float()'d into ptex3d shader text / arithmetic, never landing on a plug) — its param
    gets no captured expr and is flagged structural by the runtime's post-trace finalize."""
    _pe_local.pending = {}


_PARAM_OPS = {
    "add": lambda a, b: a + b,
    "sub": lambda a, b: a - b,
    "mul": lambda a, b: a * b,
    "div": lambda a, b: a / b,
    "neg": lambda a, b: -a,
}


class _ParamTable(_BaseParamTable):
    """E0 document parameters — name -> value + an OPTIONAL unit TAG (E0 part 2 typed
    literals) + a per-name structural flag + declaration (signature) order. The family-
    neutral machinery now lives in the base ParamTable (JUL13_DFLOW E2 hoist, re-exported
    here for compat); the terrain subclass only pins the error type so an unknown-param
    set() still raises TerrainDocParamError exactly as before. Values stay PLAIN numeric
    (the tag is metadata, not carried on the value) so the ExprIR doc-param leaves read a plain
    number and the elaborated graph is tag-free — unchanged values bake byte-identically.
    Non-empty ONLY on the editor trace path; plain instantiation (viewer / scenes) leaves it
    empty and the document behaves as before E0."""

    _error_cls = TerrainDocParamError


def _eval_param_node(node, table):
    """Evaluate an ExprIR document-parameter tree against the CURRENT params table. A doc-param
    leaf reads the live table value (this is what makes a param edit re-elaborate re-trace-free);
    a const reads its literal."""
    if isinstance(node, _exprir.ParamRef):
        return float(table.get(node.name))
    if isinstance(node, _exprir.Const):
        return float(node.value)
    if node.name == "neg":
        return -_eval_param_node(node.args[0], table)
    return _PARAM_OPS[node.name](_eval_param_node(node.args[0], table),
                                 _eval_param_node(node.args[1], table))


def _param_names_of(node, out=None):
    """Every document parameter referenced by a doc-param ExprIR tree (dependency extraction)."""
    if out is None:
        out = set()
    if isinstance(node, _exprir.ParamRef):
        out.add(node.name)
    elif isinstance(node, _exprir.Call):
        for a in node.args:
            _param_names_of(a, out)
    return out


def _mark_structural(node, table):
    """Flag every doc param referenced by `node` structural (forced concretization: int()/
    __index__/__bool__ on a captured expr — a loop count, range(), int/bool prop)."""
    if table is not None:
        for name in _param_names_of(node):
            table.mark_structural(name)


class _ParamCapture(float):
    """Trace-time capture handle for E0 document-parameter arithmetic (the old _ParamExpr).
    A FLOAT SUBCLASS so it folds cleanly to its CURRENT value in any numeric context (ptex3d
    _wrap/_fmt_float, ParamPack _canon, plain arithmetic with a non-param operand) while the
    operator overloads build a shared-ExprIR tree (`self._node`). The STORED / serialized form
    is the bare IR node — this handle is transient. elaborate RE-EVALUATES the node against the
    params table on a param edit — no re-trace (the P0 fix). float() mints a per-use claim token
    the recorder binds when the value lands on a plug/prop; int()/__index__/__bool__ are FORCED
    concretization (flag the referenced param(s) structural)."""

    __slots__ = ("_node", "_table")

    def __new__(cls, value, node, table):
        self = float.__new__(cls, value)
        self._node = node
        self._table = table
        return self

    @staticmethod
    def param(table, name):
        return _ParamCapture(float(table.get(name)), _exprir.ParamRef(_exprir.PARAM_DOC, name), table)

    @staticmethod
    def const(value, table):
        return _ParamCapture(float(value), _exprir.Const(float(value)), table)

    def _make(self, op, a_node, b_node):
        node = _exprir.Call(op, a_node) if b_node is None else _exprir.Call(op, a_node, b_node)
        return _ParamCapture(_eval_param_node(node, self._table), node, self._table)

    # ---- claim + forced-concretization protocol -----------------------------
    def __float__(self):
        r = _eval_param_node(self._node, self._table)   # PLAIN float at CURRENT value (claim token)
        _register_param_float(r, self._node)
        return r

    def __int__(self):
        _mark_structural(self._node, self._table)
        return int(_eval_param_node(self._node, self._table))

    def __index__(self):
        _mark_structural(self._node, self._table)
        return int(_eval_param_node(self._node, self._table))

    def __bool__(self):
        _mark_structural(self._node, self._table)
        return bool(_eval_param_node(self._node, self._table))

    # ---- symbolic arithmetic (tree on a numeric operand; defer to the other type else) ---
    def _wrap(self, o):
        if isinstance(o, _ParamCapture):
            return o
        if isinstance(o, (int, float)) and not isinstance(o, bool):
            return _ParamCapture.const(o, self._table)
        return None                    # non-numeric (e.g. a ptex3d SurfNode) -> NotImplemented

    def __add__(self, o):
        w = self._wrap(o)
        return self._make("add", self._node, w._node) if w is not None else NotImplemented

    def __radd__(self, o):
        w = self._wrap(o)
        return self._make("add", w._node, self._node) if w is not None else NotImplemented

    def __sub__(self, o):
        w = self._wrap(o)
        return self._make("sub", self._node, w._node) if w is not None else NotImplemented

    def __rsub__(self, o):
        w = self._wrap(o)
        return self._make("sub", w._node, self._node) if w is not None else NotImplemented

    def __mul__(self, o):
        w = self._wrap(o)
        return self._make("mul", self._node, w._node) if w is not None else NotImplemented

    def __rmul__(self, o):
        w = self._wrap(o)
        return self._make("mul", w._node, self._node) if w is not None else NotImplemented

    def __truediv__(self, o):
        w = self._wrap(o)
        return self._make("div", self._node, w._node) if w is not None else NotImplemented

    def __rtruediv__(self, o):
        w = self._wrap(o)
        return self._make("div", w._node, self._node) if w is not None else NotImplemented

    def __neg__(self):
        return self._make("neg", self._node, None)


def param_expr_string(expr):
    """Human-readable source for a document-parameter expression (editor display + the .py
    writer). A param leaf renders as the ctor-kwarg name; arithmetic renders as source.
    Delegates to the shared canonical pretty-printer — character-identical to the pre-E2.5
    bespoke renderer."""
    return _exprir.pretty_print(expr, PARAMS_CTX)


def _collect_captured_param_names(doc):
    """Every document parameter referenced by at least one recorded param expr (a captured
    param — editing it re-elaborates re-trace-free). A param NOT in this set was folded /
    forced / unused and is structural."""
    names = set()

    def _walk(children):
        for ch in children:
            if isinstance(ch, DocNode):
                for expr in ch.param_exprs.values():
                    _param_names_of(expr, names)
            if isinstance(ch, (DocLoop, DocGroupCall)):
                _walk(ch.children)
    _walk(doc._root)
    return names


def _coerce_like(old, value):
    """Coerce an editor-supplied value to the recorded param's type so int stays int
    (doc-JSON encodable + rebake-stable). vec2/str pass through unchanged."""
    if isinstance(old, bool):
        return bool(value)
    if isinstance(old, int):
        return int(round(value)) if isinstance(value, float) else int(value)
    if isinstance(old, float):
        return float(value)
    return value


# --- plug reference handles --------------------------------------------------
# A ref is always an out-plug handle (source of a connection). It carries the
# producing node (a DocNode or a _Placeholder) + the plug name. Elaboration
# resolves it either via a per-iteration substitution (placeholders) or via the
# node -> real-module map (DocNodes).

class _DocOutPlug:
    __slots__ = ("node", "plug_name")

    def __init__(self, node, plug_name):
        self.node = node
        self.plug_name = plug_name


class _DocInPlug:
    __slots__ = ("node", "plug_name")

    def __init__(self, node, plug_name):
        self.node = node
        self.plug_name = plug_name


class _Placeholder:
    """A synthetic connection source resolved by substitution at unroll (a loop
    carry input, or a loop's carried output). Never becomes a real module."""
    __slots__ = ("name",)

    def __init__(self, name):
        self.name = name


# --- module + plug proxies (duck-type DgModuleData for ops.py / _node.py) -----

class _DocInputsProxy:
    """Mimics module.inputs: getattr yields an in-plug handle (connect targets);
    setattr records a param value (and forwards to the scratch module so
    _apply_packs' schema/type reflection stays consistent); repr forwards the real
    schema string that _input_schema() regex-parses."""

    def __init__(self, docnode):
        object.__setattr__(self, "_docnode", docnode)

    def __getattr__(self, name):
        dn = object.__getattribute__(self, "_docnode")
        return _DocInPlug(dn, name)

    def __setattr__(self, name, value):
        dn = object.__getattribute__(self, "_docnode")
        dn._record_param("inputs", name, value)

    def __repr__(self):
        dn = object.__getattribute__(self, "_docnode")
        return repr(dn._real.inputs)


class _DocOutputsProxy:
    def __init__(self, docnode):
        object.__setattr__(self, "_docnode", docnode)

    def __getattr__(self, name):
        dn = object.__getattribute__(self, "_docnode")
        dn._out_names.add(name)
        return _DocOutPlug(dn, name)


class _DocModuleProxy:
    """Mimics a DgModuleData for the DSL ops: .inputs / .outputs proxies, reflected
    scalar setattr (m.octaves = 5, m.op = 2, m.channel = ...), and getattr forwarding
    to the scratch module so _baked_scalar_kind()'s getattr sees real defaults."""

    def __init__(self, docnode):
        object.__setattr__(self, "_docnode", docnode)

    def __getattr__(self, name):
        dn = object.__getattribute__(self, "_docnode")
        if name == "inputs":
            return dn._inputs_proxy
        if name == "outputs":
            return dn._outputs_proxy
        if name == "_name":
            return dn.local_name
        # baked-scalar reflection (_apply_packs) + any other read -> scratch module.
        return getattr(dn._real, name)

    def __setattr__(self, name, value):
        dn = object.__getattribute__(self, "_docnode")
        if name == "bypassed":
            # STRUCTURAL bypass flag (T.bypass op / editor): recorded on the DocNode
            # (badges / undo / doc-JSON, validated bypassable) and forwarded to the scratch
            # reflection module. elaborate forwards DocNode.bypassed onto the freshly-created
            # real module, where the C++ resolveConnectedOutput splices it out.
            dn.set_bypassed(value)
            if dn._real is not None and hasattr(dn._real, "bypassed"):
                dn._real.bypassed = bool(value)
            return
        dn._record_param("module", name, value)


# --- document nodes ----------------------------------------------------------

class DocNode:
    """One recorded DSL op: op class + ordered param sets + input connections.
    Mirrors one DSL call (spec 2.1)."""

    def __init__(self, doc, local_name, module_clazz, _make_real=True):
        self.doc = doc
        self.local_name = local_name
        self.clazz = module_clazz
        self.clazz_name = module_clazz.__name__ if module_clazz is not None else None
        # scratch instance: ONLY for _apply_packs schema/baked-scalar reflection
        # during the live trace. Never attached to any GraphData. Absent after
        # deserialize (elaborate re-creates fresh modules and replays params).
        self._real = module_clazz.createShared() if (_make_real and module_clazz is not None) else None
        self.param_actions = []          # ordered [(kind, name, value)]
        self.iter_params = {}            # (kind, name) -> ExprIR node (L.i; terrain.iter ctx)
        self.param_exprs = {}            # (kind, name) -> ExprIR node (E0 doc-param; re-eval'd
                                         # at elaborate so a param edit needs no re-trace)
        self.connections = []            # [(in_plug_name, _DocOutPlug)]
        self.bypassed = False            # editor flag (persisted): elaborate aliases
                                         # this node's outputs to its pass-through input
        self._out_names = set()
        self._inputs_proxy = _DocInputsProxy(self)
        self._outputs_proxy = _DocOutputsProxy(self)
        self.proxy = _DocModuleProxy(self)

    def _record_param(self, kind, name, value):
        # E0: a symbolic document parameter landed here — either the value IS a _ParamCapture (a
        # bare param / arithmetic tree flowed straight to the plug/prop) or a float() coercion
        # of one did (bound by identity). Record the IR NODE (elaborate re-evaluates it) + the
        # current concrete value (scratch-module forward + pywriter literal fallback).
        if isinstance(value, _ParamCapture):
            ev = float(_eval_param_node(value._node, value._table))
            self.param_exprs[(kind, name)] = value._node
            self.param_actions.append((kind, name, ev))
            self._forward(kind, name, ev)
            return
        claimed = _claim_param_float(value)
        if claimed is not None:
            ev = float(value)
            self.param_exprs[(kind, name)] = claimed
            self.param_actions.append((kind, name, ev))
            self._forward(kind, name, ev)
            return
        pending = _take_pending_iter()
        if pending is not None:
            expr, ret = pending
            if value is ret:
                # identity: the coerced L.i expression flowed straight into this plug.
                ev = float(ret)
                self.iter_params[(kind, name)] = expr
                self.param_actions.append((kind, name, ev))
                self._forward(kind, name, ev)
                return
            # a coercion happened but its result was transformed / mis-routed -> LOUD.
            raise RuntimeError(
                f"an L.i expression was coerced to a float but its value did not flow "
                f"directly into plug {name!r} on {self.clazz_name} (the op set "
                f"{name!r}={value!r}). " + _LI_HELP)
        self.param_actions.append((kind, name, value))
        self._forward(kind, name, value)

    def _forward(self, kind, name, value):
        if self._real is None:
            return
        if kind == "inputs":
            setattr(self._real.inputs, name, value)
        else:
            setattr(self._real, name, value)

    # ---- editor mutation API (S1; L2 — the document is the source of truth) ----

    def editable_params(self):
        """Ordered [(kind, name, value)] the editor may set_param — the EFFECTIVE
        (last-write-wins) value of each recorded param, EXCLUDING iteration (L.i)
        params (those are read-only in v1, see iter_param_view())."""
        seen = {}
        order = []
        for (kind, name, value) in self.param_actions:
            key = (kind, name)
            if key in self.iter_params or key in self.param_exprs:
                continue
            if key not in seen:
                order.append(key)
            seen[key] = value
        return [(k, n, seen[(k, n)]) for (k, n) in order]

    def iter_param_view(self):
        """Read-only view of this node's iteration (L.i) params:
        [(kind, name, expr_string, i0_value)]. NOT editable in v1 (edit the DSL
        source to change an iteration expression)."""
        return [(kind, name, iter_expr_string(expr), float(_eval_iter_node(expr, {})))
                for (kind, name), expr in self.iter_params.items()]

    def param_expr_view(self):
        """Read-only view of this node's DOCUMENT-PARAMETER-driven (E0 ExprIR) params:
        [(kind, name, expr_string, current_value)]. These are excluded from editable_params()
        (they are edited via the referenced Terrain Parameter, not the node plug), but they are
        still REAL, bound params — surfacing them read-only keeps the propsheet honest instead of
        silently dropping a plug that carries an expression (topology-honesty; ops self-defend)."""
        return [(kind, name, param_expr_string(expr), float(_eval_param_node(expr, self.doc.params)))
                for (kind, name), expr in self.param_exprs.items()]

    def set_param(self, kind, name, value):
        """Editor mutation (L2): update a recorded param action's value in place.
        `kind` is 'inputs' (a plug) or 'module' (a reflected scalar). Updates the
        LAST recorded (kind,name) action (elaborate applies actions in order, last
        wins). Rejects LOUDLY: an iteration (L.i) param is read-only in v1; a
        (kind,name) never recorded on this node is unknown."""
        if (kind, name) in self.iter_params:
            raise TerrainDocParamError(
                f"param {name!r} on {self.clazz_name} is an iteration expression "
                f"(L.i-driven) — iteration params are read-only in v1; edit the DSL "
                f"source to change the expression.")
        if (kind, name) in self.param_exprs:
            raise TerrainDocParamError(
                f"param {name!r} on {self.clazz_name} is driven by document parameter(s) "
                f"{sorted(self.param_exprs[(kind, name)]._param_names())} — edit the Terrain "
                f"Parameter (doc.params), not the node plug (E0).")
        idx = None
        for i, (k, n, _v) in enumerate(self.param_actions):
            if k == kind and n == name:
                idx = i
        if idx is None:
            have = sorted({n for (k, n, _v) in self.param_actions if k == kind})
            raise TerrainDocParamError(
                f"no editable param {name!r} (kind={kind!r}) on {self.clazz_name}; "
                f"recorded {kind} params: {have}.")
        value = _coerce_like(self.param_actions[idx][2], value)
        self.param_actions[idx] = (kind, name, value)
        self._forward(kind, name, value)
        return value

    def bypassable(self):
        """False for source nodes (no input to pass through) and captures."""
        return bool(self.connections) and self.clazz_name != "CaptureModule"

    def set_bypassed(self, flag):
        """Editor mutation (L2, STRUCTURAL): toggle the bypass flag. A bypassed node is
        never materialized — elaborate aliases its outputs to its FIRST input's source
        (recursively across bypassed chains). Re-elaborate + rebake to apply."""
        flag = bool(flag)
        if flag and not self.bypassable():
            why = ("captures cannot be bypassed" if self.clazz_name == "CaptureModule"
                   else "no input to pass through (source node)")
            raise TerrainDocParamError(
                f"cannot bypass {self.local_name!r} [{self.clazz_name}]: {why}")
        self.bypassed = flag
        return flag


class DocCapture:
    """A capture sink (channel list -> a CaptureModule DocNode). The document owns
    it; elaborate rediscovers capture modules by class to build the channel map."""

    def __init__(self, node, channels):
        self.node = node
        self.channels = list(channels)


class _Carry:
    __slots__ = ("name", "placeholder", "out_placeholder", "initial_ref", "body_out_ref")

    def __init__(self, name, initial_ref):
        self.name = name
        self.placeholder = _Placeholder(f"{name}#in")
        self.out_placeholder = _Placeholder(f"{name}#out")
        self.initial_ref = initial_ref          # _DocOutPlug in the PARENT scope
        self.body_out_ref = None                 # _DocOutPlug in the BODY scope


class DocLoop:
    """kind=loop: a body subgraph traced ONCE + count + loop-carried bindings.
    Elaboration = unroll (iteration i outputs feed iteration i+1 inputs)."""

    def __init__(self, doc, count, path, carries):
        self.doc = doc
        self.count = int(count)
        self.path = path
        self.children = []                       # body nodes/subgroups (creation order)
        self.carries = carries                   # ordered {name: _Carry}
        self.bypassed = False                    # editor flag (persisted): elaborate skips
                                                 # the unroll and aliases each carry's post-
                                                 # loop output to that carry's INITIAL ref

    def set_count(self, n):
        """Editor mutation (L2): set the loop's STRUCTURAL iteration count (>=0 int).
        Re-elaborate + rebake to apply — raising N->N+k keeps the first-N iterations'
        Merkle hashes (cache hits); only the k new tail iterations recompute."""
        n = int(n)
        if n < 0:
            raise TerrainDocParamError(f"loop count must be >= 0 (got {n})")
        self.count = n
        return n

    def bypassable(self):
        """Always True: a bypassed loop aliases each carry's post-loop output to that
        carry's INITIAL ref (per-carry, type-safe by construction — every loop declares
        at least one carry, and a carry shares its initial's type)."""
        return True

    def set_bypassed(self, flag):
        """Editor mutation (L2, STRUCTURAL): toggle the loop's bypass flag. A bypassed loop
        NEVER unrolls — elaborate aliases each carry's post-loop output to its INITIAL ref
        (cook cache correct for free: no body module is created). Re-elaborate + rebake."""
        self.bypassed = bool(flag)
        return self.bypassed


class DocGroupCall:
    """kind=encapsulate: one call-site expansion of an @T.group function. Re-traced
    per call with the real args, so each call expands independently (function-call
    semantics; ptex3d function-assets precedent)."""

    def __init__(self, doc, path, func_name, args):
        self.doc = doc
        self.path = path
        self.func_name = func_name
        self.args = args                         # {argname: _DocOutPlug | scalar} (model)
        self.children = []
        self.output_ref = None                   # _DocOutPlug the call returned
        self.bypassed = False                    # editor flag (persisted): elaborate skips
                                                 # the body and aliases the group output to
                                                 # its FIRST terrain-typed input arg's ref

    def _first_terrain_arg(self):
        """The first (insertion-order) call argument that is a terrain-node ref (a
        _DocOutPlug) — the ref a bypassed group passes through as its output. None for a
        generator group (every arg is a scalar / there is no terrain input to pass through)."""
        for _name, val in self.args.items():
            if isinstance(val, _DocOutPlug):
                return val
        return None

    def bypassable(self):
        """True iff the group has a terrain-typed input argument to pass through as its
        output. A generator group (no such input) is NOT bypassable — there is nothing to
        alias its output to; set_bypassed refuses it LOUDLY (ops-self-defend)."""
        return self._first_terrain_arg() is not None

    def set_bypassed(self, flag):
        """Editor mutation (L2, STRUCTURAL): toggle the group's bypass flag. A bypassed group
        NEVER expands — elaborate aliases its output to its FIRST terrain-typed input arg's ref
        (mirror the node first-input rule). Refuses LOUDLY when the group has no such input (a
        generator group). Re-elaborate + rebake to apply."""
        flag = bool(flag)
        if flag and not self.bypassable():
            raise TerrainDocParamError(
                f"cannot bypass group {self.func_name!r}: it has no terrain-typed input "
                f"argument to pass through (a generator group)")
        self.bypassed = flag
        return flag


class DocSwitch:
    """kind=switch: N branches traced eagerly + a selector. The switch is a real node
    in the connection graph — downstream consumers wire to its OUTPUT PLACEHOLDER, and
    ELABORATION (the authority, L2) binds that placeholder to the SELECTED branch head.
    So a document loaded from doc-JSON with no source can re-select: mutate `.selected`
    (or `.selector`) and re-elaborate to route the other branch downstream."""

    def __init__(self, selector, branches, selected):
        self.selector = selector                 # original selector value (retained metadata)
        self.branches = branches                 # {name: _DocOutPlug} branch heads
        self.selected = selected                 # AUTHORITATIVE resolved branch name
        self.out_placeholder = _Placeholder("switch#out")
        self.bypassed = False                    # always False (a switch is NOT bypassable):
                                                 # present only so the badge handler is uniform

    def select(self, name):
        """Choose a branch by name (re-elaborate to apply)."""
        if name not in self.branches:
            raise KeyError(f"no such switch branch {name!r}; have {sorted(self.branches)}")
        self.selected = name

    def bypassable(self):
        """A switch is NOT bypassable in this slice — it has no single input to pass through;
        branch selection (select) already covers the need."""
        return False

    def set_bypassed(self, flag):
        """Refuses LOUDLY (ops-self-defend): a switch cannot be bypassed (S-B). Use branch
        selection (DocSwitch.select) to route a single branch downstream."""
        if flag:
            raise TerrainDocParamError(
                "cannot bypass a T.switch: it has no single input to pass through — use "
                "branch selection (DocSwitch.select) instead")
        self.bypassed = False
        return False


# --- the trace-time recorder (duck-types dflow.GraphData) --------------------

class _DocGraph:
    """Returned by current_graph() while a HeightField is tracing. Ops call
    .create()/.connect() on it exactly as they would a real GraphData."""

    def __init__(self, doc):
        self._doc = doc

    def create(self, named, module_clazz):
        node = DocNode(self._doc, named, module_clazz)
        self._doc._append_child(node)
        return node.proxy

    def connect(self, in_plug, out_plug):
        if not isinstance(in_plug, _DocInPlug) or not isinstance(out_plug, _DocOutPlug):
            raise TypeError("terrain document connect() expects doc plug handles")
        in_plug.node.connections.append((in_plug.plug_name, out_plug))


# --- the document ------------------------------------------------------------

class TerrainDoc(GraphDocument):
    """The structured terrain program (JUL13_DFLOW E2 terrain specialization of
    GraphDocument). Built by the trace; elaborate() derives a fresh dflow.GraphData
    from it (L2). The base-surface methods (tree_paths / editable_params / set_param /
    set_bypassed / select_output / add_node / delete_node / to_json / from_json /
    node positions / undo checkpoints / elaborate) are satisfied below — most delegate
    to the module-level implementations the existing consumers already call, so this
    refactor is behavior- and byte-identical while making a TerrainDoc bind through the
    generic editor core."""

    def __init__(self):
        super().__init__()                       # base: params table + node-position store
        self._root = []                          # top-level children (creation order)
        self._scope_stack = [self._root]         # current container's children list
        self._scope_owners = [None]              # parallel: owning group (None=root)
        self._captures = []                      # [DocCapture]
        self._group_counter = 0
        # doc-level select-as-output target (base.select_output; serialized in doc-JSON):
        # the DocNode whose 'Out' drives the display captures at bake — elaborate stamps
        # graph.output_node from it when no editor session display node overrides it. None
        # = no saved marker (the graph bakes with its authored capture wiring).
        self._select_output = None
        # E0 document parameters (DSL ctor kwargs promoted to first-class document data).
        # Empty for plain instantiation (viewer / scenes); populated by the editor trace path
        # (TerrainRuntime) with the _ParamExpr symbols the DocNodes' param_exprs reference.
        self.params = _ParamTable()
        self.graph = _DocGraph(self)

    # ---- GraphDocument base surface (delegators to the module-level API) ------
    # A bare same-named call below (tree_paths / to_json / from_json / delete_node)
    # resolves to the MODULE-level function by normal name lookup — a method body does
    # not see the class namespace, so there is no shadowing / recursion. These add the
    # generic editor-core binding surface without touching the existing call paths.

    def tree_paths(self):
        return tree_paths(self)

    def editable_params(self, node):
        return node.editable_params()

    def set_param(self, node, kind, name, value):
        return node.set_param(kind, name, value)

    def node_class_name(self, node):
        # the DSL/pybind module class ("CombineModule", ...) — feeds the propsheet's
        # reflection-derived enum-choice lookup (E1). Only DocNodes carry a class.
        return getattr(node, "clazz_name", None)

    # ---- editor EXPRESSION-SOURCE fields (E2.5 S7 — terrain T.expr) -----------
    # ExprModule carries a reflected `expr_source`: a PYTHON ptex3d expression STRING that
    # elaborate() recompiles into shadertext on every rebake (doc.py:1138). It is edited AS
    # SOURCE through the propsheet detail editor (a CodeView), NOT as a one-line scalar row —
    # so the node property model surfaces it via editor.custom == "expr" and EXCLUDES it from
    # the plain module rows. Family-neutral base returns [] for every other node.

    def expr_fields(self, node):
        """[(reflected_property, context_name)] for a DocNode's editable expression-source
        fields, or []. Only an ExprModule with a recorded expr_source qualifies; the context
        name labels the terrain source form (a ptex3d expression STRING, not an ExprIR tree)."""
        if not (isinstance(node, DocNode) and node.clazz_name == "ExprModule"):
            return []
        if self._expr_source_of(node) is None:
            return []
        return [("expr_source", "terrain.ptex3d")]

    def expr_field_source(self, node, field):
        """The current author SOURCE of a terrain expression field — the recorded ptex3d
        expression string (last-write-wins). '' when unset."""
        if field != "expr_source":
            raise TerrainDocParamError(f"no terrain expr field {field!r} (have expr_source)")
        return self._expr_source_of(node) or ""

    def set_expr_field(self, node, field, source):
        """Editor mutation (L2): VALIDATE the ptex3d source by compiling it the SAME way
        elaborate does (doc.py rebake path), THEN write it through the doc edit path so the
        next rebake recompiles the shadertext + re-keys the cook cache. Raises loudly
        (TerrainDocParamError) on a bad-syntax / non-scalar / over-referenced expression —
        the document is UNCHANGED (the write never happens)."""
        if field != "expr_source":
            raise TerrainDocParamError(f"no terrain expr field {field!r} (have expr_source)")
        from . import ops as _ops
        _ops._compile_expr_source(source, len(node.connections))   # loud on invalid, pre-write
        node.set_param("module", "expr_source", source)            # doc edit path -> recompile
        return source

    @staticmethod
    def _expr_source_of(node):
        """The last recorded expr_source value on `node`, or None (never set — a pre-B1 binary
        or a non-source ExprModule)."""
        val = None
        for (k, n, v) in node.param_actions:
            if k == "module" and n == "expr_source":
                val = v
        return val

    def set_bypassed(self, obj, flag):
        return obj.set_bypassed(flag)

    def node_bypass_badge(self, obj):
        # bypass badge ('B') rides any bypassable DocNode AND the STRUCTURAL constructs
        # (DocLoop / DocGroupCall) — a bypassed loop skips its unroll, a bypassed group
        # aliases its output to its first terrain input. DocSwitch (and anything else)
        # carries no bypass badge.
        if isinstance(obj, (DocNode, DocLoop, DocGroupCall)):
            return (obj.bypassable(), bool(obj.bypassed))
        return None

    def node_output_badge(self, obj):
        # display / select-as-output badge ('O') stays NODE-only (a construct has no single
        # 'Out'). Shown on every DocNode; disabled (enabled=False) on captures.
        if isinstance(obj, DocNode):
            return obj.clazz_name != "CaptureModule"
        return None

    def select_output(self, node):
        return self.set_select_output(node)

    def add_node(self, op_name, after, name=None):
        return add_op_node(self, op_name, after, name=name)

    def delete_node(self, node):
        return delete_node(self, node)

    def connect(self, dst_key, in_plug, src_key, out_plug):
        """Editor mutation (L2, STRUCTURAL): wire a producer out-plug to a consumer
        in-plug, OVERWRITING whatever fed the consumer. OWNS the native-edge-store write
        (DocNode.connections / loop-carry initial_ref / group arg / switch branch — the
        same ref slots _iter_ref_slots covers), so the C1 canvas routes every node/
        construct wire through here rather than poking the store itself. Endpoints are
        tree_paths() keys. Loud, catchable refusals (ops self-defend) on an unknown key,
        an unknown plug, or a construct with no resolvable output. Splice-on-wire works
        because a re-wire onto an already-fed input replaces the source."""
        return connect_edge(self, dst_key, in_plug, src_key, out_plug)

    def disconnect(self, *args, **kwargs):
        """DECIDED (JUL13_DFLOW E2): a terrain input is ALWAYS fed — elaboration requires
        every consumer plug to resolve to a source — so an edge cannot be orphaned, only
        re-sourced. A bare disconnect is therefore REFUSED LOUDLY (never a silent no-op):
        re-wire the input to a different source (connect overwrites) or delete the node."""
        raise TerrainDocParamError(
            "terrain inputs are always fed; connect a different source or delete the node")

    def to_json(self):
        return to_json(self)

    @classmethod
    def from_json(cls, data):
        return from_json(data)

    # ---- trace-time recording API -------------------------------------------

    def _append_child(self, child):
        self._scope_stack[-1].append(child)

    def _push_scope(self, group):
        self._scope_stack.append(group.children)
        self._scope_owners.append(group)

    def _pop_scope(self):
        self._scope_stack.pop()
        self._scope_owners.pop()

    def _current_group(self):
        return self._scope_owners[-1]

    def _next_group_path(self, base):
        n = self._group_counter
        self._group_counter += 1
        return f"{base}_{n}"

    def record_capture(self, cap_proxy, channels):
        self._captures.append(DocCapture(cap_proxy._docnode, channels))

    def set_select_output(self, docnode):
        """Editor / DSL mutation (L2): mark `docnode` (a DocNode, or None to clear) as the
        doc-level select-as-output target. elaborate stamps graph.output_node from it unless
        a session display node overrides. Loud on a non-DocNode (ops self-defend)."""
        if docnode is not None and not isinstance(docnode, DocNode):
            raise TerrainDocParamError(
                f"select_output target must be a document node (got {type(docnode).__name__})")
        self._select_output = docnode
        return docnode

    # ---- elaboration (the ONLY constructor of GraphData) --------------------

    _DISPLAY_CHANNELS = ("height", "normal")

    def elaborate(self, display_node=None):
        """Derive a fresh dflow.GraphData. Returns (graph, {channel: CaptureModule}).

        display_node (the editor's 'select as output', SESSION state — never part of the
        document) OR the doc-level select_output target (base.select_output, persisted) marks
        the graph's DISPLAY output. The graph is elaborated in FULL and its
        GraphData.output_node is stamped with the marked node's ELABORATED module name (for a
        node inside a T.loop, the composite LoopModule — whose "Out" is the last-iteration carry
        — what a live display shows). The C++ bake driver honors the marker: it re-points height/normal captures to
        that node's 'Out' and drops the rest — nothing HERE rewires captures. cap_map is
        filtered to the surviving display channels so callers path only what the bake writes."""
        g = _dflow.GraphData.createShared()
        g.cacheable = True
        node_real = {}       # id(DocNode) -> real module
        node_name = {}       # id(DocNode) -> elaborated graph module name (last iteration wins)
        sub = {}             # id(_Placeholder) -> real out-plug
        cap_map = {}         # channel -> real CaptureModule
        index_env = {}       # id(DocLoop) -> current iteration
        # the DISPLAY target (session display_node OR the persisted select_output) is
        # threaded into the expansion so a DocLoop target can promote its height-typed carry
        # to the module's "Out" boundary (the C++ bake re-points height/normal captures to
        # that node's 'Out' — a multi-carry loop otherwise has no "Out").
        target = display_node if display_node is not None else self._select_output
        self._expand(self._root, g, node_real, node_name, sub, cap_map, "", index_env,
                     display_target=target)
        if target is not None:
            name = node_name.get(id(target))
            if name is None:
                raise RuntimeError(
                    f"select-as-output: display node "
                    f"{getattr(target, 'local_name', target)!r} is not in the elaborated "
                    f"graph (bypassed / unselected branch / a node from another document).")
            g.output_node = name
            # the C++ bake keeps only height/normal captures (re-pointed to the marked node)
            # and drops the rest — mirror that in cap_map so callers path only what is written.
            cap_map = {c: cap for c, cap in cap_map.items() if c in self._DISPLAY_CHANNELS}
            if not cap_map:
                raise RuntimeError(
                    "select-as-output: the document has no height/normal capture — display "
                    "mode needs at least one of them.")
        return g, cap_map

    def _expand(self, children, g, node_real, node_name, sub, cap_map, prefix, index_env,
                loop_emit=None, display_target=None):
        for child in children:
            if isinstance(child, DocNode):
                self._expand_node(child, g, node_real, node_name, sub, cap_map, prefix, index_env, loop_emit)
            elif isinstance(child, DocLoop):
                if loop_emit is not None:
                    # a T.loop nested inside a T.loop body would need a LoopModule-inside-a-
                    # LoopModule whose L.i table could depend on the OUTER index (not a single
                    # per-iteration value). Not emitted yet — fail LOUDLY (ops self-defend).
                    raise RuntimeError(
                        f"nested T.loop {child.path!r} inside a T.loop body is not supported by "
                        f"the LoopModule emitter yet — flatten it or file a follow-up.")
                self._expand_loop(child, g, node_real, node_name, sub, cap_map, prefix, index_env,
                                  display_target=display_target)
            elif isinstance(child, DocGroupCall):
                if loop_emit is not None:
                    raise RuntimeError(
                        f"@T.group call {child.func_name!r} inside a T.loop body is not supported "
                        f"by the LoopModule emitter yet — inline it or file a follow-up.")
                if child.bypassed:
                    # STRUCTURAL bypass: the group body never materializes (cook-correct for
                    # free). Its output aliases to the FIRST terrain-typed input arg's ref —
                    # downstream consumers (which wire to the group's returned node) resolve
                    # THROUGH sub to that arg. A generator group is refused at set_bypassed;
                    # a load with a stripped arg fails LOUDLY here (never silently).
                    arg_ref = child._first_terrain_arg()
                    if arg_ref is None:
                        raise RuntimeError(
                            f"bypassed group {child.func_name!r} has no terrain-typed input "
                            f"argument to pass through (a generator group cannot be bypassed)")
                    if child.output_ref is not None:
                        sub[id(child.output_ref.node)] = _resolve(arg_ref, node_real, sub)
                else:
                    self._expand(child.children, g, node_real, node_name, sub, cap_map,
                                 prefix + child.path + "/", index_env, display_target=display_target)
            elif isinstance(child, DocSwitch):
                if loop_emit is not None:
                    raise RuntimeError(
                        "T.switch inside a T.loop body is not supported by the LoopModule "
                        "emitter yet — hoist it out of the loop or file a follow-up.")
                self._expand_switch(child, node_real, sub)
            else:
                raise RuntimeError(f"unknown document child {type(child).__name__}")

    def _expand_switch(self, sw, node_real, sub):
        # ELABORATE is the selection authority (L2): bind the switch's output placeholder
        # to the SELECTED branch head, so downstream consumers route to it. Flipping
        # sw.selected on a loaded document and re-elaborating rewires to the other branch.
        if sw.selected not in sw.branches:
            raise RuntimeError(
                f"T.switch selected branch {sw.selected!r} is not among the branches "
                f"{sorted(sw.branches)} — set DocSwitch.selected to a valid branch name.")
        sub[id(sw.out_placeholder)] = _resolve(sw.branches[sw.selected], node_real, sub)

    def _expand_node(self, child, g, node_real, node_name, sub, cap_map, prefix, index_env, loop_emit=None):
        inner_name = prefix + child.local_name
        real = g.create(inner_name, child.clazz)
        node_real[id(child)] = real
        node_name[id(child)] = inner_name
        if child.bypassed:
            # STRUCTURAL bypass: materialize the module but mark it a transparent PASS-THROUGH
            # — the C++ resolveConnectedOutput hops every dependency read of its output to its
            # own first connected input at wiring time, so it drops out of the demand /
            # cook-Merkle chain WITHOUT deletion (its physical edges stay for the hop). The
            # Python side no longer aliases (see _resolve): the C++ resolver owns the splice.
            real.bypassed = True
        for (kind, name, value) in child.param_actions:
            _set_param(real, kind, name, value)
        # E0: re-evaluate captured document-parameter exprs into their plug/prop AFTER the
        # trace-time snapshot (so the LIVE param value wins). This is what makes a param edit
        # re-elaborate re-trace-free. With params unchanged, _eval() reproduces the recorded
        # concrete value bit-for-bit (byte-identical GraphData); coerced to the recorded type.
        for (kind, name), expr in child.param_exprs.items():
            _set_param(real, kind, name, _coerce_like(_last_action_value(child, kind, name),
                                                      _eval_param_node(expr, self.params)))
        if loop_emit is None:
            # top-level (or a non-loop nested scope): bake the L.i value for the current
            # iteration index directly into the plug (there is no per-iteration feed here).
            for (kind, name), expr in child.iter_params.items():
                _set_param(real, kind, name, float(_eval_iter_node(expr, index_env)))
        else:
            # INSIDE a T.loop body: an L.i param becomes a per-iteration FEED on the LoopModule
            # (a value TABLE evaluated from the SAME _IterExpr the unroll would eval — bit-exact).
            # It must target a FLOAT INPUT PLUG (the C++ feed writes typedInputNamed<Float>).
            for (kind, name), expr in child.iter_params.items():
                if kind != "inputs":
                    raise RuntimeError(
                        f"L.i param {name!r} on {child.clazz_name} in a T.loop body targets a "
                        f"{kind!r} (not an input plug) — the per-iteration feed writes a float "
                        f"input plug only. Move the L.i arithmetic to an input plug.")
                loop_emit.add_iter_feed(inner_name, name, expr)
        for (in_name, out_plug) in child.connections:
            if loop_emit is None:
                real_out = _resolve(out_plug, node_real, sub)
                g.connect(getattr(real.inputs, in_name), real_out)
            else:
                loop_emit.wire_input(g, real, inner_name, in_name, out_plug, node_real, sub)
        # B3: an editor-authored expression node (T.expr) recompiles its shadertext from the
        # recorded expr_source so a propsheet edit to the source takes effect on rebake (loud
        # TerrainDocParamError on failure). Feature-guarded on B1's reflected expr_source.
        if child.clazz_name == "ExprModule" and hasattr(real, "expr_source"):
            src = real.expr_source
            if src:
                from . import ops as _ops
                text, surfnode = _ops._compile_expr_source(src, len(child.connections))
                real.shadertext = text
                # keep the canonical ExprIR tree (cook-hash identity) in step with an
                # edited source, so a propsheet source edit re-keys the cook cache.
                _ops._set_expr_tree(real, surfnode)
        if child.clazz_name == "CaptureModule":
            if loop_emit is not None:
                raise RuntimeError(
                    "a capture inside a T.loop body is not supported — captures are the graph's "
                    "output sinks and belong OUTSIDE the loop (capture the carry after the loop).")
            chans = _channel_list(real.channel)
            for c in chans:
                cap_map[c] = real

    def _all_doc_nodes(self):
        """Flat list of every DocNode in the document (recursing loop / group bodies).
        Used by the display-carry heuristic's forward reachability walk."""
        out = []

        def rec(children):
            for ch in children:
                if isinstance(ch, DocNode):
                    out.append(ch)
                elif isinstance(ch, (DocLoop, DocGroupCall)):
                    rec(ch.children)
        rec(self._root)
        return out

    def _carry_reaches_display(self, carry):
        """True iff the loop carry's post-loop output (out_placeholder) forward-reaches a
        height/normal CAPTURE through the document's connection graph — the "feeds the
        downstream chain toward the height capture" signal for the multi-carry display pick."""
        display_nodes = {id(c.node) for c in self._captures
                         if set(c.channels) & set(self._DISPLAY_CHANNELS)}
        nodes = self._all_doc_nodes()
        frontier = {id(carry.out_placeholder)}
        consumed = set()
        progressed = True
        while progressed:
            progressed = False
            for n in nodes:
                if id(n) in consumed:
                    continue
                if any(id(ref.node) in frontier for (_in, ref) in n.connections):
                    consumed.add(id(n))
                    frontier.add(id(n))
                    progressed = True
                    if id(n) in display_nodes:
                        return True
        return False

    def _expand_loop(self, loop, g, node_real, node_name, sub, cap_map, prefix, index_env,
                     display_target=None):
        # A T.loop elaborates to a REAL LoopModule composite (the Houdini subnet model) —
        # NOT a flat unroll. The body is expanded ONCE into the module's nested subgraph;
        # carry reads/writes become boundary promotions + carries, loop-invariant externals
        # become non-carry promoted inputs, and L.i params become per-iteration feed TABLES.
        # The C++ LoopModuleInst iterates the nested body _count times (carry buffers fed back,
        # per-iteration cook cache => the N->N+k Merkle reuse the unroll had). Bypass is now the
        # ordinary module splice: DocLoop.bypassed sets the composite's .bypassed and the C++
        # resolver aliases the loop's output to its first connected input (the carry initial).
        # An EMPTY / identity loop (NO carry is written in the body — e.g. the editor's
        # add_loop wraps a node in an empty loop, grown later) is a pure PASS-THROUGH:
        # count no-op iterations leave every carry at its INITIAL. Alias each carry's
        # post-loop output to its initial (byte-identical to the old unroll's empty-body
        # path) and emit NO module — an identity loop must not perturb the bake.
        #
        # FULLY-BYPASSED BODY (f2): a carry whose sole body producer(s) are ALL bypassed
        # hops (via _resolve_body_bypass) back to the carry's OWN read placeholder — the
        # iteration leaves it unchanged, so the loop is the identity it already supports for
        # the empty body. When EVERY carry passes through this way (e.g. the editor bypassed
        # the only body op), route it through the same INITIAL-alias path so bypassing the
        # only body op == bypassing the whole loop (never RAISE). A carry that resolves to an
        # EXTERNAL node (not its own placeholder, not a body node) is still genuinely
        # malformed and falls through to the loud raise in the carry-resolution loop below.
        def _passthrough(c):
            if c.body_out_ref is None:
                return True                          # never written == identity (empty body)
            return _resolve_body_bypass(c.body_out_ref).node is c.placeholder
        if all(_passthrough(c) for c in loop.carries.values()):
            for name, c in loop.carries.items():
                sub[id(c.out_placeholder)] = _resolve(c.initial_ref, node_real, sub)
            # DISPLAY of an IDENTITY loop (empty / fully-bypassed body): its "output" IS the
            # carry initial — point the display marker at the (first) carry's initial producer
            # so displaying such a loop shows the pass-through value (best-effort: only when the
            # initial resolves to a real elaborated top-level module).
            if loop is display_target:
                init_node = next(iter(loop.carries.values())).initial_ref.node
                nm = node_name.get(id(init_node))
                if nm is not None:
                    node_name[id(loop)] = nm
            return
        # DISPLAY of this loop (Fix 1): its "output" is the height-typed carry. A single-carry
        # loop already exposes its carry as "Out"; a MULTI-carry loop (erox carries h + aux)
        # picks deterministically — the carry whose output feeds the downstream chain toward a
        # height/normal capture if unambiguous, else the first carry (all terrain carries are
        # hf-image-typed) — and promotes THAT carry's boundary output as "Out" so the C++
        # re-point (outputNamed("Out")) reaches it.
        display_carry = None
        if loop is display_target and len(loop.carries) > 1:
            reaching = [name for name, c in loop.carries.items()
                        if self._carry_reaches_display(c)]
            display_carry = reaching[0] if len(reaching) == 1 else next(iter(loop.carries))
        loop_module = _terrain.LoopModule.createShared()
        loop_module.count = loop.count
        if loop.bypassed:
            loop_module.bypassed = True
        body_graph = loop_module.subgraph
        emit       = _LoopEmit(loop, loop_module, node_real, sub, display_carry=display_carry)
        # expand the body ONCE into the nested subgraph (loop_emit intercepts carry / external
        # reads into promotions and L.i params into feed tables).
        b_node_real, b_node_name, b_sub = {}, {}, {}
        self._expand(loop.children, body_graph, b_node_real, b_node_name, b_sub, cap_map,
                     "", index_env, loop_emit=emit)
        # each carry's next value comes from its body_out_ref (an inner module's output plug),
        # hopped through any BYPASSED body node so the promoted output points at the live
        # producer (the bypassed node then goes undemanded — never force-computed; f2).
        for name, c in loop.carries.items():
            if c.body_out_ref is None:
                raise RuntimeError(
                    f"loop carry {name!r} is never written in the body — assign L.{name} = ... "
                    f"(a carry with no body write has no next-iteration producer).")
            out_ref = _resolve_body_bypass(c.body_out_ref)
            onode = out_ref.node
            if id(onode) not in b_node_name:
                raise RuntimeError(
                    f"loop carry {name!r} output resolves OUTSIDE the loop body "
                    f"(L.{name} = a carry read, an external node, or a fully-bypassed body) — "
                    f"the carry's next value must be produced by a body node.")
            emit.add_carry_output(name, b_node_name[id(onode)], out_ref.plug_name)
        emit.finalize(index_env)                 # promote + addCarry + iter-feed TABLES + reshape
        loop_name = prefix + loop.path
        g.addModule(loop_module, loop_name)
        node_name[id(loop)] = loop_name          # the loop's elaborated module name (bypass marker)
        # SELECT-AS-OUTPUT of a loop-body node: a single-carry loop exposes its carry as the
        # module's "Out", so displaying the body node that WRITES the carry maps to the loop's
        # LAST-iteration value (the composite-model equivalent of the old unroll's last i).
        if len(loop.carries) == 1:
            (only_carry,) = tuple(loop.carries.values())
            node_name[id(only_carry.body_out_ref.node)] = loop_name
        # RESHAPE SELF-DEFENSE: the pyext plug proxies return None for UNKNOWN plugs, so a
        # reshape that silently produced no boundary plugs (proven failure mode: elaborate
        # BEFORE the engine init — LoopModule reshape needs the initialized engine since
        # step 3) would store None into `sub` and surface much later as a misleading
        # "unresolved placeholder". Name the real failure HERE.
        def _boundary(plugs, bname):
            p = getattr(plugs, bname)
            if p is None:
                raise RuntimeError(
                    f"T.loop {loop.path!r}: LoopModule reshape produced no boundary plug "
                    f"{bname!r} — elaborate() before engine init? (document build is init-free; "
                    f"elaboration is not — run it post-appinit, e.g. on the GPU thread)")
            return p
        # host wiring: each carry's boundary INPUT plug (present iff the carry is READ in the
        # body — `promoted_inputs` records the promotion) takes the carry's INITIAL value;
        # loop-invariant externals take their source.
        promoted_in = {b for (b, _m, _p) in emit.promoted_inputs}
        for name, c in loop.carries.items():
            bname = emit.carry_in_boundary[name]
            if bname in promoted_in:             # unread carry => no boundary input (== unroll)
                g.connect(_boundary(loop_module.inputs, bname), _resolve(c.initial_ref, node_real, sub))
        for (boundary, ext_out) in emit.ext_sources:
            g.connect(_boundary(loop_module.inputs, boundary), ext_out)
        # downstream reads of L.<carry> (the carry out-placeholder) resolve to the LoopModule's
        # boundary OUTPUT plug (the final-iteration carry value).
        for name, c in loop.carries.items():
            sub[id(c.out_placeholder)] = _boundary(loop_module.outputs, emit.carry_out_boundary[name])


def _set_param(real, kind, name, value):
    if kind == "inputs":
        setattr(real.inputs, name, value)
    else:
        setattr(real, name, value)


def _resolve(out_plug, node_real, sub):
    node = out_plug.node
    # sub is the substitution map: loop-carry OUT-placeholders (-> the LoopModule's boundary
    # output plug), switch OUTPUT PLACEHOLDERS, and the elaborate-time OUTPUT ALIAS of a
    # bypassed DocGroupCall (its output node -> its first terrain-typed input arg). All resolve
    # to an already-real out-plug — check it first for any node type (placeholder or DocNode).
    r = sub.get(id(node))
    if r is not None:
        return r
    if isinstance(node, _Placeholder):
        raise RuntimeError(f"unresolved placeholder {node.name!r}")
    # A bypassed NODE (not a construct) is MATERIALIZED (with real.bypassed=True) and its
    # consumers wire to its own output plug — the C++ resolveConnectedOutput does the pass-
    # through hop at wiring time. No Python-side aliasing here for nodes (edge stays raw; L2).
    real = node_real.get(id(node))
    if real is None:
        raise RuntimeError(f"unresolved reference to node {node.local_name!r}")
    r = getattr(real.outputs, out_plug.plug_name)
    if r is None:
        # the pyext outputs proxy returns None for unknown plugs — fail LOUDLY.
        raise RuntimeError(
            f"node {node.local_name!r} [{node.clazz_name}] has no output plug "
            f"{out_plug.plug_name!r}")
    return r


def _resolve_body_bypass(out_ref):
    """Hop a body-node reference through any BYPASSED producer body nodes — the elaborate-
    time mirror of the C++ resolveConnectedOutput splice. Needed for a carry's promoted
    OUTPUT: it reads its inner producer's Out DIRECTLY (forced root), so unlike an ordinary
    consumer edge it never passes through the nested sorter's bypass hop. Without this, a
    bypassed carry-writer is force-computed and its (live) output leaks into the carry
    (f2). Terrain edges are homogeneous (HfImage), so the FIRST connection is the pass-
    through input; recurses across bypass chains, self-defends against cycles."""
    seen = set()
    node = out_ref.node
    while (isinstance(node, DocNode) and node.bypassed
           and node.connections and id(node) not in seen):
        seen.add(id(node))
        out_ref = node.connections[0][1]   # first connected input's source (the pass-through)
        node = out_ref.node
    return out_ref


class _LoopEmit:
    """Collects the boundary promotions / carries / iter-feed tables while a T.loop BODY is
    expanded ONCE into a LoopModule's nested subgraph (replaces the flat unroll). A carry read
    inside the body -> a promoted INPUT (many inner plugs may share one carry boundary name;
    the C++ reshape dedupes it). A read of a node created OUTSIDE the loop (loop-invariant) ->
    a NON-carry promoted input wired to that external source in the host. An L.i param -> a
    per-iteration feed TABLE evaluated from the SAME _IterExpr the unroll would eval (bit-exact)."""

    def __init__(self, loop, loop_module, outer_node_real, outer_sub, display_carry=None):
        self.loop            = loop
        self.module          = loop_module
        self.outer_node_real = outer_node_real
        self.outer_sub       = outer_sub
        # per-carry boundary plug names. A SINGLE-carry loop uses the plain "In"/"Out"
        # (the SubGraphModule convention) so the module has an "Out" the select-as-output
        # marker can point at (display of the carry-writing body node == last iteration).
        # Multi-carry loops disambiguate with "{name}#in"/"{name}#out". (# keeps boundary
        # names clear of real terrain plug names; names never affect the baked pixels.)
        # DISPLAY (Fix 1): when a MULTI-carry loop is the display target, `display_carry`
        # names the chosen height-typed carry — its OUTPUT boundary is renamed "Out" so the
        # C++ bake's outputNamed("Out") reaches it (its INPUT boundary stays "{name}#in";
        # the rename is UNIQUE — other carries keep "{name}#out"). display_carry is None for
        # every non-display elaboration, so the normal bake is byte-identical.
        single = (len(loop.carries) == 1)
        self.carry_in_boundary  = {name: ("In"  if single else f"{name}#in")  for name in loop.carries}
        self.carry_out_boundary = {name: ("Out" if single else f"{name}#out") for name in loop.carries}
        if display_carry is not None:
            self.carry_out_boundary[display_carry] = "Out"
        self._placeholder_carry = {id(c.placeholder): name for name, c in loop.carries.items()}
        self.promoted_inputs  = []   # [(outer, inner_module, inner_plug)]
        self.promoted_outputs = []   # [(outer, inner_module, inner_plug)]
        self.iter_feeds       = []   # [(inner_module, inner_plug, ExprIR node)]
        self._ext_boundary    = {}   # (id(src_node), plug_name) -> boundary name (dedupe)
        self.ext_sources      = []   # [(boundary, external_real_out_plug)]

    def _carry_of_placeholder(self, node):
        return self._placeholder_carry.get(id(node))

    def wire_input(self, body_graph, real, inner_module, inner_plug, out_plug, nested_node_real, nested_sub):
        src = out_plug.node
        cname = self._carry_of_placeholder(src)
        if cname is not None:                     # a carry read -> promoted (carry) input
            self.promoted_inputs.append((self.carry_in_boundary[cname], inner_module, inner_plug))
            return
        if id(src) in nested_node_real or id(src) in nested_sub:  # body-internal edge
            real_out = _resolve(out_plug, nested_node_real, nested_sub)
            body_graph.connect(getattr(real.inputs, inner_plug), real_out)
            return
        # loop-invariant external -> a NON-carry promoted input (dedupe multiple reads of one src)
        key = (id(src), out_plug.plug_name)
        boundary = self._ext_boundary.get(key)
        if boundary is None:
            ext_out  = _resolve(out_plug, self.outer_node_real, self.outer_sub)
            boundary = f"ext{len(self.ext_sources)}#in"
            self._ext_boundary[key] = boundary
            self.ext_sources.append((boundary, ext_out))
        self.promoted_inputs.append((boundary, inner_module, inner_plug))

    def add_iter_feed(self, inner_module, inner_plug, expr):
        self.iter_feeds.append((inner_module, inner_plug, expr))

    def add_carry_output(self, name, inner_module, inner_plug):
        self.promoted_outputs.append((self.carry_out_boundary[name], inner_module, inner_plug))

    def finalize(self, index_env):
        m = self.module
        for (outer, im, ip) in self.promoted_inputs:
            m.promoteInput(outer, im, ip)
        for (outer, im, ip) in self.promoted_outputs:
            m.promoteOutput(outer, im, ip)
        for name in self.loop.carries:
            m.addCarry(name, self.carry_in_boundary[name], self.carry_out_boundary[name])
        # the value TABLE is evaluated EXACTLY as the unroll's _set_param did (doc.py _eval) —
        # regenerated on every elaborate, so a count edit covers the table length automatically.
        for (im, ip, expr) in self.iter_feeds:
            values = [float(_eval_iter_node(expr, {**index_env, id(self.loop): k})) for k in range(self.loop.count)]
            m.addIterFeedTable(im, ip, values)
        m.reshape()   # build the boundary HfImage plugs from the promotion tables


def _last_action_value(docnode, kind, name):
    """Last recorded concrete value for (kind, name) — the type template a re-evaluated
    param expr coerces back to (int baked prop stays int)."""
    v = None
    for (k, n, val) in docnode.param_actions:
        if k == kind and n == name:
            v = val
    return v


def reeval_captured_params(doc):
    """Refresh every captured plug/prop's recorded CONCRETE value from its param expr against
    the CURRENT params table — called after a document-parameter mutation so param_actions
    (hence doc-JSON, undo diffs, the pywriter literal fallback) stays honest. elaborate reads
    the exprs directly, so the bake is correct either way; this keeps the serialized snapshot
    consistent with a fresh re-trace (the guard's pristine compare)."""
    def _walk(children):
        for ch in children:
            if isinstance(ch, DocNode):
                for (kind, name), expr in ch.param_exprs.items():
                    tmpl = _last_action_value(ch, kind, name)
                    ev = _eval_param_node(expr, doc.params)
                    ev = _coerce_like(tmpl, ev) if tmpl is not None else float(ev)
                    for i, (k, n, _v) in enumerate(ch.param_actions):
                        if k == kind and n == name:
                            ch.param_actions[i] = (kind, name, ev)
            if isinstance(ch, (DocLoop, DocGroupCall)):
                _walk(ch.children)
    _walk(doc._root)


def _effective_param(docnode, name):
    """Last-write-wins recorded param value on a DocNode (any kind), or None."""
    v = None
    for (_k, n, val) in docnode.param_actions:
        if n == name:
            v = val
    return v


def _channel_list(spec):
    return [c for c in str(spec).split(",") if c]


# --- editor topology introspection (E3 honesty) ------------------------------
# The node editor renders the DOCUMENT (L1), so it needs the DECLARED plug surface of a
# node's module class, not merely what a live trace happened to wire. Two helpers below
# feed the canvas so it shows the TRUE topology regardless of how a document was built
# (fresh trace, editor mutation, or a doc-JSON reload).

_declared_outputs_cache = {}


def declared_output_plugs(node):
    """ALL declared output plug names of a DocNode's module class — regardless of whether
    each currently has a consumer — so the editor lists (and can wire from) every real
    output, not only the connected ones (a node's unconnected outputs were invisible +
    unwireable before). Reflection via dflow.plugSpec (survives a doc-JSON reload, where the
    trace-recorded _out_names are gone); cached per class. Falls back to the trace-recorded
    output names, then the single 'Out' (a non-capture always produces Out; a CaptureModule
    declares none — it is a sink)."""
    cn = node.clazz_name
    cached = _declared_outputs_cache.get(cn)
    if cached is not None:
        return list(cached)
    outs = None
    try:
        spec = _dflow.plugSpec("terrain::%sData" % cn)
        if spec is not None:
            outs = [p["name"] for p in spec["outputs"]]
    except Exception:
        outs = None
    if outs is not None:
        _declared_outputs_cache[cn] = list(outs)
        return list(outs)
    # reflection unavailable (unknown class / pre-init) — best-effort from the trace.
    if node._out_names:
        return sorted(node._out_names)
    return [] if cn == "CaptureModule" else ["Out"]


def loop_external_refs(doc, loop):
    """Ordered [(port_name, ref)] — the loop-invariant EXTERNAL producers a loop body reads:
    a node created OUTSIDE the loop that is neither a carry nor a body node. elaborate promotes
    each to a NON-carry loop input (_LoopEmit ext_sources); the editor must too, or the loop
    renders as a graph component DISCONNECTED from its external feeders (owner-visible: opening a
    loop asset shows two islands). port_name is derived from the producer's STABLE tree-path so
    the parent loop input port and any consumer of this helper agree across a doc-JSON reload.
    Deduped by (producer, plug); the parent scope order is preserved."""
    keys = {id(o): k for (_pk, k, o) in tree_paths(doc)}
    carry_ph = {id(c.placeholder) for c in loop.carries.values()}
    body_ids = set()

    def _collect(children):
        for ch in children:
            if isinstance(ch, DocNode):
                body_ids.add(id(ch))
            elif isinstance(ch, (DocLoop, DocGroupCall)):
                _collect(ch.children)
    _collect(loop.children)

    out, seen = [], set()

    def _refs_of(ch):
        if isinstance(ch, DocNode):
            return [r for (_i, r) in ch.connections]
        if isinstance(ch, DocLoop):
            r = []
            for c in ch.carries.values():
                r += [c.initial_ref, c.body_out_ref]
            return r
        if isinstance(ch, DocGroupCall):
            r = [v for v in ch.args.values() if isinstance(v, _DocOutPlug)]
            if ch.output_ref is not None:
                r.append(ch.output_ref)
            return r
        if isinstance(ch, DocSwitch):
            return list(ch.branches.values())
        return []

    def _scan(children):
        for ch in children:
            for ref in _refs_of(ch):
                if ref is None:
                    continue
                rk = id(ref.node)
                if rk in body_ids or rk in carry_ph:
                    continue
                key = (rk, ref.plug_name)
                if key in seen:
                    continue
                seen.add(key)
                base = keys.get(rk) or getattr(ref.node, "local_name", "ext")
                out.append(("ext:%s#%s" % (base, ref.plug_name), ref))
            if isinstance(ch, (DocLoop, DocGroupCall)):
                _scan(ch.children)
    _scan(loop.children)
    return out


_SCATTER_CHAN_RE = _re.compile(r"^_scatter_(.+)_w(\d+)$")


def scatter_weight_captures(doc):
    """Group the document's AUTO-CAPTURED scatter-weight channels by scatter SINK name.

    base.scatter() records each placement type's weight field as a hidden
    `_scatter_<name>_w<idx>` CaptureModule (the CPU placer reads them post-bake). A scatter
    is a real BAKE SINK (a mask-driven placement set), but nothing groups those captures, so
    the sink itself was invisible in the canvas. Returns {name -> ordered [(idx, capture_node,
    weight_producer_ref)]} so the editor can synthesize ONE visible sink node per scatter with
    an input edge from each weight's producer."""
    sinks = {}
    for (_pk, _k, obj) in tree_paths(doc):
        if not (isinstance(obj, DocNode) and obj.clazz_name == "CaptureModule"):
            continue
        m = _SCATTER_CHAN_RE.match(str(_effective_param(obj, "channel") or ""))
        if m is None:
            continue
        name, idx = m.group(1), int(m.group(2))
        producer = obj.connections[0][1] if obj.connections else None
        sinks.setdefault(name, []).append((idx, obj, producer))
    for name in sinks:
        sinks[name].sort(key=lambda t: t[0])
    return sinks


# --- TerrainNode wrapping a placeholder (loop carry read) --------------------

class _PlaceholderModule:
    __slots__ = ("_docnode", "_name")

    def __init__(self, placeholder):
        self._docnode = placeholder
        self._name = placeholder.name


def _placeholder_node(placeholder, plug="Out"):
    return TerrainNode(_PlaceholderModule(placeholder), _DocOutPlug(placeholder, plug))


def _node_ref(terrain_node):
    """Extract the _DocOutPlug a TerrainNode carries (its wired source)."""
    return terrain_node._output_plug


# --- T.loop (L-A + V-A) ------------------------------------------------------

class LoopHandle:
    """`L` in `with T.loop(...) as L`. Attribute access reads/writes named carries;
    `L.i` is the per-iteration index for scalar param arithmetic."""

    def __init__(self, loop):
        object.__setattr__(self, "_loop", loop)
        object.__setattr__(self, "_current", {})     # name -> TerrainNode (during body)
        object.__setattr__(self, "_exited", False)

    def __getattr__(self, name):
        loop = object.__getattribute__(self, "_loop")
        if name == "i":
            return _IterCapture.index(loop)
        carries = loop.carries
        if name in carries:
            if object.__getattribute__(self, "_exited"):
                return _placeholder_node(carries[name].out_placeholder)
            return object.__getattribute__(self, "_current")[name]
        raise AttributeError(name)

    def __setattr__(self, name, value):
        loop = object.__getattribute__(self, "_loop")
        if name in loop.carries:
            if not isinstance(value, TerrainNode):
                raise TypeError(
                    f"loop carry {name!r} must be assigned a terrain node "
                    f"(got {type(value).__name__})")
            object.__getattribute__(self, "_current")[name] = value
            return
        raise AttributeError(f"unknown loop carry {name!r} (declare it in T.loop(...))")


class _LoopCtx:
    def __init__(self, doc, loop, handle):
        self._doc = doc
        self._loop = loop
        self._handle = handle

    def __enter__(self):
        self._doc._append_child(self._loop)
        self._doc._push_scope(self._loop)
        cur = object.__getattribute__(self._handle, "_current")
        for name, c in self._loop.carries.items():
            cur[name] = _placeholder_node(c.placeholder)
        return self._handle

    def __exit__(self, exc_type, exc, tb):
        # unwind the scope FIRST so an exception in the body leaves the trace clean.
        self._doc._pop_scope()
        if exc_type is not None:
            _take_pending_iter()   # drop any stray L.i pending; the body exception wins
            return False
        # a coerced L.i that no plug set claimed (e.g. math.sin(L.i)) is a LOUD error.
        assert_no_pending_iter("at the end of a T.loop body")
        cur = object.__getattribute__(self._handle, "_current")
        for name, c in self._loop.carries.items():
            c.body_out_ref = _node_ref(cur[name])
        _check_loop_orphans(self._loop)
        object.__setattr__(self._handle, "_exited", True)
        return False


def _check_loop_orphans(loop):
    """LOUD error (ops-self-defend): a loop body node that flows to no carry is a
    bug — name the offenders. Reachability walks back from every carry output."""
    body_ids = {id(ch): ch for ch in loop.children if isinstance(ch, DocNode)}
    reachable = set()
    stack = []
    for c in loop.carries.values():
        op = c.body_out_ref
        if op is not None and isinstance(op.node, DocNode) and id(op.node) in body_ids:
            stack.append(op.node)
    while stack:
        n = stack.pop()
        if id(n) in reachable:
            continue
        reachable.add(id(n))
        for (_in_name, src) in n.connections:
            if isinstance(src.node, DocNode) and id(src.node) in body_ids:
                stack.append(src.node)
    orphans = [ch.local_name for cid, ch in body_ids.items() if cid not in reachable]
    if orphans:
        raise RuntimeError(
            "T.loop body creates node(s) that flow to no carry (orphans): "
            + ", ".join(sorted(orphans))
            + " — every node in a loop body must feed a carry (L.<name>).")


def loop(count, *, name=None, bypassed=False, **carries):
    """with T.loop(N, carry=init, ...) as L: (L-A). kwargs declare the initial
    carries; the body reads/writes L.<carry> and may use L.i in scalar param
    arithmetic; carries are read back off L after the block. Count is STRUCTURAL.
    bypassed=True authors the loop already bypassed (a SAVED .py can carry the flag):
    the body still traces (recorded) but elaborate skips the unroll and aliases each
    carry's post-loop output to its INITIAL ref."""
    doc = _current_doc("T.loop")
    if not carries:
        raise ValueError("T.loop requires at least one carry kwarg (e.g. T.loop(16, h=h0))")
    ordered = {}
    for cname, init in carries.items():
        if not isinstance(init, TerrainNode):
            raise TypeError(
                f"T.loop carry {cname!r} initial value must be a terrain node "
                f"(got {type(init).__name__})")
        ordered[cname] = _Carry(cname, _node_ref(init))
    path = name if name else doc._next_group_path("loop")
    docloop = DocLoop(doc, count, path, ordered)
    docloop.bypassed = bool(bypassed)
    return _LoopCtx(doc, docloop, LoopHandle(docloop))


# --- @T.group (G-A) ----------------------------------------------------------

def group(fn):
    """@T.group — args become promoted plugs; each call site expands independently.
    Bare-decorator form only (G-A)."""
    import functools

    @functools.wraps(fn)
    def _wrapper(*args, **kwargs):
        doc = _current_doc("@T.group")
        path = doc._next_group_path(fn.__name__)
        arg_model = {}
        try:
            import inspect
            names = list(inspect.signature(fn).parameters.keys())
        except (TypeError, ValueError):
            names = []
        for i, a in enumerate(args):
            key = names[i] if i < len(names) else f"arg{i}"
            arg_model[key] = _node_ref(a) if isinstance(a, TerrainNode) else a
        for k, a in kwargs.items():
            arg_model[k] = _node_ref(a) if isinstance(a, TerrainNode) else a
        call = DocGroupCall(doc, path, fn.__name__, arg_model)
        doc._append_child(call)
        doc._push_scope(call)
        try:
            result = fn(*args, **kwargs)
        finally:
            doc._pop_scope()
        if isinstance(result, TerrainNode):
            call.output_ref = _node_ref(result)
        return result

    return _wrapper


# --- T.switch (S-B) ----------------------------------------------------------

def switch(selector, **branches):
    """T.switch(sel, name_a=expr_a, name_b=expr_b) — named branches, EAGER trace
    (all branches already live in the document; the selector picks one). The
    selector is STRUCTURAL (change it -> re-trace/re-elaborate)."""
    doc = _current_doc("T.switch")
    if not branches:
        raise ValueError("T.switch requires at least one named branch")
    for bname, expr in branches.items():
        if not isinstance(expr, TerrainNode):
            raise TypeError(
                f"T.switch branch {bname!r} must be a terrain node "
                f"(got {type(expr).__name__})")
    key = _switch_key(selector, branches)
    if key not in branches:
        raise KeyError(
            f"T.switch selector {selector!r} does not name a branch; "
            f"have {sorted(branches)}")
    marker = DocSwitch(selector,
                       {b: _node_ref(e) for b, e in branches.items()}, key)
    doc._append_child(marker)
    # downstream wires to the SWITCH (its placeholder), not the branch head directly, so
    # elaboration is the selection authority — not the trace-time `selector` value.
    return _placeholder_node(marker.out_placeholder)


def _switch_key(selector, branches):
    if isinstance(selector, str):
        return selector
    if isinstance(selector, bool):
        return sorted(branches)[1 if selector else 0]
    if isinstance(selector, int):
        keys = list(branches.keys())
        if 0 <= selector < len(keys):
            return keys[selector]
    return selector


# --- helpers -----------------------------------------------------------------

def _current_doc(what):
    g = current_graph()
    doc = getattr(g, "_doc", None)
    if not isinstance(doc, TerrainDoc):
        raise RuntimeError(
            f"{what} used outside a HeightField trace — the explicit terrain "
            f"constructs are only available inside a HeightField subclass __init__.")
    return doc


###############################################################################
# doc-JSON — the editor-native save format + undo-checkpoint format + the S0
# round-trip gate. A plain dict tree (json.dumps-ready). Refs are (doc-id, plug)
# pairs; values carry an explicit type tag so int/float/vec2 survive exactly.
###############################################################################

_DOC_JSON_VERSION = 1


def _enc_value(v):
    if isinstance(v, bool):
        return ["b", v]
    if isinstance(v, int):
        return ["i", v]
    if isinstance(v, float):
        return ["f", v]
    if isinstance(v, str):
        return ["s", v]
    if isinstance(v, _vec2):
        return ["v2", v.x, v.y]
    if isinstance(v, (tuple, list)):     # numeric tuple param (e.g. center=(0.0, 0.0))
        return ["t", [_enc_value(x) for x in v]]
    raise TypeError(f"cannot doc-encode param value of type {type(v).__name__}")


def _dec_value(enc):
    tag = enc[0]
    if tag == "b":
        return bool(enc[1])
    if tag == "i":
        return int(enc[1])
    if tag == "f":
        return float(enc[1])
    if tag == "s":
        return str(enc[1])
    if tag == "v2":
        return _vec2(float(enc[1]), float(enc[2]))
    if tag == "t":
        return tuple(_dec_value(x) for x in enc[1])
    raise ValueError(f"bad doc value tag {tag!r}")


def _enc_param_table_value(table, name):
    """Serialize one document-parameter's value (E0 part 2). A TAGGED param emits the
    ratified typed-literal node `{"<unit>": value}` (foreign-JSON-tooling-proof: the kind
    no longer rides the bare number type); a plain param stays the plain `_enc_value` form
    (raw numbers as before — additive, so pre-part-2 doc-JSON loads unchanged)."""
    tag = table.tag_of(name)
    v = table.get(name)
    if tag is not None:
        return {tag: float(v)}
    return _enc_value(v)


def _dec_param_table_value(enc):
    """Inverse of _enc_param_table_value: returns (value, tag). The tagged-node form is a
    dict with a single unit key; the plain form is the `_enc_value` list."""
    if isinstance(enc, dict):
        (tag, val), = enc.items()
        if tag not in _units.UNIT_TAGS:
            raise ValueError(
                f"bad terrain doc param unit tag {tag!r}; vocabulary v1 = "
                f"{sorted(_units.UNIT_TAGS)}")
        return float(val), tag
    return _dec_value(enc), None


# The terrain doc-JSON wire forms below walk / build the shared ExprIR nodes but keep the
# EXISTING bytes exactly (the {"param":..}/{"const":..}/{"op":..,"a":..,"b":..} shape) — the IR
# is the in-memory representation only; the persisted format stays frozen until the coordinated
# salt-bump slice (E2.5 S4/Q5). Do NOT change these bytes here.

def _enc_param_expr(e):
    if isinstance(e, _exprir.ParamRef):     # doc-param leaf (kind PARAM_DOC)
        return {"param": e.name}
    if isinstance(e, _exprir.Const):
        return {"const": float(e.value)}
    if e.name == "neg":
        return {"op": "neg", "a": _enc_param_expr(e.args[0])}
    return {"op": e.name, "a": _enc_param_expr(e.args[0]), "b": _enc_param_expr(e.args[1])}


def _dec_param_expr(d, table):
    if "param" in d:
        return _exprir.ParamRef(_exprir.PARAM_DOC, d["param"])
    if "const" in d:
        return _exprir.Const(float(d["const"]))
    op = d["op"]
    if op == "neg":
        return _exprir.Call("neg", _dec_param_expr(d["a"], table))
    return _exprir.Call(op, _dec_param_expr(d["a"], table), _dec_param_expr(d["b"], table))


def _enc_iter(expr, loop_ids):
    if isinstance(expr, _exprir.ParamRef):  # index leaf; name = loop object
        return {"index": loop_ids[id(expr.name)]}
    if isinstance(expr, _exprir.Const):
        return {"const": float(expr.value)}
    if expr.name == "neg":
        return {"op": "neg", "a": _enc_iter(expr.args[0], loop_ids)}
    return {"op": expr.name, "a": _enc_iter(expr.args[0], loop_ids),
            "b": _enc_iter(expr.args[1], loop_ids)}


def _dec_iter(d, loops_by_id):
    if "index" in d:
        return _exprir.ParamRef("index", loops_by_id[d["index"]])
    if "const" in d:
        return _exprir.Const(float(d["const"]))
    op = d["op"]
    if op == "neg":
        return _exprir.Call("neg", _dec_iter(d["a"], loops_by_id))
    return _exprir.Call(op, _dec_iter(d["a"], loops_by_id), _dec_iter(d["b"], loops_by_id))


class _IdAlloc:
    def __init__(self):
        self._map = {}
        self._next = 0

    def get(self, obj):
        key = id(obj)
        if key not in self._map:
            self._map[key] = self._next
            self._next += 1
        return self._map[key]


def tree_paths(doc):
    """Deterministic editor paths for every document object — the SINGLE keying
    source shared by the outliner model and session references (the display node).
    Segments: DocNode -> local_name; DocLoop/DocGroupCall -> .path; DocSwitch ->
    ordinal 'switchN'. Returns [(parent_key, key, obj)] in tree order."""
    out = []
    switch_ord = [0]

    def _walk(parent_key, children):
        for ch in children:
            if isinstance(ch, DocNode):
                seg = ch.local_name
            elif isinstance(ch, (DocLoop, DocGroupCall)):
                seg = ch.path
            elif isinstance(ch, DocSwitch):
                seg = f"switch{switch_ord[0]}"
                switch_ord[0] += 1
            else:
                seg = "unknown"
            key = f"{parent_key}/{seg}" if parent_key else seg
            out.append((parent_key, key, ch))
            if isinstance(ch, (DocLoop, DocGroupCall)):
                _walk(key, ch.children)

    _walk("", doc._root)
    return out


def find_by_path(doc, path):
    """The document object at a tree_paths() key, or None."""
    for (_pk, key, obj) in tree_paths(doc):
        if key == path:
            return obj
    return None


def _iter_ref_slots(doc):
    """Yield (get, set) accessors over every _DocOutPlug reference in the document —
    node input connections, loop carry refs, switch branch heads, group-call args and
    outputs. The remap surface for delete-with-reconnect."""
    def _walk(children):
        for ch in children:
            if isinstance(ch, DocNode):
                for i in range(len(ch.connections)):
                    yield (lambda c=ch, ix=i: c.connections[ix][1],
                           lambda v, c=ch, ix=i: c.connections.__setitem__(
                               ix, (c.connections[ix][0], v)))
            elif isinstance(ch, DocLoop):
                for c in ch.carries.values():
                    yield (lambda cc=c: cc.initial_ref,
                           lambda v, cc=c: setattr(cc, "initial_ref", v))
                    yield (lambda cc=c: cc.body_out_ref,
                           lambda v, cc=c: setattr(cc, "body_out_ref", v))
                yield from _walk(ch.children)
            elif isinstance(ch, DocGroupCall):
                for name, val in ch.args.items():
                    if isinstance(val, _DocOutPlug):
                        yield (lambda g=ch, n=name: g.args[n],
                               lambda v, g=ch, n=name: g.args.__setitem__(n, v))
                yield (lambda g=ch: g.output_ref,
                       lambda v, g=ch: setattr(g, "output_ref", v))
                yield from _walk(ch.children)
            elif isinstance(ch, DocSwitch):
                for bname in ch.branches:
                    yield (lambda s=ch, b=bname: s.branches[b],
                           lambda v, s=ch, b=bname: s.branches.__setitem__(b, v))
    yield from _walk(doc._root)


def _resolve_out_ref(obj, out_plug):
    """The _DocOutPlug a wire from `obj`'s `out_plug` output carries — the SOURCE side
    of connect. Mirrors the producer map the canvas builds per level: a DocNode drives
    its named out-plug; a construct drives its synthetic output placeholder (loop carry /
    switch out) or its returned ref (group). Loud on an unwireable source."""
    if isinstance(obj, DocNode):
        return _DocOutPlug(obj, out_plug)
    if isinstance(obj, DocLoop):
        c = obj.carries.get(out_plug)
        if c is None:
            raise TerrainDocParamError(
                f"connect: loop {obj.path!r} has no carry output {out_plug!r}; "
                f"have {list(obj.carries)}")
        return _DocOutPlug(c.out_placeholder, "Out")
    if isinstance(obj, DocSwitch):
        return _DocOutPlug(obj.out_placeholder, "Out")
    if isinstance(obj, DocGroupCall):
        if obj.output_ref is None:
            raise TerrainDocParamError(
                f"connect: group {obj.path!r} has no output to wire from")
        return _DocOutPlug(obj.output_ref.node, obj.output_ref.plug_name)
    raise TerrainDocParamError(
        f"connect: a {type(obj).__name__} cannot be a wire source")


def _write_in_slot(obj, in_plug, ref):
    """Write `ref` into `obj`'s `in_plug` native edge slot — the DESTINATION side of
    connect, OVERWRITING whatever fed it (terrain inputs are always fed). Covers exactly
    the ref slots _iter_ref_slots enumerates: DocNode.connections, loop-carry initial_ref,
    group arg, switch branch. Loud on an unknown plug / unwireable destination."""
    if isinstance(obj, DocNode):
        for i, (n, _r) in enumerate(obj.connections):
            if n == in_plug:
                obj.connections[i] = (in_plug, ref)
                return
        # terrain inputs are ALWAYS fed — every real input plug already carries a
        # connection, so a name not among them is not a wireable input; refuse LOUDLY
        # (you re-source an existing input, you never add one via connect).
        raise TerrainDocParamError(
            f"connect: node {obj.local_name!r} [{obj.clazz_name}] has no input plug "
            f"{in_plug!r}; inputs are {[n for (n, _r) in obj.connections]}")
    if isinstance(obj, DocLoop):
        c = obj.carries.get(in_plug)
        if c is None:
            raise TerrainDocParamError(
                f"connect: loop {obj.path!r} has no carry input {in_plug!r}; "
                f"have {list(obj.carries)}")
        c.initial_ref = ref
        return
    if isinstance(obj, DocGroupCall):
        if in_plug not in obj.args or not isinstance(obj.args[in_plug], _DocOutPlug):
            raise TerrainDocParamError(
                f"connect: group {obj.path!r} has no terrain-typed arg {in_plug!r}")
        obj.args[in_plug] = ref
        return
    if isinstance(obj, DocSwitch):
        if in_plug not in obj.branches:
            raise TerrainDocParamError(
                f"connect: switch has no branch {in_plug!r}; have {list(obj.branches)}")
        obj.branches[in_plug] = ref
        return
    raise TerrainDocParamError(
        f"connect: a {type(obj).__name__} cannot be a wire destination")


def connect_edge(doc, dst_key, in_plug, src_key, out_plug):
    """Editor mutation (L2, STRUCTURAL): wire src_key.out_plug -> dst_key.in_plug in the
    native edge store, OVERWRITING the consumer's current source. Both keys are
    tree_paths() keys. The single owner of the terrain document's edge write (the C1
    canvas routes here); loud, catchable refusals (ops self-defend) throughout."""
    dst = find_by_path(doc, dst_key)
    if dst is None:
        raise TerrainDocParamError(f"connect: no document object at dst key {dst_key!r}")
    src = find_by_path(doc, src_key)
    if src is None:
        raise TerrainDocParamError(f"connect: no document object at src key {src_key!r}")
    _write_in_slot(dst, in_plug, _resolve_out_ref(src, out_plug))
    return (src_key, out_plug, dst_key, in_plug)


def delete_node(doc, node):
    """Editor mutation (L2, STRUCTURAL): remove a DocNode from the document,
    reconnecting its consumers to its pass-through (first) input — the delete twin
    of bypass. Captures ARE deletable (they are document content; from-scratch
    authoring needs the channel set editable) — deleting one also drops its
    DocCapture registration. Refusals are LOUD (ops-self-defend): the LAST
    height-channel capture (the live display cannot bake without it — undo brings
    a mistaken delete back), and a source node with consumers has nothing to
    reconnect them to. Re-elaborate + rebake to apply; undo = restore a doc-JSON
    checkpoint."""
    if not isinstance(node, DocNode):
        raise TerrainDocParamError("only document nodes can be deleted (v1)")
    if node.clazz_name == "CaptureModule":
        # the display path REQUIRES height + normal; those two are protected while
        # they are the last of their kind. All OPTIONAL channels are deletable
        # (from-scratch authoring needs the channel set editable).
        chans = _channel_list(_effective_param(node, "channel") or "")
        for protected in ("height", "normal"):
            if protected in chans and not any(
                    c.node is not node and protected in c.channels
                    for c in doc._captures):
                raise TerrainDocParamError(
                    f"cannot delete {node.local_name!r}: it is the LAST {protected!r} "
                    f"capture — the display path requires height+normal; optional "
                    f"channels are deletable")
        doc._captures = [c for c in doc._captures if c.node is not node]
    replacement = node.connections[0][1] if node.connections else None
    slots = []
    for (get, set_) in _iter_ref_slots(doc):
        ref = get()
        if ref is not None and ref.node is node:
            slots.append(set_)
    if slots and replacement is None:
        raise TerrainDocParamError(
            f"cannot delete source node {node.local_name!r} [{node.clazz_name}]: "
            f"consumers depend on it and it has no input to reconnect them to")
    for set_ in slots:
        set_(replacement)

    def _remove(children):
        if node in children:
            children.remove(node)
            return True
        for ch in children:
            if isinstance(ch, (DocLoop, DocGroupCall)) and _remove(ch.children):
                return True
        return False

    if not _remove(doc._root):
        raise TerrainDocParamError(
            f"{node.local_name!r} is not part of this document")


# --- editor ADD mutations (S3-pulled-forward: new nodes / loops) --------------

# The curated add menu is REFLECTION-CARRIED (E1-close; the hand-curated add-ops
# tuple is DELETED): every terrain module class annotated `editor.palette` in its
# C++ describeX contributes its `dsl.verb` — DSL ops whose defaults make a working
# node from a single height input (or none, for a `editor.palette.source`
# generator). The wrappers themselves are replayed, so every default lands as an
# editable recorded param. `editor.palette.sort` preserves the curated menu order;
# `editor.palette.recipe` keys the python insertion recipes (_apply_recipe below):
# flow_erode is a COMPOSITE (inserts its flow3d companion — the canonical pairing);
# fill_closed_basins chains through its .filled output; expr needs an identity
# default source.
_PALETTE_CACHE = None


def _palette():
    """verb -> {clazz, sort, source, recipe} for every palette-annotated terrain
    module class, built from core.dataflow.moduleClasses() (the rtti class tree —
    no hand tables). Cached once; an engine-not-ready (empty) result is NOT cached
    so the first post-init query wins. Curation conflicts fail LOUD."""
    global _PALETTE_CACHE
    if _PALETTE_CACHE is None:
        from orkengine.core import dataflow as _dflow  # lazy: keep doc.py import-cheap
        built = {}
        for c in _dflow.moduleClasses():
            if c.get("family") != "terrain":
                continue
            anns = c.get("annotations", {})
            if not anns.get("editor.palette"):
                continue
            verb = anns.get("dsl.verb")
            if not verb:
                raise TerrainDocParamError(
                    f"palette class {c['name']!r} is annotated editor.palette but "
                    f"carries no dsl.verb — annotate the curated verb in its describeX")
            if verb in built:
                raise TerrainDocParamError(
                    f"palette verb {verb!r} is annotated on BOTH "
                    f"{built[verb]['clazz']!r} and {c['name']!r} — curate ONE class "
                    f"per verb")
            built[verb] = {
                "clazz": c["name"],
                "sort": int(anns.get("editor.palette.sort", 1 << 30)),
                "source": bool(anns.get("editor.palette.source", False)),
                "recipe": anns.get("editor.palette.recipe"),
            }
        if not built:
            return {}          # engine not initialized yet — retry next call
        _PALETTE_CACHE = built
    return _PALETTE_CACHE


def editor_add_ops():
    """The add-menu verb tuple in curated order (editor.palette.sort, then name) —
    the public palette surface the outliner / canvas add menus consume."""
    pal = _palette()
    return tuple(sorted(pal, key=lambda v: (pal[v]["sort"], v)))


def _palette_entry_or_raise(op_name):
    """The palette entry for a verb, or a LOUD refusal (unknown verb / engine not
    initialized — either way the add cannot proceed)."""
    pal = _palette()
    ent = pal.get(op_name)
    if ent is None:
        have = sorted(pal) if pal else "(none — engine not initialized)"
        raise TerrainDocParamError(
            f"add: unknown op {op_name!r}; have {have}")
    return ent


def _apply_recipe(op_name, op, recipe, tn_in):
    """Insertion RECIPES — python code KEYED by the class's editor.palette.recipe
    annotation (no name-matched hand tuple). A recipe makes the op's defaults land
    as a WORKING node from one height input; an unknown key fails LOUD (a class
    annotated with a recipe this code cannot perform must never half-insert)."""
    from . import ops as _ops
    if recipe is None:
        return op(tn_in)
    if recipe == "expr":
        return op("ctx.input(0)", inputs=[tn_in])  # identity default; user edits the source
    if recipe == "flow_erode":
        f = _ops.flow3d(tn_in)          # composite: the canonical flow pairing
        return op(tn_in, f.discharge)
    if recipe == "fill_closed_basins":
        return op(tn_in).filled         # struct result: chain through .filled
    raise TerrainDocParamError(
        f"add: {op_name!r} is annotated with unknown insertion recipe {recipe!r} — "
        f"teach doc._apply_recipe or fix the class annotation")


def _scope_of(doc, child):
    """(children_list, owner_chain) containing `child`; owner_chain = group/loop
    objects from root down to the owning scope (for scope-correct mini-traces)."""
    def _walk(children, owners):
        if child in children:
            return children, owners
        for ch in children:
            if isinstance(ch, (DocLoop, DocGroupCall)):
                r = _walk(ch.children, owners + [ch])
                if r is not None:
                    return r
        return None
    r = _walk(doc._root, [])
    if r is None:
        raise TerrainDocParamError(
            f"{getattr(child, 'local_name', getattr(child, 'path', '?'))!r} "
            f"is not part of this document")
    return r


def _uniquify_in_scope(scope_children, node):
    """Sibling DocNode names must be unique (tree_paths keys). Bump the new node's
    name to the first free {base}_{n} on collision."""
    used = {ch.local_name for ch in scope_children
            if isinstance(ch, DocNode) and ch is not node}
    if node.local_name not in used:
        return
    base = node.local_name.rsplit("_", 1)[0]
    n = 0
    while f"{base}_{n}" in used:
        n += 1
    node.local_name = f"{base}_{n}"


def _tnode_for_ref(ref):
    """A DSL TerrainNode wrapping an existing document output ref — the input handle
    for a mini-trace op replay."""
    if isinstance(ref.node, _Placeholder):
        return _placeholder_node(ref.node, ref.plug_name)
    return TerrainNode(ref.node.proxy, ref)


def _mini_trace(doc, owners, fn):
    """Run `fn` (DSL op wrapper calls) tracing INTO this document at the given scope
    chain — the editor's add-node replay (same recording path as an authored trace)."""
    prev = enter_trace(doc.graph)
    for o in owners:
        doc._push_scope(o)
    try:
        return fn()
    finally:
        for _ in owners:
            doc._pop_scope()
        leave_trace(prev)
        _clear_param_pending()
        assert_no_pending_iter("after an editor add-node trace")


def _out_ref_slots(doc, node):
    """Setters for every reference to `node`'s PRIMARY ('Out') output — the chain
    rewire surface (non-primary plugs stay on the original node)."""
    slots = []
    for (get, set_) in _iter_ref_slots(doc):
        ref = get()
        if ref is not None and ref.node is node and ref.plug_name == "Out":
            slots.append(set_)
    return slots


def add_op_node(doc, op_name, after, name=None):
    """Editor mutation (L2, STRUCTURAL): insert a NEW op node right after `after` in
    its scope. Chain semantics: the new node takes after's primary output as input,
    and after's former primary consumers rewire to the new node. A SOURCE op (fbm)
    starts a new branch instead (no input, no rewire)."""
    from . import ops as _ops
    if not isinstance(after, DocNode):
        raise TerrainDocParamError("add: select a document NODE to insert after")
    ent = _palette_entry_or_raise(op_name)
    children, owners = _scope_of(doc, after)
    idx = children.index(after)
    is_source = ent["source"]
    slots = [] if is_source else _out_ref_slots(doc, after)
    op = getattr(_ops, op_name)

    def _make():
        if is_source:
            return op()
        tn_in = _tnode_for_ref(_DocOutPlug(after, "Out"))
        return _apply_recipe(op_name, op, ent["recipe"], tn_in)

    n_before = len(children)
    tn = _mini_trace(doc, owners, _make)
    new_ref = tn._output_plug
    new_node = new_ref.node
    for set_ in slots:
        set_(new_ref)
    # the mini-trace appended at scope end (possibly SEVERAL nodes for composites) —
    # move them, order-preserved, into chain position so elaborate's creation-order
    # resolution holds (producers before consumers).
    appended = children[n_before:]
    del children[n_before:]
    for j, nn in enumerate(appended):
        children.insert(idx + 1 + j, nn)
    if name:
        new_node.local_name = str(name)
    for nn in appended:
        _uniquify_in_scope(children, nn)
    return new_node


def add_loop(doc, after, count=4):
    """Editor mutation (L2, STRUCTURAL): insert an EMPTY (identity) loop group after
    `after`, carrying its primary output; after's former primary consumers rewire to
    the loop output. Grow the body by adding ops with the loop row selected."""
    if not isinstance(after, DocNode):
        raise TerrainDocParamError("add: select a document NODE to insert a loop after")
    children, owners = _scope_of(doc, after)
    idx = children.index(after)
    slots = _out_ref_slots(doc, after)
    carry = _Carry("h", _DocOutPlug(after, "Out"))
    used = {o.path for (_pk, _k, o) in tree_paths(doc)
            if isinstance(o, (DocLoop, DocGroupCall))}
    path = doc._next_group_path("loop")
    while path in used:
        path = doc._next_group_path("loop")
    loop = DocLoop(doc, int(count), path, {"h": carry})
    children.insert(idx + 1, loop)
    out_ref = _DocOutPlug(carry.out_placeholder, "Out")
    for set_ in slots:
        set_(out_ref)
    return loop


def add_into_loop(doc, loop, op_name, name=None):
    """Editor mutation (L2, STRUCTURAL): append an op to a loop BODY (first carry):
    its input is the current body tail (the carry itself when the body is empty),
    and the carry's body output becomes the new node — per-iteration semantics."""
    from . import ops as _ops
    if not isinstance(loop, DocLoop):
        raise TerrainDocParamError("add-into-loop: select a LOOP row")
    ent = _palette_entry_or_raise(op_name)
    if not loop.carries:
        raise TerrainDocParamError(f"loop {loop.path!r} has no carries")
    carry = next(iter(loop.carries.values()))
    tail = carry.body_out_ref or _DocOutPlug(carry.placeholder, "Out")
    _children, owners = _scope_of(doc, loop)
    op = getattr(_ops, op_name)
    is_source = ent["source"]

    def _make():
        if is_source:
            return op()          # source inside a body: a per-iteration field
        tn_in = _tnode_for_ref(tail)
        return _apply_recipe(op_name, op, ent["recipe"], tn_in)

    n_before = len(loop.children)
    tn = _mini_trace(doc, owners + [loop], _make)
    new_node = tn._output_plug.node
    if not is_source:
        carry.body_out_ref = tn._output_plug
    if name:
        new_node.local_name = str(name)
    for nn in loop.children[n_before:]:
        _uniquify_in_scope(loop.children, nn)
    return new_node


def to_json(doc):
    """Serialize a TerrainDoc to a json-ready dict."""
    ids = _IdAlloc()
    loop_ids = {}
    # pre-pass: allocate ids for loops (iter-expr index refs need them).
    def _alloc_loops(children):
        for ch in children:
            if isinstance(ch, DocLoop):
                loop_ids[id(ch)] = ids.get(ch)
                _alloc_loops(ch.children)
            elif isinstance(ch, DocGroupCall):
                _alloc_loops(ch.children)
    _alloc_loops(doc._root)

    def _ref(out_plug):
        return [ids.get(out_plug.node), out_plug.plug_name]

    def _enc_children(children):
        out = []
        for ch in children:
            if isinstance(ch, DocNode):
                out.append({
                    "kind": "node",
                    "id": ids.get(ch),
                    "name": ch.local_name,
                    "clazz": ch.clazz_name,
                    "params": [[k, n, _enc_value(v)] for (k, n, v) in ch.param_actions],
                    "iter_params": [[k, n, _enc_iter(e, loop_ids)]
                                    for (k, n), e in ch.iter_params.items()],
                    "param_exprs": [[k, n, _enc_param_expr(e)]
                                    for (k, n), e in ch.param_exprs.items()],
                    "conns": [[ip, _ref(op)] for (ip, op) in ch.connections],
                    **({"bypassed": True} if ch.bypassed else {}),
                })
            elif isinstance(ch, DocLoop):
                out.append({
                    "kind": "loop",
                    "id": ids.get(ch),
                    "count": ch.count,
                    "path": ch.path,
                    "carries": [{
                        "name": name,
                        "ph": ids.get(c.placeholder),
                        "out_ph": ids.get(c.out_placeholder),
                        "initial": _ref(c.initial_ref),
                        "body_out": _ref(c.body_out_ref) if c.body_out_ref else None,
                    } for name, c in ch.carries.items()],
                    "children": _enc_children(ch.children),
                    **({"bypassed": True} if ch.bypassed else {}),
                })
            elif isinstance(ch, DocGroupCall):
                out.append({
                    "kind": "group",
                    "path": ch.path,
                    "func": ch.func_name,
                    # only the terrain-node (ref) args are persisted — scalar args are pure
                    # call metadata (never consumed by elaborate); the ref args are what a
                    # bypass alias + delete-with-reconnect need to survive a doc-JSON round-trip.
                    "args": [[name, _ref(val)] for name, val in ch.args.items()
                             if isinstance(val, _DocOutPlug)],
                    "children": _enc_children(ch.children),
                    "output": _ref(ch.output_ref) if ch.output_ref else None,
                    **({"bypassed": True} if ch.bypassed else {}),
                })
            elif isinstance(ch, DocSwitch):
                out.append({
                    "kind": "switch",
                    "selector": ch.selector,
                    "branches": {b: _ref(r) for b, r in ch.branches.items()},
                    "selected": ch.selected,
                    "out_ph": ids.get(ch.out_placeholder),
                })
        return out

    root = _enc_children(doc._root)   # build first so every node id is allocated
    out = {
        "version": _DOC_JSON_VERSION,
        "root": root,
        "captures": [{"node": ids.get(c.node), "channels": c.channels}
                     for c in doc._captures],
        # doc-level select-as-output target (base.select_output): the marked node's id, or None.
        "select_output": (ids.get(doc._select_output)
                          if doc._select_output is not None else None),
        # E0 document parameters (additive block; absent on pre-E0 doc-JSON -> empty params).
        # A tagged param (E0 part 2) serializes its value as the typed-literal node
        # {"<unit>": v}; plain params keep the plain _enc_value form.
        "params": [[name, _enc_param_table_value(doc.params, name),
                    doc.params.is_structural(name)]
                   for name in doc.params.names()],
    }
    # Node canvas positions (E2; editor data owned by the document layer, keyed by
    # tree_paths() key). ADDITIVE and emitted ONLY when non-empty, so the authored corpus
    # (which never sets positions) serializes byte-identically to pre-E2 doc-JSON.
    if doc._node_positions:
        out["positions"] = {k: [p[0], p[1]] for k, p in doc._node_positions.items()}
    return out


def from_json(data):
    """Reconstruct a TerrainDoc from a to_json() dict."""
    from orkengine.lev2 import terrain as _terrain
    if data.get("version") != _DOC_JSON_VERSION:
        raise ValueError(f"unsupported terrain doc-JSON version {data.get('version')!r}")
    doc = TerrainDoc()
    # E0 params table FIRST (node param_exprs reference it). Additive: pre-E0 doc-JSON has no
    # "params" key -> the default empty table (loads with empty params, per adjudication 11).
    for (name, enc, structural) in data.get("params", []):
        value, tag = _dec_param_table_value(enc)
        doc.params.declare(name, value, tag=tag)
        if structural:
            doc.params.mark_structural(name)
    by_id = {}          # doc-id -> DocNode | _Placeholder
    loops_by_id = {}    # doc-id -> DocLoop
    deferred = []       # (obj, kind, payload) ref-wiring after all objects exist

    def _build(children_json):
        built = []
        for cj in children_json:
            kind = cj["kind"]
            if kind == "node":
                clazz = getattr(_terrain, cj["clazz"])
                node = DocNode(doc, cj["name"], clazz, _make_real=False)
                node.clazz_name = cj["clazz"]
                node.bypassed = bool(cj.get("bypassed", False))
                node.param_actions = [(k, n, _dec_value(v)) for (k, n, v) in cj["params"]]
                by_id[cj["id"]] = node
                deferred.append(("node", node, cj))
                built.append(node)
            elif kind == "loop":
                carries = {}
                loop = DocLoop(doc, cj["count"], cj["path"], carries)
                loop.bypassed = bool(cj.get("bypassed", False))
                loops_by_id[cj["id"]] = loop
                by_id[cj["id"]] = loop
                for carj in cj["carries"]:
                    c = _Carry(carj["name"], None)
                    by_id[carj["ph"]] = c.placeholder
                    by_id[carj["out_ph"]] = c.out_placeholder
                    carries[carj["name"]] = c
                    deferred.append(("carry", c, carj))
                loop.children = _build(cj["children"])
                built.append(loop)
            elif kind == "group":
                call = DocGroupCall(doc, cj["path"], cj["func"], {})
                call.bypassed = bool(cj.get("bypassed", False))
                call.children = _build(cj["children"])
                deferred.append(("group", call, cj))
                built.append(call)
            elif kind == "switch":
                sw = DocSwitch(cj["selector"], {}, cj["selected"])
                # re-register the SAME output placeholder under its serialized id so
                # downstream connection refs resolve to it.
                by_id[cj["out_ph"]] = sw.out_placeholder
                deferred.append(("switch", sw, cj))
                built.append(sw)
        return built

    doc._root = _build(data["root"])
    doc._scope_stack = [doc._root]

    def _ref(pair):
        return _DocOutPlug(by_id[pair[0]], pair[1]) if pair else None

    for (what, obj, cj) in deferred:
        if what == "node":
            obj.iter_params = {(k, n): _dec_iter(e, loops_by_id)
                               for (k, n, e) in cj["iter_params"]}
            obj.param_exprs = {(k, n): _dec_param_expr(e, doc.params)
                               for (k, n, e) in cj.get("param_exprs", [])}
            obj.connections = [(ip, _ref(op)) for (ip, op) in cj["conns"]]
        elif what == "carry":
            obj.initial_ref = _ref(cj["initial"])
            obj.body_out_ref = _ref(cj["body_out"])
        elif what == "group":
            obj.output_ref = _ref(cj["output"])
            obj.args = {name: _ref(pair) for (name, pair) in cj.get("args", [])}
        elif what == "switch":
            obj.branches = {b: _ref(r) for b, r in cj["branches"].items()}

    for capj in data["captures"]:
        doc._captures.append(DocCapture(by_id[capj["node"]], capj["channels"]))
    so = data.get("select_output")
    doc._select_output = by_id.get(so) if so is not None else None
    # Node canvas positions (E2; additive — absent on pre-E2 doc-JSON -> empty layout).
    doc._node_positions = {k: (float(v[0]), float(v[1]))
                           for k, v in data.get("positions", {}).items()}
    return doc
