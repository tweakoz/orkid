###############################################################################
# ork.dflow._bindings — bind() registration + graph emission.
#
# DslNode.bind(plug_name, expr) records a Binding triple on the active trace.
# ParticleSystem.generatedflow() lowers each binding and applies it to the
# graphdata: lazy-adds a Globals (or ParametersModule, future) source module,
# appends the chain stages with explicit "NNN_kind" keys (defeats orklut's
# sorted-key iteration), and emits the source→target connection.
#
# Multi-input or unsupported expressions raise NotImplementedError pointing to
# task #27 (FloatExprModule) for now. ParamRef raises pointing to task #26.
###############################################################################

from orkengine.core import dataflow as _dflow
from orkengine.lev2 import particles as _particles

from ._context_classes import python_class_for
from ._expr import (
    Vec3Expr, MinMaxExpr, LerpExpr, Const, ContextRef, EntityRef,
    EntityTransform, ParamRef, PromotedSource,
    BinOp, UnaryFn, PowExpr, ClampExpr, FmodExpr, SmoothstepExpr,
    QuantizeExpr, CurveExpr,
)
from ._lower import lower_to_chain


# Per-particle context vars — namespaced under "ptc.*". Anything else
# (time, dt, named params, etc.) is uniform-rate. The lowerer + emitter
# machinery handles per-particle eval for these (Pool.UnitAge, Pool.Random,
# etc.); on a UNIFORM input plug they'd silently collapse to whichever
# value the source backing-store happens to hold → garbage.
def _expression_is_varying(expr):
    """Walk the Expr tree; True if any leaf is a per-particle ContextRef
    (or a PromotedSource pointing at a varying output plug)."""
    if isinstance(expr, ContextRef):
        return expr.dsl_name.startswith("ptc.")
    if isinstance(expr, PromotedSource):
        # Underlying plug knows its own rate. PromotedSource holds an
        # output plug from a typed module (Min/Max/Lerp/Vec4Combine/etc.);
        # those are UNIFORM today, but check the rate anyway in case a
        # future module emits varying outputs.
        rate = getattr(expr.output_plug, "rate", "uniform")
        return rate.startswith("varying")
    if isinstance(expr, (Const, ParamRef)):
        return False
    # Composites — recurse into children. Mirror _lower._children but
    # inline since that helper isn't exported.
    if isinstance(expr, BinOp):
        return _expression_is_varying(expr.lhs) or _expression_is_varying(expr.rhs)
    if isinstance(expr, UnaryFn):
        return _expression_is_varying(expr.arg)
    if isinstance(expr, PowExpr):
        return _expression_is_varying(expr.x) or _expression_is_varying(expr.k)
    if isinstance(expr, ClampExpr):
        return (_expression_is_varying(expr.x)
                or _expression_is_varying(expr.lo)
                or _expression_is_varying(expr.hi))
    if isinstance(expr, MinMaxExpr):
        return _expression_is_varying(expr.a) or _expression_is_varying(expr.b)
    if isinstance(expr, LerpExpr):
        return (_expression_is_varying(expr.a)
                or _expression_is_varying(expr.b)
                or _expression_is_varying(expr.t))
    if isinstance(expr, FmodExpr):
        return _expression_is_varying(expr.x) or _expression_is_varying(expr.k)
    if isinstance(expr, SmoothstepExpr):
        return (_expression_is_varying(expr.x)
                or _expression_is_varying(expr.edge0)
                or _expression_is_varying(expr.edge1))
    if isinstance(expr, QuantizeExpr):
        return _expression_is_varying(expr.x) or _expression_is_varying(expr.step)
    if isinstance(expr, CurveExpr):
        return _expression_is_varying(expr.x)
    if isinstance(expr, Vec3Expr):
        return (_expression_is_varying(expr.x)
                or _expression_is_varying(expr.y)
                or _expression_is_varying(expr.z))
    return False


# Plugs whose declared rate is UNIFORM but whose CONSUMER iterates
# particles and re-pulls per particle. The plug-declared rate doesn't fully
# capture this — it describes the plug's own output cadence, not how the
# consumer drives reads. Until we add a proper consumer-rate annotation on
# the C++ side, this whitelist is the pragmatic answer.
#
# Adding a new per-particle-iterated input plug? Add its name here.
# (Names are global since plug names don't collide in practice — Aux is
# only on emitters; Width/Length/Scale only on StreakRenderer; etc.)
_VARYING_FRIENDLY_PLUGS = frozenset({
    "Aux",      # emitter — mPerParticleAux callback re-pulls per particle
    "Width",    # StreakRenderer — iterates particles in _render
    "Length",   # StreakRenderer
    "Scale",    # StreakRenderer / SpriteRenderer
    "Size",     # SpriteRenderer
})


def _validate_bind_rate(binding, expr, target_module, target_plug_name):
    """Raise if a varying-rate expression is being bound to a uniform-rate
    plug whose consumer ALSO reads it uniformly — the per-particle value
    would silently collapse to whichever particle's contribution the
    chain happens to sample once per compute. Silent garbage in.

    Skipped for:
      - Typed-module synthetic inputs (Min/Max/Lerp/Vec4Combine) — those
        evaluate via their own dataflow path and honor varying upstream
        through the chain.
      - Known consumer-iterated plugs (_VARYING_FRIENDLY_PLUGS) — their
        consumer's compute() loop re-pulls per particle even though the
        plug rate is declared uniform.
    """
    if not _expression_is_varying(expr):
        return
    target_plug = getattr(target_module.inputs, target_plug_name)
    rate = getattr(target_plug, "rate", "uniform")
    if rate.startswith("varying"):
        return  # OK — target accepts per-particle
    if target_plug_name in _VARYING_FRIENDLY_PLUGS:
        return  # OK — consumer is known to iterate
    # Allow varying sources on typed-module inputs (Min/Max/Lerp/Vec4Combine
    # etc.). Their consumer is the module's compute() which re-pulls per
    # dataflow rules; the chain handles per-particle eval transparently. We
    # detect these by module name prefix ("_DSL_") since they're synthetic.
    mod_name = getattr(target_module, "name", None) or \
               getattr(target_module, "_name", None)
    if mod_name and mod_name.startswith("_DSL_"):
        return
    bmod_name = getattr(binding.module, "name", None) or \
                getattr(binding.module, "_name", None) or "?"
    raise RuntimeError(
        f"bind(plug={binding.plug_name!r} on module {bmod_name!r}): "
        f"target plug rate is {rate!r} but the expression contains a "
        f"varying-rate (per-particle) leaf. The consumer reads this plug "
        f"once per compute call, so the per-particle value would silently "
        f"collapse to garbage. Either bind to a varying-rate plug (e.g. "
        f"renderer.Width, emitter.Aux.x) or rewrite the expression using "
        f"only uniform sources (time, dt, Expr.param.*, constants).")


def _is_variable_pow(node):
    """A PowExpr counts as a typed-module case ONLY when the exponent is
    NOT a Const. Const-exponent pow still chain-lowers via floatxfpowdata."""
    return isinstance(node, PowExpr) and not isinstance(node.k, Const)


# Reserved name format for implicit SINGLETON context-source modules. Format
# is f"_DSL_{ClassName}" — keyed by Python class identity so multiple DSL
# names backed by the same module class share one instance per graph (e.g.
# Expr.time and Expr.dt both back into a single `_DSL_Globals`).
# Vec3Combine modules emitted by the lowerer use this prefix + a per-graph
# index counter so each Vec3 binding gets its own instance.
_VEC3COMBINE_PREFIX = "_DSL_Vec3Combine_"
# Reserved name for the implicit Parameters module. One per graph; the DSL's
# self.expose() lazy-adds it on first call (in base.py) and the emitter
# looks it up here for Expr.param() resolution.
PARAMS_MODULE_NAME = "_DSL_Parameters"


class Binding:
    """A staged bind: connect the source `expr` resolves to → module.inputs.plug_name
    with the appropriate floatxf chain stages on the way in."""
    __slots__ = ("module", "plug_name", "expr")

    def __init__(self, module, plug_name, expr):
        self.module = module
        self.plug_name = plug_name
        self.expr = expr

    def __repr__(self):
        return f"Binding({self.module._name!r}, {self.plug_name!r}, {self.expr!r})"


class AxisBinding:
    """Per-axis binding for a vec4 input plug — `emitter.Aux.x/.y/.z/.w =`.
    Each axis is independently lowered; emit time materializes a single
    Vec4CombineModule and wires the four axis subgraphs into X/Y/Z/W.
    Unset axes default to Const(0)."""
    __slots__ = ("module", "plug_name", "axis_exprs")

    def __init__(self, module, plug_name, axis, expr):
        self.module = module
        self.plug_name = plug_name
        self.axis_exprs = {"x": Const(0.0), "y": Const(0.0),
                           "z": Const(0.0), "w": Const(0.0)}
        self.axis_exprs[axis] = expr

    def set_axis(self, axis, expr):
        self.axis_exprs[axis] = expr

    def __repr__(self):
        return (f"AxisBinding({self.module._name!r}, {self.plug_name!r}, "
                f"axes={ {k:v for k,v in self.axis_exprs.items()} })")


def emit_bindings(graphdata, bindings):
    """Apply each staged binding to graphdata. Mutates the graph: may add a
    Globals or Vec3Combine source module, appends floatxf chain stages to
    bound input plugs, creates source→target connections."""
    if not bindings:
        return

    # Per-call counter for naming emitted Vec3Combine modules.
    vec3_counter = 0

    for binding in bindings:
        if isinstance(binding, AxisBinding):
            _emit_axis_binding(graphdata, binding)
        elif _is_vec3_expr(binding.expr):
            _emit_vec3_binding(graphdata, binding, vec3_counter)
            vec3_counter += 1
        else:
            _emit_scalar_binding(graphdata, binding)


def _is_vec3_expr(node):
    """True if the Expr produces a vec3 value at the top level. Used by
    the binding dispatcher to choose the vec3 path. Currently:

      - Vec3Expr                       (three scalar axes combined)
      - EntityRef('pos'|'scale')       (vec3 leaf — quat is not vec3)
      - EntityTransform                (vec3 = host_xf * local)
      - BinOp('+', vec3, vec3)         (componentwise sum)
    """
    if isinstance(node, Vec3Expr):
        return True
    if isinstance(node, EntityRef):
        return node.component in ("pos", "scale")
    if isinstance(node, EntityTransform):
        return True
    if isinstance(node, BinOp) and node.op == "+":
        return _is_vec3_expr(node.lhs) and _is_vec3_expr(node.rhs)
    return False


def _emit_axis_binding(graphdata, binding):
    """Emit a Vec4-componentwise binding (e.g. emitter.Aux.x = ...).
    Creates a Vec4Combine module, wires each of the four axis Exprs into
    X/Y/Z/W via the recursive _wire_expr helper, connects the combined
    fvec4 output to the target vec4 input plug."""
    vec4_name = _alloc_unique_name(graphdata, "Vec4Combine")
    vec4combine = graphdata.create(vec4_name, _dflow.Vec4CombineModule)

    for axis, axis_input in (("x", "X"), ("y", "Y"), ("z", "Z"), ("w", "W")):
        _wire_expr(graphdata, binding, binding.axis_exprs[axis],
                   vec4combine, axis_input)

    target_plug = getattr(binding.module.inputs, binding.plug_name)
    graphdata.connect(target_plug, vec4combine.outputs.value)


def _alloc_unique_name(graphdata, kind):
    """Probe for the next free _DSL_<kind>_N module name."""
    i = 0
    while True:
        name = f"_DSL_{kind}_{i}"
        if graphdata.findModule(name) is None:
            return name
        i += 1


def _emit_scalar_binding(graphdata, binding):
    """Emit a single scalar binding. Dispatches into the recursive _wire_expr
    helper so the same path handles top-level chains AND nested typed
    modules (Min/Max/Lerp) whose inputs may themselves be chains or other
    typed modules."""
    _wire_expr(graphdata, binding, binding.expr,
               binding.module, binding.plug_name)


def _wire_expr(graphdata, binding, expr, target_module, target_plug_name):
    """Lower `expr` and wire its value into target_module.inputs[target_plug_name].

    Strategy, in order of preference:
      1. Const → set the input plug value directly (no source plug needed).
      2. lower_to_chain succeeds → resolve source, apply chain stages to the
         target plug, connect source → target. This is the cheap path for
         single-source linear expressions like 0.6 + sin(time)*0.2.
      3. MinMaxExpr / LerpExpr → emit the corresponding typed module
         (_DSL_Min_N, _DSL_Max_N, _DSL_Lerp_N) and recurse for each input.
         Each sub-expression is independently lowered (could be a chain, a
         nested typed module, or — eventually — a FloatExprModule).
      4. Anything else → NotImplementedError (FloatExprModule fallback, #27d).
    """

    if isinstance(expr, Const):
        setattr(target_module.inputs, target_plug_name, expr.value)
        return

    # Bind-rate validation: catch varying source → uniform target mismatches
    # before they silently corrupt runtime behavior.
    _validate_bind_rate(binding, expr, target_module, target_plug_name)

    # Try chain path on the expression as-is.
    if _try_emit_chain(graphdata, binding, expr,
                       target_module, target_plug_name):
        return

    # Top-level typed-module nodes — emit the module + recurse for inputs.
    if isinstance(expr, MinMaxExpr):
        kind = "Min" if expr.op == "min" else "Max"
        mod_cls = _dflow.MinModule if expr.op == "min" else _dflow.MaxModule
        mod = _create_typed_module(graphdata, kind, mod_cls)
        _wire_expr(graphdata, binding, expr.a, mod, "A")
        _wire_expr(graphdata, binding, expr.b, mod, "B")
        target_plug = getattr(target_module.inputs, target_plug_name)
        graphdata.connect(target_plug, mod.outputs.value)
        return

    if isinstance(expr, LerpExpr):
        mod = _create_typed_module(graphdata, "Lerp", _dflow.LerpModule)
        _wire_expr(graphdata, binding, expr.a, mod, "A")
        _wire_expr(graphdata, binding, expr.b, mod, "B")
        _wire_expr(graphdata, binding, expr.t, mod, "T")
        target_plug = getattr(target_module.inputs, target_plug_name)
        graphdata.connect(target_plug, mod.outputs.value)
        return

    if _is_variable_pow(expr):
        # Variable-exponent pow — chain stage can't handle a non-Const K.
        mod = _create_typed_module(graphdata, "Pow", _dflow.PowModule)
        _wire_expr(graphdata, binding, expr.x, mod, "X")
        _wire_expr(graphdata, binding, expr.k, mod, "K")
        target_plug = getattr(target_module.inputs, target_plug_name)
        graphdata.connect(target_plug, mod.outputs.value)
        return

    # Last resort before #27d: the expression's TOP isn't a typed-module
    # expr, but it may CONTAIN one (e.g. max(x, 0.05) * 20). Promote any
    # such subtree to its own real typed module, replace it with a
    # PromotedSource leaf, then retry chain-lowering on the rewritten tree.
    promoted = _promote_typed_subtrees(graphdata, binding, expr)
    if promoted is not expr:
        if _try_emit_chain(graphdata, binding, promoted,
                           target_module, target_plug_name):
            return

    raise NotImplementedError(
        f"bind(plug={binding.plug_name!r}): expression cannot be lowered "
        f"to a chain or any current typed module (multi-source mix or "
        f"unsupported op). FloatExprModule fallback isn't implemented "
        f"yet — see task #27d.")


def _try_emit_chain(graphdata, binding, expr, target_module, target_plug_name):
    """Attempt chain-lowering; on success, apply chain stages + connect.
    Returns True if the chain path handled the expression."""
    lowered = lower_to_chain(expr)
    if lowered is None:
        return False
    descriptor, stages = lowered
    if descriptor[0] == "const":
        setattr(target_module.inputs, target_plug_name, descriptor[1])
        return True
    source_plug = _resolve_source(graphdata, binding, descriptor)
    target_plug = getattr(target_module.inputs, target_plug_name)
    _apply_chain_to_plug(target_plug, stages)
    graphdata.connect(target_plug, source_plug)
    return True


def _promote_typed_subtrees(graphdata, binding, node):
    """Walk `node` post-order; for any MinMaxExpr/LerpExpr subtree, emit it
    as its own typed module (via _wire_expr recursion) and replace the
    subtree with a PromotedSource leaf pointing at the new module's output.
    Returns the rewritten tree (or `node` unchanged if nothing to promote)."""

    if isinstance(node, (MinMaxExpr, LerpExpr)) or _is_variable_pow(node):
        # Materialize this subtree into its own typed module, then return a
        # PromotedSource leaf the outer chain can wire from.
        if isinstance(node, MinMaxExpr):
            kind = "Min" if node.op == "min" else "Max"
            mod_cls = _dflow.MinModule if node.op == "min" else _dflow.MaxModule
            mod = _create_typed_module(graphdata, kind, mod_cls)
            _wire_expr(graphdata, binding, node.a, mod, "A")
            _wire_expr(graphdata, binding, node.b, mod, "B")
        elif isinstance(node, LerpExpr):
            mod = _create_typed_module(graphdata, "Lerp", _dflow.LerpModule)
            _wire_expr(graphdata, binding, node.a, mod, "A")
            _wire_expr(graphdata, binding, node.b, mod, "B")
            _wire_expr(graphdata, binding, node.t, mod, "T")
        else:  # variable-K PowExpr
            mod = _create_typed_module(graphdata, "Pow", _dflow.PowModule)
            _wire_expr(graphdata, binding, node.x, mod, "X")
            _wire_expr(graphdata, binding, node.k, mod, "K")
        return PromotedSource(mod.outputs.value)

    # Leaves (Const, ContextRef, ParamRef, PromotedSource) — nothing to do.
    if isinstance(node, (Const, ContextRef, ParamRef, PromotedSource)):
        return node

    # Composite — recurse into children. Build a fresh node only if any
    # child changed, otherwise return `node` unchanged so the "is" check
    # in the caller can short-circuit.
    def rec(child):
        return _promote_typed_subtrees(graphdata, binding, child)

    if isinstance(node, BinOp):
        l, r = rec(node.lhs), rec(node.rhs)
        if l is node.lhs and r is node.rhs: return node
        return BinOp(node.op, l, r)
    if isinstance(node, UnaryFn):
        a = rec(node.arg)
        if a is node.arg: return node
        return UnaryFn(node.fn, a)
    if isinstance(node, PowExpr):
        x, k = rec(node.x), rec(node.k)
        if x is node.x and k is node.k: return node
        return PowExpr(x, k)
    if isinstance(node, ClampExpr):
        x, lo, hi = rec(node.x), rec(node.lo), rec(node.hi)
        if x is node.x and lo is node.lo and hi is node.hi: return node
        return ClampExpr(x, lo, hi)
    if isinstance(node, FmodExpr):
        x, k = rec(node.x), rec(node.k)
        if x is node.x and k is node.k: return node
        return FmodExpr(x, k)
    if isinstance(node, SmoothstepExpr):
        x, e0, e1 = rec(node.x), rec(node.edge0), rec(node.edge1)
        if x is node.x and e0 is node.edge0 and e1 is node.edge1: return node
        return SmoothstepExpr(x, e0, e1)
    if isinstance(node, QuantizeExpr):
        x, s = rec(node.x), rec(node.step)
        if x is node.x and s is node.step: return node
        return QuantizeExpr(x, s)
    if isinstance(node, CurveExpr):
        x = rec(node.x)
        if x is node.x: return node
        return CurveExpr(x, node.curve)

    return node


def _materialize_for_share(graphdata, binding, expr):
    """Materialize `expr` ONCE into a graph source we can wire into multiple
    consumers (vec3 broadcast). Returns either the original Const (no
    sharing needed), a PromotedSource (when a typed module was created), or
    a PromotedSource wrapping a no-op chain materialization.

    For pure-chain expressions, the chain stages are CLONED per consumer by
    _wire_expr — that's fine. For typed-module expressions we want one
    module shared across consumers, which is what PromotedSource gives us."""
    if isinstance(expr, Const):
        return expr
    # If the top is a typed-module expr OR contains one, materialize via
    # the promotion helper (which builds the module and returns a
    # PromotedSource leaf).
    if isinstance(expr, (MinMaxExpr, LerpExpr)):
        return _promote_typed_subtrees(graphdata, binding, expr)
    # Composite that contains a typed-module subtree somewhere — promote
    # those subtrees in place. The outer chain still re-lowers per consumer,
    # which is cheap (just stage-list rebuild, no module duplication).
    return _promote_typed_subtrees(graphdata, binding, expr)


def _create_typed_module(graphdata, kind, module_cls):
    """Lazy-allocate the next free _DSL_<kind>_N slot. Probes by name so we
    don't need to thread a counter through the recursion."""
    i = 0
    while True:
        name = f"_DSL_{kind}_{i}"
        if graphdata.findModule(name) is None:
            return graphdata.create(name, module_cls)
        i += 1


def _emit_vec3_binding(graphdata, binding, vec3_counter):
    """Materialize a vec3 expression tree into a single output plug, then
    connect that to the target vec3 input. Vec3Expr lowers to a
    Vec3Combine (per-axis scalar chains); EntityRef/EntityTransform/+
    lower to their own modules. The recursion bottoms out in scalar
    chains at axis-level."""
    source_plug = _materialize_vec3_source(graphdata, binding,
                                           binding.expr, vec3_counter)
    target_plug = getattr(binding.module.inputs, binding.plug_name)
    graphdata.connect(target_plug, source_plug)


def _materialize_vec3_source(graphdata, binding, expr, vec3_counter=0):
    """Recursively produce an output plug yielding the vec3 value of `expr`.

    Vec3Expr               → Vec3Combine module (per-axis scalar chains).
    EntityRef('pos'/'scale') → EntityRef module (shared by entity name).
    EntityTransform        → TransformPoint / TransformDir; local input
                             wired via this same function (allows e.g.
                             transformPoint(Vec3Expr(...)) — local can be
                             any vec3 expression).
    BinOp('+', vec3, vec3) → Vec3Add module; A and B materialized
                             recursively.
    """
    if isinstance(expr, Vec3Expr):
        return _materialize_vec3combine(graphdata, binding, expr, vec3_counter)

    if isinstance(expr, EntityRef):
        return _resolve_entity_ref(graphdata, binding,
                                   expr.entity_name, expr.component)

    if isinstance(expr, EntityTransform):
        # Choose the right transform module class + IO plug names; both
        # share the same shape (one vec3 input, one vec3 output).
        if expr.mode == "point":
            module_cls   = _particles.TransformPoint
            reserved     = f"_DSL_TransformPoint_{expr.entity_name}"
            input_plug   = "LocalPoint"
            output_plug  = "WorldPoint"
        else:
            module_cls   = _particles.TransformDir
            reserved     = f"_DSL_TransformDir_{expr.entity_name}"
            input_plug   = "LocalDir"
            output_plug  = "WorldDir"
        # Unique name per (entity, mode, local-instance) so two different
        # transformPoint(...) calls with different local inputs get
        # separate modules. We allocate a fresh name; sharing happens for
        # the EntityRef lookup (Simulation side), not for the transform
        # module here.
        name = _alloc_unique_name(graphdata, f"{reserved.split('_DSL_', 1)[1]}")
        module = graphdata.create(name, module_cls)
        module.entity_name = expr.entity_name
        local_plug = _materialize_vec3_source(graphdata, binding, expr.local, vec3_counter)
        graphdata.connect(getattr(module.inputs, input_plug), local_plug)
        return getattr(module.outputs, output_plug)

    if isinstance(expr, BinOp) and expr.op == "+":
        name = _alloc_unique_name(graphdata, "Vec3Add")
        module = graphdata.create(name, _particles.Vec3Add)
        a_plug = _materialize_vec3_source(graphdata, binding, expr.lhs, vec3_counter)
        b_plug = _materialize_vec3_source(graphdata, binding, expr.rhs, vec3_counter)
        graphdata.connect(module.inputs.A, a_plug)
        graphdata.connect(module.inputs.B, b_plug)
        return module.outputs.Sum

    raise RuntimeError(
        f"bind(plug={binding.plug_name!r}): no vec3 lowering path for "
        f"{type(expr).__name__} (got {expr!r})")


def _materialize_vec3combine(graphdata, binding, vec3_expr, vec3_counter):
    """Original Vec3Combine path — three scalar axes → one fvec3 output.
    Used both for top-level Vec3Expr bindings and for vec3-shaped
    subtrees that recurse into Vec3Combine."""
    vec3_name = f"{_VEC3COMBINE_PREFIX}{vec3_counter}"
    if graphdata.findModule(vec3_name) is not None:
        # vec3_counter collided (nested vec3 — pick a fresh slot).
        vec3_name = _alloc_unique_name(graphdata, "Vec3Combine")
    vec3combine = graphdata.create(vec3_name, _particles.Vec3Combine)

    # Vec3Expr's single-arg broadcast (`Expr.vec3(x)`) sets all three
    # axes to the SAME Expr object. Detect that and materialize the
    # expression exactly once — share the result across X/Y/Z. Avoids
    # creating three identical typed-module copies for `vec3(max(p, 0))`
    # and similar.
    bx, by, bz = vec3_expr.x, vec3_expr.y, vec3_expr.z
    if bx is by and by is bz:
        promoted = _materialize_for_share(graphdata, binding, bx)
        for axis_name in ("X", "Y", "Z"):
            _wire_expr(graphdata, binding, promoted, vec3combine, axis_name)
    else:
        for axis_name, axis_expr in (("X", bx), ("Y", by), ("Z", bz)):
            _wire_expr(graphdata, binding, axis_expr, vec3combine, axis_name)
    return vec3combine.outputs.value


def _apply_chain_to_plug(target_plug, stages):
    """Attach the lowered chain stages to a FloatXf input plug's transformer.
    Uses NUMERICALLY-PREFIXED keys ("001_scale", "002_sine", ...) because
    orklut iterates by key — execution order = lexicographic key order, so
    numeric prefixes guarantee execution matches the lowerer's intended order
    regardless of stage count."""
    for i, stage in enumerate(stages):
        key = f"{i:03d}_{stage.kind}"
        target_plug.transformer.set(key, _build_stage(stage))


def _resolve_source(graphdata, binding, descriptor):
    """Look up (or lazy-create) the source plug for a non-const descriptor."""
    kind = descriptor[0]
    if kind == "ctx":
        return _resolve_context_var(graphdata, binding, descriptor[1])
    if kind == "entity":
        return _resolve_entity_ref(graphdata, binding,
                                   descriptor[1], descriptor[2])
    if kind == "param":
        return _resolve_param(graphdata, binding, descriptor[1])
    if kind == "promoted":
        # The plug was captured during _promote_typed_subtrees — just hand it
        # back to the chain emitter for connection.
        return descriptor[1]
    raise RuntimeError(f"unknown source descriptor kind: {kind!r}")


def _resolve_entity_ref(graphdata, binding, entity_name, component):
    """Resolve Expr.entity('name').pos/.quat/.scale — find-or-create a
    particles.EntityRef module configured to track that name, then
    return the requested output plug. Module name is keyed by
    entity_name so multiple references to the same entity share one
    instance (matches the SINGLETON pattern for Globals).

    Component is one of {'pos', 'quat', 'scale'} → output plug
    {'Pos', 'Quat', 'Scale'}. Wrong type at the consumer's input plug
    (e.g. binding a quat reference into a vec3 input) is surfaced as
    a connection error by the underlying connect()."""
    plug_for = {
        "pos":           "Pos",
        "quat":          "Quat",
        "scale":         "Scale",
        "scale_uniform": "ScaleUniform",   # scalar — typed as float for chain bindings
    }
    if component not in plug_for:
        raise RuntimeError(
            f"bind(plug={binding.plug_name!r}): Expr.entity({entity_name!r}) "
            f"has no component {component!r} — expected one of "
            f"{sorted(plug_for)}")

    # Import locally to dodge import-cycle with ork.dflow.particles which
    # itself imports _bindings via the particle DSL surface.
    from orkengine.lev2 import particles as _lev2_particles
    reserved_name = f"_DSL_EntityRef_{entity_name}"
    existing = graphdata.findModule(reserved_name)
    if existing is None:
        existing = graphdata.create(reserved_name, _lev2_particles.EntityRef)
        # graphdata.create returns the moduledata directly (mirrors how
        # _resolve_context_var consumes existing.outputs — there's no
        # node-wrapper layer here).
        existing.entity_name = entity_name
    return getattr(existing.outputs, plug_for[component])


def _resolve_param(graphdata, binding, name):
    """Resolve Expr.param(name) — find the implicit Parameters module in this
    graph and return its named output plug. The module must already exist
    (DSL author should have called self.expose(name, default) first) and the
    name must already be exposed on it."""
    pm = graphdata.findModule(PARAMS_MODULE_NAME)
    if pm is None:
        raise RuntimeError(
            f"bind(plug={binding.plug_name!r}): Expr.param({name!r}) "
            f"used but no parameters have been exposed in this graph. "
            f"Call self.expose({name!r}, default=...) in __init__ first.")
    if name not in list(pm.param_names):
        raise RuntimeError(
            f"bind(plug={binding.plug_name!r}): Expr.param({name!r}) "
            f"references an unexposed parameter. Available: {list(pm.param_names)}. "
            f"Call self.expose({name!r}, default=...) in __init__.")
    return getattr(pm.outputs, name)


def _resolve_context_var(graphdata, binding, dsl_name):
    """Resolve a registered context-variable name (e.g. 'time',
    'ptc.unit_age') into the source output plug. Consults the C++ registry
    for policy + plug name, the Python class mirror for the module class.

    - SINGLETON policy: find-or-create one instance per source class in the
      graph (so multiple binds of Expr.time share the same Globals).
    - REQUIRE_EXISTING policy: find the user-declared module of the right
      class; raise if missing (the DSL never invents one)."""

    spec = _dflow.context_variables.lookup(dsl_name)
    if spec is None:
        raise RuntimeError(
            f"bind(plug={binding.plug_name!r}): context variable "
            f"{dsl_name!r} is not registered in the C++ ContextVariableRegistry. "
            f"Check the static initializer in the engine module's .cpp file.")

    py_class = python_class_for(dsl_name)
    if py_class is None:
        raise RuntimeError(
            f"bind(plug={binding.plug_name!r}): context variable "
            f"{dsl_name!r} has a C++ registration but no Python class "
            f"mapping. The family package needs to call "
            f"register_python_class({dsl_name!r}, <PyClass>) at import time.")

    if spec.policy == "singleton":
        reserved_name = f"_DSL_{py_class.__name__}"
        existing = graphdata.findModule(reserved_name)
        if existing is None:
            existing = graphdata.create(reserved_name, py_class)
        return getattr(existing.outputs, spec.output_plug_name)

    if spec.policy == "require_existing":
        found = graphdata.findModuleByClass(py_class)
        if found is None:
            raise RuntimeError(
                f"bind(plug={binding.plug_name!r}): context variable "
                f"{dsl_name!r} requires a {py_class.__name__!r} module in "
                f"the graph but none was found. Declare one explicitly in "
                f"your DSL class (e.g. P.PoolData(size=...) for the pool).")
        return getattr(found.outputs, spec.output_plug_name)

    raise RuntimeError(
        f"bind(plug={binding.plug_name!r}): unknown context-var policy "
        f"{spec.policy!r} for {dsl_name!r}")


def _build_stage(stage):
    """Construct the C++ floatxf stage object for the given ChainStage."""
    xf = _dflow.floatxf
    if stage.kind == "scale":
        s = xf.scale(stage.args["scale"]); s.do_scale = True;   return s
    if stage.kind == "bias":
        s = xf.bias(stage.args["bias"]);   s.do_bias  = True;   return s
    if stage.kind == "sine":
        s = xf.sine();                     s.do_sine  = True;   return s
    if stage.kind == "abs":
        s = xf.abs();                      s.do_abs   = True;   return s
    if stage.kind == "power":
        s = xf.power(stage.args["power"]); s.do_pow   = True;   return s
    if stage.kind == "mod":
        s = xf.mod(stage.args["mod"]);     s.do_mod   = True;   return s
    if stage.kind == "smoothstep":
        s = xf.smoothstep(stage.args["edge0"], stage.args["edge1"])
        s.do_smoothstep = True
        return s
    if stage.kind == "quantize":
        s = xf.quantize(stage.args["step"]); s.do_quantize = True; return s
    if stage.kind == "curve":
        curve_data = stage.args["curve"]
        # MultiCurve1D pyext binding exposes `setPoint` + `splitSegment`
        # (see ork.core/src/python/common_bindings/pyext_math.cpp). Older
        # versions of this check guessed `addControlPoint` which was never
        # bound — that was a typo, not a deferred feature.
        if not hasattr(curve_data, "setPoint"):
            raise TypeError(
                f"Expr.curve(...) expected a MultiCurve1D (or duck-compatible "
                f"object with .setPoint); got {type(curve_data).__name__}. "
                f"Build one with `dataflow.floatxf.multicurve().multicurve` "
                f"and populate via splitSegment(i) + setPoint(idx, t, v).")
        s = xf.multicurve()
        s.multicurve = curve_data
        s.do_curve = True
        return s
    raise RuntimeError(f"unknown stage kind: {stage.kind!r}")
