###############################################################################
# Plywood — procedural plywood (stacked cross-grain veneers), GEOV2 ptex3d.
#
# VOLUMETRIC: plies are thin slabs stacked along a thickness axis (object space).
# Each ply runs wood_grain with the grain direction rotated 90 deg from its
# neighbours (the real cross-ply construction), and a thin dark glue line sits at
# every ply boundary. Read on a surface: a face cut parallel to the plies shows one
# veneer's grain; a cut across them shows the stacked plies + glue lines — exactly
# like real plywood, with no triplanar.
#
#   from ork.hypergraph.assets.materials import Plywood
#   mat = self.asset.Ptex3d("ply", dsl_class=Plywood, ply_freq=6.0)
#
# Runtime ctx.param: scale, ply_freq, ply_axis, grain_axis_a/b, ring_freq,
#   ring_warp, warp_freq, ring_contrast, grain_freq, grain, glue, glue_color,
#   early_color, late_color, roughness. Bake-time (ctor): octaves, pom_steps.
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import wood_grain


class Plywood(Ptex3d):
  def __init__(self, ctx, *, octaves=4, pom_steps=0,
               scale=1.0, ply_freq=6.0, ply_axis=vec3(0.0, 0.0, 1.0),
               grain_axis_a=vec3(1.0, 0.0, 0.0), grain_axis_b=vec3(0.0, 1.0, 0.0),
               ring_freq=9.0, ring_warp=0.4, warp_freq=1.4, ring_contrast=2.0,
               grain_freq=45.0, grain=0.12,
               glue=0.06, glue_color=vec3(0.20, 0.12, 0.06),
               early_color=vec3(0.80, 0.64, 0.42), late_color=vec3(0.60, 0.44, 0.26),
               roughness=0.45, bump_scale=0.004, relief_depth=0.02):
    scl   = ctx.param("scale",         scale)
    plyf  = ctx.param("ply_freq",      ply_freq)        # plies per object unit
    plyax = ctx.param("ply_axis",      ply_axis)        # stacking (thickness) axis
    axA   = ctx.param("grain_axis_a",  grain_axis_a)
    axB   = ctx.param("grain_axis_b",  grain_axis_b)
    rfreq = ctx.param("ring_freq",     ring_freq)
    rwarp = ctx.param("ring_warp",     ring_warp)
    wfreq = ctx.param("warp_freq",     warp_freq)
    rcon  = ctx.param("ring_contrast", ring_contrast)
    gfreq = ctx.param("grain_freq",    grain_freq)
    grn   = ctx.param("grain",         grain)
    gluew = ctx.param("glue",          glue)            # glue-line width
    gluec = ctx.param("glue_color",    glue_color)
    early = ctx.param("early_color",   early_color)
    late  = ctx.param("late_color",    late_color)
    rough = ctx.param("roughness",     roughness)
    bumps = ctx.param("bump_scale",    bump_scale)
    depth = ctx.param("relief_depth",  relief_depth)

    p      = ctx.P_object * scl
    t      = P.dot(p, plyax) * plyf
    layer  = P.floor(t)
    within = P.fract(t)                                  # [0,1] within the ply
    parity = P.fract(layer * 0.5) * 2.0                  # 0,1 alternating plies
    axis   = P.mix(axA, axB, parity)                     # cross-grain per ply

    w   = wood_grain(p, axis, rfreq, rwarp, wfreq, octaves,
                     grain_freq=gfreq, ring_contrast=rcon)
    col = P.mix(early, late, w.band)
    col = col * P.mix(1.0 - grn, 1.0 + grn, w.grain)

    # glue line: dark thin band at each ply boundary (within near 0 or 1)
    edge     = P.min(within, 1.0 - within)
    glueline = 1.0 - P.smoothstep(0.0, gluew, edge)      # 1 at the boundary
    col      = P.mix(col, gluec, glueline)

    self.surface(
      albedo    = col,
      metallic  = 0.0,
      roughness = P.mix(rough, rough + 0.12, glueline),  # glue line a touch rougher
    )
    # veneer grain raised, glue line recessed
    self.displace(w.band * 0.5 - glueline, scale=bumps,
                  parallax_steps=pom_steps, depth=depth)


__all__ = ["Plywood"]
