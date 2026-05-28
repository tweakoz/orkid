################################################################################
# elliptical_exposed.py — HyperSyn variant of elliptical.py that exposes
# runtime-mutable parameters via expose() / Expr.param().
#
# Identical visuals to elliptical.py by default (the param defaults reproduce
# the original constants), but the host can mutate `Turb` and `Rate` at
# runtime via a SET_PARAM ECS notify to vary the look without rebuilding.
#
# Hosted by ./ork.ecs/examples/python/particles/ptc3.py — keyboard presets
# swap the values to demo expose end-to-end.
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr

tokens = CrcStringProxy()


class EllipticalExposedSystem(ParticleSystem):
  """Elliptical attractor with two exposed scalar parameters:
       Turb  — multiplier on turbulence amplitude  (default 1.0)
       Rate  — multiplier on emission rate         (default 1.0)
  The defaults reproduce the look of elliptical.py exactly."""

  def __init__(self):
    super().__init__()

    # Runtime-mutable parameters. Host changes these via:
    #   controller.systemNotify(sys_ptc, tokens.SET_PARAM,
    #                           {tokens.name: "Turb", tokens.value: 0.5})
    self.expose("Turb", default=1.0)
    self.expose("Rate", default=1.0)

    # ---- graph ----

    self.ptc_pool = P.PoolData(size=50000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=1.0,
                                       DispersionAngle=180,
                                       LifeSpan=2.0,
                                       Scalar=3,
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

    # ---- material ----

    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 1
    self.material.gradient.setColorStops({
      0.0: vec4(1, 1, 1, 1),
      0.4: vec4(1, 0, 1, 1),
      0.5: vec4(.2, .4, 1, 1),
      1.0: vec4(0, 0, 0, 1),
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    # ---- renderer ----

    self.streaks = P.StreakRenderer(self.gravity, name="STRK",
                                    material=self.material,
                                    Length=0.15,
                                    Width=0.015)
    self.render(self.streaks)

    ############################################################################
    # Bindings — exercise the full lowerer surface:
    #
    #   cos   → rewritten to sin(x+π/2) by _lower._rewrite, pure chain
    #   max   → MaxModule (floor on turbulence so Turb=0 still emits a hint)
    #   min   → MinModule (ceiling on MaxV, prevents runaway when modulated)
    #   lerp  → LerpModule, with each input itself a chain or const — blends
    #           between a steady emission and a pulsating one based on Rate
    ############################################################################

    e = Expr

    # Phase-shifted via cos (was sin in elliptical.py). cos rewrites to a
    # bias(π/2) + sine chain — same single floatxf chain, zero new modules.
    self.emitter.MinV = 0.6 + e.cos(e.time) * 0.2

    # min: cap the modulated MaxV at 0.5 — would otherwise overshoot when the
    # underlying oscillator dips. Emits _DSL_Min_0; the chain feeds input A.
    self.emitter.MaxV = e.min(0.4 - e.sin(e.time) * 0.2, 0.5)

    # lerp: Rate controls a blend between a steady 1000 emissions/sec (Rate=0)
    # and a pulsating 0..20000 (Rate=1). T input is the exposed param; the B
    # input itself is a sin-driven chain. Emits _DSL_Lerp_0.
    self.emitter.EmissionRate = e.lerp(
        1000.0,
        20000.0 * (0.5 + e.sin(e.time) * 0.5),
        e.param.Rate)

    # max: floor on Turb so the trickle preset still produces a hint of
    # turbulence rather than going to dead-zero. Emits _DSL_Max_0; the result
    # then scales by 20 and broadcasts across all three axes through
    # _DSL_Vec3Combine_0.
    self.turbulence.Amount = e.vec3(e.max(e.param.Turb, 0.05) * 20.0)
