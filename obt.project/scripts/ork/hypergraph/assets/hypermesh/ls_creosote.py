###############################################################################
# Creosote bush (Larrea tridentata) — L-system archetype 0 (sympodial): a low,
# dense, many-shooted desert shrub (thin stems, short internodes).
#   ./_ork.hypermesh.validate.py creosote -o /tmp/creosote.png
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype


class Creosote(Hypermesh):
  def __init__(self, depth=5, children=3, seg_len=0.4, base_radius=0.05, sides=5,
               branch_angle=46.0, tropism=0.07, jitter=0.35, apical=0.1, internodes=2,
               len_decay=0.8, rad_decay=0.72, roll=137.5, seed=19, budget=4000):
    super().__init__()
    n = self.lsystem(archetype=Archetype.SYMPODIAL, depth=depth, children=children, seg_len=seg_len,
                     base_radius=base_radius, sides=sides, branch_angle=branch_angle,
                     tropism=tropism, jitter=jitter, apical=apical, internodes=internodes,
                     len_decay=len_decay, rad_decay=rad_decay, roll=roll, seed=seed,
                     budget=budget)
    self.output(n)


__all__ = ["Creosote"]
