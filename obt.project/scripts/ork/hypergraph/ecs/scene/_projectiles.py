###############################################################################
# Scene shootable projectiles
#
# Split out of scene/__init__.py for readability. Mixed into Scene via multiple
# inheritance (class Scene(TerrainMixin, WalkerMixin, ProjectilesMixin) in __init__.py), so the public API is
# unchanged: self.projectile_pool(...) still works.  Methods reference self.* (entity /
# declare_component / SG / asset / ...) provided by the core Scene + sibling mixins.
###############################################################################

from orkengine.core import vec3

class ProjectilesMixin:
  """Scene shootable projectiles"""

  def projectile_pool(self, name="ball_spawner", *,
                      model="data://tests/pbr_calib_lopoly.glb",
                      radius=0.25, mass=3.0,
                      friction=0.9, restitution=0.9, angular_damping=0.05,
                      max_count=256, lifetime=8.0,
                      node_name=None, layers=None, trail=None,
                      trail_delay=0.0, fire=False):
    """Shootable-projectile pool (the walker()-style library call): ONE
    system-level instanced node (capacity max_count) + a physics+visual
    archetype paired BY NAME + a dynamic-only spawner with lifetime
    recycling. An input script shoots through the spawner handle::

        spawner = simulation.findSpawner("ball_spawner")     # once, at link
        spawner.spawn(pos=..., vel=dir*speed, avel=spin, scale=...)

    The SHOOT policy (key, speed, offset, spin) belongs to the input
    script; this call owns the DATA (shape, mass, contact response,
    capacity, recycling). NOTE bullet combines contact friction and
    restitution multiplicatively — static colliders (terrain_collider /
    scatter_collider) should carry ~1.0 for the projectile's own values
    to read as its effective response. Returns the _SpawnerDecl.

    trail — optional ParticleSystem asset wrapper (e.g. a fireball.py
    FireTrail declared with emitter_entity="@host"): every spawned
    projectile gets its own ParticlesComponent whose emitter follows
    the projectile's live transform; particles live in WORLD space, so
    the trail stays behind the flight path. Despawn removes the trail
    with the entity.

    trail_delay — seconds before the trail IGNITES after the projectile
    spawns (the component-level StartDelay), so the fire doesn't start
    right in the shooter's face. At 30 m/s, 0.1s ≈ 3m of clearance.

    fire — convenience: build the standard E2B fireball trail (the
    ren_scatter recipe — world-space fire+smoke buoyant @host with an
    emission point light) and wire it as `trail` with a sensible
    trail_delay. So `projectile_pool(fire=True)` is shootable fireballs in
    one call. Ignored if an explicit `trail` is passed."""
    from orkengine import ecs as _ecs
    if fire and trail is None:
      # the standard fireball trail (matches ren_scatter): one shared @host graphdata
      # serves every projectile in the pool; the emitter binds each ball's live transform.
      trail = self.asset.ParticleSystem(
          name + "_fire",
          dsl_file          = "fireball",
          emitter_entity    = "@host",
          intensity         = 2.0,
          fire_size         = 0.55,
          buoyancy          = 0.25,
          smoke             = True,
          emitter_intensity = 15.0,
          emitter_radius    = 20.0)
      if trail_delay == 0.0:
        trail_delay = 0.20   # ignite ~3.6m downrange at 30 m/s — not in the shooter's face
    self._ensure_system("BulletSystem", linGravity=vec3(0.0, -12.25, 0.0))  # matches the walker default (owner aug07)
    node_name = node_name or (name + "_node")
    self.SG.instanced_node(node_name, model=model, capacity=max_count,
                           layers=layers)
    shape        = _ecs.BulletShapeSphereData()
    shape.radius = float(radius)
    arch = self.archetype(name + "_arch")
    self.component(
        arch,
        "BulletObjectComponent",
        shape              = shape,
        mass               = float(mass),
        friction           = float(friction),
        restitution        = float(restitution),
        angularDamping     = float(angular_damping),
        instance_node_name = node_name)
    self.component(
        arch,
        "SceneGraphComponent",
        instance_node_name = node_name)
    if trail is not None:
      # particles REQUIRE the declared hosting system (components only link
      # to declared systems; ParticlesComponent asserts at link otherwise —
      # dynamic-spawn archetypes never trip the composition auto-register).
      self._ensure_system("ParticlesGlobalSystem")
      # layername must be EXPLICIT here — THE LAYER TRAP: the C++ fallback
      # for an empty layername is the SG's "sg_default", which the forward
      # compositor's fixed role set never renders (particles compute but
      # silently never draw — the exact D.5 hypermesh lesson). The DSL's
      # declare_component() special-case defaults it, but this is the
      # PRIMITIVE component path, which does not. std_transparent draws
      # AFTER std_forward — the trail composites OVER its own projectile.
      trail_layers = "std_transparent"
      if getattr(self.SG, "_aux_channels", None):
        # item D: the trail also renders into every declared aux channel
        # (e.g. aux_heat) — materials without that technique pair skip.
        trail_layers += "".join(",aux_" + c for c in self.SG._aux_channels)
      self.component(
          arch,
          "ParticlesComponent",
          drawabledata = trail,
          layername    = trail_layers,
          pool_size    = 1,
          duration     = 0.0,
          start_delay  = float(trail_delay))
    return self.spawner(name, arch, autospawn=False, lifetime=lifetime)
