from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def nozzle_emitter(upstream, *, name=None, **plug_kwargs):
    """Directional nozzle emitter. Maps to particles.NozzleEmitter."""
    return chain_op(_particles.NozzleEmitter, "EMITZ", upstream,
                    name=name, **plug_kwargs)
