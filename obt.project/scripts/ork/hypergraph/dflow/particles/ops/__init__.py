###############################################################################
# ork.dflow.particles.ops — particles DSL op registry.
#
# Each .py module in this directory defines one DSL op. Public callables are
# re-exported here so the family's __init__.py can alias them as P.<op_name>.
#
# Adding a new op:
#   1. Create ops/<name>.py with `def <name>(upstream, *, name=None, **kw):`
#      that delegates to ._chain.chain_op (for chain ops) or builds the
#      module manually (for source ops like pool_data).
#   2. Re-export it here: `from .<name> import <name>`
#   3. Add to ../__init__.py: `<name> = ops.<name>`
###############################################################################

from .pool_data import pool_data

# emitters
from .elliptical_emitter import elliptical_emitter
from .ring_emitter import ring_emitter
from .nozzle_emitter import nozzle_emitter
from .line_emitter import line_emitter

# forces / attractors
from .gravity import gravity
from .directional_force import directional_force
from .expr_force import expr_force
from .turbulence import turbulence
from .curl_noise import curl_noise
from .vortex import vortex
from .drag import drag
from .poly_drag import poly_drag
from .elliptical_attractor import elliptical_attractor
from .sph_attractor import sph_attractor
from .point_attractor import point_attractor

# colliders
from .plane_collider import plane_collider
from .sphere_collider import sphere_collider
from .vdb_collider import vdb_collider

# renderers (chain terminuses; accept material= kwarg)
from .streak_renderer import streak_renderer
from .sprite_renderer import sprite_renderer
from .light_renderer import light_renderer
from .vdb_level_set_renderer import vdb_level_set_renderer


__all__ = [
    # sources
    "pool_data",
    # emitters
    "elliptical_emitter", "ring_emitter", "nozzle_emitter", "line_emitter",
    # forces / attractors
    "gravity", "directional_force", "expr_force",
    "turbulence", "curl_noise", "vortex", "drag", "poly_drag",
    "elliptical_attractor", "sph_attractor", "point_attractor",
    # colliders
    "plane_collider", "sphere_collider", "vdb_collider",
    # renderers
    "streak_renderer", "sprite_renderer", "light_renderer", "vdb_level_set_renderer",
]
