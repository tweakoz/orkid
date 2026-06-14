###############################################################################
# ork.dflow.particles.ops.pool_data — particle pool DSL op.
#
# Maps to the C++ ptc::ParticlePoolData / Python `particles.Pool` module.
# Authoring pattern:
#
#   pool = P.pool_data(size=4096, name="POOL")
#
# Equivalent imperative form (existing ptc_*.py pattern):
#
#   pool_module = graphdata.create("POOL", particles.Pool)
#   pool_module.pool_size = 4096
###############################################################################

import itertools

from orkengine.lev2 import particles as _particles

from ..._trace import current_graph, DslNode


# Per-trace anonymous-name counter — DSL ops without an explicit name= get an
# auto-incremented suffix. Reset implicitly per-trace because module-level
# globals leak across; using a counter keyed by graph id keeps names unique
# within a graph without cross-graph collisions.
_anon_counters = {}


def _next_anon_name(prefix, graph):
    key = id(graph)
    if key not in _anon_counters:
        _anon_counters[key] = itertools.count(0)
    return f"{prefix}_{next(_anon_counters[key])}"


def pool_data(size, name=None):
    """Particle pool — the buffer every emitter/force/renderer in the chain
    references. Returns a DslNode whose `.output_plug` is the pool's `pool`
    output (downstream emitter ops consume it via their `pool` input).

    Args:
      size: max number of particles in the pool (maps to .pool_size on the
            underlying ParticlePoolData)
      name: optional module name in the graph; defaults to "pool_<n>" with an
            auto-incremented suffix. Existing imperative-form scripts use
            short upper-case names like "POOL" — pass name="POOL" to match.
    """
    graph = current_graph()
    if graph is None:
        raise RuntimeError(
            "P.pool_data() called outside a trace context — did you forget to "
            "call `super().__init__()` at the top of your ParticleSystem "
            "subclass __init__?")
    module_name = name if name is not None else _next_anon_name("pool", graph)
    module = graph.create(module_name, _particles.Pool)
    module.pool_size = size
    # ParticlePoolData exposes a single output named "pool" (see C++
    # ParticleModuleData::_initPoolIOs); that's the plug downstream ops connect
    # to via their own `pool` input.
    return DslNode(module=module, output_plug=module.outputs.pool)
