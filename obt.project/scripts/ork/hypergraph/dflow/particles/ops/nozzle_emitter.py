from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def nozzle_emitter(upstream, *packs, name=None, **plug_kwargs):
    """Directional nozzle emitter. Maps to particles.NozzleEmitter."""
    return chain_op(_particles.NozzleEmitter, "EMITZ", upstream,
                    name=name, packs=packs, **plug_kwargs)
