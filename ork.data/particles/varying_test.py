################################################################################
# varying_test.py — variant of elliptical.py that drives streak
# width PER PARTICLE via sin(unit_age).
#
# Same graph as elliptical.py, plus one extra bind:
#
#   self.streaks.bind("Width", (Expr.sin(Expr.ptc.unit_age * 6.28) * 0.5 + 0.5) * 0.05)
#
# Lowering:
#   Source : Pool.UnitAge     (per-particle, EPR_VARYING1)
#   Chain  : scale(6.28) → sine → scale(0.5) → bias(0.5) → scale(0.05)
#   Target : streaks.Width    (read per particle by the renderer's compute loop)
#
# Host with:
#   ork.particles.player.py ork.lev2/pyext/tests/hypersyn/particles/elliptical_per_particle.py
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P
from ork.dflow import Expr as E

tokens = CrcStringProxy()


class VaryingUnitAge(ParticleSystem):
  """Same as EllipticalSystem, with per-particle width modulation via UnitAge.
  Each particle's width pulses through sin over its own lifetime (so younger
  particles and older particles in the same frame have different widths)."""

  def __init__(self):
    super().__init__()

    self.ptc_pool = P.PoolData(size=80000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=1.0,
                                       DispersionAngle=179,
                                       LifeSpan=3.0,
                                       Scalar=3,
                                       EmissionRate=10000,
                                       MinU=0,
                                       MaxU=1,
                                       P1=vec3(0, 0, 0),
                                       P2=vec3(0, 0, 0))

    self.turbulence = P.Turbulence(self.emitter, name="TURB")

    self.vortex = P.Vortex(self.turbulence, name="VORT",
                           VortexStrength=-5,
                           OutwardStrength=-1,
                           Falloff=0.001)

    self.elliptical = P.EllipticalAttractor(self.vortex, name="SPHR",
                                            Scalar=1,
                                            Power=0.01,
                                            Inertia=111,
                                            Dampening=0.999,
                                            P1=vec3(0, 0, 0),
                                            P2=vec3(0, 0, 0))

    self.gravity = P.Gravity(self.elliptical, name="GRAV",
                             Center=vec3(0, 1, 0),
                             G=0,
                             Mass=1,
                             OthMass=1,
                             MinDistance=10)

    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 1
    self.material.gradient.setColorStops({
      0.0: vec4(0, 0, 0, 0),
      0.1: vec4(1, 1, 1, 1),
      0.4: vec4(1, 0, 1, 1),
      0.5: vec4(.2, .4, 1, 1),
      1.0: vec4(0, 0, 0, 1),
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    self.streaks = P.StreakRenderer(self.gravity, name="STRK",
                                    material=self.material,
                                    Length=0.15,
                                    Width=0.015)
    self.render(self.streaks)

    ############################################################################
    # Bindings:
    #   - Same time-driven MinV/MaxV/Amount as elliptical.py
    #   - NEW: Width pulses per particle via sin(unit_age). unit_age is 0→1
    #     across each particle's lifespan; multiplying by 2π gives one full
    #     sine cycle per lifetime, then we remap (sin*0.5+0.5) to [0,1] and
    #     scale to [0, 0.05] — width pulses smoothly from 0 → 0.05 → 0 across
    #     the particle's life.
    ############################################################################

    self.emitter.MinV      = 0.6 + E.sin(E.time) * 0.2
    self.emitter.MaxV      = 0.4 - E.sin(E.time) * 0.2
    self.turbulence.Amount = E.vec3(E.sin(E.time * 0.25) * 20)

    # The new per-particle binding. unit_age is per-particle (EPR_VARYING1),
    # so the chain runs per particle inside the renderer's compute loop and
    # each particle gets its own width based on its own age.
    self.streaks.Width = (E.sin(E.ptc.unit_age * 6.28 * 2.0) * 0.5 + 0.5) * 0.05
