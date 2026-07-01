###############################################################################
# Ocotillo (Fouquieria splendens) — L-system archetype 3: many long unbranched
# whip stems from a common base, splaying outward then curving up.
#   ./_ork.hypermesh.validate.py ocotillo -o /tmp/ocotillo.png
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype


class Ocotillo(Hypermesh):
  def __init__(self, depth=16, children=18, seg_len=0.4, base_radius=0.06, sides=5,
               branch_angle=30.0, taper=0.8, tropism=0.01, jitter=0.3, roll=10.0,
               seed=5, budget=4000):
    super().__init__()
    # branch_angle = splay off vertical; tropism ~0 so whips DON'T converge to vertical;
    # jitter drives the chaotic azimuth/tilt/waviness real ocotillo shows.
    n = self.lsystem(archetype=Archetype.OCOTILLO, depth=depth, children=children, seg_len=seg_len,
                     base_radius=base_radius, sides=sides, branch_angle=branch_angle,
                     taper=taper, tropism=tropism, jitter=jitter, roll=roll, seed=seed,
                     budget=budget)
    self.output(n)


__all__ = ["Ocotillo"]
