###############################################################################
# worleyf1 — a Solid base albedo MODULATED by Worley cellular F1 (P.voronoi.f1 =
# distance to the nearest cell site -> smooth rounded domes). Clone of solid.py +
# the noise term.
#
# `inp_xf` (mtx4) transforms the ctx.P domain the noise reads (baked); `octaves`
# fBm-stacks the cellular field. NB the F1 VALUE has no band-limited (AA) form yet
# (only P.voronoi.fwedge — the cell EDGE — is AA-corrected); f1 is smooth so it
# aliases far less than the flat cell value (cf. voronoi.py).
#
#   from ork.hypergraph.assets.materials.terrain import WorleyF1
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import domain_xf, fbm_stack
from orkengine.core import vec3, mtx4


class WorleyF1(Ptex3d):
  def __init__(self, ctx, *,
               height_scale=2000.0,          # forwarded by the terrain wrapper; unused here
               albedo=vec3(1),
               metallic=0.0,
               roughness=1.0,
               inp_xf=None,                   # mtx4 domain transform (None = identity)
               octaves=1):                    # 1 = pure cellular; >1 fBm-stacks
    p   = domain_xf(ctx, ctx.P, "inp_xf", mtx4() if inp_xf is None else inp_xf)
    n   = fbm_stack(lambda q: P.voronoi(q).f1, p, octaves)   # cellular F1 (distance to site)
    alb = ctx.param("albedo", albedo)                        # runtime-bindable base color
    self.surface(albedo=alb * n, metallic=metallic, roughness=roughness)


__all__ = ["WorleyF1"]
