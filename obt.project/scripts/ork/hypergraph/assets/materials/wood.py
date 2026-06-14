###############################################################################
# Wood — procedural grained wood (teak / oak / pine / ...), GEOV2 ptex3d.
#
# VOLUMETRIC: built on wood_grain — growth rings are concentric cylinders around
# the grain axis (the trunk), defined in OBJECT SPACE and read at the surface, so
# the real cut pattern emerges automatically (cathedral arches on a flat cut,
# straight lines on a quarter cut) with NO triplanar and NO projection seams.
# `Wood` is the base; `Teak` / `Oak` / `Pine` are presets (just different defaults).
#
#   from ork.hypergraph.assets.materials import Teak, Oak, Pine
#   mat = self.asset.Ptex3d("oak", dsl_class=Oak)
#
# Runtime ctx.param: scale, axis, ring_freq, ring_warp, warp_freq, ring_contrast,
#   grain_freq, grain, pore, pore_freq, pore_thresh, early_color, late_color,
#   roughness, rough_grain, bump_scale. Bake-time (ctor): octaves, pom_steps.
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import wood_grain


class Wood(Ptex3d):
  PRESET = {}   # subclasses override defaults here

  def __init__(self, ctx, *, octaves=4, pom_steps=0, **kw):
    k = dict(
      scale=1.0, axis=vec3(0.0, 1.0, 0.0),
      ring_freq=8.0, ring_warp=0.5, warp_freq=1.2, ring_contrast=2.2,
      grain_freq=45.0, grain=0.12,
      pore=0.0, pore_freq=70.0, pore_thresh=0.72,
      early_color=vec3(0.55, 0.38, 0.21), late_color=vec3(0.32, 0.19, 0.09),
      roughness=0.42, rough_grain=0.12, bump_scale=0.004, relief_depth=0.02,
    )
    k.update(self.PRESET)
    k.update(kw)

    scl   = ctx.param("scale",         k["scale"])
    axis  = ctx.param("axis",          k["axis"])           # grain direction (trunk axis)
    rfreq = ctx.param("ring_freq",     k["ring_freq"])
    rwarp = ctx.param("ring_warp",     k["ring_warp"])      # ring waviness
    wfreq = ctx.param("warp_freq",     k["warp_freq"])
    rcon  = ctx.param("ring_contrast", k["ring_contrast"])  # earlywood/latewood sharpness
    gfreq = ctx.param("grain_freq",    k["grain_freq"])
    grn   = ctx.param("grain",         k["grain"])          # long-grain streak strength
    pore  = ctx.param("pore",          k["pore"])           # ring-porous pores (oak)
    pfreq = ctx.param("pore_freq",     k["pore_freq"])
    pthr  = ctx.param("pore_thresh",   k["pore_thresh"])
    early = ctx.param("early_color",   k["early_color"])
    late  = ctx.param("late_color",    k["late_color"])
    rough = ctx.param("roughness",     k["roughness"])
    rgrn  = ctx.param("rough_grain",   k["rough_grain"])
    bumps = ctx.param("bump_scale",    k["bump_scale"])
    depth = ctx.param("relief_depth",  k["relief_depth"])

    p = ctx.P_object * scl
    w = wood_grain(p, axis, rfreq, rwarp, wfreq, octaves,
                   grain_freq=gfreq, ring_contrast=rcon)

    # colour: earlywood -> latewood by the ring band, modulated by long-grain
    # streaks, then ring-porous pores (open vessels) darken the earlywood
    col   = P.mix(early, late, w.band)
    col   = col * P.mix(1.0 - grn, 1.0 + grn, w.grain)
    # ring-porous pores: an analytic-AA threshold (smoothstep over the noise's pixel
    # footprint) instead of a hard step(), so the open vessels don't sparkle.
    pf    = P.fbm(p * pfreq, octaves)
    pe    = P.fwidth(pf) + 0.001
    pores = P.smoothstep(pthr - pe, pthr + pe, pf) * (1.0 - w.band) * pore
    col   = col * (1.0 - pores * 0.55)

    self.surface(
      albedo    = col,
      metallic  = 0.0,
      # latewood reads a touch rougher; the long grain breaks it up
      roughness = P.mix(rough, rough + rgrn, w.band) + (w.grain - 0.5) * 0.10,
    )
    # subtle relief: harder latewood stands proud, pores recess
    self.displace(w.band * 0.7 + pores, scale=bumps,
                  parallax_steps=pom_steps, depth=depth)


class Teak(Wood):
  # golden-brown, straight oily grain, satin finish, no open pores
  PRESET = dict(
    early_color=vec3(0.60, 0.42, 0.22)*0.5, late_color=vec3(0.40, 0.26, 0.13)*0.5,
    ring_freq=6.0, ring_warp=0.35, ring_contrast=1.8,
    grain=0.10, grain_freq=40.0, pore=0.0, roughness=0.24, rough_grain=0.50)


class Oak(Wood):
  # light tan, prominent ring-porous grain (open pores), cathedral arches
  PRESET = dict(
    early_color=vec3(0.66, 0.52, 0.34), late_color=vec3(0.46, 0.34, 0.20),
    ring_freq=7.0, ring_warp=0.6, ring_contrast=2.4,
    grain=0.14, grain_freq=50.0, pore=0.1, pore_freq=80.0, pore_thresh=0.70,
    roughness=0.26, rough_grain=0.14)


class Pine(Wood):
  # pale yellow, close rings with strong dark latewood, soft (knots not modeled)
  PRESET = dict(
    early_color=vec3(0.82, 0.67, 0.42), late_color=vec3(0.56, 0.38, 0.18),
    ring_freq=11.0, ring_warp=0.5, ring_contrast=3.2,
    grain=0.10, grain_freq=46.0, pore=0.0, roughness=0.50, rough_grain=0.12)


__all__ = ["Wood", "Teak", "Oak", "Pine"]
