from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def plane_collider(upstream, *packs, name=None, **plug_kwargs):
    """Half-space collider — bounces / dampens particles against an infinite
    plane defined by Center+Normal. Maps to particles.PlaneCollider.

    Plugs: Center (vec3), Normal (vec3), Restitution (float), Friction (float).
    Defaults: floor at y=0 with +Y normal, Restitution=0.5, Friction=0.0.
    """
    return chain_op(_particles.PlaneCollider, "PCOL", upstream,
                    name=name, packs=packs, **plug_kwargs)
