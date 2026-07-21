###############################################################################
# roads/base.py — the ENGINE-FACING R. DSL (needs the built C++ R-modules).
#
# Roads is a multi-currency graph (Q5 in-graph coupling): terrain field ops (T.*)
# emit INTO this graph via the shared trace context (the Hypermesh cross-family
# pattern), RouteSpine consumes their HfImage channels, the mask/parcel/seed
# modules ride the spine, and stock MaskBlend (terrain) flattens the heightfield
# under the roadbed — ONE Merkle domain. Verbs mirror route_ref/mask_ref.
#
#   from ork.hypergraph.dflow import terrain as T
#   from ork.hypergraph.dflow import roads as Rd
#   town = Rd.Roads(extent_m=1024.0, layout_cell_m=8.0)
#   h  = T.fbm(...); slope = T.slope(h); disch = T.discharge(...)   # emit into town's graph
#   spine = town.route_spine(pois=[(-300,0),(280,120)], height=h, slope=slope,
#                            discharge=disch, max_grade=0.1, width_m=6.0)
#   bed   = town.roadbed_mask(spine, width_m=6.0)
#   town.keepout_mask(bed)
#   seeds = town.building_seeds(town.parcelize(spine))
###############################################################################
from orkengine.core import dataflow as _dflow
from orkengine import lev2 as _lev2


class Roads:
  """Compose the R-family graph. `route_spine` is the hub; the rest ride the spine.
  extent_m/layout_cell_m default onto every verb (one framing per town)."""

  def __init__(self, extent_m=1024.0, layout_cell_m=8.0, field_dim=512, seed=1):
    self.extent_m = float(extent_m)
    self.layout_cell_m = float(layout_cell_m)
    self.field_dim = int(field_dim)
    self.seed = int(seed)
    self.graphdata = _dflow.GraphData.createShared()
    self._n = 0
    self._terminal = None
    # open a trace context so T.* terrain field ops author their modules INTO this
    # graph (the Hypermesh cross-family pattern); closed at materialize()/save().
    from .._trace import enter_trace
    self._prev_trace = enter_trace(self.graphdata)

  def _add(self, mod, base):
    self.graphdata.addModule(mod, "%s_%d" % (base, self._n))
    self._n += 1
    self._terminal = mod
    return mod

  def _close_trace(self):
    from .._trace import current_graph, leave_trace
    if getattr(self, "_prev_trace", None) is not None and current_graph() is self.graphdata:
      leave_trace(self._prev_trace)
      self._prev_trace = None
    self.graphdata.cacheable = True   # STATIC layout graph -> per-node cook cache (content hashes)

  # ---- the routing hub ----
  def route_spine(self, pois, height=None, slope=None, curvature=None, discharge=None,
                  width_m=6.0, max_grade=0.12, w_slope=6.0, w_curv=3.0, w_water=1.0e4,
                  disch_thresh=0.55, grade_weight=4.0, base_cost=1.0,
                  min_radius_m=24.0, station_m=8.0, vcurve_len_m=120.0, clearance_m=0.3):
    """Least-cost spine forest from terrain field channels, then CURVATURE-SMOOTHED
    (centripetal Catmull-Rom resampled at station_m, per-station turn capped at
    station/min_radius — a hard curvature floor). The road_elev profile then gets C1
    PARABOLIC VERTICAL CURVES (per-station grade change capped at station/vcurve_len_m)
    solved against a NO-BURIAL clearance floor (deck rides >= terrain + clearance_m), and
    max_grade re-enforced. `pois` = list of (x,z) world tuples (>=2). Wire terrain field
    DSL nodes to height/slope/curvature/discharge (each optional; more channels => richer
    cost). Returns the module whose `.outputs.Out` is the smoothed XfNodeGraph spine."""
    m = _lev2.hypermesh.RouteSpineModule.createShared()
    flat = []
    for (x, z) in pois:
      flat += [float(x), float(z)]
    m.set_pois(flat)
    m.extent_m = self.extent_m
    m.layout_cell_m = self.layout_cell_m
    m.field_dim = self.field_dim
    m.width_m = float(width_m)
    m.max_grade = float(max_grade)
    m.w_slope = float(w_slope)
    m.w_curv = float(w_curv)
    m.w_water = float(w_water)
    m.disch_thresh = float(disch_thresh)
    m.grade_weight = float(grade_weight)
    m.base_cost = float(base_cost)
    m.min_radius_m = float(min_radius_m)
    m.station_m = float(station_m)
    m.vcurve_len_m = float(vcurve_len_m)
    m.clearance_m = float(clearance_m)
    m.seed = self.seed
    self._add(m, "route_spine")
    for (fld, name) in ((height, "Height"), (slope, "Slope"), (curvature, "Curvature"), (discharge, "Discharge")):
      if fld is not None:
        # a terrain field is a TerrainNode (.output_plug); a raw module exposes .outputs.Out
        plug = getattr(fld, "output_plug", None)
        if plug is None:
          plug = fld.outputs.Out
        self.graphdata.connect(getattr(m.inputs, name), plug)
    return m

  # ---- roadbed / keepout fields (spec §0 BACKWARD coupling) ----
  def roadbed_mask(self, spine, width_m=6.0, shoulder_m=2.0, v_meters_per_tile=8.0, out_dim=None):
    """Rasterize the spine -> Roadbed / RoadElev / UvU / UvV HfImage fields (the
    road is dim-independent; output dim = terrain bake dim so MaskBlend flattens)."""
    m = _lev2.hypermesh.RoadbedMaskModule.createShared()
    m.width_m = float(width_m)
    m.shoulder_m = float(shoulder_m)
    m.v_meters_per_tile = float(v_meters_per_tile)
    m.extent_m = self.extent_m
    m.out_dim = int(out_dim) if out_dim is not None else self.field_dim
    self._add(m, "roadbed_mask")
    self.graphdata.connect(m.inputs.In, spine.outputs.Out)
    return m

  def keepout_mask(self, roadbed, keepout_radius_m=6.0):
    """Keepout = dilate(Roadbed>0). Feed INVERTED to scatter sinks (no trees on roads)."""
    m = _lev2.hypermesh.KeepoutMaskModule.createShared()
    m.keepout_radius_m = float(keepout_radius_m)
    m.extent_m = self.extent_m
    self._add(m, "keepout_mask")
    self.graphdata.connect(m.inputs.Roadbed, roadbed.outputs.Out)  # RoadbedMask primary "Out" = coverage
    return m

  # ---- parcels + building seeds ----
  def parcelize(self, spine, frontage_m=12.0, depth_m=16.0, spacing_m=2.0, jitter=0.4):
    m = _lev2.hypermesh.ParcelizeModule.createShared()
    m.frontage_m = float(frontage_m)
    m.depth_m = float(depth_m)
    m.spacing_m = float(spacing_m)
    m.jitter = float(jitter)
    m.extent_m = self.extent_m
    m.seed = self.seed
    self._add(m, "parcelize")
    self.graphdata.connect(m.inputs.In, spine.outputs.Out)
    return m

  def building_seeds(self, parcels, type_weights=None, emit_adapter=0):
    """One seed per parcel. Default emit_adapter=0 = scatter-sink InstanceSet ("same
    as tree scatter"); the freeform-SoA schema is built internally (owner Q2 seam)."""
    m = _lev2.hypermesh.BuildingSeedsModule.createShared()
    if type_weights:
      m.set_type_weights([float(w) for w in type_weights])
    m.emit_adapter = int(emit_adapter)
    m.seed = self.seed
    self._add(m, "building_seeds")
    self.graphdata.connect(m.inputs.Parcels, parcels.outputs.Out)
    return m

  # ---- swept road MESH + junction geometry + material split (R-family v2) ----
  def road_mesh(self, spine, v_meters_per_tile=8.0, junction_setback_scale=1.5,
                junction_min_edge_frac=0.45, road_gid=1, junction_gid=2,
                height=None, shoulder_m=3.0, shoulder_gid=3, clearance_m=0.3,
                max_bank_deg=6.0, bank_runoff_m=30.0, bank_ref_radius_m=24.0):
    """Skin the spine FOREST into a swept road-ribbon GpuMesh: one quad per edge (sits
    on road_elev, U=lateral, V=arc-length/v_meters_per_tile), a junction PATCH per fork
    (>= 2 children) welded crack-free to the approach mouths, and a gid material split
    (road_gid on ribbon faces, junction_gid on patches — the parametric asphalt material
    binds by gid). When `height` (a terrain HfImage field) is wired, each deck edge + the
    apron perimeter grow an OUTER SKIRT dropping to the shared terrain (shoulder_gid, a
    gravel band) so the road GROUNDS with no floating silhouette; the apron also enforces
    the no-burial clearance_m floor. The deck SUPERELEVATES into plan-view curves (bank
    capped at max_bank_deg, reaching full bank at radius bank_ref_radius_m, ramped over
    bank_runoff_m — a subtle cross-slope, flat on straights + at aprons). Returns the module
    whose `.outputs.Out` is the road GpuMesh."""
    import math as _math
    m = _lev2.hypermesh.RoadMeshModule.createShared()
    m.v_meters_per_tile = float(v_meters_per_tile)
    m.junction_setback_scale = float(junction_setback_scale)
    m.junction_min_edge_frac = float(junction_min_edge_frac)
    m.road_gid = int(road_gid)
    m.junction_gid = int(junction_gid)
    m.extent_m = self.extent_m
    m.shoulder_m = float(shoulder_m)
    m.shoulder_gid = int(shoulder_gid)
    m.clearance_m = float(clearance_m)
    m.max_bank_rad = _math.radians(float(max_bank_deg))
    m.bank_runoff_m = float(bank_runoff_m)
    m.bank_ref_radius_m = float(bank_ref_radius_m)
    self._add(m, "road_mesh")
    self.graphdata.connect(m.inputs.In, spine.outputs.Out)   # XfNodeGraph edge
    if height is not None:
      plug = getattr(height, "output_plug", None)
      if plug is None:
        plug = height.outputs.Out
      self.graphdata.connect(m.inputs.Height, plug)          # OPTIONAL grounding field
    return m

  # ---- flatten seam: stock MaskBlend composes RoadElev by Roadbed over the terrain
  #      height (ZERO new terrain C++). Callers wire T.mask_blend(A=height, B=bed.RoadElev,
  #      M=bed.Roadbed) in the terrain re-bake — kept in the terrain DSL, not duplicated here.

  def output(self, node):
    self._terminal = node
    return node

  def close(self):
    self._close_trace()
    return self.graphdata


# re-export the RoadProfile constants (kept aligned with mask_ref.RoadProfile)
class RoadProfile:
  def __init__(self, width_m=6.0, shoulder_m=2.0, v_meters_per_tile=8.0):
    self.width_m = float(width_m)
    self.shoulder_m = float(shoulder_m)
    self.v_meters_per_tile = float(v_meters_per_tile)


class BuildTypeTable:
  """Building-archetype weight table (owner Q2: variant sets are DATA — a declared
  list, consumed generically; adding a type is zero-C++)."""
  def __init__(self, weights):
    self.weights = [float(w) for w in weights]
