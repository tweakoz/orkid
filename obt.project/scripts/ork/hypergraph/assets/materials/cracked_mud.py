###############################################################################
# Cracked-mud ptex3d surfaces — reusable GEOV2 procedural materials.
#
# A cellular cracked-mud surface (domain-warped voronoi plates, recessed cracks,
# earthy per-plate tone + dirt grain). ONE class, `pom_steps` gates the relief:
#   pom_steps == 0 (default) — bump only (damp-clay finish so the bump reads).
#   pom_steps  > 0           — + PROCEDURAL parallax occlusion: real crack depth
#                              + self-occlusion (matte). EXPENSIVE/TEMPORARY —
#                              marches the height `pom_steps`x/fragment (GEOV2
#                              §18.5; a baked-height texture replaces the march
#                              once auto-uv lands).
#
# A ptex3d DSL surface class — author a material from it via the asset wrapper:
#
#   from ork.hypergraph.assets.materials import CrackedMud
#   mat = self.asset.Ptex3d("mud", dsl_class=CrackedMud, cell_scale=5.0)          # bump
#   mat = self.asset.Ptex3d("mud", dsl_class=CrackedMud, pom_steps=12)            # + parallax
#
# Knobs (all are ctor kwargs; the value sets the param default — settable
# statically from the scene and still runtime-bindable via mat.bindParam):
#   bake-time (folds into shader): pom_steps
#   runtime  (uniform, no recompile): cell_scale, base_color, crack, warp,
#            bump_scale, crack_color, relief (parallax depth)
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P, rgb


def _cracked_mud_fields(ctx, *, cell_scale, base_color, crack, warp, bump_scale, crack_color):
  """Build the shared cracked-mud surface fields (the relief technique is added
  by the concrete class). Each value is a ctx.param (runtime uniform) whose
  default the caller passes — so nothing here recompiles when tweaked."""
  cell_scale = ctx.param("cell_scale", cell_scale)         # cell density (runtime, no recompile)
  base   = ctx.param("base_color",  base_color)            # mud tone
  crack  = ctx.param("crack",       crack)                 # crack width, cell units
  warp   = ctx.param("warp",        warp)                  # plate irregularity
  bumps  = ctx.param("bump_scale",  bump_scale)            # relief strength
  crackc = ctx.param("crack_color", crack_color)           # deep-crack tint

  # domain-warp the cell coordinate -> organic plates (low-octave: runs hot in
  # the bump/march taps).
  pc     = ctx.P_object * cell_scale
  wv     = P.vec3(P.fbm(pc * 0.6, 2), P.fbm(pc * 0.6 + 17.0, 2), P.fbm(pc * 0.6 + 41.0, 2))
  vcoord = pc + warp * wv
  cell   = P.voronoi(vcoord)
  plate  = P.smoothstep(0.0, crack, cell.fwedge)           # 0 in crack, 1 on plate

  tint  = P.mix(rgb(0.70, 0.55, 0.34), rgb(1.05, 0.95, 0.74), cell.cell)
  mud   = base * tint
  grain = P.fbm(ctx.P_object * cell_scale * 5.0, 4)
  mud   = mud * P.mix(0.82, 1.15, grain)
  mud   = mud * P.mix(0.50, 1.0, P.smoothstep(0.0, crack * 3.0, cell.fwedge))  # darken crack lips

  albedo = P.mix(crackc, mud, plate)                       # deep crack -> earthy plate
  return dict(albedo=albedo, cell=cell, vcoord=vcoord, crack=crack,
              bumps=bumps, plate=plate)


# shared param defaults so the two classes stay in sync
_MUD = dict(base_color=vec3(0.50, 0.33, 0.19), crack=0.05, warp=0.30,
            bump_scale=0.025, crack_color=vec3(0.05, 0.035, 0.022))


class CrackedMud(Ptex3d):
  """Cracked mud — cellular plates, recessed cracks. ONE class; `pom_steps`
  (bake-time) gates the relief: pom_steps == 0 (default) -> bump only (damp-clay
  finish so the normal-perturbation reads); pom_steps > 0 -> + PROCEDURAL parallax
  occlusion (matte; depth reads geometrically — EXPENSIVE/TEMPORARY march, GEOV2
  §18.5). The displacement is a COMPOSABLE SurfNode expression, so heights add
  like any other expression (cracks + a sine wave, etc.). All knobs are runtime
  ctx.params except `pom_steps`. (Interim: the bump is finite-difference; a
  `grad()` autodiff pass will make it analytic + still composable.)"""

  def __init__(self, ctx, *, pom_steps=0, cell_scale=5.0,
               base_color=_MUD["base_color"], crack=_MUD["crack"], warp=_MUD["warp"],
               bump_scale=_MUD["bump_scale"], relief=0.06, crack_color=_MUD["crack_color"]):
    f = _cracked_mud_fields(ctx, cell_scale=cell_scale, base_color=base_color,
                            crack=crack, warp=warp, bump_scale=bump_scale,
                            crack_color=crack_color)
    # bump-only needs some gloss to show the normal; parallax shows depth -> matte
    rough = (P.mix(0.95, 0.86, f["cell"].cell2 * f["plate"]) if pom_steps > 0
             else P.mix(0.45, 0.70, f["cell"].cell2 * f["plate"]))
    self.surface(albedo=f["albedo"], metallic=0.0, roughness=rough)
    # COMPOSABLE height expression (1 = plate top, 0 = deep crack) — add your own
    # fields like any expression, e.g. cracks + a sine swell:
    #   height + 0.15 * P.sin(ctx.P_object.x * 8.0)
    height = P.smoothstep(0.0, f["crack"] * 2.0, f["cell"].fwedge)
    self.displace(height, scale=f["bumps"], parallax_steps=pom_steps, depth=relief)


__all__ = ["CrackedMud"]
