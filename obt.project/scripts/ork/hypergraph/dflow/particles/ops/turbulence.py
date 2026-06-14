from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def turbulence(upstream, *packs, name=None, **plug_kwargs):
    """Per-particle turbulence/noise. Maps to particles.Turbulence."""
    return chain_op(_particles.Turbulence, "TURB", upstream,
                    name=name, packs=packs, **plug_kwargs)
