###############################################################################
# fbm — a Solid base albedo MODULATED by value-noise fBm (band-limited: P.fbm_aa,
# so it anti-aliases as it goes sub-pixel). Clone of solid.py + the noise term.
#
# `inp_xf` (mtx4) transforms the ctx.P domain the noise reads (scale/rotate the
# noise frame; baked); `octaves` sets the fBm depth.
#
#   from ork.hypergraph.assets.materials.terrain import Fbm
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import domain_xf
from orkengine.core import vec3, mtx4


class Fbm(Ptex3d):
  def __init__(self, ctx, *,
               height_scale=2000.0,          # forwarded by the terrain wrapper; unused here
               albedo=vec3(1),
               metallic=0.0,
               roughness=1.0,
               inp_xf=None,                   # mtx4 domain transform (None = identity)
               octaves=4,
               aa = 1.0):
    p   = domain_xf(ctx, ctx.P, "inp_xf", mtx4() if inp_xf is None else inp_xf)
    n   = P.fbm_aa(p, octaves=int(octaves), aa=aa)   # value-noise fBm, band-limited, ~[0,1]
    alb = ctx.param("albedo", albedo)                 # runtime-bindable base color
    self.surface(albedo=alb * n, metallic=metallic, roughness=roughness)


__all__ = ["Fbm"]
