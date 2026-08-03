###############################################################################
# bld_cottage — GR-B variant A: a single-storey gabled COTTAGE (the smallest
# grammar building — 3 bays, centre door, 2 through-windows, steep-ish gable).
# All geometry from the _building recipe; this file only fixes the proportions.
#
# GID MAP (semantic; document at every preset — the A1 law):
#   0 WALL  1 GLASS  2 DOOR  3 ROOF  4 TRIM
#
# VET: allow_self_intersect — the panes/door/plinth EMBED into the walls (kit-bash
# assembly); crossing hits + parity-ambiguous interiors demote to WARN by intent.
#
#   ork.hypermesh.viewer.py bld_cottage --msaa 2
#   _ork.hypermesh.validate.py bld_cottage -o /tmp/bld_cottage.obj
###############################################################################
from ork.hypergraph.assets.hypermesh._building import Building


class Cottage(Building):
  VET = dict(allow_self_intersect=True)

  def __init__(self, seed=11, **overrides):
    defaults = dict(storeys=1,
                    bays=3,
                    bay_w=2.4,
                    storey_h=2.8,
                    depth=5.0,
                    roof="gable",
                    roof_h=1.7,
                    win_w=1.1,
                    win_h=1.3)
    super().__init__(seed=seed, **{**defaults, **overrides})


__all__ = ["Cottage"]
