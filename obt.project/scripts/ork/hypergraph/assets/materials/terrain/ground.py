###############################################################################
# ground — natural terrain surface noise, scaled for ON-FOOT locomotion: the
# optical-flow reference that makes 10 mph READ as 10 mph. Two band-limited
# value-noise fBm bands over WORLD meters (ctx.P), both anti-aliased (fbm_aa
# fades sub-pixel octaves, so 2km sightlines stay clean):
#
#   PATCH band  — color mix albedo_lo <-> albedo_hi over `patch_m`-scale features
#                 (default 64m lattice, 4 octaves -> 64m..4m) — kills large-scale
#                 repetition across the whole map.
#   DETAIL band — brightness modulation over `detail_m`-scale features (default
#                 4m lattice, 6 octaves -> 4m..0.06m) — the near-field decimeter
#                 texture the eye tracks while walking.
#
# Domains ride domain_xf (runtime mtx rows) — rescale live, no recompile.
#
#   from ork.hypergraph.assets.materials.terrain.ground import Ground
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import domain_xf
from orkengine.core import vec3, quat, mtx4


def _scale_xf(s):
  return mtx4.composed(vec3(0, 0, 0), quat(), vec3(s, s, s))


class Ground(Ptex3d):
  def __init__(self, ctx, *,
               height_scale=2000.0,                # forwarded by the terrain wrapper; unused
               albedo_lo=vec3(0.30, 0.27, 0.22),   # dirt
               albedo_hi=vec3(0.43, 0.42, 0.30),   # dry grass
               roughness=0.92,
               detail=0.35,                        # detail-band brightness depth (0..~0.5)
               patch_m=64.0,                       # patch-band base wavelength (meters)
               detail_m=4.0,                       # detail-band base wavelength (meters)
               patch_octaves=4,
               detail_octaves=6,
               aa=1.0):
    p_patch = domain_xf(ctx, ctx.P, "gnd_patch", _scale_xf(1.0 / float(patch_m)))
    p_det   = domain_xf(ctx, ctx.P, "gnd_det", _scale_xf(1.0 / float(detail_m)))
    t = P.fbm_aa(p_patch, octaves=int(patch_octaves), aa=aa)   # ~[0,1] patch field
    d = P.fbm_aa(p_det, octaves=int(detail_octaves), aa=aa)    # ~[0,1] detail field

    alo = ctx.param("albedo_lo", albedo_lo)
    ahi = ctx.param("albedo_hi", albedo_hi)
    det = ctx.param("detail", float(detail))

    # brightness factor in [1-detail, 1+detail], centered at 1 for d=0.5
    bf  = 1.0 - det + d * (det * 2.0)
    alb = P.mix(alo, ahi, t) * bf
    # patches also vary micro-roughness a touch (grass slightly rougher than dirt)
    rgh = ctx.param("roughness", float(roughness)) * (0.92 + t * 0.08)
    self.surface(albedo=alb, metallic=0.0, roughness=rgh)


__all__ = ["Ground"]
