###############################################################################
# secbake_scatter — the minimal SCATTER terrain for the section-bake x scatter-
# instancing gate (ork.data/scenes/ren_section_bake_scatter.py). Small, gently
# rolling, and DETERMINISTIC: a constant weight field + jitter=0 keeps every
# placer cell at its cell CENTER, so the sink always emits the same
# GRID_N x GRID_N grid of props (the placer derives its cell count as
# extent_m * sqrt(density) — hence the density expression below).
#
# Nothing here is tree-specific: it exists so a gate can exercise the
# scatter-SSBO instance path (instance_source=(asset, sink, type_id)) without
# baking a production-scale terrain.
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.materials.terrain.solid import Solid
from orkengine.core import vec3

GRID_N = 4            # placer cells per axis -> GRID_N^2 placed instances
EXTENT = 32.0         # terrain side, meters
SINK   = "props"      # the scatter sink name consumers reference
SOLO   = "solo"       # a SINGLE-point sink: the gate's multiplicity negative control
SCALE  = 2.5          # per-point uniform scale (SdfBaked box edge 1.2 -> 3 m props)
LIFT   = 1.5          # meters along the up axis: half a prop, so bases sit on the ground


class SecBakeScatter(HeightField):
  EXTENT_M       = EXTENT
  MATERIAL_CLASS = Solid
  # COOL ground albedo: the gate isolates the scattered props by WARMTH (R-B) — the baked
  # adobe/timber sections are warm — so the ground must not contribute warm pixels.
  MATERIAL_PARAMS = {"albedo": vec3(0.10, 0.12, 0.22), "roughness": 1.0}

  def __init__(self, relief=1.5):
    super().__init__()
    h = T.fbm(frequency=2.0, octaves=3) * relief     # gentle relief, TRUE METERS
    self.capture(h, "height")
    uniform = T.normalize(h) * 0.0 + 1.0             # constant-1 weight -> every cell is kept
    self.scatter(SINK,
        density = (GRID_N / EXTENT) ** 2,            # placer cells = extent_m * sqrt(density)
        seed    = 3,
        align   = "up",                              # props stay axis-aligned (top face = the TOP gid)
        yaw     = (0.0, 0.0),
        scale   = (SCALE, SCALE),
        cutoff  = 0.0,
        jitter  = 0.0,                               # cell centers: a repeatable grid
        lift    = LIFT,
        types   = {"prop": uniform})
    # The SOLO sink is the same placement recipe capped at ONE point: a gate consuming it
    # draws through the SAME instanced technique and the SAME bake, so only the instance
    # COUNT differs — which is exactly what a multiplicity oracle must be able to reject.
    self.scatter(SOLO,
        count   = 1,
        seed    = 3,
        align   = "up",
        yaw     = (0.0, 0.0),
        scale   = (SCALE, SCALE),
        cutoff  = 0.0,
        jitter  = 0.0,
        lift    = LIFT,
        types   = {"prop": uniform})


__all__ = ["SecBakeScatter", "GRID_N", "EXTENT", "SINK", "SOLO", "SCALE", "LIFT"]
