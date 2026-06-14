from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def directional_force(upstream, *packs, name=None, **plug_kwargs):
    """Constant per-particle acceleration in a uniform direction.

    Δv = Direction.normalized() * Magnitude * dt — applied to every
    particle each tick regardless of position. Use for wind, thrust,
    or any constant pull where the existing point-attractor Gravity
    isn't a fit. Direction defaults to (0,-1,0); Magnitude defaults to
    9.81 (no-input case mimics canonical down-gravity).

    Plugs: Direction (vec3), Magnitude (float).
    """
    return chain_op(_particles.DirectionalForce, "DFRC", upstream,
                    name=name, packs=packs, **plug_kwargs)
