###############################################################################
# SpaceshipHull — procedural sci-fi hull plating, mappable onto ANY shape (ptex3d).
#
# Decomposed into reusable function assets (ork.hypergraph.ptex3d.functions):
#   * panel_split(uv, levels) -> irregular rectangular panels (id, edge, local-uv)
#   * greeble(local, id, ...)  -> raised sub-plates inside each panel
#   * triplanar(ctx, .)        -> projects both onto an arbitrary 3D hull, no UVs.
# The material maps the greyscale/data outputs onto a painted-metal PBR surface:
# recessed seam grooves + greebles (bump), per-panel paint jitter, and edges
# chipped to bare metal. Bump only (no parallax).
#
#   from ork.hypergraph.assets.materials import SpaceshipHull
#   mat = self.asset.Ptex3d("hull", dsl_class=SpaceshipHull, greeble_cover=0.3)
#
# Runtime ctx.param: seam, bump_scale, greeble_cover (density dial, 0 = clean
#   panels), greeble_height, panel_color, metal_color, roughness, rough_jitter,
#   wear. Bake-time (ctor): levels (panel count), greeble_grid, scale, sharpness.
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import triplanar, panel_split, greeble


class SpaceshipHull(Ptex3d):
  def __init__(self, ctx, *, levels=6, greeble_grid=4.0, scale=4.0, sharpness=8.0,
               seam=0.012, bump_scale=0.02,
               greeble_cover=0.30, greeble_height=0.45,
               panel_color=vec3(0.32, 0.34, 0.36), metal_color=vec3(0.15, 0.16, 0.18),
               roughness=0.8, rough_jitter=0.14, wear=0.5):
    seamw = ctx.param("seam",           seam)
    bumps = ctx.param("bump_scale",     bump_scale)
    cover = ctx.param("greeble_cover",  greeble_cover)   # greeble density (0 = clean panels)
    gh    = ctx.param("greeble_height", greeble_height)  # greeble bump strength
    paint = ctx.param("panel_color",    panel_color)
    metal = ctx.param("metal_color",    metal_color)
    rough = ctx.param("roughness",      roughness)
    rjit  = ctx.param("rough_jitter",   rough_jitter)
    wearp = ctx.param("wear",           wear)

    # one triplanar carrying everything the surface needs: panel id, seam dist,
    # greeble height -> vec4(id, edge, greeble, _)
    def field(uv):
      pan = panel_split(uv, levels)
      grb = greeble(P.vec2(pan.z, pan.w), pan.x, cover, greeble_grid)
      return P.vec4(pan.x, pan.y, grb, 0.0)

    f     = triplanar(ctx, field, scale=scale, sharpness=sharpness)
    pid   = f.x                                  # per-panel hash
    edge  = f.y                                  # distance to nearest seam
    greeb = f.z                                  # greeble height [0,1]

    panel  = P.smoothstep(0.0, seamw, edge)      # 0 in the seam groove, 1 on the panel
    r2     = P.fract(pid * 97.13)                # decorrelated per-panel random
    wear_m = (1.0 - panel) * wearp               # paint chipped to metal along seams

    paint_v = paint * P.mix(0.85, 1.15, pid)     # per-panel paint-batch jitter
    self.surface(
      albedo    = P.mix(paint_v, metal, wear_m),
      metallic  = wear_m,                                          # paint dielectric; worn edges metallic
      roughness = P.mix(rough + rjit * (r2 - 0.5), 0.42, wear_m),  # per-panel satin; worn metal smoother
    )
    # panels raised over recessed seams, greebles raised on top
    self.displace(panel + greeb * gh, scale=bumps)


__all__ = ["SpaceshipHull"]
