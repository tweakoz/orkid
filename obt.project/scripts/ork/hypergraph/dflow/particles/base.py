###############################################################################
# ork.dflow.particles.base — ParticleSystem family base class.
#
# User code subclasses ParticleSystem and builds the graph in __init__ via DSL
# ops. The base class:
#  - allocates a fresh dflow.GraphData on construction
#  - opens a trace context so DSL ops target that graph
#  - calls user __init__ (subclass-implemented)
#  - tears down trace on success or exception
#  - exposes self.render(node) to record the output renderer
#  - exposes self.graphdata + self.output_renderer for caller use
#  - generatedflow() returns the populated GraphData (the standard handle that
#    downstream code — eventually a particles materializer — turns into a
#    drawable)
#
# M1 deliberately keeps materialize logic out — particle "materialization" is
# `ParticlesDrawableData(graphdata)` + `createDrawable()` (per the existing
# ptc_*.py pattern), already in C++. A thin Python wrapper will land in a
# follow-up step alongside the rest of the particle ops.
###############################################################################

from orkengine.core import dataflow as _dflow
from orkengine.lev2 import particles as _particles
from .._trace import enter_trace, leave_trace, DslNode
from .._bindings import emit_bindings, PARAMS_MODULE_NAME


class ParticleSystem:
    """Family base for HyperSyn particles graphs authored via the DSL.

    Subclass and implement __init__:

        from ork.hypergraph.dflow.particles import ParticleSystem
        from ork.hypergraph.dflow import particles as P

        class FireExplosion(ParticleSystem):
          def __init__(self):
            super().__init__()                       # set up trace context
            self.pool = P.pool_data(size=4096)       # DSL ops mutate self.graphdata
            ...
            self.render(some_renderer_node)

    Use the resulting instance:

        sys = FireExplosion()
        graphdata = sys.generatedflow()              # the populated dflow.GraphData
        # ... downstream: createGraphInst, attach to a drawable, etc.
    """

    def __init__(self, **kwargs):
        # Fresh empty graph; DSL ops will populate it during user __init__.
        self.graphdata = _dflow.GraphData.createShared()
        self.output_renderer = None
        # Staged bindings — populated by DslNode.bind() calls during user
        # __init__. Drained at generatedflow() time.
        self._bindings = []
        # **kwargs are accepted and ignored at the base — subclasses opt in
        # by declaring their own signature (e.g. `def __init__(self, *,
        # funnel_sdf=None, base_radius=1.0)`) and capturing the kwargs they
        # care about. This enables parameterized particle systems (HYPERECS
        # Tier 2) — same class reusable by Tier 1 standalone viewer (default
        # kwargs), Tier 3 Scene composite (kwargs supplied by host).
        # Backward-compatible: all existing subclasses call super().__init__()
        # with zero positional args and don't accept kwargs themselves.
        #
        # Open the trace context. The matching leave_trace() happens in
        # generatedflow() — keeping it open across __init__ + post-init mutation
        # so users can still call DSL ops outside __init__ if they want to
        # extend the graph later (uncommon but supported).
        #
        # Subclasses MUST call super().__init__() at the top of their __init__
        # so the trace is active for the DSL calls that follow.
        self._prev_trace = enter_trace(self.graphdata, bindings=self._bindings)

    def onUpdate(self, updinfo):
        """Per-frame Python tick hook. The base default is a no-op so DSL
        classes that author everything statically (via bind() in __init__)
        don't need to define this method. Subclasses MAY override for the
        legacy imperative-mutation form (`self.emitter.inputs.X = ...`); new
        DSL code should prefer bind() with Expr so the per-frame path stays
        in C++."""
        pass

    def expose(self, name, default=0.0):
        """Declare a runtime-mutable scalar parameter visible to the graph.

        Adds a named float output plug on the graph's (lazy-added) Parameters
        module. Author binds against it via `Expr.param("Name")`; gameplay
        mutates the value at runtime via a SET_PARAM ECS notify on the
        owning ParticlesComponent.

        Defaults reapply on every START / slot recycle — a fresh slot
        always sees the declared defaults until gameplay overrides.
        """
        if not self._is_tracing():
            raise RuntimeError(
                f"expose({name!r}) called outside a trace context — "
                f"call super().__init__() at the top of your "
                f"ParticleSystem subclass __init__.")
        # Lazy-add the Parameters module to this graph. One per graph, shared
        # by every expose() call — the same module's output set grows.
        pm = self.graphdata.findModule(PARAMS_MODULE_NAME)
        if pm is None:
            pm = self.graphdata.create(PARAMS_MODULE_NAME, _particles.Parameters)
        pm.addFloatParam(name, float(default))

    def render(self, node):
        """Record the final renderer node. node must be a DslNode produced by a
        particles renderer op (sprite_renderer / streak_renderer / etc.)."""
        if not isinstance(node, DslNode):
            raise TypeError(
                f"ParticleSystem.render() expects a DslNode (the output of a "
                f"renderer op like P.streak_renderer(...)); got {type(node).__name__}")
        self.output_renderer = node

    def generatedflow(self):
        """Close the trace context, apply staged bindings, return the populated
        dflow.GraphData. Idempotent — calling more than once just returns the
        same graph (and won't re-apply bindings on subsequent calls)."""
        if self._is_tracing():
            leave_trace(self._prev_trace)
            self._prev_trace = None
            # Apply bindings AFTER closing the trace — emit_bindings does
            # graph.create(...) for the implicit Globals, and we don't want
            # any nested DSL ops it might trigger to land in our trace state.
            emit_bindings(self.graphdata, self._bindings)
            self._bindings = []   # drain so re-call is a no-op
        return self.graphdata

    def _is_tracing(self):
        # Internal helper — True iff this instance still owns the trace.
        from .._trace import current_graph
        return current_graph() is self.graphdata
