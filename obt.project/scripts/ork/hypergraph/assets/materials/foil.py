###############################################################################
# Foil — crinkled metal foil (gold leaf / silver / copper), GEOV2 ptex3d.
#
# As intuited: foil is just PERTURBED NORMALS + standard metallic PBR. The only
# special part is the crumple normal field (reusable `crumple` — ridged volumetric
# turbulence -> finite-diff bump -> shattered faceted normals); the rest is a
# metallic surface with a reflectance tint. Generic by design: change `metal_color`
# for gold / silver / copper / any foil. No projection (volumetric crumple), so it
# wraps any shape. The geometric specular AA in the template tames the sharp-crease
# sparkle.
#
#   from ork.hypergraph.assets.materials import Foil
#   mat = self.asset.Ptex3d("gold", dsl_class=Foil)                       # gold leaf
#   mat = self.asset.Ptex3d("foil", dsl_class=Foil,
#                           metal_color=vec3(0.97, 0.96, 0.91))           # silver
#
# Runtime ctx.param: scale (crumple frequency), wrinkle (crease depth / normal
#   strength), metal_color (reflectance tint), roughness, crease_rough, rough_var.
# Bake-time (ctor): octaves (crumple detail), pom_steps (parallax; 0 = bump).
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import crumple


class Foil(Ptex3d):
  def __init__(self, ctx, *, octaves=5, pom_steps=0,
               scale=6.0, wrinkle=0.01,
               metal_color=vec3(1.0, 0.78, 0.34),     # gold reflectance tint (F0)
               roughness=0.22, crease_rough=0.42, rough_var=0.08):
    scl   = ctx.param("scale",        scale)          # crumple frequency
    wrnk  = ctx.param("wrinkle",      wrinkle)        # crease depth -> normal strength
    tint  = ctx.param("metal_color",  metal_color)    # gold / silver / copper / ...
    rough = ctx.param("roughness",    roughness)      # facet polish
    crgh  = ctx.param("crease_rough", crease_rough)   # creases scuff rougher
    rvar  = ctx.param("rough_var",    rough_var)

    c     = crumple(ctx.P_object * scl, octaves)              # [0,1] crumple height
    micro = P.fbm(ctx.P_object * scl * 3.0, octaves)          # fine roughness break-up

    self.surface(
      albedo    = tint,                              # for a metal, base colour IS the F0 tint
      metallic  = 1.0,                               # foil is metal
      # shiny facets, rougher down in the creases, plus a little micro variation
      roughness = P.mix(crgh, rough, c) + (micro - 0.5) * rvar,
      transmission=0.5,
    )
    # the crumple drives the perturbed normals (that's the whole effect)
    self.displace(c, scale=wrnk, parallax_steps=pom_steps, depth=wrnk)


__all__ = ["Foil"]
