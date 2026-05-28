################################################################################
# polydrag.py — dendrite-style quantized CurlNoise with PolyDrag downstream.
#
# Same emit + CurlNoise(Levels=3) setup as dendrite.py, but adds a heavy
# nonlinear PolyDrag in the chain after the curl force. The result:
# particles still get LAUNCHED at the ψ-level boundaries (the sharp spikes
# of the quantized curl), but the drag bites hardest while they're moving
# fast — so each bolt has a crisp ignition, then a quick exponential-like
# decay. Reads as discrete arcs rather than long sustained discharges.
#
# Coefficient choices for visible nonlinearity:
#   Quadratic — air-resistance-like, decel ∝ |v|². Dominates at high speed.
#   Cubic     — kicks in only for the fastest particles. Makes the
#               speed cap feel "hard."
#   Constant + Linear left at 0 so slow drift in between bolts isn't
#   killed — only the launched particles take the hit.
#
# Hosted by ork.particles.player.py:
#   ork.particles.player.py polydrag
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P
from ork.dflow import Expr

tokens = CrcStringProxy()


class PolyDragSystem(ParticleSystem):

  def __init__(self):
    super().__init__()

    self.ptc_pool = P.PoolData(size=40000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=1.1,
                                       DispersionAngle=0,
                                       LifeSpan=4.0,
                                       Scalar=3,
                                       EmissionRate=10000,
                                       MinU=0, MaxU=1,
                                       MinV=0.3, MaxV=0.5,
                                       P1=vec3(0, 0, 0),
                                       P2=vec3(0, 0, 0))

    # Quantized CurlNoise — same as dendrite.py. Spikes at ψ-level
    # boundaries launch particles at high velocity.
    self.curl = P.CurlNoise(self.emitter, name="CURL",
                            Strength=5.0,
                            Frequency=0.5,
                            Speed=0.18,
                            CellSize=0,
                            Levels=3)

    # Heavy nonlinear drag — bites hardest at the peak speed each
    # particle achieves after a curl spike. Particles whose post-launch
    # velocity is small barely feel it; the fast ones get braked quickly.
    self.drag = P.PolyDrag(self.curl, name="DRAG",
                           Constant=25.0,
                           Linear=0.0,
                           Quadratic=0.8,    # main brake
                           Cubic=0.4)        # snaps top speed flat

    self.turbulence = P.Turbulence(self.drag, name="TURB", Amount=vec3(10.0))

    # Same warm violet/magenta palette as dendrite.py
    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 1.5
    self.material.gradient.setColorStops({
      0.0: vec4(1.0, 1.0, 1.0, 1),
      0.2: vec4(1.0, 0.7, 1.0, 1),
      0.5: vec4(0.8, 0.3, 1.0, 0.9),
      0.8: vec4(0.3, 0.0, 0.5, 0.5),
      1.0: vec4(0.05, 0.0, 0.1, 0),
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    self.streaks = P.StreakRenderer(self.turbulence, name="STRK",
                                    material=self.material,
                                    Length=0.10,
                                    Width=0.010)
    self.render(self.streaks)

    # Optional: pulse the quadratic drag with time so the brake comes
    # and goes — particles fan out more during low-drag phases.
    # self.drag.Quadratic = 0.4 + Expr.sin(Expr.time * 0.3) * 0.4
