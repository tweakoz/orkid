################################################################################
# exprforce.py — HyperSyn DSL demo for the E2.5 ExprIR per-particle FORCE consumer (S8).
#
# P.ExprForce drives a per-particle acceleration authored as an ExprIR TREE over the honest
# per-particle symbols (PF.unit_age / vel_* / speed / ...). Here: a swirl about +Y
# (fx = -k*vel_z, fz = +k*vel_x) that eases in with age, plus a speed-proportional damping so
# the swirl reaches a steady radius. Evaluated per particle on the CPU by the ExprForce module
# -> the sim advance is fully deterministic. (The S9 color-ramp ExprIR consumer has its own
# demo, exprcolor.py.)
#
# Hosted by ork.particles.player.py:
#   ork.particles.player.py exprforce
################################################################################

from orkengine.core import CrcStringProxy, vec3, vec4
from orkengine.lev2 import particles, Texture

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow.particles.exprir_particles import PF          # S8 force context

tokens = CrcStringProxy()


class ExprForceSystem(ParticleSystem):
  """Elliptical emit -> ExprIR force (swirl) -> streak render, with an ExprIR-baked ramp."""

  def __init__(self):
    super().__init__()

    self.ptc_pool = P.PoolData(size=80000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=0.5,
                                       DispersionAngle=180,
                                       LifeSpan=3.0,
                                       Scalar=2,
                                       EmissionRate=20000,
                                       MinU=0, MaxU=1,
                                       MinV=0, MaxV=1,
                                       P1=vec3(0, 0, 0),
                                       P2=vec3(0, 0, 0))

    # S8: the ExprIR force. `ease` ramps the swirl in over the first third of life; the
    # tangential terms rotate velocity about +Y; the damping term pulls speed toward a steady
    # value. Strength is the A8-parametric multiplier (a plug).
    ease = PF.smoothstep(0.0, 0.33, PF.unit_age)
    self.force = P.ExprForce(self.emitter, name="EFRC",
                             strength=1.0,
                             fx=(PF.vel_z * -2.5 - PF.vel_x * 0.4) * ease,
                             fz=(PF.vel_x * 2.5 - PF.vel_z * 0.4) * ease,
                             fy=PF.smoothstep(0.2, 1.0, PF.unit_age) * -1.5)

    # ---- material ----

    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 0.5
    self.material.gradient.setColorStops({
      0.0: vec4(0, 0, 0, 0),
      0.3: vec4(0.9, 0.6, 1.0, 1),
      0.6: vec4(0.4, 0.2, 0.9, 0.8),
      1.0: vec4(0, 0, 0.2, 0),
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    # ---- renderer ----

    self.streaks = P.StreakRenderer(self.force, name="STRK",
                                    material=self.material,
                                    Length=0.20,
                                    Width=0.020)
    self.render(self.streaks)


__all__ = ["ExprForceSystem"]
