###############################################################################
# ork.dflow.particles — particles family DSL vocabulary.
#
# Usage (per SKILL.md):
#
#   from ork.hypergraph.dflow.particles import ParticleSystem
#   from ork.hypergraph.dflow import particles as P
#
#   class MyExplosion(ParticleSystem):
#     def __init__(self):
#       super().__init__()
#       self.pool = P.PoolData(size=4096, name="POOL")
#       emit = P.EllipticalEmitter(self.pool, LifeSpan=2.0, EmissionRate=1000)
#       grav = P.Gravity(emit, G=0.01, MinDistance=1)
#       self.streaks = P.StreakRenderer(grav, material=my_mat,
#                                       Length=0.15, Width=0.015)
#       self.render(self.streaks)
#
# Naming convention: user-facing op aliases are CamelCase because they read as
# class-instantiation calls (each invocation creates a new module in the graph).
# Internal module paths and function definitions stay snake_case
# (ops.elliptical_emitter etc.) following Python file/function conventions.
###############################################################################

from orkengine.lev2 import particles as _lev2_particles

from .base import ParticleSystem
from . import ops  # populates ops registry
from .._context_classes import register_python_class
# `resolve` imports ParticleSystem from us, so import it at the bottom of
# this module — re-exported below so callers can do
# `from ork.hypergraph.dflow.particles import resolve_dsl_file, load_dsl_class`.

# Register the Python module classes that materialize this family's DSL
# context variables. The C++ side registered the (DSL name, output plug,
# policy) tuples via static initializers in modules_global.cpp / modules_pool.cpp.
# Here we attach the Python class wrappers needed by the emitter to actually
# call graphdata.create() and findModuleByClass() in headless contexts.
register_python_class("time",         _lev2_particles.Globals)
register_python_class("dt",           _lev2_particles.Globals)
register_python_class("ptc.unit_age", _lev2_particles.Pool)
register_python_class("ptc.random",   _lev2_particles.Pool)

# DSL op aliases — CamelCase because each call constructs a new module
# (analogous to a class instantiation). `from ork.hypergraph.dflow import particles as P`
# then yields P.PoolData(...), P.EllipticalEmitter(...), etc.
PoolData            = ops.pool_data
EllipticalEmitter   = ops.elliptical_emitter
RingEmitter         = ops.ring_emitter
NozzleEmitter       = ops.nozzle_emitter
LineEmitter         = ops.line_emitter
Gravity             = ops.gravity
DirectionalForce    = ops.directional_force
ExprForce           = ops.expr_force
Turbulence          = ops.turbulence
CurlNoise           = ops.curl_noise
Vortex              = ops.vortex
Drag                = ops.drag
PolyDrag            = ops.poly_drag
EllipticalAttractor = ops.elliptical_attractor
SphAttractor        = ops.sph_attractor
PointAttractor      = ops.point_attractor
PlaneCollider       = ops.plane_collider
SphereCollider      = ops.sphere_collider
VdbCollider         = ops.vdb_collider
StreakRenderer      = ops.streak_renderer
SpriteRenderer      = ops.sprite_renderer
LightRenderer       = ops.light_renderer
VdbLevelSetRenderer = ops.vdb_level_set_renderer


from .resolve import (
    search_path,
    resolve_dsl_file,
    load_dsl_class,
    list_dsl_files,
)

# Freestyle particle fragment DSL — author the FreestyleParticleMaterial's
# fragment shader as Python expressions (ptex3d IR, trace-time is_streak);
# materialize_fragment() returns the reflected shader_path reference.
from .fragment import (
    FreestyleFragment,
    FragmentCtx,
    materialize_fragment,
)

__all__ = [
    "ParticleSystem",
    "PoolData",
    "EllipticalEmitter", "RingEmitter", "NozzleEmitter", "LineEmitter",
    "Gravity", "DirectionalForce", "ExprForce", "Turbulence", "CurlNoise", "Vortex", "Drag", "PolyDrag",
    "EllipticalAttractor", "SphAttractor", "PointAttractor",
    "PlaneCollider", "SphereCollider", "VdbCollider",
    "StreakRenderer", "SpriteRenderer", "LightRenderer", "VdbLevelSetRenderer",
    # DSL bare-name resolution (shared by ork.particles.player.py and ECS demos)
    "search_path", "resolve_dsl_file", "load_dsl_class", "list_dsl_files",
    # freestyle fragment shader DSL
    "FreestyleFragment", "FragmentCtx", "materialize_fragment",
]
