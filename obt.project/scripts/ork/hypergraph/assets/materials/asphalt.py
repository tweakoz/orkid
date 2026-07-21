###############################################################################
# Asphalt — parametric road-surface material (GEOV2 ptex3d), UV-charted.
#
# ONE material for every road width. It reads the road's own UV chart (the same
# one the R-family road modules emit): U = ctx.uv.x is LATERAL, normalized 0..1
# across `width_m`; V = ctx.uv.y is ARC-LENGTH in METERS (world-metric). Every
# feature is designed off that chart, so it lays down straight on a straight road
# and follows the arc-length through a curve with no stretch.
#
# The 1/2/4-lane "set" is DATA, not code: ROAD_PRESETS is a declared dict of
# variant kwargs over this one material (owner: variant sets are data). lane_count
# and width_m ride as BINDABLE ctx.params — the lane-divider math is closed-form in
# the runtime lane_count (no Python loop, nothing baked), so ONE generated shader
# serves every variant; picking a lane count just rebinds a uniform default.
#
#   from ork.hypergraph.assets.materials import Asphalt, ROAD_PRESETS, road_preset
#   mat = self.asset.Ptex3d("road", dsl_class=Asphalt, **road_preset("2lane"))
#   mat = self.asset.Ptex3d("road", dsl_class=Asphalt, lane_count=4, width_m=14.6)
#
# Features: interior lane dividers (lane_count-1, DASHED via a V-periodic mask,
# yellow where a divider coincides with the road centre, white elsewhere), solid
# edge lines inset from each shoulder, aggregate/patch albedo+roughness noise in
# metric space (continuous along V -> no tiling seam), and a lane-count-aware
# wheel-track wear darkening. Asphalt reads MATTE (roughness well above 0.75).
#
# Bake-time (ctor, structural): oct_macro / oct_agg (fbm loop bounds).
###############################################################################

from orkengine.core import vec3
from ork.hypergraph.ptex3d import Ptex3d, P
from ork.hypergraph.ptex3d.functions import stripe_lattice, dash_mask, fbm2d


# The declared, self-defended lane-count set. A value outside it (or a non-positive
# width) is refused loudly at authoring time.
ROAD_LANE_COUNTS = (1, 2, 4)

# Variant DATA: presets = kwargs over the ONE Asphalt material (never subclasses).
# Widths are whole-carriageway meters at a ~3.65 m (12 ft) lane.
ROAD_PRESETS = {
  "1lane": dict(lane_count=1, width_m=3.7),
  "2lane": dict(lane_count=2, width_m=7.3),
  "4lane": dict(lane_count=4, width_m=14.6),
}


def road_preset(name):
  """Look up a declared road variant -> a fresh kwargs dict for Asphalt. Loud KeyError
  (naming the available presets) on an unknown variant."""
  if name not in ROAD_PRESETS:
    raise KeyError("road_preset(%r): unknown road variant; declared presets are %s"
                   % (name, sorted(ROAD_PRESETS)))
  return dict(ROAD_PRESETS[name])


class Asphalt(Ptex3d):
  def __init__(self, ctx, *, oct_macro=4, oct_agg=5,
               lane_count=2, width_m=7.3,
               meters_per_tile=4.0, aggregate_m=0.045,
               asphalt_color=vec3(0.055, 0.056, 0.060),
               roughness=0.92, rough_var=0.06,
               line_width_m=0.12, edge_inset_m=0.22,
               dash_period_m=12.0, dash_duty=0.28,
               line_color=vec3(0.80, 0.80, 0.78),
               center_color=vec3(0.72, 0.60, 0.06), line_roughness=0.66,
               wheel_track_m=1.8, wear_width_m=0.55, wear_amount=0.38,
               shoulder_color=vec3(0.30, 0.245, 0.175), shoulder_roughness=0.96,
               shoulder_agg_m=0.5):
    # ── self-defend: only the declared lane counts, only positive widths ──
    lc_round = int(round(lane_count))
    if abs(lane_count - lc_round) > 1e-6 or lc_round not in ROAD_LANE_COUNTS:
      raise ValueError(
        "Asphalt: lane_count=%r is not in the declared road set %s "
        "(use ROAD_PRESETS / road_preset)." % (lane_count, list(ROAD_LANE_COUNTS)))
    if width_m <= 0.0:
      raise ValueError("Asphalt: width_m=%r must be positive (meters across the road)." % (width_m,))

    lanes  = ctx.param("lane_count",      lane_count)      # runtime lane count (closed-form; not baked)
    width  = ctx.param("width_m",         width_m)         # road width (meters), U normalizes across it
    mpt    = ctx.param("meters_per_tile", meters_per_tile) # asphalt macro-texture wavelength (meters)
    aggm   = ctx.param("aggregate_m",     aggregate_m)     # fine aggregate wavelength (meters)
    asphc  = ctx.param("asphalt_color",   asphalt_color)
    rough  = ctx.param("roughness",       roughness)       # asphalt roughness (matte, >0.75)
    rvar   = ctx.param("rough_var",       rough_var)
    linw   = ctx.param("line_width_m",    line_width_m)     # painted line width (meters)
    einset = ctx.param("edge_inset_m",    edge_inset_m)     # edge line inset from the pavement edge
    dperm  = ctx.param("dash_period_m",   dash_period_m)    # dashed divider on+off length (meters)
    dduty  = ctx.param("dash_duty",       dash_duty)        # painted fraction of the dash cycle
    linec  = ctx.param("line_color",      line_color)
    cenc   = ctx.param("center_color",    center_color)     # centreline yellow
    linr   = ctx.param("line_roughness",  line_roughness)   # paint semi-matte sheen
    wtrk   = ctx.param("wheel_track_m",   wheel_track_m)     # tyre spacing within a lane
    wwid   = ctx.param("wear_width_m",    wear_width_m)      # wheel-track darkening band width
    wamt   = ctx.param("wear_amount",     wear_amount)       # wheel-track darkening strength

    lat    = ctx.uv.x                                        # U: 0..1 across the road width
    arc    = ctx.uv.y                                        # V: arc-length in meters
    metric = P.vec2(lat * width, arc)                        # both axes in meters (isotropic aggregate)
    half_u = (linw * 0.5) / width                            # painted half-line in U units

    # ── REGION selector (ctx.Cd.x from the RoadMesh COLOR channel): 0 road / 1 junction /
    #    2 shoulder. Each face is region-uniform, so a hard step is crisp. Markings are
    #    painted ONLY on the road ribbon (suppressed across the junction apron — owner:
    #    "no centreline through the junction box"); the shoulder is a gravel/dirt band. ──
    region      = ctx.Cd.x
    is_road     = 1.0 - P.step(0.5, region)                  # 1 on the ribbon, 0 on apron/shoulder
    is_shoulder = P.step(1.5, region)                        # 1 on the grounding skirt
    shcol   = ctx.param("shoulder_color",     shoulder_color)   # dirt/gravel tone
    shrough = ctx.param("shoulder_roughness", shoulder_roughness)
    shaggm  = ctx.param("shoulder_agg_m",     shoulder_agg_m)    # gravel stone wavelength (meters)
    gpos    = P.vec2(ctx.P.x, ctx.P.z)                       # world XZ -> gravel varies in 3D, UV-free
    gspeck  = fbm2d(gpos * (1.0 / P.max(shaggm, 0.01)), oct_agg)
    gcoarse = fbm2d(gpos * (1.0 / P.max(shaggm * 8.0, 0.08)), oct_macro)
    gravel  = shcol * (P.mix(0.70, 1.28, gspeck) * P.mix(0.84, 1.12, gcoarse))
    gravel_r = shrough + 0.04 * (gspeck - 0.5)

    # ── asphalt base: coarse patch tone x fine aggregate speckle, both continuous
    #    in V (no fract/tile) so there is NO tiling seam down the road ──
    macro = fbm2d(metric * (1.0 / mpt), oct_macro)           # large-scale patchiness [0,1]
    agg   = fbm2d(metric * (1.0 / aggm), oct_agg)            # aggregate stones [0,1]
    asph  = asphc * (P.mix(0.72, 1.34, agg) * P.mix(0.86, 1.14, macro))

    # ── lane lattice: closed-form in the runtime lane count ──
    lane   = stripe_lattice(lat, lanes)

    # ── wheel-track wear (lane-count-aware): two darkened strips per lane, at the
    #    tyre offsets either side of each lane centre; broken up along V ──
    d_cen_m  = lane.center * width                           # meters from the nearest lane centre
    wear_lat = 1.0 - P.smoothstep(0.0, wwid, P.abs(d_cen_m - wtrk * 0.5))
    wear_v   = P.mix(0.6, 1.0, fbm2d(P.vec2(lat * width * 3.0, arc * 0.22), 3))
    wear     = P.saturate(wear_lat * wear_v) * wamt
    asph     = asph * P.mix(1.0, 0.62, wear)                 # darker in the tracks
    asph_r   = rough + rvar * (agg - 0.5) - wear * 0.10      # a touch polished where worn

    # ── interior lane dividers, DASHED (yellow at the true centre, white else) — RIBBON ONLY ──
    div_core = 1.0 - self.aa_ramp(lane.boundary, half_u * 0.6, half_u)
    dash     = dash_mask(arc, dperm, dduty)
    divider  = P.saturate(div_core) * lane.interior * dash * is_road
    cen_prox = 1.0 - self.aa_ramp(P.abs(lat - 0.5), half_u, half_u * 2.0)   # 1 only if a divider is at U=0.5
    div_col  = P.mix(linec, cenc, cen_prox)

    # ── solid edge lines, inset from each shoulder — RIBBON ONLY (terminate at the apron) ──
    eu     = einset / width
    d_edge = P.min(P.abs(lat - eu), P.abs(lat - (1.0 - eu)))
    edge   = P.saturate(1.0 - self.aa_ramp(d_edge, half_u * 0.6, half_u)) * is_road

    # ── composite: asphalt, then edge paint, then divider paint on top; then the gravel
    #    shoulder band replaces the surface entirely where region == shoulder ──
    alb   = P.mix(asph, linec, edge)
    alb   = P.mix(alb, div_col, divider)
    paint = P.max(edge, divider)
    rough = P.mix(asph_r, linr, paint)                       # asphalt matte, paint semi-matte
    alb   = P.mix(alb, gravel, is_shoulder)
    rough = P.mix(rough, gravel_r, is_shoulder)
    self.surface(
      albedo    = alb,
      metallic  = 0.0,
      roughness = rough,
    )


__all__ = ["Asphalt", "ROAD_PRESETS", "ROAD_LANE_COUNTS", "road_preset"]
