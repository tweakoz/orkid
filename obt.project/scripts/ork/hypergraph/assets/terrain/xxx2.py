###############################################################################
# xxx2 — xxx.py forked, with STRATA TERRACING via the unified procedural substrate.
# The erosion pipeline is IDENTICAL to xxx.py; the only addition is, at the very
# end, an hfdisplacement that snaps the eroded height to strata-band elevations so
# the geometry forms benches that COINCIDE with the strata a material shades
# (terrace_strata reads the current height via ctx.P_object.y = in0 * height_m).
#
# REQUIRES the Phase-2 build (multi-input ExprModule + hfdisplacement). For the
# benches to line up EXACTLY with hmview's strata, set STRATA_PERIOD_M to hmview's
# band period 1/(scale*strata_freq) (defaults: 1/(0.04*0.02) = 1250 m).
#   ork.terrain.viewer2.py xxx2 -M hmview
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.ptex3d import Ptex3d, P   # Ptex3d: the inline material base (same file)
from ork.hypergraph.colors import hsv
_TAU = 6.28318530718
###############################################################################
# erosion-only vertical exaggeration (meters that normalized 1.0 is DURING erosion).
EROSION_HEIGHT_M = 4000.0
# strata terracing — ONE source of truth (STRATA_PERIOD_M); the material's strata_freq
# is DERIVED from it so the shaded bands sit at the same elevations as the benches.
SCALE           = 0.04      # hmview detail-frequency multiplier (ctx.param "scale")
STRATA_PERIOD_M = 100.0     # bench spacing in world-Y (the single source)
TERRACE         = 0.25      # 0 = no terracing (pure erosion) .. 1 = full snap to benches
STRATA_FREQ     = 1.0 / (SCALE * STRATA_PERIOD_M)   # hmview band period = 1/(scale*freq)
###############################################################################
P_FBM = T.ParamPack(
    frequency=9.2,
    octaves=8
)
###############################################################################
P_THERM = T.ParamPack(
    talus_deg=16.0,
    rate=0.10,
    exaggerated_height_m=EROSION_HEIGHT_M,
    iterations=570 )
###############################################################################
P_EROX = T.ParamPack(
    sim_time_s=3.5,
    rain_mps=0.06,
    evaporation_per_s=0.05,
    flow_speed_max_mps=22.0,
    capacity_Kc=0.1,
    erosion_rate_per_s=3.0,
    deposition_rate_per_s=1.0,
    creep_m2ps=16.0,
    exaggerated_height_m=EROSION_HEIGHT_M )
###############################################################################
P_PHA = T.ParamPack(
    strength=0.07,
    gully_weight=2.0,
    detail=0.6,
    scale=0.0125,
    cell_scale=2.7,
    normalization=0.5,
    lacunarity=2.0,
    gain=0.5,
    default_height=0.5,
    octaves=1 )
###############################################################################


def strata_phase(ctx, scl, sfrq):
    """Strata PHASE in cycles — the ONE source BOTH the shader (band = sin(phase*TAU)) and
    the terrace (snap so phase hits integers) use, so the benches land EXACTLY on the bands.
    The warp is XZ-only (no Y dependence), so d(phase)/d(elevation) = scl*sfrq exactly ->
    the terrace snap below is exact. P.noise (single octave) — the proven-in-compute path."""
    p    = ctx.P_object * scl
    warp = (P.noise(P.vec3(p.x, 0.0, p.z) * 1.5) - 0.5) * 0.01
    return P.dot(p, P.vec3(0.0, 1.0, 0.12)) * sfrq + warp


def terrace_strata(ctx):
    """Snap the current height so its strata phase lands on a band boundary -> geometric
    benches coincide EXACTLY with XXX2Mat's bands (same strata_phase). Reads the current
    height via ctx.P_object.y (= in0 * height_m). Soft riser so the steps aren't razor."""
    ph  = strata_phase(ctx, SCALE, STRATA_FREQ)
    phs = P.floor(ph) + P.smoothstep(0.30, 0.70, P.fract(ph))   # snap to integer band; soft riser
    dy  = (phs - ph) / (SCALE * STRATA_FREQ)                     # elevation shift (m) onto the band
    return (ctx.P_object.y + dy) / ctx.height_m                 # normalized snapped height


class XXX2Mat(Ptex3d):
    """Pure HEIGHT/SLOPE color classifier (NO texture) — hmview's exact palette. dirt ->
    grass with elevation, then a rock band, then snow; steep faces go rock. On the
    terraced terrain the elevation bands + the (steep) risers vs (flat) treads read as
    colored strata on their own — no procedural banding needed. Colors/thresholds are
    bindable (ctx.param); the only AA is mask-edge widening + a specular-AA roughness
    floor (both geometric, not texture)."""

    def __init__(self, ctx, *,
                 height_scale  = 4000.0,
                 mottle_scale  = 0.03,                       # the ONE shared noise frequency (broad mottle)
                 grass_lo = 0.26,
                 grass_hi = 0.39,           # h01: dirt -> grass (low ground)
                 rock_lo  = 0.42, 
                 rock_hi  = 0.60,           # h01: ground -> rock
                 snow_lo  = 0.72,                            # h01: rock -> snow (up to 1.0)
                 rock_slope_lo = 0.75, 
                 rock_slope_hi = 0.80, # slope: gentle -> cliff -> rock
                 dirt_color   = hsv( 10.0, 0.25, 0.5),       # 2-variant dirt/grass/rock (mottle blends each)
                 dirt_color2  = hsv( 18.0, 0.20, 0.30),
                 grass_color  = hsv(129.4, 0.25, 0.25),
                 grass_color2 = hsv(110.0, 0.20, 0.12),
                 rock_color   = hsv(  5.0, 0.0, 0.30),
                 rock_color2 = hsv( 10.0, 0.0, 0.40),
                 snow_color  = hsv(220.0, 0.091, 0.99)):
        hs    = ctx.param("height_scale",  height_scale)
        msc   = ctx.param("mottle_scale",  mottle_scale)
        g_lo  = ctx.param("grass_lo",      grass_lo)
        g_hi  = ctx.param("grass_hi",      grass_hi)
        r_lo  = ctx.param("rock_lo",       rock_lo)
        r_hi  = ctx.param("rock_hi",       rock_hi)
        s_lo  = ctx.param("snow_lo",       snow_lo)
        sl0   = ctx.param("rock_slope_lo", rock_slope_lo)
        sl1   = ctx.param("rock_slope_hi", rock_slope_hi)
        cdrt  = ctx.param("dirt_color",   dirt_color)
        cdrt2 = ctx.param("dirt_color2",  dirt_color2)
        cgrs  = ctx.param("grass_color",  grass_color)
        cgrs2 = ctx.param("grass_color2", grass_color2)
        crok  = ctx.param("rock_color",   rock_color)
        crok2 = ctx.param("rock_color2", rock_color2)
        csno  = ctx.param("snow_color",  snow_color)

        h01   = P.saturate(ctx.P.y / hs)        # elevation [0,1]
        slope = P.saturate(1.0 - ctx.N.y)       # 0 flat .. 1 vertical (mesh normal)
        p     = ctx.P_object

        # ONE shared mottle, footprint band-limited (fades to its mean -> no shimmer when
        # sub-pixel). The SAME value blends each layer's two color variants (broad, not grain).
        foot  = P.max(P.length(P.dFdx(p)), P.length(P.dFdy(p)))
        mot   = P.fbm(p * msc, 3)
        mot   = 0.5 + (mot - 0.5) * (1.0 - P.smoothstep(0.30, 0.70, msc * foot))
        dirt  = P.mix(cdrt, cdrt2, mot)
        grass = P.mix(cgrs, cgrs2, mot)
        rock  = P.mix(crok, crok2, mot)

        def aa_ramp(x, e0, e1):
            # soft crossover, edge never narrower than 1px (fwidth) — geometric AA, not texture.
            c  = (e0 + e1) * 0.5
            hw = P.max((e1 - e0) * 0.5, P.fwidth(x) + 1e-4)
            return P.smoothstep(c - hw, c + hw, x)

        albedo, rough = dirt, 0.95                                                  # lowest = dirt
        m = aa_ramp(h01, g_lo, g_hi); albedo = P.mix(albedo, grass, m); rough = P.mix(rough, 0.90, m)
        m = aa_ramp(h01, r_lo, r_hi); albedo = P.mix(albedo, rock,  m); rough = P.mix(rough, 0.62, m)
        m = aa_ramp(h01, s_lo, 1.0);  albedo = P.mix(albedo, csno,  m); rough = P.mix(rough, 0.85, m)
        m = aa_ramp(slope, sl0, sl1); albedo = P.mix(albedo, rock,  m); rough = P.mix(rough, 0.62, m)  # steep -> rock
        # specular AA (geometric): fold sub-pixel normal variance into roughness.
        n_var = P.dot(P.fwidth(ctx.N), P.fwidth(ctx.N))
        rough = P.saturate(P.sqrt(rough * rough + P.min(0.4, n_var * 2.0)))
        self.surface(albedo=albedo, metallic=0.0, roughness=rough)


class XXX2(HeightField):
    # ---- authored PHYSICAL world scale (erosion exaggeration is per-op EROSION_HEIGHT_M) ----
    EXTENT_M = 32768.0
    HEIGHT_M = 4000.0
    # shader lives IN THIS FILE (XXX2Mat above) -> MATERIAL_CLASS. Pure height/slope color
    # (no texture); the terraces supply the strata structure. Defaults are fine, so no params.
    MATERIAL_CLASS  = XXX2Mat
    MATERIAL_PARAMS = {}

    def __init__(self,iters = 16):
        ####################################
        super().__init__()
        ero_out = T.Const(0)
        ####################################
        base = T.Fbm( P_FBM ) * 0.5 + 0.5
        ero_bas = base*0.03
        ####################################
        for i in range(0,iters):
          ero_inp = ero_out+ero_bas
          bfill = T.basin_fill(ero_inp)
          bfill = (ero_inp*0.90)+(bfill*0.1)
          thr_out = T.erode_thermal( bfill,P_THERM)
          erox_out = T.erox( thr_out,P_EROX)
          pha_out = T.pha(erox_out,P_PHA)
          xxx_out = (erox_out*0.9) + (pha_out*0.1)
          terr     = self.hfdisplacement(terrace_strata, xxx_out)
          terr_out = T.Mix(xxx_out, terr, 0.1)
          ero_out = T.lpf(terr_out, cutoff_m=4)
        ####################################
        for i in range(0,iters*2):
          bfill = T.basin_fill(ero_out)
          ero_out = (ero_out*0.95)+(bfill*0.05)
        lpf_out = T.lpf(ero_out, cutoff_m=4)
        ero_out = (ero_out*0.15)+(lpf_out*0.85)
        lpf_out = T.lpf(ero_out, cutoff_m=2)
        ero_out = (ero_out*0.15)+(lpf_out*0.85)
        ####################################
        # STRATA TERRACING (the unified-substrate addition): snap to band elevations,
        # blended TERRACE toward the eroded height (so drainage detail survives), then a
        # light lpf to soften the risers into plausible benches.
        ####################################
        self.capture(ero_out,"height",cache=True)
        self.capture(ero_out,"normal",cache=True)
