from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def light_renderer(upstream, *, name=None, material=None, **plug_kwargs):
    """Per-particle dynamic point-light renderer. Maps to particles.LightRenderer."""
    return chain_op(_particles.LightRenderer, "LITE", upstream,
                    name=name, material=material, **plug_kwargs)
