from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def elliptical_attractor(upstream, *, name=None, **plug_kwargs):
    """Two-focus elliptical attractor. Maps to particles.EllipticalAttractor."""
    return chain_op(_particles.EllipticalAttractor, "SPHR", upstream,
                    name=name, **plug_kwargs)
