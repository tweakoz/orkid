###############################################################################
# NeedleProc / NeedleCard — the PROCEDURAL conifer needle-tuft material, two
# forms sharing ONE look definition (`needle_fields`, the card-bake contract —
# see ptex3d/card_bake.py and leaf.py, the same shape):
#
#   needle_fields  a SHARP two-rank needle comb: a front rank of fanned,
#                  per-needle-varied strips over a pale UNDERSIDE rank
#                  (stomatal-band read), staggered ragged tips, a woody twig
#                  core, fine along-needle striation (hi-freq), plus the
#                  card.v2 translucency + cavity fields.
#   NeedleProc     LIVE analytic form (tuning surface / reference).
#   NeedleCard     STORED form: one shared baked card texture; per-pixel cost
#                  is a texture fetch. Tint, per-tuft Cd.y brightness and the
#                  sun backlight LIVE.
#
# The recipe: the standard conifer-tuft atlas construction — parallel strip
# combs in fanned coordinates (strips diverge toward the tip), golden-ratio
# per-needle hashes for length/width/brightness decorrelation, a second
# offset rank underneath for depth. Edges are BAKED SHARP (≈1.5-texel AA
# band): the stored card mips, so silhouette crispness is free; the analytic
# form re-widens via aa_ramp automatically.
#
# Both: A2C (needs MSAA), hard 0.5 discard, alpha_cutout masked depth prepass,
# two-sided sun-facing normal resolve, sun-wrap backlight off the baked
# translucency channel (shared foliage_surface — see leaf.py). Card geometry
# from Hypermesh.leaves(); u = across the tuft, v = petiole..tip.
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.assets.materials.leaf import foliage_surface, TWO_PI
from orkengine.core import vec3

# shade gain: per-needle brightness + the pale underside rank peak ABOVE 1
# (young growth / stomatal bands brighter than the base tint). The card stores
# shade NORMALIZED to 0..1; both forms multiply the gain back so live and
# stored render identically.
NEEDLE_SHADE_GAIN = 1.40


def needle_fields(P, u, v, ramp,
                  needles=8.0,        # front-rank needle count across the tuft
                  rank2_needles=11.0, # underside-rank needle count
                  fan=0.55,           # strips diverge toward the tip (tuft splay)
                  edge=0.020,         # needle edge AA half-band (strip-fract units)
                  tip_soft=0.008,     # tip AA half-band (v units)
                  stem_w=0.014,       # twig core half-width (u units)
                  striate=60.0,       # along-needle striation frequency (hi-freq)
                  striate_amp=0.10):  # striation shade amplitude
  """Two-rank needle comb fields (both realizations). Returns the card.v2 tuple
  (alpha, shade, transl, cavity); shade is normalized — consumers multiply
  NEEDLE_SHADE_GAIN back for radiance."""
  def rank(n, seed, wmul, lmul):
    # fanned strip coordinate: strips spread outward as v rises
    t   = (u - 0.5) * n / (1.0 + fan * v) + 0.5 * n + seed
    idx = P.floor(t)                               # which needle
    d   = P.abs(P.fract(t) - 0.5)                  # 0 needle center .. 0.5 gap
    h1  = P.fract(idx * 0.6180339887)              # per-needle hashes (golden ratio)
    h2  = P.fract(idx * 0.7548776662)
    ln  = (0.55 + 0.42 * h1) * lmul                # ragged decorrelated lengths
    hw  = 0.24 * wmul * (0.70 + 0.50 * h2) * (1.0 - 0.35 * v)   # per-needle width, tapering
    a   = ((1.0 - ramp(d, hw - edge, hw + edge))
           * (1.0 - ramp(v, ln - tip_soft, ln + tip_soft)))
    return a, h1
  aA, hA = rank(needles,       0.0,  1.0,  1.0)    # front rank
  aB, hB = rank(rank2_needles, 0.37, 0.72, 0.92)   # pale underside rank, behind
  # woody twig core the needles attach to
  stem = ((1.0 - ramp(P.abs(u - 0.5), stem_w, stem_w + 0.010))
          * (1.0 - ramp(v, 0.78, 0.85)))
  alpha = P.max(P.max(aA, aB), stem)
  stem_only = stem * (1.0 - P.max(aA, aB))
  # fine along-needle striation — the hi-freq shade content (per-needle phase)
  stri = P.sin((v * striate + hA * 7.0 + hB * 3.0) * TWO_PI)
  # ---- shade: front rank dark green, underside rank PALE, tips young/lighter ----
  sA = (0.74 + 0.22 * hA) * P.mix(0.90, 1.16, v)
  sB = (0.98 + 0.18 * hB) * P.mix(0.95, 1.12, v)
  sh = P.mix(sB, sA, aA)                           # front rank owns where it covers
  sh = sh * (1.0 + striate_amp * 0.5 * stri)
  sh = P.mix(sh, 0.38, stem_only)                  # woody twig
  shade = P.saturate(sh * (1.0 / NEEDLE_SHADE_GAIN))
  # ---- translucency: thin tips pass light, underside more, twig blocks ----
  trA = 0.35 + 0.30 * v
  trB = 0.50 + 0.30 * v
  tr  = P.mix(trB, trA, aA)
  transl = P.saturate(P.mix(tr, 0.06, stem_only))
  # ---- cavity/AO: crowded tuft base darkens, twig recessed ----
  cav = P.mix(0.55, 1.0, v)
  cavity = P.saturate(P.mix(cav, 0.70, stem_only))
  return alpha, shade, transl, cavity


class NeedleProc(Ptex3d):
  """LIVE analytic needle tuft (reference/tuning form). Field params pass
  straight through to needle_fields."""
  def __init__(self, ctx, *, albedo=vec3(0.07, 0.16, 0.09), roughness=0.7, metallic=0.0,
               **field_params):
    uv = ctx.uv
    alpha, shade, transl, cavity = needle_fields(
        P, uv.x, uv.y,
        lambda x, e0, e1: self.aa_ramp(x, e0, e1),
        **field_params)
    foliage_surface(self, ctx, shade=shade * NEEDLE_SHADE_GAIN, transl=transl,
                    cavity=cavity, alpha=alpha,
                    albedo=albedo, roughness=roughness, metallic=metallic,
                    lamina_tint=(1.10, 1.04, 0.62), lamina_tint_amt=0.35,
                    transmit_tint=(0.45, 0.95, 0.35),
                    backlit_gain=1.5, backlit_power=8.0, backlit_wrap=0.15)


class NeedleCard(Ptex3d):
  """STORED needle tuft: samples the shared baked card (card.v2: r=normalized
  shade × NEEDLE_SHADE_GAIN live, g=translucency, b=cavity, a=silhouette)."""
  def __init__(self, ctx, *, albedo=vec3(0.07, 0.16, 0.09), roughness=0.7, metallic=0.0):
    card = ctx.tex("CardTex")                      # bind via sampler_textures
    foliage_surface(self, ctx, shade=card.x * NEEDLE_SHADE_GAIN, transl=card.y,
                    cavity=card.z, alpha=card.w,
                    albedo=albedo, roughness=roughness, metallic=metallic,
                    lamina_tint=(1.10, 1.04, 0.62), lamina_tint_amt=0.35,
                    transmit_tint=(0.45, 0.95, 0.35),
                    backlit_gain=1.5, backlit_power=8.0, backlit_wrap=0.15)


__all__ = ["NeedleProc", "NeedleCard", "needle_fields", "NEEDLE_SHADE_GAIN"]
