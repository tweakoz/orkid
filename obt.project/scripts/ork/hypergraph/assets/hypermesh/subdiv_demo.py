###############################################################################
# SubdivDemo — Catmull-Clark SMOOTH subdivision. A cube CC-subdivides toward a sphere; the
# control cage is un-shared (per-face dup verts), so subdivide(smooth=True) WELDS internally,
# runs the CC position rules on the GPU each frame, then recomputes smooth normals from the
# RESULT geometry (shared _gatherNormalsText with the Normals module).
#
# `level` cycles 0..3 over time so the progressive rounding is visible (runtime int plug; the
# topology for every level is baked once, positions+normals recompute live on the GPU).
#
#   ./ork.hypermesh.viewer.py subdiv_demo        # [W] wireframe toggle, [O] dump OBJ
###############################################################################
import math
from ork.hypergraph.dflow.hypermesh import Hypermesh


class SubdivDemo(Hypermesh):
  def __init__(self, level=2):
    super().__init__()
    box = self.box(size=1.0)
    self._subd = self.subdivide(box, smooth=True)
    self._subd.inputs.level = int(level)
    self.output(self._subd)

  def onUpdate(self, updinfo):
    # cycle level 1->2->3 every ~2.5s so the rounding is visible (0 would show the raw cube).
    lvl = 1 + int(updinfo.absolutetime / 2.5) % 3
    self._subd.inputs.level = lvl


__all__ = ["SubdivDemo"]
