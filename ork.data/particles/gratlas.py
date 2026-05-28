################################################################################
# gratlas.py — HyperSyn DSL example for GradientAtlasMaterial.
#
# Same emitter / force chain as elliptical.py, but rendered through a
# GradientAtlasMaterial instead of GradientMaterial. The fragment shader
# samples the atlas at vec2(unit_age, aux.x):
#   X axis = particle age (existing semantic; identical to GradientMaterial)
#   Y axis = particle.aux.x  (per-particle, set by the emitter via Aux.x)
#
# CURRENT STATE (with subtasks #44 + #45 landed):
#   - aux.x defaults to 0 for every particle, so every particle samples the
#     atlas's top row. The result looks like the GradientMaterial path with
#     row 0 of the atlas as the active gradient.
#
# WHAT THE REMAINING SUBTASKS UNLOCK FOR THIS FILE:
#   #46 — `self.streaks.GradientPhase = Expr.time * 0.1` will scroll the
#         whole atlas's V coordinate over time, animating the gradient.
#   #47 — `self.emitter.Aux.x = ...` will let each emit cohort set aux.x
#         from any Expr (uniform per cohort).
#   #48 — `self.emitter.Aux.x = Expr.ptc.random` or
#         `Expr.rand_range(0, 1)` will give per-particle variation so each
#         particle samples its OWN row → the killer use case for the atlas.
#
# Hosted by ork.particles.player.py or any of the ECS demos:
#   ork.particles.player.py ork.data/particles/gratlas.py
################################################################################

import math

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine.lev2 import particles, Texture, Image, GfxEnv

from ork.dflow.particles import ParticleSystem
from ork.dflow import particles as P
from ork.dflow import Expr

tokens = CrcStringProxy()


################################################################################
# Atlas authoring — pure-Python procedural construction.
#
# Atlas dimensions: ATLAS_W (= unit_age axis) × ATLAS_H (= phase axis).
# Each row is one complete gradient. The atlas cycles through THREE palette
# targets — icy → fiery → emerald → icy — evenly spaced across V. With the
# (#46) GradientPhase scroll, the visible gradient transitions smoothly
# through all three on a loop. Each segment is linearly interpolated between
# adjacent palettes; the cycle wraps at V=1 → V=0 so there's no seam.
################################################################################

ATLAS_W = 256
ATLAS_H = 96   # 3 palettes × 32 rows each — keep divisible by len(PALETTES)

# Multi-stop palettes — each entry is (key_in_[0,1], rgba). Stops MUST be in
# ascending key order. sample_palette() does linear interp between neighbors.
ICY_STOPS = [
  (0.00, (1.00, 1.00, 1.00, 1.00)),   # white hot core
  (0.30, (0.65, 0.90, 1.00, 1.00)),   # ice-blue
  (0.60, (0.10, 0.30, 0.85, 0.80)),   # deep blue
  (1.00, (0.00, 0.00, 0.20, 0.00)),   # dark / fade
]

FIERY_STOPS = [
  (0.00, (1.00, 1.00, 0.80, 1.00)),   # white-yellow core
  (0.30, (1.00, 0.70, 0.10, 1.00)),   # orange
  (0.60, (0.90, 0.10, 0.00, 0.80)),   # deep red
  (1.00, (0.20, 0.00, 0.00, 0.00)),   # dark / fade
]

EMERALD_STOPS = [
  (0.00, (0.85, 1.00, 0.90, 1.00)),   # mint-white core
  (0.30, (0.20, 0.95, 0.30, 1.00)),   # bright emerald
  (0.60, (0.05, 0.55, 0.15, 0.80)),   # deep emerald
  (1.00, (0.00, 0.15, 0.00, 0.00)),   # dark / fade
]

# Palette cycle order. The atlas walks these in sequence and wraps at the
# end — segment N is `lerp(PALETTES[N], PALETTES[(N+1) % len], t)`.
PALETTES = [ICY_STOPS, FIERY_STOPS, EMERALD_STOPS]


def _sample_palette(stops, u):
  """Linear-interpolate the multi-stop palette at u ∈ [0,1]."""
  if u <= stops[0][0]:
    return stops[0][1]
  for i in range(len(stops) - 1):
    k0, c0 = stops[i]
    k1, c1 = stops[i + 1]
    if u <= k1:
      t = (u - k0) / (k1 - k0) if k1 > k0 else 0.0
      return tuple(c0[j] * (1 - t) + c1[j] * t for j in range(4))
  return stops[-1][1]


def _build_atlas_image(width, height):
  """Construct an RGBA8 lev2.Image cycling through PALETTES along V.
  Pure CPU work; no gfx context needed. The cycle wraps at V=1→V=0 so the
  GradientPhase scroll has no visible seam."""
  buf = bytearray(width * height * 4)
  n = len(PALETTES)
  for y in range(height):
    # Map y to a continuous phase in [0, n). Segment N is between
    # PALETTES[N] and PALETTES[(N+1) % n].
    phase  = (y / height) * n
    seg    = int(phase) % n
    t      = phase - int(phase)
    pal_a  = PALETTES[seg]
    pal_b  = PALETTES[(seg + 1) % n]
    for x in range(width):
      u = x / max(width - 1, 1)
      ca = _sample_palette(pal_a, u)
      cb = _sample_palette(pal_b, u)
      r = int(max(0.0, min(1.0, ca[0] * (1 - t) + cb[0] * t)) * 255)
      g = int(max(0.0, min(1.0, ca[1] * (1 - t) + cb[1] * t)) * 255)
      b = int(max(0.0, min(1.0, ca[2] * (1 - t) + cb[2] * t)) * 255)
      a = int(max(0.0, min(1.0, ca[3] * (1 - t) + cb[3] * t)) * 255)
      i = (y * width + x) * 4
      buf[i]     = r
      buf[i + 1] = g
      buf[i + 2] = b
      buf[i + 3] = a
  return Image.createFromBuffer(width, height, tokens.RGBA8, bytes(buf))


def _build_atlas_texture(name):
  """Build the procedural atlas image and upload to GPU as a Texture.
  Called from the DSL __init__ — the player invokes that inside onGpuInit so
  the gfx context is already alive at this point."""
  img = _build_atlas_image(ATLAS_W, ATLAS_H)
  ctx = GfxEnv.ref.loadingContext()
  tex = Texture(name)
  ctx.TXI.updateTexture(tex, img, False)
  return tex


class GratlasSystem(ParticleSystem):
  """Elliptical attractor with vortex+turbulence, streak-rendered through a
  gradient-atlas material. Visible variation will come once #46/#47/#48
  land — this file is ready to absorb them as the bindings get authored."""

  def __init__(self):
    super().__init__()

    # ---- graph: pool → emitter → turb → vortex → ellipse → gravity → streaks ----

    self.ptc_pool = P.PoolData(size=50000, name="POOL")

    self.emitter = P.EllipticalEmitter(self.ptc_pool, name="EMITN",
                                       EmissionVelocity=1.0,
                                       DispersionAngle=180,
                                       LifeSpan=2.0,
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

    # ---- material: gradient atlas (the new bit) ----

    # The atlas: procedurally built RGBA8 texture. Rows interpolate icy ↔
    # fiery via a half-cosine, so scrolling V over time (via #46 once it
    # lands) produces a smooth icy → fiery → icy loop.
    self.material = particles.GradientAtlasMaterial.createShared()
    self.material.atlas = _build_atlas_texture("gratlas_atlas")
    self.material.blending = tokens.ADDITIVE
    self.material.depthtest = tokens.OFF
    self.material.colorIntensity = 1.0
    # cookie: per-particle sprite shape (same as elliptical.py /
    # varying_test.py use). Multiplied with the gradient sample in the
    # ps_grad_atlas fragment shader.
    self.material.modulation_texture = Texture.load("src://effect_textures/knob2")

    # ---- renderer (chain terminus + render() sink for the player) ----

    self.streaks = P.StreakRenderer(self.gravity, name="STRK",
                                    material=self.material,
                                    Length=0.15,
                                    Width=0.015)
    self.render(self.streaks)

    ############################################################################
    # Time-driven bindings — same as elliptical.py so the underlying motion
    # is recognizable. The GRADIENT animation hooks land with #46/#47/#48.
    ############################################################################

    e = Expr

    self.emitter.MinV      = 0.6 + e.sin(e.time) * 0.2
    self.emitter.MaxV      = 0.4 - e.sin(e.time) * 0.2
    self.turbulence.Amount = e.vec3(e.sin(e.time * 0.25) * 20)

    # Scroll the atlas's V coordinate over time. The fragment shader does
    # fract() on this, so the value can grow unbounded — every 1.0 of
    # GradientPhase is one full loop through the icy↔fiery cycle.
    self.streaks.GradientPhase = e.time * 0.1

    # Per-particle Aux.x via rand_range — each particle samples its own
    # row of the atlas. The range itself slides along a sine wave of time:
    # at any given moment the spawn window is 0.4 wide and its center
    # oscillates between 0.1 and 0.9 over a ~21-second period (2π/0.3).
    # Combined with the GradientPhase scroll above, you get per-particle
    # variation (different rows in each cohort) layered on top of global
    # icy↔fiery motion.
    self.emitter.Aux.x = e.sin(e.time * 1.0)*0.5+0.5
