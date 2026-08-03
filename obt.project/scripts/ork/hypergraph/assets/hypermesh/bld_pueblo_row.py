###############################################################################
# bld_pueblo_row — pueblo variant A: a single-storey ROW BLOCK (4 contiguous
# room modules under one flat roof — the aggregated cellular staple; parapet +
# viga ends + a couple of T-doors and sparse small openings; blank back wall).
#
# GID MAP: 0 WALL adobe  2 DOOR timber  3 ROOF adobe  4 TRIM timber  (_pueblo.py)
# VET: allow_self_intersect — vigas/lintels/plates embed by design (kit-bash).
#
#   ork.hypermesh.viewer.py bld_pueblo_row --msaa 2
#   _ork.hypermesh.validate.py bld_pueblo_row -o /tmp/bld_pueblo_row.obj
###############################################################################
from ork.hypergraph.assets.hypermesh._pueblo import Pueblo


class PuebloRow(Pueblo):
  VET = dict(allow_self_intersect=True)

  def __init__(self, seed=17, **overrides):
    defaults = dict(storeys=1,
                    rooms=4,
                    room_w=3.6,
                    depth=5.2,
                    storey_h=2.5,
                    parapet_h=0.4,
                    n_doors=2,
                    win_keep=0.55)
    super().__init__(seed=seed, **{**defaults, **overrides})


__all__ = ["PuebloRow"]
