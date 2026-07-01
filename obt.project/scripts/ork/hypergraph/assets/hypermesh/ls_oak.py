###############################################################################
# Oak — L-system archetype 0 (sympodial): a fuller, rounder deciduous crown
# (more children, gentle angles, mild apical dominance).
#   ./_ork.hypermesh.validate.py oak -o /tmp/oak.png
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype


class Oak(Hypermesh):
  def __init__(self, depth=8, children=3, seg_len=0.6, base_radius=0.24, sides=18,
               branch_angle=36.0, tropism=0.05, jitter=0.3, apical=0.25, internodes=2,
               len_decay=0.78, rad_decay=0.7, roll=137.5, seed=13, budget=20000):
    super().__init__()
    n = self.lsystem(archetype=Archetype.SYMPODIAL, depth=depth, children=children, seg_len=seg_len,
                     base_radius=base_radius, sides=sides, branch_angle=branch_angle,
                     tropism=tropism, jitter=jitter, apical=apical, internodes=internodes,
                     len_decay=len_decay, rad_decay=rad_decay, roll=roll, seed=seed,
                     budget=budget)
    self.output(n)


__all__ = ["Oak"]
