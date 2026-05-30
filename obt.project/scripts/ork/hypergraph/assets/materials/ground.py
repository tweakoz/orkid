###############################################################################
# ground — procedural volumetric Dirt + Grass (GEOV2 ptex3d).
#
# Organic ground materials composed from the function library (fbm, voronoi spots,
# domain_warp, streaks). VOLUMETRIC: every field is a 3D function of the object
# point, so it wraps any shape with no triplanar. Built for VARIATION — two-tone
# colour ranges + patch/clump/grain noises + wetness/dryness + scatter, all runtime.
#
#   from ork.hypergraph.assets.materials import Dirt, Grass
#   mat = self.asset.Ptex3d("dirt", dsl_class=Dirt, wet=0.4)
#   mat = self.asset.Ptex3d("grass", dsl_class=Grass, dryness=0.3)
#
# Bake-time (ctor): octaves, pom_steps.
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import spots, streaks, domain_warp

_PI = 3.14159265359


class Dirt(Ptex3d):
  # mottled soil with scattered pebbles, lumpy relief, optional wetness
  def __init__(self, ctx, *, octaves=5, pom_steps=0, scale=2.0,
               soil_color=vec3(0.34, 0.24, 0.15), soil_color2=vec3(0.20, 0.13, 0.08),
               color_var=0.35, mottle_freq=2.5, grain_freq=28.0,
               pebble_freq=14.0, pebble=0.45, pebble_radius=0.27, pebble_warp=0.4,
               pebble_color=vec3(0.46, 0.42, 0.38), pebble_color2=vec3(0.30, 0.26, 0.20),
               wet=0.0, lump=0.5, roughness=0.9, bump_scale=0.025, relief_depth=0.04):
    scl   = ctx.param("scale",         scale)
    soil1 = ctx.param("soil_color",    soil_color)
    soil2 = ctx.param("soil_color2",   soil_color2)
    cvar  = ctx.param("color_var",     color_var)
    mfreq = ctx.param("mottle_freq",   mottle_freq)
    gfreq = ctx.param("grain_freq",    grain_freq)
    pfreq = ctx.param("pebble_freq",   pebble_freq)
    pamt  = ctx.param("pebble",        pebble)          # fraction of cells with a pebble
    prad  = ctx.param("pebble_radius", pebble_radius)
    pwarp = ctx.param("pebble_warp",   pebble_warp)
    peb1  = ctx.param("pebble_color",  pebble_color)
    peb2  = ctx.param("pebble_color2", pebble_color2)
    wetA  = ctx.param("wet",           wet)             # 0 dry .. 1 wet (darker, glossier)
    lumpA = ctx.param("lump",          lump)
    rough = ctx.param("roughness",     roughness)
    bumps = ctx.param("bump_scale",    bump_scale)
    depth = ctx.param("relief_depth",  relief_depth)

    p      = ctx.P_object * scl
    mottle = P.fbm(p * mfreq, octaves)                  # large soil patches
    grain  = P.fbm(p * gfreq, octaves)                  # fine grain
    soil   = P.mix(soil1, soil2, mottle)
    soil   = soil * P.mix(1.0 - cvar, 1.0 + cvar, grain)

    # pebbles: scattered voronoi discs (only a fraction of cells), own colour
    sp     = spots(p, pfreq, radius=prad, warp=pwarp, warp_freq=1.0, octaves=octaves)
    peb    = sp.mask * P.step(1.0 - pamt, sp.cell)
    pebcol = P.mix(peb1, peb2, sp.cell2)
    col    = P.mix(soil, pebcol, peb)

    # wetness: darken + drop roughness (pools in the low spots)
    col    = col * P.mix(1.0, 0.55, wetA)
    self.surface(
      albedo    = col,
      metallic  = 0.0,
      roughness = P.mix(rough, 0.30, wetA) - peb * 0.1,   # pebbles a touch glossier
    )
    # lumpy soil + raised pebbles
    self.displace((mottle - 0.5) * lumpA + grain * 0.15 + peb * 0.6,
                  scale=bumps, parallax_steps=pom_steps, depth=depth)


class Grass(Ptex3d):
  # clumpy lawn: green tone variation, dry patches, blade streaks, dirt showing through
  def __init__(self, ctx, *, octaves=5, pom_steps=0, scale=3.0,
               grass_color=vec3(0.20, 0.38, 0.10), grass_color2=vec3(0.34, 0.46, 0.14),
               dry_color=vec3(0.55, 0.50, 0.22), dirt_color=vec3(0.26, 0.18, 0.10),
               clump_freq=10.0, patch_freq=2.0, dryness=0.25,
               blade_dir=vec3(0.2, 1.0, 0.1), blade_freq=70.0, blade_squash=0.07, blade=0.3,
               dirt=0.15, dirt_freq=3.0, roughness=0.72, bump_scale=0.012, relief_depth=0.03):
    scl   = ctx.param("scale",        scale)
    grn1  = ctx.param("grass_color",  grass_color)
    grn2  = ctx.param("grass_color2", grass_color2)
    dryc  = ctx.param("dry_color",    dry_color)
    dirtc = ctx.param("dirt_color",   dirt_color)
    cfreq = ctx.param("clump_freq",   clump_freq)
    pfreq = ctx.param("patch_freq",   patch_freq)
    dry   = ctx.param("dryness",      dryness)          # 0 lush .. 1 dry/yellow
    bdir  = ctx.param("blade_dir",    blade_dir)
    bfreq = ctx.param("blade_freq",   blade_freq)
    blad  = ctx.param("blade",        blade)            # blade-streak strength
    dirtA = ctx.param("dirt",         dirt)             # bare-dirt patches
    dfreq = ctx.param("dirt_freq",    dirt_freq)
    rough = ctx.param("roughness",    roughness)
    bumps = ctx.param("bump_scale",   bump_scale)
    depth = ctx.param("relief_depth", relief_depth)

    p      = ctx.P_object * scl
    clump  = P.fbm(p * cfreq, octaves)                  # green tone variation
    patch  = P.fbm(p * pfreq, octaves)                  # lush vs dry patches
    blades = streaks(p, bdir, bfreq, squash=blade_squash, octaves=octaves)

    col = P.mix(grn1, grn2, clump)
    col = P.mix(col, dryc, P.smoothstep(0.55 - dry * 0.5, 0.95 - dry * 0.5, patch))  # dry patches
    col = col * P.mix(1.0 - blad, 1.0 + blad, blades)   # blade streaks

    # bare dirt showing through in worn patches
    dirtv = P.smoothstep(0.62, 0.78, P.fbm(p * dfreq, octaves)) * dirtA
    col   = P.mix(col, dirtc, dirtv)

    self.surface(
      albedo    = col,
      metallic  = 0.0,
      roughness = P.mix(rough, 0.85, dirtv),            # dirt patches more matte
    )
    # fine blade relief, flattened where it's bare dirt
    self.displace(blades * (1.0 - dirtv), scale=bumps,
                  parallax_steps=pom_steps, depth=depth)


class Gravel(Ptex3d):
  # densely packed warped voronoi stones, varied colour, recessed gaps
  def __init__(self, ctx, *, octaves=4, pom_steps=0, scale=2.5,
               stone_freq=8.0, warp=0.4, warp_freq=1.0, gap=0.05, dome=1.7,
               stone_color=vec3(0.50, 0.46, 0.40), stone_color2=vec3(0.32, 0.27, 0.22),
               stone_color3=vec3(0.42, 0.34, 0.25), color_var=0.30,
               grain=0.10, grain_freq=30.0, gap_color=vec3(0.11, 0.09, 0.07),
               roughness=0.85, bump_scale=0.003, relief_depth=0.00):
    scl   = ctx.param("scale",        scale)
    sfreq = ctx.param("stone_freq",   stone_freq)
    wamt  = ctx.param("warp",         warp)             # stone-shape irregularity
    wfrq  = ctx.param("warp_freq",    warp_freq)
    gapw  = ctx.param("gap",          gap)
    domeA = ctx.param("dome",         dome)
    st1   = ctx.param("stone_color",  stone_color)
    st2   = ctx.param("stone_color2", stone_color2)
    st3   = ctx.param("stone_color3", stone_color3)
    cvar  = ctx.param("color_var",    color_var)
    grn   = ctx.param("grain",        grain)
    gfreq = ctx.param("grain_freq",   grain_freq)
    gapc  = ctx.param("gap_color",    gap_color)
    rough = ctx.param("roughness",    roughness)
    bumps = ctx.param("bump_scale",   bump_scale)
    depth = ctx.param("relief_depth", relief_depth)

    p  = ctx.P_object * scl
    pv = domain_warp(p * sfreq, wamt, wfrq, octaves)
    v  = P.voronoi(pv)
    fw = v.fwedge

    # ANALYTIC AA. The per-cell stone colour / gaps / relief are DISCRETE per cell
    # (hard discontinuities at every stone edge) — the worst aliasers, and not edge-
    # AA-able. Instead fade them toward their averages as the cells go sub-pixel:
    #   lod  = cells-per-pixel from the voronoi-input footprint (1 = sub-pixel)
    #   glod = the (higher-frequency) grain's own sub-pixel fade
    lod  = P.smoothstep(0.35, 0.85, P.length(P.fwidth(pv)))
    glod = P.smoothstep(0.35, 0.85, gfreq * P.length(P.fwidth(p)))
    mean_stone = (st1 + st2 + st3) * (1.0 / 3.0)

    # 3 rock tones by the cell hashes + fine grain (grain faded to its mean first),
    # then the whole stone colour faded toward the average stone when sub-pixel
    grain_n = P.mix(0.5, P.fbm(p * gfreq, octaves), 1.0 - glod)
    sc = P.mix(P.mix(st1, st2, v.cell), st3, P.fract(v.cell * 5.1))
    sc = sc * P.mix(1.0 - cvar, 1.0 + cvar, grain_n)
    sc = P.mix(sc, mean_stone, lod)

    gl  = 1.0 - P.smoothstep(0.0, gapw + P.fwidth(fw), fw)   # AA'd gap mask (colour)
    glh = 1.0 - P.smoothstep(0.0, gapw, fw)                  # object-space (bump)
    col = P.mix(sc, gapc, gl * (1.0 - lod * 0.8))            # gaps fade out sub-pixel

    self.surface(
      albedo    = col,
      metallic  = 0.0,
      roughness = P.mix(rough, rough + 0.1, gl) + (grain_n - 0.5) * 0.1,
    )
    # stones raised (domes), gaps recessed — relief flattens to smooth at distance
    dome_h = P.saturate(1.0 - v.f1) * (1.0 - glh) * (1.0 - lod)
    self.displace(dome_h * domeA - glh * (1.0 - lod) * 0.3, scale=bumps,
                  parallax_steps=pom_steps, depth=depth)


class Sand(Ptex3d):
  # fine granular sand with wind ripples + grain sparkle
  def __init__(self, ctx, *, octaves=5, pom_steps=0, scale=2.0,
               sand_color=vec3(0.74, 0.61, 0.38), sand_color2=vec3(0.60, 0.47, 0.27),
               mottle_freq=3.0, color_var=0.22,
               ripple_dir=vec3(1.0, 0.0, 0.35), ripple_freq=0.13,
               ripple_warp=0.6, ripple_warp_freq=1.6, ripple=0.5,
               grain=0.10, grain_freq=120.0, sparkle=0.18,
               roughness=0.78, bump_scale=0.008, relief_depth=0.03):
    scl   = ctx.param("scale",            scale)
    sand1 = ctx.param("sand_color",       sand_color)
    sand2 = ctx.param("sand_color2",      sand_color2)
    mfreq = ctx.param("mottle_freq",      mottle_freq)
    cvar  = ctx.param("color_var",        color_var)
    rdir  = ctx.param("ripple_dir",       ripple_dir)
    rfreq = ctx.param("ripple_freq",      ripple_freq)
    rwarp = ctx.param("ripple_warp",      ripple_warp)
    rwf   = ctx.param("ripple_warp_freq", ripple_warp_freq)
    ramp  = ctx.param("ripple",           ripple)        # ripple strength
    grn   = ctx.param("grain",            grain)
    gfreq = ctx.param("grain_freq",       grain_freq)
    spk   = ctx.param("sparkle",          sparkle)       # per-grain roughness glints
    rough = ctx.param("roughness",        roughness)
    bumps = ctx.param("bump_scale",       bump_scale)
    depth = ctx.param("relief_depth",     relief_depth)

    p      = ctx.P_object * scl
    mottle = P.fbm(p * mfreq, octaves)
    grain_n = P.fbm(p * gfreq, octaves)
    col = P.mix(sand1, sand2, mottle)
    col = col * P.mix(1.0 - cvar, 1.0 + cvar, grain_n)

    # wind ripples: a smooth domain-warped sine wave (height in [0,1])
    ph     = P.dot(p, rdir) * rfreq + rwarp * (P.fbm(p * rwf, octaves) - 0.5)
    ripple_h = P.sin(ph * _PI) * 0.5 + 0.5
    col    = col * P.mix(0.88, 1.0, ripple_h)           # troughs slightly shaded

    self.surface(
      albedo    = col,
      metallic  = 0.0,
      roughness = rough - spk * (grain_n - 0.5),         # grain sparkle (roughness glints)
    )
    self.displace(ripple_h * ramp + grain_n * 0.15, scale=bumps,
                  parallax_steps=pom_steps, depth=depth)


__all__ = ["Dirt", "Grass", "Gravel", "Sand"]
