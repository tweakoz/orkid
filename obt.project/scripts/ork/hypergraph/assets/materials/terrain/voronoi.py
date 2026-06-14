###############################################################################
# voronoi — a Solid base albedo MODULATED by Voronoi cell value (P.voronoi.cell =
# a flat per-cell random hash -> a stepped cell mosaic). Clone of solid.py + the
# noise term.
#
# `inp_xf` (mtx4) transforms the ctx.P domain the noise reads (baked); `octaves`
# fBm-stacks the cellular field. NB the flat cell value has hard inter-cell steps
# and NO band-limited (AA) form yet (only P.voronoi.fwedge — the cell EDGE — is
# AA-corrected), so it will alias at cell boundaries until an AA value is added.
#
#   from ork.hypergraph.assets.materials.terrain import Voronoi
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import domain_xf, fbm_stack
from orkengine.core import vec3, mtx4


class Voronoi(Ptex3d):
  def __init__(self, ctx, *,
               height_scale=2000.0,          # forwarded by the terrain wrapper; unused here
               albedo=vec3(1),
               metallic=0.0,
               roughness=1.0,
               inp_xf=None,                   # mtx4 domain transform (None = identity)
               octaves=1):                    # 1 = pure cellular; >1 fBm-stacks
    p   = domain_xf(ctx, ctx.P, "inp_xf", mtx4() if inp_xf is None else inp_xf)
    n   = fbm_stack(lambda q: P.voronoi(q).cell, p, octaves)  # flat per-cell value
    alb = ctx.param("albedo", albedo)                         # runtime-bindable base color
    self.surface(albedo=alb * n, metallic=metallic, roughness=roughness)


__all__ = ["Voronoi"]
