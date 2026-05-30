###############################################################################
# CarbonFiber — woven carbon-fiber composite, mappable onto ANY shape (ptex3d).
#
# Decomposed into reusable function assets (ork.hypergraph.ptex3d.functions):
#   * carbon_weave(uv)  -> greyscale 2x2-twill (height + warp/weft mask)
#   * triplanar(ctx, .) -> projects that 2D weave onto an arbitrary 3D surface
#                          (plane / sphere / cube / irregular) with NO UVs.
# This material just maps the greyscale weave outputs onto PBR fields: a bump
# from the tow relief, a clear-coat-ish gloss that peaks on the tow crowns, and
# a faint warp/weft tonal split. Bump only (no parallax).
#
#   from ork.hypergraph.assets.materials import CarbonFiber
#   mat = self.asset.Ptex3d("cf", dsl_class=CarbonFiber, scale=12.0)
#
# Knobs (runtime ctx.param): base_color, bump_scale, gloss, weft_tint.
# Bake-time (ctor): scale (weave frequency), sharpness (triplanar blend width).
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import triplanar, carbon_weave


class CarbonFiber(Ptex3d):
  def __init__(self, ctx, *, scale=4.0, sharpness=1.0,
               base_color=vec3(0.2, 0.2, 0.25), bump_scale=0.001,
               gloss=0.03, weft_tint=0.10):
    base   = ctx.param("base_color", base_color)
    bumps  = ctx.param("bump_scale", bump_scale)
    glo    = ctx.param("gloss",      gloss)        # roughness of the tow crowns
    tint   = ctx.param("weft_tint",  weft_tint)    # warp/weft tonal split amount

    w      = triplanar(ctx, carbon_weave, scale=scale, sharpness=sharpness)
    height = w.x                                   # tow relief [0,1]
    over   = w.y                                   # 1 = warp tow, 0 = weft tow

    # near-black carbon; crowns lift a touch, warp vs weft split by a faint tint
    tone = base * P.mix(0.8, 1.25, height)
    tone = tone * P.mix(1.0 - tint, 1.0 + tint, over)
    self.surface(
      albedo    = tone,
      metallic  = 0.0,                             # dielectric resin coat over fiber
      # clear-coat sheen: glossy crowns, rougher in the woven valleys
      roughness = P.mix(0.5, glo, height),
    )
    self.displace(height, scale=bumps)             # woven relief (bump only)


__all__ = ["CarbonFiber"]
