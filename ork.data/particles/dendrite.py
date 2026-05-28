################################################################################
# dendrite.py — CurlNoise with ONLY potential quantization (Levels) active.
#
# ψ is snapped to N discrete levels (no spatial cell grid). The resulting
# boundaries between constant-ψ regions are organic, fractal-shaped
# iso-curves of the noise itself — so the spike-launches happen along
# branching, dendritic paths instead of along axis-aligned cell faces.
# Reads as neural-network arbors, root systems, lightning forks.
#
# Distinguishing features vs. circuit.py:
#   - Levels > 0, CellSize = 0 → boundaries are ORGANIC iso-curves of the
#     noise. Branching feels arborescent / biological / Jacob's-ladder.
#   - circuit.py is the inverse: CellSize>0, Levels=0 → AXIS-ALIGNED
#     boundaries → orthogonal / stamped / circuit-trace look.
#
# Hosted by ork.particles.player.py:
#   ork.particles.player.py dendrite
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P
from ork.dflow import Expr

tokens = CrcStringProxy()


class DendriteSystem(ParticleSystem):

  def __init__(self):
    super().__init__()

    self.ptc_pool = P.PoolData(size=80000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=0.1,
                                       DispersionAngle=180,
                                       LifeSpan=4.0,
                                       Scalar=3,
                                       EmissionRate=10000,
                                       MinU=0, MaxU=1,
                                       MinV=0.3, MaxV=0.5,
                                       P1=vec3(0, 0, 0),
                                       P2=vec3(0, 0, 0))

    # Potential quantization ONLY — CellSize stays at 0 (no spatial grid).
    # Lower Levels = chunkier ψ steps = sparser, stronger spike branches.
    self.curl = P.CurlNoise(self.emitter, name="CURL",
                            Strength=0.5,
                            Frequency=0.5,
                            Speed=0.18,
                            CellSize=0,
                            Levels=3)         # 3 levels per axis = sparse arbors

    # Warm violet/magenta palette — neural / Jacob's-ladder association
    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 1.5
    self.material.gradient.setColorStops({
      0.0: vec4(1.0, 1.0, 1.0, 1),     # hot white
      0.2: vec4(1.0, 0.7, 1.0, 1),     # pale magenta
      0.5: vec4(0.8, 0.3, 1.0, 0.9),   # violet arc
      0.8: vec4(0.3, 0.0, 0.5, 0.5),   # deep purple fade
      1.0: vec4(0.05, 0.0, 0.1, 0),
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    self.streaks = P.StreakRenderer(self.curl, name="STRK",
                                    material=self.material,
                                    Length=0.10,
                                    Width=0.010)
    self.render(self.streaks)
