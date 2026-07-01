###############################################################################
# Conifer (pine/fir) — L-system archetype 1: a straight central leader with a
# whorl of drooping laterals at each level, shorter toward the top (conical).
#   ./_ork.hypermesh.validate.py conifer -o /tmp/conifer.png
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, Archetype


class Conifer(Hypermesh):
  def __init__(self, seed=7, budget=9000):
    super().__init__()
    n = self.lsystem(archetype=Archetype.CONIFER, 
                     depth=13.5, 
                     children=12, 
                     seg_len=0.6,
                     base_radius=0.22,
                     taper=1.0, 
                     sides=18, 
                     branch_angle=77,
                     tropism=1.52, 
                     jitter=0.35, 
                     jit_azimuth=0.18, # 0.8 
                     jit_pitch=0.35, # 0.35
                     jit_length=0.5, # 0.5
                     jit_spacing=0.3, # 0.3
                     jit_drop=0.25, # 0.25
                     jit_wave=0.25, # 0.25
                     roll=70.0, 
                     len_decay=0.75,
                     rad_decay=0.75, 
                     seed=seed, 
                     budget=budget)
    self.output(n)


__all__ = ["Conifer"]
