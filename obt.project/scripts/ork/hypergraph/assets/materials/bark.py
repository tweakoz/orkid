###############################################################################
# Bark — procedural DECIDUOUS tree bark (oak/maple-like) for the L-system tree:
# deep VERTICAL furrows running up the trunk, grayish-brown, rougher in the
# crevices, with a finer bark grain on top. Built in OBJECT SPACE so the furrows
# follow the trunk with no projection seams; squashing the noise lattice along the
# trunk axis (Y) gives the vertical coherence, a domain warp makes the furrows
# wander, and the furrow depth drives a bump relief. All colors authored in HSV.
#
#   mat = self.asset.Ptex3d("bark", dsl_class=Bark)
###############################################################################
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import domain_warp
from ork.hypergraph.colors import hsv


class Bark(Ptex3d):
  def __init__(self, ctx, *, octaves=4, **kw):
    k = dict(
      scale        = 2.3,                    # furrow frequency (around the trunk)
      squash       = 0.26,                   # Y-squash -> furrows run VERTICALLY up the trunk
      warp         = 0.40,                   # domain-warp amount (furrows wander, not ruler-straight)
      contrast     = 0.4,                    # furrow vs ridge sharpness
      grain        = 0.18,                   # fine bark-grain strength on top of the furrows
      ridge_color  = hsv(30,  0.36, 0.20),   # sun-faced ridge — lighter grayish-tan
      furrow_color = hsv(22,  0.52, 0.12),   # shadowed crevice — dark brown
      rough_base   = 0.88,                   # bark is rough ...
      rough_var    = 0.10,                   # ... and rougher down in the furrows
      relief       = 0.025,                  # bump depth (furrows recess)
    )
    AA = 0.5
    k.update(kw)
    scl   = ctx.param("scale",        k["scale"])
    squash = ctx.param("squash",      k["squash"])
    warp  = ctx.param("warp",         k["warp"])
    con   = ctx.param("contrast",     k["contrast"])
    grn   = ctx.param("grain",        k["grain"])
    ridge = ctx.param("ridge_color",  k["ridge_color"])
    furr  = ctx.param("furrow_color", k["furrow_color"])
    rough = ctx.param("bark_roughness", k["rough_base"])
    rvar  = ctx.param("rough_var",    k["rough_var"])
    relf  = ctx.param("relief",       k["relief"])

    p     = ctx.P_object * scl
    p     = P.vec3(p.x, p.y * squash, p.z)             # squash Y -> vertical streaks (RUNTIME param)
    p     = domain_warp(p, warp, 1.0, octaves)         # wandering furrows
    # BANDLIMITED fbm (footprint-band-limited / procedural mip): sub-pixel octaves fade to their mean
    # instead of aliasing, so the furrows + grain don't sparkle on distant/grazing trunks (the worst
    # offender is the high-freq grain at scl*7). aa=1.0 = neutral; raise it for softer-at-distance.
    f     = P.fbm_aa(p, octaves, aa=AA)               # [0,1] streaky base
    b     = P.clamp((f - 0.5) * con + 0.5, 0.0, 1.0)   # sharpen into furrow (low) / ridge (high)
    grain = P.fbm_aa(ctx.P_object * (scl * 7.0), octaves, aa=AA)   # fine bark grain (un-squashed)
    shade = P.clamp(b + (grain - 0.5) * grn, 0.0, 1.0)

    self.surface(
      albedo    = P.mix(furr, ridge, shade),
      metallic  = 0.0,
      roughness = rough + (1.0 - shade) * rvar,        # furrows read rougher
    )
    self.displace(b, scale=relf, parallax_steps=0, depth=relf)   # furrow bump relief (furrows recess)


__all__ = ["Bark"]
