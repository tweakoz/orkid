###############################################################################
# Cholla / cane cactus (Cylindropuntia) — L-system archetype 0 (sympodial) with
# short fat low-taper segments + heavy jitter → gnarled jointed candelabra.
#   ./_ork.hypermesh.validate.py cholla -o /tmp/cholla.png
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype


class Cholla(Hypermesh):
  def __init__(self, depth=6, children=2, seg_len=0.35, base_radius=0.09, sides=8,
               branch_angle=50.0, tropism=0.09, jitter=0.4, apical=0.1, internodes=1,
               len_decay=0.82, rad_decay=0.82, roll=100.0, seed=17, budget=4000):
    super().__init__()
    n = self.lsystem(archetype=Archetype.SYMPODIAL, 
                     depth=depth, 
                     children=children, 
                     seg_len=seg_len,
                     base_radius=base_radius, 
                     sides=sides, 
                     branch_angle=branch_angle,
                     tropism=tropism, 
                     jitter=jitter, 
                     apical=apical, 
                     internodes=internodes,
                     len_decay=len_decay, 
                     rad_decay=rad_decay, 
                     roll=roll, 
                     seed=seed,
                     budget=budget)
    self.output(n)


__all__ = ["Cholla"]
