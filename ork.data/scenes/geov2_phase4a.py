#!/usr/bin/env ork.python
###############################################################################
# geov2_phase4a.py — cracked mud (GEOV2 ptex3d) as an ECS scene.
#
# The same DSL-synthesized cracked-mud surface as geov2_phase3a.py, but authored
# as a hypergraph ECS Scene so it runs under the ECS viewer (with its tonemapping
# / exposure / postfx controls) and round-trips through serialize→deserialize.
#
#   - cellular plates + recessed cracks (.fwedge), per-plate tone/roughness
#   - GEOV2 Phase 4 ANALYTIC BUMP: the cracks catch light (self.displace())
#   - bindable uniforms (base_color / crack / warp) ride PbrMaterialGenData
#
# The ptex3d asset wrapper materializes the DSL to a cached .fxv2 and packs a
# PbrMaterialGenData (shaderpath + param defaults); the IcoSphere asset bakes
# the .ogeo and references the material by name — the same round-trip path as
# mtl_showcase's PbrMaterial spheres.
#
# Run:
#   ork.scene.viewer.py geov2_phase4a
###############################################################################

from orkengine.core import lev2_pyexdir, vec3
from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.ptex3d import Ptex3d as PtexBase, P, rgb

lev2_pyexdir.addToSysPath()


###############################################################################
# Cracked mud — cellular plates, recessed cracks, analytic-bump relief.
# (Defined at module scope, so it needs explicit P / rgb / vec3 imports — the
# Scene's bare-name injection only applies inside Scene methods.)
###############################################################################

class CrackedMud(PtexBase):
  def __init__(self, ctx, *, cell_scale=5.0):
    base  = ctx.param("base_color", vec3(0.50, 0.33, 0.19))   # mud tone (bindable)
    crack = ctx.param("crack", 0.05)                          # crack width, cell units
    warp  = ctx.param("warp", 0.30)                           # plate irregularity
    bumps = ctx.param("bump_scale", 0.025)                      # bump strength (bindable)

    # domain-warp the cell coordinate so plates are organic, not lattice-regular.
    # 2-octave fbm (low-freq plate shape) — this warp runs in the surface AND in
    # every analytic-bump tap, so keep its octave count low.
    pc     = ctx.P_object * cell_scale
    wv     = P.vec3(P.fbm(pc * 0.6, 2), P.fbm(pc * 0.6 + 17.0, 2), P.fbm(pc * 0.6 + 41.0, 2))
    vcoord = pc + warp * wv                                    # warped cell coordinate
    cell   = P.voronoi(vcoord)                                 # .edge .fwedge .cell .cell2
    plate  = P.smoothstep(0.0, crack, cell.fwedge)            # 0 in crack, 1 on plate

    # per-plate earthy tone + fine dirt grain
    tint  = P.mix(rgb(0.70, 0.55, 0.34), rgb(1.05, 0.95, 0.74), cell.cell)
    mud   = base * tint
    grain = P.fbm(ctx.P_object * cell_scale * 5.0, 4)
    mud   = mud * P.mix(0.82, 1.15, grain)
    mud   = mud * P.mix(0.50, 1.0, P.smoothstep(0.0, crack * 3.0, cell.fwedge))

    crackcol = rgb(0.05, 0.035, 0.022)                        # deep crack bottom
    # SHINY for now — a matte surface has no specular response, so the analytic
    # bump (a normal perturbation) is invisible. Metal + low roughness makes the
    # relief read in the env reflection / highlights. (Revert to metallic=0,
    # roughness~0.9 for true matte mud once the bump is dialed in.)
    self.surface(
      albedo    = P.mix(crackcol, mud, plate),               # earthy -> cracked-bronze tint
      metallic  = 1.0,                                        # shiny metal (reveals the bump)
      roughness = P.mix(0.0, 0.18, cell.cell2 * plate),      # glossy; authored low (eff ~0.3-0.43)
    )

    # Phase 4 — ANALYTIC cellular relief (GEOV2 §18 option 1): plates raised,
    # cracks recessed, bump from ONE gradient-voronoi eval (no finite-difference
    # taps). ~3x cheaper than the general self.displace() path. Reuses vcoord.
    self.displace_cellular(vcoord, width=crack * 2.0, scale=bumps)


###############################################################################

class CrackedMudScene(Scene):

  def __init__(self):
    super().__init__()

    SG = self.scenegraph(
      preset            = "ForwardPBR",
      skybox_path       = "<ork_envmaps2>/blender_forest.xir",
      SkyboxIntensity   = 1.0,
      DiffuseIntensity  = 1.0,
      SpecularIntensity = 1.0,
      AmbientLight      = vec3(0.06))

    mat = self.asset.Ptex3d("mud_mtl", dsl_class=CrackedMud, cell_scale=5.0)
    drw = self.asset.IcoSphere("mud_sphere", radius=2.5, subdivisions=5, material=mat)

    self.entity(
      "mudball",
      transform  = {"translation": vec3(0, 0, 0)},
      components = [SG.component(nodes={"n": {"drawable": drw}})])
