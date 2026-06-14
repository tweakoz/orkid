###############################################################################
# animal_skin — procedural volumetric animal skins (GEOV2 ptex3d).
#
# All VOLUMETRIC: the patterns are 3D functions of the object-space point, so they
# wrap any shape with no triplanar, and every pattern edge is ANALYTIC-AA (built on
# the fwidth-AA spots / rosette / stripes primitives, so the markings don't sparkle
# at distance). Fur skins are flat colour (matte); reptile has scale relief.
#
#   from ork.hypergraph.assets.materials import Leopard, Cheetah, Zebra, Reptile
#   mat = self.asset.Ptex3d("leopard", dsl_class=Leopard)
#
# All pattern/colour knobs are runtime ctx.param. Bake-time (ctor): octaves,
# pom_steps (reptile relief).
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import spots, rosette, stripes, domain_warp


class Cheetah(Ptex3d):
  # solid round black spots on tan, evenly spread
  def __init__(self, ctx, *, octaves=4,
               spot_freq=6.0, spot_radius=0.30, warp=0.3, warp_freq=1.0,
               base_color=vec3(0.78, 0.58, 0.32), spot_color=vec3(0.06, 0.05, 0.04),
               fur=0.06, fur_freq=55.0, roughness=0.72):
    sfreq = ctx.param("spot_freq",   spot_freq)
    srad  = ctx.param("spot_radius", spot_radius)
    wamt  = ctx.param("warp",        warp)               # organic spot placement
    wfrq  = ctx.param("warp_freq",   warp_freq)
    base  = ctx.param("base_color",  base_color)
    spotc = ctx.param("spot_color",  spot_color)
    furA  = ctx.param("fur",         fur)
    rough = ctx.param("roughness",   roughness)

    p   = ctx.P_object
    s   = spots(p, sfreq, radius=srad, warp=wamt, warp_freq=wfrq, octaves=octaves)
    spv = s.mask * P.mix(0.8, 1.0, s.cell)                  # slight per-spot darkness var
    fur_n = P.fbm(p * fur_freq, octaves)
    col = base * P.mix(1.0 - furA, 1.0 + furA, fur_n)       # fur mottle
    self.surface(albedo=P.mix(col, spotc, spv), 
                 metallic=0.0, 
                 roughness=rough,
                 sheen_factor=1,
                 sheen_color=vec3(1,1,.9),
                 sheen_roughness=0.25)


class Leopard(Ptex3d):
  # broken-ring rosettes with a darker interior, on golden tan
  def __init__(self, ctx, *, octaves=4,
               rosette_freq=4.5,
               warp=0.3, warp_freq=1.0,
               base_color=vec3(0.82, 0.62, 0.30), fill_color=vec3(0.70, 0.50, 0.22),
               spot_color=vec3(0.16, 0.09, 0.04), fur=0.05, fur_freq=55.0, roughness=0.72):
    rfreq = ctx.param("rosette_freq", rosette_freq)
    base  = ctx.param("base_color",   base_color)
    wamt  = ctx.param("warp",        warp)               # organic spot placement
    wfrq  = ctx.param("warp_freq",   warp_freq)
    fillc = ctx.param("fill_color",   fill_color)
    spotc = ctx.param("spot_color",   spot_color)
    furA  = ctx.param("fur",          fur)
    rough = ctx.param("roughness",    roughness)

    p   = ctx.P_object
    r   = rosette(p, rfreq, warp=wamt, warp_freq=wfrq, octaves=octaves)
    fur_n = P.fbm(p * fur_freq, octaves)
    col = base * P.mix(1.0 - furA, 1.0 + furA, fur_n)
    col = P.mix(col, fillc, r.fill * 0.6)                   # darker rosette interior
    col = P.mix(col, spotc, r.ring)                         # dark broken ring
    self.surface(albedo=col, 
                 metallic=0.0, 
                 roughness=rough,
                 sheen_factor=1,
                 sheen_color=vec3(1,1,.9),
                 sheen_roughness=0.25)


class Zebra(Ptex3d):
  # sharp irregular black/white stripes
  def __init__(self, ctx, *, octaves=5,
               stripe_freq=5.0, stripe_dir=vec3(1.0, 0.25, 0.0), warp=0.55, warp_freq=1.6,
               light=vec3(0.90, 0.89, 0.85), dark=vec3(0.06, 0.05, 0.05), roughness=0.6):
    sfreq = ctx.param("stripe_freq", stripe_freq)
    sdir  = ctx.param("stripe_dir",  stripe_dir)
    warpA = ctx.param("warp",        warp)
    warpF = ctx.param("warp_freq",   warp_freq)
    lite  = ctx.param("light",       light)
    dark_ = ctx.param("dark",        dark)
    rough = ctx.param("roughness",   roughness)

    p = ctx.P_object
    s = stripes(p, sdir, sfreq, warpA, warpF, octaves)
    self.surface(albedo=P.mix(dark_, lite, s), 
                 metallic=0.0, 
                 roughness=rough,
                 sheen_factor=1,
                 sheen_color=vec3(1,1,.9),
                 sheen_roughness=0.25)


class Reptile(Ptex3d):
  # tessellated scales (dome relief) with per-scale colour + a banded blotch pattern
  def __init__(self, ctx, *, octaves=4, pom_steps=0,
               scale_freq=12.0, dome=1.6, border=0.04,
               warp=0.5, warp_freq=1.7,
               base_color=vec3(0.30, 0.42, 0.20), scale_var=0.18,
               blotch_freq=2.0, blotch=0.75, blotch_color=vec3(0.14, 0.18, 0.09),
               border_color=vec3(0.25, 0.35, 0.15), roughness=0.5, bump_scale=0.00625):
    sfreq = ctx.param("scale_freq",   scale_freq)
    domeA = ctx.param("dome",         dome)
    bordw = ctx.param("border",       border)
    wamt  = ctx.param("warp",         warp)               # voronoi domain-warp (organic scales)
    wfrq  = ctx.param("warp_freq",    warp_freq)          # warp scale (low = big wavy distortion)
    base  = ctx.param("base_color",   base_color)
    svar  = ctx.param("scale_var",    scale_var)
    bfreq = ctx.param("blotch_freq",  blotch_freq)
    blot  = ctx.param("blotch",       blotch)
    blotc = ctx.param("blotch_color", blotch_color)
    bordc = ctx.param("border_color", border_color)
    rough = ctx.param("roughness",    roughness)
    bumps = ctx.param("bump_scale",   bump_scale)

    p  = ctx.P_object
    # warp the voronoi INPUT (in scale-space, so `warp` reads as a fraction of a
    # scale) -> irregular organic scales instead of a too-regular tessellation
    pv = domain_warp(p * sfreq, wamt, wfrq, octaves)
    v  = P.voronoi(pv)
    fw = v.fwedge                                          # grazing-corrected border dist

    # per-scale colour + large banded blotches (snake markings)
    blotchv = P.smoothstep(0.5 - blot * 0.5, 0.5 + blot * 0.5, P.fbm(p * bfreq, octaves))
    col = base * P.mix(1.0 - svar, 1.0 + svar, v.cell)
    col = P.mix(col, blotc, blotchv)
    # dark recessed borders between scales (analytic-AA)
    bl  = 1.0 - P.smoothstep(0.0, bordw + P.fwidth(fw), fw)
    blh = 1.0 - P.smoothstep(0.0, bordw, fw)              # object-space, for the bump
    col = P.mix(col, bordc, bl)

    self.surface(
      albedo    = col,
      metallic  = 0.0,
      roughness = P.mix(rough, rough + 0.25, bl),         # shiny scale, matte groove
    )
    # each scale a raised dome, recessed at the borders
    dome_h = P.saturate(1.0 - v.f1) * (1.0 - blh)
    self.displace(dome_h * domeA, scale=bumps,
                  parallax_steps=pom_steps, depth=bump_scale)


__all__ = ["Cheetah", "Leopard", "Zebra", "Reptile"]
