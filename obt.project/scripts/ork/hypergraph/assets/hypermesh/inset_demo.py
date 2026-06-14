###############################################################################
# InsetDemo — the `inset` op (a face -> a smaller inner polygon + a collar ring),
# generalized: `sides=8` resamples each quad/tri cap to a regular OCTAGON, so the
# studs we then extrude have an 8-sided (rounder) cross-section instead of square.
# Every sphere face is inset to an octagon, that octagon is tagged into bit 1, and
# extruded into a short stud. onUpdate animates the inset `amount` (a RUNTIME plug,
# no recompile) so the octagon caps — and the studs on them — breathe.
#
# Try: fill=False on the inset for octagonal HOLES; sides=None for plain square studs.
#
#   ./ork.hypermesh.viewer.py inset_demo
###############################################################################
import math
from ork.hypergraph.dflow.hypermesh import Hypermesh, S, isolate, group, POLY


class InsetDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    n = self.uvsphere(radius=1.6, segments=24, rings=16)
    n = self.select(n, S.area > -1.0, domain=POLY, op=isolate(group(0)))   # every face
    # quad/tri -> regular OCTAGON cap (the collar bridges the boundary to it); tag the cap into bit 1.
    self._inset = self.inset(n, amount=0.35, sides=8, slot=0, mask_inner=isolate(group(1)))
    # grow the octagon caps into studs. mode="vertex" (region) extrude rises from the octagon's SHARED rim
    # (one edge loop used by both the collar and the walls) -> one watertight manifold, no floor, no doubled
    # rim, winding inherited consistently from the surface (no see-through, consistent from inside too).
    n = self.extrude_faces(self._inset, distance=0.25, slot=1, mode="vertex", mask_cap=isolate(group(1)))
    # vertex-mode walls inherit the surface normal -> recompute. smooth_normals averages over the shared rim,
    # giving the manifold studs a clean filleted look (the boss base/top read as soft rolled edges).
    n = self.smooth_normals(n)
    self.output(n)

  def onUpdate(self, updinfo):
    t = updinfo.absolutetime
    self._inset.inputs.amount = 0.30 + 0.18 * math.sin(t * 1.2)   # octagon caps breathe (runtime, no rebuild)


__all__ = ["InsetDemo"]
