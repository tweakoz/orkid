###############################################################################
# ork.hypergraph.dflow.terrain.pywriter — the DOCUMENT -> runnable .py DSL writer
# (JUL09 S3 codegen half). The editor's Save emits a HeightField .py asset (the
# ork.scene.viewer.py / hyperecs consume form); doc-JSON stays INTERNAL ONLY (undo
# checkpoints). This is the inverse of the trace: doc.py records DSL ops INTO a
# TerrainDoc, and to_python() regenerates the DSL that would record the same tree.
#
# Emission is per-MODULE-CLASS (not per-wrapper): the document is one DocNode per
# dflow module, LOWER than the ops.py wrappers (e.g. `h*0.5+0.5` is three Remap
# nodes), so we map each module class back to the wrapper call that constructs it.
# The graph is walked in DOCUMENT ORDER (producers precede consumers), binding one
# local variable per node; refs resolve to that variable (+ .discharge/.filled/...
# for the multi-output structs). Explicit constructs round-trip: DocLoop -> a real
# `with T.loop(...) as L:` (L.i params re-emitted as the source expression), captures
# -> self.capture(). Authored ptex3d expression nodes (ExprModule: hfbake /
# hfdisplacement / expr_field / T.pow) CANNOT be regenerated from the document — the
# writer fails LOUDLY naming the node (ops-self-defend; the editor cannot create them
# either). A8: every user-tweakable stays a wrapper kwarg / plug, never inlined.
###############################################################################

import keyword
import re

from orkengine.core import vec2 as _vec2
from ... import units as _units
from .doc import (
    DocNode, DocLoop, DocGroupCall, DocSwitch, _Placeholder,
    TerrainDocParamError, _collect_captured_param_names, _iter_ref_slots)
# E0 part 3 (operator re-sugaring): the raise pass is the verified INVERSE of the _node.py
# operator lowering, so it reads the SAME op codes + no-clamp sentinels the lowering emits
# (single source of truth — never re-declared here).
from ._node import (
    _NO_CLAMP_LO, _NO_CLAMP_HI, OP_ADD, OP_SUB, OP_MUL, OP_MIN, OP_MAX, OP_MIX)


# --- multi-output modules: out-plug name -> namedtuple attr the wrapper returns ---
# (ops.py: flow3d -> Flow3DResult, fill_closed_basins -> FillBasinsResult,
#  relax_uv -> RelaxUvResult). A ref to a non-primary plug renders as var.<attr>.
_MULTI_OUT = {
    "Flow3DModule":          {"Out": "dir", "Discharge": "discharge", "Metrics": "metrics"},
    "FillClosedBasinsModule": {"Out": "filled", "Basin": "basin", "CenterPit": "center_pit"},
    "RelaxUvModule":         {"Out": "uv", "Binormal": "binormal"},
}

# NoiseModule.basis code -> the named noise wrapper (ops._NOISE_BASIS inverse).
_NOISE_FN = {0: "perlin", 1: "simplex", 2: "worleyf1", 3: "voronoi"}
# CurvatureModule._mode code -> the wrapper's string (ops._CURV_MODE inverse).
_CURV_MODE_NAME = {0: "convex", 1: "concave", 2: "magnitude"}

# generic emitters: module class -> (wrapper fn under T, ordered positional input plugs).
# every recorded param is emitted as a name=value kwarg (the wrapper kwarg name == the
# plug / baked-scalar name; the wrapper sorts kind internally) EXCEPT the connection
# plugs (positional) and the per-class special selectors handled below. Explicit is
# safer than default-elision (spec) — reproduce every recorded value.
_GENERIC = {
    "ConstModule":            ("const",              ()),
    "RemapModule":            ("remap",              ("In",)),
    "NormalizeModule":        ("normalize",          ("In",)),
    "TerraceModule":          ("terrace",            ("In",)),
    "SlopeModule":            ("slope",              ("In",)),
    "LpfModule":              ("lpf",                ("In",)),
    "BasinFillModule":        ("basin_fill",         ("In",)),
    "ThermalErodeModule":     ("erode_thermal",      ("In",)),
    "EroxModule":             ("erox",               ("In",)),
    "PhaModule":              ("pha",                ("In",)),
    "FlowErodeModule":        ("flow_erode",         ("In", "Discharge")),
    "Flow3DModule":           ("flow3d",             ("In",)),
    "RelaxUvModule":          ("relax_uv",           ("In",)),
    "FillClosedBasinsModule": ("fill_closed_basins", ("In",)),
}

# E0 part 3 operator sugar — expression precedence (higher binds tighter). An operand text
# carries its precedence so the raise pass parenthesizes MINIMALLY yet re-lowers to the exact
# same module tree (the raise is notation-only): + / - are _P_ADD, * is _P_MUL, everything
# atomic (a variable, a literal, a T.Min/Max/mix call, a parenthesized param expr) is _P_ATOM.
_P_ADD, _P_MUL, _P_ATOM = 1, 2, 3

# auto-name patterns from _node.py's anon_name(): a remap/combine may raise to operator sugar
# ONLY when it is auto-named (the author gave no name=), so a deliberately named node keeps its
# explicit T.remap(...) form. (const_N / mask_N are not raised, so are not listed.)
_ANON_NAME_RE = {
    "RemapModule":   re.compile(r"^remap_\d+$"),
    "CombineModule": re.compile(r"^comb_\d+$"),
}

# variable identifiers the writer reserves (never assigned to a node var).
_RESERVED = {"self", "super", "T", "HeightField", "vec2", "L", "L1", "L2", "L3", "L4", "L5"}


def _sanitize_ident(base):
    """A valid, non-keyword python identifier from a document local_name."""
    s = "".join(c if (c.isalnum() or c == "_") else "_" for c in str(base))
    if not s or not (s[0].isalpha() or s[0] == "_"):
        s = "n_" + s
    if keyword.iskeyword(s):
        s = s + "_"
    return s


def _class_name_from_stem(stem):
    """A CamelCase class identifier from a filename stem (my_terrain -> MyTerrain)."""
    parts = [p for p in "".join(c if c.isalnum() else " " for c in str(stem)).split() if p]
    name = "".join(p[:1].upper() + p[1:] for p in parts) or "Terrain"
    if not (name[0].isalpha() or name[0] == "_"):
        name = "T" + name
    if keyword.iskeyword(name):
        name = name + "_"
    return name


class _Writer:
    def __init__(self, doc, class_name, extent_m, source_note):
        self.doc = doc
        self.class_name = class_name
        self.extent_m = float(extent_m)
        self.source_note = source_note
        self._lines = []               # (level, text)
        self._var_of = {}              # id(DocNode) -> variable name
        # E0 part 3: id(DocNode) -> (text, prec) for an operator node RAISED to inline sugar
        # (folded into its single consumer, so it emits NO standalone line).
        self._inline = {}
        # consumer counts drive the inline-vs-named-variable choice (a shared intermediate must
        # keep a variable); the doc-level display target must keep a variable too (select_output
        # references it by var), so it is excluded from sugar even when otherwise eligible.
        self._consumers = self._count_consumers(doc)
        self._display_id = (id(doc._select_output)
                            if getattr(doc, "_select_output", None) is not None else None)
        self._ph_expr = {}             # id(_Placeholder) -> reference expression
        self._handle_of = {}           # id(DocLoop) -> loop handle var ("L", "L1", ...)
        self._used = set(_RESERVED) | {class_name}
        self._need_vec2 = False
        self._units_used = set()       # E0 part 2: unit constructors referenced in the signature
        self._redundant_suffixes = []  # [(name, tag)] a suffixed name now redundant with its tag
        # E0 captured document parameters: emitted as ctor kwargs (current value = default) and
        # referenced by name in the body (a plug/prop expr renders as the param / arithmetic).
        params = getattr(doc, "params", None)
        self._captured_params = (_collect_captured_param_names(doc)
                                 if params is not None and len(params) else set())
        self._used |= self._captured_params   # keep node vars from colliding with param names

    # ---- line + name helpers -------------------------------------------------

    def _line(self, level, text):
        self._lines.append((level, text))

    def _fresh_var(self, base):
        ident = _sanitize_ident(base)
        if ident not in self._used:
            self._used.add(ident)
            return ident
        n = 2
        while f"{ident}_{n}" in self._used:
            n += 1
        v = f"{ident}_{n}"
        self._used.add(v)
        return v

    # ---- value + iter-expr rendering -----------------------------------------

    def _render_value(self, v):
        if isinstance(v, bool):
            return "True" if v else "False"
        if isinstance(v, int):
            return str(v)
        if isinstance(v, float):
            return repr(v)              # round-trips exactly (eval(repr(x)) == x)
        if isinstance(v, str):
            return repr(v)
        if isinstance(v, _vec2):
            self._need_vec2 = True
            return "vec2(%s, %s)" % (repr(float(v.x)), repr(float(v.y)))
        raise TerrainDocParamError(
            f"cannot regenerate .py: unsupported param value type "
            f"{type(v).__name__!r} ({v!r})")

    def _iter_expr_py(self, expr):
        """The L.i iteration expression as source (mirrors doc.iter_expr_string but
        emits the OWNING loop's handle, e.g. '(2 + (L.i / 32))')."""
        op = expr._op
        if op == "index":
            handle = self._handle_of.get(id(expr._a))
            if handle is None:
                raise TerrainDocParamError(
                    "cannot regenerate .py: an L.i expression references a loop that is "
                    "not in scope (nested-loop index leak)")
            return f"{handle}.i"
        if op == "const":
            c = float(expr._a)
            return str(int(c)) if c.is_integer() else repr(c)
        if op == "neg":
            return "-" + self._iter_expr_py(expr._a)
        sym = {"add": "+", "sub": "-", "mul": "*", "div": "/"}[op]
        return "(%s %s %s)" % (self._iter_expr_py(expr._a), sym, self._iter_expr_py(expr._b))

    def _param_expr_py(self, expr):
        """A captured document-parameter expression as source (E0): a param leaf emits the
        ctor-kwarg local variable; arithmetic emits inline source (the ExprIR text form)."""
        op = expr._op
        if op == "param":
            return str(expr._a)
        if op == "const":
            c = float(expr._a)
            return str(int(c)) if c.is_integer() else repr(c)
        if op == "neg":
            return "-" + self._param_expr_py(expr._a)
        sym = {"add": "+", "sub": "-", "mul": "*", "div": "/"}[op]
        return "(%s %s %s)" % (self._param_expr_py(expr._a), sym, self._param_expr_py(expr._b))

    def _kwarg_src(self, node, kind, name, value):
        """Source for one op kwarg: a captured document-parameter expr (E0) renders as the
        param reference / arithmetic; otherwise the recorded literal."""
        expr = node.param_exprs.get((kind, name))
        if expr is not None:
            return self._param_expr_py(expr)
        return self._render_value(value)

    # ---- reference resolution ------------------------------------------------

    @staticmethod
    def _count_consumers(doc):
        """id(DocNode) -> number of references to its output across the whole document (node
        inputs, loop carries, group args, switch branches). One consumer = an operator node may
        fold into it; more than one = it must keep a named variable (correct reuse)."""
        counts = {}
        for (get, _set) in _iter_ref_slots(doc):
            ref = get()
            if ref is not None and isinstance(ref.node, DocNode):
                counts[id(ref.node)] = counts.get(id(ref.node), 0) + 1
        return counts

    def _ref_expr(self, out_plug):
        """(text, prec) for a reference. An operator node RAISED to inline sugar yields its
        expression text + precedence (so operator contexts parenthesize correctly); everything
        else is an _P_ATOM (a variable, a placeholder handle, or a multi-output var.attr)."""
        if out_plug is None:
            raise TerrainDocParamError("cannot regenerate .py: a required reference is unset")
        node = out_plug.node
        if isinstance(node, _Placeholder):
            expr = self._ph_expr.get(id(node))
            if expr is None:
                raise TerrainDocParamError(
                    f"cannot regenerate .py: unresolved placeholder {node.name!r}")
            return (expr, _P_ATOM)
        if isinstance(node, DocNode):
            inlined = self._inline.get(id(node))
            if inlined is not None:
                return inlined
            # a bypassed node still emits its OWN var + a following T.bypass(var); consumers
            # reference that var (the C++ resolveConnectedOutput does the pass-through hop at
            # elaborate) — the writer no longer flattens the bypass into the input's ref.
            var = self._var_of.get(id(node))
            if var is None:
                raise TerrainDocParamError(
                    f"cannot regenerate .py: reference to node {node.local_name!r} before "
                    f"it is defined (document not in producer-first order)")
            multi = _MULTI_OUT.get(node.clazz_name)
            if multi is not None:
                attr = multi.get(out_plug.plug_name)
                if attr is None:
                    raise TerrainDocParamError(
                        f"cannot regenerate .py: node {node.local_name!r} "
                        f"[{node.clazz_name}] has no output plug {out_plug.plug_name!r}")
                return (f"{var}.{attr}", _P_ATOM)
            return (var, _P_ATOM)
        raise TerrainDocParamError(
            f"cannot regenerate .py: reference to unknown source {type(node).__name__}")

    def _render_ref(self, out_plug):
        """The raw reference text (an inline sugar expression stands alone in a call-argument /
        assignment position — no wrapping parens needed there). Operator contexts use _ref_expr
        directly to parenthesize by precedence."""
        return self._ref_expr(out_plug)[0]

    @staticmethod
    def _lpar(text, prec, opprec):
        """Parenthesize a LEFT operand of a left-associative op — only when it binds looser."""
        return f"({text})" if prec < opprec else text

    @staticmethod
    def _rpar(text, prec, opprec):
        """Parenthesize a RIGHT operand of a left-associative op — when it binds looser OR
        equal (so a - (b - c) / a * (b * c) keep their exact subtree, not the left-assoc regroup)."""
        return f"({text})" if prec <= opprec else text

    # ---- effective params ----------------------------------------------------

    def _effective_params(self, node):
        """Ordered [(kind, name)] + {(kind,name): value}, last-write-wins, first-
        occurrence order, EXCLUDING iteration (L.i) params (emitted as expressions)."""
        seen = {}
        order = []
        for (kind, name, value) in node.param_actions:
            key = (kind, name)
            if key in node.iter_params:
                continue
            if key not in seen:
                order.append(key)
            seen[key] = value
        return order, seen

    # ---- node emission -------------------------------------------------------

    def _emit_node(self, node, level):
        clazz = node.clazz_name
        if clazz == "CaptureModule":
            self._emit_capture(node, level)
            return
        if clazz == "ExprModule":
            self._emit_expr_blob(node, level)
            return
        conn = {ip: op for (ip, op) in node.connections}
        # E0 part 3: RAISE an anonymous, unflagged, pattern-matching remap/combine to operator
        # sugar (notation-only inverse of the _node.py lowering). A single-consumer sugar node
        # FOLDS into its consumer (no line); a shared one keeps a named variable.
        text, prec, eligible = self._node_expr(node, conn)
        if eligible and self._consumers.get(id(node), 0) == 1:
            self._inline[id(node)] = (text, prec)
            return
        var = self._fresh_var(node.local_name)
        self._var_of[id(node)] = var
        self._line(level, f"{var} = {text}")
        if node.bypassed:
            self._line(level, f"T.bypass({var})")

    # ---- E0 part 3: operator re-sugaring (the raise pass) --------------------

    def _sugar_eligible(self, node):
        """A remap/combine may raise to operator sugar ONLY when it is auto-named (no user
        name=), carries no flags (unbypassed), and is not the doc-level display target
        (select_output keeps it a named variable). Anything else stays explicit."""
        rx = _ANON_NAME_RE.get(node.clazz_name)
        if rx is None or not rx.match(node.local_name):
            return False
        if node.bypassed:
            return False
        return id(node) != self._display_id

    def _node_expr(self, node, conn):
        """(text, prec, eligible): the value expression for a body node. `eligible` marks a
        sugar-eligible operator (raise pass) — it inlines when single-consumer, else becomes a
        named variable carrying the same sugar. Non-operators return their explicit RHS."""
        clazz = node.clazz_name
        if clazz == "CombineModule":
            text, prec, raisable = self._combine_expr(node, conn)
            return (text, prec, raisable and self._sugar_eligible(node))
        if clazz == "RemapModule" and self._sugar_eligible(node):
            sug = self._remap_sugar(node, conn)
            if sug is not None:
                return (sug[0], sug[1], True)
        return (self._render_rhs(node, conn), _P_ATOM, False)

    def _remap_sugar(self, node, conn):
        """(text, prec) if this RemapModule matches EXACTLY one affine lowering pattern —
        scale-only (`in * s`, bias fixed 0) or bias-only (`in + b`, scale fixed 1), both with
        the no-clamp sentinels — else None (a real clamp, a fused scale+bias, or the identity
        remap all stay explicit T.remap(...)). A scale/bias driven by a captured document
        parameter (E0) raises to arithmetic referencing that parameter (`in * amplitude`)."""
        _order, seen = self._effective_params(node)
        need = (("inputs", "scale"), ("inputs", "bias"), ("inputs", "lo"), ("inputs", "hi"))
        if any(k not in seen for k in need):
            return None
        pe = node.param_exprs
        # no-clamp sentinels must be the exact LITERALS the lowering bakes (a param-driven or
        # real bound means the author wanted a clamp -> keep explicit).
        if (("inputs", "lo") in pe or ("inputs", "hi") in pe
                or seen[("inputs", "lo")] != _NO_CLAMP_LO
                or seen[("inputs", "hi")] != _NO_CLAMP_HI):
            return None
        scale_param = ("inputs", "scale") in pe
        bias_param = ("inputs", "bias") in pe
        # the non-payload operand must be the lowering's fixed literal (bias 0 for a scale remap,
        # scale 1 for a bias remap). EXACTLY one pattern must hold (identity matches both).
        is_scale = (not bias_param) and seen[("inputs", "bias")] == 0.0
        is_bias = (not scale_param) and seen[("inputs", "scale")] == 1.0
        if is_scale == is_bias:
            return None
        if "In" not in conn:
            return None
        in_t, in_p = self._ref_expr(conn["In"])
        if is_scale:
            operand = (self._param_expr_py(pe[("inputs", "scale")]) if scale_param
                       else self._render_value(seen[("inputs", "scale")]))
            return (f"{self._lpar(in_t, in_p, _P_MUL)} * {operand}", _P_MUL)
        # bias-only: `in + b`, or `in - |b|` for a negative literal (both re-lower to bias=b).
        if bias_param:
            return (f"{in_t} + {self._param_expr_py(pe[('inputs', 'bias')])}", _P_ADD)
        b = seen[("inputs", "bias")]
        if b < 0.0:
            return (f"{in_t} - {self._render_value(-b)}", _P_ADD)
        return (f"{in_t} + {self._render_value(b)}", _P_ADD)

    def _combine_expr(self, node, conn):
        """(text, prec, raisable) for a CombineModule. add/mul -> infix `+`/`*`, min/max ->
        T.Min/T.Max (raisable=True — the raise pass may inline these); sub -> infix `-` and mix
        -> T.mix stay raisable=False (emitted, never folded into a chain). Operands parenthesize
        by precedence so the text re-lowers to the identical two-input Combine subtree."""
        _order, seen = self._effective_params(node)
        op = int(seen[("module", "op")])
        at, ap = self._ref_expr(conn["A"])
        bt, bp = self._ref_expr(conn["B"])
        if op == OP_ADD:
            return (f"{self._lpar(at, ap, _P_ADD)} + {self._rpar(bt, bp, _P_ADD)}", _P_ADD, True)
        if op == OP_MUL:
            return (f"{self._lpar(at, ap, _P_MUL)} * {self._rpar(bt, bp, _P_MUL)}", _P_MUL, True)
        if op == OP_MIN:
            return (f"T.Min({at}, {bt})", _P_ATOM, True)
        if op == OP_MAX:
            return (f"T.Max({at}, {bt})", _P_ATOM, True)
        if op == OP_SUB:
            return (f"{self._lpar(at, ap, _P_ADD)} - {self._rpar(bt, bp, _P_ADD)}", _P_ADD, False)
        if op == OP_MIX:
            # a uniform-scalar mix (a per-texel field mix is a MaskBlendModule, not Combine).
            tval = seen.get(("inputs", "t"), 0.5)
            return (f"T.mix({at}, {bt}, t={self._render_value(0.5 if tval is None else tval)})",
                    _P_ATOM, False)
        raise TerrainDocParamError(
            f"cannot regenerate .py: CombineModule {node.local_name!r} has unknown op {op!r}")

    def _emit_expr_blob(self, node, level):
        """Expression node (ExprModule). An editor-authored T.expr carries its SOURCE STRING
        (expr_source) — emit `T.expr(<source>, inputs=[...])`, re-authorable + editable. A
        legacy authored node (hfbake / hfdisplacement / expr_field / T.pow) has no source (the
        Python function was never document state), so emit the COMPILED shadertext verbatim
        through the raw escape hatch (runnable + byte-faithful; edit the original authored
        source to change it)."""
        conn = {ip: op for (ip, op) in node.connections}
        _, seen = self._effective_params(node)
        source = seen.get(("module", "expr_source"))
        ins = []
        k = 0
        while f"In{k}" in conn:
            ins.append(self._render_ref(conn[f"In{k}"]))
            k += 1
        var = self._fresh_var(node.local_name)
        self._var_of[id(node)] = var
        if source:
            self._line(level, f"{var} = T.expr({source!r}, inputs=[{', '.join(ins)}])")
        else:
            text = seen.get(("module", "shadertext"))
            if not text:
                raise TerrainDocParamError(
                    f"cannot regenerate .py: expression node {node.local_name!r} has no "
                    f"expr_source or shader text recorded (corrupt document?)")
            self._line(level, f"# {var}: COMPILED expression blob (authored fn not in the document)")
            self._line(level, f"{var} = T.expr_field_raw(")
            self._line(level, f"    inputs=[{', '.join(ins)}],")
            self._line(level, f"    shadertext={text!r})")
        if node.bypassed:
            self._line(level, f"T.bypass({var})")

    def _emit_capture(self, node, level):
        conn = {ip: op for (ip, op) in node.connections}
        if "In" not in conn:
            raise TerrainDocParamError(
                f"cannot regenerate .py: capture {node.local_name!r} has no input")
        in_ref = self._render_ref(conn["In"])
        _, seen = self._effective_params(node)
        chan_str = str(seen.get(("module", "channel"), "") or "")
        cache = bool(seen.get(("module", "cache"), False))
        chans = [c for c in chan_str.split(",") if c]
        if not chans:
            raise TerrainDocParamError(
                f"cannot regenerate .py: capture {node.local_name!r} has no channel")
        if len(chans) == 1:
            chan_lit = repr(chans[0])
        else:
            chan_lit = "[" + ", ".join(repr(c) for c in chans) + "]"
        self._line(level, f"self.capture({in_ref}, {chan_lit}, cache={self._render_value(cache)})")

    # ---- per-class RHS renderers ---------------------------------------------

    def _render_rhs(self, node, conn):
        # CombineModule is handled ahead of here in _node_expr (operator sugar carries precedence);
        # this path renders the explicit-wrapper classes.
        clazz = node.clazz_name
        if clazz == "MaskBlendModule":
            return "T.mix(%s, %s, %s)" % (self._render_ref(conn["A"]),
                                          self._render_ref(conn["B"]),
                                          self._render_ref(conn["M"]))
        if clazz == "FbmModule":
            return self._render_fbm(node, conn)
        if clazz == "NoiseModule":
            return self._render_noise(node, conn)
        if clazz == "GradientModule":
            return self._render_gradient(node)
        if clazz == "CurvatureModule":
            return self._render_curvature(node, conn)
        spec = _GENERIC.get(clazz)
        if spec is None:
            raise TerrainDocParamError(
                f"cannot regenerate .py: no emitter for module class {clazz!r} "
                f"(node {node.local_name!r})")
        fn, inputs = spec
        return self._render_generic(node, conn, fn, inputs, skip=())

    def _value_kwargs(self, node, skip=()):
        """[ 'name=value', ... ] for every effective param (skipping `skip`) + the L.i
        iteration params rendered as source expressions."""
        order, seen = self._effective_params(node)
        out = []
        for (kind, name) in order:
            if name in skip:
                continue
            out.append(f"{name}={self._kwarg_src(node, kind, name, seen[(kind, name)])}")
        for (kind, name), expr in node.iter_params.items():
            if name in skip:
                continue
            out.append(f"{name}={self._iter_expr_py(expr)}")
        return out

    def _render_generic(self, node, conn, fn, inputs, skip):
        pos = []
        for p in inputs:
            if p not in conn:
                raise TerrainDocParamError(
                    f"cannot regenerate .py: node {node.local_name!r} [{node.clazz_name}] "
                    f"missing input connection {p!r}")
            pos.append(self._render_ref(conn[p]))
        args = pos + self._value_kwargs(node, skip=skip)
        return "T.%s(%s)" % (fn, ", ".join(args))

    def _render_fbm(self, node, conn):
        # generator (no positional input); offset/offset_vel are vec2 kwargs the wrapper
        # accepts directly; a wired warp is warp=(wx, wy).
        args = self._value_kwargs(node)
        if "warp_x" in conn and "warp_y" in conn:
            args.append("warp=(%s, %s)" % (self._render_ref(conn["warp_x"]),
                                           self._render_ref(conn["warp_y"])))
        return "T.fbm(%s)" % ", ".join(args)

    def _render_noise(self, node, conn):
        order, seen = self._effective_params(node)
        basis = None
        for (kind, name) in order:
            if name == "basis":
                basis = int(seen[(kind, name)])
        fn = _NOISE_FN.get(basis)
        if fn is None:
            raise TerrainDocParamError(
                f"cannot regenerate .py: NoiseModule {node.local_name!r} has unknown "
                f"basis {basis!r}")
        args = self._value_kwargs(node, skip=("basis",))
        if "warp_x" in conn and "warp_y" in conn:
            args.append("warp=(%s, %s)" % (self._render_ref(conn["warp_x"]),
                                           self._render_ref(conn["warp_y"])))
        return "T.%s(%s)" % (fn, ", ".join(args))

    def _render_gradient(self, node):
        order, seen = self._effective_params(node)
        args = []
        for (kind, name) in order:
            v = seen[(kind, name)]
            if name == "dir" and isinstance(v, _vec2):
                args.append("dir_x=%s" % repr(float(v.x)))
                args.append("dir_y=%s" % repr(float(v.y)))
            else:
                args.append(f"{name}={self._kwarg_src(node, kind, name, v)}")
        return "T.gradient(%s)" % ", ".join(args)

    def _render_curvature(self, node, conn):
        order, seen = self._effective_params(node)
        args = [self._render_ref(conn["In"])]
        for (kind, name) in order:
            v = seen[(kind, name)]
            if name == "mode":
                mode = _CURV_MODE_NAME.get(int(v))
                if mode is None:
                    raise TerrainDocParamError(
                        f"cannot regenerate .py: curvature {node.local_name!r} has unknown "
                        f"mode {v!r}")
                args.append("mode=%s" % repr(mode))
            else:
                args.append(f"{name}={self._kwarg_src(node, kind, name, v)}")
        return "T.curvature(%s)" % ", ".join(args)

    # ---- containers ----------------------------------------------------------

    def _emit_children(self, children, level, depth):
        for child in children:
            if isinstance(child, DocNode):
                self._emit_node(child, level)
            elif isinstance(child, DocLoop):
                self._emit_loop(child, level, depth)
            elif isinstance(child, (DocGroupCall, DocSwitch)):
                what = "@T.group" if isinstance(child, DocGroupCall) else "T.switch"
                raise TerrainDocParamError(
                    f"cannot regenerate .py: the document contains a {what} construct, "
                    f"which the .py writer does not yet emit (the terrain editor cannot "
                    f"create these; keep the original .py source).")
            else:
                raise TerrainDocParamError(
                    f"cannot regenerate .py: unknown document child "
                    f"{type(child).__name__}")

    def _emit_loop(self, loop, level, depth):
        handle = "L" if depth == 0 else f"L{depth}"
        self._handle_of[id(loop)] = handle
        inits = [(name, self._render_ref(c.initial_ref)) for name, c in loop.carries.items()]
        parts = [str(loop.count)] + [f"{name}={ref}" for name, ref in inits]
        if loop.bypassed:
            # a bypassed loop authors the flag on the call — the body still traces (recorded)
            # but elaborate skips the unroll and aliases each carry's output to its initial.
            parts.append("bypassed=True")
        head = ", ".join(parts)
        self._line(level, f"with T.loop({head}) as {handle}:")
        for name, c in loop.carries.items():
            self._ph_expr[id(c.placeholder)] = f"{handle}.{name}"
        if loop.children:
            self._emit_children(loop.children, level + 1, depth + 1)
        else:
            self._line(level + 1, "pass")
        for name, c in loop.carries.items():
            if c.body_out_ref is not None:
                ref = self._render_ref(c.body_out_ref)
                if ref != f"{handle}.{name}":     # skip an identity (empty-body) carry
                    self._line(level + 1, f"{handle}.{name} = {ref}")
        # post-loop rebind: pin each carry OUT to a concrete var so a reused handle
        # (a later sibling loop) never aliases the wrong iteration.
        for name, c in loop.carries.items():
            postvar = self._fresh_var(name)
            self._line(level, f"{postvar} = {handle}.{name}")
            self._ph_expr[id(c.out_placeholder)] = postvar

    # ---- assembly ------------------------------------------------------------

    def build(self):
        # body first (populates _need_vec2 + node vars), then the header.
        self._emit_children(self.doc._root, 2, 0)
        self._emit_select_output(2)
        return self._assemble()

    def _signature(self):
        """`def __init__(self, <captured params>=<current value>, ...)` — E0 emits the
        captured document parameters (referenced by name in the body) as ctor kwargs so the
        class round-trips as a parameterized HeightField. A TAGGED param (E0 part 2) emits its
        typed-literal default `name=meters(2000)` (recording the unit constructor for the
        import line); a plain param emits `name=<value>` as before. STRUCTURAL params fold into
        the body as literals (they need a re-trace to change and are not editor-round-trippable
        as live parameters — like the group/switch/expr constructs the writer already flattens
        or refuses); a plain (no-params) document emits `def __init__(self):` exactly as before.
        No automatic renames: a name whose unit suffix is now redundant with its tag is noted
        in an advisory comment (the author renames per-asset, this slice never does)."""
        params = getattr(self.doc, "params", None)
        if not self._captured_params or params is None:
            return "self"
        parts = ["self"]
        for name in params.names():
            if name not in self._captured_params:
                continue
            tag = params.tag_of(name)
            if tag is not None:
                self._units_used.add(tag)
                parts.append(f"{name}={_units.unit_source(tag, params.get(name))}")
                suffix = next((s for s in _units.SUFFIX_UNITS
                               if name.endswith(s) and _units.SUFFIX_UNITS[s] == tag), None)
                if suffix is not None:
                    self._redundant_suffixes.append((name, tag))
            else:
                parts.append(f"{name}={self._render_value(params.get(name))}")
        return ", ".join(parts)

    def _emit_select_output(self, level):
        """Emit `self.select_output(<var>)` for the document's doc-level select-as-output
        target (base.select_output), after every node + capture is defined. None = nothing."""
        tgt = getattr(self.doc, "_select_output", None)
        if tgt is None:
            return
        var = self._var_of.get(id(tgt))
        if var is None:
            raise TerrainDocParamError(
                "cannot regenerate .py: the select_output target node is not in the document")
        self._line(level, f"self.select_output({var})")

    def _assemble(self):
        # signature FIRST — it records the unit constructors used + any redundant suffixes,
        # both of which the header below emits (import line / advisory comment).
        signature = self._signature()
        H = []
        H.append("#" * 79)
        H.append(f"# {self.class_name} — terrain HeightField, generated by the terrain editor")
        H.append("# DSL writer (JUL09 S3). Heights are TRUE METERS; the field spans EXTENT_M")
        H.append("# meters across XZ (centered at the origin). Re-authorable: it reads like a")
        H.append("# hand-written HeightField and round-trips (edit + re-save).")
        if self.source_note:
            H.append(f"# source: {self.source_note}")
        H.append("#" * 79)
        H.append("from ork.hypergraph.dflow.terrain import HeightField")
        H.append("from ork.hypergraph.dflow import terrain as T")
        if self._units_used:
            names = ", ".join(sorted(self._units_used))
            H.append(f"from ork.hypergraph.units import {names}")
        if self._need_vec2:
            H.append("from orkengine.core import vec2")
        H.append("")
        H.append("")
        H.append(f"class {self.class_name}(HeightField):")
        H.append(f"    EXTENT_M = {repr(self.extent_m)}")
        H.append("")
        for (name, tag) in self._redundant_suffixes:
            H.append(f"    # note: {name!r} carries a unit suffix now redundant with its "
                     f"{tag} tag — rename to drop it if desired (author's choice).")
        H.append(f"    def __init__({signature}):")
        H.append("        super().__init__()")

        body = "\n".join("    " * lvl + text for (lvl, text) in self._lines)
        out = "\n".join(H)
        if body:
            out += "\n" + body
        out += "\n"
        return out


def to_python(doc, *, class_name, extent_m, source_note=""):
    """Regenerate a runnable HeightField .py module from a TerrainDoc (S3 codegen).

    class_name : the emitted HeightField subclass name (a valid identifier).
    extent_m   : EXTENT_M for the class (heights are TRUE METERS; extent is XZ).
    source_note: optional provenance line for the header (e.g. the session label).

    Raises TerrainDocParamError (LOUD, named) when the document holds something the
    writer cannot reconstruct: an authored ExprModule (hfbake / hfdisplacement /
    expr_field / T.pow) or an @T.group / T.switch construct."""
    if not class_name or not (class_name[0].isalpha() or class_name[0] == "_"):
        raise TerrainDocParamError(f"to_python: invalid class_name {class_name!r}")
    return _Writer(doc, class_name, extent_m, source_note).build()


def class_name_from_stem(stem):
    """Public helper: derive a CamelCase HeightField class name from a filename stem."""
    return _class_name_from_stem(stem)
