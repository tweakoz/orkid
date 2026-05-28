from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def ring_emitter(upstream, *, name=None, **plug_kwargs):
    """Particles emitted on a ring. Maps to particles.RingEmitter."""
    return chain_op(_particles.RingEmitter, "EMITR", upstream,
                    name=name, **plug_kwargs)
