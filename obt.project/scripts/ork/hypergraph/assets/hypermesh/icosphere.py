###############################################################################
# IcoSphere — an icosahedron midpoint-subdivided + projected to the sphere (a true icosphere); all-tri,
# smooth (position-direction) normals. `subdivisions` is a runtime plug (0 = base 20-tri icosahedron).
# onUpdate ANIMATES subdivisions 0 -> 4 and back (ping-pong) over a 15s loop + pulses radius — watch
# the sphere refine level by level (live dynamic topology).
#
#   from ork.hypergraph.assets.hypermesh import IcoSphere
#   gmesh = IcoSphere(subdivisions=3).materialize(ctx)
###############################################################################
import math
from ork.hypergraph.dflow.hypermesh import Hypermesh


class IcoSphere(Hypermesh):
  def __init__(self, radius=1.6, subdivisions=2):
    super().__init__()
    self._base_radius = radius
    self._ico = self.icosphere(radius=radius, subdivisions=subdivisions)
    self._sub = self.subdivide(self._ico, smooth=True)
    self._sub.inputs.level = 2
    self.output(self._sub)

  # live: step the subdivision level 0 -> 4 -> 0 (ping-pong) over 15s + pulse the radius.
  def onUpdate(self, updinfo):
    t     = updinfo.absolutetime
    phase = (t % 15.0) / 15.0
    tri   = 1.0 - abs(2.0 * phase - 1.0)             # 0 -> 1 -> 0 over 15s
    #self._ico.inputs.subdivisions = int(round(4 * tri))
    self._ico.inputs.radius       = self._base_radius + 0.18 * math.sin(t * 1.3)


__all__ = ["IcoSphere"]
