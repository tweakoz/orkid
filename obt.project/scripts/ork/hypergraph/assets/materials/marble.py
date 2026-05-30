###############################################################################
# Marble — procedural veined mineral (marble / onyx / veined stone), GEOV2 ptex3d.
#
# VOLUMETRIC: built on vein_field — domain-warped turbulence read straight off the
# object-space point, so the veins run continuously THROUGH the block (like real
# stone cut from a quarry) with NO triplanar and NO projection seams; it wraps any
# shape. Two vein layers (coarse + fine) compose into a network; polished and flat
# by default (veins are colour, not relief — the iconic glossy marble), with
# opt-in relief for eroded/unpolished stone.
#
#   from ork.hypergraph.assets.materials import Marble
#   mat = self.asset.Ptex3d("marble", dsl_class=Marble)                 # white Carrara
#   mat = self.asset.Ptex3d("marble", dsl_class=Marble, relief=True)    # eroded veins
#
# Runtime ctx.param: vein_freq, vein_dir, warp, warp_freq, vein_sharpness,
#   vein_contrast, vein2_freq, vein2_dir, vein2_amount, base_color, vein_color,
#   vein_color2, mottle, mottle_freq, roughness, vein_roughness, vein_relief.
# Bake-time (ctor): octaves (turbulence detail), relief (emit bump), pom_steps.
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import vein_field


class Marble(Ptex3d):
  def __init__(self, ctx, *, octaves=5, relief=False, pom_steps=0,
               vein_freq=2.0, vein_dir=vec3(1.0, 0.35, 0.2), warp=0.8, warp_freq=1.4,
               vein_sharpness=3.0, vein_contrast=0.9,
               vein2_freq=6.5, vein2_dir=vec3(0.2, 1.0, 0.35), vein2_amount=0.45,
               base_color=vec3(0.86, 0.86, 0.83), vein_color=vec3(0.30, 0.31, 0.35),
               vein_color2=vec3(0.55, 0.49, 0.42), mottle=0.06, mottle_freq=4.0,
               roughness=0.12, vein_roughness=0.28, vein_relief=0.012, bump_scale=0.02):
    vfreq = ctx.param("vein_freq",      vein_freq)
    vdir  = ctx.param("vein_dir",       vein_dir)
    warpA = ctx.param("warp",           warp)          # domain-warp amount (sinuousness)
    warpF = ctx.param("warp_freq",      warp_freq)
    sharp = ctx.param("vein_sharpness", vein_sharpness)  # thin (high) vs broad (low) veins
    contr = ctx.param("vein_contrast",  vein_contrast)
    v2f   = ctx.param("vein2_freq",     vein2_freq)
    v2dir = ctx.param("vein2_dir",      vein2_dir)
    v2amt = ctx.param("vein2_amount",   vein2_amount)   # secondary fine-vein strength
    base  = ctx.param("base_color",     base_color)
    vcol  = ctx.param("vein_color",     vein_color)
    vcol2 = ctx.param("vein_color2",    vein_color2)     # tint for some of the fine veins
    mott  = ctx.param("mottle",         mottle)
    mfreq = ctx.param("mottle_freq",    mottle_freq)
    rough = ctx.param("roughness",      roughness)
    vrgh  = ctx.param("vein_roughness", vein_roughness)  # veins less polished than base
    vrel  = ctx.param("vein_relief",    vein_relief)

    p = ctx.P_object

    # two vein layers -> a network (coarse primary + fine secondary)
    v1 = vein_field(p, vdir,  vfreq, warpA,       warpF,       octaves, sharp)
    v2 = vein_field(p, v2dir, v2f,   warpA * 0.5, warpF * 2.0, octaves, sharp + 1.0)
    veins = P.saturate(v1 * contr + v2 * v2amt)

    # colour: base stone -> vein colour, with some fine veins tinted differently,
    # then a subtle large-scale mottle across the base
    col = P.mix(base, vcol, veins)
    col = P.mix(col, vcol2, P.saturate(v2 * v2amt) * 0.6)
    col = col * P.mix(1.0 - mott, 1.0 + mott, P.fbm(p * mfreq, octaves))

    self.surface(
      albedo    = col,
      metallic  = 0.0,                                  # dielectric stone
      roughness = P.mix(rough, vrgh, veins),            # polished base, softer veins
    )
    # polished marble is FLAT — only emit the (costly) bump when relief is opted in
    # (eroded/unpolished stone): the harder base stands proud, soft veins recess.
    if relief or pom_steps > 0:
      self.displace(1.0 - veins, scale=vrel if relief else bump_scale,
                    parallax_steps=pom_steps, depth=bump_scale)


__all__ = ["Marble"]
