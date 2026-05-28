from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def sph_attractor(upstream, *, name=None, **plug_kwargs):
    """Spherical attractor. Maps to particles.SphAttractor."""
    return chain_op(_particles.SphAttractor, "ATTR", upstream,
                    name=name, **plug_kwargs)
