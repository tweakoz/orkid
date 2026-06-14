###############################################################################
# RippleGrid — a sine-rippled grid fed into ONE SubdivideModule whose `level` is a runtime int plug
# (0=passthrough, L=L rounds of 1->4). onUpdate animates the ripple amp/freq AND cycles the subdivide
# level 0,1,2,3 (factor 1,2,4,8) once a second — DYNAMIC TOPOLOGY every frame (the module re-pools its
# buffers when the size-class changes). No recompute_tbn: subdivide interpolates+normalizes N/B.
#
#   from ork.hypergraph.assets.hypermesh import RippleGrid
#   gmesh = RippleGrid(grid=64, subdivide=2).materialize(ctx)
###############################################################################
import math
from ork.hypergraph.dflow.hypermesh import Hypermesh


class RippleGrid(Hypermesh):
  def __init__(self, grid=64, amp=1.2, freq=2.2, subdivide=2):
    super().__init__()
    self._base_amp  = amp
    self._base_freq = freq
    self._ripple = self.ripple(grid=grid, amp=amp, freq=freq)
    self._subdiv = self.subdivide(self._ripple,smooth=True)     # ONE module; runtime `level` int plug
    self._subdiv.inputs.level = int(subdivide)
    self.output(self._subdiv)

  # live hook: app passes updinfo each frame -> animate the ripple AND cycle the subdivide level.
  def onUpdate(self, updinfo):
    t = updinfo.absolutetime*0.3
    self._ripple.inputs.amp   = self._base_amp  + 0.9 * math.sin(t * 2.0)
    self._ripple.inputs.freq  = self._base_freq + 1.0 * math.sin(t * 0.7)
    self._subdiv.inputs.level = (2 + int(t) % 4)          # 0,1,2,3 (factor 1,2,4,8) once per second


__all__ = ["RippleGrid"]
