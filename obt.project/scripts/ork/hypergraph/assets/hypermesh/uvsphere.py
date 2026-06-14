###############################################################################
# UvSphere — a lat/long sphere with MIXED topology: QUAD body faces + TRIANGLE pole fans, in one
# indexed mesh. The render-time compute triangulates the mixed faces. `segments`/`rings` are runtime
# plugs; onUpdate ANIMATES them (and pulses `radius`) — live dynamic topology: the tessellation grows
# and shrinks (segments 4<->48, rings 3<->24, ping-pong) over a 15s loop.
#
#   from ork.hypergraph.assets.hypermesh import UvSphere
#   gmesh = UvSphere().materialize(ctx)
###############################################################################
import math
from ork.hypergraph.dflow.hypermesh import Hypermesh


class UvSphere(Hypermesh):
  def __init__(self, radius=1.6, segments=32, rings=16):
    super().__init__()
    self._base_radius = radius
    self._sphere = self.uvsphere(radius=radius, segments=segments, rings=rings)
    self.output(self._sphere)

  # live: ping-pong the tessellation (segments/rings) + pulse the radius over a 15s loop.
  def onUpdate(self, updinfo):
    t     = updinfo.absolutetime
    phase = (t % 15.0) / 15.0
    tri   = 1.0 - abs(2.0 * phase - 1.0)              # 0 -> 1 -> 0 over 15s
    self._sphere.inputs.segments = int(round(4 + (48 - 4) * tri))
    self._sphere.inputs.rings    = int(round(3 + (24 - 3) * tri))
    self._sphere.inputs.radius   = self._base_radius + 0.18 * math.sin(t * 1.3)


__all__ = ["UvSphere"]
