###############################################################################
# UvSphereSubdiv — a MIXED quad/tri UV-sphere fed into ONE SubdivideModule whose `level` is a runtime
# int plug. onUpdate cycles the subdivide level 0,1,2,3 once per second (a full sweep every 4s) — the
# same dynamic-topology animation as [[RippleGrid]], but exercising the per-face fan-split on a mesh
# that mixes quad body faces with triangle pole fans (the render-time triangulator + subdivide must
# both handle the mixed CSR each frame, re-pooling buffers as the level changes).
#
#   from ork.hypergraph.assets.hypermesh import UvSphereSubdiv
#   live = UvSphereSubdiv().materialize_live(ctx)
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh


class UvSphereSubdiv(Hypermesh):
  def __init__(self, radius=1.6, segments=24, rings=12, subdivide=0):
    super().__init__()
    self._sphere = self.uvsphere(radius=radius, segments=segments, rings=rings)
    self._subdiv = self.subdivide(self._sphere)     # ONE module; runtime `level` int plug
    self._subdiv.inputs.level = int(subdivide)
    self.output(self._subdiv)

  # live hook: cycle the subdivide level 0,1,2,3 (one step / second; full sweep every 4s).
  def onUpdate(self, updinfo):
    t = updinfo.absolutetime
    self._subdiv.inputs.level = int(t) % 4


__all__ = ["UvSphereSubdiv"]
