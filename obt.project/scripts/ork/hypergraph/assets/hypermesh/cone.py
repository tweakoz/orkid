###############################################################################
# Cone — MIXED topology: `sides` triangle side faces (apex fan) + ONE n-gon base cap, in one indexed
# mesh. The render fan-triangulates the base; subdivide turns it into n quads. `sides` is a runtime
# plug (>=3). onUpdate ANIMATES `sides` 3 -> 72 and back (ping-pong) over a 15s loop — live dynamic
# topology (the mesh re-pools as the side count crosses pow2 size-classes).
#
#   from ork.hypergraph.assets.hypermesh import Cone
#   gmesh = Cone(sides=24).materialize(ctx)
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh


class Cone(Hypermesh):
  def __init__(self, radius=1.5, height=2.5, sides=24):
    super().__init__()
    self._cone = self.cone(radius=radius, height=height, sides=sides)
    self.output(self._cone)

  # live: ping-pong the side count 3 <-> 72 over a 15-second loop (triangle wave).
  def onUpdate(self, updinfo):
    t     = updinfo.absolutetime
    phase = (t % 15.0) / 15.0
    tri   = 1.0 - abs(2.0 * phase - 1.0)         # 0 -> 1 -> 0 over 15s
    self._cone.inputs.sides = int(round(3 + (72 - 3) * tri))


__all__ = ["Cone"]
