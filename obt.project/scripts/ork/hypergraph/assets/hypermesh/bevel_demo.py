###############################################################################
# bevel_demo — the first real EDGE BEVEL (chamfer). A welded cube, all 12 edges selected by dihedral,
# beveled by `amount`, then face_normals for hard-surface shading.
#
#   box -> smooth_normals -> select(LINE, dihedral>10) -> bevel(0.2) -> face_normals
#
# Expect a chamfered cube: 6 shrunk square faces + 12 flat chamfer strips along the edges + 8 triangular
# corner caps. `amount` is a RUNTIME plug — onUpdate animates it (the topology is baked amount-independent;
# only positions recompute on GPU, so the chamfer grows/shrinks smoothly with no rebuild).
#
#   ./ork.hypermesh.viewer.py bevel_demo        # [W] wireframe to see the chamfer/cap topology
###############################################################################
import math
from ork.hypergraph.dflow.hypermesh import Hypermesh, sel_dihedral_gt, LINE, replace, group


class BevelDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    w = self.smooth_normals(self.box(size=1.0))
    s = self.select(w, sel_dihedral_gt(10.0), domain=LINE, op=replace(group(0)))   # all 12 cube edges (90°)
    self._bevel = self.bevel(s, amount=0.2, slot=0)
    self.output(self.face_normals(self._bevel))

  def onUpdate(self, updinfo):
    t = updinfo.absolutetime
    self._bevel.inputs.amount = 0.18 + 0.12 * math.sin(t * 1.5)   # animate the chamfer width (runtime plug)


__all__ = ["BevelDemo"]
