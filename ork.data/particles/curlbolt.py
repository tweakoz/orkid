################################################################################
# curlbolt.py — quantized CurlNoise demo, aiming for an electrical-discharge
# look.
#
# Same skeleton as curl.py but with the quantization knobs cranked. The
# vector potential ψ is held piecewise constant (both on a cell grid via
# CellSize AND on noise level sets via Levels) → the curl is ~zero almost
# everywhere with sharp spikes at the boundaries. Particles drift slowly,
# then get LAUNCHED across a boundary, then drift again. Reads as bolts /
# arcs / discharge sparks rather than smooth swirling.
#
# Tuning notes:
#   CellSize  — bigger = larger uniform zones (rarer but stronger spikes).
#               Try 0.4..2.0.
#   Levels    — fewer = chunkier ψ steps (sparser, stronger spikes).
#               Try 2..6 for crisp boundaries; 0 = no level quantization.
#   Strength  — has to be high to make the spike-launches visible. The
#               quiescent in-cell drift is near-zero, so most of the
#               apparent motion comes from boundary kicks.
#   Epsilon   — keep small relative to CellSize so the central differences
#               actually catch the boundary discontinuity.
#
# Hosted by ork.particles.player.py:
#   ork.particles.player.py curlbolt
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P
from ork.dflow import Expr

tokens = CrcStringProxy()


class CurlBoltSystem(ParticleSystem):

  def __init__(self):
    super().__init__()

    self.ptc_pool = P.PoolData(size=80000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=0.2,
                                       DispersionAngle=180,
                                       LifeSpan=1.0,
                                       Scalar=2,
                                       EmissionRate=20000,
                                       MinU=0, MaxU=1,
                                       MinV=0, MaxV=1,
                                       P1=vec3(0, 0, 0),
                                       P2=vec3(0, 0, 0))

    # Quantized CurlNoise — both cell-grid and level quantization are on.
    self.curl = P.CurlNoise(self.emitter, name="CURL",
                            Strength=1.2,    # high, since spikes are rare
                            Frequency=1.0,
                            Speed=0.1,
                            CellSize=0.18,     # axis-aligned cell grid
                            Levels=4)         # 4 discrete ψ levels per axis

    # ---- material — high-contrast cool palette evokes electricity ----

    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 1.8
    self.material.gradient.setColorStops({
      0.0: vec4(0.0, 0.0, 0.0, 0),     # hot white core
      0.2: vec4(0.7, 0.9, 1.0, 1),     # pale electric blue
      0.5: vec4(0.2, 0.5, 1.0, 0.9),   # deep blue arc
      0.8: vec4(0.4, 0.1, 0.9, 0.5),   # violet fade
      1.0: vec4(0.0, 0.0, 0.1, 0),
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    # ---- renderer — short, narrow streaks read as arc segments ----

    self.streaks = P.StreakRenderer(self.curl, name="STRK",
                                    material=self.material,
                                    Length=0.08,
                                    Width=0.012)
    self.render(self.streaks)
