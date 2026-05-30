###############################################################################
# Cracked-mud ptex3d surfaces — reusable GEOV2 procedural materials.
#
# A cellular cracked-mud surface (domain-warped voronoi plates, recessed cracks,
# earthy per-plate tone + dirt grain) in two relief flavors:
#
#   CrackedMud     — cheap ANALYTIC cellular bump (1 gradient-voronoi eval,
#                    GEOV2 §18 option 1). Damp-clay finish so the bump reads.
#   CrackedMudPOM  — PROCEDURAL parallax occlusion: real crack depth + self-
#                    occlusion. Matte. EXPENSIVE/TEMPORARY — marches the height
#                    `steps`x/fragment (GEOV2 §18.5; a baked-height texture
#                    replaces the march once auto-uv lands).
#
# These are ptex3d DSL surface classes — author a material from one via the
# ptex3d asset wrapper:
#
#   from ork.hypergraph.assets.materials import CrackedMud
#   mat = self.asset.Ptex3d("mud", dsl_class=CrackedMud, cell_scale=5.0)
#
# Knobs (all are ctor kwargs; the value sets the param default — settable
# statically from the scene and still runtime-bindable via mat.bindParam):
#   bake-time (folds into shader): steps (POM only)
#   runtime  (uniform, no recompile): cell_scale, base_color, crack, warp,
#            bump_scale, crack_color, relief (POM parallax depth)
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
  """Cracked mud with the cheap ANALYTIC cellular bump. Damp-clay finish (a touch
  of gloss) so the normal-perturbation relief reads — a pure bump is near-
  invisible on a fully matte surface. All knobs are runtime ctx.params (ctor
  kwargs set their defaults); nothing here is bake-time."""

  def __init__(self, ctx, *, cell_scale=5.0,
               base_color=_MUD["base_color"], crack=_MUD["crack"], warp=_MUD["warp"],
               bump_scale=_MUD["bump_scale"], crack_color=_MUD["crack_color"]):
    f = _cracked_mud_fields(ctx, cell_scale=cell_scale, base_color=base_color,
                            crack=crack, warp=warp, bump_scale=bump_scale,
                            crack_color=crack_color)
    self.surface(
      albedo    = f["albedo"],
      metallic  = 0.0,
      roughness = P.mix(0.45, 0.70, f["cell"].cell2 * f["plate"]),   # damp clay (bump reads)
    )
    self.displace_cellular(f["vcoord"], width=f["crack"] * 2.0, scale=f["bumps"])


class CrackedMudPOM(Ptex3d):
  """Cracked mud with PROCEDURAL parallax occlusion — true crack depth + self-
  occlusion, so it reads matte (no specular needed). Only `steps` (the march loop
  bound) is BAKE-TIME; every other knob is a runtime ctx.param. EXPENSIVE/
  TEMPORARY: re-marches the procedural height `steps`x/fragment (GEOV2 §18.5;
  swap for a baked-height texture sample once auto-uv lands)."""

  def __init__(self, ctx, *, steps=12, cell_scale=5.0,
               base_color=_MUD["base_color"], crack=_MUD["crack"], warp=_MUD["warp"],
               bump_scale=_MUD["bump_scale"], relief=0.06, crack_color=_MUD["crack_color"]):
    f = _cracked_mud_fields(ctx, cell_scale=cell_scale, base_color=base_color,
                            crack=crack, warp=warp, bump_scale=bump_scale,
                            crack_color=crack_color)
    self.surface(
      albedo    = f["albedo"],
      metallic  = 0.0,
      roughness = P.mix(0.95, 0.86, f["cell"].cell2 * f["plate"]),   # matte mud
    )
    relief = ctx.param("relief", relief)                     # parallax depth, object units
    height = P.smoothstep(0.0, f["crack"] * 2.0, f["cell"].fwedge)
    self.displace(height, scale=f["bumps"], parallax_steps=steps, depth=relief)


__all__ = ["CrackedMud", "CrackedMudPOM"]
