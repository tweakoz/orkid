from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def gravity(upstream, *, name=None, **plug_kwargs):
    """Many-particles-toward-one-body gravity (each particle pulled toward a
    Center point with G/Mass/OthMass/MinDistance controls). Maps to
    particles.Gravity."""
    return chain_op(_particles.Gravity, "GRAV", upstream,
                    name=name, **plug_kwargs)
