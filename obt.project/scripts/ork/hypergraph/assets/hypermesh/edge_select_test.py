###############################################################################
# edge_select_test — verification asset for LINE (edge) SELECTION + the dihedral atom.
#
# box -> smooth_normals (weld to a manifold cube) -> select(LINE, sel_dihedral_gt(45)) -> edgetest.
# The edge Select enumerates unique edges (MeshEdges), evaluates the per-edge predicate (here: dihedral
# angle between the edge's two faces), and writes the EDGE-domain __tags. edgetest then encodes each
# edge as a vertex: XY = (va, vb), Z = count + 10*selected_bit. So in the dumped OBJ ([O] in the viewer):
#   z ~= 2   -> interior edge, NOT selected
#   z ~= 12  -> interior edge, SELECTED (dihedral > 45deg)
#
#   ./ork.hypermesh.viewer.py edge_select_test
#
# A cube's 12 edges are all 90deg -> all 12 select at >45deg (and none at >100deg). Swap the predicate
# (sel_length_gt, sel_id_range, S.angle/S.length expressions) and live-reload (-w) to explore. Infra
# harness — the points are verification data, not a renderable model.
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh, sel_dihedral_gt, LINE, replace, group


class EdgeSelectTest(Hypermesh):
  def __init__(self):
    super().__init__()
    w = self.smooth_normals(self.box(size=1.0))
    s = self.select(w, sel_dihedral_gt(45.0), domain=LINE, op=replace(group(0)))
    self.output(self.edgetest(s))


__all__ = ["EdgeSelectTest"]
