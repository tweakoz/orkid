from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def point_attractor(upstream, *packs, name=None, **plug_kwargs):
    """Point attractor. Maps to particles.PointAttractor."""
    return chain_op(_particles.PointAttractor, "ATTR", upstream,
                    name=name, packs=packs, **plug_kwargs)
