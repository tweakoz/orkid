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
# ECOLOGY LINE (owner, round 3): ONE warped ~2000 m boundary — grass and trees STOP
# at it, and steep faces below it stay soil-mantled (more dirt than rock). Captured
# as a FIELD (channel "soil_line", TRUE METERS) so the scatter forks (trees/grass)
# and the material's rock gate consume the SAME irregular line — the grass_density
# coherence precedent: the boundary cannot disagree with itself.
###############################################################################
SOIL_LINE_M     = 2000.0    # the nominal ecology boundary
SOIL_LINE_AMP_M = 300.0     # +- a few hundred meters of meander
P_SOIL = T.ParamPack(
    frequency=6.0,          # ~5 km meander features over the 32.8 km domain
    octaves=3 )             # a little roughness on the line, no jitter
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
    depth  = T.lpf(filled - node, cutoff=depth_smooth_m, units='meters')         # basin CHARACTERISTIC depth (smooth)
    floor  = filled - playa * depth                                # flat fill, LOWERED -> flat low playa
    slope  = T.slope(node, radius_m=slope_radius_m)                # 0 flats .. 1 mountain flanks
    mask   = (1.0 - slope)
    mask   = mask * mask * mask * strength                         # concentrate in flats; scale per-pass
    return T.Mix(node, floor, mask)                                # flats -> low flat playa; ranges -> rugged


def _tan_deg(deg):
    """tan() of a DEGREES expression (params are runtime uniforms, so the conversion
    must run in-shader — sin/cos are the P primitives; there is no P.tan)."""
    r = deg * 0.0174532925
    return P.sin(r) / P.cos(r)


class XXX3Mat(Ptex3d):
    """ALPS mask-classifier (TERR2 round 1) — the standard alpine slope/altitude layer
    stack, computed from BAKED terrain analysis fields (SAMPLER_CHANNELS below), not from
    the smoothed render-mesh ctx.N.y/ctx.P.y. Elevation zonation (valley soil -> meadow ->
    tundra -> bare gravel) is a partition (aa_bands + mix_bands, no over-paint leak); rock
    is a slope over-paint (tan 35deg -> 50deg, fbm-warped edge, strata-banded neutral
    grey limestone/gneiss); snow is Alps-heavy: fbm-warped snowline ~2700 m, slope
    shedding (full <35deg, thin 35-50deg, gone >55deg), aspect bonus (away-from-sun faces
    hold), concave accumulation / convex wind scour — painted OVER rock so cliffs punch
    through. All thresholds are bindable ctx.param (A8); heights are TRUE METERS.

    ROUND 3 (shading truth + peaks): signed normal encode + data off the PNG alpha
    (see _emit); per-layer world-XZ procedural micro-height whose finite-diff gradient
    perturbs the baked normal while the SAME field modulates that layer's albedo,
    roughness and micro-AO (shading/color co-variance); dipped-strata cliff bands
    (20-30deg regional dip, asymmetric tread/riser sawtooth) that cut ACROSS the
    topography; snow drift/sastrugi structure with blue-shadowed hollows and rock ribs
    punching through steep sub-facets; rock value span widened to near-black..bright
    limestone; snow albedo capped at the physical 0.95."""

    # baked terrain channels this material samples via ctx.tex(<sampler>):
    #   sampler uniform  <-  bake capture channel (an EXR in the terrain's asset cache).
    # bind_textures() (below) uploads + binds these onto the live PBRMaterial; the terrain
    # viewer just invokes the hook, so the FlowMap<-flow_discharge mapping lives HERE, in
    # the asset that samples it — not hardcoded in the generic viewer.
    SAMPLER_CHANNELS = {"FlowMap":   "flow_discharge",
                        "HeightMap": "height",      # TRUE METERS (native bake grid)
                        "SlopeMap":  "slope8",      # Reinhard tan: s = t/(1+t), radius 8 m
                        "SlopeMap32":"slope32",     # ... radius 32 m (landform; snow shed)
                        "CurvC16":   "curv_c16",    # concave, 16 m  (cavities / gully walls)
                        "CurvC96":   "curv_c96",    # concave, 96 m  (hollows / cirques)
                        "CurvX96":   "curv_x96",    # convex,  96 m  (ridges / domes)
                        "FlowDir":   "flow_dir",    # RG = unit downhill dir *0.5+0.5
                        "FlowMetrics":"flow_metrics",  # B/z = TWI wetness (xxx3 flow3d packing)
                        "SoilLine":  "soil_line"}   # warped ~2000 m ecology line, TRUE METERS

    def __init__(self, ctx, **kw):
        albedo, rough, ao, masks = self._alpine_surface(ctx, **kw)
        self._emit(ctx, albedo, rough, ao, grad=masks.get("grad"))

    def _emit(self, ctx, albedo, rough, ao, grad=None):
        """Shared tail: specular AA, surface(), the atlas captures and their stored
        reconstruction. Forks (XXX3GrassMat) call _alpine_surface, blend their own
        layers, then this.

        ROUND 3, two capture-truth fixes scoped to THIS material family:
        1. SIGNED NORMAL ENCODE. The old capture wrote ctx.N raw into UNORM RGBA8 —
           the negative X/Z hemisphere clamped to 0 (25% of texels forced flat-up,
           18.8 deg mean shading-normal error, round-2 audit). Encode *0.5+0.5 here,
           decode *2-1 (+renormalize) in surface_stored below — the pair lives in this
           one function so no other surface_stored user changes meaning.
        2. DATA OFF THE ALPHA CHANNEL. The engine PNG writer converts associated ->
           unassociated alpha on save (RGB is DIVIDED by A): with roughness in base.w
           the stored albedo was scaled 1/rough (snow 0.87/0.78 clipped to pure white
           over 3.2% of the atlas; wet floors doubled), and ao in nrmao.w would have
           destroyed the signed encode the same way. Roughness+AO now ride their own
           'mrao' target; an UNFILLED w carries coverage 1.0, so the divide is a no-op
           and the WARM (png) atlas finally matches the COLD (direct RTG) bake.
        `grad` (vec2, d(micro_h)/d(x,z)) is the layers' co-varying micro-relief
        gradient: it perturbs the mesh normal BEFORE the encode, so baked shading and
        baked color agree by construction."""
        n_var = P.dot(P.fwidth(ctx.N), P.fwidth(ctx.N))
        rough = P.saturate(P.sqrt(rough * rough + P.min(0.4, n_var * 2.0)))
        N = ctx.N
        if grad is not None:
            bg = ctx.param("bump_gain", 1.0)      # micro-relief -> normal strength (live)
            N  = P.normalize(P.vec3(N.x - grad.x * bg, N.y, N.z - grad.y * bg))
        self.surface(albedo=albedo, metallic=0.0, roughness=rough, normal=N, ao=ao)
        c_alb = self.capture("base",  albedo,        "xyz")   # w UNFILLED -> coverage 1.0
        c_nrm = self.capture("nrmao", N * 0.5 + 0.5, "xyz")   # SIGNED->UNORM encode
        c_rgh = self.capture("mrao",  rough,         "x")
        c_ao  = self.capture("mrao",  ao,            "y")
        self.surface_stored(
            albedo    = ctx.tex(c_alb),
            metallic  = 0,
            roughness = ctx.tex(c_rgh),
            normal    = P.normalize(ctx.tex(c_nrm) * 2.0 - 1.0),   # UNORM->SIGNED decode
            ao        = ctx.tex(c_ao))

    @staticmethod
    def _grad_xz(fn, q, eps):
        """Finite-difference XZ gradient of a scalar micro-height expression `fn(q)`
        (meters), evaluated at bake resolution — the analytic-derivative bump of the
        procedural micro-relief (3 evals; bake-time cost only, the result bakes into
        the stored atlas). Returns (h, vec2(dh/dx, dh/dz))."""
        h0 = fn(q)
        hx = fn(q + P.vec3(1.0, 0.0, 0.0) * eps)
        hz = fn(q + P.vec3(0.0, 0.0, 1.0) * eps)
        return h0, P.vec2((hx - h0) / eps, (hz - h0) / eps)

    def _alpine_surface(self, ctx, *,
                 # ── noise scales (1/meters; the fbm base wavelength is ~1/scale) ──
                 mottle_scale   = 0.28,     # fine near-field mottle (sub-atlas at 8 m/texel)
                 macro_scale    = 0.005,    # ~200 m tonal patches — the atlas-visible mottle
                 # ── elevation zonation, TRUE METERS (partition edges) ──
                 valley_hi_m    = 700.0,    # valley soil -> meadow (owner spot-check at
                                            #   898 m read half-dirt with the edge at 900;
                                            #   the 700-2000 belt is now FULL meadow)
                 meadow_hi_m    = 1750.0,   # meadow -> alpine tundra (treeline ~1900)
                 alpine_hi_m    = 2500.0,   # tundra -> bare gravel/frost debris
                 band_soft_m    = 150.0,    # transition half-width
                 # ── rock (slope over-paint; DEGREES of true terrain angle) ──
                 rock_lo_deg    = 35.0,
                 rock_hi_deg    = 50.0,
                 rock_edge_warp = 0.35,     # +- fractional warp of the slope ramp edge
                 rock_warp_scale= 0.002,    # ~500 m warp features
                 rock_rough     = 0.85,     # matte needs >0.75 in this engine
                 # ── ecology line (samples the baked soil_line channel) ──
                 soil_soft_m    = 150.0,    # transition half-width about the warped line
                 rock_below_frac= 0.40,     # rock share kept on steep faces BELOW the line
                                            #   (the rest of the steep paint goes DIRT:
                                            #   dirt:rock ~ 1.5:1 — owner, round 3)
                 # ── ROUND 3: dipped strata + fractured micro-relief (the peaks' grey
                 #    bones). MATERIAL-side beds only — the geometric terrace keeps its
                 #    own ~7deg phase; these bands deliberately cut ACROSS the topography
                 #    (real strata dip 10-45deg along a consistent regional strike; the
                 #    round-2 contour-ring artifact was a ~7deg dip tracing elevation). ──
                 strata_dip_deg   = 24.0,   # true dip of the bed planes off horizontal
                 strata_azim_deg  = 35.0,   # regional dip azimuth (degrees, world XZ)
                 province_scale   = 0.00025,# ~4 km provinces of slowly-drifting strike
                 province_rot_deg = 55.0,   # +- azimuth drift across provinces
                 strata_period_m  = 940.0,  # bed spacing along the dip normal (owner-tuned:
                                            #   broad geological formations, not pinstripes)
                 strata_riser     = 0.40,   # riser fraction of the cycle (asymmetric sawtooth)
                 strata_step_m    = 3.5,    # ledge relief the riser drops (micro-height)
                 strata_warp      = 3.90,   # phase warp, cycles (owner: 3x) — bands meander
                                            #   hard; folded-formation read, never metric
                 strata_warp_scale= 0.0009, # ~1.1 km warp features: LARGE-scale irregularity
                                            #   (higher freq here = small-scale marble — avoid)
                 strata_tone_amt  = 0.30,   # per-bed albedo alternation (limestone/dolomite)
                 bed_gate_scale   = 0.0004, # ~2.5 km bedding provinces: NOT every massif is
                 bed_gate_lo      = 0.35,   #   visibly bedded — the gate fades the whole
                 bed_gate_hi      = 0.62,   #   strata read (relief+tone+riser) in and out,
                                            #   killing the uniform-corduroy artifact
                 riser_dark       = 0.45,   # shadowed-riser albedo x (dark, not pinstripe-black)
                 riser_ao         = 0.45,   # micro-AO carved under the risers
                 rock_frac_scale  = 0.018,  # fracture ridged-fbm, ~55 m features
                 rock_frac_m      = 7.0,    # fracture micro-relief amplitude (meters)
                 rock_frac_fine   = 0.35,   # fine fracture octave (x3.1 freq) amplitude ratio
                 rock_var         = 0.70,   # fracture-field albedo contrast (value span)
                 crack_ao         = 0.35,   # micro-AO in the fracture troughs
                 rock_rough_var   = 0.10,   # roughness co-variance off the same field
                 micro_eps_m      = 4.0,    # finite-diff step (half an 8 m atlas texel)
                 # ── snow (Alps-heavy) ──
                 snowline_m     = 2700.0,
                 snow_warp_m    = 250.0,    # +- fbm warp of the snowline
                 snow_warp_scale= 0.0005,   # ~2 km warp features
                 snow_soft_m    = 150.0,    # altitude ramp half-width
                 snow_full_deg  = 35.0,     # full cover below this slope
                 snow_thin_deg  = 50.0,     # ... fades to snow_thin_keep by this
                 snow_gone_deg  = 55.0,     # ... and is GONE above this
                 snow_thin_keep = 0.30,
                 snow_aspect_m  = 150.0,    # snowline drop on hold-aspect faces
                 snow_aspect_dir= (0.0, -1.0),  # horizontal world dir that HOLDS snow
                                            # (doubles as the sastrugi WIND direction)
                 snow_accum_m   = 80.0,     # snowline drop in concave hollows/cirques
                 snow_scour     = 0.30,     # convex-ridge cover loss (wind scour)
                 snow_rough     = 0.78,
                 # ── ROUND 3: snow micro-structure (drift/sastrugi + rock ribs) ──
                 snow_shadow    = (0.62, 0.67, 0.78),  # blue-shadowed accumulation hollows
                 snow_tone_gain = 1.6,      # drift field -> crest/hollow tone contrast
                 snow_rough_var = 0.10,     # crests wind-packed (lower rough), hollows fluffy
                 sast_amp_m     = 1.8,      # sastrugi micro-relief amplitude
                 sast_across    = 0.055,    # 1/m ACROSS the wind (~18 m ridge spacing)
                 sast_along     = 0.008,    # 1/m ALONG the wind (~125 m — elongated ridges)
                 drift_scale    = 0.004,    # ~250 m drift/accumulation field
                 drift_amp_m    = 2.5,
                 rib_lo         = 0.55,     # fracture-crest cut where rock RIBS shed snow
                 rib_hi         = 0.80,
                 rib_amt        = 0.85,     # how completely a rib sheds its snow
                 rib_slope      = 2.0,      # landform-slope gain gating ribs to steep faces
                 # ── palette (LINEAR physical albedo; rock sat <= 0.08, NO blue) ──
                 dirt_color     = hsv( 28.0, 0.30, 0.10),
                 dirt_color2    = hsv( 40.0, 0.25, 0.05),
                 grass_color    = hsv(108.0, 0.45, 0.15),   # genuinely GREEN meadow (the old
                 grass_color2   = hsv( 98.0, 0.40, 0.09),   #   sat-0.3 olive read grey once the
                                                            #   capture-truth fixes stopped
                                                            #   over-lighting the floor)
                 tundra_color   = hsv( 48.0, 0.22, 0.13),
                 tundra_color2  = hsv( 40.0, 0.18, 0.07),
                 gravel_color   = hsv( 40.0, 0.05, 0.22),
                 gravel_color2  = hsv( 35.0, 0.07, 0.12),
                 rock_color     = hsv( 40.0, 0.05, 0.40),   # bright weathered limestone tread
                 rock_color2    = hsv( 30.0, 0.08, 0.07),   # near-black fissure/shadow faces
                 snow_color     = (0.92, 0.93, 0.95),       # sunlit crest — physical cap 0.95
                 ground_rough   = 0.92,
                 grass_rough    = 0.88,
                 # ── ROUND 3: ground/scree micro-relief (world-XZ procedural, atlas-resolvable) ──
                 clast_scale    = 0.04,     # ~25 m talus clast cells
                 clast_amp_m    = 1.2,
                 hum_scale      = 0.025,    # ~40 m soil/meadow hummocks
                 hum_amp_m      = 0.8,
                 hum_tone       = 0.12,     # hummock albedo co-variance (subtle)
                 # ── ROUND 2: drainage wetness / sediment (wet = the ONE legit gloss) ──
                 wet_thresh     = 0.35,     # flow+TWI level where wetness starts
                 wet_gain       = 3.0,
                 wet_twi_w      = 0.15,     # TWI bonus vs discharge (TWI SATURATES on flat floors)
                 wet_slope_deg  = 20.0,     # wetness fades out above this slope
                 wet_dark       = 0.62,     # albedo x at full wet (0.5-0.7 spec)
                 wet_rough      = 0.50,     # 0.45-0.6: wet sediment gloss
                 silt_color     = (0.055, 0.045, 0.035),  # wet sediment 0.03-0.08
                 silt_amt       = 0.65,     # silt paint on concave low-slope fans
                 # ── ROUND 2: scree / talus ──
                 scree_lo_deg   = 30.0,     # slope band of a resting talus fan
                 scree_hi_deg   = 38.0,
                 scree_cgain    = 2.2,      # concave slope-foot gate gain (curv 96 m)
                 scree_scale    = 0.003,    # patchy-noise features ~330 m
                 scree_thresh   = 0.50,     # noise cut: higher = sparser fans
                 scree_color    = (0.25, 0.24, 0.23),     # 0.18-0.28 grey
                 scree_color2   = (0.18, 0.17, 0.16),
                 scree_rough    = 0.90,
                 # ── ROUND 2: cavity AO (two curvature radii ∪ the flow wash) + dirt ──
                 cavity_w16     = 0.35,     # 16 m concavity weight
                 cavity_w96     = 0.45,     # 96 m concavity weight
                 cavity_ao      = 0.40,     # how dark a full cavity goes
                 cavity_tint    = (0.82, 0.76, 0.70),     # dirt tint x in concavities
                 cavity_dirt    = 0.35,
                 # cavity shading belongs to SLOPED hollows (gully walls, slope-feet):
                 # the c96 concavity field saturates to 1.0 across entire flat basin
                 # floors, and ungated it painted whole valleys a grey-washed 25%
                 # darker (the owner's 898 m grey slab). A broad flat floor sees the
                 # whole sky — no occlusion.
                 cavity_flat_lo_deg = 3.0,  # cavity fades in from this terrain angle
                 cavity_flat_hi_deg = 10.0):
        msc  = ctx.param("mottle_scale",    mottle_scale)
        mac  = ctx.param("macro_scale",     macro_scale)
        vh   = ctx.param("valley_hi_m",     valley_hi_m)
        mh   = ctx.param("meadow_hi_m",     meadow_hi_m)
        ah   = ctx.param("alpine_hi_m",     alpine_hi_m)
        bs   = ctx.param("band_soft_m",     band_soft_m)
        rlo  = ctx.param("rock_lo_deg",     rock_lo_deg)
        rhi  = ctx.param("rock_hi_deg",     rock_hi_deg)
        rew  = ctx.param("rock_edge_warp",  rock_edge_warp)
        rws  = ctx.param("rock_warp_scale", rock_warp_scale)
        rrg  = ctx.param("rock_rough",      rock_rough)
        slsf = ctx.param("soil_soft_m",     soil_soft_m)
        rbf  = ctx.param("rock_below_frac", rock_below_frac)
        sdp  = ctx.param("strata_dip_deg",   strata_dip_deg)
        saz  = ctx.param("strata_azim_deg",  strata_azim_deg)
        pvs  = ctx.param("province_scale",   province_scale)
        pvr  = ctx.param("province_rot_deg", province_rot_deg)
        spm  = ctx.param("strata_period_m",  strata_period_m)
        srf  = ctx.param("strata_riser",     strata_riser)
        sstp = ctx.param("strata_step_m",    strata_step_m)
        swp  = ctx.param("strata_warp",      strata_warp)
        sw2  = ctx.param("strata_warp_scale",strata_warp_scale)
        sbt  = ctx.param("strata_tone_amt",  strata_tone_amt)
        bgs  = ctx.param("bed_gate_scale",   bed_gate_scale)
        bglo = ctx.param("bed_gate_lo",      bed_gate_lo)
        bghi = ctx.param("bed_gate_hi",      bed_gate_hi)
        rdk  = ctx.param("riser_dark",       riser_dark)
        rao  = ctx.param("riser_ao",         riser_ao)
        rfsc = ctx.param("rock_frac_scale",  rock_frac_scale)
        rfm  = ctx.param("rock_frac_m",      rock_frac_m)
        rff  = ctx.param("rock_frac_fine",   rock_frac_fine)
        rvv  = ctx.param("rock_var",         rock_var)
        cao  = ctx.param("crack_ao",         crack_ao)
        rrv  = ctx.param("rock_rough_var",   rock_rough_var)
        meps = ctx.param("micro_eps_m",      micro_eps_m)
        sl   = ctx.param("snowline_m",      snowline_m)
        swm  = ctx.param("snow_warp_m",     snow_warp_m)
        sws  = ctx.param("snow_warp_scale", snow_warp_scale)
        ss   = ctx.param("snow_soft_m",     snow_soft_m)
        sfd  = ctx.param("snow_full_deg",   snow_full_deg)
        stdg = ctx.param("snow_thin_deg",   snow_thin_deg)
        sgd  = ctx.param("snow_gone_deg",   snow_gone_deg)
        stk  = ctx.param("snow_thin_keep",  snow_thin_keep)
        sam  = ctx.param("snow_aspect_m",   snow_aspect_m)
        sad  = ctx.param("snow_aspect_dir", snow_aspect_dir)
        scm  = ctx.param("snow_accum_m",    snow_accum_m)
        ssc  = ctx.param("snow_scour",      snow_scour)
        srg  = ctx.param("snow_rough",      snow_rough)
        csns = ctx.param("snow_shadow",     snow_shadow)
        stg  = ctx.param("snow_tone_gain",  snow_tone_gain)
        srv  = ctx.param("snow_rough_var",  snow_rough_var)
        s_amp= ctx.param("sast_amp_m",      sast_amp_m)
        s_acr= ctx.param("sast_across",     sast_across)
        s_alo= ctx.param("sast_along",      sast_along)
        s_dsc= ctx.param("drift_scale",     drift_scale)
        s_dam= ctx.param("drift_amp_m",     drift_amp_m)
        ribl = ctx.param("rib_lo",          rib_lo)
        ribh = ctx.param("rib_hi",          rib_hi)
        riba = ctx.param("rib_amt",         rib_amt)
        ribs_= ctx.param("rib_slope",       rib_slope)
        cls_ = ctx.param("clast_scale",     clast_scale)
        cla  = ctx.param("clast_amp_m",     clast_amp_m)
        hsc  = ctx.param("hum_scale",       hum_scale)
        ham  = ctx.param("hum_amp_m",       hum_amp_m)
        htn  = ctx.param("hum_tone",        hum_tone)
        cdrt = ctx.param("dirt_color",      dirt_color)
        cdrt2= ctx.param("dirt_color2",     dirt_color2)
        cgrs = ctx.param("grass_color",     grass_color)
        cgrs2= ctx.param("grass_color2",    grass_color2)
        ctnd = ctx.param("tundra_color",    tundra_color)
        ctnd2= ctx.param("tundra_color2",   tundra_color2)
        cgrv = ctx.param("gravel_color",    gravel_color)
        cgrv2= ctx.param("gravel_color2",   gravel_color2)
        crok = ctx.param("rock_color",      rock_color)
        crok2= ctx.param("rock_color2",     rock_color2)
        csno = ctx.param("snow_color",      snow_color)
        grg  = ctx.param("ground_rough",    ground_rough)
        gsr  = ctx.param("grass_rough",     grass_rough)
        wth  = ctx.param("wet_thresh",      wet_thresh)
        wgn  = ctx.param("wet_gain",        wet_gain)
        wtw  = ctx.param("wet_twi_w",       wet_twi_w)
        wsd  = ctx.param("wet_slope_deg",   wet_slope_deg)
        wdk  = ctx.param("wet_dark",        wet_dark)
        wrg  = ctx.param("wet_rough",       wet_rough)
        csil = ctx.param("silt_color",      silt_color)
        sia  = ctx.param("silt_amt",        silt_amt)
        klo  = ctx.param("scree_lo_deg",    scree_lo_deg)
        khi  = ctx.param("scree_hi_deg",    scree_hi_deg)
        kcg  = ctx.param("scree_cgain",     scree_cgain)
        ksc  = ctx.param("scree_scale",     scree_scale)
        kth  = ctx.param("scree_thresh",    scree_thresh)
        cscr = ctx.param("scree_color",     scree_color)
        cscr2= ctx.param("scree_color2",    scree_color2)
        krg  = ctx.param("scree_rough",     scree_rough)
        cv16 = ctx.param("cavity_w16",      cavity_w16)
        cv96 = ctx.param("cavity_w96",      cavity_w96)
        cvao = ctx.param("cavity_ao",       cavity_ao)
        cvt  = ctx.param("cavity_tint",     cavity_tint)
        cvd  = ctx.param("cavity_dirt",     cavity_dirt)
        cflo = ctx.param("cavity_flat_lo_deg", cavity_flat_lo_deg)
        cfhi = ctx.param("cavity_flat_hi_deg", cavity_flat_hi_deg)

        p    = ctx.P_object
        ph   = P.vec3(p.x, 0.0, p.z)                # horizontal domain for all field noise

        # ── the baked analysis fields (native-res, landform-honest) ──
        h_m  = ctx.tex("HeightMap").x               # TRUE METERS
        s01  = ctx.tex("SlopeMap").x                # Reinhard: t/(1+t)
        tanS = s01 / P.max(1.0 - s01, 0.02)         # decode -> tan(terrain angle), 8 m
        s32  = ctx.tex("SlopeMap32").x              # landform slope (32 m) — snow shed
        tanL = s32 / P.max(1.0 - s32, 0.02)
        c16  = ctx.tex("CurvC16").x                 # cavities / gully walls
        c96  = ctx.tex("CurvC96").x                 # hollows / cirques
        x96  = ctx.tex("CurvX96").x                 # ridges / domes
        fdir = ctx.tex("FlowDir").xy * 2.0 - 1.0    # unit downhill dir (horizontal)
        twi  = ctx.tex("FlowMetrics").z             # TWI wetness (flow3d metrics .B)

        mot  = P.fbm_aa(ph * msc, 7, 0.5)           # fine mottle (near field)
        motM = P.fbm_aa(ph * mac, 5, 0.5)           # macro mottle (~200 m patches)
        tone = P.saturate(mot * 0.4 + motM * 0.6)

        # ══ ROUND 3: per-layer PROCEDURAL micro-height fields (world-XZ fbm/ridged/
        #    worley — deliberately DECOUPLED from the HeightMap: the terrain content is
        #    spectrally dead <128 m and stays untouched this round). Each layer's
        #    relief, albedo modulation, roughness variance and micro-AO come from the
        #    SAME expression; its finite-diff XZ gradient (bake-resolution "analytic"
        #    derivative) perturbs the capture normal in _emit — so baked shading and
        #    baked color CO-VARY by construction (the anti-painted-on contract).
        #    Terrain fields (flow_dir/slope/aspect) only ORIENT and GATE the detail. ══

        # dipped strata: bed-plane normal from dip+azimuth; the strike drifts slowly
        # province to province so the banding stays regional, never global-striped.
        azr  = (saz + (P.fbm(ph * pvs, 2) - 0.5) * pvr) * 0.0174532925
        dipr = sdp * 0.0174532925
        bedn = P.vec3(P.sin(dipr) * P.cos(azr), P.cos(dipr), P.sin(dipr) * P.sin(azr))
        # bedding-province gate: fades the WHOLE strata read (relief + riser + bed tone)
        # region to region — real ranges alternate visibly-bedded and massive rock; a
        # uniform global corduroy is the artifact. Frozen at the fragment (a ~2.5 km
        # field's own gradient is negligible against the micro-relief's).
        bamt = P.smoothstep(bglo, bghi, P.fbm(ph * bgs, 3))

        def _strata_ph(qq):
            # 2-octave warp: the finest octave is ~half the warp scale, so the bed
            # boundaries meander at hundreds of meters — never wiggle into marble.
            return P.dot(qq, bedn) / spm + (P.fbm(qq * sw2, 2) - 0.5) * swp

        def _rock_h(qq):
            t   = P.fract(_strata_ph(qq))
            up  = P.smoothstep(0.0, 1.0 - srf, t)    # slow rise across the tread
            dwn = P.smoothstep(1.0 - srf, 1.0, t)    # sharp drop across the riser
            rid = 1.0 - P.abs(P.fbm(qq * rfsc, 4) * 2.0 - 1.0)         # fracture ridges
            rid2= 1.0 - P.abs(P.fbm(qq * rfsc * 3.1, 3) * 2.0 - 1.0)   # fine fracture
            return (up - dwn) * sstp * bamt + (rid + rid2 * rff) * rfm  # sawtooth + fracture

        wd = P.normalize(sad)                        # sastrugi wind = the snow-hold dir

        def _snow_h(qq):
            u = qq.x * wd.x + qq.z * wd.y            # along the wind
            v = qq.x * wd.y - qq.z * wd.x            # across the wind
            sast  = P.fbm(P.vec3(v * s_acr, 17.0, u * s_alo), 4)  # wind-elongated ridges
            drift = P.fbm(qq * s_dsc, 3)
            return (sast - 0.5) * 2.0 * s_amp + (drift - 0.5) * 2.0 * s_dam

        def _scree_h(qq):
            return P.voronoi(qq * cls_).fwedge * cla  # clast domes

        def _ground_h(qq):
            return (P.fbm(qq * hsc, 3) - 0.5) * 2.0 * ham   # soil/meadow hummocks

        # center-point copies of the fields for the ALBEDO/AO co-variance (the emitter
        # CSEs these against the gradient's center evals — no double cost)
        t0     = P.fract(_strata_ph(p))
        riserw = P.smoothstep(1.0 - srf, 1.0, t0)
        rmask  = P.saturate(riserw * (1.0 - riserw) * 4.0) * bamt   # mid-riser band, gated
        rid0   = 1.0 - P.abs(P.fbm(p * rfsc, 4) * 2.0 - 1.0)
        rid0   = P.saturate(rid0 + (1.0 - P.abs(P.fbm(p * rfsc * 3.1, 3) * 2.0 - 1.0)) * rff
                            - rff * 0.5)   # + the fine octave the relief carries (co-vary)
        bedv   = P.noise(P.vec3(P.floor(_strata_ph(p)) * 0.37 + 11.0, 5.0,
                                P.floor(_strata_ph(p)) * 0.113))  # per-bed tone hash
        u0     = p.x * wd.x + p.z * wd.y
        v0     = p.x * wd.y - p.z * wd.x
        sast0  = P.fbm(P.vec3(v0 * s_acr, 17.0, u0 * s_alo), 4)
        drift0 = P.fbm(p * s_dsc, 3)
        crest  = P.saturate(((sast0 - 0.5) * 1.2 + (drift0 - 0.5) * 0.8) * stg + 0.5)
        clast0 = P.voronoi(p * cls_).fwedge
        hum0   = P.saturate(P.fbm(p * hsc, 3))

        dirt   = P.mix(cdrt, cdrt2, tone)
        grass  = P.mix(cgrs, cgrs2, tone)
        tundra = P.mix(ctnd, ctnd2, tone)
        gravel = P.mix(cgrv, cgrv2, tone)

        # ── elevation zonation: a PARTITION (mix_bands), not over-paint ──
        e_val, e_mdw, e_tnd, e_grv = self.aa_bands(
            h_m, 0.0, vh, mh, ah, RELIEF_M, soft=bs)
        albedo, rough = self.mix_bands(dirt, grg, [
            (e_val, dirt,   grg),
            (e_mdw, grass,  gsr),
            (e_tnd, tundra, grg),
            (e_grv, gravel, grg),
        ])

        # ground micro: hummock relief + the SAME field nudging the soil tone
        _, g_gnd = self._grad_xz(_ground_h, p, meps)
        grad     = g_gnd                              # vec2 d(micro_h)/d(x,z) accumulator
        mao      = P.saturate(1.0 - (1.0 - hum0) * htn)   # micro-AO accumulator
        albedo   = albedo * P.mix(1.0 - htn, 1.0 + htn, hum0)

        # ── rock: slope over-paint tan(35deg)->tan(50deg), fbm-warped edge. ROUND 3:
        #    the tone comes from the DIPPED strata sawtooth + fracture field (the same
        #    expressions that carve the relief): bright weathered treads, per-bed
        #    alternation, near-black shadowed risers — cliff bands that cut ACROSS the
        #    topography instead of the old contour-ringing sin(elevation) band. ──
        rockw   = (P.fbm(ph * rws, 4) - 0.5) * 2.0
        tan_eff = tanS * (1.0 + rockw * rew)
        rock_select = self.aa_ramp(tan_eff, _tan_deg(rlo), _tan_deg(rhi))
        # ── ECOLOGY LINE (owner, round 3): below the warped ~2000 m soil line the
        #    steep-face paint splits — rock keeps rock_below_frac of it and the
        #    REMAINDER goes DIRT (not meadow green): low peaks read soil-mantled
        #    earth, the high massifs read stone. The line is the SAME baked field
        #    the grass and tree scatters stop at. ──
        sline  = ctx.tex("SoilLine").x              # warped boundary, TRUE METERS
        above  = self.aa_ramp(h_m, sline - slsf, sline + slsf)
        rock_w = rock_select * P.mix(rbf, 1.0, above)
        dirt_w = rock_select - rock_w               # the sub-line steep remainder
        albedo = P.mix(albedo, dirt, dirt_w)
        rough  = P.mix(rough,  grg,  dirt_w)
        rockshade = P.saturate((rid0 - 0.5) * rvv + 0.5)
        rock  = P.mix(crok2, crok, rockshade)               # near-black .. bright limestone
        rock  = P.mix(rock, rock * (0.6 + 0.8 * bedv), sbt * bamt)  # bed alternation, gated
        rock  = rock * P.mix(1.0, rdk, rmask)               # shadowed riser faces
        albedo = P.mix(albedo, rock, rock_w)
        rough  = P.mix(rough, P.saturate(rrg + (rid0 - 0.5) * rrv), rock_w)
        _, g_rk = self._grad_xz(_rock_h, p, meps)
        grad   = P.mix(grad, g_rk, rock_w)
        mao    = P.mix(mao, P.saturate(1.0 - rmask * rao - (1.0 - rid0) * cao), rock_w)

        # ── ROUND 2 scree/talus: the 30-38deg resting-angle band ⊗ concave slope-foot
        #    (curv 96 m collects below the cliffs) ⊗ patchy noise -> grey debris fans.
        #    ROUND 3: clast-cell micro-relief; domes lit, interstices dark (co-vary). ──
        sband  = self.aa_band(tanS, _tan_deg(klo), _tan_deg(khi))
        spat   = P.smoothstep(kth, kth + 0.15, P.fbm(ph * ksc, 4))
        scree_w = P.saturate(sband * P.saturate(c96 * kcg) * spat)
        scree   = P.mix(cscr, cscr2, tone) * P.mix(0.75, 1.15, clast0)
        albedo  = P.mix(albedo, scree, scree_w)
        rough   = P.mix(rough,  krg,   scree_w)
        _, g_sc = self._grad_xz(_scree_h, p, meps)
        grad    = P.mix(grad, g_sc, scree_w)

        # ── ROUND 2 wetness: discharge + TWI ⊗ low slope -> darker, glossier sediment.
        #    Wet is the ONE legitimate gloss in this engine (0.45-0.6); silt tints the
        #    concave low-slope fans the water actually reaches. ──
        flowd  = ctx.tex("FlowMap").x               # [0,1] normalized log-discharge
        lowsl  = 1.0 - self.aa_ramp(tanS, _tan_deg(wsd * 0.4), _tan_deg(wsd))
        wet    = P.saturate((flowd + twi * wtw - wth) * wgn) * lowsl
        wet *= 0.5 # FIX sheen
        silt_w = wet * P.saturate(c96 * 2.0) * sia
        csil = (0.65,0.25,0) # fix grey
        albedo = P.mix(albedo, csil, silt_w)
        albedo = albedo * P.mix(1.0, wdk, wet)
        rough  = P.mix(rough, wrg, wet)
        grad   = grad * (1.0 - wet * 0.7)           # sediment planes the micro-relief off

        # ── snow OVER rock (cliffs punch through via the slope shed) ──
        # slope shedding runs on the LANDFORM slope (32 m): the 8 m field sees every
        # gully wall and shreds the pack into lace — a snowpack smooths over that scale
        # and only sustained faces (still steep at 32 m) shed to bare rock.
        wsn      = (P.fbm(ph * sws, 4) - 0.5) * 2.0 * swm       # warped snowline
        aspect   = P.dot(fdir, P.normalize(sad))                # 1 = faces the hold dir
        aspect   = P.saturate(aspect) * P.saturate(tanL * 2.0)  # meaningless on flats
        line_eff = sl - wsn - aspect * sam - c96 * scm          # hollows + hold faces: lower line
        snow_alt = self.aa_ramp(h_m, line_eff - ss, line_eff + ss)
        shed     = 1.0 - self.aa_ramp(tanL, _tan_deg(sfd), _tan_deg(stdg)) * (1.0 - stk)
        shed     = shed * (1.0 - self.aa_ramp(tanL, _tan_deg(stdg), _tan_deg(sgd)))
        # ROUND 3: rock RIBS punch through on steep sub-facets of the SAME fracture
        # field the rock relief/albedo use — the peaks get grey bones, not stripes.
        rib      = self.aa_ramp(rid0 * P.saturate(tanL * ribs_), ribl, ribh) * riba
        snow_cov = P.saturate(snow_alt * shed * (1.0 - x96 * ssc) * (1.0 - rib))
        # drift/sastrugi tone off the SAME micro field: bright wind-packed crests,
        # blue-shadowed accumulation hollows; crests pack icier (lower roughness).
        snowc    = P.mix(csns, csno, crest)
        albedo   = P.mix(albedo, snowc, snow_cov)
        rough    = P.mix(rough, P.saturate(srg - (crest - 0.5) * srv * 2.0), snow_cov)
        _, g_sn  = self._grad_xz(_snow_h, p, meps)
        grad     = P.mix(grad, g_sn, snow_cov)

        # ── ROUND 2 AO: cavity (concavity at 16 m + 96 m) ∪ the flow-discharge wash —
        #    no longer pow(1-flow, .6) alone. A slight dirt tint settles in concavities. ──
        cav    = P.saturate(c16 * cv16 + c96 * cv96)
        cav    = cav * self.aa_ramp(tanS, _tan_deg(cflo), _tan_deg(cfhi))  # slopes only
        ao     = P.pow(1.0 - flowd, 0.6) * (1.0 - cav * cvao) * mao   # ∪ the micro-AO
        albedo = albedo * P.mix(P.vec3(1.0, 1.0, 1.0), cvt, cav * cvd * (1.0 - snow_cov))

        masks = {"snow": snow_cov, "rock": rock_w, "wet": wet, "scree": scree_w,
                 "grad": grad}   # the layers' micro-relief gradient -> _emit's normal

        return albedo, rough, ao, masks
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
          L.ero_out = T.lpf(terr_out, cutoff=4, units='meters')
        ero_out = L.ero_out
        ####################################
        with T.loop(DESCEND_ITERS, ero_out=ero_out) as L:
          L.ero_out = descend_basins(L.ero_out, strength=DESCEND_STRENGTH)
        ero_out = L.ero_out
        lpf_out = T.lpf(ero_out, cutoff=4, units='meters')
        ero_out = (ero_out*0.15)+(lpf_out*0.85)
        lpf_out = T.lpf(ero_out, cutoff=2, units='meters')
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
        ####################################
        # MASK SUBSTRATE (TERR2 round 1): landform-scale analysis fields the material
        # classifier samples via SAMPLER_CHANNELS — replacing render-mesh ctx.N.y/ctx.P.y
        # masks. Pure functions of the FINAL height (cheap ops, cook-cached); the erosion
        # chain above is untouched, so its channels stay cache-hits.
        ####################################
        # the shared ecology line (see the module header): a field, so forks can gate
        # their scatters on it and the material samples the identical boundary.
        # T.normalize pins the fbm to a TRUE [-1,1] span (measured raw output is
        # ~[0.16,0.92] — an uncentered line was the round-3 "line sits at 2160+-115"
        # bug), so the boundary really meanders SOIL_LINE_M +- SOIL_LINE_AMP_M.
        self._soil_line = SOIL_LINE_M \
            + T.normalize(T.Fbm(P_SOIL), out_lo=-1.0, out_hi=1.0) * SOIL_LINE_AMP_M
        self.capture(self._soil_line, "soil_line", cache=True)
        self.capture(T.slope(ero_out, radius_m=8.0),                      "slope8",   cache=True)
        self.capture(T.slope(ero_out, radius_m=32.0),                     "slope32",  cache=True)
        self.capture(T.curvature(ero_out, mode="concave", radius_m=16.0), "curv_c16", cache=True)
        self.capture(T.curvature(ero_out, mode="concave", radius_m=96.0), "curv_c96", cache=True)
        self.capture(T.curvature(ero_out, mode="convex",  radius_m=96.0), "curv_x96", cache=True)
        self.relax_uv(self._height)
       