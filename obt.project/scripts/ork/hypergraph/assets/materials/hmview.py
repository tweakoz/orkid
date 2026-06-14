###############################################################################
# hmview — height + slope BLEND terrain surface, DETAILED + fully analytic-AA.
#
# ONE material that surfaces a whole terrain by WORLD HEIGHT (ctx.P.y) and SLOPE
# (1 - ctx.N.y):
#
#   * LOWLAND FLATS — a patchy, noise-driven ground cover: a low-frequency
#     "moisture" field blends ROCK (wet hollows) -> DIRT -> GRASS, and an
#     independent "forest-coverage" field drops FOREST-BED patches over it.
#     Organic blobs, not height bands — bare wet rock in the hollows, grass on
#     the dry rises and hillsides, leafy forest floor in patches. (Rock — not a
#     low-roughness "water" — because a glossy flat alias-sparkles at grazing.)
#   * ELEVATION — the lowland cover gives way to ROCK with height, then SNOW on
#     the peaks.
#   * SLOPE — steep faces read as ROCK at any altitude.
#
# Each layer is a real procedural surface; EVERYTHING is analytically AA'd:
#   * per-layer fine detail is band-limited — each fine octave fades to its mean
#     once below the pixel footprint (length(fwidth(p))) -> no shimmer/crawl.
#   * every blend mask (height, slope, AND the lowland noise patches) is a
#     smoothstep whose edge is widened to >= 1px (fwidth of the driving variable)
#     -> boundaries never alias into jagged lines at grazing distance.
#
# Thresholds + colors are bindable ctx.param (live via material.bindParam);
# `octaves` bakes (loop bound). Height is WORLD METERS — `height_scale`
# normalizes ctx.P.y to [0,1]; pass the scale the mesh renders at. ctx.N is the
# MESH (Scharr) normal, so the slope test is the true terrain slope (no displace).
#
#   mat = self.asset.Ptex3d("hmview", dsl_class=HMView, height_scale=4000.0)
#   mat.as_gfx_material.bindParam("patch_scale", 0.9)   # bigger/smaller patches
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.colors import hsv
from ork.hypergraph.ptex3d import Ptex3d, P

_TAU = 6.28318530718


class HMView(Ptex3d):

  def __init__(self, ctx, *,
               octaves       = 5,                            # BAKED fbm loop bound
               scale         = 0.14,                         # detail frequency (1/meter, object units)
               patch_scale   = 0.6,                          # lowland patch frequency (relative to scale)
               height_scale  = 4000.0,                       # world Y / this -> [0,1]
               rock_lo       = 0.42, rock_hi       = 0.60,   # h01: lowland -> rock band
               snow_lo       = 0.72,                         # h01: rock -> snow (up to 1.0)
               rock_slope_lo = 0.55, rock_slope_hi = 0.80,   # slope: gentle -> cliff -> rock
               forest_lo     = 0.50, forest_hi     = 0.66,   # forest-bed coverage patch edge
               dirt_color    = hsv( 10.0, 0.25, 0.25),       # warm brown
               dirt_color2   = hsv( 15.0, 0.20, 0.20),       # warm brown
               grass_color   = hsv(129.4, 0.10, 0.07),       # green
               forest_color  = hsv( 34.3, 0.20, 0.18),       # dark leafy brown
               rock_color    = hsv(5.0, 0.118, 0.10),      # cool grey
               rock_color2   = hsv(10.0, 0.137, 0.15),      # darker cool grey
               snow_color    = hsv(220.0, 0.091, 0.99),      # blue-white
               strata        = 0.35, strata_freq   = 0.03,   # rock sedimentary banding
               sparkle        = 0.5,                         # snow specular glints
               grass_slope    = 0.8,                         # how strongly grass greens lowland slopes
               grass_slope_lo = 0.06, grass_slope_hi = 0.50, # slope range grass takes hold over
               aa_falloff     = 0.35,                          # GLOBAL noise-detail AA knob: >1 fades ALL noise sooner (softer), <1 sharper
               spec_aa        = 2.0,                          # geometric specular-AA: normal-variance -> roughness
               graze_rough    = 0.5,                          # roughness floor grown by the texel footprint
               elev_noise     = 0.18,                         # break elevation-band borders (snowline etc.), h01 units
               elev_noise_freq= 0.03,                          # noise frequency for that border break
               cliff_compress = 0.13,                         # rock vertical-axis compression on cliffs (lower = more stretch)
               cliff_drip     = 0.9,                          # vertical drip-streak darkening on cliffs
               dirt_on_rock   = 0.15,                         # opacity of sparse settled dirt patches on rock
               dirt_patch_freq= 0.19):                         # size of those dirt patches (lower = bigger)
    oct  = int(octaves)
    scl  = ctx.param("scale",         scale)
    pf   = ctx.param("patch_scale",   patch_scale)
    hs   = ctx.param("height_scale",  height_scale)
    r_lo = ctx.param("rock_lo",       rock_lo)
    r_hi = ctx.param("rock_hi",       rock_hi)
    s_lo = ctx.param("snow_lo",       snow_lo)
    sl0  = ctx.param("rock_slope_lo", rock_slope_lo)
    sl1  = ctx.param("rock_slope_hi", rock_slope_hi)
    f_lo = ctx.param("forest_lo",     forest_lo)
    f_hi = ctx.param("forest_hi",     forest_hi)
    cdrt = ctx.param("dirt_color",    dirt_color)
    cdrt2 = ctx.param("dirt_color2",   dirt_color2)
    cgrs = ctx.param("grass_color",   grass_color)
    cfor = ctx.param("forest_color",  forest_color)
    crok = ctx.param("rock_color",    rock_color)
    crok2 = ctx.param("rock_color2",   rock_color2)
    csno = ctx.param("snow_color",    snow_color)
    strA = ctx.param("strata",        strata)
    sfrq = ctx.param("strata_freq",   strata_freq)
    spkA = ctx.param("sparkle",       sparkle)
    gssA = ctx.param("grass_slope",    grass_slope)
    gsl0 = ctx.param("grass_slope_lo", grass_slope_lo)
    gsl1 = ctx.param("grass_slope_hi", grass_slope_hi)
    aaF  = ctx.param("aa_falloff",     aa_falloff)
    saaK = ctx.param("spec_aa",        spec_aa)
    grzK = ctx.param("graze_rough",    graze_rough)
    enoz = ctx.param("elev_noise",      elev_noise)
    efrq = ctx.param("elev_noise_freq", elev_noise_freq)
    ccmp = ctx.param("cliff_compress",  cliff_compress)
    cdrp = ctx.param("cliff_drip",      cliff_drip)
    dorA = ctx.param("dirt_on_rock",    dirt_on_rock)
    dpf  = ctx.param("dirt_patch_freq", dirt_patch_freq)

    # object/world-space detail coordinate + per-pixel footprint (the AA basis).
    p    = ctx.P_object * scl
    # `foot` = the LONGER screen-space axis of p's Jacobian: max(|dFdx(p)|,|dFdy(p)|).
    # This is exactly the metric the texture hardware uses to pick a mip LOD, so it is
    # a function of how p PROJECTS TO PIXELS (distance x the surface's angle to the
    # view RAY) and is stable under camera pitch — unlike any normal-vs-view term.
    # Distance grows it (far surfaces) and glancing anisotropy grows it (the long axis
    # stretches), so it targets exactly the undersampled surfaces and nothing else.
    foot = P.max(P.length(P.dFdx(p)), P.length(P.dFdy(p)))

    def band_limit(freq):
      # 1.0 while detail at `freq` is resolvable, -> 0.0 once sub-pixel; multiply any
      # fine (zero-mean) variation by this so it fades to its mean, never aliases.
      # freq*foot ~= cycles-per-pixel along the major footprint axis; Nyquist ~0.5, so
      # the [0.30,0.70] window brackets it. This IS the procedural mip: it fades detail
      # at distance AND at glancing (anisotropic) angles, the way a sampler drops to a
      # coarser mip there — keyed off the footprint, not the camera orientation.
      # `aaF` is the global aggressiveness knob (same scale as fbm_aa's `aa`): >1 fades
      # the grain sooner, <1 keeps it longer — kept in lock-step with the fbm_aa octaves.
      return 1.0 - P.smoothstep(0.30, 0.70, freq * foot * aaF)

    def aa_ramp(x, e0, e1):
      # soft crossover e0->e1, but the edge is never narrower than one pixel
      # (fwidth(x)) — used for EVERY mask (height, slope, and the lowland patches).
      c  = (e0 + e1) * 0.5
      hw = P.max((e1 - e0) * 0.5, P.fwidth(x) + 1e-4)
      return P.smoothstep(c - hw, c + hw, x)

    # ---------------- ground covers (albedo + roughness) ----------------------
    slope = P.saturate(1.0 - ctx.N.y)                # 0 flat .. 1 vertical (mesh normal); used widely below

    # DIRT — mottled earth + grain (also the material that settles on rock ledges).
    # base mottle uses fbm_aa: its high octaves (4*2^k) were the flats' aliasing source.
    d_v    = P.fbm_aa(p * 4.0,  oct, aaF)
    d_g    = (P.fbm(p * 20.0, 2) - 0.5) * band_limit(20.0)
    a_dirt = cdrt * P.mix(0.70, 1.30, d_v) * (1.0 + d_g * 0.40)
    r_dirt = 0.95
    a_dirt2 = cdrt2 * P.mix(0.70, 1.30, d_v) * (1.0 + d_g * 0.40)
    r_dirt2 = 0.9

    # ROCK — mottled stone + HORIZONTAL sedimentary strata + grain. The wet-hollow
    # lowland base, the elevation layer, AND the cliff/slope rock. Three cliff touches
    # make vertical faces read right:
    #   * STRETCHED look — on steep faces compress the rock-noise VERTICAL axis so the
    #     mottle/grain elongate downward (eroded striations); flats stay isotropic.
    #   * vertical DRIP STREAKS — water-stain runs down the face, gated by steepness.
    #   * sparse SETTLED DIRT — on the gentler ledges + recesses, never vertical walls.
    steep   = P.smoothstep(0.30, 0.85, slope)        # 0 flat .. 1 ~vertical
    ystr    = P.mix(1.0, ccmp, steep)                # vertical-axis compression on cliffs
    pr      = P.vec3(p.x, p.y * ystr, p.z)           # rock detail coord (Y-compressed on cliffs)
    rk_mot  = P.fbm_aa(pr * 2.5,  oct, aaF)
    s_phase = P.dot(p, vec3(0.0, 1.0, 0.12)) * sfrq + (P.fbm_aa(p * 1.5, oct, aaF) - 0.5) * 0.6   # aa: phase-jitter octaves crawl the strata
    band    = P.sin(s_phase * _TAU) * 0.5 + 0.5      # strata stay HORIZONTAL in world Y
    rk_fin  = (P.fbm(pr * 15.0, 2) - 0.5) * band_limit(15.0)
    a_rock  = crok * P.mix(0.80, 1.25, rk_mot)
    a_rock  = P.mix(a_rock, crok * 1.7, band * strA)
    a_rock  = a_rock * (1.0 + rk_fin * 0.30)
    r_rock  = P.mix(0.55, 0.70, rk_mot)

    a_rock2  = crok2 * P.mix(0.80, 1.25, rk_mot)
    a_rock2  = P.mix(a_rock2, crok2 * 1.7, band * strA)
    a_rock2  = a_rock2 * (1.0 + rk_fin * 0.30)
    r_rock2  = P.mix(0.55, 0.70, rk_mot)

    # vertical drip streaks: high-freq ACROSS the face (x/z), low-freq DOWN it (y).
    streak  = P.fbm_aa(P.vec3(p.x * 6.0, p.y * 0.5, p.z * 6.0), oct, aaF)   # aa: freq up to 96 on cliff faces
    a_rock  = a_rock * P.mix(1.0, P.mix(0.65, 1.0, streak), steep * cdrp)
    # sparse settled dirt — fwidth-AA'd sparse blobs, biased to LEDGES (1-steep) and
    # the recessed mottle (1-rk_mot): exactly where dirt can plausibly accumulate.
    dpn     = P.fbm(p * dpf, oct)
    dpe     = P.fwidth(dpn) + 0.04
    dpatch  = P.smoothstep(0.58 - dpe, 0.58 + dpe, dpn) * (1.0 - steep) * P.mix(1.0, 1.0 - rk_mot, 0.6)
    a_rock  = P.mix(a_rock, a_rock2, dpatch * dorA)
    r_rock  = P.mix(r_rock, r_rock2, dpatch * dorA)

    # GRASS — green clump variation + fine blades.
    g_v    = P.fbm_aa(p * 4.5,  oct, aaF)
    g_f    = (P.fbm(p * 26.0, 2) - 0.5) * band_limit(26.0)
    a_grs  = cgrs * P.mix(0.65, 1.25, g_v) * (1.0 + g_f * 0.40)
    r_grs  = 0.90

    # FOREST BED — dark leafy litter + warm debris flecks.
    fb_v   = P.fbm_aa(p * 3.5,  oct, aaF)
    fleck  = P.smoothstep(0.60, 0.95, P.fbm(p * 30.0, 2)) * band_limit(30.0)
    a_for  = cfor * P.mix(0.70, 1.25, fb_v)
    a_for  = P.mix(a_for, cfor * vec3(1.7, 1.3, 0.8), fleck * 0.5)   # leaf/twig flecks
    r_for  = 0.92

    # ---------------- lowland = patchy noise blend of the covers --------------
    mois   = P.fbm_aa(p * pf, oct, aaF)                           # rock-hollow(low) .. dirt .. grass(high); aa: no sub-pixel patch islands
    forest = P.fbm_aa(p * pf * 1.7 + vec3(19.3, 7.1, 31.7), oct, aaF)  # independent forest coverage; aa likewise
    ground, ground_r = a_dirt2, r_dirt                             # wettest hollows = rock (was glossy mud)
    m = aa_ramp(mois,   0.22, 0.45); ground = P.mix(ground, a_dirt, m); ground_r = P.mix(ground_r, r_dirt, m)
    m = aa_ramp(mois,   0.55, 0.82); ground = P.mix(ground, a_grs,  m); ground_r = P.mix(ground_r, r_grs,  m)
    m = aa_ramp(forest, f_lo, f_hi); ground = P.mix(ground, a_for,  m); ground_r = P.mix(ground_r, r_for,  m)
    # grass climbs the lowland HILLSIDES: gentle/moderate slope greens the cover
    # (mud/dirt give way to grass). Genuinely steep faces still go rock via the slope
    # override below, and altitude overrides to rock/snow. Amount + range bindable.
    gsl      = aa_ramp(slope, gsl0, gsl1) * gssA
    ground   = P.mix(ground,   a_grs, gsl)
    ground_r = P.mix(ground_r, r_grs, gsl)

    # ---------------- elevation layer (snow; rock computed up top) ------------
    # SNOW — drifts with a cool shadow in the hollows + band-limited sparkle.
    sn_drift = P.fbm_aa(p * 1.2,  oct, aaF)
    sn_fine  = (P.fbm(p * 22.0, 2) - 0.5) * band_limit(22.0)
    a_snow   = P.mix(csno * 0.92, csno, P.smoothstep(0.30, 0.72, sn_drift)) * (1.0 + sn_fine * 0.06)
    # snow sparkle: a HARD-thresholded glint field. band_limit fades its amplitude with
    # distance, but near-field each glint edge is sub-pixel -> crawls; widen the threshold
    # by fwidth (>= ~1px, like aa_ramp) so the glints are resolved, not aliased.
    sn_n     = P.fbm(p * 60.0, 2)
    sn_e     = P.max(0.035, P.fwidth(sn_n))
    spk      = P.smoothstep(0.965 - sn_e, 0.965 + sn_e, sn_n) * band_limit(60.0) * spkA
    r_snow   = P.mix(0.85, 0.10, spk)

    # ---------------- final assembly: lowland -> rock -> snow, + slope rock ----
    h01   = P.saturate(ctx.P.y / hs)
    # break the elevation-band borders (lowland->rock and the SNOWLINE) with a
    # low-frequency noise so they undulate organically instead of reading as flat
    # height contours — snow dips into gullies, bare patches poke through. The noise
    # is added to the height DRIVING the masks, so aa_ramp still analytically AA's
    # the (now perturbed) edge via fwidth(h_elev).
    h_elev = P.saturate(h01 + (P.fbm_aa(p * efrq, oct, aaF) - 0.5) * enoz)   # aa: break-noise octaves wiggle the snow/rock line sub-pixel
    albedo, rough = ground, ground_r
    m = aa_ramp(h_elev, r_lo, r_hi); albedo = P.mix(albedo, a_rock, m); rough = P.mix(rough, r_rock, m)
    m = aa_ramp(h_elev, s_lo, 1.0);  albedo = P.mix(albedo, a_snow, m); rough = P.mix(rough, r_snow, m)
    m = aa_ramp(slope,  sl0,  sl1);  albedo = P.mix(albedo, a_rock, m); rough = P.mix(rough, r_rock, m)

    # ---------------- analytic specular antialiasing --------------------------
    # Grazing-angle / distant ground presents a highly anisotropic footprint; the
    # gentle per-pixel normal wobble then makes the Fresnel-boosted specular lobe
    # sparkle/crawl (worst on the flats). Widen the roughness lobe analytically:
    #   * by per-pixel NORMAL VARIANCE (Kaplanyan/Toksvig): fold sub-pixel normal
    #     wobble into roughness — sqrt(r^2 + variance) -> a wider, stable lobe.
    #   * by the texture FOOTPRINT: distant/grazing texels can't present a mirror-
    #     thin lobe (a roughness floor that grows with `foot`, capped).
    n_var = P.dot(P.fwidth(ctx.N), P.fwidth(ctx.N))           # ~ |fwidth(normal)|^2
    rough = P.sqrt(rough * rough + P.min(0.4, n_var * saaK))  # variance -> wider lobe
    rough = P.max(rough, P.min(0.6, foot * grzK))             # footprint roughness floor
    rough = P.saturate(rough)

    self.surface(albedo=albedo,
                 normal=None,
                 ao=None,
                 emissive=None, 
                 metallic=0.0, 
                 roughness=rough)


__all__ = ["HMView"]
