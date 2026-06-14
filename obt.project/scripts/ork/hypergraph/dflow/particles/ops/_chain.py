###############################################################################
# ork.dflow.particles.ops._chain — shared helper for chained particle ops.
#
# Every particle op past the pool follows the same pattern: take an upstream
# DslNode (the previous module's `pool` output), create a new module via
# graph.create, connect its `pool` input to upstream, apply plug kwargs,
# return a DslNode wrapping its `pool` output (or nothing for renderers).
#
# Each public op file (gravity.py, turbulence.py, etc.) is ~10 lines —
# basically: import the C++ particle class, call _chain_op with it.
###############################################################################

import itertools

from orkengine.lev2 import particles as _particles  # noqa: F401  re-used by ops

from ..._trace import current_graph, current_bindings, DslNode
from ..._expr import ExprNode
from ..._bindings import Binding
from ..._parampack import ParamPack, _input_schema


# Per-graph anonymous-name counter (mirrors pool_data.py's _anon_counters but
# scoped to the chain helper so it doesn't collide with pool counters).
_anon_counters = {}


def _next_anon_name(prefix, graph):
    key = (id(graph), prefix)
    if key not in _anon_counters:
        _anon_counters[key] = itertools.count(0)
    return f"{prefix}_{next(_anon_counters[key])}"


def chain_op(particle_class, name_prefix, upstream, *,
             name=None, material=None, packs=(), **plug_kwargs):
    """Create a particle module that consumes the upstream pool, applies plug
    kwargs, optionally attaches a material (renderer ops only), and returns a
    DslNode wrapping its own `pool` output so the chain can continue.

    Args:
      particle_class: the C++ Python class (e.g. _particles.Gravity)
      name_prefix:    short prefix for auto-generated module names ("grav",
                      "emit", "streak", ...) — used when caller doesn't pass
                      an explicit name=. Matches the convention in
                      ptc_*.py imperative tests.
      upstream:       DslNode whose .output_plug is connected to the new
                      module's pool input.
      name:           optional explicit module name override.
      material:       optional material to assign to the new module's
                      .material attribute (renderer ops accept this; others
                      have no .material and will raise if you pass it).
      **plug_kwargs:  forwarded to module.inputs.<key> = value (each is a
                      named input plug on the module).
    """
    if not isinstance(upstream, DslNode):
        raise TypeError(
            f"chain_op expected upstream DslNode (output of a particle op); "
            f"got {type(upstream).__name__}")
    graph = current_graph()
    if graph is None:
        raise RuntimeError(
            "chain_op called outside a trace context — call super().__init__() "
            "at the top of your ParticleSystem subclass __init__.")
    module_name = name if name is not None else _next_anon_name(name_prefix, graph)
    module = graph.create(module_name, particle_class)
    # connect: new module's `pool` input ← upstream `pool` output
    graph.connect(module.inputs.pool, upstream.output_plug)
    if material is not None:
        module.material = material
    # merge ParamPack(s) into plug_kwargs: no duplicate keys across packs (ValueError),
    # explicit plug_kwargs WIN over pack values. Packs are CONSTANT bundles (never Exprs).
    if packs:
        merged, owner = {}, {}
        for i, p in enumerate(packs):
            if not isinstance(p, ParamPack):
                raise TypeError(f"{particle_class.__name__} pack args must be ParamPack; got {type(p).__name__}")
            for k, v in p.as_dict().items():
                if k in owner:
                    raise ValueError(f"param {k!r} supplied by multiple packs (no duplicates allowed)")
                owner[k] = i
                merged[k] = v
        merged.update(plug_kwargs)  # explicit override
        plug_kwargs = merged
        # validate constant param NAMES against the module's input schema (clean error
        # vs the C++ OrkAssert abort). Expr-valued kwargs use a different path; skip them.
        schema = _input_schema(module.inputs)
        if schema:
            for k, v in plug_kwargs.items():
                if not isinstance(v, ExprNode) and k not in schema:
                    raise KeyError(f"{particle_class.__name__} has no input param {k!r}; valid: {sorted(schema)}")
    # Route kwargs the same way DslNode.__setattr__ does:
    #   - ExprNode  → stage a Binding so the lowerer wires up Globals /
    #     ContextVar / floatxf chain on emit (lets per-particle exprs
    #     like Expr.curve(Expr.ptc.unit_age, profile) reach Size/Width/...)
    #   - scalars / vec3 / quat → direct setattr through the C++
    #     input_proxy (sets the plug's stored default value).
    for k, v in plug_kwargs.items():
        if isinstance(v, ExprNode):
            bindings = current_bindings()
            if bindings is None:
                raise RuntimeError(
                    f"{particle_class.__name__}({k}=<Expr>) called outside "
                    f"a trace context — call super().__init__() at the top "
                    f"of your ParticleSystem subclass __init__.")
            bindings.append(Binding(module, k, v))
        else:
            setattr(module.inputs, k, v)
    return DslNode(module=module, output_plug=module.outputs.pool)
