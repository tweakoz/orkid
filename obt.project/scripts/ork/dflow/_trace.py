###############################################################################
# ork.dflow._trace — trace-context primitive for the HyperSyn DSL.
#
# A "trace context" is the thread-local state DSL ops consult to know which
# graph they should add modules+connections to. The family base class (e.g.
# ParticleSystem) sets up the trace context before calling user __init__ and
# tears it down after. While the trace is active, op calls (P.pool_data,
# P.gravity, etc.) implicitly target the current graph.
#
# DslNode is the opaque handle ops return. Internally it wraps a concrete
# outplugdata_ptr_t (an output of a real dflow module created via
# graphdata.create). Downstream ops accept DslNode arguments and connect to
# their wrapped plug. The wrapping is just to keep the DSL surface decoupled
# from C++ types — at materialize time, DslNode.output_plug is the underlying
# plug the consumer wires to.
###############################################################################

import threading


_local = threading.local()


def current_graph():
    """Return the graph currently being traced, or None if no trace is active.

    DSL ops call this to obtain the graphdata_ptr_t they should mutate. Calling
    a DSL op outside an active trace is a usage error.
    """
    return getattr(_local, "graph", None)


def current_bindings():
    """Return the staged-bindings list for the active trace, or None if no
    trace. DslNode.bind() appends to this; the family base consumes it at
    generatedflow() time."""
    return getattr(_local, "bindings", None)


def enter_trace(graph, bindings=None):
    """Push (graph, bindings) as the current trace target. Returns a snapshot
    of the previous state so the caller can restore it on leave. If `bindings`
    is None, an empty list is created — the caller owns the same list, so it
    can read it back after leave_trace() to emit deferred work."""
    prev = (getattr(_local, "graph", None), getattr(_local, "bindings", None))
    _local.graph = graph
    _local.bindings = bindings if bindings is not None else []
    return prev


def leave_trace(prev):
    """Pop the trace, restoring the previous (graph, bindings) pair. Accepts
    the tuple returned by enter_trace()."""
    prev_graph, prev_bindings = prev
    _local.graph = prev_graph
    _local.bindings = prev_bindings


class _ComponentVecProxy:
    """Per-axis accessor for vec3/vec4 input plugs on a DslNode. Returned by
    DslNode.Aux (and any future vec component proxies). Mirrors DslNode's
    bind-vs-direct routing per axis:

       proxy.x = Expr(...)   →  dslnode.bind("AuxX-or-equivalent", value)

    Actually does it via Vec4Expr so the recursive vec4 emitter picks it up
    just like Expr.vec4(x, y, z, w) does — see _emit_aux_binding in
    _bindings.py. Component assignments accumulate on the proxy until the
    binding is finalized; setting only .x leaves .y/.z/.w as Const(0)."""
    __slots__ = ("_dslnode", "_prefix")

    _AXIS_INDEX = {"x": 0, "y": 1, "z": 2, "w": 3}

    def __init__(self, dslnode, prefix):
        object.__setattr__(self, "_dslnode", dslnode)
        object.__setattr__(self, "_prefix", prefix)

    def __setattr__(self, axis, value):
        if axis not in _ComponentVecProxy._AXIS_INDEX:
            raise AttributeError(
                f"vec4 proxy supports only .x/.y/.z/.w (got {axis!r})")
        # Build (or update) the staged AxisBinding for this plug.
        from ._expr import ExprNode, Const
        if isinstance(value, (int, float)):
            value = Const(float(value))
        if not isinstance(value, ExprNode):
            raise TypeError(
                f"{self._prefix}.{axis} = ... expects an Expr tree or "
                f"numeric literal; got {type(value).__name__}")
        _record_axis_binding(self._dslnode, self._prefix, axis, value)


def _record_axis_binding(dslnode, plug_name, axis, expr):
    """Add or update an AxisBinding entry for (dslnode.module, plug_name).
    Multiple .x/.y/.z/.w assignments coalesce onto the same AxisBinding so
    the emitter sees one Binding with up to four axis Exprs."""
    from ._bindings import AxisBinding
    bindings = current_bindings()
    if bindings is None:
        raise RuntimeError(
            f"{plug_name}.{axis} = ... called outside a trace context — "
            f"call super().__init__() at the top of your ParticleSystem "
            f"subclass __init__.")
    for b in bindings:
        if (isinstance(b, AxisBinding)
                and b.module is dslnode._module
                and b.plug_name == plug_name):
            b.set_axis(axis, expr)
            return
    bindings.append(AxisBinding(dslnode._module, plug_name, axis, expr))


class DslNode:
    """Opaque handle wrapping a real outplugdata. Returned by DSL ops; passed
    as input to downstream ops. The DSL surface treats DslNode as a black box;
    op .build() functions reach inside via .output_plug to make connections."""

    __slots__ = ("_module", "_output_plug")

    def __init__(self, module, output_plug):
        self._module = module               # the underlying dgmoduledata_ptr_t
        self._output_plug = output_plug     # an outplugdata_ptr_t on _module

    @property
    def module(self):
        """The underlying dflow module the op created. Available so user code can
        write `self.emit.inputs.LifeSpan = 1.0` for per-frame mutation, matching
        the imperative-form pattern that ptc_*.py examples use today."""
        return self._module

    @property
    def output_plug(self):
        """The underlying outplugdata_ptr_t this node hands downstream consumers."""
        return self._output_plug

    @property
    def inputs(self):
        """Pass-through to the underlying module's inputs proxy so per-frame
        mutation works: `self.emit.inputs.MinV = 0.6`."""
        return self._module.inputs

    @property
    def outputs(self):
        return self._module.outputs

    def bind(self, plug_name, expr):
        """Stage a binding: at generatedflow() time, the named input plug on
        this node's module will be wired up to whatever source the Expr resolves
        to (Globals.RelTime for Expr.time, ParametersModule for Expr.param(...))
        with the corresponding floatxf chain stages applied.

        `expr` accepts: an Expr tree (the common case), a Python number (auto-
        wrapped as a constant), or any object the lowerer can constant-fold to
        a leaf. Non-Expr non-numeric values raise TypeError.

        Authors usually use the attribute-assignment form instead of calling
        bind() directly:

            self.emitter.MinV = 0.6 + Expr.sin(Expr.time) * 0.2

        which routes through __setattr__ → bind() for Expr values. bind() is
        still here for dynamic plug names (`plug_name` is a variable).
        """
        from ._expr import ExprNode, Const
        from ._bindings import Binding

        if isinstance(expr, (int, float)):
            expr = Const(expr)
        if not isinstance(expr, ExprNode):
            raise TypeError(
                f"bind(plug={plug_name!r}, expr=...) expects an Expr tree or "
                f"numeric literal; got {type(expr).__name__}")

        bindings = current_bindings()
        if bindings is None:
            raise RuntimeError(
                f"bind() called outside a trace context — call super().__init__() "
                f"at the top of your ParticleSystem subclass __init__.")
        bindings.append(Binding(self._module, plug_name, expr))

    # Attributes that map to DslNode itself (slots + properties + methods)
    # — never interpreted as plug names. setattr to any of these falls
    # through to the default (slots → write; properties → AttributeError).
    _RESERVED_ATTRS = frozenset({
        "_module", "_output_plug",                       # __slots__
        "module", "output_plug", "inputs", "outputs",    # @property
        "Aux",                                           # component-vec proxy property
        "bind",                                          # method
    })

    @property
    def Aux(self):
        """Component-vec proxy for the underlying Vec4 'Aux' plug. Lets DSL
        authors write the per-axis form:

            self.emitter.Aux.x = 0.5
            self.emitter.Aux.y = Expr.sin(Expr.time) * 0.5 + 0.5

        Each axis binding lowers independently and combines through a
        Vec4Combine module whose fvec4 output connects to the Aux plug.
        The emitter reads that vec4 per-emit and writes to particle._aux.

        Available on emitter DslNodes (which carry the Aux input plug).
        Setting an axis on a non-emitter raises at generatedflow() time."""
        return _ComponentVecProxy(self, "Aux")

    def __setattr__(self, name, value):
        """Route attribute writes by type:

           self.emitter.MinV = Expr(...)   →  bind(name, value)
           self.emitter.MinV = 0.5         →  module.inputs.MinV = 0.5
           self.emitter.module = ...       →  AttributeError (slot violation)

        Lets DSL authors write the algebraic form without bind() boilerplate."""
        if name in DslNode._RESERVED_ATTRS:
            # Slot writes (during __init__) succeed; property/method writes
            # raise the natural slot-error since neither has a setter.
            object.__setattr__(self, name, value)
            return
        from ._expr import ExprNode
        if isinstance(value, ExprNode):
            self.bind(name, value)
        else:
            # Plain value — set the plug's stored default directly, matching
            # the existing self.X.inputs.PLUG = value imperative pattern.
            setattr(self._module.inputs, name, value)

    def __repr__(self):
        return f"DslNode(module={self._module._name!r})"
