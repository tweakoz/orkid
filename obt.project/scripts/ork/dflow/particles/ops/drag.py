from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def drag(upstream, *, name=None, **plug_kwargs):
    """Linear drag (velocity damping). Maps to particles.Drag."""
    return chain_op(_particles.Drag, "DRAG", upstream,
                    name=name, **plug_kwargs)
