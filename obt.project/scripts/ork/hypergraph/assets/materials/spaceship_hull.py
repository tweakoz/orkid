###############################################################################
# SpaceshipHull — procedural sci-fi hull plating, mappable onto ANY shape (ptex3d).
#
# VOLUMETRIC: built on box_partition — a native 3D recursive box subdivision.
# The panel layout is a property of OBJECT SPACE, read directly at the surface
# point, so there is NO triplanar and NO projection seams — the plating is
# continuous over any shape (planes / spheres / cubes / irregular hulls). The
# finite-diff bump already works in 3D, so no projection is needed for relief.
# Panels (coarse box levels) + greebles (finer box levels) come from one tree.
# The material maps it onto painted-metal PBR: recessed seams + greebles (bump
# or parallax), per-panel paint jitter, edges chipped to bare metal, analytic-AA
# seam lines, and cavity AO in the grooves / greeble bases.
#
#   from ork.hypergraph.assets.materials import SpaceshipHull
#   mat = self.asset.Ptex3d("hull", dsl_class=SpaceshipHull, greeble_cover=0.3)
#   mat = self.asset.Ptex3d("hull", dsl_class=SpaceshipHull, pom_steps=24)  # parallax
#
# Runtime ctx.param: scale, seam, bump_scale, relief_depth, greeble_cover (0 =
#   clean panels), greeble_height, greeble_bevel, seam_ao, greeble_ao,
#   panel_color, metal_color, roughness, rough_jitter, wear, metallic (base
#   panel metalness). Bake-time (ctor):
#   levels (panel count), sublevels (greeble depth), pom_steps (parallax march; 0
#   = bump only).
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import box_partition


class SpaceshipHull(Ptex3d):
  def __init__(self, ctx, *, levels=4, sublevels=3, scale=2.5, pom_steps=0,
               seam=0.015, bump_scale=0.02, relief_depth=0.04,
               greeble_cover=0.30, greeble_height=0.45, greeble_bevel=0.05,
               seam_ao=0.6, greeble_ao=0.5,
               panel_color=vec3(0.42, 0.44, 0.46), metal_color=vec3(0.25, 0.26, 0.28),
               roughness=0.5, rough_jitter=0.14, wear=0.5, metallic=0.0):
    scl   = ctx.param("scale",          scale)       # RUNTIME panel scale (re-tile live)
    seamw = ctx.param("seam",           seam)
    bumps = ctx.param("bump_scale",     bump_scale)
    depth = ctx.param("relief_depth",   relief_depth)     # parallax relief depth (object units)
    cover = ctx.param("greeble_cover",  greeble_cover)   # greeble density (0 = clean panels)
    gh    = ctx.param("greeble_height", greeble_height)  # greeble bump strength
    bevel = ctx.param("greeble_bevel",  greeble_bevel)   # greeble edge bevel width
    saoP  = ctx.param("seam_ao",        seam_ao)         # cavity AO in the seam grooves
    gaoP  = ctx.param("greeble_ao",     greeble_ao)      # cavity AO at greeble bases
    paint = ctx.param("panel_color",    panel_color)
    metal = ctx.param("metal_color",    metal_color)
    rough = ctx.param("roughness",      roughness)
    rjit  = ctx.param("rough_jitter",   rough_jitter)
    wearp = ctx.param("wear",           wear)
    metb  = ctx.param("metallic",       metallic)        # base metalness of the painted panels

    # ONE volumetric box tree, read straight off the object-space point — no
    # projection. .x=panel id, .y=panel seam dist, .z=greeble id, .w=greeble dist
    bp     = box_partition(ctx.P_object * scl, ctx.N_object, levels, sublevels)
    pid    = bp.x
    pedge  = bp.y
    gid    = bp.z
    gedge  = bp.w

    # object-space seam mask drives the (re-evaluable) bump; a SEPARATE analytic-AA
    # mask drives the visuals — fwidth(pedge) is the seam's per-pixel footprint, so
    # the smoothstep band tracks pixel size and the edge never aliases. (fwidth is
    # kept out of the displace path so the bump stays object-stable.)
    panel  = P.smoothstep(0.0, seamw, pedge)                       # bump groove
    aa     = P.fwidth(pedge)
    panelV = P.smoothstep(seamw - aa, seamw + aa, pedge)           # antialiased visual seam
    r2     = P.fract(pid * 97.13)                # decorrelated per-panel random
    wear_m = (1.0 - panelV) * wearp              # paint chipped to metal along seams

    # greebles: a fraction `cover` of fine cells raised, beveled, kept off seams
    greeb  = P.step(1.0 - cover, gid) * P.smoothstep(0.0, bevel, gedge) * panel

    # CAVITY AO (baked into albedo — the material AO channel is still inert in the
    # forward path): darken the seam grooves, and a thin band at each greeble base.
    gband  = greeb * (1.0 - greeb) * 4.0                           # peaks in the greeble bevel
    ao     = P.mix(1.0 - saoP, 1.0, panelV) * (1.0 - gaoP * gband)

    paint_v = paint * P.mix(0.85, 1.15, pid)     # per-panel paint-batch jitter
    self.surface(
      albedo    = P.mix(paint_v, metal, wear_m) * ao,             # cavity AO in the recesses
      metallic  = P.mix(metb, 1.0, wear_m),                       # base metalness; worn edges full metal
      roughness = P.mix(rough + rjit * (r2 - 0.5), 0.42, wear_m),  # per-panel satin; worn metal smoother
    )
    # panels raised over recessed seams, greebles raised on top. pom_steps>0 adds
    # real parallax-occlusion (bake-time march bound); 0 = bump only.
    self.displace(panel + greeb * gh, scale=bumps,
                  parallax_steps=pom_steps, depth=depth)


__all__ = ["SpaceshipHull"]
