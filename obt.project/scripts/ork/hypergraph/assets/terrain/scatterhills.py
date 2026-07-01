###############################################################################
# scatterhills — rolling fbm hills with a two-type SCATTER sink (the E.2 demo
# terrain). The "rocks" sink places two mutually-exclusive types over altitude
# masks: type 0 ("boulder") favors the valleys, type 1 ("shard") the ridges.
# Consumers reference the placement as scatter_asset=<asset>, sink="rocks".
###############################################################################

from ork.hypergraph.dflow.terrain import HeightField
from ork.hypergraph.dflow import terrain as T


class ScatterHills(HeightField):
  # REALISTIC, NAVIGABLE proportions: 2km x 2km, 100m total relief. Rolling fbm
  # (no terracing — at this scale terrace risers become unwalkable cliffs);
  # typical slopes ~5-15 deg, walkable by the 2m capsule at 10mph.
  EXTENT_M = 2048.0
  HEIGHT_M = 100.0

  def __init__(self, octaves=6):
    super().__init__()
    h = T.fbm(frequency=3.0, octaves=octaves) * 0.75 + T.fbm(frequency=10.0) * 0.25
    self.capture(h, "height")

    alt = T.normalize(h)
    self.scatter("rocks",
        density = 0.010,             # ~rocks/m^2 over the kept mask (4x denser)
        seed    = 31,
        align   = "normal",
        scale   = (1.0, 2.6),
        cutoff  = 0.08,
        jitter  = 0.95,
        lift    = 0.15,
        types   = {"boulder": 1.0 - alt,    # valleys
                   "shard":   0.3 + alt},   # ridges + a FLOOR (shards everywhere)
        # the PHYSICS PROXY per type — baked PER POINT into the ScatterSet, so the
        # collider shape rides the data (BulletShapeScatter reads items, no hardcoding).
        # boulder: icosphere r=1.1 -> sphere; shard: cone r=0.7 h=3.2 -> CONE proxy
        # (btConeShape, exact match of the visual primitive; base at item origin).
        colliders = {"boulder": ("sphere", 1.1),
                     "shard":   ("cone", 0.7, 3.2)})


__all__ = ["ScatterHills"]
