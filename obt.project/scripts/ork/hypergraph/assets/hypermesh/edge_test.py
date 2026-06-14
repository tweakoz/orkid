###############################################################################
# edge_test — verification asset for the GPU-native edge-enumeration primitive (MeshEdges).
#
# <generator> -> edgetest enumerates the mesh's unique undirected edges ENTIRELY on the GPU
# (corner->face -> emit canonical directed edges -> MeshSort -> mark run-starts -> scan -> build the
# unique-edge table with 1-2 face adjacency; no readback). The edgetest harness then writes one OUTPUT
# vertex per edge encoding (va, vb, count) in XYZ (unused slots = the (-1,-1,-1) sentinel), so dumping
# the OBJ ([O] in the viewer) lets you read the edge table back.
#
#   ./ork.hypermesh.viewer.py edge_test     # icosphere (manifold): 30 edges, all count=2 (Euler V12-E30+F20=2)
#
# Swap the source to box(size=1.0) (live-reload with -w) for the raw split-vert case: 24 edges, all
# count=1 (boundary — the split topology shares no vertex index). NOTE: this is an infrastructure
# harness; the points it emits are verification data, not a renderable model. The visual payoff of the
# edge work is BEVEL (next), which builds on this enumeration.
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh


class EdgeTestDemo(Hypermesh):
  def __init__(self):
    super().__init__()
    self.output(self.edgetest(self.icosphere(radius=1.0, subdivisions=0)))


__all__ = ["EdgeTestDemo"]
