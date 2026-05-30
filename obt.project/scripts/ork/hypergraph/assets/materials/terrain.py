###############################################################################
# terrain — procedural volumetric Rock / Snow / Mud (GEOV2 ptex3d).
#
# VOLUMETRIC geological/weather surfaces composed from the function library
# (fbm, voronoi, domain_warp, cell_lod). 3D fields read at the surface (no
# triplanar), rich runtime variation, analytic-AA on the discrete (crack) fields.
#
#   from ork.hypergraph.assets.materials import Rock, Snow, Mud
#   mat = self.asset.Ptex3d("rock", dsl_class=Rock, strata=0.5)
#   mat = self.asset.Ptex3d("mud",  dsl_class=Mud,  water_level=0.5)
#
# Bake-time (ctor): octaves, pom_steps.
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import domain_warp, cell_lod

_PI = 3.14159265359


class Rock(Ptex3d):
  # cliff face: mottled stone + sedimentary strata bands + a recessed crack network
  def __init__(self, ctx, *, octaves=5, pom_steps=0, scale=1.5,
               rock_color=vec3(0.20, 0.17, 0.13), rock_color2=vec3(0.15, 0.12, 0.13),
               color_var=0.4, mottle_freq=4.0, detail_freq=12.0,
               strata=0.4, strata_dir=vec3(0.0, 1.0, 0.1), strata_freq=0.37, strata_warp=0.85,
               strata_color=vec3(0.52, 0.45, 0.36)*1.5,
               crack_freq=0.17, crack=0.1, crack_warp=0.45, crack_width=0.04,
               crack_color=vec3(0.07, 0.06, 0.05),
               roughness=0.52, bump_scale=0.03, relief_depth=0.08):
    scl   = ctx.param("scale",        scale)
    rk1   = ctx.param("rock_color",   rock_color)
    rk2   = ctx.param("rock_color2",  rock_color2)
    cvar  = ctx.param("color_var",    color_var)
    mfreq = ctx.param("mottle_freq",  mottle_freq)
    dfreq = ctx.param("detail_freq",  detail_freq)
    strA  = ctx.param("strata",       strata)            # sedimentary banding strength
    sdir  = ctx.param("strata_dir",   strata_dir)
    sfreq = ctx.param("strata_freq",  strata_freq)
    swarp = ctx.param("strata_warp",  strata_warp)
    strc  = ctx.param("strata_color", strata_color)
    cfreq = ctx.param("crack_freq",   crack_freq)
    crk   = ctx.param("crack",        crack)             # crack visibility
    cwarp = ctx.param("crack_warp",   crack_warp)
    cwid  = ctx.param("crack_width",  crack_width)
    crkc  = ctx.param("crack_color",  crack_color)
    rough = ctx.param("roughness",    roughness)
    bumps = ctx.param("bump_scale",   bump_scale)
    depth = ctx.param("relief_depth", relief_depth)

    p      = ctx.P_object * scl
    mottle = P.fbm(p * mfreq, octaves)
    detail = P.fbm(p * dfreq, octaves)
    base   = P.mix(rk1, rk2, mottle) * P.mix(1.0 - cvar, 1.0 + cvar, detail)

    # sedimentary strata: warped colour bands across `strata_dir`
    sph    = P.dot(p, sdir) * sfreq + swarp * (P.fbm(p * 1.5, octaves) - 0.5)
    band   = P.sin(sph * _PI) * 0.5 + 0.5
    base   = P.mix(base, strc, band * strA)

    # crack network: warped voronoi borders (fwedge), analytic-AA + cell LOD fade
    pv     = domain_warp(p * cfreq, cwarp, 1.0, octaves)
    v      = P.voronoi(pv)
    lod    = cell_lod(pv)
    crackV = (1.0 - P.smoothstep(0.0, cwid + P.fwidth(v.fwedge), v.fwedge)) * crk * (1.0 - lod)
    crackH = (1.0 - P.smoothstep(0.0, cwid, v.fwedge)) * (1.0 - lod)
    col    = P.mix(base, crkc, crackV)

    self.surface(
      albedo    = col,
      metallic  = 0.0,
      roughness = P.mix(rough, rough + 0.12, crackV) + (detail - 0.5) * 0.1,
    )
    # rough lumpy stone + recessed cracks
    self.displace((mottle - 0.5) * 0.6 + (detail - 0.5) * 0.15 - crackH * 0.6,
                  scale=bumps, parallax_steps=pom_steps, depth=depth)


class Snow(Ptex3d):
  # bright snow: drifts, blue shadow in the hollows, sparkle glints, subsurface glow
  def __init__(self, ctx, *, octaves=5, pom_steps=0, scale=2.0,
               snow_color=vec3(0.93, 0.95, 0.99), shadow_color=vec3(0.87, 0.91, 0.99),
               drift_freq=0.8, drift=0.6, fine_freq=28.0,
               sparkle=0.4, sparkle_freq=70.0,
               roughness=0.8, bump_scale=0.047, relief_depth=0.07,
               subsurface=0.5):
    scl   = ctx.param("scale",        scale)
    snowc = ctx.param("snow_color",   snow_color)
    shadc = ctx.param("shadow_color", shadow_color)
    dfreq = ctx.param("drift_freq",   drift_freq)
    drftA = ctx.param("drift",        drift)
    ffreq = ctx.param("fine_freq",    fine_freq)
    spkA  = ctx.param("sparkle",      sparkle)
    spkF  = ctx.param("sparkle_freq", sparkle_freq)
    rough = ctx.param("roughness",    roughness)
    bumps = ctx.param("bump_scale",   bump_scale)
    depth = ctx.param("relief_depth", relief_depth)

    p     = ctx.P_object * scl
    drift = P.fbm(p * dfreq, octaves)                    # soft mounds
    fine  = P.fbm(p * ffreq, octaves)

    # white, with a cool shadow tint settling into the hollows
    col = P.mix(shadc, snowc, P.smoothstep(0.30, 0.72, drift))

    # sparkle: rare bright low-roughness specks, faded out when sub-pixel (so distant
    # snow is smooth rather than a boiling glitter field)
    spk = P.smoothstep(0.96 - spkA * 0.12, 1.0, P.fbm(p * spkF, 2))
    spk = spk * (1.0 - P.smoothstep(0.35, 0.9, spkF * P.length(P.fwidth(p))))

    self.surface(
      albedo    = col,
      metallic  = 0.0,
      roughness = P.mix(rough, 0.12, spk),               # matte snow, glinting crystals
      # snow's soft translucent glow (a class lobe — constant)
      subsurface_factor = subsurface,
      subsurface_color  = vec3(0.90, 0.93, 1.0),
    )
    self.displace(drift * drftA + (fine - 0.5) * 0.12, scale=bumps,
                  parallax_steps=pom_steps, depth=depth)


class Mud(Ptex3d):
  # wet mud with water pooled in the low spots (flat, mirror-reflective puddles)
  def __init__(self, ctx, *, octaves=2, pom_steps=0, scale=2.0,
               mud_color=vec3(0.27, 0.22, 0.18), mud_color2=vec3(0.20, 0.17, 0.14),
               color_var=0.3, height_freq=2.2, grain_freq=14.0,
               water_level=0.25, water_color=vec3(0.05, 0.05, 0.06),
               ripple=0.12, ripple_freq=22.0, lump=0.6,
               roughness=0.85, bump_scale=0.035, relief_depth=0.06):
    scl   = ctx.param("scale",        scale)
    mud1  = ctx.param("mud_color",    mud_color)
    mud2  = ctx.param("mud_color2",   mud_color2)
    cvar  = ctx.param("color_var",    color_var)
    hfreq = ctx.param("height_freq",  height_freq)
    gfreq = ctx.param("grain_freq",   grain_freq)
    wlvl  = ctx.param("water_level",  water_level)        # raise -> more / deeper puddles
    watc  = ctx.param("water_color",  water_color)
    ripA  = ctx.param("ripple",       ripple)
    rfreq = ctx.param("ripple_freq",  ripple_freq)
    lumpA = ctx.param("lump",         lump)
    rough = ctx.param("roughness",    roughness)
    bumps = ctx.param("bump_scale",   bump_scale)
    depth = ctx.param("relief_depth", relief_depth)

    p      = ctx.P_object * scl
    height = P.fbm(p * hfreq, octaves)                   # terrain; low areas hold water
    grain  = P.fbm(p * gfreq, octaves)
    mud    = P.mix(mud1, mud2, P.fbm(p * 6.0, octaves)) * P.mix(1.0 - cvar, 1.0 + cvar, grain)

    # puddle where height < water_level; a darkened wet ring just above the waterline
    pud     = P.smoothstep(wlvl, wlvl - 0.05, height)
    wetring = P.smoothstep(wlvl + 0.12, wlvl, height) * (1.0 - pud)
    mud     = mud * P.mix(1.0, 0.6, wetring)

    col = P.mix(mud, watc, pud)
    self.surface(
      albedo    = col,
      metallic  = 0.0,
      roughness = P.mix(rough, 0.04, pud),               # mud matte, water a near-mirror
    )
    # lumpy mud relief, FLAT in the puddles (so the water reads as a level mirror),
    # plus a faint ripple on the water surface
    ripple_h = P.sin(P.dot(p, vec3(1.0, 0.0, 0.7)) * rfreq) * ripA
    self.displace((height - 0.5) * lumpA * (1.0 - pud) + pud * ripple_h * 0.1,
                  scale=bumps, parallax_steps=pom_steps, depth=depth)


__all__ = ["Rock", "Snow", "Mud"]
