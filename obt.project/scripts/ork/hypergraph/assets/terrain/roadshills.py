###############################################################################
# roadshills — the R-family DEMO terrain: an erox-class VALLEY landscape the road
# LAYOUT threads through. Two halves, ONE height source (the driver-seam fix):
#
#   1. RoadsHills (HeightField)  — the terrain the owner opens TODAY:
#        ork.terrain.edit.py roadshills
#      A blended fbm massif run through a few erox (physical hydraulic) passes so
#      water carves valleys, plus a slope-driven two-type scatter (trees favor the
#      valley floors, rocks the ridges). This is the ground the roads follow.
#
#   2. build_roads_layout()      — the R. cascade (the LAYOUT), wired EXACTLY as
#      test_route_tierb.py wires it, on the SAME valley field, PLUS the v2 skinner:
#        terrain fields (the SHARED height + its slope) -> route_spine (a Y forest:
#        3 POIs, a fork) -> roadbed_mask (flatten field + road UV) -> MaskBlend
#        FLATTEN (mix(height, road_elev, roadbed) — spec §0 backward coupling) ->
#        keepout_mask -> road_mesh (swept ribbon + junction patches + gid split).
#        One Merkle graph (Q5).
#
# ONE HEIGHT SOURCE (this was the seam): both halves author the SAME height DAG via
# `roadshills_height()`. The RoadsHills bake renders it; the road layout ROUTES on it
# (RouteSpine reads it as an in-graph HfImage — the DisplaceByField / test_route_tierb
# coupling substrate, NOT a re-derived lookalike). So the road cost/elevation match
# the rendered surface BY CONSTRUCTION, and MaskBlend flattens THAT height under the
# roadbed. The height bakes clean under the MESH driver (materialize_live) too — the
# erox loop is UNROLLED (raw passes) so it authors flat into both the HeightField
# document trace and the Roads graph trace (T.loop is document-only).
#
# VIEWING (honest, current-engine): `ork.terrain.edit.py roadshills` bakes the
# HeightField via the TERRAIN driver, which does NOT run the hypermesh
# onTopologyReady cascade the roads modules need — so the swept ribbon does not
# appear INSIDE the terrain editor. The composed render (terrain PRIMARY + road mesh
# CONTRIBUTOR, both on this ONE height) lives in the RoadScene lane driver
# (render_road_on_terrain.py). The cross-driver identity is EXACT: the RoadsHills
# height baked by the TERRAIN driver is BYTE-IDENTICAL to the layout's height baked by
# the MESH driver (roadshills-consistency oracle O1: worst 0.00 m over 1024 taps), so
# road_elev == the rendered terrain at every spine node (O2: 0.00 m) and the ribbon
# sits flush by construction.
#
# THE FLATTEN SEAM (§0 back-coupling, honest): roadbed_mask emits the flatten INPUTS
# (RoadElev + Roadbed) on the REAL height, and stock MaskBlend composes them
# (mix(height, RoadElev, Roadbed) == road_elev under coverage — oracle O3: 0.00e+00).
# But MATERIALIZING that MaskBlend in-graph does NOT run in either driver today:
#   * MESH driver — a terrain MaskBlend (TerrainComputeInst, needs bakeAcquire/BakeEnv)
#     placed DOWNSTREAM of the hypermesh RoadbedMask is not carried through the terrain
#     BakeEnv acquire path, so its output SSBO is never allocated -> null-deref (SIGSEGV,
#     verified). It is not a wiring typo — the mesh driver has no bake pass for a terrain
#     sink hung off a hypermesh producer.
#   * TERRAIN driver — never runs the hypermesh road modules at all (no MeshEnv / no
#     onTopologyReady cascade), so RoadElev/Roadbed do not exist there.
# So the flatten is PROVEN (O3) but not yet RENDERED: surfacing it needs a C++ seam
# closure (the mesh driver bake-acquiring a downstream terrain sink, or the terrain
# driver stocking a MeshEnv + running the road cascade). Flagged for the coordinator;
# at this demo resolution the carve is sub-texel anyway (a 6 m road spans <1 texel of
# the 8 m/texel field), so the ribbon already reads flush without it.
###############################################################################

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.units import meters


# a few gentle erox passes: thin concentrating flow carves valleys the roads then
# follow (the erox.py fluvial recipe, lighter — this is a demo, not a hero bake).
EROX = dict(
    sim_time_s            = 120.0,
    rain_mps              = 0.006,
    evaporation_per_s     = 0.08,
    flow_speed_max_mps    = 10.0,
    capacity_Kc           = 0.5,
    erosion_rate_per_s    = 0.6,
    deposition_rate_per_s = 1.0,
    creep_m2ps            = 3.0,
)

# NAVIGABLE proportions: 2 km across, ~320 m authored relief -> ~96 m after the erox
# incision carves it into valleys — wide + gentle enough for a 6 m road at max_grade 0.22
# (the router stays well below the grade cap; the road hugs the valleys, flush by design).
EXTENT_M      = 2048.0
OCTAVES       = 6
RELIEF_M      = 320.0
ERODE_PASSES  = 3

# a Y FOREST: 3 POIs => at least one fork the junction geometry demonstrates.
ROAD_POIS = [(-760.0, -680.0), (720.0, 640.0), (680.0, -720.0)]


def roadshills_height(octaves=OCTAVES, relief_m=RELIEF_M, erode_passes=ERODE_PASSES):
  """THE shared height DAG (blended fbm -> erox incision -> lpf), authored into the
  CURRENTLY-TRACED graph — a HeightField document (RoadsHills) OR the Roads/hypermesh
  graph (build_roads_layout). Returns the height node. RAW erox loop (not T.loop, which
  is document-only) so it flattens IDENTICALLY into either trace: the rendered terrain
  and the routed surface are ONE field by construction."""
  h = (T.fbm(frequency=3.2, octaves=octaves) * 0.72
       + T.fbm(frequency=9.0) * 0.28) * relief_m
  # hydraulic incision -> concentrating channels (the valleys the layout routes into).
  for _ in range(erode_passes):
    h = T.erox(h, **EROX)
  h = T.lpf(h, cutoff=8, units='meters')      # settle single-texel noise; keep landforms
  return h


class RoadsHills(HeightField):
  EXTENT_M = EXTENT_M

  def __init__(self, octaves=OCTAVES, relief=meters(RELIEF_M), erode_passes=ERODE_PASSES):
    super().__init__()
    h = roadshills_height(octaves=octaves, relief_m=float(relief), erode_passes=erode_passes)
    self.capture(h, "height", cache=True)
    self.capture(h, "normal", cache=True)

    # slope-driven two-type scatter: trees on the gentle valley floors, rocks on the
    # steep ridges (mutually exclusive per point). Once the roads coupling lands, the
    # keepout mask multiplies into these weights so the corridor visibly clears.
    steep = T.slope(h, radius_m=12.0)
    gentle = T.normalize(steep, out_lo=1.0, out_hi=0.0)   # invert: 1 in the flats
    self.scatter("cover",
        density = 0.006,
        seed    = 17,
        align   = "normal",
        scale   = (0.8, 2.2),
        cutoff  = 0.10,
        jitter  = 0.9,
        lift    = 0.1,
        types   = {"tree": gentle,          # valley floors
                   "rock": 0.25 + steep},   # ridges (+ a light floor)
        colliders = {"tree": ("capsule", 0.6, 3.0),
                     "rock": ("sphere", 0.9)})


# ---------------------------------------------------------------------------
# the R. LAYOUT cascade on the SAME valley field (the MESH-driver path). Mirrors
# test_route_tierb.py's wiring; the FLATTEN + road_mesh() lines are the v2 additions.
# ---------------------------------------------------------------------------
def build_roads_layout(extent_m=EXTENT_M, layout_cell_m=48.0, field_dim=256, seed=7,
                       octaves=OCTAVES, relief_m=RELIEF_M, erode_passes=ERODE_PASSES,
                       with_mesh=True, sinks=True):
  """Build the R-family layout graph over the RoadsHills valley (the SHARED height +
  its slope). Returns the dflow GraphData (bake it with hm.materialize_live).
  layout_cell_m is DELIBERATELY coarse vs the 6 m road so the fork's apron fits cleanly
  (the RoadMesh authoring guidance). with_mesh=False stops at the v1 fields (for a binary
  without RoadMesh). sinks=False drops the roadbed/keepout/building_seeds sinks — a road
  MESH render wants the ribbon ALONE (building_seeds emits an InstanceSet that a mesh
  drawable would otherwise instance the ribbon over; the coupling oracles keep sinks=True)."""
  from orkengine import lev2 as _lev2
  from ork.hypergraph.dflow import roads as Rd

  town = Rd.Roads(extent_m=extent_m, layout_cell_m=layout_cell_m, field_dim=field_dim, seed=seed)
  # THE ONE HEIGHT SOURCE: author the SAME RoadsHills height DAG in-graph, so the layout
  # routes on the RENDERED surface (RouteSpine reads it as an in-graph HfImage — the
  # coupling substrate; NOT a re-derived lookalike). extent/dim match the terrain bake, so
  # the height is BYTE-IDENTICAL cross-driver (oracle O1) -> the ribbon sits flush.
  h = roadshills_height(octaves=octaves, relief_m=relief_m, erode_passes=erode_passes)
  slope = T.slope(h, radius_m=12.0)
  town.route_spine(
      ROAD_POIS,
      height=h,
      slope=slope,
      width_m=6.0,
      max_grade=0.22,
      w_slope=8.0,
      grade_weight=8.0,
      export="roadshills")   # street_spine artifact (physics-proxy law) — spine_collider consumes BY NAME
  spine = town._terminal
  # roadbed_mask emits the FLATTEN INPUTS on the REAL height: Out (roadbed coverage) +
  # RoadElev. stock MaskBlend composes them (mix(height, RoadElev, Roadbed) == road_elev
  # under coverage — oracle O3). Materializing that MaskBlend in-graph does NOT run in the
  # mesh driver (a terrain sink downstream of a hypermesh producer is not bake-acquired ->
  # SIGSEGV) nor the terrain driver (no road cascade) — the flatten SEAM documented in the
  # header. So the inputs are produced here; the MaskBlend consumer awaits the C++ seam.
  if sinks:
    bed = town.roadbed_mask(
        spine,
        width_m=6.0,
        shoulder_m=3.0,
        v_meters_per_tile=8.0)
    town.keepout_mask(bed, keepout_radius_m=8.0)   # inverted -> clears the scatter corridor
    town.building_seeds(town.parcelize(spine))     # frontage placement along the lanes

  # -------- v2 (RoadMesh): grows the swept mesh + junction geometry + gid split.
  #          v2.5: `height=h` grows the terrain-grounding shoulder skirt (gid 3, gravel
  #          band) on the SAME shared height the layout routes on — the road GROUNDS with
  #          no floating silhouette (owner: "there should be an interface that intersects
  #          terrain correctly (a shoulder)"). A FEW LINES; needs the roads-v2.5 C++.
  if with_mesh:
    town.road_mesh(
        spine,
        v_meters_per_tile=8.0,
        junction_setback_scale=1.5,
        road_gid=1,
        junction_gid=2,
        height=h,
        shoulder_m=3.0,
        shoulder_gid=3)
  # --------
  town._close_trace()
  return town.graphdata


__all__ = ["RoadsHills", "build_roads_layout", "roadshills_height",
           "EXTENT_M", "ROAD_POIS", "RELIEF_M", "OCTAVES", "ERODE_PASSES"]
