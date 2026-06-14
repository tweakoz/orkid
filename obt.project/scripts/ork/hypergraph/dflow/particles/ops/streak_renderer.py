###############################################################################
# ork.dflow.particles.ops.streak_renderer — StreakRenderer DSL op.
#
# Renderer ops are chain terminuses. They have the same structure as other
# chain ops (pool input, pool output) but additionally accept a `material=`
# kwarg that sets the renderer's non-plug .material attribute.
#
# Authoring:
#
#   streaks = P.streak_renderer(upstream, material=mat, Length=0.15, Width=0.015)
#   self.render(streaks)
###############################################################################

from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def streak_renderer(upstream, *packs, name=None, material=None, **plug_kwargs):
    """Streak (motion-blur trail) renderer. Maps to particles.StreakRenderer."""
    return chain_op(_particles.StreakRenderer, "STRK", upstream,
                    name=name, material=material, packs=packs, **plug_kwargs)
