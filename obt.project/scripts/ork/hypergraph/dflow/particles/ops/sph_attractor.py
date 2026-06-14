from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def sph_attractor(upstream, *packs, name=None, **plug_kwargs):
    """Spherical attractor. Maps to particles.SphAttractor."""
    return chain_op(_particles.SphAttractor, "ATTR", upstream,
                    name=name, packs=packs, **plug_kwargs)
