###############################################################################
# Lily pads / round pillars — ptex3d surface (GEOV2). A happy accident from the
# cobblestone work: a FULL HEMISPHERE dome over the voronoi cells (radial .f1
# profile) reads as round pads / pillars rather than cobbles. Kept as its own
# material because the shape is its own thing.
#
#   from ork.hypergraph.assets.materials import LilyPads
#   mat = self.asset.Ptex3d("lilies", dsl_class=LilyPads, pom_steps=16)
#
# ONE class; `pom_steps` gates the relief: 0 (default) = bump only; > 0 = +
# PROCEDURAL parallax (EXPENSIVE/TEMPORARY, GEOV2 §18.5; baked-height later).
#
# Knobs (all are ctor kwargs; the value sets the param default — settable
# statically from the scene and still runtime-bindable):
#   bake-time (folds into shader): pom_steps
#   runtime  (uniform, no recompile): cell_scale, base_color, warp, radius,
#                                     bump_scale, relief, water_color
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P, rgb


class LilyPads(Ptex3d):
  """Round hemispherical pads/pillars over warped voronoi cells. ONE class;
  `pom_steps` (bake-time) gates the relief: 0 (default) = bump only; > 0 = +
  PROCEDURAL parallax occlusion (EXPENSIVE/TEMPORARY march, GEOV2 §18.5). Every
  other knob is a runtime ctx.param (ctor kwarg sets the default)."""

  def __init__(self, ctx, *, pom_steps=0, cell_scale=4.0,
               base_color=vec3(0.30, 0.45, 0.30),
               warp=0.20, radius=0.70, bump_scale=0.03, relief=0.10,
               water_color=vec3(0.04, 0.16, 0.20)):
    cell_scale = ctx.param("cell_scale", cell_scale)         # cell density (runtime)
    base   = ctx.param("base_color", base_color)              # pad green
    warp   = ctx.param("warp",       warp)                    # cell-shape irregularity
    radius = ctx.param("radius",     radius)                  # pad radius, cell units (full -> touching)
    bumps  = ctx.param("bump_scale", bump_scale)              # bump strength
    relief = ctx.param("relief",     relief)                  # parallax depth, object units

    pc     = ctx.P_object * cell_scale
    wv     = P.vec3(P.fbm(pc * 0.6, 2), P.fbm(pc * 0.6 + 13.0, 2), P.fbm(pc * 0.6 + 29.0, 2))
    vcoord = pc + warp * wv
    cell   = P.voronoi(vcoord)

    # FULL hemisphere over distance-to-cell-centre (.f1) — the very-round profile.
    t      = P.saturate(cell.f1 / radius)
    dome   = P.sqrt(P.saturate(1.0 - t * t))                  # hemisphere
    height = dome * P.mix(0.85, 1.0, cell.cell2)

    pad    = P.smoothstep(0.04, 0.35, dome)
    grain  = P.fbm(ctx.P_object * cell_scale * 5.0, 3)
    padcol = base * P.mix(rgb(0.75, 0.85, 0.70), rgb(1.10, 1.15, 1.00), cell.cell)
    padcol = padcol * P.mix(0.85, 1.10, grain)
    water  = ctx.param("water_color", water_color)            # the gaps are WATER, not black
    albedo = P.mix(water, padcol, pad)                         # pads floating on water

    # bump-only -> glossier so the dome relief reads; parallax -> damp/matte
    rough = (P.mix(0.85, 0.55, cell.cell * pad) if pom_steps > 0
             else P.mix(0.60, 0.35, cell.cell * pad))
    self.surface(albedo=albedo, metallic=0.0, roughness=rough)
    self.displace(height, scale=bumps, parallax_steps=pom_steps, depth=relief)


__all__ = ["LilyPads"]
