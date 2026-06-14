###############################################################################
# point_select_test — verification asset for POINT (vertex) SELECTION.
#
# box -> smooth_normals (weld to 8 verts) -> select(POINT, S.P.y > 0) -> verttest. The POINT Select
# evaluates the per-vertex predicate (here: above the y=0 plane) and writes the VERTEX-domain __tags;
# verttest emits one vertex per input vertex as (vertid, selbit, 0). In the dumped OBJ ([O]) the Y
# coordinate IS the selection bit: y==1 selected, y==0 not. A cube has 4 of 8 verts above y=0.
#
#   ./ork.hypermesh.viewer.py point_select_test
#
# Swap the predicate (S.P / S.N / sel_normal_dir / sel_dist_point / sel_height_band) + live-reload (-w).
# Infra harness — the points are verification data, not a renderable model.
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, S, POINT, replace, group


class PointSelectTest(Hypermesh):
  def __init__(self):
    super().__init__()
    w = self.smooth_normals(self.box(size=1.0))
    s = self.select(w, S.P.y > 0.0, domain=POINT, op=replace(group(0)))
    self.output(self.verttest(s))


__all__ = ["PointSelectTest"]
