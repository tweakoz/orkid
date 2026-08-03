###############################################################################
# scn_swest.py — ANCIENT SOUTHWEST PUEBLO: the P1+P2 scene. swestvale terrain
# (erodeflow-derived mesa-and-wash valley, 2 km / ~300 m relief, banded strata
# material — cracked mud REMOVED, owner directive 07-22) with three pueblo
# variants scattered into TIGHT plaza
# rings by the terrain's "buildings" sink (flat bench x wash proximity x worley
# annulus — each cluster keeps an empty central plaza):
#
#   type 0 row      — single-storey room-block row (the aggregated staple)
#   type 1 stepped  — two-storey set-back terrace block (Taos silhouette)
#   kivas (own sink "kivas", 07-22) — SUNKEN stone-masonry rings: the terrain
#   bakes a 0.6 m bowl per kiva (in-graph scatter_place cut) and the coursed
#   drystone ring seats on the pit floor; stone-tinted material, timber ladder
#
# Every building is ONE mesh with four gid buckets, bound here by NAME — adobe
# shares the GROUND's tan/ochre hue family (buildings ARE the local dirt: the
# most authentic single move of the look target):
#
#   gid 0 WALL adobe   gid 2 DOOR timber (dark; the no-glass recesses)
#   gid 3 ROOF adobe   gid 4 TRIM timber (vigas / lintels / ladders)
#
# Walkable: WASD + cursor; box proxies ride the scatter so walls block. NO
# roads in this run (P3). Declaration order = dependency order (terrain bakes
# the placement the building drawables read).
#
#   ork.scene.viewer.py scn_swest
#   ork.scene.tojson.py -i scn_swest -o /tmp/sw.ecs && ork.ecs.player.exe /tmp/sw.ecs
#
# Offscreen-diagnostic env toggles (walk mode owns the camera, so establishing
# shots need a fly variant — the scn_msaatest/scn_roadscene env precedent):
#   SWEST_FLY=1          walkable off -> the player's --camdist/--camheight orbit cam
#   SWEST_SPAWN=x,y,z    walker spawn override (drop the eye into a chosen village)
#   SWEST_TERRA_MODE=proc  per-pixel terrain material (bypass the stored atlas —
#                        the [M]-devkey live path; the atlas hides per-pixel-slope
#                        defects behind the render-dim mesh normal)
#   SWEST_MATMODE=proc|stored  BUILDING material mode (DEFAULT proc pending
#                        surface_stored ambient parity; SWEST_TERRA_MODE
#                        precedent). proc = per-pixel adobe/timber ptex3d per gid bucket
#                        (byte-identical to the pre-O3 declaration). stored = O3 per-
#                        section texture-ARRAY bake: each building mesh gets section_unwrap
#                        (one layer per gid), the drawable draws ONCE with a stored
#                        SectionArrayPBR sampler, and materials={gid:mat} becomes the
#                        per-gid BAKE MAP (AdobeStored/TimberStored, same owner tints).
#                        Wins in the color pass AND the 3 shadow cascades (no per-pixel
#                        fbm ALU per shaded fragment). instance_variation rides the live
#                        forward multiply on the sampler (per-block tint drift preserved).
#   SWEST_BAKE_RES=N     per-section bake resolution (stored mode; DEFAULT 4096 — owner
#                        bump 2026-07-24 for interactive close-up gauging (512 ~= 3 cm/texel;
#                        4096 ~= 0.4 cm/texel, well past the 0.26 m adobe grain). History:
#                        256 too soft at ~6 cm/texel; 512 was the jul22-jul24 default. A8 knob.
#
# PERF GAUGE (fixed close-up instrument, O1 2026-07-22): SWEST_SPAWN=822,150,530
# — inside the BIGGEST village (seating round 07-22 re-derived placements:
# n=136 blocks, centroid ~(795,141,536); nearest wall 5 m from the spawn),
# walls filling the frame; measure ork.ecs.player.exe <ecs> --offscreen-forever
# FPS lines. The default spawn vantage is cull-friendly — never the metric.
###############################################################################

import os

from orkengine.core import vec3, lev2_pyexdir

from ork.hypergraph.ecs.scene import Scene
from ork.hypergraph.dflow.hypermesh import GpuMeshRenderSource
from ork.hypergraph.assets.hypermesh.bld_pueblo_row import PuebloRow
from ork.hypergraph.assets.hypermesh.bld_pueblo_stepped import PuebloStepped
from ork.hypergraph.assets.hypermesh.bld_kiva import Kiva
from ork.hypergraph.assets.materials.adobe import Adobe, AdobeStored
from ork.hypergraph.assets.materials.timber import Timber, TimberStored
from ork.hypergraph.assets.materials.hypermesh.section_array import SectionArrayPBR

lev2_pyexdir.addToSysPath()

###############################################################################
# SITE + CLOCK. The SITE and the DATE are solved for the owner's original ask —
# the sun setting WNW with a fat moon already up (see the sky() call for the
# method and the residuals); that pose is still one SWEST_TOD=18.67833 away.
# The run now OPENS AT NOON (owner, 07-30): the clock is a compressed stand-in,
# not a real one, and midday is the state a look pass wants first. The clock
# itself is the library's (a full day in 180 wall seconds), so the whole arc —
# sunset, night, sunrise — still passes within the first three minutes.
###############################################################################

SITE_LATITUDE_DEG = 36.0        # pueblo country, the scene's stated latitude
SITE_DAY_OF_YEAR  = 223.0       # Aug 11 — the date the sunset pose was solved on
SITE_TIME_OF_DAY  = 7.0         # hours UT. The solved sunset pose (sun
                                # elevation +2.00, bearing 287.52) is 18.67833.

# SWEST_TOD=<hours> sets the hour the run OPENS at. It is no longer the only way
# to see another hour: the sky clock is live (sky_time_system.py, attached by
# Scene.sky) — ] [ scrub, \ pause, = - speed, ' ; step a day, 0 back to this
# authored hour, and the same controls arrive programmatically as SkyTime*
# controller messages. The env knob stays because an OFFSCREEN still needs its
# hour before the first frame, where there is nobody to press a key.
TIME_OF_DAY = float(os.environ.get("SWEST_TOD", SITE_TIME_OF_DAY))

# SWEST_TIMESCALE=<sim-sec per wall-sec>: the library clock (480 = 24h/180s) by
# default; 0 FREEZES the sky at SWEST_TOD — offscreen stills at a NAMED hour
# (load/settle frames otherwise drift the clock hours before the snapshot).
TIME_SCALE = float(os.environ.get("SWEST_TIMESCALE", "100.0"))

###############################################################################
# THE MOON IS DECLARED, NOT INHERITED FROM THE CALENDAR. The night this scene
# is lit for is a FULL moon — true opposition, so the moon rises as the sun sets
# and holds the sky till dawn. Left undeclared, that only happened because
# day 223 of the year 2000 happened to be near-full; any other year on the same
# date is a different moon, and a crescent night here is an unlit village.
# phase='full' pins the opposition itself, so the night is the same night in
# every year.
#
# The elevation is the second half of the pin: at a declared phase the moon's
# longitude is already fixed, and all that is left is the 5.15 deg of orbital
# latitude — which is exactly what sets the moon's DECLINATION, i.e. how high
# the full moon rides. -65.0 is the top of the reachable band at this hour
# (the library names the band in its error if a value falls outside), and it
# buys ~45 deg of moon at culmination against ~38 undeclared.
#
# It is stated AGAINST SITE_TIME_OF_DAY: t=0 is the run's opening instant, so
# re-cutting the clock by eye moves the reachable band with it (at 18.67 h the
# same full moon is on the horizon, band -6..+2). SWEST_TOD therefore keeps the
# phase — which is hour-proof — and drops the elevation rather than turning a
# diagnostic knob into a hard error.
###############################################################################

MOON_PHASE = "full"
# -65.0 was solved against the ORIGINAL site hour (12.0); the owner has since
# moved SITE_TIME_OF_DAY to 7.0 and the reachable band at that hour is
# unverified — declare the PHASE only (year-proof full moon) and leave the
# elevation to the ephemeris until the band is re-solved at 7.0 (W12 author).
MOON_INITIAL_ELEVATION = None

TERRA = "swest_terra"

# scatter type order in swestvale = declaration index: 0 row, 1 stepped.
# Kivas ride their OWN sink ("kivas" scatter_place export) — sunken bowls.
VARIANTS = [("row", PuebloRow), ("stepped", PuebloStepped)]


class SwestScene(Scene):

  def __init__(self):
    super().__init__()

    # BUILDING material mode (O3): proc = per-pixel ptex3d per gid bucket (byte-identical
    # to the pre-O3 scene); stored = per-section texture-ARRAY bake (one draw + a stored
    # sampler, materials={} = the per-gid bake map). Fail-loud on a bad value.
    # DEFAULT proc until the surface_stored ambient/IBL seam lands (stored
    # buildings currently receive DIRECT light only -> near-black in shadow;
    # rendering-lane fix in flight 07-23). Flip default to stored on parity.
    matmode = os.environ.get("SWEST_MATMODE", "proc")
    if matmode not in ("proc", "stored"):
      raise ValueError("SWEST_MATMODE must be 'proc' or 'stored' (got %r)" % matmode)
    stored   = (matmode == "stored")
    bake_res = int(os.environ.get("SWEST_BAKE_RES", "4096"))
    AdobeCls  = AdobeStored  if stored else Adobe
    TimberCls = TimberStored if stored else Timber

    ##########################
    # THE SKY — the family's one call: procedural dome + the celestial ensemble
    # (sun + moon + star dome over one site and one clock) + the ACES display
    # stage. The exposure block is the scene's own, restated so the numbers the
    # look was tuned against cross the library unchanged; desert4k stays as the
    # IBL source and the procedural warm-up fallback.
    #
    # THE SOLVE — the owner asked this scene for "a sky with sunset and moon
    # visible", so the SITE and DATE are solved for BOTH bodies at once. The
    # opening hour is no longer that instant (the run starts at noon), but the
    # date is what makes the instant reachable, so the solve stands:
    #   sun  elevation +2.00 deg (setting), bearing 287.52 (WNW)
    #   moon elevation 26.02 deg, 83% lit (waxing gibbous), bearing 148.5 (SSE)
    # at latitude 36.0 N (pueblo country, the scene's stated latitude), Aug 11
    # (day 223), 18.67833 h UT. Method: hold the latitude at the scene's own,
    # bisect the clock for the sunset elevation on every day of the year, then
    # take the day whose moon is high and full enough to read — the moon rides
    # the same site and clock by construction, so "both at once" is a search
    # over the date, not a second sky.
    #
    # THE MOON HALF OF THAT SOLVE IS NOW DECLARED instead of searched for (see
    # MOON_PHASE above): the date no longer has to supply a good moon, so the
    # sun's half stands on its own and the night is the same in any year.
    #
    # POSE CHANGE, DECLARED. The old static sun was elevation 45 / azimuth 285.
    # That azimuth is the DSL's (about +Y, 0 = light travelling toward +Z),
    # which is the NEGATIVE of a compass bearing — see Scene.sun()'s docstring —
    # so the light this scene actually rendered came from bearing 75 (ENE, a
    # mid-morning sun), not the WNW golden hour the comment described. The solve
    # lands on the INTENT (WNW, low) rather than on that frame, because the ask
    # was a new look; the residual 2.52 deg of bearing is the cost of keeping
    # the pueblo latitude exact.
    #
    # The sky source moves from BAKED to PROCEDURAL with this call: a baked
    # envmap has no sunset to redden and no moon to rise.
    ##########################

    self.sky(
        skybox_path        = "<ork_envmaps2>/desert4k.xir",
        skybox_intensity   = 1.2,
        diffuse_intensity  = 1.5,
        specular_intensity = 1.0,
        ambient_light      = vec3(0.0),
        msaa               = 2,
        ssaa               = 0,
        latitude_deg  = SITE_LATITUDE_DEG,
        day_of_year   = SITE_DAY_OF_YEAR,
        time_of_day   = TIME_OF_DAY,
        time_scale    = TIME_SCALE,
        phase         = MOON_PHASE,
        moon_initial_elevation = MOON_INITIAL_ELEVATION,
        # SWEST_NOSUN=1 keeps its old meaning: no sun — which now means no
        # ensemble at all (no moon, no stars, no display-exposure drive), the
        # pre-sun baseline this toggle exists for.
        celestial     = (os.environ.get("SWEST_NOSUN") != "1"),
        moon          = True,
        stars         = True,
        sun_color     = vec3(1.0, 0.82, 0.60),
        sun_intensity = 2.5,
        # W7 P1 (owner field report, jul30): undeclared cadence = hot re-render
# into the live single buffer = visible flicker. This number is now the
        # WHOLE cadence — the drift triggers that used to override it are gone.
        # 60s = owner's declared update period (jul30).
        shadow_snapshot_interval = 10.0,
        # 4K cascades: owner live-tuned 07-23, kept verbatim across the
        # conversion. SWEST_SUNSHADOW=0 = sun light WITHOUT cascades (isolates
        # shadow cost).
        sun_params    = {"shadow_map_size": 4096,
                         "shadow_caster": (os.environ.get("SWEST_SUNSHADOW") != "0"),
                         # arm the S2a double buffer: snapshots render into the
                         # inactive set (amortized one band per frame) and flip
                         # atomically behind a 12-frame factor crossfade — the
                         # live map is never sampled mid-render.
                         "shadow_snapshot_bands_per_frame": 1,
                         "shadow_crossfade_frames": 12,
                         # CLOUD SHADOWS — the deck below casts onto the vale
                         # (and dims the disc it is in front of). Extent covers
                         # the 2 km terrain with headroom; softness is the mip
                         # bias that keeps these fuzzier than the buildings'
                         # 4K-cascade edges in the same frame.
                         # SWEST_CLOUD_SHADOW=<0..1> (0 = off, the A/B leg).
                         "cloud_shadow_strength":
                             float(os.environ.get("SWEST_CLOUD_SHADOW", "3.0")),
                         "cloud_shadow_extent": 9000.0,
                         "cloud_shadow_softness": 0.0})

    # SWEST_IBLFADE=<frames>: override the procedural-IBL crossfade window
    # (SkyAtmosphereData.ibl_crossfade_frames; 0 = hard swap). Unset = engine
    # default. Offscreen STILLS must pass 0 — at unthrottled offscreen fps the
    # auto-sized fade spans hundreds of frames, so a snapshot after a publish
    # would capture a half-faded IBL blend (hub gate-author rule, 2026-07-30).
    iblfade = os.environ.get("SWEST_IBLFADE")
    if iblfade is not None:
      from orkengine.lev2 import SkyAtmosphereData
      atmo = SkyAtmosphereData()
      atmo.ibl_crossfade_frames = int(iblfade)
      self.SG._decl.sub_calls.append(("declareParams", ({"SkyAtmosphere": atmo},), {}))

    # swest never asks for a depth prepass, it INHERITS one (the ECS path adds it
    # automatically because NodeDef._skipAutoDepthPrepass defaults false). To force
    # it either way, set ORKID_DPP=0/1 — engine-wide, every scene, no scene edit
    # (ork/renderoverrides.py).

    self.system_data("HypermeshSystem")

    # TERRAIN FIRST — bakes the relief AND the "buildings" placement the variants
    # read. Spawn above the vale's max relief (300 m) so the walker drops on.
    # render/bake 1024/2048 keeps first-cook rounds tolerable (owner brief).
    spawn = vec3(0.0, 360.0, 0.0)
    sp = os.environ.get("SWEST_SPAWN")
    if sp:
      p = [float(c) for c in sp.replace(" ", "").split(",")]
      spawn = vec3(p[0], p[1], p[2])
    self.terrain(TERRA,
                 dsl_file         = "swestvale",
                 spawn            = spawn,
                 chunk            = 128,
                 render_dimension = 2048,
                 bake_dimension   = 8192,       # 
                 bake_res         = 8192,       # stored atlas budget: 4096 -> 0.5 m/texel
                                                # (softens the 1 m/texel mush that drove
                                                # the earlier proc switch)
                 mode             = os.environ.get("SWEST_TERRA_MODE", "stored"),
                                                # O1 perf directive 07-22: per-pixel proc
                                                # material was a top frame cost at village
                                                # vantages — bake once, sample the atlas
                                                # (env override = diagnostic only)
                 walkable         = (os.environ.get("SWEST_FLY") != "1"))

    # shared INSTANCED gid materials — adobe in the ground's hue family; timber
    # for everything the openings + rooflines say in wood
    def _mat(name, cls, **kw):
      return self.asset.Ptex3d(name,
                               dsl_class     = cls,
                               vertex_source = GpuMeshRenderSource(instanced=True),
                               **kw)

    # AdobeCls/TimberCls = the proc classes in proc mode, the STORED capture variants
    # (AdobeStored/TimberStored) in stored mode — SAME owner-tuned kwargs either way (the
    # stored ctors pass them through to the proc ctor for the bake; only instance_variation
    # is handled specially — see the sampler note below).
    wall = _mat("sw_adobe",
                AdobeCls,
                instance_variation = 0.14)
    # NO cracked mud on buildings (owner directive 07-22) — Adobe's crack layer
    # is bake-time-gated OFF by default; nothing here re-enables it.
    roof = _mat("sw_adobe_roof",
                AdobeCls,
                albedo_lo          = vec3(0.42, 0.315, 0.20),
                albedo_hi          = vec3(0.55, 0.43, 0.285),
                grain_m            = 0.18,
                instance_variation = 0.12)
    door = _mat("sw_door",
                TimberCls,
                early_color        = vec3(0.24, 0.17, 0.11),
                late_color         = vec3(0.13, 0.095, 0.07),
                silver             = 0.12,
                roughness          = 0.90)
    trim = _mat("sw_timber",
                TimberCls,
                instance_variation = 0.18)

    # one instanced drawable per variant PER SINK — mesh once, drawn at every
    # scatter point of its type_id; per-gid buckets route the materials in one
    # call. TWO building sinks since the v2 adoption split (07-22): "buildings"
    # = the aggregation lattice (party-wall chains on benches/flats),
    # "buildings_hill" = the contour-following hillside fringe.
    for tid, (nm, cls) in enumerate(VARIANTS):
      # stored mode appends section_unwrap as the LAST mesh op (one layer per gid); proc
      # leaves the mesh byte-identical. ONE mesh + ONE sampler per variant, shared across
      # its two sinks — same graph + bake map -> same content key -> the second sink loads
      # the first's baked cache (no double bake).
      mesh = self.asset.Hypermesh("bld_pueblo_" + nm,
                                  dsl_class = cls,
                                  **({"sectioned": True} if stored else {}))
      # the drawn stored SAMPLER: one draw over the whole gid'd mesh, sampling the baked
      # per-section arrays at ctx.layer. instance_variation = the WALL's 0.14 (the dominant
      # surface) re-applied LIVE so scattered blocks still drift block-to-block in stored mode.
      sampler = (_mat("sw_sampler_" + nm, SectionArrayPBR, instance_variation = 0.14)
                 if stored else None)
      for sink in ("buildings", "buildings_hill"):
        tag = "" if sink == "buildings" else "_hill"
        if stored:
          dd = mesh.drawable_data(
              material        = sampler,                        # the stored sampler (drawn once)
              materials       = {0: wall, 2: door, 3: roof, 4: trim},  # per-gid BAKE MAP (incl gid 0)
              section_bake    = True,
              bake_res        = bake_res,
              cull            = True,                           # per-view GPU frustum cull
              instance_source = (TERRA, sink, tid))
        else:
          dd = mesh.drawable_data(
              material        = wall,               # gid 0 (default bucket)
              materials       = {2: door, 3: roof, 4: trim},
              cull            = True,               # per-view GPU frustum cull
              instance_source = (TERRA, sink, tid))
        self.entity(
            "swest_" + nm + tag,
            components=[self.declare_component(
                "HypermeshComponent",
                drawabledata = dd,
                layername = "std_forward",
                nodename  = "hm_" + nm + tag)])

    # SUNKEN KIVAS — own sink: stone-masonry ring on the baked bowl floor.
    # gid 0 gets a dry-sandstone tint (the geometry carries the coursing; the
    # material only says "stone, not mud"): grey-tan, low patchiness, fine
    # grain, dead rough. Ladder stays timber via gid 4.
    kiva_stone = _mat("sw_kiva_stone",
                      AdobeCls,
                      albedo_lo          = vec3(0.40, 0.36, 0.295),
                      albedo_hi          = vec3(0.56, 0.51, 0.415),
                      grain_m            = 0.14,
                      roughness          = 0.97,
                      instance_variation = 0.10)
    # ring height 1.3 pairs with the terrain's KIVA_SINK_M 0.9 (pit-v2, owner
    # 07-22 "terrain lower inside the kiva"): collar = 1.3 - 0.9 = 0.4 m above
    # the court, chamber rim-to-floor 1.3 m. course_h 0.44 keeps the stone
    # count inside the hypermesh 64-register pool at the taller ring.
    kiva_mesh = self.asset.Hypermesh("bld_kiva",
                                     dsl_class = Kiva,
                                     height    = 1.3,
                                     course_h  = 0.44,
                                     radius    = 3.35,  # owner 07-22: +1m dia, +0.25, +0.25
                                     wall_t    = 1.2,   # owner 07-22: +0.5 then +0.25 thicker
                                     stone_len = 1.4,   # scale stones with the circumference —
                                                        # keeps ~56 stones inside the 64-register
                                                        # hm_mesh pool (and chunkier = the look
                                                        # the owner liked)
                                     floor     = False,
                                     ladder_drop = 3.0,  # == swestvale KIVA_SINK_M: ladder
                                                         # base on the shaft floor (sync by
                                                         # hand until the kiva goes fully
                                                         # parametric)  # SHAFT kiva: the asset owns NOTHING
                                                         # below grade — no chamber-floor slab
                                     **({"sectioned": True} if stored else {}))
    kiva_sampler = (_mat("sw_sampler_kiva", SectionArrayPBR, instance_variation = 0.10)
                    if stored else None)
    if stored:
      kiva_dd = kiva_mesh.drawable_data(
          material        = kiva_sampler,         # the stored sampler (drawn once)
          materials       = {0: kiva_stone, 4: trim},  # per-gid BAKE MAP (stone ring + ladder timber)
          section_bake    = True,
          bake_res        = bake_res,
          cull            = True,
          instance_source = (TERRA, "kivas", 0))
    else:
      kiva_dd = kiva_mesh.drawable_data(
          material        = kiva_stone,           # gid 0 (stone ring)
          materials       = {4: trim},            # ladder timber
          cull            = True,
          instance_source = (TERRA, "kivas", 0))
    self.entity(
        "swest_kiva",
        components=[self.declare_component(
            "HypermeshComponent",
            drawabledata = kiva_dd,
            layername = "std_forward",
            nodename  = "hm_kiva")])

    # walls block: ONE compound static per sink from the scatter box proxies
    self.scatter_collider(TERRA,
                          sink        = "buildings",
                          friction    = 0.9,
                          restitution = 0.05)
    self.scatter_collider(TERRA,
                          sink        = "buildings_hill",
                          friction    = 0.9,
                          restitution = 0.05)
    # kiva rims block too (their own sink's box proxies ride the sunken xforms)
    self.scatter_collider(TERRA,
                          sink        = "kivas",
                          friction    = 0.9,
                          restitution = 0.05)

    ##########################
    # CLOUD DECK — NM high-desert summer fair-weather cumulus, composed from the
    # sky library (_cloud_deck.py; scn_forest is the fuller-deck
    # reference). ONE sparse cumulus deck, base raised to ~3.5 km ASL (dry-air
    # high bases over high terrain), well-separated puffs, deep blue between.
    # WEATHER DATA ONLY — no hour is baked: the deck's radiometry follows the
    # live celestial sun (elevation dimming + intensity weighting inside
    # CloudLayerMtl), so the same deck reads correctly at any SWEST_TOD.
    # Knobs (data, env-overridable per the SWEST_* precedent):
    #   SWEST_CLOUDS=0            no deck at all (perf A/B / diagnostics)
    #   SWEST_CLOUD_COVER=<0..1>  sky-cover fraction      (default 0.20)
    #   SWEST_CLOUD_TILE=<mult>   cloud-cell world size   (default 1.0)
    #   SWEST_CLOUD_BASE=<m>      deck altitude offset    (default 2000 ->
    #                             cumulus base 1500+2000 = 3500 m ASL)
    #   SWEST_CLOUD_GAIN=<mult>   deck radiance gain (lit/shadow/haze colors;
    #                             the scn_forest exposure-coupling
    #                             precedent — the library colors were tuned
    #                             against the dimmer gauge exposure)
    ##########################
    if os.environ.get("SWEST_CLOUDS", "1") != "0":
      cg = float(os.environ.get("SWEST_CLOUD_GAIN", "2.0"))  # owner-ratified jul30 (g2.0/c0.20 from the decision set)
      self.cloud_decks(
          specs        = [("cloud_cumulus_2k", "cumulus", "cumulus2k")],
          cover        = float(os.environ.get("SWEST_CLOUD_COVER", "0.20")),
          tile         = float(os.environ.get("SWEST_CLOUD_TILE", "1.0")),
          alt_offset_m = float(os.environ.get("SWEST_CLOUD_BASE", "2000.0")),
          sag_datum_m  = 150.0,   # swestvale mean grade (heights pinned 0..300)
          colors       = {
              "lit_color":      (0.60 * cg, 0.575 * cg, 0.55 * cg),
              "shadow_color":   (0.155 * cg, 0.170 * cg, 0.20 * cg),
              "haze_color":     (0.30 * cg, 0.38 * cg, 0.47 * cg),
              "overcast_color": (0.295 * cg, 0.305 * cg, 0.325 * cg),
          })



__all__ = ["SwestScene"]
