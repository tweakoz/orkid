###############################################################################
# swestvale_fire — swestvale (see the header below) PLUS the HERO FIRE-PIT SITE
# that scn_swest_fire composes. This is a FORK, not a variant flag: the parent
# graph is shared with scn_swest and must keep its bake byte-identical.
#
# WHAT THE FORK ADDS (three moves, in this order):
#
#  1. KEEP-OUT. A disc at the pit site is subtracted from the buildings weight
#     and from the kiva weight, so nothing else can grade over the hero pad.
#  2. THE HERO PAD, AUTHORED. `T.mix(z, T.const(PIT_PAD_Y), disc)` — the pad
#     elevation is a DECLARED NUMBER, not a sampled one. That is the whole
#     reason this composes cleanly: scatter_place samples the height it is
#     handed, so a pad flattened to a constant makes the placed item's world Y
#     an authored constant too, and the SCENE can compute the fire's world
#     origin from the same constants instead of reading it back out of a bake.
#  2b. THE PYRE'S COLLIDER SINK. A second scatter_place on the SAME lattice
#     point, carrying the timber stack's capsule proxy — a sink declares exactly
#     one collider per type, and the pit's is a walk-INTO ring that deliberately
#     leaves its interior open. See the PYRE_ block below.
#  3. THE SUNKEN HEARTH. A second, smaller disc drops the pad by
#     PIT_HEARTH_CUT_M — applied AFTER the scatter_place sample so it cannot
#     move the pit down with it. bld_firepit's own analytic floor slab (radius
#     5.07 m, top at hearth_depth - floor_top below grade) lies over the cut.
#
# THE PLACEMENT IS EXACT, BY ARITHMETIC. scatter_place's candidate grid in
# count mode is ncx = ceil(sqrt(4*count)) cells across EXTENT_M, and with
# jitter=0 a candidate sits at ((i+0.5)*cell - EXTENT_M/2). PIT_COUNT=1024 ->
# ncx 64 -> cell 32 m -> candidates on x,z = 32i - 1008. PIT_X / PIT_Z are ON
# that lattice (checked below, fail-loud), and the weight disc is narrower than
# one cell, so EXACTLY ONE candidate survives and it is the authored point.
# Weight 1.5 (>1) makes the keep-probability draw unconditional.
#
# PHYSICS PROXY (law): the pit's collider is a RING annulus whose dims are
# RESTATED from bld_firepit's GENERATING params below — never measured off the
# render mesh (and `self.asset.Hypermesh` discards the DSL instance, so the
# asset's own ring_proxy is unreachable from a scene anyway). Keep them in sync
# by hand until the pit goes fully parametric — the same standing coupling
# bld_kiva's ladder_drop carries.
#
#   ork.terrain.viewer2.py -d 1024 swestvale_fire
###############################################################################
#
# swestvale — desert mesa-and-wash valley for the scn_swest pueblo scene: the
# erodeflow recipe (terrace -> flow3d/flow_erode loops -> flow/basin captures)
# COPY-ADAPTED (the sanctioned terrain-asset move — erodeflow's module constant
# HSCALE differs) and rescaled from 16 km canyon country to a 2 km walkable
# valley:
#
#   EXTENT_M 2048, relief ~300 m   (mesa-and-wash, NOT canyon abyss)
#   LESS TERRACING                 step_m = HSCALE/8 (was /32), blend 0.45 (was
#                                  0.75) — owner directive "less terracing"
#   FEWER EROSION ITERS            12 + 4 (was 32 + 5) — bake speed
#   KEEPS the flow captures        discharge = the water-proximity signal the
#                                  village mask consumes
#   KEEPS the erodeflow Material   banded strata (tan/ochre palette = the SAME
#                                  hue family as the adobe buildings); cracked-
#                                  mud playa term REMOVED entirely (owner
#                                  directive 07-22 — ground included); surface
#                                  provenance = the W·M closed form (owner
#                                  addendum 07-22 late — see the Material class
#                                  comment: W membership capture + M response
#                                  matrix as ctx.param data)
#
# Plus the P1 additions: a VILLAGE SUITABILITY mask (flat bench x discharge
# proximity x worley plaza cells) and IN-GRAPH placement via T.scatter_place
# (adoption round 07-22): one deterministic CPU module places against the
# PRE-pad height and emits per-footprint GRADING pads (grade-to-plane cut-and-
# fill under every block); a stock MaskBlend flattens the height under each
# footprint BEFORE the height/normal captures, so placement can never drift
# from its own pads. Clustering stays TIGHT — the worley band is an ANNULUS,
# so each cluster keeps an EMPTY central plaza and the room-blocks ring it.
# V2 AGGREGATION (adoption round 07-22): candidates snap to a heading-aligned
# 14.4 m lattice (the row massing length) with a 3 m lane every 4th line —
# room-blocks ABUT into party-wall chains; cluster_pads levels each chain onto
# ONE grade plane (cross-level seams rejected). Yaw is a COMPOSED radians field
# (yaw_mode="direct"): per-village frame + k*90deg door-to-plaza selection on
# flats, 22.5deg-quantized contour-following on hillsides. Scale is LOCKED 1.0
# (architecture law: doors are the human anchor).
#
#   ork.terrain.viewer2.py -d 1024 swestvale
###############################################################################
import math

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.colors import hsv
from orkengine.core import vec3, vec2
###############################################################################
HSCALE = 300.0     # TOTAL RELIEF in meters (the field is pinned to [0, HSCALE])
###############################################################################
# THE HERO FIRE-PIT SITE — the constants scn_swest_fire imports. ONE source of
# truth for the site: the terrain grades to them and the scene derives the pit
# mesh's world origin (and therefore the fire's) from the same numbers.
###############################################################################
PIT_SINK  = "firepit"     # scatter_place export -> instance_source + scatter_collider
PIT_COUNT = 1024          # candidate grid: ncx 64 -> 32 m cells (see the header)
PIT_SEED  = 101           # decorrelated from buildings(13) / hill(17) / kivas(29)

# SITE, chosen off the parent's baked height by measurement, not by eye:
# a bench at 160.2..160.7 m over the pad footprint (0.53 m of natural relief
# across 20 m — the flattest building-free point in the vale that is still
# within sight of a village), with the nearest pueblo block 53 m away at +Z so
# the fire and the village share a frame from one camera axis.
PIT_X = -720.0            # MUST be on the 32 m candidate lattice (32i - 1008)
PIT_Z =  592.0
PIT_PAD_Y = 160.50        # AUTHORED pad elevation (cut/fill stays under 0.3 m)

PIT_PAD_HALF   = 8.0      # flat pad half-extent (m). The pit's measured bbox is
                          # 12.24 x 12.38 m -> half 6.19; 8.0 leaves ~1.8 m of
                          # level ground outside the stone all the way round.
PIT_PAD_APRON  = 5.0      # grade-to-plane feather into the natural bench
PIT_KEEPOUT_M  = 26.0     # no pueblo block / kiva may grade inside this radius

# --- RESTATED FROM bld_firepit's GENERATING PARAMS (see the header note) -----
PIT_HEARTH_DEPTH = 0.65   # bld_firepit hearth_depth: exterior grade above the
                          # hearth floor. The mesh's y=0 IS the hearth floor, so
                          # the placement lift is -PIT_HEARTH_DEPTH.
PIT_RIM_H        = 1.25   # bld_firepit rim_h: stone proud of exterior grade
PIT_ANCHOR_RISE  = 0.55   # bld_firepit anchor_rise: megalith tops above the rim

# THE SUNKEN HEARTH cut. bld_firepit's analytic floor slab spans y -0.40..0.15
# about the hearth-floor plane at radius 5.07 m, i.e. PAD_Y-1.05 .. PAD_Y-0.50
# once seated; cutting to PAD_Y-1.10 puts the terrain just under that slab, so
# the stone floor is what you see in the pit and no dirt pokes through it. The
# ramp lands at 5.40 m — inside the ring's battered inner face (4.72 m) at its
# top, i.e. buried in the masonry, never visible.
PIT_HEARTH_CUT_M = 1.10
PIT_HEARTH_R0    = 4.30   # full cut depth inside this radius
PIT_HEARTH_R1    = 5.40   # back to pad level by here (under the wall)

# RING COLLIDER (physics-proxy law: generating params only). dims are
# (r_mid, half_height, thickness) — a 12-segment walk-INTO annulus, the same
# kind bld_kiva's collar uses. Sized off the ring grammar plus the NEAR-PLANE
# STANDOFF (owner 2026-07-22: >= 0.6 m, cam_near 0.5 — the eye may never get
# closer to a visible wall than the near plane):
#   outer = widest stone (anchor megaliths, radius + batter + anchor spread —
#           MEASURED 6.21 on the OBJ, matching the grammar) + 0.60 = 6.85
#   inner = battered inner face (radius - wall_t + batter = 4.72) - 0.60 = 4.10
#   -> r_mid 5.475, thickness 2.75
# Half-height: the ring is CENTERED on the item origin (the hearth-floor plane),
# so it must reach the anchor tops; the +0.10 is bld_firepit's per-block height
# jitter (measured mesh top 2.507 vs the grammar's 2.45). The spare depth below
# grade is buried and harmless.
PIT_RING_R_MID  = 5.475
PIT_RING_THICK  = 2.75
PIT_RING_HALF_H = PIT_HEARTH_DEPTH + PIT_RIM_H + PIT_ANCHOR_RISE + 0.10   # 2.55

# --- THE PYRE'S PROXY (bld_pyre) — a SECOND sink at the SAME point ----------
# The pit's ring collider is a walk-INTO collar: it blocks laterally and leaves
# the interior OPEN, which is what lets a walker come down the entry gap and
# stand on the hearth floor. That is right for the stone and wrong for the
# TIMBER standing in the middle of it — with only the ring declared, a walker
# who enters the pit walks straight through 5 m of stacked logs.
#
# bld_pyre publishes `collider_half_extents` and `pyre_proxy` from its own
# GENERATING params, but a scatter sink is where a collider is DECLARED (the
# kind+dims ride the placed item), and `self.asset.Hypermesh` discards the DSL
# instance — so, exactly like the pit's ring above, the numbers are RESTATED
# here from the asset's generating grammar and kept in sync by hand.
# scn_swest_fire re-derives every term it can from bld_pyre's own ctor
# signature and prints a loud notice if one has moved.
#
#   foot_r = max(lean_r 2.45, spar_r 2.30) + lean_r_jit 0.20 + log_d/2 0.155
#          = 2.805                          <- the mantle's butt footprint
#   top_y  = crib_top 3.688 (11 courses bedded at crib_settle 0.95 on ~0.31 m
#            timber) + spar_over 0.62 + lean_head_jit 0.26 + log_d/2 0.155
#          = 4.723                          <- the spar crown
#
# A CAPSULE, not a box: the stack is round in plan, and a box's corners would
# stand 3.97 m out — over a metre of hearth floor blocked where there is no
# timber. A Y-axis btCapsuleShape(r, h) is CENTERED on the item origin (the
# hearth-floor plane), so h = 2*top_y makes the FULL-RADIUS cylinder span
# 0 .. top_y — the whole stack — with the caps buried below grade and out of
# reach above the crown. Same convention as the buildings' boxes, whose y term
# is likewise the FULL height for a centered shape.
PYRE_SINK   = "pyre"
PYRE_FOOT_R = 2.805
PYRE_TOP_Y  = 4.723

# THE FIRE'S WORLD ORIGIN — the mesh's y=0 (hearth floor) plane in world.
# bonfire.py adds its own HEARTH_Y (0.15, the floor slab surface) on top of
# this, so a scene passes exactly this vec3 as every layer's `offset`.
PIT_MESH_ORIGIN = (PIT_X, PIT_PAD_Y - PIT_HEARTH_DEPTH, PIT_Z)


def _disc(x, z, r0, r1, gain=1.0):
  """Source string for a radial disc field about (x, z) in WORLD METERS: `gain`
  inside r0, 0 outside r1, smoothstepped between. The terrain expr substrate
  hands ctx.P_object.x/.z as origin-centred world XZ (ptex3d/compute_template),
  which is what makes an AUTHORED site expressible at all."""
  return ("%.8g * (1.0 - P.smoothstep(%.8g, %.8g, P.length(P.vec2("
          "ctx.P_object.x - (%.8g), ctx.P_object.z - (%.8g)))))"
          % (gain, r0, r1, x, z))
###############################################################################
class Material(Ptex3d):
    # the erodeflow strata + flow-tint material (owner-ratified; cracked mud
    # REMOVED 07-22), height-recalibrated to this valley's HSCALE.
    #
    # W·M SURFACE RESPONSE (owner-adjudicated 2026-07-22 late — ROADS_SPEC_DRAFT
    # final addendum, "the continuous surface-provenance law"). Every slope- or
    # surface-keyed response is the closed form
    #
    #     per-texel response(domain) = W(texel) · M[:, domain]
    #
    # W — the built-class MEMBERSHIP vector (f32 [0,1] each), the P4 dressing
    #     masks promoted unchanged and stack-converted to a PARTITION (telescoping
    #     complements in the retired over-paint order trampled->cut->fill->stone,
    #     so the albedo composite is ALGEBRAICALLY IDENTICAL to the old sequential
    #     mix stack). RGBA-packed into the "wm" capture atlas (the named W
    #     artifact the physics-friction leg will sample CPU-side):
    #         wm.x = pad    (trampled/packed village ground incl. graded pads —
    #                        B4 disturbance folds the footprints in)
    #         wm.y = riser  (drystone: terrace-riser faces + fill-lip retaining)
    #         wm.z = cut    (cut scar uphill: grading carved below natural)
    #         wm.w = BUILT-NESS Σw (fill rides the decode w_fill = a-(r+g+b);
    #                        see the capture comment — the alpha slot must stay
    #                        nonzero wherever any class is, or the PNG cache
    #                        write blanks the rgb)
    #     NATURAL GEOLOGY IS THE RESIDUAL: w_nat = 1 - wm.a — never captured;
    #     the canyon/strata response fades exactly as built-ness rises. (The
    #     old hand-wired "manmade = max(...)" tint gate was the one-cell
    #     version of this — it is now the natural-residual column BY
    #     CONSTRUCTION.)
    # M — the class x domain response matrix, REFLECTED f32 DATA (A8: ctx.param
    #     vec4 ROWS riding the ublk_ptex_params UBO — every cell live-pokeable
    #     via bindParam / MATERIAL_PARAMS; a new grading mode = a new ROW of
    #     data, zero shader edits). v1 rows: M_pad / M_riser / M_cut / M_fill.
    #     v1 domain COLUMNS (one vec4 component each):
    #         .x  strata-tint gate — canyon flow/strata tint response (natural=1
    #             by residual construction; built rows default 0)
    #         .y  palette pick     — CONTINUOUS index into the palette params
    #             pal_pad(0) / pal_cut(1) / pal_fill(2) / pal_stone(3)
    #             (piecewise-linear LUT; fractional index = honest blend)
    #         .z  roughness        — class roughness target
    #         .w  detail-set pick  — CONTINUOUS index into the detail programs
    #             plain(0) / foot-mottle(1) / strata-lines(2) / talus-mottle(3)
    #             / stone-courses(4)
    #     Roads add trampled/roadbed/ramp rows in a second W capture when they
    #     land; friction/acoustic/scatter/AO columns are later domains.
    SAMPLER_CHANNELS = {
       "FlowMetrics": "flow_metrics",
       "FlowDischarge": "flow_discharge",
       "FillDepth": "fill_depth",
       "Basin": "basin",
       "CenterPit": "center_pit",
       "Normal": "normal",
       "Disturbance": "disturbance",   # B4 trampled ground (village footprint)
       "PadMask": "padmask",           # P4 pad coverage (1 footprint -> 0 across apron)
       "CutFill": "cutfill",           # P4 signed pad delta, meters (+fill / -cut)
       "KivaMask": "kivamask",         # sunken-kiva bowl coverage (pit + apron)
       "Riser": "riser"}               # P4 terrace-riser corridor (feather-scoped)

    def __init__(self, ctx, height_scale=HSCALE,
                 # M rows (class x domain, columns = tint/palette/rough/detail —
                 # see the class comment). Ctor kwargs so MATERIAL_PARAMS / the
                 # scene can poke any cell as DATA (variant-sets-are-data).
                 M_pad   = (0.0, 0.0, 0.90, 1.0),
                 M_riser = (0.0, 3.0, 0.97, 4.0),
                 M_cut   = (0.0, 1.0, 1.00, 2.0),
                 M_fill  = (0.0, 2.0, 1.00, 3.0),
                 # palette entries (vec3 albedo bases the .y column indexes)
                 pal_pad   = (0.56, 0.46,  0.33),   # packed village tan
                 pal_cut   = (0.40, 0.27,  0.17),   # fresh-cut ochre-red scar
                 pal_fill  = (0.40, 0.33,  0.24),   # dark talus gravel
                 pal_stone = (0.38, 0.325, 0.26)):  # drystone courses
      p     = ctx.P_object
      p2    = p * P.vec3(0.3, 0.03, 0.3)
      p3    = p * P.vec3(0.03, 0.3, 0.03)
      p4    = p * P.vec3(0.5, 0.5, 0.5)
      p5    = p * 0.13
      hs    = ctx.param("height_scale", height_scale)
      h01   = P.saturate(ctx.P.y / hs)        # elevation [0,1]
      slope = P.saturate(1.0 - ctx.N.y)       # 0 flat .. 1 vertical (mesh normal)
      noise  = P.fbm_aa(p * 0.01, octaves=8, aa=0.35)
      noise2 = P.pow(P.fbm_aa(p2, octaves=2, aa=0.35), 0.35)
      noise3 = P.pow(P.fbm_aa(p3, octaves=2, aa=0.35), 0.25)
      noise4 = P.pow(P.fbm_aa(p4, octaves=2, aa=0.35), 0.15)
      noise5 = P.pow(P.fbm_aa(p5, octaves=1, aa=0.35), 0.9)
      AO    = P.pow(1.0 - ctx.tex("FlowDischarge").x, 0.5)  # darken where flow concentrates
      #########################################################################
      # W EMITTER — the P4 dressing masks, promoted unchanged (spec addendum).
      # Face gates come from the BAKED FIELDS, not ctx.N (second-bake lesson):
      # the render-dim mesh normal averages a 2.5 m apron face into near-flat,
      # so |cf| (meters on the apron ring) is the face-height gate and the
      # riser channel carries its own T.slope gate baked at bake_dimension.
      #########################################################################
      dst   = ctx.tex("Disturbance").x
      pm    = P.max(ctx.tex("PadMask").x, ctx.tex("KivaMask").x)  # kiva bowls dress too
      rz    = ctx.tex("Riser").x
      cf    = ctx.tex("CutFill").x                     # meters: + fill, - cut
      apr   = P.smoothstep(0.03, 0.18, pm) * (1.0 - P.smoothstep(0.90, 0.99, pm))
      cutm  = apr * P.smoothstep(0.20, 0.70, 0.0 - cf)
      film  = apr * P.smoothstep(0.20, 0.70, cf)
      # drystone: retaining band capping the fill lip (only where the drop is
      # tall enough to need holding, cf > ~0.5 m) + the terrace riser faces
      # (rz arrives pre-gated by baked slope — see the riser capture)
      ret   = (P.smoothstep(0.50, 0.75, pm) * (1.0 - P.smoothstep(0.93, 0.995, pm))
               * P.smoothstep(0.50, 1.10, cf))
      stone = P.saturate(P.max(ret, rz))
      # paint-stack -> partition: per-class OPACITIES (the old per-blend mix
      # amounts) telescoped in paint order, so Σ built + residual == 1 and the
      # weighted-sum composite reproduces the retired over-paint stack exactly.
      a_t = 0.85 * P.saturate(dst)      # trampled paint opacity
      a_c = 0.90 * cutm                 # cut-scar paint opacity
      a_f = 0.90 * film                 # fill-talus paint opacity
      a_s = 0.92 * stone                # drystone paint opacity (painted last)
      w_pad   = a_t * (1.0 - a_c) * (1.0 - a_f) * (1.0 - a_s)
      w_cut   = a_c * (1.0 - a_f) * (1.0 - a_s)
      w_fill  = a_f * (1.0 - a_s)
      w_riser = a_s
      w_nat   = P.saturate(1.0 - (w_pad + w_riser + w_cut + w_fill))  # RESIDUAL
      #########################################################################
      # M — the response matrix as DATA (ctor kwargs -> ctx.param vec4 rows)
      #########################################################################
      Mp  = ctx.param("M_pad",   M_pad)
      Mr  = ctx.param("M_riser", M_riser)
      Mc  = ctx.param("M_cut",   M_cut)
      Mf  = ctx.param("M_fill",  M_fill)
      pl0 = ctx.param("pal_pad",   pal_pad)
      pl1 = ctx.param("pal_cut",   pal_cut)
      pl2 = ctx.param("pal_fill",  pal_fill)
      pl3 = ctx.param("pal_stone", pal_stone)
      # detail programs (all evaluated, continuously selected — same shader
      # cost as the retired stack, which also evaluated every noise every texel)
      mot   = P.pow(P.fbm_aa(p * 0.9, octaves=2, aa=0.35), 0.5)
      det1  = 0.85 + 0.30 * mot                        # 1: foot-traffic mottle
      # cut scar: fresh strata banding (~0.55 m lines in height space) — value
      # contrast is the read (tan-on-tan vanished in the third bake)
      sline = 0.80 + 0.20 * P.smoothstep(0.25, 0.75, P.fract(ctx.P.y * (1.0 / 0.55)))
      det2  = (0.70 + 0.50 * noise2) * sline           # 2: strata lines
      tal_m = P.pow(P.fbm_aa(p * 1.3, octaves=3, aa=0.35), 0.5)
      det3  = 0.60 + 0.65 * tal_m                      # 3: talus mottle (~1 m)
      # 0.65 m courses: the 4096 atlas is 0.5 m/texel — 0.45 m courses were
      # sub-texel noise; 0.65 m resolves as banding (a bake_res 8192 bump is
      # the knob that would let true 0.3-0.45 m coursing read, at 4x atlas cost)
      crs   = P.fract(ctx.P.y * (1.0 / 0.65) + P.fbm_aa(p * 0.45, octaves=2, aa=0.35) * 0.6)
      mort  = self.aa_band(crs, 0.0, 0.16, soft=0.05)
      st_m  = P.pow(P.fbm_aa(p * 1.7, octaves=3, aa=0.35), 0.6)
      det4  = (0.65 + 0.60 * st_m) * P.mix(1.0, 0.45, mort)  # 4: stone courses

      def pal(idx):     # piecewise-linear palette LUT (fractional idx blends)
        c = P.mix(pl0, pl1, P.saturate(idx))
        c = P.mix(c, pl2, P.saturate(idx - 1.0))
        return P.mix(c, pl3, P.saturate(idx - 2.0))

      def det(idx):     # piecewise-linear detail-set LUT (0 = plain 1.0)
        d = P.mix(1.0, det1, P.saturate(idx))
        d = P.mix(d, det2, P.saturate(idx - 1.0))
        d = P.mix(d, det3, P.saturate(idx - 2.0))
        return P.mix(d, det4, P.saturate(idx - 3.0))
      #########################################################################
      # W·M PRODUCT — the closed form. Tint column first: the old hand-wired
      # manmade gate is the natural-residual column by construction (built rows
      # carry an explicit response cell instead of being max()'d away).
      #########################################################################
      tintg = P.saturate(w_nat
                         + w_pad * Mp.x + w_riser * Mr.x
                         + w_cut * Mc.x + w_fill * Mf.x)
      C     = ctx.tex("FlowMetrics").xyz
      C     = P.vec3(C.x, P.pow(C.z, 1.5), 0)  # red=high-slope, blue=high-discharge
      C     = P.mix(P.vec3(1, 1, 1), C, slope * tintg)
      C     = C * P.pow(noise, 0.25)
      # cracked-mud term REMOVED entirely (owner directive 07-22) — ground included;
      # the playa floors read as plain banded strata + noise now
      sels = self.aa_bands(h01 + noise5 * 0.05, 0.0, 0.095, 0.158, 0.245, 0.345, 0.445, 0.555, 1.0, soft=0.15)
      layers = [
        #  select      color              roughness
        ( sels[0], hsv(0, 0, 1),        1.0 ),
        ( sels[1], hsv(0, 0.55, 0.5),   noise4 ),
        ( sels[2], hsv(5, 0.50, 0.5),   noise4 ),
        ( sels[3], hsv(10, 0.45, 0.5),  1.0 ),
        ( sels[4], hsv(15.5, 0.40, 0.5), 1.0 ),
        ( sels[5], hsv(20, 0.35, 0.5),  1.0 ),
        ( sels[6], hsv(25, 0.0, 0.5),   0.75 ),
      ]
      aoo = noise2
      aoo = P.mix(aoo, noise3, P.pow(slope, 0.5) * (1.0 - sels[6]))
      aoo = P.pow(aoo, 0.75)
      albedo, rough = self.mix_bands(hsv(0, 0, 1), 1.0, layers)
      # albedo domain: natural strata (flow-tinted) by the residual + each
      # built class's palette x detail by its membership
      albedo = (albedo * C * w_nat
                + pal(Mp.y) * det(Mp.w) * w_pad
                + pal(Mr.y) * det(Mr.w) * w_riser
                + pal(Mc.y) * det(Mc.w) * w_cut
                + pal(Mf.y) * det(Mf.w) * w_fill)
      # roughness domain: natural banded rough by the residual + class targets
      rough  = (rough * w_nat
                + Mp.z * w_pad + Mr.z * w_riser + Mc.z * w_cut + Mf.z * w_fill)
      self.surface(albedo=albedo, metallic=0, roughness=rough, ao=AO * aoo)
      # EXPLICIT CAPTURES (terrain mode="stored"): bake the costly composite into
      # a packed PBR atlas, then reconstruct from it (the erodeflow pattern).
      c_alb = self.capture("base",  albedo,   "xyz")
      c_rgh = self.capture("base",  rough,    "w")
      c_nrm = self.capture("nrmao", ctx.N,    "xyz")
      c_ao  = self.capture("nrmao", AO * aoo, "w")
      # W ATLAS — the named class-weight artifact (spec: "captured as standard
      # channels, RGBA-packed four built classes per capture"). Not consumed by
      # surface_stored (the W·M product is already baked into `base`, zero
      # runtime cost); it EXISTS for the other W·M domains: the physics-friction
      # leg samples this same field CPU-side at contact points, scatter/AO
      # columns read it later.
      # PACKING (alpha-blanking-proof): the PNG cache write blanks RGB wherever
      # A==0 (fully-transparent-pixel optimization in the image writer), so the
      # alpha slot must be nonzero wherever ANY class is. Therefore:
      #     wm.rgb = (w_pad, w_riser, w_cut)
      #     wm.a   = BUILT-NESS = w_pad + w_riser + w_cut + w_fill
      # decode: w_fill = a - (r+g+b)  (clamp >= 0);  w_nat = 1 - a.
      # a == 0  <=>  all classes 0  <=>  blanked rgb is correct by construction.
      # NOTE for CPU consumers: the atlas lives in the RELAX-UV chart (like
      # base/nrmao) — sample via relaxed_uv, not planar world XZ.
      self.capture("wm", P.vec4(w_pad, w_riser, w_cut,
                                w_pad + w_riser + w_cut + w_fill))
      self.surface_stored(
          albedo    = ctx.tex(c_alb),
          metallic  = 0,
          roughness = ctx.tex(c_rgh),
          normal    = ctx.tex(c_nrm),
          ao        = ctx.tex(c_ao))
###############################################################################
class SwestValeFire(HeightField):
    EXTENT_M = 2048.0
    MATERIAL_CLASS  = Material
    MATERIAL_PARAMS = {}

    def __init__(self, origin=vec3(0), iters=12,
                 # A8: the hero site rides ctor kwargs (defaulting to the module
                 # constants the scene imports) so a slide never needs a code edit.
                 pit_x=PIT_X, pit_z=PIT_Z, pit_pad_y=PIT_PAD_Y,
                 pit_pad_half=PIT_PAD_HALF, pit_pad_apron=PIT_PAD_APRON,
                 pit_keepout_m=PIT_KEEPOUT_M,
                 pit_hearth_cut_m=PIT_HEARTH_CUT_M,
                 pit_hearth_r0=PIT_HEARTH_R0, pit_hearth_r1=PIT_HEARTH_R1,
                 pit_hearth_depth=PIT_HEARTH_DEPTH,
                 pit_ring_r_mid=PIT_RING_R_MID, pit_ring_half_h=PIT_RING_HALF_H,
                 pit_ring_thick=PIT_RING_THICK,
                 pit_count=PIT_COUNT, pit_seed=PIT_SEED, pit_sink=PIT_SINK,
                 pyre_sink=PYRE_SINK, pyre_foot_r=PYRE_FOOT_R,
                 pyre_top_y=PYRE_TOP_Y):
        super().__init__()
        # THE LATTICE CHECK — fail loud, because the failure mode is silent and
        # fatal: an off-lattice site puts the placed pit up to half a cell (16 m)
        # away from the pad and the hearth cut that were authored for it.
        _ncx  = int(math.ceil(math.sqrt(float(pit_count) * 4.0)))
        _cell = self.EXTENT_M / float(_ncx)
        for _nm, _v in (("pit_x", pit_x), ("pit_z", pit_z)):
          _i = (float(_v) + self.EXTENT_M * 0.5) / _cell - 0.5
          if abs(_i - round(_i)) > 1e-6:
            raise ValueError(
              "swestvale_fire: %s=%g is not on the scatter_place candidate lattice "
              "(count=%d -> %d cells of %.4f m; valid values are (i+0.5)*%.4f - %.1f). "
              "Nearest: %.4f" % (_nm, _v, pit_count, _ncx, _cell, _cell,
                                 self.EXTENT_M * 0.5,
                                 (round(_i) + 0.5) * _cell - self.EXTENT_M * 0.5))
        # base relief: fbm reseeded by world origin (the erodeflow idiom)
        z = (T.fbm(offset=vec2(origin.x, origin.z), frequency=3.0, octaves=5) * 0.5 + 0.5) * HSCALE
        z = T.basin_fill(z, blend=0.5)
        # LESS TERRACING (owner directive): 8 broad mesa benches, soft blend
        z = T.terrace(z, step_m=HSCALE / 8.0, blend=0.45)
        #############################
        # first erosion pass (low-freq wash carving) — the erodeflow loop, fewer iters
        #############################
        with T.loop(int(iters), z=z) as L:
          fi  = L.i / float(iters)
          fii = 1.0 - fi
          filt = 2 + ((fii * fii) * 48)
          flow = T.flow3d(L.z)
          zz   = T.flow_erode(L.z, flow.discharge,
                              dt=0.5,
                              k_erode=0.5,
                              k_deposit=0.05,
                              m=0.5, n=1.5,
                              dep_m=0.5,
                              clamp_frac=1.0,
                              blend=1.0)
          zz  = T.erode_thermal(zz, iterations=6, blend=1.0)
          L.z = T.lpf(zz, cutoff=filt, units='meters', blend=1.0)
        z = L.z
        #############################
        # filter pass: calm the high benches, keep the washes crisp
        #############################
        zn = T.normalize(z)
        hi = T.band(zn, 0.45, 1.0, soft=0.05)
        bl = T.mix(0.85, 0.45, hi)
        z = T.lpf(z, cutoff=96, units='meters', blend=bl)
        z = T.lpf(z, cutoff=48, units='meters', blend=bl)
        z = T.lpf(z, cutoff=24, units='meters', blend=bl)
        #############################
        # second erosion pass (a little hi-freq detail back in)
        #############################
        iters2 = 4
        with T.loop(int(iters2), z=z) as L:
          fi  = L.i / float(iters2)
          fii = 1.0 - fi
          filt = 2 + ((fii * fii) * 12)
          flow = T.flow3d(L.z)
          zz   = T.flow_erode(L.z, flow.discharge,
                              dt=0.15,
                              k_erode=0.5,
                              k_deposit=0.05,
                              m=0.5, n=1.5,
                              dep_m=0.5,
                              clamp_frac=1.0,
                              blend=1.0)
          L.z = T.lpf(zz, cutoff=filt, units='meters', blend=1.0)
        z = L.z
        #############################
        # METERS CALIBRATION (the erodeflow contract): pin the eroded field to
        # [0, HSCALE] — HSCALE == total relief in METERS, floor at 0.
        #############################
        z = T.normalize(z, out_lo=0.0, out_hi=HSCALE)
        #############################
        # B1 PREPARED BENCHES (seating round 07-22): the ancestral builders LEVELED
        # their ground — flatten the terrain toward the local bench elevation under
        # every village cell, so room-blocks stop bridging undulations townwide.
        # The cell product is derived on the PRE-bench relief (the worley field is
        # height-independent, so the post-bench suitability picks the SAME cells),
        # then FEATHERED (blur + re-gain -> a ~1.0 plateau with a soft ~15 m skirt)
        # so the pads read as made ground, not a stamp. Placements re-derive
        # automatically: T.scatter_place samples this SAME z in-graph.
        # The dry term keeps live wash channels OUT of the pads (never dam a wash).
        #############################
        w1     = T.worleyf1(frequency=4.0)   # ONE worley field defines the villages
        ux     = T.gradient(dir_x=1.0, dir_y=0.0)
        uz     = T.gradient(dir_x=0.0, dir_y=1.0)
        inner  = T.band(ux, 0.06, 0.94, soft=0.015) * T.band(uz, 0.06, 0.94, soft=0.015)
        cell   = T.band(w1, 0.0, 0.30, soft=0.06)      # plaza core + room-block ring
        # pad mask: the slope gate is LOOSE (exclude only cliff faces) — the pads
        # must flatten the rough ground the village NEEDS flattened; gating pads
        # on already-gentle ground would only ever level what was already level
        # (the round-1 lesson). The post-bench suitability's strict `gentle` then
        # places blocks only where a pad actually succeeded.
        # ...AND a macro-grade gate slightly STRICTER than the suitability's
        # relaxed one (adoption round 07-22): per-footprint scatter_place pads
        # decouple seating from benching, so the round-5 "gates must match"
        # rationale INVERTS into an asymmetry — blocks without B1 benches are
        # fine (each grades its own pad); B1 benches without blocks are the
        # round-5 unexplained-earthworks failure. Keep the bench gate inside
        # the placement gate ((0.24,0.42) vs (0.28,0.50)) so every benched
        # cell can actually fill, and the steepest re-admitted ground gets
        # sparse individually-padded blocks instead of bare contour rings.
        site_p = ((1.0 - T.smoothstep(T.slope(z, radius_m=8.0), 0.30, 0.55))
                  * (1.0 - T.smoothstep(T.slope(z, radius_m=20.0), 0.24, 0.42))
                  * T.band(T.normalize(z), 0.12, 0.65, soft=0.08)
                  * (1.0 - T.band(T.normalize(T.flow3d(z).discharge), 0.30, 1.0, soft=0.10))
                  * inner)
        # feather floor 0.18 (was 0.10, adoption round): marginal/skirt pad
        # products on steep knolls rang visible contour steps with no village
        # on them — raise the floor so only decisively-suitable cells bench;
        # saturated village interiors (~1.0 products) are unaffected.
        feather = T.smoothstep(T.lpf(site_p * cell, cutoff=24.0, units='meters'), 0.18, 0.55)
        # three moves (round-2/3 lessons): LPF kills undulation but NOT tilt — a
        # village on a steady 10deg grade still puts 2.5 m of fall under one
        # 15 m block. So: (1) iron the bumps toward the regional trend, (2) iron
        # the pad interior, then (3) TERRACE the pads at ONE-STOREY steps (2.6 m
        # — the ancestral cut-and-fill move). Blocks then sit on flat plateaus;
        # the risers read as made retaining edges; and because the suitability's
        # strict `gentle` re-derives from THIS z, placements auto-avoid the
        # risers (the plateau-selection causality).
        z_s = T.mix(z, T.lpf(z, cutoff=128.0, units='meters'), feather)
        z_s = T.mix(z_s, T.lpf(z_s, cutoff=32.0, units='meters'), feather)
        z   = T.terrace(z_s, step_m=2.6, blend=feather)
        #############################
        # HYDROLOGY captures — from the PRE-pad height, DELIBERATELY (adjudicated
        # v1 rule): drainage reflects the NATURAL terrain; the building pads below
        # are made ground and must not redirect the washes. This is a knowing
        # flow-vs-final-height inconsistency — flow/basin channels are computed
        # on z while render/physics/material consume the PADDED height' captured
        # after T.scatter_place — accepted for v1 (drainage-fall on pads is a
        # named future grading mode, not this round's).
        # flow.discharge doubles as the village water-proximity signal below.
        #############################
        fcb  = T.fill_closed_basins(z, min_depth=0.02 * HSCALE)
        flow = T.flow3d(z)
        self.capture(T.normalize(flow.discharge), "flow_discharge", cache=True)
        self.capture(flow.metrics, "flow_metrics", cache=True)
        self.capture(fcb.filled, "filled", cache=True)
        self.capture(fcb.basin, "basin", cache=True)
        self.capture(fcb.center_pit, "center_pit", cache=True)
        self.capture(T.normalize(fcb.filled - z), "fill_depth", cache=True)
        #############################
        # VILLAGE SUITABILITY — the "buildings" placement weights. Keep-out product:
        #   gentle  — pueblo blocks demand near-flat benches (mostly pre-leveled
        #             by the B1 pads above now)
        #   bench   — mid-elevation: above the wash floors, below the mesa rims
        #   plaza   — worley-cell ANNULUS: buildings RING a kept-empty plaza
        #             core (tight clusters — much tighter than hamletvale)
        #   near_w  — WATER-PROXIMITY BAND: an attraction RING from the blurred
        #             discharge ("near but above" — real pueblos sit beside
        #             farmable drainage, never in it); favor, never a gate
        #   solar   — SOUTH-FACING preference (Taos/Acoma archaeology): the
        #             aspect probe is the tilt-differencing trick — slope(z-ramp)
        #             exceeds slope(z+ramp) exactly where the terrain descends
        #             toward +Z (the shared building-front direction; the T.
        #             vocabulary has no aspect helper and the expr substrate has
        #             no neighbor taps, so aspect composes from slope algebra).
        #             Flats stay neutral; favor, never a gate
        #   dry     — but never IN the active wash channel itself
        # (w1 / inner / cell are declared with the B1 bench block above — ONE
        # worley field defines the villages: annulus = room-blocks, core = kiva.)
        #############################
        zn2    = T.normalize(z)
        slope  = T.slope(z, radius_m=8.0)
        gentle = 1.0 - T.smoothstep(slope, 0.12, 0.30)   # blocks have below-grade skirts;
        bench  = T.band(zn2, 0.12, 0.65, soft=0.08)      # eroded benches need tolerance
        dn     = T.normalize(flow.discharge)
        wet    = T.lpf(dn, cutoff=90.0, units='meters')
        near_w = 0.85 + 0.15 * T.band(wet, 0.02, 0.14, soft=0.04)  # the farmable ring
        dry    = 1.0 - T.band(dn, 0.30, 1.0, soft=0.10)
        rampS  = T.gradient(dir_x=0.0, dir_y=1.0, scale=0.22 * self.EXTENT_M)
        asp    = T.slope(z - rampS, radius_m=16.0) - T.slope(z + rampS, radius_m=16.0)
        solar  = 0.85 + 0.15 * T.smoothstep(asp, -0.01, 0.05)
        plaza  = T.band(w1, 0.03, 0.28, soft=0.06)
        ring_in = T.band(w1, 0.03, 0.15, soft=0.04)      # plaza-adjacent INNER band
        core   = T.band(w1, 0.0, 0.032, soft=0.008)      # kiva court: innermost ring only
                                                         # (tightened, kiva QA 07-22 — see the
                                                         # KIVA-COURT EXCLUSION on m below)
        # RISER AVOIDANCE (round-3 lesson): the terraced pads are perfectly flat,
        # but a block whose 15 m long axis CROSSES a riser bridges a full 2.6 m
        # step. The lip lives where the SMOOTH pad field sits halfway between
        # plateau levels — fract(z_smooth/step) ~ 0.5 — so gate suitability by
        # distance-from-lip in HEIGHT space (auto-scales with grade: gentler
        # hill -> wider plateaus -> proportionally wider safe zone). feather-
        # scoped: outside the pads there are no risers to avoid.
        frac   = T.expr("P.fract(ctx.input(0) * %.8g)" % (1.0 / 2.6), inputs=[z_s])
        r_safe = 1.0 - feather * T.band(frac, 0.26, 0.74, soft=0.05)
        # MACRO-GRADE GATE, RELAXED (adoption round 07-22): the round-5 honest
        # thinning past g=0.12 existed because corner-grazes on narrow plateaus
        # were STRUCTURAL. T.scatter_place now grades a per-footprint pad under
        # every block (grade-to-plane cut-and-fill), so hillsides are RE-ADMITTED
        # — the gate only excludes true escarpment where a 15 m pad would carve
        # multi-meter scars (slope@20m past ~0.28). Deliberately WIDER than the
        # B1 site_p gate above (see the asymmetry note there): benched cells
        # must always fill; the extra 0.42-0.50 fringe places sparse blocks on
        # their own scatter_place pads without village-scale benching.
        s20    = T.slope(z, radius_m=20.0)               # shared: grade gate + heading hill-select
        grade  = 1.0 - T.smoothstep(s20, 0.28, 0.50)
        site   = gentle * bench * dry * inner * r_safe * grade
        # 1.75 = density gain: keep-probability rides the total weight, and the
        # preference products (near_w * solar) + the riser/grade gates pull the
        # ballpark under the ~850-block target — the gain restores it without
        # touching structure (interior keep saturates at 1.0)
        # ...times the KIVA-COURT EXCLUSION (kiva QA 07-22): the plaza band's soft
        # edge admitted blocks down into the court center where the kiva pits are
        # cut — a building pad overlapping a kiva pit re-grades the bowl away
        # (cross-sink union, the audited failure). Hard-keep the innermost court
        # (w1 < ~0.075 ~= kiva-admissible 0.040 + the ~16 m worst-case pit+pad+
        # apron clearance) for the kivas.
        m      = (site * plaza * near_w * solar * 1.75
                  * (1.0 - T.band(w1, 0.0, 0.075, soft=0.01)))
        #############################
        # FORK: HERO-PIT KEEP-OUT. The pit's pad is composed LAST and would win
        # any overlap on height, but a pueblo block whose own pad overlapped it
        # would still bake its cut/fill dressing and its collider through the
        # ceremonial ground. Kill the weight instead — same move the kiva-court
        # exclusion above makes, at the hero site. Feathered over ~8 m so the
        # village edge thins toward the pit rather than ending on a hard line.
        #############################
        pit_keepout = T.expr(_disc(pit_x, pit_z,
                                   float(pit_keepout_m) - 8.0, float(pit_keepout_m)))
        m      = m * (1.0 - pit_keepout)
        #############################
        # B4 TRAMPLED GROUND: bake a "disturbance" channel — the village footprint
        # (cells incl. the plaza core) dilated ~20 m — that the terrain Material
        # blends packed-earth surfacing by (the SAMPLER_CHANNELS seam; the same
        # mechanism the dirt roads will use). Zero runtime cost: the blend runs
        # inside the stored-atlas bake.
        #############################
        disturb = T.smoothstep(T.lpf(site * cell, cutoff=30.0, units='meters'), 0.06, 0.50)
        # (captured AFTER scatter_place below — P4 folds the pad footprints in.
        #  The NODE is declared here so every pre-place module keeps its creation
        #  order/name — the placement inputs stay bit-identical.)

        # TIGHT clustering law: the candidate GRID must be DENSE (~26 m pitch)
        # with the mask doing the killing — a sparse grid can never place blocks
        # 15-25 m apart no matter the mask (the 4-points-of-64 first-bake lesson).
        # Abutting/overlapping placements are WANTED: contiguous cellular blocks.
        #
        # IN-GRAPH placement (adoption round 07-22): T.scatter_place replaces the
        # post-bake "buildings" sink — SAME export_name, so every consumer
        # (drawable_data instance_source=(<terra>,"buildings",type_id) and the
        # scatter_collider compound) reads the same buildings.ogeo unchanged.
        # The module places against THIS pre-pad z + the weight fields and emits
        # per-footprint GRADING pads (PadMask/PadElev); the stock MaskBlend below
        # (the roads-flatten composition) grades the terrain to each block's
        # plane BEFORE the height/normal captures — placement can never drift
        # from its own pads.
        #
        # scatter type order = declaration index: 0 row, 1 stepped, 2 kiva —
        # scn_swest binds meshes via drawable_data(instance_source=(<terra>,
        # "buildings", type_id)). Collider half-extents mirror each recipe's
        # collider_half_extents grammar (massing + flare; y = FULL height —
        # centered btBoxShape covers 0..H above grade; ladders excluded).
        # B3 07-22: the basal flare widened into a talus batter (flare_m 0.35),
        # so x/z grew +0.2 vs the pre-B3 numbers:
        #   PuebloRow     W 14.4+0.7 -> 7.7   H 2.9   D 5.2+0.7 -> 3.1
        #   PuebloStepped W 15.2+0.7 -> 8.15  H 5.4   D 5.6+0.7 -> 3.35
        #   Kiva          r 2.6+0.35 -> 2.95  H 0.9
        # ...PLUS the NEAR-PLANE STANDOFF (owner, 2026-07-22): x/z half-extents padded
        # +0.6m (> cam_near 0.5) so the eye can never get closer to a visible wall than
        # the near plane — pressing against a facade must never clip it open. Eye-to-wall
        # floor = pad + capsule radius (0.35) ~= 0.95m. Height (y) unpadded (no clip path
        # from above). If cam_near ever grows past 0.6, grow COLLIDER_STANDOFF_M with it.
        #
        # PAD footprints = the talus-flare base half-extents (the massing+flare
        # numbers above, WITHOUT the collider's +0.6 standoff — the pad grades
        # ground the walls actually stand on); apron_m 2.5 feathers the made
        # ground into the natural grade (grade-to-plane, the owner's term).
        #
        #############################
        # V2 DIRECT HEADINGS + TWO-SINK SPLIT (adoption round 07-22).
        #
        # THE DEDUP LAW (learned bake-by-bake this round): the aggregation
        # lattice dedups candidates by FRAME-LOCAL cell keys, and the heading
        # is sampled at the PRE-snap probe while weights/placement sample the
        # POST-snap point — so ANY spatial variation of the heading field
        # (quantization bins, 90deg facing selections, even smooth drift)
        # splits the key space and lets two candidates snap to the SAME world
        # lattice point: superposed crossing blocks (521 NN interpenetrations,
        # first v2 bake; DMZ mitigations recovered only partial ground because
        # boundary-adjacent probes carry FRAME MEMORY past any weight gate).
        # Aggregation is only coherent where the heading field is CONSTANT
        # over each contiguous placeable region. Hence the SPLIT:
        #
        #   sink "buildings"      — the AGGREGATION sink: benches + flats,
        #       heading = per-village CONSTANT (voronoi cell value as radians;
        #       voronoi shares w1's feature lattice so it is exactly constant
        #       per village and its only discontinuities lie in the empty mesa
        #       between cells). Lattice + lanes + cluster pads: party-wall
        #       chains, all coherent by construction.
        #   sink "buildings_hill" — the CONTOUR sink: the un-benched sloped
        #       fringe, heading = CONTINUOUS contour axis (downhill vector
        #       from per-axis tilt-differencing on THIS post-bench z, rotated
        #       90deg), NO lattice (nothing to dedup -> continuous heading is
        #       safe), cluster pads level the organic overlaps — the terraced
        #       v1 hillside look, now along-contour.
        #
        # The two masks are BUFFERED apart (~a block) so the sinks can never
        # cross-pad each other's footprints; the empty ribbon reads as the
        # bench lip break. PLAZA FACING (k*90 door-to-plaza selection) was
        # ATTEMPTED and DEFERRED: any per-point k-selection re-splits the key
        # space (the dedup law) — facing wants an engine seam (snap-position
        # dedup keys or per-component k), not more field math.
        #############################
        rampX = T.gradient(dir_x=1.0, dir_y=0.0, scale=0.22 * self.EXTENT_M)
        dxd   = T.slope(z - rampX, radius_m=20.0) - T.slope(z + rampX, radius_m=20.0)
        dzd   = T.slope(z - rampS, radius_m=20.0) - T.slope(z + rampS, radius_m=20.0)
        vor   = T.voronoi(frequency=4.0)                 # cell-id field (w1's lattice)
        s20b  = T.lpf(s20, cutoff=60.0, units='meters')  # smooth hill/flat select
        sig   = T.lpf(dxd * dxd + dzd * dzd, cutoff=24.0, units='meters')
        # per-village constant heading (radians); 61.6934 spreads cell values
        heading_flat = T.expr(
            "P.fract(ctx.input(0) * 61.6934) * 1.5707963", inputs=[vor])
        # continuous contour axis (radians): atan2(downhill) + pi/2
        heading_hill = T.expr(
            "P.atan2(ctx.input(1), ctx.input(0)) + 1.5707963", inputs=[dxd, dzd])
        # hill territory: decisively sloped AND a decisive downhill signal
        # (weak-signal ridges/benches read atan2 noise — they stay sink-A land)
        hillC = T.expr("P.step(0.07, ctx.input(0)) * P.step(0.005, ctx.input(1))",
                       inputs=[s20b, sig])
        hillD = T.lpf(hillC, cutoff=30.0, units='meters')
        aA    = 1.0 - T.smoothstep(hillD, 0.06, 0.18)    # flats sink: no hill within ~15 m
        aB    = T.smoothstep(hillD, 0.82, 0.94)          # hill sink: >=~15 m inside hill land
        mb    = m * aA                                   # aggregation-sink weights
        place = T.scatter_place(z,
                     export_name = "buildings",
                     count   = 20000,                 # CANDIDATE density, not a target
                                                      # (the kept set is mask-killed):
                                                      # ~7.2 m candidate grid => >=3
                                                      # candidates per lattice cell, so
                                                      # party-wall chains have no holes
                     seed    = 13,
                     align   = "up",                  # blocks stand PLUMB
                     scale   = (1.0, 1.0),            # NEVER scale architecture (door anchor law)
                     cutoff  = 0.30,
                     jitter  = 0.35,                  # pre-snap probe jitter (v2 snaps after)
                     lift    = 0.0,                   # bedding is the massing's below-grade skirt
                     apron_m = 2.5,
                     yaw_from_field = heading_flat,
                     yaw_mode       = "direct",       # the field sample IS the heading (radians)
                     # V2 AGGREGATION (adoption round 07-22): lattice pitch = the
                     # row MASSING length (4 room modules x 3.6 m = 14.4) so end
                     # walls TILE FLUSH along the long axis — party-wall chains
                     # with the 3.6 m room rhythm in phase across the seam; the
                     # 15.2 m stepped overlaps 0.4 m per end (walls merge). Every
                     # 4th lattice line opens a 3 m lane (lane law 3-3.5 m) —
                     # room-block runs cap at ~58 m (Taos scale). Cross-row
                     # spacing rides the same pitch: 14.4 - 6.2 depth = 9.2 m
                     # streets, +3 m at every 4th row.
                     lattice_m      = 14.4,
                     lane_every     = 4,
                     lane_m         = 3.0,
                     # CLUSTER PADS: union intersecting footprints -> ONE grade
                     # plane per abutment chain (party walls LEVEL by
                     # construction); a late candidate stepping >1 m against an
                     # admitted overlapping chain is REJECTED (the cross-level
                     # party-wall interpenetration class retires).
                     cluster_pads   = True,
                     max_seam_m     = 1.0,
                     types   = {"row":     mb,
                                # SIZE HIERARCHY: multi-storey blocks RING the
                                # plaza (worley INNER band, 4x weight); outliers
                                # stay mostly 1-storey rows
                                "stepped": mb * gentle * (0.25 + 0.75 * ring_in)},
                     footprints = {"row":     (7.7, 3.1),
                                   "stepped": (8.15, 3.35)},
                     colliders = {"row":     ("box", 8.3, 2.9, 3.7),
                                  "stepped": ("box", 8.75, 5.4, 3.95)})
        #############################
        # HILLSIDE CONTOUR SINK "buildings_hill" (the split's second half):
        # the un-benched sloped fringe, CONTINUOUS contour heading (safe — no
        # lattice, so no dedup key space to shear), v1 candidate density,
        # cluster pads level the organic overlaps into accretion masses.
        # scn_swest binds the same row/stepped meshes + a second compound
        # collider to this sink.
        #############################
        place_h = T.scatter_place(z,
                     export_name = "buildings_hill",
                     count   = 6000,                  # v1 candidate grid (~13 m)
                     seed    = 17,                    # decorrelated
                     align   = "up",
                     scale   = (1.0, 1.0),
                     cutoff  = 0.30,
                     jitter  = 0.35,
                     lift    = 0.0,
                     apron_m = 2.5,
                     yaw_from_field = heading_hill,
                     yaw_mode       = "direct",
                     cluster_pads   = True,
                     max_seam_m     = 1.0,
                     types   = {"row":     m * aB,
                                "stepped": m * aB * gentle * (0.25 + 0.75 * ring_in)},
                     footprints = {"row":     (7.7, 3.1),
                                   "stepped": (8.15, 3.35)},
                     colliders = {"row":     ("box", 8.3, 2.9, 3.7),
                                  "stepped": ("box", 8.75, 5.4, 3.95)})
        #############################
        # SUNKEN KIVAS (owner 07-22: "cut a hole in the terrain") — a SECOND
        # scatter_place, kivas only, export "kivas" (scn_swest rebinds the kiva
        # drawable to (terra,"kivas",0) + adds a "kivas" scatter_collider). The
        # bowl composes IN-GRAPH from the module's own pads: the pad plane
        # dropped KIVA_SINK_M below the sampled grade cuts the pit; lift=
        # -KIVA_SINK_M seats the stone ring on the pit floor (bld_kiva origin =
        # ring base), so the masonry collar shows (ring height - sink) above the
        # court and the courses LINE the hole — the Mesa Verde kiva court.
        # Order: the kiva cut composes FIRST; building pads win overlaps.
        # (The per-type "excavate" pad mode stays parked engine work —
        # composition reached the hole without it.)
        #############################
        # KIVA = SHAFT (owner clarification 07-22: a deep dug shaft was the
        # intent FROM THE BEGINNING — the 0.6/0.9 m "bowl" readings were a
        # misunderstanding). Design: the stone ring COLLAR stays AT GRADE
        # (lift=0) and a 30 m shaft drops INSIDE it — the shaft footprint
        # (2.3 half-extent, apron 0.4) stays inside the ring's inner wall so
        # the collar stands on undisturbed grade and the feather cannot
        # undermine it. You step over the collar and FALL IN (the ring
        # collider kind leaves the interior open; the terrain collider
        # carries the 30 m walls + floor). Ladder is rim dressing.
        KIVA_SINK_M = 3.0
        place_k = T.scatter_place(z,
                     export_name = "kivas",
                     count   = 6000,
                     seed    = 29,                    # decorrelated from the buildings grid
                     align   = "up",
                     scale   = (1.0, 1.0),
                     cutoff  = 0.30,
                     jitter  = 0.35,
                     lift    = -0.3,                  # ring sunk ~1ft into the dirt (owner 07-22);
                                                      # shaft drops inside; skirt+first course bed in
                     apron_m = 0.25,
                     yaw_from_field = heading_flat,   # ladder shares the village heading
                     yaw_mode       = "direct",
                     # cluster pads on the KIVA sink too (kiva QA 07-22):
                     # overlapping kiva pits union to ONE court plane (adjacent
                     # kivas share a dug court instead of seaming through each
                     # other's bowls); a late kiva stepping >0.4 m against an
                     # admitted court is dropped — a 1 m apron seam through a
                     # 0.6 m bowl is no bowl at all.
                     cluster_pads   = True,
                     max_seam_m     = 0.4,
                     # FORK: the same hero-pit keep-out the buildings weight takes.
                     mask    = site * core * 1.4 * (1.0 - pit_keepout),
                     footprints = {"_": (2.2, 2.2)},   # SHAFT: owner-calibrated CONSTANT (07-22 —
                                                      # the ring was ENLARGED to r3.1/wall .95
                                                      # specifically to OVERLAP this mouth and
                                                      # plug the rim leaks — do NOT re-derive the
                                                      # shaft from the collar). DEPTH-CALIBRATED (owner
                                                      # measured +1m over): at 30m deep the
                                                      # apparent mouth = data edge + feather
                                                      # (mask tail x 30m IS hole) + ~0.5m bake-
                                                      # texel quantization + ~1m render-texel
                                                      # smear (2m/texel TRIANGLE downsample
                                                      # interpolates the plunge outward). So
                                                      # DEPTH-DEPENDENT: at 30m deep the owner
                                                      # calibrated data=1.35 (feather tail x depth
                                                      # + grid smear all read as hole); at 3m the
                                                      # same terms shrink ~proportionally, so data
                                                      # rises to 2.2 for the same ~2.8 apparent.
                                                      # THE LAW: apparent = data + feather*k(depth)
                                                      # + grid smear — recalibrate on depth change
                                                      # (or auto-solve via apparent_radius_m when
                                                      # excavate mode formalizes). Deep-cut law: declared
                                                      # footprint = APPARENT - feather - ~1.5
                                                      # grid terms at these dims.
                     colliders  = {"_": ("ring", 3.2, 1.55, 2.4)})   # enterable annulus:
                                                      # outer 4.4 (r3.35 collar + flare + >=0.6
                                                      # standoff — the ONE tracked coupling: the
                                                      # near-plane invariant), inner 2.0 — 4.0m
                                                      # opening; SHAFT deliberately NOT tracked
                                                      # (owner: constant, rim-sealing)
        zk = T.mix(z, place_k.pad_elev - KIVA_SINK_M, place_k.pad_mask)
        #############################
        # height' = mix(height, PadElev, PadMask) — the roads-flatten composition:
        # grade the terrain to each block's plane. RENDER + PHYSICS + MATERIAL all
        # consume THIS padded height (the hydrology captures above deliberately do
        # not — see the adjudicated-order comment there).
        #############################
        zp = T.mix(zk, place_h.pad_elev, place_h.pad_mask)   # hill pads first...
        zp = T.mix(zp, place.pad_elev, place.pad_mask)       # ...aggregation pads win overlaps
        #######################################################################
        # FORK — THE HERO FIRE-PIT SITE. Three composed steps, and the ORDER is
        # the whole trick (see the file header).
        #
        # (1) THE PAD, AUTHORED. Not `mix(z, place_f.pad_elev, ...)` — the pad
        #     elevation is a DECLARED CONSTANT, so the ground the pit stands on
        #     is a number this file and the scene both know, instead of a number
        #     only the bake knows. Cut/fill against the natural bench measures
        #     under 0.3 m at this site, so nothing here reads as earthworks.
        #############################
        pit_pad  = T.expr(_disc(pit_x, pit_z, float(pit_pad_half),
                                float(pit_pad_half) + float(pit_pad_apron)))
        zf       = T.mix(zp, T.const(float(pit_pad_y)), pit_pad)
        #############################
        # (2) THE PLACEMENT — one candidate, at the authored point, on the FLAT
        #     pad. scatter_place samples the height it is HANDED, so sampling
        #     `zf` (pad applied, hearth NOT yet cut) makes the item's world Y
        #     exactly pit_pad_y + lift = pit_pad_y - hearth_depth: the mesh's
        #     hearth-floor plane, seated so `rim_h` of stone stands proud of the
        #     graded court. jitter=0 pins the candidate to its lattice point.
        #     apron_m=0 / a texel footprint keep the module's OWN pad inert —
        #     this sink exists for the instance + the collider, not for grading.
        #############################
        place_f = T.scatter_place(zf,
                     export_name = str(pit_sink),
                     count   = int(pit_count),
                     seed    = int(pit_seed),
                     align   = "up",                  # the pit stands PLUMB
                     scale   = (1.0, 1.0),            # NEVER scale architecture
                     yaw     = (0.0, 0.0),            # authored orientation: the entry gap
                                                      # (az 200 deg) faces away from the village
                     # ONE weight disc, narrower than one 32 m candidate cell, so
                     # exactly one candidate can survive. 1.5 (>1) makes the
                     # keep-probability draw unconditional — the hero must never
                     # be a coin flip.
                     mask    = T.expr(_disc(pit_x, pit_z, 10.0, 12.0, gain=1.5)),
                     cutoff  = 0.50,
                     jitter  = 0.0,                   # EXACT lattice point (see the header)
                     lift    = -float(pit_hearth_depth),
                     apron_m = 0.0,
                     footprints = {"_": (0.0, 0.0)},  # -> one texel; the pad above is the grading
                     colliders  = {"_": ("ring", float(pit_ring_r_mid),
                                                 float(pit_ring_half_h),
                                                 float(pit_ring_thick))})
        #############################
        # (2b) THE PYRE'S OWN SINK — the SAME candidate lattice, the SAME seed,
        #     the SAME weight disc and the SAME lift, so it resolves to the one
        #     candidate the pit resolves to and the two items are placed at the
        #     identical world transform (bld_pyre's mesh origin contract is
        #     bld_firepit's, so the RENDER side already rides the pit's own
        #     instance matrix — this sink exists ONLY to carry a second
        #     collider proxy, because a scatter sink declares exactly one
        #     collider per type and the pit's is a walk-INTO ring).
        #
        #     PHYSICS-PROXY LAW: the capsule's dims are bld_pyre's generating
        #     grammar (mantle butt radius, spar crown) restated at the top of
        #     this file — nothing here reads a vertex, and the render mesh's
        #     jitter never reaches the collider.
        #
        #     Chained through place_f.height for the same reason step 3 is: the
        #     elaborator PRUNES a module nothing consumes, and this sink's only
        #     product is a side effect (the .ogeo).
        #############################
        place_p = T.scatter_place(place_f.height,
                     export_name = str(pyre_sink),
                     count   = int(pit_count),
                     seed    = int(pit_seed),
                     align   = "up",
                     scale   = (1.0, 1.0),
                     yaw     = (0.0, 0.0),
                     mask    = T.expr(_disc(pit_x, pit_z, 10.0, 12.0, gain=1.5)),
                     cutoff  = 0.50,
                     jitter  = 0.0,
                     lift    = -float(pit_hearth_depth),
                     apron_m = 0.0,
                     footprints = {"_": (0.0, 0.0)},   # -> one texel; no grading
                     colliders  = {"_": ("capsule", float(pyre_foot_r),
                                                    2.0 * float(pyre_top_y))})
        #############################
        # (3) THE SUNKEN HEARTH — AFTER the sample, or the pit would ride the
        #     cut down with it. bld_firepit's analytic floor slab lies over this
        #     hole and the ramp dies inside the masonry.
        #
        #     NOTE the input is place_f.HEIGHT, the module's passthrough, not the
        #     `zf` it was handed. That is not cosmetic: the elaborator PRUNES a
        #     module nothing consumes, and this sink's only product is a side
        #     effect (the .ogeo). Chaining through the passthrough is what puts
        #     it in the live graph — without it the module never runs, the export
        #     never appears, and the first symptom is BulletShapeScatter
        #     asserting on a missing firepit.ogeo at load.
        #############################
        pit_cut = T.expr(_disc(pit_x, pit_z, float(pit_hearth_r0), float(pit_hearth_r1)))
        zp      = T.mix(place_p.height,
                        T.const(float(pit_pad_y) - float(pit_hearth_cut_m)), pit_cut)
        self.capture(zp, "height", cache=True)
        self.capture(zp, "normal", cache=True)
        # pad channels baked for the COUPLING instrument (seating_audit: baked
        # height under PadMask>0.9 must equal PadElev) — padmask is ALSO
        # material-consumed since P4 (SAMPLER_CHANNELS "PadMask"). COMBINED
        # across the two building sinks (mask = max; elev = hill where the hill
        # pad is strong, else aggregation — cores never overlap, the aA/aB
        # buffer keeps the sinks a block apart; only apron feathers meet).
        self.capture(T.Max(place.pad_mask, place_h.pad_mask), "padmask", cache=True)
        self.capture(T.mix(place.pad_elev, place_h.pad_elev,
                           T.smoothstep(place_h.pad_mask, 0.6, 0.9)),
                     "padelev", cache=True)
        # kiva bowl channels: instrument (zp under kivamask>0.9 == kivaelev -
        # KIVA_SINK_M, buildings-overlap excepted) + material (KivaMask joins
        # PadMask in the apron gate so the pit rim gets the cut-scar dressing).
        self.capture(place_k.pad_mask, "kivamask", cache=True)
        self.capture(place_k.pad_elev, "kivaelev", cache=True)
        #############################
        # P4 PAD-EDGE INTERFACE DRESSING captures (material-only; placement and
        # the padded height above are UNTOUCHED). Real cut-and-fill is ASYMMETRIC
        # — the symmetric smoothstep skirt reads as melted ground — so bake the
        # signed evidence the Material dresses by:
        #   cutfill — SIGNED pad delta in METERS: PadElev - pre-pad z. Negative
        #             = CUT scar uphill (grading carved below natural ground);
        #             positive = FILL slope below the lip (pad stands proud).
        #             GATED by the pad apron: PadElev is 0 outside pads, so the
        #             raw difference would read -z everywhere else.
        #   riser   — B1 terrace-riser corridor: band of the SMOOTH pad field's
        #             height in 2.6 m terrace space (the same frac seam r_safe
        #             avoids), feather-scoped. The Material confines the corridor
        #             to the steep riser FACE via the baked normal — the
        #             deliberately-BUILT terrace edges get a drystone read.
        #############################
        padg  = T.smoothstep(place.pad_mask, 0.01, 0.08)
        hpadg = T.smoothstep(place_h.pad_mask, 0.01, 0.08)
        kpadg = T.smoothstep(place_k.pad_mask, 0.01, 0.08)
        self.capture(T.clamp(place.pad_elev - z, lo=-6.0, hi=6.0) * padg
                     + T.clamp(place_h.pad_elev - z, lo=-6.0, hi=6.0) * hpadg
                     + T.clamp((place_k.pad_elev - KIVA_SINK_M) - z, lo=-6.0, hi=6.0) * kpadg,
                     "cutfill", cache=True)
        # slope-gated IN-GRAPH (bake_dimension resolves the 2.6 m step faces;
        # the render-dim mesh normal does NOT — the second-bake lesson): the
        # corridor picks the riser NEIGHBORHOOD, T.slope picks the FACE.
        self.capture(T.band(frac, 0.32, 0.68, soft=0.08) * feather
                     * T.smoothstep(T.slope(zp, radius_m=3.0), 0.15, 0.38),
                     "riser", cache=True)
        # P4: fold the pad footprints into the disturbance channel — graded pads
        # ARE made ground; the village floors + every pad read packed/lived-on.
        disturb = T.Max(disturb, T.smoothstep(place.pad_mask, 0.30, 0.90))
        disturb = T.Max(disturb, T.smoothstep(place_h.pad_mask, 0.30, 0.90))  # hill pads too
        disturb = T.Max(disturb, T.smoothstep(place_k.pad_mask, 0.30, 0.90))  # kiva courts too
        # FORK: the ceremonial ground reads PACKED — a fire pit this size is the
        # most walked-on ground in the vale. Deliberately WIDER than the graded
        # pad (a trampled apron, not a construction edge), and folded into
        # `disturbance` ONLY: the pad is NOT written into padmask/padelev/cutfill,
        # because those three are the seating_audit instrument's contract (baked
        # height under PadMask>0.9 == PadElev) and this pad's elevation is an
        # authored constant, not a scatter_place plane.
        disturb = T.Max(disturb, T.expr(_disc(pit_x, pit_z, 13.0, 21.0)))
        self.capture(disturb, "disturbance", cache=True)
        self.relax_uv(zp)
        self.z = zp


__all__ = ["SwestValeFire"]
