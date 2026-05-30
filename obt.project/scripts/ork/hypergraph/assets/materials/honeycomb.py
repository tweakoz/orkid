###############################################################################
# HoneyComb — uniform, seamless hex-sphere (GEOV2 ptex3d).
#
# Uses P.spherecells: the IMPLICIT spherical-Fibonacci lattice — uniform density,
# seamless, NO pole singularity, NO stored points, NO UV chart. The tile count
# `cells` is a runtime scalar (more tiles, no recompile). Per-cell color from the
# cell id, recessed seams from the F2-F1 gap. (A sphere can't be all-hexagons —
# Euler forces ~12 pentagons; the lattice spreads that defect uniformly.) Bump
# only, no parallax.
#
#   from ork.hypergraph.assets.materials import HoneyComb
#   mat = self.asset.Ptex3d("comb", dsl_class=HoneyComb, cells=400)
#
# Knobs (all runtime ctx.param): cells (tile count), tile (seam width),
#                                bump_scale, base_color, hue_spread, seam_color.
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P


class HoneyComb(Ptex3d):
  def __init__(self, ctx, *,
               cells=400.0, tile=0.18, bump_scale=0.02,
               base_color=vec3(0.95, 0.70, 0.12), hue_spread=0.06,
               seam_color=vec3(0.05, 0.03, 0.0)):
    n      = ctx.param("cells",      cells)       # tile count (runtime -> no recompile)
    tile_  = ctx.param("tile",       tile)        # seam width (fraction of the F2-F1 gap)
    bumps  = ctx.param("bump_scale", bump_scale)
    base   = ctx.param("base_color", base_color)
    seamc  = ctx.param("seam_color", seam_color)

    g    = P.spherecells(ctx.P_object, n)
    cid  = g.id                                    # per-cell hash in [0,1]
    # normalize the seam by the cell size (.f1 ~ cell radius) so the seam width is
    # uniform across tile counts; `tile` is then a fraction in [0,1].
    cell = P.smoothstep(0.0, tile_ * (g.f1 + 0.02), g.edge)   # 0 at seam, 1 inside cell

    tone   = base * P.mix(0.78, 1.12, cid)
    jitter = P.vec3(1.0 + hue_spread * (cid - 0.5), 1.0, 1.0 - hue_spread * (cid - 0.5))
    self.surface(
      albedo    = P.mix(seamc, tone * jitter, cell),
      metallic  = 0.0,
      roughness = P.mix(0.55, 0.35, cell),          # waxy cell, matte seam
    )
    self.displace(cell, scale=bumps)                # recessed seams (bump only)


__all__ = ["HoneyComb"]
