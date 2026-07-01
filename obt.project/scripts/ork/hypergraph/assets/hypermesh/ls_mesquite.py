###############################################################################
# Mesquite (Prosopis) — L-system archetype 0 (sympodial): a gnarled, spreading
# desert tree; strong stochastic jitter + upward tropism + apical dominance.
#   ./_ork.hypermesh.validate.py mesquite -o /tmp/mesquite.png
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype


class Mesquite(Hypermesh):
  def __init__(self, depth=7, children=2, seg_len=0.7, base_radius=0.18, sides=6,
               branch_angle=42.0, tropism=0.07, jitter=0.35, apical=0.35, internodes=3,
               len_decay=0.8, rad_decay=0.72, roll=137.5, seed=11, budget=6000):
    super().__init__()
    n = self.lsystem(archetype=Archetype.SYMPODIAL, depth=depth, children=children, seg_len=seg_len,
                     base_radius=base_radius, sides=sides, branch_angle=branch_angle,
                     tropism=tropism, jitter=jitter, apical=apical, internodes=internodes,
                     len_decay=len_decay, rad_decay=rad_decay, roll=roll, seed=seed,
                     budget=budget)
    self.output(n)


__all__ = ["Mesquite"]
