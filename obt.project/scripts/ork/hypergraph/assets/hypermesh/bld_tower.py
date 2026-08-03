###############################################################################
# bld_tower — GR-B variant C: a four-storey FLAT-ROOF tower (1 bay, near-square
# footprint, parapet ring + recessed roof deck — the vertical landmark of the
# hamlet skyline; rare in the scatter distribution).
#
# GID MAP: 0 WALL  1 GLASS  2 DOOR  3 ROOF  4 TRIM   (see _building.py)
# VET: allow_self_intersect — embedded trims by design (kit-bash assembly).
#
#   ork.hypermesh.viewer.py bld_tower --msaa 2
#   _ork.hypermesh.validate.py bld_tower -o /tmp/bld_tower.obj
###############################################################################
from ork.hypergraph.assets.hypermesh._building import Building


class Tower(Building):
  VET = dict(allow_self_intersect=True)

  def __init__(self, seed=37, **overrides):
    defaults = dict(storeys=4,
                    bays=1,
                    bay_w=3.6,
                    storey_h=2.6,
                    depth=3.6,
                    roof="flat",
                    parapet_drop=0.55,
                    win_w=0.9,
                    win_h=1.2,
                    door_w=1.2,
                    plinth_h=0.4)
    super().__init__(seed=seed, **{**defaults, **overrides})


__all__ = ["Tower"]
