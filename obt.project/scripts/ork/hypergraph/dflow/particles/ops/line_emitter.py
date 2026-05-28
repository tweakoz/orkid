from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def line_emitter(upstream, *, name=None, **plug_kwargs):
    """Emitter along a line segment P1→P2. Maps to particles.LineEmitter."""
    return chain_op(_particles.LineEmitter, "EMITL", upstream,
                    name=name, **plug_kwargs)
