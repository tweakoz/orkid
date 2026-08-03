###############################################################################
# hamletvale — rolling meadow vale with a "buildings" SCATTER sink (the GR-B
# hamlet ground). Placement = the KEEP-OUT mask product: buildings are kept OUT
# of steep ground (slope), OUT of the ridge tops + lowest hollows (elevation
# band), and OUT of everywhere except the worley-cell VILLAGE clusters — so the
# few placed buildings gather into hamlets instead of salting the whole map.
# Three mutually-exclusive types (weighted per point, the holistic scatter):
#   type 0 cottage    — the staple, anywhere the mask allows
#   type 1 longhouse  — demands the FLATTEST ground (long footprint)
#   type 2 tower      — rare landmark (low relative weight)
# Consumers bind meshes per type via drawable_data(instance_source=(<asset>,
# "buildings", type_id)). Box collider proxies ride the set (btBoxShape is
# CENTERED at the item origin -> y half-extent = FULL height covers 0..H above
# grade; the buried lower half is harmless).
###############################################################################
from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.materials.terrain.ground import Ground
from ork.hypergraph.colors import hsv
from ork.hypergraph.units import meters


class HamletVale(HeightField):
  EXTENT_M       = 1024.0            # 1km x 1km — a walkable vale, 2-3 visible clusters
  MATERIAL_CLASS = Ground
  MATERIAL_PARAMS = dict(albedo_lo=hsv(95, 0.35, 0.30),    # meadow green
                         albedo_hi=hsv(65, 0.30, 0.42),    # dry-grass patches
                         detail=0.30,
                         patch_m=48.0,
                         detail_m=3.0,
                         roughness=0.95)

  def __init__(self, relief=meters(55)):
    super().__init__()
    h = (T.fbm(frequency=2.4, octaves=6) * 0.72 +
         T.fbm(frequency=9.0, octaves=3) * 0.28) * relief
    self.capture(h, "height")

    # ---- the KEEP-OUT mask (each factor REMOVES ground from play) ----
    elev    = T.normalize(h)
    slope   = T.slope(h, radius_m=4.0)
    gentle  = 1.0 - T.smoothstep(slope, 0.10, 0.22)            # OUT: steep faces
    vale    = T.band(elev, 0.10, 0.55, soft=0.08)              # OUT: ridge tops + hollows
    village = T.band(T.worleyf1(frequency=3.0), 0.0, 0.30,     # OUT: everything but the
                     soft=0.12)                                # worley-cell cluster cores
    m       = gentle * vale * village

    self.scatter("buildings",
                 count   = 140,                    # a few hamlets, not a city
                 seed    = 7,
                 align   = "up",                   # buildings stand PLUMB (never slope-aligned)
                 yaw     = (0.0, 6.28318),
                 scale   = (1.0, 1.0),             # NEVER uniform-scale architecture: it scales the
                                                   # DOORS/storeys — the human-scale anchors. Variety
                                                   # comes from variants/params (bays), not scale.
                 cutoff  = 0.35,                   # hard keep-out floor (no stragglers off-mask)
                 jitter  = 0.55,                   # low jitter = grid-ish spacing, fewer overlaps
                 lift    = 0.0,                    # sill AT grade — slope bedding is the recipe's
                                                   # below-grade plinth skirt now, not a sink that
                                                   # billed 15cm to the door's visible height
                 types   = {"cottage":   m,
                            "longhouse": m * gentle,           # flattest ground only
                            "tower":     m * 0.25},            # rare
                 # half-extents per variant footprint (matches bld_* defaults + proud
                 # facades/plinth; y = FULL height, see header note on centered boxes)
                 colliders = {"cottage":   ("box", 3.7, 4.6, 2.9),
                              "longhouse": ("box", 5.3, 7.8, 3.5),
                              "tower":     ("box", 1.95, 10.5, 2.2)})


__all__ = ["HamletVale"]
