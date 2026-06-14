from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def sprite_renderer(upstream, *packs, name=None, material=None, **plug_kwargs):
    """Per-particle camera-facing sprite renderer. Maps to particles.SpriteRenderer."""
    return chain_op(_particles.SpriteRenderer, "SPRI", upstream,
                    name=name, material=material, packs=packs, **plug_kwargs)
