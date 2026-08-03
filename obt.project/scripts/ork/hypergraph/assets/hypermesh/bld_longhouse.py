###############################################################################
# bld_longhouse — GR-B variant B: a two-storey gabled LONGHOUSE (4 bays — the
# widest footprint; a 7-window through-grid over the long facades, shallower
# gable than the cottage so the two silhouettes read apart at scatter distance).
#
# GID MAP: 0 WALL  1 GLASS  2 DOOR  3 ROOF  4 TRIM   (see _building.py)
# VET: allow_self_intersect — embedded trims by design (kit-bash assembly).
#
#   ork.hypermesh.viewer.py bld_longhouse --msaa 2
#   _ork.hypermesh.validate.py bld_longhouse -o /tmp/bld_longhouse.obj
###############################################################################
from ork.hypergraph.assets.hypermesh._building import Building


class Longhouse(Building):
  VET = dict(allow_self_intersect=True)

  def __init__(self, seed=23, **overrides):
    defaults = dict(storeys=2,
                    bays=4,
                    bay_w=2.6,
                    storey_h=2.9,
                    depth=6.2,
                    roof="gable",
                    roof_h=1.9,
                    ridge_w=0.14,
                    win_w=1.2,
                    win_h=1.4,
                    door_w=1.3)
    super().__init__(seed=seed, **{**defaults, **overrides})


__all__ = ["Longhouse"]
