from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def vortex(upstream, *, name=None, **plug_kwargs):
    """Vortex force (rotation + outward/inward). Maps to particles.Vortex."""
    return chain_op(_particles.Vortex, "VORT", upstream,
                    name=name, **plug_kwargs)
