################################################################################
# exprcolor.py — HyperSyn DSL demo for the E2.5 ExprIR renderer COLOR-ramp consumer (S9).
#
# The material gradient's 256-entry ramp LUT is BAKED from a color expression of unit_age
# (context "particles.color"): bake_gradient evaluates the r/g/b/a channel expressions across
# unit_age in [0,1] and fills the gradient's color stops. v1 evaluates at BAKE time (the ramp
# is a 1-D function of unit_age only). Here: a hot core (bright) -> cool tail authored as
# smooth channel curves, with a smooth alpha fade-in/out.
#
# Same expression -> byte-identical LUT (determinism); a different expression -> a different
# ramp (see test_gradient_expr.py). Hosted by ork.particles.player.py:
#   ork.particles.player.py exprcolor
################################################################################

from orkengine.core import CrcStringProxy, vec3
from orkengine.lev2 import particles, Texture

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow.particles.gradient_expr import PC, bake_gradient   # S9 color context

tokens = CrcStringProxy()


class ExprColorSystem(ParticleSystem):
  """Elliptical emit -> curl -> streak render, ramp BAKED from a color ExprIR of unit_age."""

  def __init__(self):
    super().__init__()

    self.ptc_pool = P.PoolData(size=80000, name="POOL")
    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=0.5,
                                       DispersionAngle=180,
                                       LifeSpan=3.0,
                                       Scalar=2,
                                       EmissionRate=20000,
                                       MinU=0, MaxU=1, MinV=0, MaxV=1,
                                       P1=vec3(0, 0, 0), P2=vec3(0, 0, 0))
    self.curl = P.CurlNoise(self.emitter, name="CURL", Strength=2.0, Frequency=0.35, Speed=0.5)

    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 0.5

    # S9: the ramp is a color EXPRESSION of unit_age, baked to the 256-entry LUT.
    #   hot->cool: r starts high and fades; g a mid-life pulse; b rises late; a fades in/out.
    bake_gradient(
        self.material.gradient,
        r=1.0 - PC.smoothstep(0.1, 0.7, PC.unit_age),
        g=PC.smoothstep(0.0, 0.4, PC.unit_age) * (1.0 - PC.smoothstep(0.5, 1.0, PC.unit_age)),
        b=PC.smoothstep(0.4, 1.0, PC.unit_age),
        a=PC.smoothstep(0.0, 0.15, PC.unit_age) * (1.0 - PC.smoothstep(0.7, 1.0, PC.unit_age)))

    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    self.streaks = P.StreakRenderer(self.curl, name="STRK", material=self.material,
                                    Length=0.20, Width=0.020)
    self.render(self.streaks)


__all__ = ["ExprColorSystem"]
