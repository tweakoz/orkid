################################################################################
# curl.py — HyperSyn DSL demo for CurlNoiseForce.
#
# Same skeleton as elliptical.py but the only force is CurlNoise. Particles
# emit from a wide ellipsoidal nozzle and immediately get swept into the
# divergence-free flow field — the result is organic swirling motion with
# no clumping or sources (the curl-of-noise construction guarantees
# ∇·F = 0, so particles never converge to a point or fly apart from one).
#
# Try tweaking the bindings:
#   Strength  — magnitude of the swept force. Higher = particles get
#               yanked harder around the swirls.
#   Frequency — spatial scale of the flow. Smaller = bigger, lazier
#               swirls. Larger = tight rapid eddies.
#   Speed     — how fast the field itself evolves over time. 0 = static
#               (particles still move through it but the field is frozen).
#
# Hosted by ork.particles.player.py:
#   ork.particles.player.py curl
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr

tokens = CrcStringProxy()


class CurlSystem(ParticleSystem):
  """Elliptical emit → CurlNoise force → streak render. The most
  bare-bones way to see what curl noise looks like in isolation."""

  def __init__(self):
    super().__init__()

    self.ptc_pool = P.PoolData(size=80000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=0.5,
                                       DispersionAngle=180,
                                       LifeSpan=2.0,
                                       Scalar=2,
                                       EmissionRate=20000,
                                       MinU=0, MaxU=1,
                                       MinV=0, MaxV=1,
                                       P1=vec3(0, 0, 0),
                                       P2=vec3(0, 0, 0))

    # The star of the show. Defaults (Strength=1, Frequency=0.5, Speed=0.1,
    # Epsilon=0.05) give nice slow swirls; we bump Strength here so the
    # particles really get carried by the field.
    self.curl = P.CurlNoise(self.emitter, name="CURL",
                            Strength=3.0,
                            Frequency=0.35,
                            Speed=0.75)

    # ---- material ----

    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 0.5
    self.material.gradient.setColorStops({
      0.0: vec4(0, 0, 0, 0),
      0.3: vec4(0.6, 0.9, 1.0, 1),
      0.6: vec4(0.2, 0.4, 0.9, 0.8),
      1.0: vec4(0, 0, 0.2, 0),
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    # ---- renderer ----

    self.streaks = P.StreakRenderer(self.curl, name="STRK",
                                    material=self.material,
                                    Length=0.20,
                                    Width=0.020)
    self.render(self.streaks)

    # Slowly modulate the curl Strength so the visible energy ebbs and
    # flows over time — pure time-driven chain, no per-particle eval.
    self.curl.Strength = 2.5 + Expr.sin(Expr.time * 0.2) * 1.5
