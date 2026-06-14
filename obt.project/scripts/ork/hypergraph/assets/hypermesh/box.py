###############################################################################
# Box — a cube (indexed QUAD topology + a per-FACE material_id attribute). The simplest non-grid,
# non-triangle primitive: 6 quad faces; the render-time compute fan-triangulates them. `size` =
# half-extent. Static (no onUpdate) -> the viewer renders it live but never re-evaluates the graph.
#
#   from ork.hypergraph.assets.hypermesh import Box
#   gmesh = Box(size=1.0).materialize(ctx)   # num_faces == 6 quads
###############################################################################
from ork.hypergraph.dflow.hypermesh import Hypermesh


class Box(Hypermesh):
  def __init__(self, size=1.0):
    super().__init__()
    box = self.box(size=size)
    self.output(box)


__all__ = ["Box"]
