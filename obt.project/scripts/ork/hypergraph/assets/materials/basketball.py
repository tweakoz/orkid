###############################################################################
# BasketBall — procedural basketball (GEOV2 ptex3d).
#
# Orange pebbled rubber with near-black recessed seams. Seams are simple vertical
# gore segments (like the beachball) for an easy, clean pattern. The pebbling is
# high-freq value noise driving the micro-bump AND a roughness variation so the
# raised grains read a touch shinier than the valleys. Bump only (no parallax).
#
#   from ork.hypergraph.assets.materials import BasketBall
#   mat = self.asset.Ptex3d("ball", dsl_class=BasketBall, panels=8)
#
# Knobs:
#   bake-time (ctor kwargs): panels (gore count), pebble_oct (noise octaves)
#   runtime  (ctx.param):    base_color, seam, pebble_freq, pebble_amt, bump_scale
###############################################################################

import math

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P, rgb

_TAU = 2.0 * math.pi


class BasketBall(Ptex3d):
  def __init__(self, ctx, *, panels=8, pebble_oct=3,
               base_color=vec3(0.82, 0.33, 0.09),
               seam=0.04, pebble_freq=65.0, pebble_amt=0.10, bump_scale=0.015):
    base   = ctx.param("base_color",  base_color)
    seam_w = ctx.param("seam",        seam)         # seam groove width
    pfreq  = ctx.param("pebble_freq", pebble_freq)  # pebble density (higher = smaller grains)
    pamt   = ctx.param("pebble_amt",  pebble_amt)   # pebble bump strength (0..1)
    bumps  = ctx.param("bump_scale",  bump_scale)

    p = ctx.P_object
    # vertical gore segments (like the beachball) — simple, clean seam pattern
    u  = P.atan2(p.z, p.x) * (1.0 / _TAU) + 0.5
    e  = P.fract(u * float(panels))
    dv = P.min(e, 1.0 - e)                          # 0 at a seam, 0.5 mid-segment
    # UNIFORM-thickness seams: meridians converge near the poles, so a fixed
    # longitude band shrinks in world space. Divide the threshold by cos(lat) to
    # hold a constant WORLD width (the gores merge into a small junction at the
    # very poles, clamped by the 0.15 floor).
    coslat   = P.sqrt(p.x * p.x + p.z * p.z) / P.length(p)   # 1 at equator -> 0 at poles
    seamline = P.smoothstep(0.0, seam_w / P.max(coslat, 0.15), dv)   # 0 near seam, 1 on panel

    pebble = P.fbm(p * pfreq, pebble_oct)           # small high-freq grains ~[0,1]

    orange = base * P.mix(0.88, 1.10, pebble)       # pebble tone variation
    albedo = P.mix(rgb(0.03, 0.02, 0.015), orange, seamline)   # near-black seams

    self.surface(
      albedo    = albedo,
      metallic  = 0.0,                                          # dielectric rubber
      # seam matte; on the panel, grain PEAKS (pebble high) are a touch shinier
      roughness = P.mix(0.82, P.mix(0.74, 0.46, pebble), seamline),
    )
    # bump: panels carry the pebble grain; seams cut deep grooves
    panel  = P.mix(1.0 - pamt, 1.0, pebble)
    height = P.mix(0.0, panel, seamline)
    self.displace(height, scale=bumps)


__all__ = ["BasketBall"]
