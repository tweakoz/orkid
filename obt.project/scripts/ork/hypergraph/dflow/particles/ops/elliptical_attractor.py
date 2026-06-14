from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def elliptical_attractor(upstream, *packs, name=None, **plug_kwargs):
    """Two-focus elliptical attractor. Maps to particles.EllipticalAttractor."""
    return chain_op(_particles.EllipticalAttractor, "SPHR", upstream,
                    name=name, packs=packs, **plug_kwargs)
