###############################################################################
# bld_pueblo_stepped — pueblo variant B: a TWO-storey stepped block. The upper
# storey is SET BACK (and one room short, hugging one end) so the lower roof is
# the upper level's TERRACE: front + side parapets, a terrace-level T-door, viga
# ends under both rooflines, and a leaning timber ladder — the Taos silhouette.
#
# GID MAP: 0 WALL adobe  2 DOOR timber  3 ROOF adobe  4 TRIM timber  (_pueblo.py)
# VET: allow_self_intersect — vigas/lintels/plates/ladder embed by design.
#
#   ork.hypermesh.viewer.py bld_pueblo_stepped --msaa 2
#   _ork.hypermesh.validate.py bld_pueblo_stepped -o /tmp/bld_pueblo_stepped.obj
###############################################################################
from ork.hypergraph.assets.hypermesh._pueblo import Pueblo


class PuebloStepped(Pueblo):
  VET = dict(allow_self_intersect=True)

  def __init__(self, seed=41, **overrides):
    defaults = dict(storeys=2,
                    rooms=4,
                    room_w=3.8,
                    depth=5.6,
                    storey_h=2.5,
                    parapet_h=0.4,
                    setback=2.4,
                    upper_rooms=3,
                    n_doors=1,
                    win_keep=0.5,
                    ladder=True)
    super().__init__(seed=seed, **{**defaults, **overrides})


__all__ = ["PuebloStepped"]
