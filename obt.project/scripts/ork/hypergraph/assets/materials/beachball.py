###############################################################################
# BeachBall — procedural inflatable beach ball (GEOV2 ptex3d).
#
# Vertical colored gore panels (longitude via atan2) in bright hues, with
# recessed welded seams (bump). Glossy vinyl PBR (no parallax — just bump).
#
#   from ork.hypergraph.assets.materials import BeachBall
#   mat = self.asset.Ptex3d("ball", dsl_class=BeachBall, panels=6)
#
# Knobs:
#   bake-time (ctor kwargs): panels (gore count), saturation/value (panel hues)
#   runtime  (ctx.param):    seam (weld width), bump_scale
###############################################################################

import math

from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.colors import hsv

_TAU = 2.0 * math.pi


class BeachBall(Ptex3d):
  def __init__(self, ctx, *, panels=6, saturation=0.85, value=1.0,
               seam=0.04, bump_scale=0.02):
    seam_w = ctx.param("seam",       seam)        # weld width (runtime)
    bumps  = ctx.param("bump_scale", bump_scale)  # seam relief strength (runtime)

    p = ctx.P_object
    # longitude -> [0, 1], split into `panels` gores
    u     = P.atan2(p.z, p.x) * (1.0 / _TAU) + 0.5
    pu    = u * float(panels)
    panel = P.floor(pu)                            # 0 .. panels-1
    e     = P.fract(pu)
    edge  = P.min(e, 1.0 - e)                      # 0 at a seam, 0.5 mid-panel

    # per-panel bright colors — Python hsv constants selected by panel index
    cols = [hsv(360.0 * i / panels, saturation, value) for i in range(panels)]
    col  = cols[0]
    for i in range(1, panels):
      col = P.mix(col, cols[i], P.step(float(i) - 0.5, panel))

    seammask = P.smoothstep(0.0, seam_w, edge)     # 0 at seam, 1 on panel
    self.surface(
      albedo    = col * P.mix(0.55, 1.0, seammask),          # darken the weld line
      metallic  = 0.0,                                       # dielectric vinyl
      roughness = P.mix(0.30, 0.12, seammask),              # glossy panel, rougher weld
    )
    # bump only (parallax_steps defaults 0) — recessed weld grooves
    self.displace(seammask, scale=bumps)


__all__ = ["BeachBall"]
