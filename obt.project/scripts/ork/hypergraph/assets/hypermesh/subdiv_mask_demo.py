###############################################################################
# SubdivMaskDemo — MASKED Catmull-Clark. A cube whose 3 faces around the +X+Y+Z corner are tagged
# (group 0) and subdivide(smooth=True, slot=0); the other 3 faces are left alone. Result: that corner
# ROUNDS toward a sphere-octant while the opposite corner stays sharp, joined by a WATERTIGHT n-gon
# seam (the boundary edges are a crease — pinned to the straight cube edges; the unselected faces
# absorb the new midpoints and become n-gons). `level` cycles 1..3 so the rounding is visible live.
#
#   ./ork.hypermesh.viewer.py subdiv_mask_demo     # [W] wireframe (see the seam), [O] dump OBJ
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh, sel_normal_dir, add, group, POLY


class SubdivMaskDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    box = self.box(size=1.0)
    # tag the 3 faces whose normals face the +X+Y+Z corner (N·(1,1,1) > 0) into group 0.
    corner = sel_normal_dir(n=vec3(1, 1, 1), t=0.2, soft=0.1)
    tagged = self.select(box, corner, domain=POLY, op=add(group(0)))
    self._subd = self.subdivide(tagged, smooth=True, slot=0)
    self._subd.inputs.level = 3
    self.output(self._subd)

  def onUpdate(self, updinfo):
    self._subd.inputs.level = 1 + int(updinfo.absolutetime / 2.5) % 3


__all__ = ["SubdivMaskDemo"]
