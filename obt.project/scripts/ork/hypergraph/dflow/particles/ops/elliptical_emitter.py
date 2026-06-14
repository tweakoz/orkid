###############################################################################
# ork.dflow.particles.ops.elliptical_emitter — EllipticalEmitter DSL op.
#
# Maps to C++ ptc::EllipticalEmitterData / Python `particles.EllipticalEmitter`.
# Authoring:
#
#   emit = P.elliptical_emitter(pool, LifeSpan=2.0, EmissionRate=10000, ...)
###############################################################################

from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def elliptical_emitter(upstream, *packs, name=None, **plug_kwargs):
    return chain_op(_particles.EllipticalEmitter, "EMITN", upstream,
                    name=name, packs=packs, **plug_kwargs)
