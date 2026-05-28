from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def sphere_collider(upstream, *, name=None, **plug_kwargs):
    """Solid-sphere collider — bounces / dampens particles off the OUTSIDE of
    a sphere defined by Center + Radius. Maps to particles.SphereCollider.

    Plugs: Center (vec3), Radius (float), Restitution (float), Friction (float).
    Defaults: 1m sphere at origin, Restitution=0.5, Friction=0.0.
    """
    return chain_op(_particles.SphereCollider, "SCOL", upstream,
                    name=name, **plug_kwargs)
