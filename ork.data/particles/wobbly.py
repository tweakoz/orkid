################################################################################
# circuit.py — CurlNoise with ONLY position quantization (CellSize) active.
#
# Sampled at a snapped grid, ψ is constant within each cell so the curl is
# ~zero inside cells and spikes at the axis-aligned boundaries. Particles
# drift quietly through a cell, then get launched orthogonally across a
# face — the result reads as charge flowing through stamped circuit traces.
#
# Distinguishing features vs. dendrite.py:
#   - CellSize > 0, Levels = 0 → boundaries are AXIS-ALIGNED (orthogonal,
#     stamped). Particles preferentially move along X/Y/Z directions.
#   - dendrite.py is the inverse: CellSize=0, Levels>0 → boundaries are
#     ORGANIC iso-curves of the noise → arborescent / neural patterns.
#
# Hosted by ork.particles.player.py:
#   ork.particles.player.py circuit
################################################################################

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr

tokens = CrcStringProxy()


class CircuitSystem(ParticleSystem):

  def __init__(self):
    super().__init__()

    self.ptc_pool = P.PoolData(size=80000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=0.01,
                                       DispersionAngle=180,
                                       LifeSpan=0.5,
                                       Scalar=3,
                                       EmissionRate=20000,
                                       MinU=0, MaxU=1,
                                       MinV=0, MaxV=1,
                                       P1=vec3(0, 0, 0),
                                       P2=vec3(0, 0, 0))

    # Position quantization ONLY — Levels stays at 0 (smooth on the
    # cell-cell scale, sharp boundaries between cells).
    self.curl = P.CurlNoise(self.emitter, name="CURL",
                            Strength=0.01,
                            Frequency=0.3, 
                            InpNoise = 0.3,
                            InpNoiseFreq = 3.7,
                            Speed=1.1,
                            CellSize=0.0,
                            Levels=0)

    # Green/cyan palette = "PCB silkscreen" association
    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 1.0
    self.material.gradient.setColorStops({
      0.0: vec4(1.0, 1.0, 1.0, 1),     # hot white
      0.2: vec4(0.4, 1.0, 0.7, 1),     # mint
      0.5: vec4(0.1, 0.9, 0.5, 0.9),   # bright cyan-green
      0.8: vec4(0.5, 0.4, 0.2, 0.5),   # deep green trace
      1.0: vec4(0.0, 0.05, 0.0, 0),
    })
    self.material.modtexture_asset = "src://effect_textures/knob2"  # serializable asset (round-trips to the ECS player; live modulation_texture does not)

    self.streaks = P.StreakRenderer(self.curl, name="STRK",
                                    material=self.material,
                                    Length=0.05,
                                    Width=0.03)
    self.render(self.streaks)

    # Slowly pulse the strength so traces "energize" rhythmically.
    self.curl.Strength = 40.0 + Expr.sin(Expr.time * 0.5) * 20.0
