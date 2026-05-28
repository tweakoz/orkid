################################################################################
# curlxf.py — CurlNoise warp showcase. Cycles through Ripple → InpNoise →
# Rot over a 30-second loop, each warp dominating for ~1/3 of the cycle
# while the others fade out. Quantization (CellSize / Levels) stays off
# so the warp effects show on smooth, unmodified curl flow.
#
# Phase weighting: each warp's amplitude is a half-rectified sine of a
# shared phase angle with a 2π/3 offset between them. That gives smooth
# fade-in / fade-out per warp with small overlap regions where two warps
# blend — visually you see Ripple's concentric rings give way to InpNoise's
# organic fractal distortions, which then yield to Rot's spinning sample
# axes. Repeat.
#
# What to watch for in each phase:
#   Ripple   — concentric "breathing" rings; particles seem to push
#              outward then collapse inward periodically along radii.
#   InpNoise — organic, fractal distortion of the swirl pattern; eddies
#              warp into bulging, taffy-pulled shapes.
#   Rot      — the whole flow field tumbles; particles get redirected as
#              their sample position rotates around them.
#
# Hosted by ork.particles.player.py:
#   ork.particles.player.py curlxf
################################################################################

import math

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture

from ork.hypergraph.dflow.particles import ParticleSystem
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr as E

tokens = CrcStringProxy()


class CurlXfSystem(ParticleSystem):

  def __init__(self):
    super().__init__()

    self.ptc_pool = P.PoolData(size=80000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=-0.2,
                                       DispersionAngle=180,
                                       LifeSpan=2.0,
                                       Scalar=2,
                                       EmissionRate=10000,
                                       MinU=0, MaxU=1,
                                       MinV=0, MaxV=1.0,
                                       P1=vec3(0, 0, 0),
                                       P2=vec3(0, 0, 0))

    # No quantization — we want the warps to act on smooth curl flow so
    # their distortions read clearly.
    self.curl = P.CurlNoise(self.emitter, name="CURL",
                            Strength=0.1,
                            Frequency=4.1,
                            Speed=0.15,
                            CellSize=0,
                            Levels=0)

    # ---- material — neutral white→amber→deep so warps stand out ----

    self.material = particles.GradientMaterial.createShared()
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 1.2
    self.material.gradient.setColorStops({
      0.0: vec4(0.0, 0.0, 0.0, 1),
      0.3: vec4(1.0, 0.4, 0.2, 0.8),
      0.6: vec4(1.0, 0.85, 0.5, 1),
      1.0: vec4(0.1, 0.0, 0.0, 0),
    })
    self.material.modulation_texture = Texture.load("src://effect_textures/ptc3.png")

    self.streaks = P.StreakRenderer(self.curl, name="STRK",
                                    material=self.material,
                                    Length=0.55,
                                    Width=0.05)
    self.render(self.streaks)

    ############################################################################
    # Warp cycling: shared phase angle, 2π/3 offset between the three warps.
    # Period = 2π / RATE seconds. RATE = 0.21 gives ~30s per full cycle.
    ############################################################################

    RATE = 1.9
    PHASE_OFFSET_2 = 2.094     # 2π/3
    PHASE_OFFSET_3 = 4.189     # 4π/3

    phase = E.time * RATE

    # Half-rectified sines, each nonzero for half the cycle, peaking at
    # the midpoint of its third. max(0, x) clips the negative half so the
    # three weights take turns rather than running simultaneously.
    ripple_w   = E.max(0, E.sin(phase))
    inpnoise_w = E.max(0, E.sin(phase + PHASE_OFFSET_2))
    rot_w      = E.max(0, E.sin(phase + PHASE_OFFSET_3))

    # Ripple: peak amplitude 0.6 at midpoint of its phase third.
    # RippleFreq held steady — only the amplitude pulses on/off.
    self.curl.Ripple     = ripple_w * 0.6
    self.curl.RippleFreq = 3.0

    # InpNoise: peak amplitude 0.45 at midpoint of its phase third.
    self.curl.InpNoise     = inpnoise_w * 0.45
    self.curl.InpNoiseFreq = 1.2

    # Rot: three Euler axes spin at different rates while the rot phase
    # is active. When rot_w=0 the whole vec3 collapses to zero
    # (identity) and the field returns to its un-rotated state.
    #
    # Expressed via lerp(0, spin, rot_w) per axis instead of the more
    # natural `spin * rot_w`, because the chain lowerer can't handle
    # two non-const sources multiplied directly (FloatExprModule
    # fallback would, but that's task #27d). Lerp's typed module
    # accepts independent inputs by design — same end result, just
    # routed through a 3-input module instead of a 2-input multiply.
    self.curl.Rot = E.vec3(
      E.lerp(0, E.sin(E.time * 0.5) * 1.2, rot_w),
      E.lerp(0, E.cos(E.time * 0.4) * 1.0, rot_w),
      E.lerp(0, E.sin(E.time * 0.7) * 0.8, rot_w),
    )
