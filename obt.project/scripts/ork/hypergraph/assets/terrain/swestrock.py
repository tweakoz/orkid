###############################################################################
# swestrock — PHASE-1 STRATA PROTOTYPE for the southwest terrain campaign
# (SWEST2 plan of record §3-§6, §10 Phase 1). Standalone asset, no village.
#
# TECHNIQUE (industry recipe -> orkid): the World-Machine/Houdini layered-rock
# stack —
#   A. REGION PLAN first (layout masks): warped-edge mesa-wall band on the
#      north edge + a warped elliptical badlands blob east + open flat.
#   B. BASE UPLIFT: flat plain vs flat plateau top ("flat says sedimentary").
#   C. STRATIGRAPHY COLUMN: ONE 1-D column function — an author-definable list
#      of (thickness_m, hardness, color_id) — evaluated at warped/dipped
#      elevation.  THE SINGLE-SOURCE LAW: this one function drives the
#      hardness field (erosion), the terrace displacement (silhouette), and
#      the material color (SWEST2 risk #2 — bands must coincide at rims).
#   D. EDGE WARP on the region masks (rims scallop, never circles).
#   E. TERRACE BY LAYER HARDNESS via hfdisplacement (strata_bake skeleton,
#      generalized to a variable column): hard beds hold near-vertical risers,
#      soft beds slope back.
#   F. FLUVIAL: outer cheap loops (flow3d -> flow_erode stream-power + thermal
#      masked to cliff-base) then ONE T.erox (Mei virtual-pipes) quality pass
#      with `erodibility=` RECOMPUTED from the CURRENT height (the T.erox
#      docstring recompute pattern — the field is static per pass).
#   G. THERMAL TALUS masked to cliff feet -> 35deg aprons; the pass delta is
#      captured ("talus_delta") so the material darkens the parent rock there.
#   H. BADLANDS: fine 1-4 m column terrace + T.pha rilling, blend=badlands.
#   J. COLOR KEYED TO LAYER (not elevation-bands): §3 palette — Wingate
#      red-orange, buff caprock, Chinle purple/maroon/grey-green, ash ONLY in
#      badlands.  Standing rule: NO WHITE outside the badlands ash band.
#
# METERS LAW: heights are TRUE METERS end to end; there is NO final normalize —
# the column IS the vertical calibration (a normalize would shear the material
# bands off the geometric ledges).
#
# A8: erosion loop counts / sim times / terrace strengths / extent ride ctor
# kwargs (scene dsl_kwargs / env); the column tables are author-definable
# module data (owner decision 5: thickness entries are per-stratum parameters);
# material palette anchors ride ctx.param (live-pokeable).
#
#   ork.terrain.viewer2.py -d 1024 swestrock          (owner live view)
#   scene: ork.data/scenes/scn_swestrock.py
###############################################################################
import os

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.dsl import SurfaceCtx
from orkengine.core import vec3, vec2

###############################################################################
# STRATIGRAPHY COLUMNS — author-definable (thickness_m, hardness, color_id),
# bottom-up. Owner numbers: badlands bands 1-4 m; mesa section thicker
# (caprock ~8 m hard 0.9 / cliff sandstone ~40 m 0.7 / siltstone ~25 m 0.25 /
# shale ~15 m 0.1 class).
###############################################################################
MESA_COLUMN = [
    (30.0, 0.15, "wash"),       #   0- 30  valley fill / wash sand
    (15.0, 0.10, "purple"),     #  30- 45  Chinle shale (purple/maroon)
    (10.0, 0.16, "greygreen"),  #  45- 55  Chinle grey-green
    (25.0, 0.25, "silt"),       #  55- 80  siltstone slope-former
    (40.0, 0.70, "wingate"),    #  80-120  Wingate red-orange — main cliff
    (18.0, 0.22, "chinle2"),    # 120-138  soft bench (slope-back break)
    (38.0, 0.68, "wingate2"),   # 138-176  upper cliff band (two bands, §3)
    ( 8.0, 0.90, "cap"),        # 176-184  buff caprock, sharp rim
    (240.0, 0.16, "tuff"),      # 184-424  OVERBURDEN: soft tuff/gravel
                                # piedmont above the caprock — covers the
                                # owner-scale pediment (420 m) so high ground
                                # keeps honest strata instead of saturating at
                                # cap color/hardness
]
BAD_COLUMN = [                  # fine column, WRAPPED (Chinle repeats); 1-4 m
    (2.5, 0.14, "purple"),
    (1.2, 0.28, "greygreen"),
    (3.5, 0.10, "maroon"),
    (1.0, 0.35, "ash"),         # the ONLY near-white, and only in badlands
    (2.8, 0.12, "greygreen"),
    (1.5, 0.22, "purple"),
    (4.0, 0.10, "maroon"),
    (1.8, 0.30, "ash"),
]

# §3 palette — linear RGB anchors, each belonging to a NAMED rock layer.
PALETTE_ANCHORS = {
    "cap":       (0.60, 0.53, 0.38),   # buff caprock (darkened from §3's
                                       # 0.72 — wide flat tops at noon read
                                       # snow-pale, the meringue disease)
    "wingate":   (0.62, 0.32, 0.18),   # Wingate red-orange
    "wingate2":  (0.66, 0.38, 0.22),   # upper cliff, slightly lighter
    "silt":      (0.55, 0.34, 0.22),   # red-brown siltstone
    "chinle2":   (0.50, 0.36, 0.28),   # soft bench
    "purple":    (0.42, 0.28, 0.30),   # Chinle purple/maroon
    "maroon":    (0.45, 0.26, 0.24),
    "greygreen": (0.40, 0.42, 0.34),
    "ash":       (0.64, 0.61, 0.54),   # badlands ash — palest band allowed
                                       # (toned from §3's 0.70 — read snow-
                                       # bright on wide mound-top exposures)
    "wash":      (0.66, 0.58, 0.44),   # wash sand
    "sage":      (0.36, 0.38, 0.29),   # sage-flat stipple green
    "tuff":      (0.55, 0.48, 0.38),   # piedmont tuff/gravel overburden
}

# regional dip + low-freq band warp (bands undulate, never dead level). The
# SAME constants feed the terrain displacement AND the material — coincidence
# by construction (they fold into both compiled expressions together).
DIP_X       = 0.004     # 0.4% east dip  (~33 m across 8192)
DIP_Z       = 0.007     # 0.7% south dip
WARP_AMP_M  = 6.0       # vertical band undulation
WARP_FREQ   = 1.0 / 900.0

SEED = 71               # every noise offset derives from this (deterministic)


def _off(k):
    """Stateless seeded offset for a noise basis (counter-hash style)."""
    h = (SEED * 1013 + k * 7919) % 9973
    return vec2(float(h) * 3.7, float((h * 31) % 997) * 2.3)


def _bases(column):
    b = [0.0]
    for th, _hd, _cid in column:
        b.append(b[-1] + th)
    return b


def strata_yy(x, z, y):
    """The warped/dipped column elevation — THE single-source coordinate. The
    warp noise is y-INDEPENDENT (xz only) so intermediate-vs-final height never
    shears the material bands off the terraced geometry."""
    w = P.noise(P.vec3(x * WARP_FREQ, 37.7, z * WARP_FREQ))
    return y - DIP_X * x - DIP_Z * z - (w - 0.5) * (2.0 * WARP_AMP_M)


def col_hardness(yv, column):
    """Piecewise-constant hardness of the column at elevation yv (SurfNode)."""
    b = _bases(column)
    yc = P.clamp(yv, 1.0e-3, b[-1] - 1.0e-3)
    acc = None
    for i, (th, hd, _cid) in enumerate(column):
        m = P.step(b[i], yc) - P.step(b[i + 1], yc)
        acc = m * hd if acc is None else acc + m * hd
    return acc


def col_snap(yv, column, w_min=0.07, w_max=0.48):
    """Terrace-by-hardness height remap (the strata_bake terrace generalized to
    a variable column): within each band t' = smoothstep(0.5-w, 0.5+w, t) with
    w keyed to hardness — hard band = tight S (near-vertical riser through the
    band, small ledges at its top/bottom edges = the rim bench), soft band =
    near-linear (slopes back). f(b_i) = b_i at every boundary, so band
    ELEVATIONS never move — color stays coincident with the ledge."""
    b = _bases(column)
    yc = P.clamp(yv, 1.0e-3, b[-1] - 1.0e-3)
    acc = None
    for i, (th, hd, _cid) in enumerate(column):
        m = P.step(b[i], yc) - P.step(b[i + 1], yc)
        t = (yc - b[i]) * (1.0 / th)
        w = w_max + (w_min - w_max) * (hd ** 0.7)
        tt = P.smoothstep(0.5 - w, 0.5 + w, t)
        v = b[i] + th * tt
        acc = m * v if acc is None else acc + m * v
    return acc + (yv - yc)     # out-of-column heights pass through


def col_color(yv, column, pal):
    """Chained-mix layer color at elevation yv; edge softness scales with the
    thinner neighbour band (fine badlands bands keep crisp 0.1 m edges)."""
    c = pal[column[0][2]]
    b = _bases(column)
    for i in range(1, len(column)):
        e = max(0.06, min(0.8, 0.25 * min(column[i - 1][0], column[i][0])))
        c = P.mix(c, pal[column[i][2]], P.smoothstep(b[i] - e, b[i] + e, yv))
    return c


_BAD_TOTAL = _bases(BAD_COLUMN)[-1]


###############################################################################
# MATERIAL — stratum-keyed (stage J). Color comes from the SAME column function
# the geometry terraced by, evaluated at ctx.P.y — coincidence at rims is by
# construction, not by matching two authored tables. Surfacing extras ride
# baked channels through the SAMPLER_CHANNELS seam (the swestvale pattern):
# talus darkens the parent layer, discharge keys wash sand + AO, badlands mask
# gates the fine column + the ash allowance, varnish streaks steep faces.
###############################################################################
class Material(Ptex3d):
    SAMPLER_CHANNELS = {
        "FlowMetrics":   "flow_metrics",
        "FlowDischarge": "flow_discharge",
        "TalusDelta":    "talus_delta",
        "Badlands":      "badlands",
        "Normal":        "normal",
    }

    def __init__(self, ctx,
                 # palette anchors as DATA (A8: ctx.param rows, live-pokeable)
                 pal_cap       = PALETTE_ANCHORS["cap"],
                 pal_wingate   = PALETTE_ANCHORS["wingate"],
                 pal_wingate2  = PALETTE_ANCHORS["wingate2"],
                 pal_silt      = PALETTE_ANCHORS["silt"],
                 pal_chinle2   = PALETTE_ANCHORS["chinle2"],
                 pal_purple    = PALETTE_ANCHORS["purple"],
                 pal_maroon    = PALETTE_ANCHORS["maroon"],
                 pal_greygreen = PALETTE_ANCHORS["greygreen"],
                 pal_ash       = PALETTE_ANCHORS["ash"],
                 pal_wash      = PALETTE_ANCHORS["wash"],
                 pal_sage      = PALETTE_ANCHORS["sage"],
                 pal_tuff      = PALETTE_ANCHORS["tuff"],
                 talus_dark    = 0.78,   # talus = parent layer ~20% darker (§3)
                 varnish_k     = 0.65,   # desert-varnish streak strength
                 stipple_k     = 0.20,   # sage stipple opacity on flats
                 rough_rock    = 0.96):
        p  = ctx.P_object
        y  = ctx.P.y
        pal = {
            "cap":       ctx.param("pal_cap",       pal_cap),
            "wingate":   ctx.param("pal_wingate",   pal_wingate),
            "wingate2":  ctx.param("pal_wingate2",  pal_wingate2),
            "silt":      ctx.param("pal_silt",      pal_silt),
            "chinle2":   ctx.param("pal_chinle2",   pal_chinle2),
            "purple":    ctx.param("pal_purple",    pal_purple),
            "maroon":    ctx.param("pal_maroon",    pal_maroon),
            "greygreen": ctx.param("pal_greygreen", pal_greygreen),
            "ash":       ctx.param("pal_ash",       pal_ash),
            "wash":      ctx.param("pal_wash",      pal_wash),
            "sage":      ctx.param("pal_sage",      pal_sage),
            "tuff":      ctx.param("pal_tuff",      pal_tuff),
        }
        tdk = ctx.param("talus_dark", talus_dark)
        vk  = ctx.param("varnish_k",  varnish_k)
        stk = ctx.param("stipple_k",  stipple_k)
        rr  = ctx.param("rough_rock", rough_rock)

        # THE column coordinate (single source — same fn the terrace ran on)
        yy   = strata_yy(p.x, p.z, y)
        badl = P.smoothstep(0.35, 0.75, ctx.tex("Badlands").x)
        c_mesa = col_color(yy, MESA_COLUMN, pal)
        c_bad  = col_color(P.mod(yy, _BAD_TOTAL), BAD_COLUMN, pal)
        albedo = P.mix(c_mesa, c_bad, badl)
        hard   = P.mix(col_hardness(yy, MESA_COLUMN),
                       col_hardness(P.mod(yy, _BAD_TOTAL), BAD_COLUMN), badl)
        cliffg = P.smoothstep(0.45, 0.65, hard)

        # sedimentary micro-banding on the cliff faces only (value ripple keyed
        # to the SAME yy — reads as bedding without geometric cost). fwidth
        # fade: the 2.3 m period moires at distance without it.
        ripb = P.smoothstep(0.30, 0.70,
                            P.fract(yy * (1.0 / 2.3)
                                    + P.fbm_aa(p * 0.06, octaves=2, aa=0.35) * 0.8)) - 0.5
        ripvis = P.saturate(1.0 - P.fwidth(yy) * (2.0 / 2.3))
        albedo = albedo * (1.0 + cliffg * ripvis * 0.20 * ripb)
        # broad value mottle (kills the flat-color read; capped ~1 so it can
        # only darken, never bleach toward white)
        mot = P.fbm_aa(p * 0.35, octaves=3, aa=0.35)
        albedo = albedo * P.min(0.82 + 0.30 * mot, 1.02)

        # TALUS — deposition meters from the masked thermal pass: darkened
        # parent color + coarse mottle ("broken texture", §3)
        td   = ctx.tex("TalusDelta").x
        tal  = P.smoothstep(0.10, 0.45, td)
        talm = P.pow(P.fbm_aa(p * 1.3, octaves=3, aa=0.35), 0.5)
        albedo = P.mix(albedo, albedo * tdk * (0.70 + 0.60 * talm), tal)

        # ALLUVIAL COVER: gently-sloped ground outside the badlands is buried
        # colluvium/alluvium — the bedrock column only EXPOSES on slopes (and
        # in the badlands, where rilling strips the cover). Kills the
        # band-color smear across the flat (the old elevation-band disease).
        dis   = ctx.tex("FlowDischarge").x
        # cover only truly gentle ground (< ~12 deg): the slope-back Chinle
        # bands under the cliffs must stay exposed rock, not sand
        flatg = P.smoothstep(0.955, 0.988, ctx.N.y
                             + P.fbm_aa(p * 0.02, octaves=2, aa=0.35) * 0.02)
        cover = flatg * (1.0 - badl) * (1.0 - cliffg)
        albedo = P.mix(albedo, pal["wash"] * (0.82 + 0.30 * mot), cover * 0.88)
        # WASH SAND in channels/fans: discharge-keyed, brighter fresh sand
        wash  = P.smoothstep(0.55, 0.80, dis) * flatg * (1.0 - badl)
        albedo = P.mix(albedo, pal["wash"] * (0.95 + 0.25 * mot), wash)

        # SAGE-FLAT STIPPLE: grey-green broken over tan, flats outside
        # badlands. Sparser threshold + low-freq patchiness (the raw fbm's
        # low octaves read as grey smears from altitude)
        stip = (P.smoothstep(0.62, 0.80, P.fbm_aa(p * 0.9, octaves=3, aa=0.35))
                * P.smoothstep(0.35, 0.65, P.fbm_aa(p * 0.006, octaves=2, aa=0.35)))
        albedo = P.mix(albedo, pal["sage"],
                       stip * flatg * (1.0 - badl) * (1.0 - wash) * stk)

        # DESERT VARNISH: near-black streaks on steep faces below the cap —
        # y-elongated streak noise thresholded into COHERENT stripes (a raw
        # multiply reads as pepper speckle at distance), fwidth-faded far out
        slope  = P.saturate(1.0 - ctx.N.y)
        # narrow vertical drip stripes: y-free noise over xz, hard threshold ->
        # sparse ~2-5 m stripes; fwidth fade keeps them from far-field pepper
        stre   = P.noise(P.vec3(p.x * 0.45, 11.1, p.z * 0.45))
        vvis   = P.saturate(1.0 - P.fwidth(p.x + p.z) * 0.25)
        varn   = (cliffg * P.smoothstep(0.50, 0.78, slope)
                  * P.smoothstep(0.70, 0.80, stre) * vk * vvis)
        albedo = P.mix(albedo, P.vec3(0.09, 0.08, 0.075), P.saturate(varn))

        rough = rr - 0.20 * P.saturate(varn)
        # gentle drainage AO (full-strength 1-dis painted wide black smears
        # where chutes concentrate on the wall benches)
        AO    = P.pow(1.0 - 0.45 * dis, 0.5) * (0.80 + 0.20 * mot)
        self.surface(albedo=albedo, metallic=0, roughness=rough, ao=AO)
        # stored-atlas reconstruction (the swestvale explicit-capture pattern)
        c_alb = self.capture("base",  albedo, "xyz")
        c_rgh = self.capture("base",  rough,  "w")
        c_nrm = self.capture("nrmao", ctx.N,  "xyz")
        c_ao  = self.capture("nrmao", AO,     "w")
        self.surface_stored(
            albedo    = ctx.tex(c_alb),
            metallic  = 0,
            roughness = ctx.tex(c_rgh),
            normal    = ctx.tex(c_nrm),
            ao        = ctx.tex(c_ao))


###############################################################################
# THE HEIGHTFIELD
###############################################################################
class SwestRock(HeightField):
    EXTENT_M        = float(os.environ.get("SWROCK_EXTENT_M", "16384.0"))
    MATERIAL_CLASS  = Material
    MATERIAL_PARAMS = {}
    # suggested display quality (ork.terrain.viewer2 reads these; -d overrides;
    # scene files still pass render_dimension/bake_dimension/bake_res themselves)
    RENDER_DIM      = 4096     # render mesh grid (4 m cells at 8192 m)
    BAKE_DIM        = 8192     # compute grid — the mesh downsamples from it
    BAKE_RES        = 16384     # stored-atlas texels (scene stored-mode)

    # region plan (fractions of extent; D-warped at use).
    # MESA SEEDS (owner feedback aug07: irregular, many separate — and SWEST2
    # stage A: the region plan is HAND-SHAPED, never noise-thresholded; a
    # free-fbm archipelago ignored every compositional bias, measured twice).
    # Author-definable (cx, cz, ax, az) ellipse seeds, warped at use; every
    # top shares the one caprock level surface (dip+warp) — Monument Valley.
    # Center stays open for the future village flat; badlands stay mesa-free.
    # tripled density (owner aug07): mesa gaps become canyon corridors; the
    # pediment lift below keeps those gaps MILD (elevated saddles, not
    # full-depth valley). Center + badlands stay open.
    MESA_SEEDS = [
        (0.40, 0.13, 0.34, 0.15),   # the big north mass, irregular wall
        (0.83, 0.22, 0.11, 0.09),   # northeast mesa
        (0.13, 0.40, 0.10, 0.08),   # west mesa
        (0.74, 0.70, 0.08, 0.06),   # southeast butte (monument-ish)
        (0.30, 0.80, 0.13, 0.08),   # south mesa
        (0.08, 0.12, 0.11, 0.09),   # northwest corner mass
        (0.62, 0.08, 0.12, 0.08),   # north-center-east
        (0.16, 0.63, 0.09, 0.07),   # west-south
        (0.06, 0.86, 0.10, 0.07),   # southwest corner
        (0.52, 0.66, 0.08, 0.06),   # south of center
        (0.89, 0.45, 0.08, 0.07),   # east edge
        (0.92, 0.79, 0.10, 0.08),   # southeast corner
        (0.56, 0.91, 0.10, 0.07),   # south edge
        (0.33, 0.47, 0.07, 0.05),   # small butte west of center
        (0.79, 0.91, 0.07, 0.06),   # far southeast butte
    ]
    MESA_T0     = 0.22      # falloff threshold (edge foot) — ABOVE the rim-
    MESA_T1     = 0.46      # noise ceiling (~0.2), else the noise spawns
                            # phantom half-lift blotches across the open flat
    BAD_CX, BAD_CZ = 0.70, 0.48     # badlands blob center (east lowland)
    BAD_AX, BAD_AZ = 0.10, 0.07     # semi-axes (fractions) ~1.6 x 1.15 km

    FLAT_M    = 34.0        # floor base elevation (rolls swing ~+-35 m about
                            # it — lows stay above the column floor)
    PLATEAU_M = 347.0       # plateau top (caprock top 184 + soil)

    def __init__(self,
                 extent_m     = None,
                 iters_flow   = 13,     # outer cheap fluvial loops (A8) — 8
                                        # left the arroyos sub-meter (the per-
                                        # step clamp rides local relief; gentle
                                        # floors need the iterations)
                 erox_time_s  = 40.0,   # ONE quality erox pass sim time
                 erox_bed_clamp = 3.25, # bed_clamp_frac: LOWERED from the 0.5
                                        # default with measured reason — at 2 m
                                        # cells the default injected x6.01
                                        # hi-band slope energy (speckle on the
                                        # cliff faces; erox-off A/B = x1.09)
                 erox_vmax    = 10.0,   # CFL speed cap (raising it to 25 made
                                        # the face speckle WORSE, x26 hi-band —
                                        # the shallow-water model is simply out
                                        # of its regime on 60deg faces; the fix
                                        # is the erox blend mask below)
                 thermal_iters= 16,     # final talus-apron pass
                 arroyo_m     = 5.0,   # authored trunk-arroyo depth (meters)
                 pediment_m   = 520.0,   # pediment skirt lift toward every mesa
                                        # (gaps between neighbors ride up into
                                        # MILD canyon saddles)
                 river_valley_m = 226.0, # river-trunk incision depth (owner-set
                                        # aug07; gorge-class — stacks on the
                                        # arroyo carve along the main stem)
                 k_terr_mesa  = 0.85,   # strata-terrace strength, mesa column
                 k_terr_bad   = 2.50,   # strata-terrace strength, badlands
                 k_reterrace  = 0.45,   # post-erox rim re-crisp (hard-gated)
                 bad_rill_scale = 0.25,# pha gully scale (swept on the mound
                 bad_rill_strength = 2.5,   # testbed: 0.006/0.5 = invisible;
                                        # ~0.025/2.5 = the rill-fan read
                 # OUTLINE-LOBE SPECTRUM (owner aug08: rims too round — the
                 # 455/182 m rim noise is texture, not shape; a few bands).
                 # Author-definable (frequency, amplitude) rows, bipolar, each
                 # on its own seeded basis; wavelength = extent/frequency,
                 # amplitude in field units (mask threshold band is 0.24 wide).
                 # Band RMS ~2.5 = the owner-tuned single-band strength:
                 #   ~2 km grand meander / ~1 km lobes / ~512 m fingers
                 rim_lobes = ((0.1,  8.2),
                              (1.3,  7.8),
                              (2.3, 6.8),
                              (3.5, 1.6),
                              (5.5, 0.6),
                              (7.7, 0.6))
                ):
        if extent_m:
            self.EXTENT_M = float(extent_m)
        super().__init__()

        #######################################################################
        # A/D. REGION PLAN with warped edges
        #######################################################################
        ux = T.gradient(dir_x=1.0, dir_y=0.0)
        uz = T.gradient(dir_x=0.0, dir_y=1.0)
        bx = ux + T.fbm(offset=_off(1), frequency=7.0, octaves=4) * 0.025 - self.BAD_CX
        bz = uz + T.fbm(offset=_off(2), frequency=7.0, octaves=4) * 0.025 - self.BAD_CZ
        r2 = bx * bx * (1.0 / (self.BAD_AX ** 2)) + bz * bz * (1.0 / (self.BAD_AZ ** 2))
        badl = 1.0 - T.smoothstep(r2, 0.55, 1.0)
        # MESA FIELD: low-freq fbm + north bias, MINUS the center-clear bump
        # and the badlands region, PLUS the three-scale edge detail (broad
        # scallops ~450 m, alcove bites ~120 m, rim crenellation ~60 m — the
        # vertical buttress ribs a sheer face needs). Threshold -> several
        # irregular mesas with warped rims.
        # hand-shaped mesa field: warped coordinates once (broad boundary
        # meander), then per-seed quadratic falloff e = 1 - r2, max-combined;
        # alcove + crenellation noise perturb the shared field before the
        # threshold (rims scallop and grow buttress ribs, never circles).
        uxw = (ux + T.fbm(offset=_off(11), frequency=2.5, octaves=3) * 0.095
               + T.fbm(offset=_off(13), frequency=6.5, octaves=3) * 0.075
               + T.fbm(offset=_off(15), frequency=13.0, octaves=3) * 0.035)
        uzw = (uz + T.fbm(offset=_off(12), frequency=2.5, octaves=3) * 0.095
               + T.fbm(offset=_off(14), frequency=6.5, octaves=3) * 0.075
               + T.fbm(offset=_off(16), frequency=13.0, octaves=3) * 0.035)
        mraw = None
        for scx, scz, sax, saz in self.MESA_SEEDS:
            dx = uxw - scx
            dz = uzw - scz
            r2s = dx * dx * (1.0 / (sax * sax)) + dz * dz * (1.0 / (saz * saz))
            e = 1.0 - T.clamp(r2s, lo=0.0, hi=1.0)
            mraw = e if mraw is None else T.Max(mraw, e)
        # rim noise shapes the MASK only; the pediment rides the RAW falloff.
        # FAR MORE outline irregularity (owner aug07): big rim meander + fine
        # crenellation + sparse DEEP finger-canyon bites (one-sided negative
        # cuts -> re-entrant side canyons). The whole noise stack is gated by
        # proximity to a seed so the open flat never grows phantom blobs.
        bites = T.smoothstep(T.fbm(offset=_off(19), frequency=28.0, octaves=3),
                             0.18, 0.65)
        # LOBE-scale meander (bipolar: promontories AND re-entrants) — the
        # outline irregularity itself; the finer terms below are rim texture.
        # Summed spectrum from the rim_lobes table (each band its own basis).
        lobes = None
        for i, (lf, la) in enumerate(rim_lobes):
            t = (T.fbm(offset=_off(21 + i), frequency=float(lf), octaves=3)
                 - 0.5) * (2.0 * float(la))
            lobes = t if lobes is None else lobes + t
        rim_noise = (lobes
                     + T.fbm(offset=_off(8), frequency=18.0, octaves=4) * 0.40
                     + T.fbm(offset=_off(10), frequency=45.0, octaves=3) * 0.12
                     - bites * 0.60)
        mfield = mraw + rim_noise * T.smoothstep(mraw, 0.02, 0.30)
        mesa = (T.smoothstep(mfield, self.MESA_T0, self.MESA_T1)
                * (1.0 - badl))

        #######################################################################
        # B. BASE UPLIFT — flat plain, flat plateau, badlands mounds
        #######################################################################
        # THE FLOOR IS NOT FLAT (owner, aug07 — Rio Grande / Jemez /
        # Bandelier): rolling piedmont with real relief — broad 3 km swells
        # (+-22 m), hill-scale rolls (+-12 m), fine break-up (+-4 m), plus the
        # southward drainage tilt (washes run off; arroyos incise the rolls).
        # The strata terrace benches these rolls wherever they cross band
        # boundaries — the stepped Bandelier ground comes free.
        flat = (self.FLAT_M
                + (0.62 - uz) * 20.0
                + T.fbm(offset=_off(5), frequency=2.5, octaves=3) * 30.0
                + T.fbm(offset=_off(4), frequency=7.0, octaves=5) * 14.0
                + T.fbm(offset=_off(17), frequency=20.0, octaves=4) * 4.0)
        # (the analytic straight-band river corridor is RETIRED, owner aug07:
        # too straight, clipped mesas, too wide — the river now rides the
        # DISCHARGE TRUNK in the carve block below: curvy, mesa-avoiding and
        # narrow by construction, because water already routes around the
        # mesas and concentrates into a thin top-discharge band.)
        # the plateau top IS a column level-surface (real caprock follows the
        # bed): y = PLATEAU_M + dip + band-warp, built from the SAME constants
        # strata_yy subtracts — so the cap band holds the whole top and the
        # color/geometry coherence extends onto the plateau, not just the wall.
        E = self.EXTENT_M

        def _warp_expr():
            c = SurfaceCtx()
            pp = c.P_object
            w = P.noise(P.vec3(pp.x * WARP_FREQ, 37.7, pp.z * WARP_FREQ))
            return (w - 0.5) * (2.0 * WARP_AMP_M)

        dipn = (ux * (DIP_X * E) + uz * (DIP_Z * E)
                - 0.5 * (DIP_X + DIP_Z) * E)
        warpn = T.expr_field(_warp_expr())
        plateau = (self.PLATEAU_M + dipn + warpn
                   + T.fbm(offset=_off(6), frequency=11.0, octaves=5) * 3.5)
        # pediment: a SMOOTH kilometer-scale swell centered on every mesa (the
        # blurred mask — the old sharp falloff window built thin 400 m "crown
        # ring" collars at each foot). Saddles between close mesas ride high
        # -> the gaps read as canyons; far floor stays low (village + river).
        flat = (flat + T.lpf(mesa, cutoff=1500.0, units='meters')
                * float(pediment_m)
                * (0.80 + T.fbm(offset=_off(20), frequency=8.0, octaves=3) * 0.45))

        z = (T.fbm(frequency=3.0, octaves=2) * 0.75 + T.fbm(frequency=10.0) * 0.15) * 900.0
        #z += mesa*200.0

        if True:
          z += T.mix(flat, plateau, mesa)
        # badlands mass: a contiguous dissected dome + a DENSE steep mound
        # field (~58 m mounds — sparse mounds left flat pans between them that
        # the fine terrace turned into melted-looking pools).
        # clamp BEFORE pow: worley F1 can exceed 1 -> negative base -> GLSL
        # pow garbage (a -1e9 single-texel spike, found at d1024)
        mnd = T.pow(T.clamp(1.0 - T.worleyf1(offset=_off(7), frequency=140.0),
                            lo=0.0, hi=1.0), 1.4)

        if True:
          z = (z 
               + T.pow(badl, 1.5) * 12.0
               + badl * (mnd * 16.0
                         + T.fbm(offset=_off(9), frequency=200.0, octaves=3) * 2.0))

        #######################################################################
        # C/E. TERRACE BY STRATA — hfdisplacement on the shared column (the
        # strata_bake acceptance-test skeleton, hardness-gated per band)
        #######################################################################
        km, kb = float(k_terr_mesa), float(k_terr_bad)

        def _terrace_disp(ctx):
            pp = ctx.P_object
            yv = ctx.input(0)               # current height, TRUE METERS
            bm = P.saturate(ctx.input(1))   # badlands mask
            yy = strata_yy(pp.x, pp.z, yv)
            d_mesa = col_snap(yy, MESA_COLUMN) - yy
            yb = P.mod(yy, _BAD_TOTAL)
            d_bad = col_snap(yb, BAD_COLUMN, w_min=0.10, w_max=0.46) - yb
            return yv + P.mix(d_mesa * km, d_bad * kb, bm)

        if True:
          z = self.hfdisplacement(_terrace_disp, z, badl)

        # shared field builders (the SAME column, terrain-side)
        def _hard_field(zn):
            c = SurfaceCtx()
            pp = c.P_object
            yv, bm = c.input(0), P.saturate(c.input(1))
            yy = strata_yy(pp.x, pp.z, yv)
            h = P.mix(col_hardness(yy, MESA_COLUMN),
                      col_hardness(P.mod(yy, _BAD_TOTAL), BAD_COLUMN), bm)
            return T.expr_field(h, inputs=[zn, badl])

        def _talus_zone(zn, hardn):
            # cliff-base collector. The CLIFF must stay in the mask (it is the
            # SOURCE — mass-conserving thermal masked to only the soft base
            # moves nothing, measured all-zero talus_delta): full relaxation
            # on soft ground, ~30% shed on the hard faces.
            steep = T.smoothstep(T.slope(zn, radius_m=8.0), 0.50, 0.80)
            near  = T.lpf(steep, cutoff=45.0, units='meters')
            softg = 1.0 - T.smoothstep(hardn, 0.40, 0.60)
            return T.smoothstep(near, 0.18, 0.50) * (0.3 + 0.7 * softg)

        # authored (pre-erosion) height — the relief-retention datum (gate C)
        self.capture(z, "authored", cache=True)

        #######################################################################
        # F1. OUTER CHEAP LOOPS: stream-power carve + masked thermal
        #######################################################################
        # LIGHT fill only: the mesa archipelago encloses real basins — a strong
        # basin_fill raised the whole inter-mesa lowland to its spill level
        # (measured p5 35.6 -> 118.6 m). 0.1 preconditions flow without
        # drowning the flats.
        z = T.basin_fill(z, blend=0.1)

        # AUTHORED ARROYO PRE-CARVE (the flow-map carve): the per-step clamp
        # rides local relief, so the refine loop can never dig 15 m arroyos
        # into a gentle floor in tractable iterations — carve the trunk
        # network as authored depth from ONE discharge solve, loop refines.
        # Not in the badlands (rills own that ground) and not on the mesas.
        dn0 = T.normalize(T.flow3d(z).discharge)
        if True:
          z = z - ((T.smoothstep(dn0, 0.38, 0.85) * 0.75
                + T.smoothstep(dn0, 0.60, 0.95) * 0.25)
                * (arroyo_m * (1.0 - badl) * (1.0 - mesa)))

        # RIVER (owner aug07, v2): the TOP-discharge trunk — the main stem the
        # tilted basin collects into. Narrow (the top band of the log-
        # compressed discharge), meandering, and mesa-avoiding by physics.
        # Depth kept (river_valley_m + the arroyo carve it stacks on).
        z = z - (T.smoothstep(dn0, 0.90, 0.97)
                 * (float(river_valley_m) * (1.0 - badl) * (1.0 - mesa)))
        with T.loop(int(iters_flow), z=z) as L:
            flow = T.flow3d(L.z)
            zz = T.flow_erode(L.z, flow.discharge,
                              dt=1.0,
                              k_erode=0.28,
                              k_deposit=0.03,
                              m=0.5, n=1.2,
                              dep_m=0.5,
                              flat_k=6.0,
                              clamp_frac=0.5,
                              blend=0.5)
            hn = _hard_field(zz)
            zz = T.erode_thermal(zz, talus_deg=34.0, rate=0.12, iterations=6,
                                 blend=_talus_zone(zz, hn))
            L.z = T.lpf(zz, cutoff=5.0, units='meters', blend=0.20)
        z = L.z

        #######################################################################
        # F2. ONE EROX QUALITY PASS — per-cell erodibility RECOMPUTED from the
        # CURRENT height (T.erox docstring pattern; Milestone B field).
        # bed_clamp_frac / creep_max_cells stay at their tuned defaults
        # (0.5 / 4) — move only with reason.
        #######################################################################
        hard_now = _hard_field(z)
        erod = T.mix(T.const(1.8), T.const(0.12),
                     T.smoothstep(hard_now, 0.15, 0.75))
        # 6 m weathering gradient: a HARD-STEP erodibility boundary makes the
        # first soft cell past an armored cell eat the whole energy budget —
        # measured as boundary speckle bands at 2 m cells
        erod = T.lpf(erod, cutoff=6.0, units='meters')
        # blend mask: keep the erox RESULT off the hard cliff faces (armored
        # rock — the shallow-water solver speckles beyond ~50deg regardless of
        # dt/bed-clamp; washes, rills and fans all live on the soft ground)
        cliffm = T.smoothstep(T.lpf(T.smoothstep(hard_now, 0.45, 0.65),
                                    cutoff=25.0, units='meters'),
                              0.12, 0.45)
        # ...and the whole mesa-wall transition (the bench between the cliff
        # bands receives the plateau's drainage at full strength — the last
        # speckle ribbon lived there). mesa*(1-mesa) peaks on the wall zone.
        wallz = T.smoothstep(mesa * (1.0 - mesa) * 4.0, 0.15, 0.55)
        cliffm = T.Max(cliffm, wallz)
        if False:
          z = T.erox(z,
                   sim_time_s            = float(erox_time_s),
                   rain_mps              = 0.006,
                   evaporation_per_s     = 0.08,
                   flow_speed_max_mps    = float(erox_vmax),
                   capacity_Kc           = 0.5,
                   erosion_rate_per_s    = 0.7,
                   deposition_rate_per_s = 1.0,
                   creep_m2ps            = 3.0,   # 6.0 measured bit-identical:
                                                  # creep_max_cells=4 is the
                                                  # binding cap at 2 m cells
                   bed_clamp_frac        = float(erox_bed_clamp),
                   erodibility           = erod,
                   blend                 = (1.0 - cliffm * 0.9)*0.01)

        #######################################################################
        # E2. RIM RE-CRISP: light re-terrace gated to the HARD bands only (the
        # talus/soft ground the erosion built stays untouched)
        #######################################################################
        hard2 = _hard_field(z)
        cliff_gate = T.smoothstep(hard2, 0.45, 0.65) * float(k_reterrace)
        if False:
          z = self.hfdisplacement(_terrace_disp, z, badl, mask=cliff_gate)

        z += (T.fbm(frequency=12.0, octaves=2) * 0.75 + T.fbm(frequency=107.0) * 0.15) * 9.0

        #######################################################################
        # G. TALUS APRONS — masked thermal; capture the deposition delta
        #######################################################################
       
        pre = z
        if False:
            z = T.erode_thermal(z, talus_deg=34.0, rate=0.15,
                                iterations=int(thermal_iters),
                                blend=_talus_zone(z, hard2))
        self.capture(T.clamp(z - pre, lo=-8.0, hi=8.0), "talus_delta", cache=True)

        #######################################################################
        # H. BADLANDS RILLING — analytic gullies confined to the region mask
        #######################################################################
        if True:
          z = T.pha(z,
                    strength       = float(bad_rill_strength),
                    gully_weight   = 0.85,
                    detail         = 1.5,
                    scale          = float(bad_rill_scale),
                    default_height = 40.0,
                    fade_width     = 60.0,
                    blend          = badl)

        #######################################################################
        # I. CAPTURES — final height + the material/instrument channels
        #######################################################################
        flow = T.flow3d(z)
        self.capture(T.normalize(flow.discharge), "flow_discharge", cache=True)
        self.capture(flow.metrics, "flow_metrics", cache=True)
        self.capture(badl, "badlands", cache=True)
        self.capture(_hard_field(z), "hardness", cache=True)
        self.capture(z, "height", cache=True)
        self.capture(z, "normal", cache=True)
        self.relax_uv(z)
        self.z = z


__all__ = ["SwestRock"]
