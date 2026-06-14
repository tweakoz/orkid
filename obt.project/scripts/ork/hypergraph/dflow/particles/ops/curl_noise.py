from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def curl_noise(upstream, *packs, name=None, **plug_kwargs):
    """Divergence-free procedural flow field. Particles swirl through a
    3D vector potential noise sampled per particle; the curl of that
    potential is the applied force, so the field has no sources/sinks
    (∇·F=0) and motion looks organic.

    Inputs: Strength, Frequency (noise spatial scale, smaller = larger
    swirls), Speed (time-evolution rate), Epsilon (finite-diff step).

    Maps to particles.CurlNoiseForce. Composes naturally with other forces
    — stack under Turbulence/Gravity/Vortex as desired."""
    return chain_op(_particles.CurlNoiseForce, "CURL", upstream,
                    name=name, packs=packs, **plug_kwargs)
