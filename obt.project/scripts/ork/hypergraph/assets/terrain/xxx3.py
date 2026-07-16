###############################################################################
# xxx3 — xxx2 forked, adding ENDORHEIC (Great-Basin) flattening. descend_basins() pulls
# the gentle, low areas toward a smooth floor that DROOPS to the basin minimum (flat
# playas) while STEEP mountain terrain stays rugged; it's a FUNCTION invoked in a loop so
# the basins descend + flatten gradually (the floor re-smooths each pass). Everything else
# (strata terrace + the inline XXX3Mat height/slope material) is identical to xxx2.
#   ork.terrain.viewer2.py xxx3
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.ptex3d import Ptex3d, P   # Ptex3d: the inline material base (same file)
from ork.hypergraph.assets.materials.terrain.solid import Solid
from ork.hypergraph.colors import hsv
_TAU = 6.28318530718
###############################################################################
# NATURAL UNITS: heights are TRUE METERS on the graph — the base fbm is authored at
# RELIEF_M and every op (erosion included) acts on real meters. (The old normalized
# [0,1]+HEIGHT_M form eroded at an equal EROSION_HEIGHT_M=4000, so this is the same
# physical regime — no exaggeration remap needed.)
RELIEF_M = 5000.0
# strata terracing — ONE source of truth (STRATA_PERIOD_M); the material's strata_freq
# is DERIVED from it so the shaded bands sit at the same elevations as the benches.
SCALE           = 0.04      # hmview detail-frequency multiplier (ctx.param "scale")
STRATA_PERIOD_M = 100.0     # bench spacing in world-Y (the single source)
TERRACE         = 0.25      # 0 = no terracing (pure erosion) .. 1 = full snap to benches
STRATA_FREQ     = 1.0 / (SCALE * STRATA_PERIOD_M)   # hmview band period = 1/(scale*freq)
# endorheic (Great-Basin) flattening — descend_basins(), invoked in the loop below
PLAYA            = 1.0      # playa level: 0 flat lake (spill) .. 1 at the basin min .. >1 droops below
DEPTH_SMOOTH_M   = 2500.0   # smoothing of the basin "characteristic depth" (bigger = flatter/broader)
DESCEND_STRENGTH = 0.1     # per-pass blend toward the floor (small -> gradual over the loop)
DESCEND_ITERS    = 24       # number of descend passes
###############################################################################
P_FBM = T.ParamPack(
    frequency=9.2,
    octaves=8
)
###############################################################################
P_THERM = T.ParamPack(
    talus_deg=16.0,
    rate=0.10,
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
    creep_m2ps=16.0 )
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
    default_height=0.5*RELIEF_M,   # mid-height reference (METERS)
    fade_width=0.15*RELIEF_M,      # valley<->peak fade window (METERS; old 0.15 of [0,1])
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
    benches coincide EXACTLY with XXX3Mat's bands (same strata_phase). Reads the current
    height via ctx.P_object.y (= in0 — TRUE METERS). Soft riser so the steps aren't razor."""
    ph  = strata_phase(ctx, SCALE, STRATA_FREQ)
    phs = P.floor(ph) + P.smoothstep(0.30, 0.70, P.fract(ph))   # snap to integer band; soft riser
    dy  = (phs - ph) / (SCALE * STRATA_FREQ)                     # elevation shift (m) onto the band
    return ctx.P_object.y + dy                                  # snapped height (meters)


def descend_basins(node, *, strength=DESCEND_STRENGTH, playa=PLAYA,
                   depth_smooth_m=DEPTH_SMOOTH_M, slope_radius_m=128.0):
    """Endorheic (Great-Basin) flattening. Anchor the floor to basin_fill (which is FLAT
    per basin, at spill) and LOWER that flat surface by the basin's CHARACTERISTIC depth
    (a heavily-smoothed fill-depth) -> a flat playa at a LOW level, NOT the regional lpf-mean
    (which would dominate + raise the lows). Steep mountain terrain stays rugged.
      playa: 0 = flat lake (spill) .. 1 = ~the basin minimum .. >1 = droops BELOW (deeper).
      depth_smooth_m: bigger = flatter, broader playa.
    Invoke in a loop with a modest `strength` so basins descend gradually."""
    filled = T.basin_fill(node)                                   # FLAT per basin (at spill)
    depth  = T.lpf(filled - node, cutoff_m=depth_smooth_m)         # basin CHARACTERISTIC depth (smooth)
    floor  = filled - playa * depth                                # flat fill, LOWERED -> flat low playa
    slope  = T.slope(node, radius_m=slope_radius_m)                # 0 flats .. 1 mountain flanks
    mask   = (1.0 - slope)
    mask   = mask * mask * mask * strength                         # concentrate in flats; scale per-pass
    return T.Mix(node, floor, mask)                                # flats -> low flat playa; ranges -> rugged


class XXX3Mat(Ptex3d):
    """Pure HEIGHT/SLOPE color classifier (NO texture) — hmview's exact palette. dirt ->
    grass with elevation, then a rock band, then snow; steep faces go rock. On the
    terraced terrain the elevation bands + the (steep) risers vs (flat) treads read as
    colored strata on their own — no procedural banding needed. Colors/thresholds are
    bindable (ctx.param); the only AA is mask-edge widening + a specular-AA roughness
    floor (both geometric, not texture)."""

    # baked terrain channels this material samples via ctx.tex(<sampler>):
    #   sampler uniform  <-  bake capture channel (an EXR in the terrain's asset cache).
    # bind_textures() (below) uploads + binds these onto the live PBRMaterial; the terrain
    # viewer just invokes the hook, so the FlowMap<-flow_discharge mapping lives HERE, in
    # the asset that samples it — not hardcoded in the generic viewer.
    SAMPLER_CHANNELS = {"FlowMap": "flow_discharge"}

    def __init__(self, ctx, *,
                 height_scale  = 4000.0,
                 mottle_scale  = 0.28,                       # the ONE shared noise frequency (broad mottle)
                 grass_lo = 0.26,
                 grass_hi = 0.39,           # h01: dirt -> grass (low ground)
                 rock_lo  = 0.42, 
                 rock_hi  = 0.60,           # h01: ground -> rock
                 snow_lo  = 0.72,                            # h01: rock -> snow (up to 1.0)
                 rock_slope_lo = 0.75, 
                 rock_slope_hi = 0.80, # slope: gentle -> cliff -> rock
                 dirt_color   = hsv( 30.0, 0.25, 0.25),       # 2-variant dirt/grass/rock (mottle blends each)
                 dirt_color2  = hsv( 48.0, 0.20, 0.10),
                 grass_color  = hsv(129.4, 0.15, 0.25),
                 grass_color2 = hsv(110.0, 0.10, 0.12),
                 rock_color   = hsv(  240.0, 0.1, 0.30),
                 rock_color2 = hsv( 230.0, 0.15, 0.40),
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

        mot   = P.fbm_aa(p * msc, 7, 0.5)
        #mot   = P.fbm(p * msc)
        dirt  = P.mix(cdrt, cdrt2, mot)
        grass = P.mix(cgrs, cgrs2, mot)
        rock  = P.mix(crok, crok2, mot)

        # ── strata: paint colored layers over a dirt base, lowest elevation first ──
        # Each layer is (select, color, roughness). `select` is a 0..1 geometric-AA
        # mask saying "how much THIS layer shows here". Painting in order means later
        # layers (snow, cliffs) sit on top of earlier ones (grass, rock).
        # aa_ramp + mix_layers are Ptex3d built-ins (self.*), shared by every material.
        grass_select = self.aa_ramp(h01,   g_lo, g_hi)    # elevation: dirt  -> grass
        rock_select  = self.aa_ramp(h01,   r_lo, r_hi)    # elevation: grass -> rock
        snow_select  = self.aa_ramp(h01,   s_lo, 1.0)     # elevation: rock  -> snow
        cliff_select = self.aa_ramp(slope, sl0,  sl1)     # steepness: any   -> rock (steep faces)
        ground_select = (1.0 - rock_select) * (1.0 - snow_select) * (1.0 - cliff_select)

        strata = [
            #  select        color   roughness
            ( grass_select,  grass,  0.90 ),
            ( rock_select,   rock,   0.62 ),
            ( snow_select,   csno,   0.85 ),
            ( cliff_select,  rock,   0.62 ),
        ]
        albedo, rough = self.mix_layers(dirt, 1.0, strata)   # dirt is the base layer

        flowd  = ctx.tex("FlowMap").x                      # [0,1] normalized log-discharge at ctx.uv
        ao = P.pow(1.0-flowd,0.6)
        #ao = P.mix(ao, 1, ground_select)   
        # specular AA (geometric): fold sub-pixel normal variance into roughness.
        n_var = P.dot(P.fwidth(ctx.N), P.fwidth(ctx.N))
        rough = P.saturate(P.sqrt(rough * rough + P.min(0.4, n_var * 2.0)))
        self.surface(albedo=albedo, metallic=0.0, roughness=rough, ao=ao)

        c_alb = self.capture("base",  albedo,  "xyz")
        c_rgh = self.capture("base",  rough,   "w")
        c_nrm = self.capture("nrmao", ctx.N,   "xyz")
        c_ao  = self.capture("nrmao", ao,      "w")
        self.surface_stored(
            albedo    = ctx.tex(c_alb),
            metallic  = 0,
            roughness = ctx.tex(c_rgh),
            normal    = ctx.tex(c_nrm),
            ao        = ctx.tex(c_ao))
    # bind_textures() is inherited from Ptex3d — it just consumes SAMPLER_CHANNELS above.


class XXX3(HeightField):
    # ---- authored PHYSICAL world scale (heights are TRUE METERS; relief = RELIEF_M) ----
    EXTENT_M = 32768.0
    # shader lives IN THIS FILE (XXX3Mat above) -> MATERIAL_CLASS. Pure height/slope color
    # (no texture); the terraces supply the strata structure. Defaults are fine, so no params.
    MATERIAL_CLASS  = XXX3Mat
    MATERIAL_PARAMS = {}#{"albedo":hsv(40,.20,0.2)}

    def __init__(self,iters = 16):
        ####################################
        super().__init__()
        ero_out = T.Const(0)
        ####################################
        base = (T.Fbm( P_FBM ) * 0.5 + 0.5) * RELIEF_M   # TRUE METERS (0..RELIEF_M)
        ero_bas = base*0.03
        ####################################
        # T.loop (not raw for): the document keeps ONE loop group per pass (editor-
        # collapsible, count editable). ero_bas is loop-invariant (created outside).
        with T.loop(iters, ero_out=ero_out) as L:
          ero_inp = L.ero_out+ero_bas
          bfill = T.basin_fill(ero_inp)
          bfill = (ero_inp*0.90)+(bfill*0.1)
          thr_out = T.erode_thermal( bfill,P_THERM)
          erox_out = T.erox( thr_out,P_EROX)
          pha_out = T.pha(erox_out,P_PHA)
          xxx_out = (erox_out*0.9) + (pha_out*0.1)
          terr     = self.hfdisplacement(terrace_strata, xxx_out)
          terr_out = T.Mix(xxx_out, terr, 0.1)
          L.ero_out = T.lpf(terr_out, cutoff_m=4)
        ero_out = L.ero_out
        ####################################
        with T.loop(DESCEND_ITERS, ero_out=ero_out) as L:
          L.ero_out = descend_basins(L.ero_out, strength=DESCEND_STRENGTH)
        ero_out = L.ero_out
        lpf_out = T.lpf(ero_out, cutoff_m=4)
        ero_out = (ero_out*0.15)+(lpf_out*0.85)
        lpf_out = T.lpf(ero_out, cutoff_m=2)
        ero_out = (ero_out*0.15)+(lpf_out*0.85)                # B.2: per-basin spill elevation (debug)
        flow    = T.flow3d(ero_out)     # MFD drainage area, log-compressed

        ero_out = T.normalize(ero_out,out_lo=0.0,out_hi=RELIEF_M)
        ####################################
        # STRATA TERRACING (the unified-substrate addition): snap to band elevations,
        # blended TERRACE toward the eroded height (so drainage detail survives), then a
        # light lpf to soften the risers into plausible benches.
        ####################################
        self._height = ero_out   # exposed for subclasses that add scatter masks (treeline/slope)
        self.capture(ero_out,"height",cache=True)
        self.capture(ero_out,"normal",cache=True)
        self.capture(flow.dir,"flow_dir",cache=True)
        # EXPLICIT [0,1] for the FlowMap sampler (natural units: the flush no longer
        # auto-exposes — a raw log-discharge > 1 turns the material AO pow() NaN-black).
        self.capture(T.normalize(flow.discharge),"flow_discharge",cache=True)
        self.capture(flow.metrics,"flow_metrics",cache=True)
        self.relax_uv(self._height)
       