###############################################################################
# Saguaro (Carnegiea gigantea) — L-system archetype 2: a thick vertical trunk
# with a couple of arms that grow out then curl sharply upward.
#   ./_ork.hypermesh.validate.py saguaro -o /tmp/saguaro.png
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype


class Saguaro(Hypermesh):
  def __init__(self, seed=3, budget=4000):
    super().__init__()
    # rad_decay = arm thickness step per level (~15% reduction); taper = gentle along-shoot
    # thinning. Both are DSL knobs now — no proportions baked in C++.
    n = self.lsystem( archetype=Archetype.SAGUARO, 
                      depth=14, 
                      base_radius=0.45,
                      rad_decay=0.85, 
                      seg_len=0.5, 
                      len_decay=0.5,
                      branch_angle=50,
                      internodes=1,
                      sides=18, 
                      children=4, 
                      taper=0.55,
                      tropism=1.0, 
                      jitter=0.0, 
                      roll=127.0,
                      apical=0.1,
                      
                      seed=seed, 
                      budget=budget)
    self.output(n)


__all__ = ["Saguaro"]
