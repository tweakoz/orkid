################################################################################
# fireball.py — projectile fire trail (E2B): world-space fire + smoke that a
# moving emitter leaves BEHIND it (particles live in world space; only the
# emit position follows the host), with post-emission BUOYANCY — heat rises:
# a constant upward DirectionalForce + per-tick drag gives every puff a
# rising terminal drift after it detaches from the projectile.
#
# ONE chain, ONE draw (E2B-10x): every particle IS the fire that becomes its
# own smoke. FreestyleParticleMaterial composites PREMA (premultiplied:
# out.rgb adds, out.a occludes), so the gradient ramp morphs each sprite over
# its life from additive flame (rgb hot/HDR, a~0 — flames EMIT light, the
# PBR-honest model; HDR > 1 feeds bloom) into absorptive smoke (rgb~0, a>0 —
# smoke DARKENS what's behind it) and out. The exp2 flipbook is the animated
# cookie for BOTH phases; fire-to-smoke blending is per-particle continuous
# instead of two chains fighting over draw order.
#
# The emitter follows an entity by published name, or the RESERVED "@host"
# key = the entity hosting the ParticlesComponent — the per-instance binding
# that makes ONE shared graphdata work for every projectile of a pool.
#
# Hosted standalone:
#   ork.particle.viewer.py fireball                       # static emitter
# Hosted in a scene (the projectile pool):
#   fire = self.asset.ParticleSystem("fire_trail", dsl_file="fireball",
#                                    emitter_entity="@host")
#   self.projectile_pool("ball_spawner", trail=fire, ...)
################################################################################

from orkengine.core import vec3, dataflow, CrcStringProxy
from orkengine.lev2 import particles

from ork.hypergraph.colors import hsv

from ork.hypergraph.dflow.particles import ParticleSystem, FreestyleFragment, materialize_fragment
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr as E
from ork.hypergraph.ptex3d import P as F   # GLSL expression ops (fragment DSL)

tokens = CrcStringProxy()


class FireFrag(FreestyleFragment):
  """The fire-becomes-smoke PREMA fragment, authored in the DSL (replaces
  the stock ps_freestyle_ptc with the SAME math — the override proves the
  shader_path round-trip; edit freely from here). Cookie luminance gates the
  ramp's occlusion so wispy cookie texels never read as solid smoke."""

  def __init__(self, ctx):
    cookie   = ctx.cookie()                          # flipbook cell sample
    cookie_a = F.length(cookie.xyz) * 0.5773         # 1/sqrt(3) — sheet luminance
    if ctx.is_heat:
      # AUX heat channel (item D): heat rides the AGED plume (>=~50% life),
      # which has drifted clear of the projectile — the young flame core
      # sits ON the ball and the scene-depth clip there reads badly (hard
      # edges crawling on the sphere). Unity/Unreal solve the general case
      # with soft-particle DEPTH FADE (linear-depth-difference fade); the
      # age gate is the cheap fix and reads physically: shimmer is the
      # rising hot air, not the flame surface. Cookie luminance shapes the
      # blob; accumulates additively into aux_heat (.r).
      h = cookie_a * F.smoothstep(0.65, 0.70, ctx.unit_age) \
                   * F.smoothstep(1.00, 0.75, ctx.unit_age)
      self.output(F.vec4(h, 0.0, 0.0, h))
      return
    ramp = ctx.ramp()                                # life ramp (rgb adds, w occludes)
    rgb = ramp.xyz * cookie.xyz * ctx.color_factor * ctx.modcolor.xyz * 0.5
    a   = F.saturate(ramp.w * cookie_a * ctx.alpha_factor * ctx.modcolor.w)
    if ctx.is_streak:                                # trace-time divergence:
      a = a * F.smoothstep(1.0, 0.55, ctx.uv.y)      # streaks thin toward the tail
    self.output(F.vec4(rgb.x, rgb.y, rgb.z, a))


class FireTrail(ParticleSystem):

  def __init__(self, *,
               emitter_entity="",
               intensity=2.5,
               fire_size=0.55,
               fire_rate=140.0,
               buoyancy=0.1,
               smoke=True,
               fire_delay=0.0,
               pool_size=1500,
               light_smoothing=0.20,
               light_body_bias=0.5,
               light_tint=None):
    """Fire-becomes-smoke trail (one PREMA chain).

    Kwargs:
      emitter_entity — published entity name the emit position follows.
                    "@host" (in an ECS ParticlesComponent) = the hosting
                    entity itself — each spawned projectile's own trail.
                    Empty → static world-space emitter (viewer standalone).
      intensity   — HDR multiplier on the ramp color (additive phase; >1
                    blooms). Smoke keeps a faint luminance from it (daylight
                    scatter).
      fire_size   — peak flame sprite size (world units); smoke swells to
                    ~2.8x as it cools and thins.
      fire_rate   — sprites emitted per second.
      buoyancy    — upward accel (m/s²) after emission (heat rises; drag
                    turns it into a rising terminal drift).
      smoke       — True: each flame ages INTO smoke (long life, absorptive
                    tail). False: short fire-only pops.
      fire_delay  — PER-EMITTER delayed start (seconds of slot-local time
                    before emission). For a delay across the WHOLE system
                    use the hosting ParticlesComponent's start_delay.
      pool_size   — pool capacity (rate * lifespan must fit).
      light_smoothing — emission-light flicker control: EMA time constant
                    in seconds (0 = raw per-tick flicker; ~0.2 = breathing
                    glow). Authored here, obeyed by the engine.
      light_body_bias — per-particle light weighting exponent (lum^p):
                    1.0 lets the bright white ignition flash dominate
                    (whiter, twitchier); 0.5 biases toward the orange
                    flame body (default).
      light_tint  — direct multiplier on the light color (hsv()/vec3);
                    default = a warm orange push.
    """
    super().__init__()

    if emitter_entity:
      emit_offset = E.entity(emitter_entity).transformPoint(vec3(0, 0, 0))
    else:
      emit_offset = vec3(0, 1, 0)

    # fire phase = first quarter of life; the rest is the smoke tail
    lifespan = 0.9 if smoke else 0.55

    self.pool = P.PoolData(size=pool_size, name="POOL")
    self.emit = P.RingEmitter(self.pool, name="EMIT",
                              StartDelay=fire_delay,
                              LifeSpan=lifespan,
                              EmissionRate=fire_rate,
                              EmissionVelocity=0.9,
                              EmissionRadius=0.06,
                              DispersionAngle=45.0,
                              Direction=vec3(0, 1, 0),
                              Tangent=vec3(1, 0, 0),
                              Offset=emit_offset)
    self.rise = P.DirectionalForce(self.emit, name="BUOY",
                                   Direction=vec3(0, 1, 0),
                                   Magnitude=buoyancy)
    self.drag = P.Drag(self.rise, name="DRAG", drag=0.996)
    self.turb = P.Turbulence(self.drag, name="TURB",
                             Amount=vec3(2.0, 1.0, 2.0))
    self.curl = P.CurlNoise(self.turb, name="CURL",
                            Strength=1.0,
                            Frequency=1.35,
                            Speed=0.05)

    ############################################################
    # the material: exp2 5x5 flipbook cookie x life ramp, PREMA.
    # Ramp w is the OCCLUSION channel: ~0 = pure additive flame,
    # >0 = absorptive smoke. texture_asset = the SERIALIZABLE form
    # (a live .texture drops on the embedded-graph round-trip —
    # the player would render BLANK sprites).
    ############################################################
    self.material = particles.FreestyleParticleMaterial.createShared()
    self.material.shader_path = materialize_fragment(FireFrag)
    self.material.blending = tokens.PREMA
    self.material.depthtest = tokens.LEQUALS
    self.material.texture_asset = "lev2://textures/exp2"
    self.material.gridDim = 5
    self.material.colorIntensity = intensity
    self.material.alphaIntensity = 1.0
    # emission point light shaping (item E) — flicker + color are AUTHORED
    # here (reflected material knobs; both hosts obey them)
    self.material.emission_smoothing = float(light_smoothing)
    self.material.emission_lum_power = float(light_body_bias)
    if light_tint is None:
      light_tint = hsv(28, 0.65, 1.0)   # warm orange push
    self.material.emission_tint = light_tint
    # hsv(h, s, v, a) -> vec4 stop. a is the PREMA OCCLUSION channel
    # (0 = pure additive light, >0 = absorptive smoke); h/s/v carry the
    # heat read: hot bright orange -> dim red embers -> desaturated grey.
    if smoke:
      self.material.gradient.setColorStops({
        0.00: hsv(26, 0.80, 1.00, 0.00),  # ignition — additive white-orange
        0.10: hsv(21, 0.84, 0.20, 0.02),  # peak flame
        0.25: hsv(17, 0.84, 0.01, 0.08),  # embers — smoke condensing
        0.40: hsv(0,  0.00, 0.01, 0.25),  # grey smoke, max occlusion
        0.80: hsv(0,  0.00, 0.01, 0.25),  # thinning
        1.00: hsv(0,  0.00, 0.00, 0.00),  # gone
      })
    else:
      self.material.gradient.setColorStops({
        0.00: hsv(26, 0.80, 1.00, 0.00),
        0.20: hsv(21, 0.84, 1.00, 0.00),
        0.65: hsv(16, 0.88, 0.50, 0.00),
        1.00: hsv(0,  0.00, 0.00, 0.00),
      })

    # swell through the flame, keep growing as cooling smoke (gas expands)
    self._size = dataflow.floatxf.multicurve().multicurve
    self._size.splitSegment(0)
    self._size.splitSegment(0)
    self._size.setPoint(0, 0.00, fire_size * 0.25)
    if smoke:
      self._size.setPoint(1, 0.12, fire_size)
      self._size.setPoint(2, 0.30, fire_size * 1.4)
      self._size.setPoint(3, 1.00, fire_size * 2.8)
    else:
      self._size.setPoint(1, 0.20, fire_size)
      self._size.setPoint(2, 0.65, fire_size * 0.7)
      self._size.setPoint(3, 1.00, 0.0)

    self.sprites = P.SpriteRenderer(self.curl, name="SPRI",
                                    material=self.material,
                                    Size=E.curve(E.ptc.unit_age, self._size))
    # PREMA's absorptive term is order-dependent — sort back-to-front
    self.sprites.module.depth_sort = True
    self.render(self.sprites)


__all__ = ["FireTrail"]
