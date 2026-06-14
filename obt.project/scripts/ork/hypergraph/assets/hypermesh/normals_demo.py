###############################################################################
# NormalsDemo — GROUP-BASED normal types: FLAT base, SMOOTH extrusions. The octagon
# studs from InsetDemo, but the base surface and the risen studs get different shading:
#   BASE collar (bit 0)         -> face_normals(slot=0)    (unwelded -> hard faceted)
#   EXTRUSION walls+caps (bit 2)-> smooth_normals(slot=2)  (welded -> rounded / filleted)
# The stud rim (where a smooth wall meets the flat base) splits into a hard CREASE —
# like assigning different smoothing groups per region in a modeling app.
#
# Tagging: the inset puts the octagon cap in bit 1; the extrude then tags its cap+walls
# into bit 2 (the extrusion) while the untouched collar keeps bit 0 (the base).
#
#   ./ork.hypermesh.viewer.py normals_demo
###############################################################################
import math
from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY


class NormalsDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.uvsphere(radius=1.6, segments=24, rings=16)
    n = self.select(n, S.area > -1.0, domain=POLY, op=isolate(group(0)))               # all faces -> base (bit 0)
    self._inset = self.inset(n, amount=0.30, sides=8, slot=0, mask_inner=isolate(group(1)))
    # extrude the octagon caps; tag the risen cap + walls into bit 2 (the extrusion). collar stays bit 0.
    n = self.extrude_faces(self._inset, distance=0.30, slot=1, mode="vertex",
                           mask_cap=isolate(group(2)), mask_wall=isolate(group(2)))
    n = self.face_normals(n,   slot=0)                                                 # base  -> flat
    n = self.smooth_normals(n, slot=2)                                                 # studs -> smooth
    self.output(n)

  def onUpdate(self, updinfo):
    t = updinfo.absolutetime
    self._inset.inputs.amount = 0.26 + 0.16 * math.sin(t * 1.0)                        # studs breathe


__all__ = ["NormalsDemo"]
