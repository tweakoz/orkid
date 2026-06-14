###############################################################################
# CobbleStone ptex3d surface — the classic parallax-occlusion showcase
# (AMD ToyShop street / Crysis), voronoi-based so it maps cleanly to any surface
# (unlike planar brick): flat voronoi cells with rounded rims, thin gaps. ONE
# class; `pom_steps` gates the relief:
#   pom_steps == 0 (default) — bump only (a touch of gloss so the rims read).
#   pom_steps  > 0           — + PROCEDURAL parallax occlusion (matte; the stones
#                              occlude the gaps at grazing angles). EXPENSIVE/
#                              TEMPORARY march `pom_steps`x/fragment (GEOV2 §18.5;
#                              a baked-height texture replaces it once auto-uv).
#
#   from ork.hypergraph.assets.materials import CobbleStone
#   mat = self.asset.Ptex3d("cobbles", dsl_class=CobbleStone, pom_steps=16)
#
# Knobs (all are ctor kwargs; the value sets the param default — settable
# statically from the scene and still runtime-bindable):
#   bake-time (folds into shader): pom_steps
#   runtime  (uniform, no recompile): cell_scale, base_color, warp, gap, bevel,
#                                     bump_scale, relief, gap_color
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P, rgb


class CobbleStone(Ptex3d):
  """CobbleStone — flat voronoi cells with rounded rims, thin gaps. ONE class;
  `pom_steps` (bake-time) gates the relief: pom_steps == 0 (default) -> bump only
  (a touch of gloss so the rim relief reads); pom_steps > 0 -> + PROCEDURAL
  parallax occlusion (matte; depth reads geometrically — EXPENSIVE/TEMPORARY
  march, GEOV2 §18.5). Every other knob is a runtime ctx.param (ctor kwarg sets
  the default; still runtime-bindable)."""

  def __init__(self, ctx, *, pom_steps=0,
               cell_scale=4.0,
               base_color=vec3(0.42, 0.42, 0.43),
               warp=0.25, gap=0.05, bevel=0.10, bump_scale=0.03, relief=0.06,
               gap_color=vec3(0.04, 0.04, 0.045)):
    cell_scale = ctx.param("cell_scale", cell_scale)         # cell density (runtime, no recompile)
    base   = ctx.param("base_color", base_color)             # stone grey
    warp   = ctx.param("warp",       warp)                   # stone-shape irregularity
    gap    = ctx.param("gap",        gap)                    # gap width, cell units
    bevel  = ctx.param("bevel",      bevel)                  # edge rounding width, cell units
    bumps  = ctx.param("bump_scale", bump_scale)             # bump strength
    relief = ctx.param("relief",     relief)                 # parallax depth, object units
    gapc   = ctx.param("gap_color",  gap_color)              # recessed-gap tint (dark grout)

    # warped voronoi -> irregular packed stones
    pc     = ctx.P_object * cell_scale
    wv     = P.vec3(P.fbm(pc * 0.6, 2), P.fbm(pc * 0.6 + 13.0, 2), P.fbm(pc * 0.6 + 29.0, 2))
    vcoord = pc + warp * wv
    cell   = P.voronoi(vcoord)
    fw     = cell.fwedge                                       # distance to cell wall

    # FLAT cell tops (most of each stone) with just a SMALL rounded bevel at the
    # rim down into a thin gap — preserves the voronoi cells (NOT a dome; doming
    # on .f1 reads as round pillars -> LilyPads). The flat region has zero
    # gradient (no facets); only the thin bevel band rounds. fwedge is used, so
    # pass 2 runs (the bevel is worth it); .f1's sqrt is DCE'd.
    height = P.smoothstep(gap, gap + bevel, fw)               # gap floor -> rounded bevel -> flat top

    # grey stone with per-cell tone + grain; thin dark recessed gap
    stone    = P.smoothstep(gap * 0.6, gap + bevel * 0.5, fw)
    grain    = P.fbm(ctx.P_object * cell_scale * 6.0, 4)
    stonecol = base * P.mix(rgb(0.70, 0.70, 0.72), rgb(1.18, 1.14, 1.06), cell.cell)
    stonecol = stonecol * P.mix(0.80, 1.12, grain)
    albedo   = P.mix(gapc, stonecol, stone)

    # bump-only -> a touch of gloss so the rim relief reads; parallax -> matte
    rough = (P.mix(0.92, 0.78, cell.cell * stone) if pom_steps > 0
             else P.mix(0.70, 0.50, cell.cell * stone))
    self.surface(albedo=albedo, metallic=0.0, roughness=rough)
    self.displace(height, scale=bumps, parallax_steps=pom_steps, depth=relief)


__all__ = ["CobbleStone"]
