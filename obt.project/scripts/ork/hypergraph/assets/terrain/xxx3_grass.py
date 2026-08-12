###############################################################################
# xxx3_grass — xxx3_trees (the erosion terrain + the 16-variant forest scatter, kept
# VERBATIM by subclassing) plus the two fields the procedural grass carpet reads and the
# sparse HERO-CLUMP placement set:
#
#   grass_density  — where blades grow at all, and how thickly (the task stage's per-tile
#                    density; also the coverage weight the clump scatter is gated on)
#   grass_dryness  — how sun-baked the grass is there (the mesh stage's per-blade dry mix
#                    and the surface's tint toward GrassDryColor)
#   grass_clumps   — a 2-type ScatterSet (lush / dry tussocks), the instanced hypermesh
#                    half of the hybrid placement
#
# Both fields are captured with cache=True so the whole bake stays cook-cacheable: the
# erosion graph this file inherits is byte-identical to xxx3_trees', so its channels hit the
# disk cache and only the new ones are computed.
#
# SLOPE SENSE — the trap this file exists downstream of: T.slope is HIGH ON STEEP GROUND
# (0 flat .. 1 cliff), and xxx3_trees' `gentle = slope` is an inverted leftover that in fact
# favors steep faces. Grass wants the opposite and says so explicitly (see `gentle` below);
# nothing here reuses that mask.
###############################################################################
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.terrain.xxx3 import XXX3Mat
from ork.hypergraph.assets.terrain.xxx3_trees import XXX3Trees
from ork.hypergraph.colors import hsv
from ork.hypergraph.ptex3d import P

# --- the grass ceiling: the SHARED ecology line (xxx3 soil_line, ~2000 m warped +-
# a few hundred meters) — grass stops at the SAME irregular boundary the trees and
# the material's rock/dirt gate use (owner, round 3). Only the transition width is
# authored here.
GRASS_LINE_SOFT_M = 100.0
# slope window: grass holds on flat-to-gentle ground and gives up on the faces. These are
# T.slope readings (tan of the terrain angle, Reinhard-rolled), NOT an inverted "gentle".
SLOPE_FADE_LO   = 0.10
SLOPE_FADE_HI   = 0.40
# the moisture proxy: concave curvature at landform scale (hollows/valley floors pool water,
# convex ridges shed it). The real TWI channel lives in flow_metrics.B, but a TerrainNode has
# no channel swizzle and re-running flow3d over the 8192^2 bake to reach one component would
# double the drainage pass for a mask — curvature is the same statement, one cheap module.
MOIST_RADIUS_M  = 256.0

# ROUND 2: 250k -> 80k. The carpet's own clump-height tussocks (GrassClump2.x) now carry
# the tussock read at eye level; the hero meshes are near-field garnish, and at 250k they
# were still only ~1 tuft/30 m — sparser costs nothing visually and sheds instances.
CLUMP_COUNT = 80000
CLUMP_SEED  = 7331


###############################################################################
# XXX3GrassMat — the terrain material FOR THIS FORK: the shared XXX3Mat Alps classifier
# (_alpine_surface — TERR2 round 1), plus a GRASS-FLOOR TINT — under a dense carpet the
# GROUND ITSELF goes green, weighted by the baked grass_density channel, so the carpet's
# mid/far ranges (where blades are subpixel) keep reading green instead of falling back
# to untinted dirt. The blend runs INSIDE the stored-atlas bake (the swestvale
# "disturbance" precedent): zero runtime cost, and mid/far get it by construction because
# the atlas IS what renders there. Dryness shifts the tint toward straw so dry slopes
# don't read golf-course. The tint yields to the classifier's snow/rock masks (no green
# cliffs, no green through the snowpack).
#
# (The old full-copy of the classifier body is gone: XXX3Mat now exposes
# _alpine_surface()/_emit() precisely so forks can inject a blend between classify and
# capture — and the texbake cache key walks the MRO sources, so an edit to the shared
# classifier in xxx3.py re-keys THIS fork's atlas too.)
###############################################################################
class XXX3GrassMat(XXX3Mat):

    SAMPLER_CHANNELS = {**XXX3Mat.SAMPLER_CHANNELS,
                        "GrassDensity": "grass_density",
                        "GrassDryness": "grass_dryness"}

    def __init__(self, ctx, *,
                 # --- the grass-floor tint (this fork's addition; all ctx.param -> A8) ---
                 grass_tint_lush   = (0.21, 0.34, 0.10),  # ground under a lush carpet
                 grass_tint_dry    = (0.42, 0.37, 0.16),  # ...and under a sun-baked one
                 grass_tint_gain   = 1.6,    # density -> tint weight (density peaks ~0.8)
                 grass_tint_max    = 0.85,   # never FULLY paint over the classifier
                 grass_tint_drymix = 0.55,   # how hard dryness pulls the tint to straw
                 grass_tint_rough  = 0.88,   # a grass floor is matte
                 # ROUND 3 (owner: grey floor beyond the carpet): under dense turf the
                 # visible ground IS thatch — soil AO (drainage wash/cavity/micro) must
                 # not darken it, and turf smooths the hummock micro-relief. NOT a gain
                 # game: AO describes occlusion the turf physically fills in.
                 meadow_ao_lift    = 0.85,   # AO -> 1 under the carpet, by tint weight
                 meadow_relief_smooth = 0.5, # micro-relief damped under the carpet
                 **kw):                      # everything else -> the shared Alps classifier
        gtl   = ctx.param("grass_tint_lush",   grass_tint_lush)
        gtd   = ctx.param("grass_tint_dry",    grass_tint_dry)
        gtg   = ctx.param("grass_tint_gain",   grass_tint_gain)
        gtm   = ctx.param("grass_tint_max",    grass_tint_max)
        gtdm  = ctx.param("grass_tint_drymix", grass_tint_drymix)
        gtr   = ctx.param("grass_tint_rough",  grass_tint_rough)
        aml   = ctx.param("meadow_ao_lift",    meadow_ao_lift)
        mrs   = ctx.param("meadow_relief_smooth", meadow_relief_smooth)

        albedo, rough, ao, masks = self._alpine_surface(ctx, **kw)

        # ── THE GRASS-FLOOR TINT (this fork's one addition to the classifier) ──
        # grass_density is the SAME field the task stage grows blades from, so the ground
        # greens exactly where the carpet is dense and thins with it under the canopy —
        # the tint can never disagree with the blades about where the grass is.
        gden  = ctx.tex("GrassDensity").x
        gdry  = ctx.tex("GrassDryness").x
        tint  = P.mix(gtl, gtd, P.saturate(gdry * gtdm))
        tintw = P.min(P.saturate(gden * gtg), gtm) \
                * (1.0 - masks["snow"]) * (1.0 - masks["rock"]) \
                * (1.0 - masks["wet"])   # drainage veins read THROUGH the floor tint
        albedo = P.mix(albedo, tint, tintw)
        rough  = P.mix(rough, gtr, tintw)

        # turf coverage lifts the soil AO and smooths the micro-relief (see the
        # kwarg note) — the SAME tint weight, so light and color stay coherent and
        # the floor the far blades dissolve into is lit like the blades themselves.
        ao   = P.mix(ao, 1.0, tintw * aml)
        grad = masks.get("grad")
        if grad is not None:
            grad = grad * (1.0 - tintw * mrs)

        # ROUND 3: forward the classifier's micro-relief gradient — the tint recolors
        # the floor but the hummock/rock/snow relief still shades it (co-variance).
        self._emit(ctx, albedo, rough, ao, grad=grad)


class XXX3Grass(XXX3Trees):
  # the fork-local material above replaces XXX3Mat for THIS terrain only.
  MATERIAL_CLASS  = XXX3GrassMat
  MATERIAL_PARAMS = {}
  def __init__(self, iters=16):
    super().__init__(iters=iters)                 # relief + self._height + the "trees" scatter
    h      = self._height
    elev   = T.normalize(h)                       # [0,1] over the whole relief
    slope  = T.slope(h, radius_m=8.0)             # 0 flat .. 1 steep (landform scale)
    gentle = 1.0 - T.smoothstep(slope, SLOPE_FADE_LO, SLOPE_FADE_HI)   # 1 on flats, 0 on faces
    below  = 1.0 - T.smoothstep(h - self._soil_line,                   # under the ecology line:
                                -GRASS_LINE_SOFT_M, GRASS_LINE_SOFT_M) # valleys up TO the line
    moist  = T.curvature(h, mode="concave", radius_m=MOIST_RADIUS_M)   # 0 ridge .. 1 hollow
    exposed = T.curvature(h, mode="convex", radius_m=MOIST_RADIUS_M)   # 0 hollow .. 1 ridge/dome

    # CANOPY OCCUPANCY: the SAME weight xxx3_trees scatters its 150k trees from (slope at
    # 1 m x the two species' elevation bands — including the inverted `gentle = slope`
    # quirk, reproduced deliberately: the grass must thin where the trees ARE, not where
    # they were meant to be). Field-level correlation, not per-tree shadowing — at 150k
    # trees the scatter weight IS the canopy distribution at landform scale.
    slope1 = T.slope(h, radius_m=1.0)
    soil_gate = 1.0 - T.smoothstep(h - self._soil_line, -100.0, 100.0)  # = the trees' gate
    canopy = T.clamp(slope1 * (T.band(elev, 0.0, 0.22, soft=0.0)
                               + T.band(elev, 0.16, 0.46, soft=0.0)) * soil_gate, 0.0, 1.0)

    # DENSITY: gentle ground inside the elevation band carries a baseline carpet; hollows
    # (where the water goes) carry a thick one; under the canopy the carpet visibly thins.
    density = T.clamp(gentle * below * (0.35 + moist * 0.95)
                      * (1.0 - canopy * 0.65), 0.0, 1.0)
    # DRYNESS: everything the moisture is not, on ground that bakes — flat and open, and more
    # so with elevation (thinner air, less shelter). Hollows stay lush even at altitude.
    dryness = T.clamp((1.0 - moist) * (0.35 + gentle * 0.35 + exposed * 0.45)
                      * (0.55 + elev * 0.70), 0.0, 1.0)

    self.capture(density, "grass_density", cache=True)
    self.capture(dryness, "grass_dryness", cache=True)

    # HERO CLUMPS — the sparse half of the hybrid placement: real instanced tussock meshes on
    # the same coverage the carpet uses, split lush/dry so the two variants land where their
    # look belongs (weighted-random per point, so the types never overlap).
    lush = T.clamp(density * (1.0 - dryness * 0.85), 0.0, 1.0)
    dry  = T.clamp(density * dryness, 0.0, 1.0)
    self.scatter("grass_clumps",
        count  = CLUMP_COUNT,
        seed   = CLUMP_SEED,
        align  = "normal",          # a tussock sits ON the slope it grew on
        scale  = (0.9, 1.9),        # the tussock hypermesh is authored ~0.7 m tall
        cutoff = 0.08,              # off-band ground places nothing
        jitter = 1.0,
        types  = {"lush": lush, "dry": dry})


__all__ = ["XXX3Grass"]
